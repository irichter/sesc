#if !(defined _Epoch_hpp_)
#define _Epoch_hpp_

#define CollectVersionCountStats

#include "estl.h"

#include "SysCall.h"
#include "OSSim.h"
#include "nanassert.h"
#include "Snippets.h"
#include <map>
#include <list>

#include <iostream>
#include <algorithm>
#include "mintapi.h"
#include "ThreadContext.h"

typedef std::list<SysCall *> SysCallLog;

namespace tls{

  // Number of dynamic instructions
  typedef unsigned long long InstrCount;

  typedef Pid_t ThreadID;
  
  typedef std::set<class Epoch *> EpochSet;
  typedef std::list<class Epoch *> EpochList;

  // Type for a scalar value of a clock
  typedef long int ClockValue;
  // Initial value for a clock field (all actual values are greater)
  const ClockValue initialClockValue =0x00000000l;
  // Infinite value for a clock field (all actual values are less)
  const ClockValue infiniteClockValue=0x7fffffffl;
  // Extra clock difference added when synchronizing
  extern ClockValue syncClockDelta;

  // The type for a vector clock
  class VClock{
  public:
    // Type for a vector of clock values
    typedef std::vector<ClockValue> ClockVector;
    // The type for an index of a clock in ClockVector
    typedef ClockVector::size_type size_type; 
  private:
    typedef std::list<VClock *> VClockList;
    // A list of all instances of vector clocks
    static VClockList allVClocks;
    // Counts how many instances have a particular index
    typedef std::vector<size_t> IndexCounts;
    static IndexCounts indexCounts;
    static const size_type invalidIndex=static_cast<size_type>(-1);
    // Index of my component of the clock
    size_type myIndex;
    // The actual vector clock
    ClockVector myVector;
    // Position of this clock in the list
    VClockList::iterator myListIt;
    // A clock with initial values in all fields and an invalid index
    static VClock initial;
    // A clock with infinite values in all fields and an invalid index
    static VClock infinite;
    static size_type newIndex(size_type index){
      I(index!=invalidIndex);
      if(index>=indexCounts.size()){
	size_type oldSize=indexCounts.size();
	size_type newSize=index+1;
	indexCounts.resize(newSize,0);
	infinite.myVector.resize(newSize,infiniteClockValue);
	for(VClockList::iterator listIt=allVClocks.begin();
	    listIt!=allVClocks.end();listIt++)
	  (*listIt)->myVector.resize(newSize,initialClockValue);
      }
      I(!indexCounts[index]);
      return index;
    }
  public:
    enum CmpResult { Before, Unordered, After };
    static const VClock &getInitial(void){
      return initial;
    }
    static const VClock &getInfinite(void){
      return infinite;
    }
    // Constructor for a non-indexed vector clock
    VClock(void)
      : myIndex(invalidIndex), myVector(indexCounts.size()),
	myListIt(allVClocks.insert(allVClocks.end(),this)){
    }
    // Constructor of an initial indexed VClock
    // Starts from initial non-indexed clock increments the indexed component
    VClock(size_type index)
      : myIndex(newIndex(index)), myVector(initial.myVector),
	myListIt(allVClocks.insert(allVClocks.end(),this)){
      I(myVector.size()>myIndex);
      myVector[myIndex]++;
      I(indexCounts.size()>myIndex);
      indexCounts[myIndex]++;
    }
    VClock(const VClock &other)
      : myIndex(other.myIndex), myVector(other.myVector),
	myListIt(allVClocks.insert(allVClocks.end(),this)){
      I((indexCounts.size()>myIndex)&&(indexCounts[myIndex]));
      I(myVector.size()>myIndex);
      myVector[myIndex]++;
      I(indexCounts.size()>myIndex);
      indexCounts[myIndex]++;
    }
    ~VClock(void){
      I(myListIt!=allVClocks.end());
      I(myVector.size()==indexCounts.size());
      I(*myListIt==this);
      allVClocks.erase(myListIt);
      if(myIndex!=invalidIndex){
	indexCounts[myIndex]--;
	if(indexCounts[myIndex]==0){
	  size_type oldSize=indexCounts.size();
	  size_type newSize=indexCounts.size();
	  while(newSize>0){
	    if(indexCounts[newSize-1]>0)
	      break;
	    newSize--;
	  }
	  if(newSize<oldSize){
	    indexCounts.resize(newSize);
	    for(VClockList::iterator listIt=allVClocks.begin();
		listIt!=allVClocks.end();listIt++){
	      (*listIt)->myVector.resize(newSize);
	      if((*listIt)->myIndex==oldSize)
		(*listIt)->myIndex=newSize;
	    }
	  }else{
	    for(VClockList::iterator listIt=allVClocks.begin();
		listIt!=allVClocks.end();listIt++){
	      (*listIt)->myVector[myIndex]=initialClockValue;
	    }
	    infinite[myIndex]=infiniteClockValue;
	  }
	}
      }
    }
    static size_type size(void){
      return indexCounts.size();
    }
    size_type index(void) const{
      return myIndex;
    }
    ClockValue value(void) const{
      I(myIndex<myVector.size());
      return myVector[myIndex];
    }
    CmpResult compare(const VClock &other) const{
      if(myIndex==other.myIndex){
	I(myVector[myIndex]!=other.myVector[myIndex]);
	return (myVector[myIndex]<other.myVector[myIndex])?After:Before;
      }
      if(myVector[myIndex]<=other.myVector[myIndex]){
	I(myVector[other.myIndex]<other.myVector[other.myIndex]);
	return After;
      }
      if(myVector[other.myIndex]>=other.myVector[other.myIndex]){
	I(myVector[myIndex]>other.myVector[myIndex]);
	return Before;
      }
      return Unordered;
    }
    void succeed(const VClock &other){
      I(compare(other)==Unordered);
      for(size_type index=0;index<indexCounts.size();index++)
	if(other.myVector[index]>myVector[index])
	  myVector[index]=other.myVector[index];
    }
    ClockValue &operator[](size_type index){
      I(index<myVector.size());
      return myVector[index];
    }
    const ClockValue &operator[](size_type index) const{
      I(index<myVector.size());
      return myVector[index];
    }    
  };

