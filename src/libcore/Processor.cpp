/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  James Tuck
                  Milos Prvulovic
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

#include "SescConf.h"

#include "Processor.h"

#include "Cache.h"
#include "FetchEngine.h"
#include "GMemorySystem.h"
#include "ExecutionFlow.h"
#include "OSSim.h"

Processor::Processor(GMemorySystem *gm, CPU_t i)
  :GProcessor(gm, i, 1)
  ,IFID(i, i, gm, this)
  ,pipeQ(i)
{
  spaceInInstQueue = InstQueueSize;

  bzero(RAT,sizeof(DInst*)*NumArchRegs);

  l1Cache = gm->getDataSource();

#ifdef SESC_INORDER
  TimeDelta_t l1HitDelay = SescConf->getInt(l1Cache->getDescrSection(),"hitDelay");

  bzero(RATTIME,sizeof(Time_t*)*NumArchRegs);
  const char *cpucore = SescConf->getCharPtr("","cpucore", i);
  I(cpucore);

  int nClusters = SescConf->getRecordSize(cpucore,"cluster");
  for(int i=0 ; i< nClusters; i++) {
    const char *cluster = SescConf->getCharPtr(cpucore,"cluster", i);
    I(cluster);

    if (SescConf->checkInt(cluster, "iALULat"))
      latencyVal[iALU]       = SescConf->getInt(cluster, "iALULat");
    if (SescConf->checkInt(cluster, "iMultLat"))
      latencyVal[iMult]      = SescConf->getInt(cluster, "iMultLat");
    if (SescConf->checkInt(cluster, "iDivLat"))
      latencyVal[iDiv]       = SescConf->getInt(cluster, "iDivLat");
    if (SescConf->checkInt(cluster, "iBJLat"))
      latencyVal[iBJ]        = SescConf->getInt(cluster, "iBJLat");
    if (SescConf->checkInt(cluster, "iLoadLat"))
      latencyVal[iLoad]      = SescConf->getInt(cluster, "iLoadLat") + l1HitDelay;
    if (SescConf->checkInt(cluster, "iStorelat"))
      latencyVal[iStore]     = SescConf->getInt(cluster, "iStorelat");
    if (SescConf->checkInt(cluster, "fpALULat"))
      latencyVal[fpALU]      = SescConf->getInt(cluster, "fpALULat");
    if (SescConf->checkInt(cluster, "fpMultLat"))
      latencyVal[fpMult]     = SescConf->getInt(cluster, "fpMultLat");
    if (SescConf->checkInt(cluster, "fpDivLat"))
      latencyVal[fpDiv]      = SescConf->getInt(cluster, "fpDivLat");
  }

#endif
}

Processor::~Processor()
{
  // Nothing to do
}

DInst **Processor::getRAT(const int contextId)
{
  I(contextId == Id);
  return RAT;
}

FetchEngine *Processor::currentFlow()
{
  return &IFID;
}

void Processor::saveThreadContext(Pid_t pid)
{
  I(IFID.getPid()==pid);
  IFID.saveThreadContext();
}

void Processor::loadThreadContext(Pid_t pid)
{
  I(IFID.getPid()==pid);
  IFID.loadThreadContext();
}

ThreadContext *Processor::getThreadContext(Pid_t pid)
{
  I(IFID.getPid()==pid);
  return IFID.getThreadContext();
}

void Processor::setInstructionPointer(Pid_t pid, icode_ptr picode)
{
  I(IFID.getPid()==pid);
  IFID.setInstructionPointer(picode);
}

icode_ptr Processor::getInstructionPointer(Pid_t pid)
{
  I(IFID.getPid()==pid);
  return IFID.getInstructionPointer();  
}

void Processor::switchIn(Pid_t pid)
{
  LOG("Processor(%ld):switchIn %d", Id, pid);
    
  IFID.switchIn(pid);
}

void Processor::switchOut(Pid_t pid)
{
  LOG("Processor(%ld):switchOut %d", Id, pid);

  IFID.switchOut(pid);
}

size_t Processor::availableFlows() const 
{
  return IFID.getPid() < 0 ? 1 : 0;
}

long long Processor::getAndClearnGradInsts(Pid_t pid)
{
  I(IFID.getPid() == pid);
  
  return IFID.getAndClearnGradInsts();
}

long long Processor::getAndClearnWPathInsts(Pid_t pid)
{
  I(IFID.getPid() == pid);
  
  return IFID.getAndClearnWPathInsts();
}

Pid_t Processor::findVictimPid() const
{
  return IFID.getPid();
}

void Processor::goRabbitMode(long long n2Skip)
{
  IFID.goRabbitMode(n2Skip);
}

void Processor::advanceClock()
{
#ifdef TS_STALL
  if (isStall()) return;
#endif  

  clockTicks++;

  //  GMSG(!ROB.empty(),"robTop %d Ul %d Us %d Ub %d",ROB.getIdFromTop(0)
  //       ,unresolvedLoad, unresolvedStore, unresolvedBranch);

  // Fetch Stage
  if (IFID.hasWork() ) {
    IBucket *bucket = pipeQ.pipeLine.newItem();
    if( bucket ) {
      IFID.fetch(bucket);
      // readyItem would be called once the bucket is fetched
    }
  }
  
  // ID Stage (insert to instQueue)
  if (spaceInInstQueue >= FetchWidth) {
    IBucket *bucket = pipeQ.pipeLine.nextItem();
    if( bucket ) {
      I(!bucket->empty());
      //      I(bucket->top()->getInst()->getAddr());
      
      spaceInInstQueue -= bucket->size();
      pipeQ.instQueue.push(bucket);
    }
  }

  // RENAME Stage
  if ( !pipeQ.instQueue.empty() ) {
    spaceInInstQueue += issue(pipeQ);
    //    spaceInInstQueue += issue(pipeQ);
  }
  
  retire();
}

