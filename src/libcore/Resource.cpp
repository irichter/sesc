/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  James Tuck
                  Smruti Sarangi
                  Luis Ceze

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <limits.h>

#include "Cluster.h"
#include "DInst.h"
#include "EnergyMgr.h"
#include "FetchEngine.h"
#include "GMemorySystem.h"
#include "GProcessor.h"
#include "LDSTBuffer.h"
#include "MemRequest.h"
#include "Port.h"
#include "Resource.h"

Resource::Resource(Cluster *cls, PortGeneric *aGen)
  : cluster(cls)
    ,gen(aGen)
{
  I(cls);
  if(gen)
    gen->subscribe();
}

Resource::~Resource()
{
  GMSG(!EventScheduler::empty(), "Resources destroyed with %d pending instructions"
       , EventScheduler::size());
  
  if(gen)
    gen->unsubscribe();
}

void Resource::executed(DInst *dinst)
{
  cluster->entryExecuted(dinst);
}

bool Resource::retire(DInst *dinst)
{
  cluster->entryRetired();
  dinst->destroy();
  return true;
}

/***********************************************/

MemResource::MemResource(Cluster *cls
			 ,PortGeneric *aGen
			 ,GMemorySystem *ms)
  : Resource(cls, aGen)
    ,memorySystem(ms)
{
  L1DCache = ms->getDataSource();
}

/***********************************************/

FUMemory::FUMemory(Cluster *cls, GMemorySystem *ms)
  : MemResource(cls, 0, ms)
{
  I(ms);

  L1DCache = ms->getDataSource();
}

StallCause FUMemory::schedule(DInst *dinst)
{
  if (!cluster->canIssue())
    return SmallWinStall;

  cluster->newEntry();
  dinst->setResource(this);
  
  // TODO: Someone implement a LDSTBuffer::fenceGetEntry(dinst);

  return NoStall;
}

void FUMemory::simTime(DInst *dinst, TimeDelta_t fwdLat)
{
  // Ignore fwdLat for this type of instructions (laziness)
  const Instruction *inst = dinst->getInst();
  I(inst->isFence());
  I(!inst->isLoad() );
  I(!inst->isStore() );

  // TODO: Add prefetch for Fetch&op as a MemWrite
  dinst->doAtExecutedCB.schedule(1); // Next cycle
}

bool FUMemory::retire(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();

  if( inst->getSubCode() == iFetchOp 
      && !L1DCache->canAcceptStore(static_cast<PAddr>(dinst->getVaddr())) )
    return false;


  if( inst->getSubCode() == iFetchOp ) {
#ifdef TS_CHERRY
    dinst->setCanBeRecycled();
    dinst->setMemoryIssued();
#endif
    DMemRequest::create(dinst, memorySystem, MemWrite);
  }else if( inst->getSubCode() == iMemFence ) {
    // TODO: Consistency in LDST
    dinst->destroy();
  }else if( inst->getSubCode() == iAcquire ) {
    // TODO: Consistency in LDST
    dinst->destroy();
  }else {
    I( inst->getSubCode() == iRelease );
    // TODO: Consistency in LDST
    dinst->destroy();
  }

  return true;
}

/***********************************************/

FULoad::FULoad(Cluster *cls, PortGeneric *aGen
	       ,TimeDelta_t l
	       ,TimeDelta_t lsdelay
	       ,GMemorySystem *ms
	       ,size_t maxLoads
	       ,int id)
  : MemResource(cls, aGen, ms)
  ,nForwarded("FULoad(%d):nForwarded", id)
  ,lat(l)
  ,LSDelay(lsdelay)
  ,freeLoads(maxLoads)
  ,misLoads(0)
{
  char cadena[100];
  sprintf(cadena,"FULoad(%d)", id);
  
  lsqPregEnergy = new GStatsEnergy("lsqPregLoadEnergy",cadena,id,LSQPregEnergy
				   ,EnergyMgr::get("lsqPregEnergy",id),"LSQ");
  
  iAluEnergy = new GStatsEnergy("iAluEnergy", cadena , id, IAluEnergy
				,EnergyMgr::get("iALUEnergy",id));
  
  I(ms);
  I(freeLoads>0);
}

StallCause FULoad::schedule(DInst *dinst)
{
  int freeEntries = freeLoads;
#ifdef SESC_MISPATH
  freeEntries -= misLoads;
#endif
  if( freeEntries <= 0 ) {
    I(freeEntries == 0); // Can't be negative
    return OutsLoadsStall;
  }

  if (!cluster->canIssue())
    return SmallWinStall;

  cluster->newEntry();
  dinst->setResource(this);

  LDSTBuffer::getLoadEntry(dinst);

  if (dinst->isFake())
    misLoads++;
  else
    freeLoads--;

  return NoStall;
}