  class Thread{
  private:
    // Thread ID for this thread
    ThreadID myID;
    // Vector of Thread pointers
    typedef std::vector<Thread *> ThreadVector;
    // All active threads, indexed by thread index
    static ThreadVector threadVector;
  public:
    // Type for a thread index
    typedef ThreadVector::size_type ThreadIndex;
  private:
    // Index of this thread in ThreadVector
    ThreadIndex myIndex;
    // The type for a (sorted) set of thread indices
    typedef std::set<ThreadIndex> IndexSet;
    // Set of available thread indices
    static IndexSet freeIndices;
  public:
    // All epochs in this thread that were created and not yet destroyed
    // List is sorted by SClock, front has highest SClock
    EpochList threadEpochs;
  private:
    // Position in threadEpochs of the last epoch to hold the
    // ThreadSafe status in this thread. Note that this position
    // may be the end() iterator of the threadEpochs list.
    EpochList::iterator threadSafe;    
    // The vector clock of the thread-safe epoch in this thread
    VClock threadSafeClk;
    // Not really a clock. Each entry contains a clock value.
    // The clock value at index "i" indicates a VClock value
    // of an epoch in this thread that can no longer miss any
    // races with thread "i"
    VClock noRacesMissed;
    // Position in threadEpochs of the last epoch to cross the
    // no-races-missed frontier. It is the youngest epoch in this
    // thread that can have no races with uncommitted epochs in
    // other threads. Note that this position may be the end()
    // iterator of the threadEpochs list.
    EpochList::iterator raceFrontier;
    // VClock value of the most recent raceFrontier candidate
    // If there is a candidate, this is the clock value of that
    // candidate. If no current candidate, this is the clock of
    // the current raceFrontier epoch (it was the last candidate)
    ClockValue nextFrontierClk;
    // Number of threads that can still have races with the most
    // recent raceFrontier candidate. If there is a current
    // candidate, this number should be non-zero. If there is no
    // current candidate, the number is equal to zero (the most
    // recent candidate has no more races with any threads and is
    // already the new raceFrontier)
    VClock::size_type raceFrontierHolders;
    // Called when the next-after-race-frontier epoch changes
    void newFrontierCandidate(void);
#if (defined DEBUG)
    // Remembers the last epoch in commitEpoch for debugging
    Epoch *lastCommitEpoch;
    // Remembers the last epoch to befome a frontier candidate
    Epoch *lastFrontierCandidate;
#endif // (defined DEBUG)
    bool exitAtEnd;
    int  myExitCode;
  public:
    Thread(ThreadID tid);
    ~Thread(void);
    // Adds a new epoch to this thread, inserting it into threadEpochs
    // Returns the position of the epoch in threadEpochs
    EpochList::iterator addEpoch(Epoch *epoch);
    // Removes an epoch from this thead
    // Returns true iff this was the last epoch in this thread
    bool removeEpoch(Epoch *epoch);
    // True iff no epoch from this thread is currently in the ThreadSafe state
    bool threadSafeAvailable;
    // Notifies the Thread that another epoch has become ThreadSafe
    void moveThreadSafe(void);
    // The VClock of the ThreadSafe epoch has changed
    // (We need to know it because the race frontier
    // is determined by the ThreadSafe clocks)
    void changeThreadSafeVClock(const VClock &newVClock);
    // Notifies the thread that exit was called
    void exitCalled(int exitCode){
      exitAtEnd=true;
      osSim->eventExit(myID,exitCode);
    }
    // Call to notify the Thread that one of its epochs became Committed
    void commitEpoch(class Epoch *epoch);
    // Internal function. Called when the race frontier advances
    void moveRaceFrontier(void);

    ThreadID getID(void) const{
      return myID;
    }
    ThreadIndex getIndex(void) const{
      return myIndex;
    }
    static Thread *getByIndex(ThreadIndex index){
      I(index<threadVector.size());
      return threadVector[index];
    }
    static ThreadIndex getIndexUb(void){
      return threadVector.size();
    }
  };
  
  class Epoch{
  public:
    static void staticConstructor(void);
    static void staticDestructor(void);
  private:
    
    // True iff data races should not be analyzed
    static bool dataRacesIgnored;
    // Maximum number of buffer blocks an epoch is allowed to have
    // A value of 0 means there is no limit
    static size_t maxBlocksPerEpoch;

    typedef std::vector<Epoch *> PidToEpoch;
    static PidToEpoch pidToEpoch;

    // All epochs that were created and not yet destroyed
    // List is sorted by SClock, front has highest SClock
    static EpochList allEpochs;
    static EpochList::iterator findPos(ClockValue sClock, EpochList::iterator hint=allEpochs.begin()){
      if((hint!=allEpochs.end())&&(sClock<=(*hint)->mySClock)){
	while((hint!=allEpochs.end())&&(sClock<(*hint)->mySClock))
	  hint++;
      }else{
	EpochList::reverse_iterator rhint(hint);
	while((rhint!=allEpochs.rend())&&(sClock>(*rhint)->mySClock))
	  rhint++;
	hint=rhint.base();
      }
      return hint;
    }

    // SESC context ID for this epoch
    Pid_t  myPid;
    // SESC context of this epoch at its very beginning
    ThreadContext myContext;
    
    // Create a SESC context for this thread, save its initial state, and create pid-to-epoch mapping
    void createContext(ThreadContext *context){
      I(myPid==-1);
      I(myState==State::Spawning);
      // Create SESC context
      myPid=mint_sesc_create_clone(context);
      ThreadContext *threadContext=osSim->getContext(myPid);
      threadContext->setEpoch(this);
      myContext=*threadContext;
      // Map the context's pid to this epoch
      I(myPid>0);
      PidToEpoch::size_type index=static_cast<PidToEpoch::size_type>(myPid);
      I((pidToEpoch.size()<=index)||pidToEpoch[index]==0);
      if(pidToEpoch.size()<=index)
	pidToEpoch.resize(index+1,0);
      I(pidToEpoch.size()>index);
      I(pidToEpoch[index]==0);
      pidToEpoch[index]=this;      
    }

    // Beginning time of the execution that got Decided
    Time_t beginDecisionExe;
    Time_t endDecisionExe;
    // Beginning time of the current execution
    Time_t beginCurrentExe;
    Time_t endCurrentExe;

    // Number of instructions this epoch is about to execute
    // in the current (or, if Ended, last executed) run
    long long pendInstrCount;
    // Number of instructions this epoch is has begun executing
    // in the current (or, if Ended, last executed) run
    long long execInstrCount; 
    // Number of instructions this epoch has fully executed
    // in the current (or, if Ended, last executed) run
    long long doneInstrCount;
    // Number of instructions this epoch is allowed to execute
    // If the epoch ever was Decided, this is the number of
    // instructions actually executed in the last run before it
    // became decided
    size_t maxInstrCount;
    
    // True iff epoch can execute
    // before sequential predecessors have completed
    const bool mySeqSpec;
    // True iff epoch can execute
    // before synchronization predecessors are known and have completed
    const bool mySyncSpec;

    friend class Thread;
    friend class Checkpoint;

    // The thread this epoch belongs to
    Thread *myThread;
    // Returns the thread this epoch belongs to
    Thread *getThread(void) const{
      return myThread;
    }

    VClock myVClock;
    
    // Scalar clock value of the epoch that spawned this one
    ClockValue parentSClock;
    // Scalar clock state for this epoch
    ClockValue mySClock;
    // Number of runtime adjustments of SClock in this epoch
    size_t runtimeSClockAdjustments;
    
    ClockValue getClock(void){
      return mySClock;
    }

    Checkpoint *myCheckpoint;

