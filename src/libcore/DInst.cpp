/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau

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

#include "DInst.h"

#include "Cluster.h"
#include "FetchEngine.h"
#include "OSSim.h"
#include "LDSTBuffer.h"
#include "Resource.h"

pool<DInst> DInst::dInstPool(512);
#ifdef DEBUG
long DInst::currentID=0;
#endif

DInst::DInst()
  :doAtExecutedCB(this)
  ,doAtSimTimeCB(this)
{
  pend[0].init(this);
  pend[1].init(this);
  I(MAX_PENDING_SOURCES==2);
  IS(nDeps = 0);
}

void DInst::dump(const char *str)
{
  fprintf(stderr,"%s:(%d)  DInst: vaddr=0x%x ", str, cpuId, (int)vaddr);
  if (executed) {
    fprintf(stderr, " executed");
  }else if (issued) {
    fprintf(stderr, " issued");
  }else{
    fprintf(stderr, " non-issued");
  }
  if (hasPending())
    fprintf(stderr, " has pending");
  if (!isSrc1Ready())
    fprintf(stderr, " has src1 deps");
  if (!isSrc2Ready()) 
    fprintf(stderr, " has src2 deps");
  
  inst->dump("");
}

void DInst::doAtExecuted()
{
  I( resource );
  resource->executed(this);
}

void DInst::doAtSimTime()
{
  I( resource );

  I(!isExecuted());

  I(resource->getCluster());

#ifdef SESC_PRESCHEDULE
  // wakeUp at the end of the select stage
  resource->getCluster()->wakeUpDeps(this);
#endif

  resource->simTime(this);
}

DInst *DInst::createDInst(const Instruction *inst, VAddr va, int cpuId)
{
#ifdef SESC_MISPATH
  if (inst->isType(iOpInvalid))
    return 0;
#endif

  DInst *i = dInstPool.out();

  i->inst  = inst;
  Prefetch(i->inst);
  i->cpuId = cpuId;
  i->wakeUpTime = 0;
  i->vaddr = va;
  i->first = 0;
#ifdef DEBUG
  i->ID = currentID++;
  i->resource  = 0;
  i->pendEvent = 0;
#endif
  i->fetch = 0;
  i->loadForwarded= false;
  i->issued       = false;
  i->executed     = false;
  i->depsAtRetire = false;
  i->deadStore    = false;
#ifdef SESC_CHERRY
  i->resolved     = false;
  i->earlyRecycled= false;
  i->canBeRecycled= false;
  i->memoryIssued = false;
  i->registerRecycled= false;
#endif
#ifdef SESC_MISPATH
  i->fake         = false;
#endif

#ifdef BPRED_UPDATE_RETIRE
  i->bpred    = 0;
  i->oracleID = 0;
#endif

#ifdef TASKSCALAR
  i->lvid       = 0;
  IS(i->lvidVersion=0); // not necessary to set to null (Goes together with LVID)
  i->restartVer = 0;


#endif //TASKSCALAR

#if (defined TLS)
  ID(i->myEpoch=0);
#endif
  i->pend[0].isUsed = false;
  i->pend[1].isUsed = false;
    
  return i;
}

DInst *DInst::createInst(InstID pc, VAddr va, int cpuId)
{
  const Instruction *inst = Instruction::getInst(pc);
  return createDInst(inst, va, cpuId);
}

void DInst::killSilently()
{
  I(getPendEvent()==0);
  I(getResource()==0);

  markIssued();
  markExecuted();
  if( getFetch() )
    getFetch()->unBlockFetch();

  if (getInst()->isStore())
    LDSTBuffer::storeLocallyPerformed(this);
 
  while (hasPending()) {
    DInst *dstReady = getNextPending();

    if (!dstReady->isIssued()) {
      // Accross processor dependence
      if (dstReady->hasDepsAtRetire())
	dstReady->clearDepsAtRetire();
      
      I(!dstReady->hasDeps());
      continue;
    }
    if (dstReady->isExecuted()) {
      // The instruction got executed even though it has dependences. This is
      // because the instruction got silently killed (killSilently)
      if (!dstReady->hasDeps())
	dstReady->scrap();
      continue;
    }

    if (!dstReady->hasDeps()) {
      I(dstReady->isIssued());
      I(!dstReady->isExecuted());
      Resource *dstRes = dstReady->getResource();
      I(dstRes);
      dstRes->simTime(dstReady);
    }
  }

#ifdef TASKSCALAR
#ifdef ATOMIC
#if 0
  while (! restartList.empty()) {
    struct DInst::restartPair* pair = restartList.back();
    restartList.pop_back();
    if (pair->restartVer) {
      TaskContext *tc = pair->restartVer->getTaskContext();
      if (tc && osSim->getState(tc->getPid() ) == SuspendedState)
	osSim->resume(tc->getPid());
	tc->invalidMemAccess(pair->pendingInvMemID, InvMemAtRetire);
      pair->restartVer->garbageCollect();
      restartListPool.in(pair);
    }
  }
#endif
#else
  notifyDataDepViolation(DataDepViolationAtRetire);

  if (lvid) { // maybe got killSilently
    lvid = 0;
    lvidVersion->decOutsReqs();
    lvidVersion->garbageCollect();
    IS(lvidVersion=0);
  }
  
  I(lvidVersion==0);
#endif
#endif

  if (hasDeps())
    return;
  
  I(nDeps == 0);   // No deps src

#if (defined TLS)
  I(!myEpoch);
#endif

  dInstPool.in(this); 
}

