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

#ifndef FETCHENGINE_H
#define FETCHENGINE_H

#include "nanassert.h"

#include "ExecutionFlow.h"
#include "BPred.h"
#include "GStats.h"
#include "Events.h"

#ifdef TASKSCALAR
#include "TaskContext.h"
#endif

class GMemorySystem;
class IBucket;

class FetchEngine {
private:
  // Static data
  static long long nInst2Sim;
  static long long totalnInst;
  
  const int Id;
  const int cpuId;
  GMemorySystem *gms;

  Pid_t pid;

  BPredictor *bpred;
  
#ifdef TRACE_DRIVEN
  TraceFlow flow;
#else
  ExecutionFlow flow;
#endif
  
  ushort FetchWidth;
  
  ushort BB4Cycle;
  ushort maxBB;
  TimeDelta_t BTACDelay;
  TimeDelta_t IL1HitDelay;

#ifdef TASKSCALAR
  SubLVIDType subLVID;
  GLVID       *LVID;
#endif 

  // InstID of the address that generated a misprediction
  InstID missInstID;
  Time_t missFetchTime;

#ifdef SESC_MISPATH
  bool issueWrongPath;
#endif
  bool enableICache;

protected:
  bool processBranch(DInst *dinst, ushort n2Fetched);

  // ******************* Statistics section
  GStatsAvg  avgBranchTime;
  GStatsCntr nDelayInst1;
  GStatsCntr nDelayInst2;
  GStatsCntr nFetched;
  GStatsCntr nBTAC;
  long long nGradInsts;
  long long nWPathInsts;
  // *******************

public:
  FetchEngine(int cId, int i, GMemorySystem *gms, FetchEngine *fe = 0);
  ~FetchEngine();

  void addEvent(EventType ev, CallbackBase *cb, long vaddr) {
    flow.addEvent(ev,cb,vaddr);
  }
  
  // Fills the current fetch buffer.
  //  Always fetches at most fetchWidth instructions
  void fetch(IBucket *buffer);

  // Fake fetch. Fills the buffer with fake (mispredicted) instructions
  // Only active is SESC_MISPATH def'd
  void fakeFetch(IBucket *buffer);
  void realFetch(IBucket *buffer);

  ThreadContext *getThreadContext(void){
    I(flow.currentPid()!=-1);
    return flow.getThreadContext();    
  }

  void saveThreadContext(void){
    I(flow.currentPid()!=-1);
    flow.saveThreadContext(flow.currentPid());
  }
  void loadThreadContext(void){
    I(flow.currentPid()!=-1);
    flow.loadThreadContext(flow.currentPid());
  }
  void setInstructionPointer(icode_ptr picode){
    I(flow.currentPid()!=-1);
    flow.setInstructionPointer(picode);
  }
  icode_ptr getInstructionPointer(void){
    I(flow.currentPid()!=-1);
    return flow.getInstructionPointer();    
  }

  // -1 if there is no pid
  Pid_t getPid() const { return pid; }

  void goRabbitMode(long long n2Skip) {
    flow.goRabbitMode(n2Skip);
  }

  void unBlockFetch();
  StaticCallbackMember0<FetchEngine,&FetchEngine::unBlockFetch> unBlockFetchCB;

  void dump(const char *str) const;

  void switchIn(Pid_t i);
  void switchOut(Pid_t i);
  long long getAndClearnGradInsts() {
    long long tmp = nGradInsts;
    nGradInsts=0;
    return tmp;
  }
  long long getAndClearnWPathInsts() {
    long long tmp = nWPathInsts;
    nWPathInsts=0;
    return tmp;
  }

  int getCPUId() const { return cpuId; }

  BPredictor *getBPred() const { return bpred; }
  ExecutionFlow *getExecFlow(){ return &flow ; }

  bool hasWork() const { return pid >= 0; }

  static void setnInst2Sim(long long a) {
    nInst2Sim = a;
  }
};

#endif   // FETCHENGINE_H