    // Position of this epoch in the allEpochs list
    EpochList::iterator myAllEpochsPos;
    // Position of this epoch in its Thread's threadEpochs list
    EpochList::iterator myThreadEpochsPos;
    
    // Counts clock-related anomalies for this epoch
    size_t numSrcAnomalies;
    size_t numDstAnomalies;

    void srcAnomalyFound(void){
      if(myState>=State::FullReplay){
	I(numSrcAnomalies);
      }else{
	numSrcAnomalies++;
      }
    }
    void dstAnomalyFound(void){
      if(myState>=State::FullReplay){
	I(numDstAnomalies);
      }else{
	numDstAnomalies++;
      }
    }

    // Counts data races for this epoch
    size_t numSrcRaces;
    size_t numDstRaces;

    void srcRaceFound(void){
      if(myState>=State::FullReplay){
	I(numSrcRaces);
      }else{
	numSrcRaces++;
      }
    }
    void dstRaceFound(void){
      if(myState>=State::FullReplay){
	I(numDstRaces);
      }else{
	numDstRaces++;
      }
    }

    struct State{
      enum AtomEnum {NotAtomic, Atomic}                        atom : 1;
      enum SuccEnum {NotSucceeded, WasSucceeded}               succ : 1;
      enum SyncEnum
	{PreAccess, Acquire, Access, Release, PostAccess}      sync : 3;
      enum SpecEnum {Spec, ThreadSafe, GlobalSafe, Committed}  spec : 2;
      enum ExecEnum {Spawning, Running, Waiting, Completed}    exec : 2;
      enum IterEnum {Initial, PartReplay, FullReplay, Merging} iter : 2;
      enum MergEnum {PassiveMerge, ActiveMerge}                merg : 1;
      enum WaitEnum {NoWait, WaitThreadSafe,
		     WaitGlobalSafe, WaitFullyMerged}          wait : 2;
      State(void)
	: atom(NotAtomic), succ(NotSucceeded), sync(PreAccess), spec(Spec),
	   exec(Spawning), iter(Initial), merg(PassiveMerge), wait(NoWait){
      }
      State &operator=(AtomEnum newAtom){ atom=newAtom; return *this; }
      State &operator=(SuccEnum newSucc){ succ=newSucc; return *this; }
      State &operator=(SyncEnum newSync){ sync=newSync; return *this; }
      State &operator=(SpecEnum newSpec){ spec=newSpec; return *this; }
      State &operator=(ExecEnum newExec){ exec=newExec; return *this; }
      State &operator=(IterEnum newIter){ iter=newIter; return *this; }
      State &operator=(MergEnum newMerg){ merg=newMerg; return *this; }
      State &operator=(WaitEnum newWait){ wait=newWait; return *this; }
      bool operator==(AtomEnum cmpAtom) const{ return atom==cmpAtom; }
      bool operator!=(AtomEnum cmpAtom) const{ return atom!=cmpAtom; }
      bool operator==(SuccEnum cmpSucc) const{ return succ==cmpSucc; }
      bool operator!=(SuccEnum cmpSucc) const{ return succ!=cmpSucc; }
      bool operator==(SyncEnum cmpSync) const{ return sync==cmpSync; }
      bool operator!=(SyncEnum cmpSync) const{ return sync!=cmpSync; }
      bool operator<=(SyncEnum cmpSync) const{ return sync<=cmpSync; }
      bool operator> (SyncEnum cmpSync) const{ return sync> cmpSync; }
      bool operator==(SpecEnum cmpSpec) const{ return spec==cmpSpec; }
      bool operator!=(SpecEnum cmpSpec) const{ return spec!=cmpSpec; }
      bool operator>=(SpecEnum cmpSpec) const{ return spec>=cmpSpec; }
      bool operator==(ExecEnum cmpExec) const{ return exec==cmpExec; }
      bool operator!=(ExecEnum cmpExec) const{ return exec!=cmpExec; }
      bool operator==(IterEnum cmpIter) const{ return iter==cmpIter; }
      bool operator!=(IterEnum cmpIter) const{ return iter!=cmpIter; }
      bool operator>=(IterEnum cmpIter) const{ return iter>=cmpIter; }
      bool operator==(MergEnum cmpMerg) const{ return merg==cmpMerg; }
      bool operator!=(MergEnum cmpMerg) const{ return merg!=cmpMerg; }
      bool operator==(WaitEnum cmpWait) const{ return wait==cmpWait; }
      bool operator!=(WaitEnum cmpWait) const{ return wait!=cmpWait; }
      bool operator>=(WaitEnum cmpWait) const{ return wait>=cmpWait; }
    };
    
    // State of this epoch
    State myState;
    
    static void printEpochs(void){
      std::cout << "Epochs: ";
      for(EpochList::iterator it=allEpochs.begin();it!=allEpochs.end();it++){
	Epoch *ep=*it;
	std::cout << " " << ep->mySClock << ":" << ep->getTid() << ":";
	if(ep->myState==State::Spec){
	  std::cout << "S";
	}else if(ep->myState==State::ThreadSafe){
	  std::cout << "T";
	}else if(ep->myState==State::GlobalSafe){
	  std::cout << "G";
	}else if(ep->myState==State::Committed){
	  std::cout << "C";
	}
      }
      std::cout << std::endl;
    }
    
    // Creates the initial epoch in a thread
    Epoch(ThreadID tid);
    // Creates a same-thread successor of an epoch
    Epoch(Epoch *parentEpoch);
    // Destroys this epoch
    ~Epoch(void);
   
//     // Position in allEpochs of the most recently Analyzing epoch
//     static EpochList::iterator lastAnalyzing;
//     // Position in allEpochs of the most recently Merging epoch
//     static EpochList::iterator lastMerging;

    // True iff no epoch is currently in the GlobalSafe state
    static bool globalSafeAvailable;
    // Position in allEpochs of the most recent GlobalSafe epoch
    static EpochList::iterator globalSafe;

    // State that helps us gather statistics

    // Number of epochs that are created but not safe
    static size_t createdNotSafeCount;

    // Number of epochs that are Safe but not Analyzing,
    static size_t safeNotAnalyzingCount;
    // Number of instructions executed in those epochs
    static InstrCount safeNotAnalyzingInstr;
    // Buffer blocks buffered by those epochs
    static size_t safeNotAnalyzingBufferBlocks;

    // Number of epochs that are Analyzing but not Merging,
    static size_t analyzingNotMergingCount;
    // Number of instructions executed in those epochs
    static InstrCount analyzingNotMergingInstr;
    // Buffer blocks buffered by those epochs
    static size_t analyzingNotMergingBufferBlocks;

    // Number of epochs that are Merging but not yet destroyed
    static size_t mergingNotDestroyedCount;

    // Epochs that were stopped for analysis of others
    static EpochList stoppedForAnalysis;
    
