/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
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

#include "DInst.h"

#include "Cluster.h"
#include "FetchEngine.h"
#include "OSSim.h"
#include "LDSTBuffer.h"
#include "Resource.h"

#if (defined TLS)
#include "Epoch.h"
#endif

pool<DInst> DInst::dInstPool(512, "DInst");

#ifdef DEBUG
int DInst::currentID=0;
#endif

#ifdef SESC_BAAD
int DInst::fetchQSize  = 0;
int DInst::issueQSize  = 0;
int DInst::schedQSize  = 0;
int DInst::exeQSize    = 0;
int DInst::retireQSize = 0;

GStatsTimingHist *DInst::fetchQHist1  = 0;
GStatsTimingHist *DInst::issueQHist1  = 0;
GStatsTimingHist *DInst::schedQHist1  = 0;
GStatsTimingHist *DInst::exeQHist1    = 0;
GStatsTimingHist *DInst::retireQHist1 = 0;

GStatsHist *DInst::fetchQHist2  = 0;
GStatsHist *DInst::issueQHist2  = 0;
GStatsHist *DInst::schedQHist2  = 0;
GStatsHist *DInst::exeQHist2    = 0;
GStatsHist *DInst::retireQHist2 = 0;

GStatsHist *DInst::fetchQHistUp = 0;
GStatsHist *DInst::issueQHistUp = 0;
GStatsHist *DInst::schedQHistUp = 0;
GStatsHist *DInst::exeQHistUp   = 0;
GStatsHist *DInst::retireQHistUp= 0;

GStatsHist *DInst::fetchQHistDown = 0;
GStatsHist *DInst::issueQHistDown = 0;
GStatsHist *DInst::schedQHistDown = 0;
GStatsHist *DInst::exeQHistDown   = 0;
GStatsHist *DInst::retireQHistDown= 0;

GStatsHist **DInst::avgFetchQTime  = 0;
GStatsHist **DInst::avgIssueQTime  = 0;
GStatsHist **DInst::avgSchedQTime  = 0;
GStatsHist **DInst::avgExeQTime    = 0;
GStatsHist **DInst::avgRetireQTime = 0;
#endif

DInst::DInst()
  :doAtSimTimeCB(this)
  ,doAtSelectCB(this)
  ,doAtExecutedCB(this)
{
  pend[0].init(this);
  pend[1].init(this);
  I(MAX_PENDING_SOURCES==2);
  nDeps = 0;

#ifdef SESC_BAAD
  if (avgFetchQTime == 0) {
    int maxType = static_cast<int>(MaxInstType);
    avgFetchQTime  = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
    avgIssueQTime  = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
    avgSchedQTime  = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
    avgExeQTime    = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
    avgRetireQTime = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
    for(int i = 1; i < maxType ; i++) {
      avgFetchQTime[i]  = new GStatsHist("BAAD_%sFrontQTime" ,Instruction::opcode2Name(static_cast<InstType>(i)));
      avgIssueQTime[i]  = new GStatsHist("BAAD_%sDepQTime"   ,Instruction::opcode2Name(static_cast<InstType>(i)));
      avgSchedQTime[i]  = new GStatsHist("BAAD_%sSchQTime"   ,Instruction::opcode2Name(static_cast<InstType>(i)));
      avgExeQTime[i]    = new GStatsHist("BAAD_%sExeQTime"   ,Instruction::opcode2Name(static_cast<InstType>(i)));
      avgRetireQTime[i] = new GStatsHist("BAAD_%sComRetQTime",Instruction::opcode2Name(static_cast<InstType>(i)));
    }

    fetchQHist1  = new GStatsTimingHist("BAAD_FrontQHist1");
    issueQHist1  = new GStatsTimingHist("BAAD_DepQHist1");
    schedQHist1  = new GStatsTimingHist("BAAD_SchQHist1");
    exeQHist1    = new GStatsTimingHist("BAAD_ExeQHist1");
    retireQHist1 = new GStatsTimingHist("BAAD_ComRetQHist1");

    fetchQHist2  = new GStatsHist("BAAD_FrontQHist2");
    issueQHist2  = new GStatsHist("BAAD_DepQHist2");
    schedQHist2  = new GStatsHist("BAAD_SchQHist2");
    exeQHist2    = new GStatsHist("BAAD_ExeQHist2");
    retireQHist2 = new GStatsHist("BAAD_ComRetQHist2");

    fetchQHistUp = new GStatsHist("BAAD_FrontQHistUp");
    issueQHistUp = new GStatsHist("BAAD_DepQHistUp");
    schedQHistUp = new GStatsHist("BAAD_SchQHistUp");
    exeQHistUp   = new GStatsHist("BAAD_ExeQHistUp");
    retireQHistUp= new GStatsHist("BAAD_ComRetQHistUp");

    fetchQHistDown  = new GStatsHist("BAAD_FrontQHistDown");
    issueQHistDown  = new GStatsHist("BAAD_DepQHistDown");
    schedQHistDown  = new GStatsHist("BAAD_SchQHistDown");
    exeQHistDown    = new GStatsHist("BAAD_ExeQHistDown");
    retireQHistDown = new GStatsHist("BAAD_ComRetQHistDown");
  }
#endif
}