void DInst::scrap()
{
  I(nDeps == 0);   // No deps src
  I(first == 0);   // no dependent instructions 
#ifdef TASKSCALAR
#ifdef ATOMIC
#if 0
  while (! restartList.empty()) {
    struct DInst::restartPair* pair = restartList.back();
    restartList.pop_back();
    if (pair->restartVer) {
      TaskContext *tc = pair->restartVer->getTaskContext();
      if (tc && osSim->getState(tc->getPid() ) == SuspendedState)
	osSim->resume(tc->getPid());
	tc->invalidMemAccess(pair->pendingInvMemID, InvMemAtRetire);
      pair->restartVer->garbageCollect();
      restartListPool.in(pair);
    }
  }
#endif
#else
  notifyDataDepViolation(DataDepViolationAtRetire);

  if (lvid) { // maybe got killSilently
    lvid = 0;
    lvidVersion->decOutsReqs();
    lvidVersion->garbageCollect();
    IS(lvidVersion=0);
  }

  I(lvidVersion==0);
#endif
#endif

#if (defined TLS)
  if(myEpoch) myEpoch->doneInstr();
  ID(myEpoch=(tls::Epoch *)1);
#endif

#ifdef BPRED_UPDATE_RETIRE
  if (bpred) {
    bpred->predict(inst, oracleID, true);
    IS(bred = 0);
  }
#endif

  dInstPool.in(this);
}

void DInst::destroy()
{
  I(nDeps == 0);   // No deps src

  I(issued);
  I(executed);

  awakeRemoteInstructions();

  GLOG(first != 0,"Instruction pc=0x%x failed first is pc=0x%x",(int)inst->getAddr(),(int)first->getDInst()->inst->getAddr());
  I(first == 0);   // no dependent instructions 

  scrap();
}

#ifdef TASKSCALAR
#ifdef ATOMIC
void DInst::addDataDepViolation(const HVersion* ver)
{
  I(0);
#if 0
  I(restartList.empty());

  while (list->size() > 0) {
    TaskContext *tc = list->back()->getTaskContext();
    list->pop_back();
    if (tc) {
#ifdef TS_IMMEDIAT_RESTART
      tc->invalidMemAccess(pendingInvMemID, InvMemAtFetch);
#else
      // At instruction retire only DInst::pendingInvMemD equal to the
      // TaskContext::pendingInvMemID would generate a restart
      struct restartPair* pair = restartListPool.out();
      pair->pendingInvMemID = tc->markPendingInvMem();
      // Duplicate version so that version does not get recycled by mistake
      pair->restartVer      = tc->getVersionDuplicate();
      restartList.push_back(pair);
#endif
    }
  }
#endif
}
#else
void DInst::addDataDepViolation(const HVersion *ver)
{
  I(restartVer==0);
  
  TaskContext *tc=ver->getTaskContext();
  if (tc == 0)
    return;

  // At instruction retire only DInst::dataDepViolationD equal to the
  // TaskContext::dataDepViolationID would generate a restart
  dataDepViolationID = tc->addDataDepViolation();

#ifdef TS_IMMEDIAT_RESTART
  tc->invalidMemAccess(dataDepViolationID, DataDepViolationAtFetch);
#else
  // Duplicate version so that version does not get recycled by mistake
  restartVer = tc->getVersionDuplicate();
#endif
}

void DInst::notifyDataDepViolation(DataDepViolationAt dAt, bool val)
{
  I(!(val && restartVer));
  if (restartVer==0)
    return;

  TaskContext *tc = restartVer->getTaskContext();
  if (tc)
    tc->invalidMemAccess(dataDepViolationID, dAt);
  
  restartVer->garbageCollect();
  restartVer = 0;
}
#endif
#endif

void DInst::awakeRemoteInstructions() 
{
  while (hasPending()) {
    DInst *dstReady = getNextPending();

    I(inst->isStore());
    I( dstReady->inst->isLoad());
    I(!dstReady->isExecuted());
    I( dstReady->hasDepsAtRetire());

    I( dstReady->isSrc2Ready()); // LDSTBuffer queue in src2, free by now

    dstReady->clearDepsAtRetire();
    if (dstReady->isIssued() && !dstReady->hasDeps()) {
      Resource *dstRes = dstReady->getResource();
      // Coherence would add the latency because the cache line must be brought
      // again (in theory it must be local to dinst processor and marked dirty
      I(dstRes); // since isIssued it should have a resource
      dstRes->simTime(dstReady);
    }
  }
}