    // Epochs in that require analysis 
    static EpochSet analysisNeeded;
    // Epochs that require analysis and
    // are Analyzing but not Merging
    static EpochSet analysisReady;
    // Epochs for which the analysis is complete
    // and are Analyzing but not Merging
    static EpochSet analysisDone;
    
    // True iff we are re-executing epoch for analysis
    static bool doingAnalysis;
    static void beginAnalysis(void);
    static void completeAnalysis(void);
    
    // The (at most one) epoch that is Safe and Merging while active
    // If no such epoch, activeMerging is 0
    static Epoch *activeMerging;
    // Epochs that are active and can not proceed until Safe and Merging
    static EpochSet activeWaitMerge;
    // Ended epochs that would become Merging but are waiting 
    // because there is an activeMerging epoch 
    static EpochSet endedWaitSafe;
    // Ended epochs that would become Merging but are waiting 
    // because there is an activeMerging epoch 
    static EpochSet endedWaitMerge;
    
    enum CmpResult{
      StrongBefore,
      WeakBefore,
      Unordered,
      WeakAfter,
      StrongAfter
    };

    CmpResult compareSClock(const Epoch *otherEpoch) const;
    void advanceSClock(const Epoch *predEpoch,bool sync);
  public:
    const VClock &getVClock(void) const{
      return myVClock;
    }
    
    static Epoch *initialEpoch(ThreadID tid);
    Epoch *spawnEpoch(void);
    Epoch *changeEpoch(){
      Epoch *newEpoch=spawnEpoch();
      complete();
      newEpoch->run();
      // printEpochs();
      return newEpoch;
    }
    void run(void){
      I(myState==State::Spawning);
      // Current execution begins now
      beginCurrentExe=globalClock;
      // Enable SESC execution of this epoch
      osSim->unstop(myPid);
      I(sysCallLogPos==sysCallLog.begin());
      // Change the epoch's execution state
      myState=State::Running;
      // ID(printf("Executing %ld:%d",mySClock,getTid()));
      // ID(printf((myState==State::FullReplay)?" in replay\n":"\n"));
    }
    void beginAcquire(void);
    void retryAcquire(void);
    void endAcquire(void);
    void skipAcquire(void);
    void beginRelease(void);
    void endRelease(void);
    void skipRelease(void);
    void succeeded(Epoch *succEpoch);
    void threadSave(void);
    static void advanceGlobalSafe(void);
    void tryGlobalSave(void);
    void globalSave(void);
    void complete(void);
    void tryCommit(void);
    void commit(void);

    bool waitThreadSafe(void){
      if(myState>=State::ThreadSafe)
	return true;
      if(myState>=State::WaitThreadSafe)
	return false;
      I(myState==State::Running);
      I(myState!=State::Acquire);
      // Make the epoch wait until it becomes ThreadSafe
      myState=State::Waiting;
      myState=State::WaitThreadSafe;
      osSim->stop(myPid);
      return false;
    }
    bool waitGlobalSafe(void){
      if(myState>=State::GlobalSafe)
	return true;
      if(myState>=State::WaitGlobalSafe)
	return false;
      I(myState==State::Running);
      I(myState!=State::Acquire);
      // Make the epoch wait until it becomes GlobalSafe
      myState=State::Waiting;
      myState=State::WaitGlobalSafe;
      osSim->stop(myPid);
      return false;
    }
    bool waitFullyMerged(void){
      requestActiveMerge();
      if((myState>=State::Merging)&&bufferBlocks.empty())
	return true;
      if(myState>=State::WaitFullyMerged)
	return false;
      I(myState==State::Running);
      I(myState!=State::Acquire);
      // Make the epoch wait until it becomes GlobalSafe
      myState=State::Waiting;
      myState=State::WaitFullyMerged;
      osSim->stop(myPid);
      return false;
    }
    void becomeFullyMerged(void){
      I(myState==State::WaitFullyMerged);
      I((myState>=State::Merging)&&bufferBlocks.empty());
      myState=State::Running;
      myState=State::NoWait;
      osSim->unstop(myPid);
    }
    void endThread(int exitCode){
      I(this==myThread->threadEpochs.front());
      myThread->exitCalled(exitCode);
      complete();
    }
    void squash(void);
    template<bool parentSquashed>
    void squashLocal(void);
    void unspawn(void);

    void requestActiveMerge(void);
    void beginMerge(void);
    void beginActiveMerge(void);

    static Epoch *getEpoch(Pid_t pid){
      I(pid>=0);
      ThreadContext *context=osSim->getContext(pid);
      PidToEpoch::size_type index=static_cast<PidToEpoch::size_type>(pid);
      if(pidToEpoch.size()<=index){
        I(context->getEpoch()==0);
	return 0;
      }
      I(context->getEpoch()==pidToEpoch[pid]);
      return pidToEpoch[pid];
    }

    SysCallLog sysCallLog;
    SysCallLog::iterator sysCallLogPos;
    
    void makeSysCall(SysCall *sysCall){
      I(sysCallLogPos==sysCallLog.end());
      sysCallLog.push_back(sysCall);
      I(sysCallLogPos==sysCallLog.end());
    }
    SysCall *prevSysCall(void){
      if(sysCallLogPos==sysCallLog.begin())
	return 0;
      sysCallLogPos--;
      return *sysCallLogPos;
    }
    SysCall *nextSysCall(void){
      if(sysCallLogPos==sysCallLog.end())
	return 0;
      SysCall *sysCall=*sysCallLogPos;
      sysCallLogPos++;
      return sysCall;
    }

    Pid_t getPid(void) const{
      return myPid;
    }
    ThreadID getTid(void) const{
      return myThread->getID();
    }
  private:
    // Code related to buffering memory state of an epoch begins here
    