#ifdef SESC_INORDER
bool Processor::isStall(DInst *dinst)
{
 if (dinst == 0)
    return false;

 const Instruction *tempinst = dinst->getInst();
 if (tempinst->getOpcode() != iLoad)
    return false;

 return !static_cast<Cache *>(l1Cache)->isInCache(static_cast<PAddr>(dinst->getVaddr()));
}
#endif

StallCause Processor::addInst(DInst *dinst) 
{
  const Instruction *inst = dinst->getInst();

#ifdef SESC_INORDER
  if (InOrderCore) {
    // Update the RATTIME for the destiantion register
    Time_t newTime = globalClock;
    newTime += latencyVal[inst->getOpcode()];
    
    if (isStall(RAT[inst->getSrc1()])
        || isStall(RAT[inst->getSrc2()])
#ifdef SESC_INORDER_OUT_DEPS
        || isStall(RAT[inst->getDest()])
#endif
        ) {
      return SmallWinStall;
    }
      
    if (RATTIME[inst->getSrc1()] > globalClock
       || RATTIME[inst->getSrc2()] > globalClock
#ifdef SESC_INORDER_OUT_DEPS
       || RATTIME[inst->getDest()] > globalClock 
#endif
        ) {
      return SmallWinStall;
    }

    {
      // Instructions can not be issued out-of-order
      static Time_t lastTime = 0;
      if (lastTime > newTime) {
        return SmallWinStall;
      }
      lastTime = newTime;
    }

    // No Stalls occured  
    RATTIME[inst->getDest()] = newTime;
  }
 
#else
  if (InOrderCore) {
    if(RAT[inst->getSrc1()] != 0 || RAT[inst->getSrc2()] != 0 
       || RAT[inst->getDest()] != 0
       ) {
      return SmallWinStall;
    }
  }
#endif

  StallCause sc = sharedAddInst(dinst);
  if (sc != NoStall)
    return sc;

#ifdef SESC_SEED
  {
    // Small L1 cache predictor
    static SCTable l1HitPred(0,"l1HitPred",8192,3);
    static GStatsCntr  *nL1Hit_pHit  =0;
    static GStatsCntr  *nL1Hit_pMiss =0;
    static GStatsCntr  *nL1Miss_pHit =0;
    static GStatsCntr  *nL1Miss_pMiss=0;
    if (nL1Hit_pMiss == 0) {
      nL1Hit_pHit   = new GStatsCntr("L1Pred:nL1Hit_pHit");
      nL1Hit_pMiss  = new GStatsCntr("L1Pred:nL1Hit_pMiss");
      nL1Miss_pHit  = new GStatsCntr("L1Pred:nL1Miss_pHit");
      nL1Miss_pMiss = new GStatsCntr("L1Pred:nL1Miss_pMiss");
    }
    
    if (inst->isLoad()) {
      bool l1Hit  = static_cast<Cache *>(l1Cache)->isInCache(static_cast<PAddr>(dinst->getVaddr()));
      bool pL1Hit = l1HitPred.isHighest(inst->currentID());
      if (l1Hit)
        l1HitPred.predict(inst->currentID(), true);
      else
        l1HitPred.clear(inst->currentID());

      if (l1Hit && pL1Hit) {
#ifdef SESC_SEED_STALL_LOADS
        dinst->setStallOnLoad();
#endif
        nL1Hit_pHit->inc();
      }else if (l1Hit && !pL1Hit) {
        // additional stall on load only if L1 hit & predict miss
        dinst->setStallOnLoad();
        nL1Hit_pMiss->inc();
      }else if (!l1Hit && pL1Hit) {
#ifdef SESC_SEED_STALL_LOADS
        dinst->setStallOnLoad();
#endif
        nL1Miss_pHit->inc();
      }else{
#ifdef SESC_SEED_STALL_LOADS
        dinst->setStallOnLoad();
#endif
        nL1Miss_pMiss->inc();
      }
    }
  }
#endif

  I(dinst->getResource() != 0); // Resource::schedule must set the resource field

  if(!dinst->isSrc2Ready()) {
    // It already has a src2 dep. It means that it is solved at
    // retirement (Memory consistency. coherence issues)
    if( RAT[inst->getSrc1()] )
      RAT[inst->getSrc1()]->addSrc1(dinst);
  }else{
    if( RAT[inst->getSrc1()] )
      RAT[inst->getSrc1()]->addSrc1(dinst);

    if( RAT[inst->getSrc2()] )
      RAT[inst->getSrc2()]->addSrc2(dinst);
  }

  dinst->setRATEntry(&RAT[inst->getDest()]);
  RAT[inst->getDest()] = dinst;

  I(dinst->getResource());
  dinst->getResource()->getCluster()->addInst(dinst);

  return NoStall;
}

bool Processor::hasWork() const 
{
#ifdef SESC_INORDER
  if (switching)
    return true;
#endif
  return IFID.hasWork() || !ROB.empty() || pipeQ.hasWork();
}

#ifdef SESC_MISPATH
void Processor::misBranchRestore(DInst *dinst)
{
  clusterManager.misBranchRestore();

  for (unsigned i = 0 ; i < INSTRUCTION_MAX_DESTPOOL; i++)
    misRegPool[i] = 0;

  I(dinst->getFetch() == &IFID);

  pipeQ.pipeLine.cleanMark();

  // TODO: try to remove from ROB
}
#endif
