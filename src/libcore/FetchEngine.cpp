/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Milos Prvulovic
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

#include <alloca.h>

#include "SescConf.h"
#include "FetchEngine.h"
#include "OSSim.h"
#include "LDSTBuffer.h"
#include "GMemorySystem.h"

#include "GMemorySystem.h"
#include "GMemoryOS.h"
#include "MemRequest.h"
#include "Pipeline.h"

long long FetchEngine::nInst2Sim=0;
long long FetchEngine::totalnInst=0;

FetchEngine::FetchEngine(int cId
			 ,int i
			 // TOFIX: GMemorySystem is in GPRocessor too. This is
			 // not consistent. Remove it from GProcessor and be
			 // sure that everybody that uses it gets it from
			 // FetchEngine (note that an SMT may have multiple
			 // FetchEngine)
			 ,GMemorySystem *gmem
			 ,FetchEngine *fe)
  : Id(i)
  ,cpuId(cId)
  ,gms(gmem)
  ,pid(-1)
  ,flow(cId, i, gmem)
  ,avgBranchTime("FetchEngine(%d)_avgBranchTime", i)
  ,nDelayInst1("FetchEngine(%d):nDelayInst1", i)
  ,nDelayInst2("FetchEngine(%d):nDelayInst2", i) // Not enough BB/LVIDs per cycle 
  ,nFetched("FetchEngine(%d):nFetched",i)
  ,nBTAC("FetchEngine(%d):nBTAC", i) // BTAC corrections to BTB
  ,unBlockFetchCB(this)
{
  // Constraints

  SescConf->isLong("cpucore", "fetchWidth",cId);
  SescConf->isBetween("cpucore", "fetchWidth", 1, 1024, cId);
  FetchWidth = SescConf->getLong("cpucore", "fetchWidth", cId);

  SescConf->isBetween("cpucore", "bb4Cycle",0,1024,cId);
  BB4Cycle = SescConf->getLong("cpucore", "bb4Cycle",cId);
  if( BB4Cycle == 0 )
    BB4Cycle = USHRT_MAX;
  
  const char *bpredSection = SescConf->getCharPtr("cpucore","bpred",cId);
  
  if( fe )
    bpred = new BPredictor(i,bpredSection,fe->bpred);
  else
    bpred = new BPredictor(i,bpredSection);

  SescConf->isLong(bpredSection, "BTACDelay");
  SescConf->isBetween(bpredSection, "BTACDelay", 0, 1024);
  BTACDelay = SescConf->getLong(bpredSection, "BTACDelay");

  missInstID = 0;
#ifdef SESC_MISPATH
  issueWrongPath = SescConf->getBool("","issueWrongPath");
#endif
  nGradInsts  = 0;
  nWPathInsts = 0;

  // Get some icache L1 parameters
  enableICache = SescConf->getBool("","enableICache");
  if (enableICache) {
    IL1HitDelay = 0;
  }else{
    const char *iL1Section = SescConf->getCharPtr("cpucore","instrSource", cId);
    if (iL1Section) {
      char *sec = strdup(iL1Section);
      char *end = strchr(sec, ' ');
      if (end) 
	*end=0; // Get only the first word
      // Must be the i-cache
      SescConf->isInList(sec,"deviceType","icache");

      IL1HitDelay = SescConf->getLong(sec,"hitDelay");
    }else{
      IL1HitDelay = 1; // 1 cycle if impossible to find the information required
    }
  }
}

FetchEngine::~FetchEngine()
{
  // There should be a RunningProc::switchOut that clears the statistics
  I(nGradInsts  == 0);
  I(nWPathInsts == 0);

  delete bpred;
}

#ifdef TS_PARANOID
void FetchEngine::fetchDebugRegisters(const DInst *dinst)
{
  TaskContext *tc = dinst->getVersionRef()->getTaskContext();

  if(tc == 0)
    return;
  
  RegType src1,src2,dest;
  const Instruction *inst = dinst->getInt();
  src1 = inst->getSrc1();
  src2 = inst->getSrc2();
  dest = inst->getDest();
  if( inst->hasSrc1Register() 
      && !(src1==2 && dinst->getInst()->isBranch()) 
      && src1 < HiReg 
      && tc->bad_reg[src1] 
      && !dinst->getInst()->isStore())
    fprintf(stderr,"Bad problems on Reg[%d]@0x%x:0x%x\n",src1,dinst->getInst()->getAddr(),0);

  if( inst->hasSrc2Register() 
      && !(src2==2 && dinst->getInst()->isBranch()) 
      && src2 < HiReg 
      && tc->bad_reg[src2]
      && !dinst->getInst()->isStore())
    fprintf(stderr,"Bad problems on Reg[%d]@0x%x:0x:%x\n",src2,dinst->getInst()->getAddr(),0);

  if( inst->hasDestRegister() )
    tc->bad_reg[dest] = false;
}
#endif

