/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Luis Ceze
		  Karin Strauss

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

#include <sys/time.h>
#include <unistd.h>

#include "GProcessor.h"
#include "OSSim.h"

#include "FetchEngine.h"
#include "ExecutionFlow.h"
#include "GMemoryOS.h"
#include "GMemorySystem.h"
#include "LDSTBuffer.h"


#ifdef XACTION
#include "XactionManager.h"
#endif

GProcessor::GProcessor(GMemorySystem *gm, CPU_t i, size_t numFlows)
  :Id(i)
  ,FetchWidth(SescConf->getInt("cpucore", "fetchWidth",i))
  ,IssueWidth(SescConf->getInt("cpucore", "issueWidth",i))
  ,RetireWidth(SescConf->getInt("cpucore", "retireWidth",i))
  ,RealisticWidth(RetireWidth < IssueWidth ? RetireWidth : IssueWidth)
  ,InstQueueSize(SescConf->getInt("cpucore", "instQueueSize",i))
  ,InOrderCore(SescConf->getBool("cpucore","inorder",i))
  ,MaxFlows(numFlows)
  ,MaxROBSize(SescConf->getInt("cpucore", "robSize",i))
  ,memorySystem(gm)
  ,ROB(MaxROBSize)
  ,replayQ(2*MaxROBSize)
  ,lsq(this, i)
  ,clusterManager(gm, this)
  ,robUsed("Proc(%d)_robUsed", i)
  ,retired("ExeEngine(%d)_retired", i)
  ,notRetiredOtherCause("ExeEngine(%d):noRetOtherCause", i)
  ,nLocks("Processor(%d):nLocks", i)
  ,nLockContCycles("Processor(%d):nLockContCycles", i)
{
  // osSim should be already initialized
  I(osSim);
  osSim->registerProc(this);

  SescConf->isInt("cpucore", "issueWidth",i);
  SescConf->isLT("cpucore", "issueWidth", 1025,i); // no longer than unsigned short

  SescConf->isInt("cpucore"    , "retireWidth",i);
  SescConf->isBetween("cpucore" , "retireWidth", 0, 32700,i);

  SescConf->isInt("cpucore"    , "robSize",i);
  SescConf->isBetween("cpucore" , "robSize", 2, 262144,i);

  SescConf->isInt("cpucore"    , "intRegs",i);
  SescConf->isBetween("cpucore" , "intRegs", 16, 262144,i);

  SescConf->isInt("cpucore"    , "fpRegs",i);
  SescConf->isBetween("cpucore" , "fpRegs", 16, 262144,i);

  regPool[0] = SescConf->getInt("cpucore", "intRegs",i);
  regPool[1] = SescConf->getInt("cpucore", "fpRegs",i);
  regPool[2] = 262144; // Unlimited registers for invalid output

#ifdef SESC_MISPATH
  for (int j = 0 ; j < INSTRUCTION_MAX_DESTPOOL; j++)
    misRegPool[j] = 0;
#endif

  nStall[0] = 0 ; // crash if used
  nStall[SmallWinStall]     = new GStatsCntr("ExeEngine(%d):nSmallWin",i);
  nStall[SmallROBStall]     = new GStatsCntr("ExeEngine(%d):nSmallROB",i);
  nStall[SmallREGStall]     = new GStatsCntr("ExeEngine(%d):nSmallREG",i);
  nStall[OutsLoadsStall]    = new GStatsCntr("ExeEngine(%d):nOutsLoads",i);
  nStall[OutsStoresStall]   = new GStatsCntr("ExeEngine(%d):nOutsStores",i);
  nStall[OutsBranchesStall] = new GStatsCntr("ExeEngine(%d):nOutsBranches",i);
  nStall[ReplayStall]       = new GStatsCntr("ExeEngine(%d):nReplays",i);
  nStall[PortConflictStall] = new GStatsCntr("ExeEngine(%d):PortConflict",i);
  nStall[SwitchStall]       = new GStatsCntr("ExeEngine(%d):switch",i);

  for(unsigned r = 0; r < MaxNoRetResp; r++) {
    for(unsigned s = 0; s < MaxInstType; s++) {
      for(unsigned t = 0; t < MaxRetOutcome; t++) {
	notRetired[r][s][t] = 0;
      }
    }
  }

  notRetired[Self][iOpInvalid][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iOpInvalid_NotExecuted",i);
  notRetired[Self][iALU][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iALU_NotExecuted",i);
  notRetired[Self][iMult][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iMult_NotExecuted",i);
  notRetired[Self][iDiv][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iDiv_NotExecuted",i);
  notRetired[Self][iBJ][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iBJ_NotExecuted",i);
  notRetired[Self][iLoad][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iLoad_NotExecuted",i);
  notRetired[Self][iStore][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iStore_NotExecuted",i);
  notRetired[Self][fpALU][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_fpALU_NotExecuted",i);
  notRetired[Self][fpMult][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_fpMult_NotExecuted",i);
  notRetired[Self][fpDiv][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_fpDiv_NotExecuted",i);
  notRetired[Self][iFence][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iFence_NotExecuted",i);
  notRetired[Self][iLoad][NotFinished] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iLoad_NotFinished",i);
  notRetired[Self][iStore][NoCacheSpace] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iStore_NoCacheSpace",i);
  notRetired[Self][iStore][NoCachePorts] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iStore_NoCachePorts",i);
  notRetired[Self][iStore][WaitForFence] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iStore_WaitForFence",i);
  notRetired[Self][iFence][NoCacheSpace] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iFence_NoCacheSpace",i);
  notRetired[Self][iFence][WaitForFence] = 
    new GStatsCntr("ExeEngine(%d):noRetSelf_iFence_WaitForFence",i);

  notRetired[Other][iOpInvalid][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iOpInvalid_NotExecuted",i);
  notRetired[Other][iALU][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iALU_NotExecuted",i);
  notRetired[Other][iMult][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iMult_NotExecuted",i);
  notRetired[Other][iDiv][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iDiv_NotExecuted",i);
  notRetired[Other][iBJ][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iBJ_NotExecuted",i);
  notRetired[Other][iLoad][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iLoad_NotExecuted",i);
  notRetired[Other][iStore][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iStore_NotExecuted",i);
  notRetired[Other][fpALU][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_fpALU_NotExecuted",i);
  notRetired[Other][fpMult][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_fpMult_NotExecuted",i);
  notRetired[Other][fpDiv][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_fpDiv_NotExecuted",i);
  notRetired[Other][iFence][NotExecuted] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iFence_NotExecuted",i);
  notRetired[Other][iLoad][NotFinished] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iLoad_NotFinished",i);
  notRetired[Other][iStore][NoCacheSpace] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iStore_NoCacheSpace",i);
  notRetired[Other][iStore][NoCachePorts] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iStore_NoCachePorts",i);
  notRetired[Other][iStore][WaitForFence] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iStore_WaitForFence",i);
  notRetired[Other][iFence][NoCacheSpace] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iFence_NoCacheSpace",i);
  notRetired[Other][iFence][WaitForFence] = 
    new GStatsCntr("ExeEngine(%d):noRetOther_iFence_WaitForFence",i);
  
  clockTicks=0;

  char cadena[100];
  sprintf(cadena, "Proc(%d)", (int)i);

#ifdef SESC_INORDER
  renameEnergyOutOrder = new GStatsEnergy("renameEnergy", cadena, i, IssuePower
                                          ,EnergyMgr::get("renameEnergy",i));
  robEnergyOutOrder    = new GStatsEnergy("ROBEnergy",cadena,i,IssuePower,EnergyMgr::get("robEnergy",i));

  renameEnergyInOrder  = new GStatsEnergyNull();
  robEnergyInOrder     = new GStatsEnergyNull();

  renameEnergy = renameEnergyOutOrder;
  robEnergy    = robEnergyOutOrder;
 
  InOrderMode  = true;
  OutOrderMode = false;
  currentMode  = OutOrderMode;
  switching    = false;
#else
  renameEnergy = new GStatsEnergy("renameEnergy", cadena, i, IssuePower
                                  ,EnergyMgr::get("renameEnergy",i));

  robEnergy = new GStatsEnergy("ROBEnergy",cadena, i, IssuePower, EnergyMgr::get("robEnergy",i));
#endif

  wrRegEnergy[0] = new GStatsEnergy("wrIRegEnergy", cadena, i, ExecPower
                                    ,EnergyMgr::get("wrRegEnergy",i));

  wrRegEnergy[1] = new GStatsEnergy("wrFPRegEnergy", cadena, i, ExecPower
                                    ,EnergyMgr::get("wrRegEnergy",i));

  wrRegEnergy[2] = new GStatsEnergyNull();
  
  rdRegEnergy[0] = new GStatsEnergy("rdIRegEnergy", cadena , i, ExecPower
                                    ,EnergyMgr::get("rdRegEnergy",i));

  rdRegEnergy[1] = new GStatsEnergy("rdFPRegEnergy", cadena , i, ExecPower
                                    ,EnergyMgr::get("rdRegEnergy",i));

  rdRegEnergy[2] = new GStatsEnergyNull();

  I(ROB.size() == 0);

  // CHANGE "PendingWindow" instead of "Proc" for script compatibility reasons
  buildInstStats(nInst, "PendingWindow");
  
#ifdef SESC_CHERRY
  unresolvedLoad  = -1;
  unresolvedStore = -1;
  unresolvedBranch= -1;
  
  recycleStore    = -1; // min(Ul,Ub)

  bzero(cherryRAT, sizeof(DInst *) * NumArchRegs);
#endif
  
#ifdef SESC_MISPATH
  buildInstStats(nInstFake, "FakePendingWindow");
#endif


#ifdef XACTION
  xaction = new XactionManager(Id);
#endif
}

GProcessor::~GProcessor()
{
}

#ifdef SESC_INORDER      
void GProcessor::setMode(bool mode)
{
  if(currentMode == mode)
    return;

  //currentMode = mode;
  printf("Called setMode\n");
  if(currentMode == InOrderMode)
    mode = OutOrderMode;
  else 
    mode = InOrderMode;

  switching   = true;

  if(mode == InOrderMode){
    printf("Switching to InOrderMode\n"); 
    renameEnergy = renameEnergyInOrder;
    robEnergy =  robEnergyInOrder;
    currentMode = InOrderMode;
    InOrderCore = true;
  }else{
    printf("Switching to OutOrderMode\n"); 
   renameEnergy = renameEnergyOutOrder;
   robEnergy =  robEnergyOutOrder;
   currentMode = OutOrderMode;
   InOrderCore = false;
  }

  clusterManager.setMode(mode);
  printf("Done setting Mode\n");        
}
#endif

void GProcessor::buildInstStats(GStatsCntr *i[MaxInstType], const char *txt)
{
  bzero(i, sizeof(GStatsCntr *) * MaxInstType);

  i[iOpInvalid] = new GStatsCntr("%s(%d)_iOpInvalid:n", txt, Id);
  i[iALU]   = new GStatsCntr("%s(%d)_iALU:n", txt, Id);
  i[iMult]  = new GStatsCntr("%s(%d)_iComplex:n", txt, Id);
  i[iDiv]   = i[iMult];

  i[iBJ]    = new GStatsCntr("%s(%d)_iBJ:n", txt, Id);

  i[iLoad]  = new GStatsCntr("%s(%d)_iLoad:n", txt, Id);
  i[iStore] = new GStatsCntr("%s(%d)_iStore:n", txt, Id);

  i[fpALU]  = new GStatsCntr("%s(%d)_fpALU:n", txt, Id);
  i[fpMult] = new GStatsCntr("%s(%d)_fpComplex:n", txt, Id);
  i[fpDiv]  = i[fpMult];

  i[iFence] = new GStatsCntr("%s(%d)_other:n", txt, Id);
  i[iEvent] = i[iFence];

  IN(forall((int a=1;a<(int)MaxInstType;a++), i[a] != 0));
}

StallCause GProcessor::sharedAddInst(DInst *dinst) 
{
  // rename an instruction. Steps:
  //
  // 1-Get a ROB entry
  //
  // 2-Get a register
  //
  // 3-Try to schedule in Resource (window entry...)

  if( ROB.size() >= MaxROBSize ) {
    return SmallROBStall;
  }
  
#if SESC_INORDER
  if (switching) {
    if(!ROB.empty())
      return SwitchStall; // New stall Type to add
    switching = false;
  } 
#endif
  
  const Instruction *inst = dinst->getInst();

  // Register count
  I(inst->getDstPool()<3);
#ifdef SESC_MISPATH
  if ((regPool[inst->getDstPool()] - misRegPool[inst->getDstPool()]) == 0)
    return SmallREGStall;
#else
  if (regPool[inst->getDstPool()] == 0)
    return SmallREGStall;
#endif

  Resource *res = clusterManager.getResource(inst->getOpcode());
  I(res);

  const Cluster *cluster = res->getCluster();
  StallCause sc = cluster->canIssue(dinst);
  if (sc != NoStall)
    return sc;

  sc = res->canIssue(dinst);
  if (sc != NoStall)
    return sc;

  // BEGIN INSERTION (note that schedule already inserted in the window)
  dinst->markIssued();

#ifdef SESC_BAAD
  dinst->setRenameTime();
#endif

#ifdef SESC_MISPATH
  if (dinst->isFake()) {
    nInstFake[inst->getOpcode()]->inc();
    misRegPool[inst->getDstPool()]++;
  }else{
    nInst[inst->getOpcode()]->inc();
    regPool[inst->getDstPool()]--;
  }
#else
  nInst[inst->getOpcode()]->inc();
  regPool[inst->getDstPool()]--;
#endif

  renameEnergy->inc(); // Rename RAT
  robEnergy->inc(); // one for insert

  rdRegEnergy[inst->getSrc1Pool()]->inc();
  rdRegEnergy[inst->getSrc2Pool()]->inc();
  wrRegEnergy[inst->getDstPool()]->inc();

  ROB.push(dinst);

#ifdef SESC_CHERRY
  cherryRAT[inst->getDest()] = dinst;

  // Before addInst because it can execute it :(

  // TODO: try to move to schedule? so that the ifs are avoided
  if (inst->isLoad()) {
    if (unresolvedLoad == -1) {
      if (!dinst->isResolved())
        unresolvedLoad = ROB.getIdFromTop(ROB.size()-1);

      // Early recycle Loads
      if (unresolvedStore == -1)
        dinst->getResource()->earlyRecycle(dinst);
    }
  }else if (inst->isStore()) {
    if (unresolvedStore == -1 && !dinst->isResolved())
      unresolvedStore = ROB.getIdFromTop(ROB.size()-1);
  }else if (inst->isBranch()) {
    if (unresolvedBranch == -1 && !dinst->isResolved())
      unresolvedBranch = ROB.getIdFromTop(ROB.size()-1);
  }
#endif

  dinst->setResource(res);

  return NoStall;
}

int GProcessor::issue(PipeQueue &pipeQ)
{
  int i=0; // Instructions executed counter
  int j=0; // Fake Instructions counter

  I(!pipeQ.instQueue.empty());

  if(!replayQ.empty()) {
    issueFromReplayQ();
    nStall[ReplayStall]->add(RealisticWidth);
    return 0;  // we issued 0 from the instQ;
    // FIXME:check if we can issue from replayQ and 
    // fetchQ during the same cycle
  }

  do{
    IBucket *bucket = pipeQ.instQueue.top();
    do{
      I(!bucket->empty());
      if( i >= IssueWidth ) {
        return i+j;
      }

      I(!bucket->empty());

      DInst *dinst = bucket->top();
#ifdef TASKSCALAR
      if (!dinst->isFake()) {
        if (dinst->getLVID()==0 || dinst->getLVID()->isKilled()) {
          // Task got killed. Just swallow the instruction
          dinst->killSilently();
          bucket->pop();
          j++;
          continue;
        }
      }
#endif

      StallCause c = addInst(dinst);
      if (c != NoStall) {
        if (i < RealisticWidth)
          nStall[c]->add(RealisticWidth - i);
        return i+j;
      }
      i++;
        
      bucket->pop();

    }while(!bucket->empty());
    
    pipeQ.pipeLine.doneItem(bucket);
    pipeQ.instQueue.pop();
  }while(!pipeQ.instQueue.empty());

  return i+j;
}

int GProcessor::issueFromReplayQ()
{
  int nIssued = 0;

  while(!replayQ.empty()) {
    DInst *dinst = replayQ.top();
    StallCause c = addInst(dinst);
    if (c != NoStall) {
      if (nIssued < RealisticWidth)
        nStall[c]->add(RealisticWidth - nIssued);
      break;
    }
    nIssued++;
    replayQ.pop();
    if(nIssued >= IssueWidth)
      break;
  }
  return nIssued;
}

void GProcessor::replay(DInst *dinst)
{
  //traverse the ROB for instructions younger than dinst
  //and add them to the replayQ.

#ifndef DOREPLAY
  return;
#endif

  if(dinst->isDeadInst())
    return;

  bool pushInst = false;
  unsigned int robPos = ROB.getIdFromTop(0); // head or top
  while(1) {
    DInst *robDInst = ROB.getData(robPos);
    if(robDInst == dinst)
      pushInst = true;

    if(pushInst && !robDInst->isDeadInst()) {
      replayQ.push(robDInst->clone());
      robDInst->setDeadInst();
    }

    robPos = ROB.getNextId(robPos);
    if (ROB.isEnd(robPos))
      break;
  }
}

void GProcessor::report(const char *str)
{
  Report::field("Proc(%d):clockTicks=%lld", Id, clockTicks);
  memorySystem->getMemoryOS()->report(str);
}

void GProcessor::addEvent(EventType ev, CallbackBase *cb, int vaddr)
{
  currentFlow()->addEvent(ev,cb,vaddr);
}

void GProcessor::retire()
{
#ifdef DEBUG
  // Check for progress. When a processor gets stuck, it sucks big time
  if ((((int)globalClock) & 0x1FFFFFL) == 0) {
    if (ROB.empty()) {
      // ROB should not be empty for lots of time
      if (prevDInstID == 1) {
        MSG("GProcessor::retire CPU[%d] ROB empty for long time @%lld", Id, globalClock);
      }
      prevDInstID = 1;
    }else{
      DInst *dinst = ROB.top();
      if (prevDInstID == dinst->getID()) {
        I(0);
        MSG("ExeEngine::retire CPU[%d] no forward progress from pc=0x%x with %d @%lld"
            ,Id, (uint)dinst->getInst()->getAddr() 
            ,(uint)dinst->getInst()->currentID(), globalClock );
        dinst->dump("HEAD");
        LDSTBuffer::dump("");
      }
      prevDInstID = dinst->getID();
    }
  }
#endif

  robUsed.sample(ROB.size());

  ushort i;
  
  for(i=0;i<RetireWidth && !ROB.empty();i++) {
    DInst *dinst = ROB.top();

    if( !dinst->isExecuted() ) {
      addStatsNoRetire(i, dinst, NotExecuted);
      return;
    }

    // save it now because retire can destroy DInst
    int rp = dinst->getInst()->getDstPool();

    bool fake = dinst->isFake();
#ifdef SESC_CHERRY
    RegType dest= dinst->getInst()->getDest();
#endif

    I(dinst->getResource());
    RetOutcome retOutcome = dinst->getResource()->retire(dinst);
    if( retOutcome != Retired) {
      addStatsNoRetire(i, dinst, retOutcome);
      return;
    }
    // dinst CAN NOT be used beyond this point

    if (!fake)
      regPool[rp]++;

#ifdef SESC_CHERRY
    cherryRAT[dest] = 0;

    // If the instruction can get retired for sure that the unresolved
    // pointer advanced
    I(ROB.getIdFromTop(0) != unresolvedLoad);
    //    I(ROB.getIdFromTop(0) != unresolvedStore);
    //    I(ROB.getIdFromTop(0) != unresolvedBranch);
#endif
    ROB.pop();

    robEnergy->inc(); // read ROB entry (finished?, update retirement rat...)
  }

  if(!ROB.empty() || i != 0) 
    addStatsRetire(i);
  
}

#ifdef SESC_CHERRY
void GProcessor::checkRegisterRecycle()
{
  // Register Recycle min(Us,Ub) (not multi)
  ushort robPos = ROB.getIdFromTop(ROB.size()-1);
  while(1) {
    DInst *dinst = ROB.getData(robPos);
    
    if (!dinst->hasRegisterRecycled() 
        && dinst->isExecuted() 
        && !dinst->hasPending()
        && cherryRAT[dinst->getInst()->getDest()] != dinst) {
      dinst->setRegisterRecycled();

#ifdef SESC_MISPATH
      if (dinst->isFake()) {
        misRegPool[dinst->getInst()->getDstPool()]--;
      }else{
        regPool[dinst->getInst()->getDstPool()]++;
      }
#else
      regPool[dinst->getInst()->getDstPool()]++;
#endif
    }

    robPos = ROB.getNextId(robPos);
    if (ROB.isEnd(robPos))
      break;
  }
}

void GProcessor::propagateUnresolvedLoad()
{
  I(unresolvedLoad>=0);
  while(1) {
    DInst *dinst = ROB.getData(unresolvedLoad);
    if (!dinst->isResolved() && dinst->getInst()->isLoad())
      break; // only stop advancing pointer for an unresolved load
             // (faster than waiting for an un-executed load)

    ushort robPos = ROB.getNextId(unresolvedLoad);
    if (ROB.isEnd(robPos)) {
      // There are no more instructions. The Ul covers the whole ROB
#ifdef DEBUG
      // Verify that all loads are resolved
      robPos = ROB.getIdFromTop(0);
      while(!ROB.isEnd(robPos)) {
        DInst *dinst = ROB.getData(robPos);
        I(!(dinst->getInst()->isLoad() && !dinst->isResolved()));
        robPos = ROB.getNextId(robPos);
      }
#endif
      unresolvedLoad = -1;
      break;
    }

    unresolvedLoad = robPos;

    // BEGIN RECYCLE FOR Ul advance

    // Store recycle at min(Ul,Ub)
    if (recycleStore == -1 || recycleStore != unresolvedBranch)
      recycleStore = unresolvedLoad;

    if (recycleStore == unresolvedLoad) {
      dinst = ROB.getData(unresolvedLoad);
      if (dinst->getInst()->isStore() && !dinst->isEarlyRecycled())
        dinst->getResource()->earlyRecycle(dinst);
    }
  }
}

void GProcessor::propagateUnresolvedStore()
{
  I(unresolvedStore>=0);
  while(1) {
    DInst *dinst = ROB.getData(unresolvedStore);
    if (!dinst->isResolved() && dinst->getInst()->isStore())
      break; // only stop advancing pointer for an unresolved Store
             // (faster than waiting for an un-executed Store)

    ushort robPos = ROB.getNextId(unresolvedStore);
    if (ROB.isEnd(robPos)) {
      // There are no more instructions. The Ul covers the whole ROB
#ifdef DEBUG
      // Verify that all stores are resolved
      robPos = ROB.getIdFromTop(0);
      while(!ROB.isEnd(robPos)) {
        DInst *dinst = ROB.getData(robPos);
        I(!(dinst->getInst()->isStore() && !dinst->isResolved()));
        robPos = ROB.getNextId(robPos);
      }
#endif
      unresolvedStore = -1;
      break;
    }

    unresolvedStore = robPos;
    // BEGIN RECYCLE for Us advance
    
    // LDQ entry recycled when Us
    dinst = ROB.getData(unresolvedStore);
    if (dinst->getInst()->isLoad() && !dinst->isEarlyRecycled()) {
      // Entry can be recycled
      dinst->getResource()->earlyRecycle(dinst);
    }
  }

  // checkRegisterRecycle(); all the time is an overkilll. Checked
  // when branches are resolved
}

void GProcessor::propagateUnresolvedBranch()
{
  I(unresolvedBranch>=0);
  while(1) {
    DInst *dinst = ROB.getData(unresolvedBranch);
    if (!dinst->isResolved() && dinst->getInst()->isBranch())
      break; // only stop advancing pointer for an unresolved branch
             // (faster than waiting for an un-executed branch)

    ushort robPos = ROB.getNextId(unresolvedBranch);
    if (ROB.isEnd(robPos)) {
      // There are no more instructions. The Ul covers the whole ROB
#ifdef DEBUG
      // Verify that all stores are resolved
      robPos = ROB.getIdFromTop(0);
      while(!ROB.isEnd(robPos)) {
        DInst *dinst = ROB.getData(robPos);
        I(!(dinst->getInst()->isBranch() && !dinst->isResolved()));
        robPos = ROB.getNextId(robPos);
      }
#endif
      unresolvedBranch = -1;
      break;
    }

    unresolvedBranch = robPos;

    // BEGIN RECYCLE FOR Ul advance

    // Store recycle at min(Ul,Ub)
    if (recycleStore == -1 || recycleStore != unresolvedLoad)
      recycleStore = unresolvedBranch;

    if (recycleStore == unresolvedBranch) {
      dinst = ROB.getData(unresolvedBranch);
      if (dinst->getInst()->isStore() && !dinst->isEarlyRecycled())
        dinst->getResource()->earlyRecycle(dinst);
    }
  }

  checkRegisterRecycle();

}
#endif