void DInst::dump(const char *str)
{
  fprintf(stderr,"%s:(%d)  DInst: vaddr=0x%x ", str, cId, (int)vaddr);
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

void DInst::doAtSimTime()
{
  I( resource );

  I(!isExecuted());

  I(resource->getCluster());

  if (!isStallOnLoad())
    resource->getCluster()->wakeUpDeps(this);

#ifdef SESC_BAAD
  setSchedTime();
#endif

  resource->simTime(this);
}

void DInst::doAtSelect()
{
  I(resource->getCluster());
  resource->getCluster()->select(this);
}

void DInst::doAtExecuted()
{
  I(RATEntry);
  if ( (*RATEntry) == this )
    *RATEntry = 0;

  I( resource );
  resource->executed(this);
}

#if (defined TLS)
DInst *DInst::createDInst(const Instruction *inst, VAddr va, int cId, tls::Epoch *epoch)
#else
DInst *DInst::createDInst(const Instruction *inst, VAddr va, int cId)
#endif
{
#ifdef SESC_MISPATH
  if (inst->isType(iOpInvalid))
    return 0;
#endif

  DInst *i = dInstPool.out();

#ifdef SESC_BAAD
  i->fetchTime  = 0;
  i->renameTime = 0;
  i->issueTime  = 0;
  i->schedTime  = 0;
  i->exeTime    = 0;
#endif

  i->inst       = inst;
  Prefetch(i->inst);
  i->cId        = cId;
  i->wakeUpTime = 0;
  i->vaddr      = va;
  i->first      = 0;
#ifdef SESC_SEED
  i->predParent   = 0;
  i->nDepTableEntries = 0;
  i->overflowing  = false;
  i->stallOnLoad  = false;
#endif
#ifdef DEBUG
  i->ID = currentID++;
#endif
  i->resource  = 0;
  i->RATEntry  = 0;
  i->pendEvent = 0;
  i->fetch = 0;
  i->loadForwarded= false;
  i->issued       = false;
  i->executed     = false;
  i->depsAtRetire = false;
  i->deadStore    = false;
  i->resolved     = false;
  i->deadInst     = false;
  i->waitOnMemory = false;
#ifdef SESC_CHERRY
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
  i->myEpoch=epoch;
#endif
#ifdef DINST_PARENT
  i->pend[0].setParentDInst(0);
  i->pend[1].setParentDInst(0);
#endif

  i->pend[0].isUsed = false;
  i->pend[1].isUsed = false;
    
  return i;
}

#if (defined TLS)
DInst *DInst::createInst(InstID pc, VAddr va, int cId, tls::Epoch *epoch)
#else
DInst *DInst::createInst(InstID pc, VAddr va, int cId)
#endif
{
  const Instruction *inst = Instruction::getInst(pc);
#if (defined TLS)
  return createDInst(inst, va, cId, epoch);
#else
  return createDInst(inst, va, cId);
#endif
}

DInst *DInst::clone() 
{
#if (defined TLS)
  DInst *newDInst = createDInst(inst, vaddr, cId, myEpoch);
#else
  DInst *newDInst = createDInst(inst, vaddr, cId);
#endif

#ifdef TASKSCALAR
  // setting the LVID for the cloned instruction
  // this will call incOutsReqs for the HVersion.
  newDInst->setLVID(lvid, lvidVersion);
#endif

#ifdef SESC_BAAD
  I(0);
#endif

  return newDInst;
}

void DInst::killSilently()
{
  I(getPendEvent()==0);
  I(getResource()==0);

#ifdef SESC_BAAD
  if (renameTime == 0) {
    fetchQSize--;
  }else if (issueTime == 0) {
    issueQSize--;
  }else if (schedTime == 0) {
    schedQSize--;
  }else if (exeTime == 0) {
    exeQSize--;
  }else{
    retireQSize--;
  }
#endif

  markIssued();
  markExecuted();
  if( getFetch() ) {
    getFetch()->unBlockFetch();
    IS(setFetch(0));
  }

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
  notifyDataDepViolation(DataDepViolationAtRetire);

  if (lvid) { // maybe got killSilently
    lvid = 0;
    lvidVersion->decOutsReqs();
    lvidVersion->garbageCollect();
    IS(lvidVersion=0);
  }
  
  I(lvidVersion==0);
#endif

  I(!getFetch());

  if (hasDeps())
    return;
  
  I(nDeps == 0);   // No deps src

#if (defined TLS)
  I(!myEpoch);
#endif

  I(!getFetch());
  dInstPool.in(this); 
}

void DInst::scrap()
{
  I(nDeps == 0);   // No deps src
  I(first == 0);   // no dependent instructions 

#ifdef SESC_BAAD
  I(issued && executed);
#endif

#ifdef TASKSCALAR
  notifyDataDepViolation(DataDepViolationAtRetire);

  if (lvid) { // maybe got killSilently
    lvid = 0;
    lvidVersion->decOutsReqs();
    lvidVersion->garbageCollect();
    IS(lvidVersion=0);
  }

  I(lvidVersion==0);
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

  I(!getFetch());
  dInstPool.in(this);
}

void DInst::destroy()
{
  I(nDeps == 0);   // No deps src

  I(!fetch); // if it block the fetch engine. it is unblocked again

  I(issued);
  I(executed);

  awakeRemoteInstructions();

  I(first == 0);   // no dependent instructions 
  if (first)
    LOG("Instruction pc=0x%x failed first is pc=0x%x",(int)inst->getAddr(),(int)first->getDInst()->inst->getAddr());

  scrap();
}

#ifdef TASKSCALAR
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

#ifdef SESC_BAAD
void DInst::setFetchTime()
{
  I(fetchTime == 0);
  fetchTime = globalClock;

  fetchQHistUp->sample(fetchQSize);
  fetchQSize++;

  fetchQHist1->sample(fetchQSize);  
  fetchQHist2->sample(fetchQSize);  
}

void DInst::setRenameTime()
{
  I(renameTime == 0);
  renameTime = globalClock;

  fetchQHistDown->sample(fetchQSize);
  fetchQSize--;
  issueQHistUp->sample(issueQSize);
  issueQSize++;

  fetchQHist2->sample(fetchQSize);  

  issueQHist1->sample(issueQSize);  
  issueQHist2->sample(issueQSize);  
}

void DInst::setIssueTime()
{
  I(issueTime == 0);
  issueTime = globalClock;

  issueQHistDown->sample(issueQSize);
  issueQSize--;

  schedQHistUp->sample(schedQSize);
  schedQSize++;

  issueQHist2->sample(issueQSize);  

  schedQHist1->sample(schedQSize);  
  schedQHist2->sample(schedQSize);  
}

void DInst::setSchedTime()
{
  I(schedTime == 0);
  schedTime = globalClock;

  schedQHistDown->sample(schedQSize);
  schedQSize--;

  exeQHistUp->sample(exeQSize);
  exeQSize++;

  schedQHist2->sample(schedQSize);  

  exeQHist1->sample(exeQSize);
  exeQHist2->sample(exeQSize);
}

void DInst::setExeTime()
{
  I(exeTime == 0);
  exeTime = globalClock;

  exeQHistDown->sample(exeQSize);
  exeQSize--;

  retireQHistUp->sample(retireQSize);
  retireQSize++;

  exeQHist2->sample(exeQSize);

  retireQHist1->sample(retireQSize);
  retireQHist2->sample(retireQSize);
}

void DInst::setRetireTime()
{
  I(fetchTime);
  I(renameTime);
  I(issueTime);
  I(schedTime);
  I(exeTime);

  InstType i = inst->getOpcode();
  // Based on instruction type keep statistics
  avgFetchQTime[i]->sample(renameTime-fetchTime);
  avgIssueQTime[i]->sample(issueTime-renameTime);
  avgSchedQTime[i]->sample(schedTime-issueTime);
  avgExeQTime[i]->sample(exeTime-schedTime);
  avgRetireQTime[i]->sample(globalClock-exeTime);

  retireQHistDown->sample(retireQSize);
  retireQSize--;

  int pc = inst->getAddr();
  if (pc) {
    printf("BAAD: wp=%d pc=0x%x op=%d src1=%d src2=%d dest=%u "
           ,isFake()?1:0
           ,pc,inst->getOpcode()
           ,inst->getSrc1(), inst->getSrc1(), inst->getDest()
           );
    
    if (inst->isMemory())
      printf(" addr=0x%x time=%d", getVaddr(), (int)(exeTime-schedTime));
    else if (getFetch())
      printf(" missBr");
      
    
    printf("\n");
  }
}
#endif

