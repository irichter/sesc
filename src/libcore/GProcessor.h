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

#ifndef GPROCESSOR_H
#define GPROCESSOR_H

// Generic Processor Interface.
// 
// This class is a generic interface for Processors. It has been
// design for Traditional and SMT processors in mind. That's the
// reason why it manages the execution engine (RDEX).

#include "callback.h"
#include "Cluster.h"
#include "Events.h"
#include "Instruction.h"
#include "FastQueue.h"
#include "GStats.h"
#include "nanassert.h"
#include "Pipeline.h"
#include "Resource.h"
#include "Snippets.h"

class FetchEngine;
class GMemorySystem;
class BPredictor;

class GProcessor {
private:
protected:
  // Per instance data
  const CPU_t Id;
  const int FetchWidth;
  const int IssueWidth;
  const int RetireWidth;
  const int RealisticWidth;
  const int InstQueueSize;
  const bool InOrderCore;
  const size_t MaxFlows;
  const size_t MaxROBSize;

  GMemorySystem *memorySystem;

  FastQueue<DInst *> ROB;

#ifdef SESC_CHERRY
  int unresolvedLoad;   // Ul in cherry paper
  int unresolvedStore;  // Us in cherry paper
  int unresolvedBranch; // Ub in cherry paper

  int recycleStore;   // min(Ul,Ub)
  DInst *cherryRAT[NumArchRegs];
#endif

  ClusterManager clusterManager;

  // Normal Stats
  GStatsAvg    robUsed;

  // Energy Counters
  GStatsEnergy *renameEnergy;
  GStatsEnergy *robEnergy;
  GStatsEnergyBase *wrRegEnergy[3]; // 0 INT, 1 FP, 2 NONE
  GStatsEnergyBase *rdRegEnergy[3]; // 0 INT, 1 FP, 2 NONE

  signed int regPool[INSTRUCTION_MAX_DESTPOOL];
#ifdef SESC_MISPATH
  signed int misRegPool[INSTRUCTION_MAX_DESTPOOL];
#endif

  // Updated by Processor or SMTProcessor. Shows the number of clocks
  // that the processor have been active (fetch + exe engine)
  Time_t clockTicks;

  ID(long prevDInstID);

  GStatsCntr *nStall[MaxStall];
  GStatsCntr *nInst[MaxInstType];
#ifdef SESC_MISPATH
  GStatsCntr *nInstFake[MaxInstType];
#endif

  // Construction
  void buildInstStats(GStatsCntr *nInstFake[MaxInstType], const char *txt);

  void buildUnit(const char *clusterName, GMemorySystem *ms, Cluster *cluster, InstType type
		 ,GStatsEnergyCGBase *ecg);
  void buildCluster(const char *clusterName, GMemorySystem * ms);
  void buildClusters(GMemorySystem *ms);

  GProcessor(GMemorySystem *gm, CPU_t i, size_t numFlows);

  virtual StallCause addInst(DInst *dinst) = 0;
  StallCause sharedAddInst(DInst *dinst);
  int issue(PipeQueue &pipeQ);
  void retire();

  virtual FetchEngine *currentFlow() = 0;
public:
  //Lock counters
  GStatsCntr nLocks;
  GStatsCntr nLockContCycles;

  virtual ~GProcessor();
  CPU_t getId() const { return Id; }

  GMemorySystem *getMemorySystem() const { return memorySystem; }

  // Returns the maximum number of flows this processor can support 
  size_t getMaxFlows(void) const { return MaxFlows; }

  void addEvent(EventType ev, CallbackBase *cb, long vaddr);

  void report(const char *str);

  Time_t getClockTicks() const { return clockTicks; }

  // Returns the context of thread pid
  // when called, the processor must be running thread pid
  virtual ThreadContext *getThreadContext(Pid_t pid)=0;
  
  // Stores the context of thread pid into Threads
  // When called, the processor must be running thread  pid
  virtual void saveThreadContext(Pid_t pid)=0;

  // Loads the context of thread pid from Threads
  // When called, the processor must be running thread pid
  virtual void loadThreadContext(Pid_t pid)=0;

  // Sets the program counter of the thread to picode
  // When called, the processor must be running the thread
  virtual void setInstructionPointer(Pid_t pid, icode_ptr picode)=0;

  // Returns the program counter of the thread
  // When called, the processor must be running the thread
  virtual icode_ptr getInstructionPointer(Pid_t pid)=0;

  // Inserts a fid in the processor. Notice that before called it must be sure
  // that availableFlows is bigger than one
  virtual void switchIn(Pid_t pid) = 0;

  // Extracts the pid. Precondition: the pid should be running in the processor
  virtual void switchOut(Pid_t pid) = 0;
  virtual long long getAndClearnGradInsts(Pid_t pid)  = 0; // Must be called only by RunningProcs
  virtual long long getAndClearnWPathInsts(Pid_t pid) = 0; // Must be called only by RunningProcs
  
  // Returns the number of extra threads (switchIn) that processor may accept
  virtual size_t availableFlows() const = 0;

  virtual void goRabbitMode(long long n2Skip) = 0;

  // Find a victim pid that can be switchout
  virtual Pid_t findVictimPid() const = 0;

  // Different types of cores extend this function. See SMTProcessor and
  // Processor.
  virtual void advanceClock() = 0;

  virtual bool hasWork() const=0;

#ifdef SESC_CHERRY
  void checkRegisterRecycle();
  void propagateUnresolvedLoad();
  void propagateUnresolvedStore();
  void propagateUnresolvedBranch();
#endif

#ifdef SESC_MISPATH
  virtual void misBranchRestore(DInst *dinst)= 0;
#endif
};

#endif   // GPROCESSOR_H
