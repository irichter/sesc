/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  James Tuck
                  Liu Wei
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

#ifndef TASKCONTEXT_H
#define TASKCONTEXT_H

#include <vector>
#include <queue>
#include <stack>

#include "estl.h"
#include "nanassert.h"
#include "Snippets.h"
#include "pool.h"

#include "OSSim.h"
#include "ThreadContext.h"
#include "MemBuffer.h"

#include "HVersion.h"

#include "DInst.h"
#include "BPred.h"

class TaskContext {
 private:
#ifdef OOO_PAPER_STATS
  class TaskEntry {
  public:
    long nSpawns;
    long long nInst;
    bool outOrder;
    TaskEntry() {
      nSpawns = 0;
      nInst = 0;
      outOrder = false;
    }
  };
  typedef HASH_MAP<PAddr, TaskEntry *> TaskEntriesType;
  static TaskEntriesType taskEntries;
#endif

  /******** Local Class Definitions *********/
  class Hash4TaskContext {
  public:
    size_t operator()(const TaskContext *tc) const {
      return (long)tc;
    }
  };

  /************* Static Data ***************/
  
  static pool<TaskContext, true> tcPool;
  friend class pool<TaskContext, true>;

  static unsigned long long writeData;

  static int  SyncOnRestart; // # of restarts to support before wait to become safe

  static size_t MLThreshold; // Merge Last Threshold
  static size_t MFThreshold; // Merge First Threshold

  static GStatsCntr *nRestartInvMem;
  static GStatsCntr *nRestartException;
  static GStatsCntr *nRestartBubble;

  static GStatsCntr *nMergeNext;
  static GStatsCntr *nMergeLast;
  static GStatsCntr *nMergeSuccessors;
  static GStatsCntr *nInOrderSpawn;
  static GStatsCntr *nOutOrderSpawn;
  static GStatsCntr *nCorrectInOrderSpawn;
  static GStatsCntr *nCorrectOutOrderSpawn;

#ifdef OOO_PAPER_STATS
  static GStatsCntr *nOOSpawn;
  static GStatsCntr *nIOSpawn;
  static GStatsCntr *nOOInst;
  static GStatsCntr *nIOInst;
  static GStatsCntr *nOOSpawn2;
  static GStatsCntr *nIOSpawn2;
  static GStatsCntr *nOOInst2;
  static GStatsCntr *nIOInst2;
#ifdef TS_COUNT_TASKS_AHEAD
  static GStatsAvg  *nTasksAhead;
#endif
#endif

  static GStatsCntr *thReadEnergy;  // ETODO
  static GStatsCntr *thWriteEnergy; // ETODO
  static GStatsCntr *thFillEnergy;  // ETODO

#ifdef TS_GOAHEAD
  static GStatsCntr *nIgnoredRestarts;
#endif


  static GStatsCntr *nDataDepViolation[DataDepViolationAtMax];

#ifdef ATOMIC
  static size_t transPenalty;

  static GStatsCntr *nTransactions;
  static GStatsCntr *nSquashes;
  static GStatsCntr* transTime;
#endif

// Histogram statistics
  static const size_t MaxThreadsHist=16;
  static GStatsCntr *numThreads[];
  static GStatsCntr *numRunningThreads[];

  /************* Instance Data ***************/

  typedef std::set<Pid_t> Pid_tSet;
  Pid_tSet usedThreads; // TODO: look for a better name

  Pid_t  tid;
  ThreadContext sContext;

  HVersion *memVer;

  MemBuffer *memBuffer;

  int nLocalRestarts; // # of restarts that the task received

  int nLMergeNext; // # mergeNext that this TC has done

  PAddr spawnAddr;
  long NES; // Number of Exists to Skipt (mergeNext)
  BPred::HistoryType startBPHistory; // Original history used
  BPred::HistoryType newBPHistory;
  BPred *currentBranchPredictor;

  int dataDepViolationID; // Latest restart to wait for

  bool wasSpawnedOO;   // true if the current task was spawned Out-Of-Order
  bool ooTask;         // true if the current task was Out-Of-Order
  bool dataDepViolation; // true if the task has a pending restart

  bool canEarlyAwake; // Early awake (even before receiving the commit token)

#ifdef TS_GOAHEAD
  bool goingAhead;
  bool needRestart;
#endif


protected:
  typedef std::vector<TaskContext *> Tid2TaskContextType;
  static Tid2TaskContextType tid2TaskContext;

  /********** create/destroy *********/
  TaskContext();

  void setFields(Pid_t tid, HVersion *v);

  // Return a pthread to mint 
  static void freeThread(Pid_t pid);

  void destroy();

  // Merge the TaskContext with main memory
  void mergeDestroy();