void FULoad::simTime(DInst *dinst, TimeDelta_t fwdLat)
{
  Time_t when = gen->nextSlot()+lat;

  // The check in the LD Queue is performed always, for hit & miss
  lsqPregEnergy->inc();
  iAluEnergy->inc();
  
  if (dinst->isLoadForwarded()) {
    // No fwdLat because inside the same LDSTUnit
    dinst->doAtExecutedCB.scheduleAbs(when+LSDelay);
    // forwardEnergy->inc(); // TODO: CACTI == a read in the STQ
    nForwarded.inc();
  }else{
    cacheDispatchedCB::scheduleAbs(when+fwdLat, this, dinst);
  }
}

void FULoad::executed(DInst* dinst)
{
  Resource::executed(dinst);
}

void FULoad::cacheDispatched(DInst *dinst)
{
  I( !dinst->isLoadForwarded() );
  // LOG("[0x%p] %lld 0x%lx read", dinst, globalClock, dinst->getVaddr());
  DMemRequest::create(dinst, memorySystem, MemRead);
}

bool FULoad::retire(DInst *dinst)
{

  cluster->entryRetired();

  if (!dinst->isFake())
    freeLoads++;

  dinst->destroy();

  return true;
}

#ifdef SESC_MISPATH
void FULoad::misBranchRestore()
{ 
  misLoads = 0; 
}
#endif
/***********************************************/

FUStore::FUStore(Cluster *cls, PortGeneric *aGen
		 ,TimeDelta_t l
		 ,GMemorySystem *ms
		 ,size_t maxStores
		 ,int id)
  : MemResource(cls, aGen, ms)
  ,nDeadStore("FUStore(%d):nDeadStore", id)
  ,lat(l)
  ,freeStores(maxStores)
  ,misStores(0)
{
  char cadena[100];
  sprintf(cadena,"FUStore(%d)", id);
  
  lsqWakeupEnergy = new GStatsEnergy("lsqWakeupEnergy",cadena,id,LSQWakeupEnergy
				     ,EnergyMgr::get("lsqWakeupEnergy",id),"LSQ");


  lsqPregEnergy = new GStatsEnergy("lsqPregStoreEnergy",cadena,id,LSQPregEnergy
				   ,EnergyMgr::get("lsqPregEnergy",id),"LSQ");

  iAluEnergy = new GStatsEnergy("iAluEnergy", cadena , id, IAluEnergy
				,EnergyMgr::get("iALUEnergy",id));

  I(freeStores>0);
}

StallCause FUStore::schedule(DInst *dinst)
{
  int freeEntries = freeStores;
#ifdef SESC_MISPATH
  freeEntries -= misStores;
#endif
  if( freeEntries <= 0 ) {
    I(freeEntries == 0); // Can't be negative
    return OutsStoresStall;
  }

  if( !cluster->canIssue() )
    return SmallWinStall;

  cluster->newEntry();
  dinst->setResource(this);

  LDSTBuffer::getStoreEntry(dinst);

  if (dinst->isFake()) {
    misStores++;
  }else{
    freeStores--;
  }

  return NoStall;
}

void FUStore::simTime(DInst *dinst, TimeDelta_t fwdLat)
{
  dinst->doAtExecutedCB.schedule(lat+fwdLat);
}

void FUStore::executed(DInst *dinst)
{
  // energy increment
  lsqPregEnergy->inc();
  lsqWakeupEnergy->inc();
  iAluEnergy->inc();

  cluster->entryExecuted(dinst);

#ifdef TS_CHERRY
  I(!dinst->isEarlyRecycled());

  if (dinst->isDeadStore() && !dinst->isFake()) {
    freeStores++;
    dinst->setEarlyRecycled();
    return;
  }

  if (!L1DCache->canAcceptStore(static_cast<PAddr>(dinst->getVaddr()))) 
    return;
  
  if (!dinst->isFake()) {
    freeStores++;
    dinst->setEarlyRecycled();
  }
  
  dinst->setMemoryIssued();

  DMemRequest::create(dinst, memorySystem, MemWrite);
#endif
}

void FUStore::doRetire(DInst *dinst)
{
  LDSTBuffer::storeLocallyPerformed(dinst);
  cluster->entryRetired();

  if (!dinst->isEarlyRecycled() && !dinst->isFake())
    freeStores++;
}