bool FetchEngine::processBranch(DInst *dinst, ushort n2Fetched)
{
  const Instruction *inst = dinst->getInst();
  InstID oracleID         = flow.getNextID();
#ifdef BPRED_UPDATE_RETIRE
  PredType prediction     = bpred->predict(inst, oracleID, false);
  if (!dinst->isFake())
    dinst->setBPPred(bpred, oracleID);
#else
  PredType prediction     = bpred->predict(inst, oracleID, !dinst->isFake());
#endif

  if(prediction == CorrectPrediction) {
    if( oracleID != inst->calcNextInstID() ) {
      // Only when the branch is taken check maxBB
      maxBB--;
      if( maxBB == 0 ) {
	// No instructions fetched (stall)
	if (missInstID==0)
	  nDelayInst2.add(n2Fetched);
	return false;
      }
    }
    return true;
  }

#ifdef SESC_MISPATH
  if (missInstID==0 && !dinst->isFake()) { // Only first mispredicted instruction
    I(missFetchTime == 0);
    missFetchTime = globalClock;
    dinst->setFetch(this);
  }

  missInstID = inst->calcNextInstID();
#else
  I(missInstID==0);

  missInstID = inst->currentID();
  missFetchTime = globalClock;

  if( BTACDelay ) {
    if( prediction == NoBTBPrediction && inst->doesJump2Label() ) {
      nBTAC.inc();
      unBlockFetchCB.schedule(BTACDelay);
    }else{
      dinst->setFetch(this); // blocked fetch (awaked in Resources)
    }
  }else{
    dinst->setFetch(this); // blocked fetch (awaked in Resources)
  }
#endif // SESC_MISPATH

  return false;
}

void FetchEngine::realFetch(IBucket *bucket)
{
  ushort n2Fetched=FetchWidth;
  maxBB = BB4Cycle; // Reset the max number of BB to fetch in this cycle (decreased in processBranch)

  // This method only can be called once per cycle or the restriction of the
  // BB4Cycle would not enforced
  I(pid>=0);
  I(maxBB>0);
  I(bucket->empty());
  
  I(missInstID==0);

  Pid_t myPid = flow.currentPid();
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(myPid);
  I(tc);
  GLVID *lvid = gms->findCreateLVID(tc->getVersion());
  if (lvid==0) {
    // Not enough LVIDs. Stall fetch
    I(missInstID==0);
    nDelayInst2.add(n2Fetched);
    return;
  }
  HVersion *lvidVersion = tc->getVersion(); // no duplicate
#endif

  do {
    nGradInsts++; // Before executePC because it can trigger a context switch

    DInst *dinst = flow.executePC();

    if (dinst == 0)
      break;

#ifdef TASKSCALAR
    dinst->setLVID(lvid, lvidVersion);
#endif //TASKSCALAR

    n2Fetched--;
    
    bucket->push(dinst);

#ifdef TS_PARANOID
    fetchDebugRegisters(dinst);
#endif

    //    dinst->setCPUId(cpuId);
  
    if(dinst->getInst()->isBranch()) {
      if (!processBranch(dinst, n2Fetched))
	break;
      //    }else if(dinst->getInst()->isStore()) {
      // FIXME: add a getFetchStoreEntry
      //      LDSTBuffer::getStoreEntry(dinst);
    }

  }while(n2Fetched && flow.currentPid()==myPid);

#ifdef TASKSCALAR
  if (!bucket->empty())
    lvid->garbageCollect();
#endif

  ushort tmp = FetchWidth - n2Fetched;

  totalnInst+=tmp;
  if( totalnInst >= nInst2Sim ) {
    MSG("stopSimulation at %lld (%lld)",totalnInst, nInst2Sim);
    osSim->stopSimulation();
  }

  nFetched.add(tmp);
}

void FetchEngine::fakeFetch(IBucket *bucket)
{
  I(missInstID);
#ifdef SESC_MISPATH
  if(!issueWrongPath)
    return;

  ushort n2Fetched = FetchWidth;

  do {
    // Ugly note: 4 as parameter? Anything different than 0 works
    DInst *dinst = DInst::createInst(missInstID, 4, cpuId);
    I(dinst);

    dinst->setFake();

    n2Fetched--;
    bucket->push(dinst);

    const Instruction *fakeInst = Instruction::getInst(missInstID);
    if (fakeInst->isBranch()) {
      if (!processBranch(dinst, n2Fetched))
	break;
    }else{
      missInstID = fakeInst->calcNextInstID();
    }

  }while(n2Fetched);

  nFetched.add(FetchWidth - n2Fetched);
  nWPathInsts += FetchWidth - n2Fetched;
#endif // SESC_MISPATH
}

void FetchEngine::fetch(IBucket *bucket)
{
  if(missInstID) {
    fakeFetch(bucket);
  }else{
    realFetch(bucket);
  }

  if(enableICache && !bucket->empty()) {
    IMemRequest::create(bucket->top(), gms, bucket);
  }else{
    // Even if there are no inst to fetch, bucket.empty(), it should
    // be markFetched. Otherwise, we would loose count of buckets
    bucket->markFetchedCB.schedule(IL1HitDelay); 
  }
}

void FetchEngine::dump(const char *str) const
{
  char *nstr = (char *)alloca(strlen(str) + 20);

  sprintf(nstr, "%s_FE", str);

  bpred->dump(nstr);
  flow.dump(nstr);
}

void FetchEngine::unBlockFetch()
{
  I(missInstID);
  missInstID = 0;

  Time_t n = (globalClock-missFetchTime);

  avgBranchTime.sample(n);
  n *= FetchWidth;
  nDelayInst1.add(n);

  missFetchTime=0;
}

void FetchEngine::switchIn(Pid_t i) 
{
  I(pid==-1);
  I(i>=0);
  I(nGradInsts  == 0);
  I(nWPathInsts == 0);
  pid = i;

  bpred->switchIn(i);
  flow.switchIn(i);
}

void FetchEngine::switchOut(Pid_t i) 
{
  I(pid>=0);
  I(pid==i);
  pid = -1;
  flow.switchOut(i);
  bpred->switchOut(i);
}