  // Call freeThread for all the pids in the TaskContext but the main one (so
  // that it can be restarted). TaskContext clears the state which means that
  // all the other TaskContext forget dependence with it.
  void clearState();
  void clearStateSafe();

  static PAddr adjustChildSpawnAddr(PAddr childAddr);
  static PAddr calcChildSpawnAddr(PAddr childAddr);

  /********** mergeNext/mergeLast *********/

  void mergeLast(bool inv);
  void mergeSuccessors(bool inv);

  /********** Sync *********/
public:

#ifdef TS_PARANOID
  bool bad_reg[68];
#endif

  // Once a thread becomes safe, this function is called and it awakes the
  // threads in the local TaskContext that were waiting to become safe
  void awakeIfWaiting();

  void syncBecomeSafe(bool earlyAwake=false);
  bool canSyncBecomeSafe() const {
    if(memVer->isSafe())
      return false;

    ProcessId *proc = ProcessId::getProcessId(tid);
    return !proc->isWaiting() && !proc->isExecuted();
  }

  static void collect();
  static StaticCallbackFunction0<TaskContext::collect> collectCB;

  static void preBoot();
  static void postBoot();

  static void tryPropagateSafeToken();

  // Spawn a successor for current version: versionmem interface
  void spawnSuccessor(Pid_t chilPid, PAddr childAddr);

  void normalFork(Pid_t cpid);

  void endTaskExecuted(Pid_t fpid);

  bool hasDataDepViolation() const { return dataDepViolation; }
  int addDataDepViolation() {
    I(!memVer->isSafe());

#ifdef TS_GOAHEAD
    nIgnoredRestarts->inc();
    goingAhead = true;
#endif

    dataDepViolation = true;
    dataDepViolationID++;
    return dataDepViolationID;
  }

  void invalidMemAccess(int rID, DataDepViolationAt dAt = DataDepViolationAtExe);
  void exception();


  // Merging task functionality. Those both functions are called for
  // all the fork_successor and spawns.
  bool canMergeNext();
  int  ignoreExit();

  // NOTE: restartedBy killedBy should be called ONLY by taskHandler
  void localRestart();
  void localKill(bool inv);

  RAddr read(ulong iAddr, short iFlags, RAddr addr) {
    return memBuffer->read(iAddr, iFlags, addr);
  }
  // Prepare to write to this version. Returns the address to write to.
  RAddr preWrite(RAddr addr) {
    return (RAddr)(&writeData)+MemBufferEntry::calcChunkOffset(addr);
  }
  const HVersion *postWrite(ulong iAddr, short iFlags, RAddr addr) {
    return memBuffer->postWrite(&writeData, iAddr, iFlags, addr);
  }

  // Returns the latest available TaskContext for a give tid (pid). Notice, that
  // for each thread several TaskContexts may exists. This getTaskContext only
  // returns the latest one.
  static TaskContext *getTaskContext(Pid_t tid) {
    return tid2TaskContext[tid];
  }

  Pid_t  getPid()  const { return tid; }

  const HVersion *getVersionRef() const { return memVer; }
  HVersion *getVersion() const { return memVer; }
  HVersion *getVersionDuplicate() const { 
    return memVer->duplicate(); 
  }

// Notice that HVersion is the responsible
//  bool isSafe() const     { return memVer->isSafe(); }
//  bool isFinished() const { return memVer->isFinished(); }
  
  PAddr getSpawnAddr() const { return spawnAddr; }

  static void dumpAll();

  BPred::HistoryType getBPHistory(BPred::HistoryType cHistory);
  void setBPHistory(BPred::HistoryType h);

  void setBPred(BPred *branchPredictor) {
    I(currentBranchPredictor==0);
    currentBranchPredictor = branchPredictor;
  }
  void clearBPred() {
    I(currentBranchPredictor);
    currentBranchPredictor = 0;
  }

  static void report();

  void setOOtask();
  bool isOOtask() const { return ooTask; }

#ifdef TS_GOAHEAD
  void setGoingAhead(bool ga) {
    goingAhead = ga;
  }
  bool isGoingAhead() const {
    return goingAhead;
  }

  void setNeedRestart(bool nr) {
    needRestart = nr;
  }
  bool isNeedRestart() const {
    return needRestart;
  }
#endif

#ifdef ATOMIC
private:
  bool overflowed; /* a cache set */
  bool restartCounted;
  bool suspOnTransaction;
  bool suspOnBackoff;
  int startTime;
public:
  void spawn(Pid_t childPid);
  void newVersionFixup(Pid_t pid, HVersion* v);
  void commitTransaction(int pid);
  void startTransaction(int pid);
  void resume(int pid);
  void atomicStall(Time_t stallTime);

  //  bool isRestarting() { return invAccess; }

  bool hasOverflowed() const { return overflowed; }
  void setOverflowed() { overflowed = true; }
#endif
  
};

#endif // TASKCONTEXT_H