    // Memory state buffer of an epoch consists of blocks
    // Each block buffers blockSize consecutive bytes of data and
    // their access bits. Each kind of access bits for a block is
    // stored in a sequence of entries whose type is ChunkBitMask,
    // each containing chunkSize access bits.
    typedef unsigned char ChunkBitMask;
    enum BufferBlockConstantsEnum{
      logChunkSize=3,
      logBlockSize=5,
      chunkSize=(1<<logChunkSize),
      blockSize=(1<<logBlockSize),
      chunkAddrMask=(chunkSize-1),
      blockAddrMask=(blockSize-1)
    };
    class BufferBlock;
    class BlockVersions{
      friend class BufferBlock;
      friend class Epoch;
      // Maps each block base address to its BlockVersions entry 
      typedef HASH_MAP<Address,BlockVersions *>  BlockAddrToVersions;
      static BlockAddrToVersions blockAddrToVersions;
      // List of buffer blocks
      typedef std::list<BufferBlock *> BufferBlockList;
      // Lists of all accessors and of writers of this block
      // Both lists are sorted in order of recency (most recent first)
      ID(public:)
      // List of all accessors of the block
      BufferBlockList accessors;
      // List of writers of the block
      BufferBlockList writers;
      ID(private:)
      // True iff there was never any writer
      bool rdOnly;
      // Current total number of accessors
      size_t accessorsCount;
      BlockVersions(void){
	accessorsCount=0;
	rdOnly=true;
      }
      ~BlockVersions(void){
	I(accessorsCount==0);
	I(accessors.empty());
	I(writers.empty());
      }
    public:
      static void staticDestructor(void){
	// Destroy all BlockVersions
	for(BlockAddrToVersions::iterator versIt=blockAddrToVersions.begin();
	    versIt!=blockAddrToVersions.end();versIt++){
	  BlockVersions *blockVersions=versIt->second;
	  delete blockVersions;
	}
	blockAddrToVersions.clear();
      }
      static BlockVersions *getVersions(Address blockBase){
	BlockAddrToVersions::iterator versionsIt=
	  blockAddrToVersions.find(blockBase);
	if(versionsIt!=blockAddrToVersions.end())
	  return versionsIt->second;
	BlockVersions *blockVersions=new BlockVersions();
	blockAddrToVersions.insert(BlockAddrToVersions::value_type(blockBase,blockVersions));
	return blockVersions;
      }
      bool isRdOnly(void) const{
	return rdOnly;
      }
      struct ConflictInfo{
	BufferBlock *block;
	ChunkBitMask mask;
	ConflictInfo(BufferBlock *block, ChunkBitMask mask)
	  : block(block), mask(mask){
	}
	ConflictInfo(const ConflictInfo &other)
	  : block(other.block), mask(other.mask){
	}
      };
      typedef BufferBlockList::iterator BlockPosition;
      BlockPosition findBlockPosition(const BufferBlock *block);
      void advance(BufferBlock *block);
      void access(bool isWrite, BufferBlock *currBlock, BlockPosition &blockPos);

      typedef slist<ConflictInfo> ConflictList;

      ChunkBitMask findReadConflicts(const BufferBlock *currBlock, size_t chunkIndx,
				     BlockPosition blockPos,
				     ChunkBitMask beforeMask, ChunkBitMask afterMask,
				     ConflictList &writesBefore, ConflictList &writesAfter);

      void findWriteConflicts(const BufferBlock *currBlock, size_t chunkIndx,
			      BlockPosition blockPos,
			      ChunkBitMask beforeMask, ChunkBitMask afterMask,
			      ConflictList &readsBefore, ConflictList &writesBefore,
			      ConflictList &writesAfter, ConflictList &readsAfter);

      void remove(BufferBlock *block);
      void check(Address baseAddr);
      static void checkAll(void);
    };
    // One block of buffer data and its access bits
    class BufferBlock{
      // Total number of buffered blocks in the system
      static size_t blockCount;
      // True iff the block has any exposed reads
      bool isConsumer;
      // True iff the block has any writes
      bool isProducer;
      // True iff the block has been a source for an exposed read
      bool myIsFlowSource;
      // True iff any part of the block has been overwriten by a successor
      bool myIsStale;
      // Info on other versions of this block
      friend class BlockVersions;
      friend class Epoch;
      ID(public:)
      BlockVersions *myVersions;
      BlockVersions::BufferBlockList::iterator accessorsPos;
      BlockVersions::BufferBlockList::iterator writersPos;
    public:
      BlockVersions *getVersions(void) const{
	return myVersions;
      }
      BufferBlock *getPrevVersion(bool getEquals) const{
	BlockVersions::BufferBlockList::iterator prevAccessorPos=accessorsPos;
	do{
	  prevAccessorPos++;
	  if(prevAccessorPos==myVersions->accessors.end())
	    return 0;
	}while((!getEquals)&&((*prevAccessorPos)->epoch->mySClock==epoch->mySClock));
	return *prevAccessorPos;
      }
      static size_t getBlockCount(void){
	return blockCount;
      }
      bool isAccessed(void){
	return isProducer||isConsumer;
      }
      bool isWritten(void){
	return isProducer;
      }
      bool becomeConsumer(void){
	if(isConsumer)
	  return false;
	isConsumer=true;
	return true;
      }
      bool becomeProducer(void){
	if(isProducer)
	  return false;
	isProducer=true;
	return true;
      }
      bool isFlowSource(void) const{
	return myIsFlowSource;
      }
      void becomeFlowSource(void){
	if(!myIsFlowSource){
	  myIsFlowSource=true;
	}
      }
      bool isStale(void) const{
	return myIsStale;
      }
      void becomeStale(void){
	if(!myIsStale){
	  myIsStale=true;
	}
      }
      // The epoch the block belongs to
      Epoch *epoch;
      // The base address of the block
      Address baseAddr;
      // A byte in wkData is valid if either its xpMask or
      // its wrMask bit is set.
      // Exposed mask. A set bit indicates that this version
      // did a copy-in of that byte from a previous version
      ChunkBitMask xpMask[blockSize/chunkSize];
      // Write mask. A set bit indicates that the version wrote
      // the corresponding byte
      ChunkBitMask wrMask[blockSize/chunkSize];
      // Work data. Current values of this version's data
      unsigned long long wkData[blockSize/sizeof(unsigned long long)];
      // Default constructor. Creates a new BufferBlock with all masks empty 
      BufferBlock(Epoch *epoch, Address baseAddr)
	: isConsumer(false), isProducer(false),
	  myIsFlowSource(false), myIsStale(false),
	  myVersions(BlockVersions::getVersions(baseAddr)),
	  accessorsPos(myVersions->accessors.end()),
          writersPos(myVersions->writers.end()),
	  epoch(epoch), baseAddr(baseAddr){
	blockCount++;
	I(chunkSize==sizeof(ChunkBitMask)*8);
	for(int i=0;i<blockSize/chunkSize;i++){
	  xpMask[i]=(ChunkBitMask)0;
	  wrMask[i]=(ChunkBitMask)0;
	}
      }
      ~BufferBlock(void){
	I(blockCount>0);
	blockCount--;
	myVersions->remove(this);
      }
      static void maskedChunkCopy(unsigned char *srcChunk,
				  unsigned char *dstChunk,
				  ChunkBitMask cpMask){
	I(cpMask);
	unsigned char *srcPtr=srcChunk;
	unsigned char *dstPtr=dstChunk;
	unsigned char *srcChunkEnd=srcChunk+chunkSize;
	while(srcPtr!=srcChunkEnd){
	  if(cpMask&(1<<(chunkSize-1)))
	    (*dstPtr)=(*srcPtr);
	  cpMask<<=1;
	  srcPtr++;
	  dstPtr++;
	}
      }
      // Combines the sucessor block into this block block
      void append(const BufferBlock *succBlock){
	I(baseAddr==succBlock->baseAddr);
	ID(EpochList::reverse_iterator succEpochPos(epoch->myAllEpochsPos));
	I(succBlock->epoch==*succEpochPos);
	unsigned char *srcDataPtr=(unsigned char *)succBlock->wkData;
	unsigned char *dstDataPtr=(unsigned char *)wkData;
	for(int i=0;i<blockSize/chunkSize;i++){
	  ChunkBitMask addXpMask=succBlock->xpMask[i];
	  addXpMask&=(~wrMask[i]);
	  addXpMask&=(~xpMask[i]);
	  ChunkBitMask copyMask=addXpMask;
	  if(addXpMask){
	    xpMask[i]|=addXpMask;
	    if(!isConsumer)
	      becomeConsumer();
	  }
	  ChunkBitMask succWrMask=succBlock->wrMask[i];
	  if(succWrMask){
	    copyMask|=succWrMask;
	    wrMask[i]|=succWrMask;
	    if(!isProducer)
	      becomeProducer();
	  }
	  if(copyMask){
	    maskedChunkCopy(srcDataPtr,dstDataPtr,copyMask);
	  }
	  srcDataPtr+=chunkSize;
	  dstDataPtr+=chunkSize;
	}
      }
      // Cleans the state of the block (invalidate dirty bytes)
      void clean(void){
	for(int i=0;i<blockSize/chunkSize;i++){
	  xpMask[i]&=~wrMask[i];
	  wrMask[i]=(ChunkBitMask)0;
	}
      }
      // Merges the written data of this block into main memory
      void merge(void) const{
	unsigned char *memDataPtr=(unsigned char *)baseAddr;
	unsigned char *bufDataPtr=(unsigned char *)wkData;
	const unsigned char *memBlockEnd=memDataPtr+blockSize;
	const ChunkBitMask  *wrMaskPtr=wrMask;
	while(memDataPtr<memBlockEnd){
	  ChunkBitMask wrMask=*wrMaskPtr;
	  if(wrMask)
	    maskedChunkCopy(bufDataPtr,memDataPtr,wrMask);
	  memDataPtr+=chunkSize;
	  bufDataPtr+=chunkSize;
	  wrMaskPtr++;
	}
      }
    };
    // All buffered blocks of this epoch
    typedef HASH_MAP<Address,BufferBlock *> BufferBlocks;
    BufferBlocks bufferBlocks;