bool FUStore::retire(DInst *dinst)
{
  if (dinst->isFake() || dinst->isEarlyRecycled()
#ifdef TS_CHERRY
      || dinst->isMemoryIssued()
#endif
      ) {
    doRetire(dinst);

    if (dinst->isDeadStore())
      nDeadStore.inc();

#ifdef TS_CHERRY
    if (dinst->isMemoryIssued() && !dinst->hasCanBeRecycled())
      dinst->setCanBeRecycled();
    else
      dinst->destroy();
#else
    dinst->destroy();
#endif
    return true;
  }

  //  I(!dinst->isDeadStore());

  if( L1DCache->getNextFreeCycle() > globalClock)
    return false;

  if (!L1DCache->canAcceptStore(static_cast<PAddr>(dinst->getVaddr())) )
    return false;

  // Note: The store is retired from the LDSTQueue as soon as it is send to the
  // L1 Cache. It does NOT wait until the ack from the L1 Cache is received

#ifdef TS_CHERRY
  dinst->setCanBeRecycled();
  dinst->setMemoryIssued();
#endif

  gen->nextSlot();
  DMemRequest::create(dinst, memorySystem, MemWrite);

  doRetire(dinst);

  return true;
}

#ifdef SESC_MISPATH
void FUStore::misBranchRestore() 
{ 
  misStores = 0; 
}
#endif


/***********************************************/
FUGeneric::FUGeneric(Cluster *cls
		     ,PortGeneric *aGen
		     ,TimeDelta_t l
		     ,GStatsEnergyCG *eb
  )
  :Resource(cls, aGen)
  ,lat(l)
  ,fuEnergy(eb)
{
}

StallCause FUGeneric::schedule(DInst *dinst)
{
  if( !cluster->canIssue() )
    return SmallWinStall;

  cluster->newEntry();
  dinst->setResource(this);

  return NoStall;
}

void FUGeneric::simTime(DInst *dinst, TimeDelta_t fwdLat)
{
  dinst->doAtExecutedCB.scheduleAbs(gen->nextSlot()+lat+fwdLat);
}

void FUGeneric::executed(DInst *dinst)
{
  fuEnergy->inc();
  cluster->entryExecuted(dinst);
}

/***********************************************/

FUBranch::FUBranch(Cluster *cls, PortGeneric *aGen, TimeDelta_t l, int mb)
  :Resource(cls, aGen)
   ,lat(l)
   ,freeBranches(mb)
{
  I(freeBranches>0);
}

StallCause FUBranch::schedule(DInst *dinst)
{
  if (freeBranches == 0)
    return OutsBranchesStall;

  if (!cluster->canIssue())
    return SmallWinStall;

  cluster->newEntry();
  dinst->setResource(this);
  
  freeBranches--;

  return NoStall;
}

void FUBranch::simTime(DInst *dinst, TimeDelta_t fwdLat)
{
  dinst->doAtExecutedCB.scheduleAbs(gen->nextSlot()+lat+fwdLat);
}

void FUBranch::executed(DInst *dinst)
{
  // TODO: change it to remove getFetch only call missBranch when a
  // boolean is set? Backup done at fetch 
  if (dinst->getFetch()) {
    dinst->getFetch()->unBlockFetch();
#ifdef SESC_MISPATH
    cluster->getGProcessor()->misBranchRestore(dinst);
#endif
  }

  freeBranches++;

  cluster->entryExecuted(dinst);
}

/***********************************************/

FUEvent::FUEvent(Cluster *cls)
  :Resource(cls, 0)
{
}

StallCause FUEvent::schedule(DInst *dinst)
{
  if( !cluster->canIssue() )
    return SmallWinStall;

  dinst->setResource(this);
  return NoStall;
}

void FUEvent::simTime(DInst *dinst, TimeDelta_t fwdLat)
{
  // Ignore fwdLat for this type of instructions (laziness)
  dinst->setResource(this);

  I(dinst->getInst()->getEvent() == PostEvent);
  // memfence, Relase, and Acquire are passed to FUMemory
  //
  // PreEvent, FastSimBegin, and FastSimEnd are not simulated as
  // instructions through the pipeline

  CallbackBase *cb = dinst->getPendEvent();

  // If the preEvent is created with a vaddr the event handler is
  // responsible to schedule the instruction execution.
  I( dinst->getVaddr() == 0 );
  cb->call();

  cluster->entryExecuted(dinst);
}