    BufferBlock *getBufferBlock(Address blockBase){
      BufferBlocks::iterator blockIt=bufferBlocks.find(blockBase);
      // If block exists, return it
      if(blockIt!=bufferBlocks.end())
	return blockIt->second;
      // No block exists, can one be created?
      if(maxBlocksPerEpoch&&(bufferBlocks.size()==maxBlocksPerEpoch)){
	changeEpoch();
	return 0;
      }
      // Create new block
      BufferBlock *retVal=new BufferBlock(this,blockBase);
      bufferBlocks.insert(BufferBlocks::value_type(blockBase,retVal));
#if (defined CollectVersionCountStats)
      detWinVersCounts.add(retVal);
#endif // CollectVersionCountStats
      return retVal;
    }

#if (defined CollectVersionCountStats)
    struct VersionCount{
      size_t count;
      VersionCount(void)
	: count(0){
      }
      void inc(void){
	count++;
      }
      void dec(void){
	count--;
      }
    };
    struct VersionCounts{
      typedef std::map<Address,VersionCount> CountMap;
      CountMap countMap;
      void add(const BufferBlock *bufferBlock){
	VersionCount &myCount=countMap[bufferBlock->baseAddr];
	myCount.inc();
      }
      void remove(const BufferBlock *bufferBlock){
	CountMap::iterator countIt=countMap.find(bufferBlock->baseAddr);
	I(countIt!=countMap.end());
	countIt->second.dec();
	if(!countIt->second.count)
	  countMap.erase(countIt);
      }
      void  add(const Epoch *epoch){
	for(BufferBlocks::const_iterator blockIt=epoch->bufferBlocks.begin();
	    blockIt!=epoch->bufferBlocks.end();blockIt++){
	  const BufferBlock *bufferBlock=blockIt->second;
	  add(bufferBlock);
	}
      }
      void remove(const Epoch *epoch){
	for(BufferBlocks::const_iterator blockIt=epoch->bufferBlocks.begin();
	    blockIt!=epoch->bufferBlocks.end();blockIt++){
	  const BufferBlock *bufferBlock=blockIt->second;
	  remove(bufferBlock);
	}
      }
      size_t getMax(void) const{
	size_t retVal=1;
	for(CountMap::const_iterator countIt=countMap.begin();
	    countIt!=countMap.end();countIt++){
	  const VersionCount &myCount=countIt->second;
	  retVal=std::max(retVal,myCount.count);
	}
	return retVal;
      }
      size_t getMaxNotRdOnly(void) const{
	size_t retVal=1;
	for(CountMap::const_iterator countIt=countMap.begin();
	    countIt!=countMap.end();countIt++){
	  // Skip if no producers
	  I(BlockVersions::getVersions(countIt->first));
	  if(BlockVersions::getVersions(countIt->first)->isRdOnly())
	    continue;
	  const VersionCount &myCount=countIt->second;
	  retVal=std::max(retVal,myCount.count);
	}
	return retVal;
      }
    };
    static VersionCounts detWinVersCounts;
#endif // CollectVersionCountStats

    static const ChunkBitMask accessBitMask[16][chunkSize];


    // Internal function.
    // Combines the buffer of a successor epoch into the buffer of this epoch
    void appendBuffer(const Epoch *succEpoch);
    // Internal function.
    // Cleans all buffered blocks by invalidating their dirty data.
    void cleanBuffer(void);
    // Internal funstion.
    // Input:  The base address of the block in blockAddr
    // Effect: Block is erased
    void eraseBlock(Address baseAddr);
    // Internal function.
    // Input:  The address of the block in baseAddr
    //         The initialMerge parameter is true iff 
    //         this merge is not triggered by a successor block merge
    // Effect: Block and all its predecessors are merged into memory
    //         then erased. If the last block in this epoch is merged,
    //         the epoch itself is destroyed.
    // Return: True iff this epoch was destroyed
    bool mergeBlock(Address baseAddr, bool initialMerge);
    // Internal function.
    // does eraseBlock on all blocks of this epoch.
    void eraseBuffer(void);
    // Internal function.
    // Does mergeBlock on all blocks of the epoch.
    // Note that as a result it always ends up destroying the epoch
    void mergeBuffer(void);

    typedef std::set<Address> AddressSet;
    // Data addresses that this epoch has races on
    AddressSet traceDataAddresses;
    // Code addresses that cause data races in this epoch
    AddressSet traceCodeAddresses;

  public:
    class TraceEvent{
    protected:
      // ID of the thread performing the event
      ThreadID tid;
      // Address of the instruction performing the event
      Address  iAddrV;
      // Number of instructions since the beginning of this epoch
      size_t   instrOffset;
      // Time of the occurence of the event
      Time_t myTime;
    public:
      TraceEvent(Epoch *epoch){
	tid=epoch->getTid();
	iAddrV=osSim->eventGetInstructionPointer(epoch->myPid)->addr;
	instrOffset=epoch->pendInstrCount;
	// Initially store the time of the event in the reenactment
	myTime=globalClock;
      }
      void adjustTime(const Epoch *epoch){
	// Event time from the beginning of the reenactment execution
	myTime-=epoch->beginCurrentExe;
	// Duration of the reeanctment excution
	double reenactTime=epoch->endCurrentExe-epoch->beginCurrentExe;
	// Duration of the original execution
	double originalTime=epoch->endDecisionExe-epoch->beginDecisionExe;
	// Ratio of execution speed in reenactment and original execution
	double speedRatio=originalTime/reenactTime;
	// Event time from the beginning of the original execution
	myTime=(Time_t)(myTime*speedRatio);
	// Event time in the original execution
	myTime+=epoch->beginDecisionExe;
      }
      virtual ~TraceEvent(void){}
      virtual void print(void){
	printf("%12lld %3d %8lx",myTime,tid,iAddrV);
      }
      Time_t getTime(void) const{
	return myTime;
      }
      size_t getInstrOffset(void) const{
	return instrOffset;
      }
    };
  private:
    class TraceAccessEvent : public TraceEvent{
    public:
      typedef enum AccessTypeEnum {Read, Write} AccessType;
    protected:
      Address dAddrV;
      AccessType accessType;
      size_t accessCount;
      size_t lastInstrOffset;
    public:
      TraceAccessEvent(Epoch *epoch, Address dAddrV, AccessType accessType)
	: TraceEvent(epoch), dAddrV(dAddrV), accessType(accessType){
	accessCount=1;
	lastInstrOffset=instrOffset;
      }
      virtual void print(void){
	TraceEvent::print();
	printf(": %8lx ",dAddrV);
	switch(accessType){
	case Read: printf("Rd"); break;
	case Write: printf("Wr"); break;
	}
	if(accessCount>1){
	  printf(" %d times in %d instructions",
		 accessCount,lastInstrOffset-instrOffset);
	}
      }
      TraceAccessEvent *newAccess(Epoch *epoch, Address dAddrV,
				  AccessType accessType){
// 	if((iAddrV==(Address)(osSim->eventGetInstructionPointer(epoch->myEid)->addr))&&
// 	   (TraceAccessEvent::dAddrV==dAddrV)&&
// 	   (TraceAccessEvent::accessType==accessType)){
// 	  accessCount++;
// 	  lastInstrOffset=epoch->pendInstrCount;
// 	  return 0;
// 	}else{
	  return new TraceAccessEvent(epoch,dAddrV,accessType);
// 	}
      }
      Address getDataAddr(void) const{
	return dAddrV;
      }
      AccessType getAccessType(void) const{
	return accessType;
      }
    };
    typedef std::list<TraceEvent *> TraceList;
    TraceList myTrace;
    
    struct RaceInfo{
      enum RaceType {
	None=0,
	Anti=1,
	Output=2,
	Flow=4,
	All=Anti|Output|Flow
      } raceType;
      Address   dAddrV;
      Epoch     *epoch1;      
      Epoch     *epoch2;
      RaceInfo(Address  addr, Epoch *ep1, Epoch *ep2, RaceType raceType)
	: raceType(raceType), dAddrV(addr), epoch1(ep1), epoch2(ep2){
      }
      void addType(RaceType newType){
	raceType=static_cast<RaceType>(raceType|newType);
      }
    };
    
    typedef std::map<Address,RaceInfo *> RaceByAddr;
    typedef std::map<Epoch *,RaceByAddr> RaceByEpAddr;
    typedef std::map<Epoch *,RaceInfo *> RaceByEp;
    struct RaceAddrInfo{
      RaceByEp raceByEp;
      Address   dAddrV;
      enum Position { First, Second} myPosition;
      RaceInfo::RaceType raceTypes;
      TraceAccessEvent *readAccess;
      TraceAccessEvent *writeAccess;
      RaceAddrInfo(Address dAddrV, Position myPosition)
	: dAddrV(dAddrV), myPosition(myPosition), raceTypes(RaceInfo::None),
	  readAccess(0), writeAccess(0){
      }
      ~RaceAddrInfo(void){
	if(readAccess)
	  delete readAccess;
	if(writeAccess)
	  delete writeAccess;
      }
      void addType(RaceInfo::RaceType newType){
	raceTypes=static_cast<RaceInfo::RaceType>(raceTypes|newType);
      }
      void addReadAccess(Epoch *epoch, Address dAddrV){
	if(myPosition==Second){
	  // A read can be the second access only in a flow-type race
	  if(raceTypes&RaceInfo::Flow){
	    // This is the second access of the race,
	    // so we want it as early as possible 
	    // Thus, take the first eligible access in this epoch 
	    if(!readAccess){
	      readAccess=
		new TraceAccessEvent(epoch,dAddrV,TraceAccessEvent::Read);
	      epoch->addAccessEvent(readAccess);
	    }
	  }
	}else{
	  I(myPosition==First);
	  // If no anti-type race, 
	  // A read can be the first access only in a anti-type race
	  if(raceTypes&RaceInfo::Anti){
	    // This is the first access of the race,
	    // so we want it as late as possible
	    // Thus, take the last eligible access in this epoch 
	    epoch->removeAccessEvent(readAccess);
	    delete readAccess;
	    readAccess=
	      new TraceAccessEvent(epoch,dAddrV,TraceAccessEvent::Read);
	    epoch->addAccessEvent(readAccess);
	  }
	}
      }
      void addWriteAccess(Epoch *epoch, Address dAddrV){
	if(myPosition==Second){
	  // A write can be the second access only in
	  // output- and anti-type races
	  if(raceTypes&(RaceInfo::Output|RaceInfo::Anti)){
	    // This is the second access of the race
	    // and we want it as early as possible
	    // Thus, take the first eligible access in this epoch 
	    if(!writeAccess){
	      writeAccess=
		new TraceAccessEvent(epoch,dAddrV,TraceAccessEvent::Write);
	      epoch->addAccessEvent(writeAccess);
	    }
	  }
	}else{
	  I(myPosition==First);
	  // A write can be the first access only in
	  // output- and flow-type races
	  if(raceTypes&(RaceInfo::Output|RaceInfo::Flow)){
	    // This is the first access of the race
	    // and we want it as late as possible
	    // Thus, take the last eligible access in this epoch 
	    epoch->removeAccessEvent(writeAccess);
	    delete writeAccess;
	    writeAccess=
	      new TraceAccessEvent(epoch,dAddrV,TraceAccessEvent::Write);
	    epoch->addAccessEvent(writeAccess);
	  }
	}
      }
    };
    typedef std::map<Address,RaceAddrInfo *> RaceByAddrEp;
    
    RaceByEpAddr myForwRacesByEpAddr;
    RaceByAddrEp myForwRacesByAddrEp;
    RaceByEpAddr myBackRacesByEpAddr;
    RaceByAddrEp myBackRacesByAddrEp;
    
    void addRace(Epoch *epoch1, Epoch *epoch2,
		 Address dAddrV, RaceInfo::RaceType raceType){
      RaceByAddr &forwRaceByAddr=epoch1->myForwRacesByEpAddr[epoch2];
      std::pair<RaceByAddr::iterator,bool> insOutcome=
	forwRaceByAddr.insert(RaceByAddr::value_type(dAddrV,(RaceInfo *)0));
      if(insOutcome.second){
	// If the element was actually inserted, create a new ReaceInfo
	RaceInfo *raceInfo=new RaceInfo(dAddrV,epoch1,epoch2,raceType);
	// Insert it into all the structures where it needs to be
	insOutcome.first->second=raceInfo;
	epoch2->myBackRacesByEpAddr[epoch1][dAddrV]=raceInfo;
	std::pair<RaceByAddrEp::iterator,bool> forwAddrEpInsOutcome=
	  epoch1->myForwRacesByAddrEp.insert(RaceByAddrEp::value_type(dAddrV,(RaceAddrInfo *)0));
	if(forwAddrEpInsOutcome.second){
	  RaceAddrInfo *forwAddrInfo=
	    new RaceAddrInfo(dAddrV,RaceAddrInfo::First);
	  forwAddrEpInsOutcome.first->second=forwAddrInfo;
	}
	forwAddrEpInsOutcome.first->second->raceByEp[epoch2]=raceInfo;
	forwAddrEpInsOutcome.first->second->addType(raceType);
	std::pair<RaceByAddrEp::iterator,bool> backAddrEpInsOutcome=
	  epoch2->myBackRacesByAddrEp.insert(RaceByAddrEp::value_type(dAddrV,(RaceAddrInfo *)0));
	if(backAddrEpInsOutcome.second){
	  RaceAddrInfo *backAddrInfo=
	    new RaceAddrInfo(dAddrV,RaceAddrInfo::Second);
	  backAddrEpInsOutcome.first->second=backAddrInfo;
	}
	backAddrEpInsOutcome.first->second->raceByEp[epoch1]=raceInfo;
	backAddrEpInsOutcome.first->second->addType(raceType);
      }else{
	// Race info already exists, just update the type
	insOutcome.first->second->addType(raceType);
	I(epoch1->myForwRacesByAddrEp.count(dAddrV));
	I(epoch2->myBackRacesByAddrEp.count(dAddrV));
	epoch1->myForwRacesByAddrEp[dAddrV]->addType(raceType);
	epoch2->myBackRacesByAddrEp[dAddrV]->addType(raceType);
	I(epoch2->myBackRacesByEpAddr[epoch1][dAddrV]==
	  insOutcome.first->second);
	I(epoch1->myForwRacesByAddrEp[dAddrV]->raceByEp[epoch2]==
	  insOutcome.first->second);
	I(epoch2->myBackRacesByAddrEp[dAddrV]->raceByEp[epoch1]==
	  insOutcome.first->second);
      }
    }
    bool hasNewRaces(void){
      // If no races at all, return false
      if(myForwRacesByAddrEp.empty()&&myBackRacesByAddrEp.empty())
	return false;
      // If any forward race address has not been traced already, return true
      for(RaceByAddrEp::iterator forwIt=myForwRacesByAddrEp.begin();
	  forwIt!=myForwRacesByAddrEp.end();forwIt++){
	Address dAddrV=forwIt->first;
	if(!traceDataAddresses.count(dAddrV))
	  return true;
      }
      // If any forward race address has not been traced already, return true
      for(RaceByAddrEp::iterator backIt=myBackRacesByAddrEp.begin();
	  backIt!=myBackRacesByAddrEp.end();backIt++){
	Address dAddrV=backIt->first;
	if(!traceDataAddresses.count(dAddrV))
	  return true;
      }
      return false;
    }
    void extractRaceAddresses(void){
      for(RaceByAddrEp::iterator forwIt=myForwRacesByAddrEp.begin();
	  forwIt!=myForwRacesByAddrEp.end();forwIt++){
	Address dAddrV=forwIt->first;
	traceDataAddresses.insert(dAddrV);
      }
      for(RaceByAddrEp::iterator backIt=myBackRacesByAddrEp.begin();
	  backIt!=myBackRacesByAddrEp.end();backIt++){
	Address dAddrV=backIt->first;
	traceDataAddresses.insert(dAddrV);
      }
    }
  public:
    // Prepare to read from this version. Returns the address to read from.
    Address read(Address iAddrV, short iFlags,
		 Address dAddrV, Address dAddrR);
    
    // Prepare to write to this version. Returns the address to write to.
    Address write(Address iAddrV, short iFlags,
		  Address dAddrV, Address dAddrR);

    void pendInstr(void){
      I(myState==State::Running);
      I((!maxInstrCount)||(pendInstrCount<maxInstrCount));
      pendInstrCount++;
      I(pendInstrCount>execInstrCount);
    }
    void unPendInstr(void){
      I((myState==State::Running)||(myState==State::Completed));
      pendInstrCount--;
      I(pendInstrCount>=execInstrCount);
      I(pendInstrCount>=doneInstrCount);
      if(myState==State::Completed)
	tryCommit();
    }
    void execInstr(void){
      I((myState==State::Running)||(myState==State::Waiting)||(myState==State::Completed));
      I((!maxInstrCount)||(pendInstrCount<=maxInstrCount));
      I(execInstrCount<pendInstrCount);
      execInstrCount++;
      if((pendInstrCount==maxInstrCount)&&(maxInstrCount!=0)&&
	 (myState!=State::Completed)&&(execInstrCount==pendInstrCount))
        changeEpoch();
    }
    void doneInstr(void){
      I((myState==State::Running)||(myState==State::Waiting)||(myState==State::Completed));
      I(doneInstrCount<=execInstrCount);
      doneInstrCount++;
      if(myState==State::Completed)
	tryCommit();
    }
    void cancelInstr(void){
      I(myState==State::Waiting);
      I((!maxInstrCount)||(pendInstrCount<=maxInstrCount));
      I((!maxInstrCount)||(execInstrCount<maxInstrCount));
      pendInstrCount--;
      execInstrCount--;
      doneInstrCount--;
    }

    // Adds the event to this epoch's list of trace events
    void addTraceEvent(TraceEvent *traceEvent){
      myTrace.push_back(traceEvent);
    }
    void clearTrace(void){
      while(!myTrace.empty()){
	TraceEvent *traceEvent=myTrace.front();
	myTrace.pop_front();
	delete traceEvent;
      }
    }
    typedef std::set<TraceAccessEvent *> EventSet;
    EventSet accessEvents;
    void addAccessEvent(TraceAccessEvent *event){
      accessEvents.insert(event);
    }
    void removeAccessEvent(TraceAccessEvent *event){
      accessEvents.erase(event);
    }
  };

}
#endif
