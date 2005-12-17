#include "Epoch.h"
#include "Checkpoint.h"
#include "SescConf.h"
#include "AdvancedStats.h"

namespace tls{

  Stats::Group AllStats;

  // Statistics for all epochs
  Stats::Distribution NumRunSClockAdj(&AllStats,"number of run-time SClock adjustments");
  Stats::Distribution BufBlocksAdjusted(&AllStats,"number of buffer blocks per SClock adjustment");
  Stats::Distribution EpochInstObserver(&AllStats,"number of instructions per epoch");
  Stats::Distribution EpochBuffObserver(&AllStats,"bytes of buffer space per epoch");

  // Statistics for re-enacted epochs
  Stats::Distribution ReplayRelSpeed(&AllStats,"relative speed of reenactement execution");

  // Statistics for data race detection
  Stats::Distribution SrcAnomaliesPerEpoch(&AllStats,"source anomalies detected per epoch"),
    DstAnomaliesPerEpoch(&AllStats,"destination anomalies detected per epoch"),
    SrcRacesPerEpoch(&AllStats,"source races detected per epoch"),
    DstRacesPerEpoch(&AllStats,"destination races detected per epoch"),
    SrcRacesPerSrcRaceEpoch(&AllStats,"source races detected per source-race epoch"),
    SrcAnomaliesPerSrcRaceEpoch(&AllStats,"source anomalies detected per source-race epoch"),
    AnomaliesPerSrcRaceEpoch(&AllStats,"total anomalies detected per source-race epoch"),
    RacesPerRaceEpoch(&AllStats,"total races detected per race epoch"),
    AnomaliesPerRaceEpoch(&AllStats,"total anomalies detected per race epoch");

  // Statistics for acquire/release
  Stats::Distribution AcquireBlocksNeeded(&AllStats,"number of buffer blocks needed to perform acquire");
  
  // Per-epoch statistics for the detection window  
  Stats::Distribution DetWinEpochObserver(&AllStats,"number of epochs in epoch's detection window");
  Stats::Distribution DetWinInstObserver(&AllStats,"number of instructions in epoch's detection window");
  Stats::Distribution DetWinBuffObserver(&AllStats,"bytes of buffer space in epoch's detection window");
#if (defined CollectVersionCountStats)
  Stats::Distribution DetWinVersObserver(&AllStats,"number of same-block versions in epoch's detection window");
#endif // CollectVersionCountStats

  // Per-instruction statistics for the detection window
  Stats::Distribution InsDetWinEpochObserver(&AllStats,"number of epochs in instruction's detection window");
  Stats::Distribution InsDetWinInstObserver(&AllStats,"number of instructions in instruction's detection window");
  Stats::Distribution InsDetWinBuffObserver(&AllStats,"bytes of buffer space in instruction's detection window");
#if (defined CollectVersionCountStats)
  Stats::Distribution InsDetWinVersObserver(&AllStats,"number of same-block versions in instruction's detection window");
#endif // CollectVersionCountStats

  // Statistics for the race groups
  
  // Window of the race group
  Stats::Distribution RaceGrpWinEpochObserver(&AllStats,"number of epochs in the window of the race group");
  Stats::Distribution RaceGrpWinInstObserver(&AllStats,"number of instructions in the window of the race group");
  Stats::Distribution RaceGrpWinBuffObserver(&AllStats,"bytes of buffer space in the window of the race group");
  Stats::Distribution RaceGrpWinVersObserver(&AllStats,"number of same-block versions in the window of the race group");
  // Initial race in the race group
  Stats::Distribution RaceGrpIniEpochObserver(&AllStats,"number of epochs for the initial race of the group");
  Stats::Distribution RaceGrpIniInstObserver(&AllStats,"number of instructions for the initial race of the group");
  Stats::Distribution RaceGrpIniBuffObserver(&AllStats,"bytes of buffer space for the initial race of the group");
  Stats::Distribution RaceGrpIniVersObserver(&AllStats,"number of same-block versions for the initial race of the group");
  // Shortest race in the race group
  Stats::Distribution RaceGrpShortEpochObserver(&AllStats,"number of epochs for the shortest race of the group");
  Stats::Distribution RaceGrpShortInstObserver(&AllStats,"number of instructions for the shortest race of the group");
  Stats::Distribution RaceGrpShortBuffObserver(&AllStats,"bytes of buffer space for the shortest race of the group");
  Stats::Distribution RaceGrpShortVersObserver(&AllStats,"number of same-block versions for the shortest race of the group");

  Stats::Distribution AccuNumIndepEpochs(&AllStats,"accurate number of independent epochs");
  Stats::Distribution AccuFarIndepEpoch(&AllStats,"distance to farthest independent epoch");
  Stats::Distribution TwoLimNumIndepEpochs(&AllStats,"two-limit number of independet epochs");
  Stats::Distribution TwoLimFarIndepEpoch(&AllStats,"two-limit distance to latest independent epoch");
  Stats::Distribution OneLimNumIndepEpochs(&AllStats,"one-limit number of independent epochs");
  Stats::Distribution OneLimFarIndepEpoch(&AllStats,"one-limit distance to latest independent epoch");

  Stats::Distribution SysCallCount(&AllStats,"recorded system calls per epoch");

  typedef std::set<Address> AddressSet;
  AddressSet raceInstAddr, anomalyInstAddr;

  unsigned int numBegThread=0;
  unsigned int numEndThread=0;

  ClockValue syncClockDelta;

  void Epoch::staticConstructor(void){
    syncClockDelta=SescConf->getInt("TLS","syncClockDelta");
    //    SescConf->isBool("TLS","ignoreRaces");
    //    dataRacesIgnored=SescConf->getBool("TLS","ignoreRaces");
    dataRacesIgnored=false;
    //    SescConf->isInt("TLS","epochBufferSizeLimit");
    //    SescConf->isGT("TLS","epochBufferSizeLimit",-1);
    //    long epochBufferSizeLimit=SescConf->getInt("TLS","epochBufferSizeLimit");
	 int epochBufferSizeLimit=0;
    I(epochBufferSizeLimit>=0);
    maxBlocksPerEpoch=(size_t)(epochBufferSizeLimit/blockSize);
    I(maxBlocksPerEpoch*blockSize==(size_t)epochBufferSizeLimit);
    Checkpoint::staticConstructor();
  }

  // Statistics for synchronization

  // Number of waiting acquirers an action has
  Stats::Distribution WaitingAcquirers(&AllStats,"number of waiting acquirers for a sync action");

  bool Epoch::dataRacesIgnored;
  size_t Epoch::maxBlocksPerEpoch;

  void Epoch::staticDestructor(void){
    AllStats.report();
    if(raceInstAddr.empty()){
      printf("No races found\n");
    }else{
      printf("Races in %u, anomalies in %u (%5.3f) instructions\n",
	     raceInstAddr.size(),anomalyInstAddr.size(),
	     (float)(anomalyInstAddr.size())/((float)raceInstAddr.size()));
      AddressSet::iterator raceAddrIt=raceInstAddr.begin();
      AddressSet::iterator anomalyAddrIt=anomalyInstAddr.begin();
      while((raceAddrIt!=raceInstAddr.end())||(anomalyAddrIt!=anomalyInstAddr.end())){
	Address addr=0;
	char *type="";
	if((raceAddrIt==raceInstAddr.end())||
	   ((anomalyAddrIt!=anomalyInstAddr.end())&&(*raceAddrIt>*anomalyAddrIt))){
	  addr=*anomalyAddrIt;
	  type=" A";
	  anomalyAddrIt++;
	}else if((anomalyAddrIt==anomalyInstAddr.end())||
		 ((raceAddrIt!=raceInstAddr.end())&&(*anomalyAddrIt>*raceAddrIt))){
	  addr=*raceAddrIt;
	  type="R ";
	  raceAddrIt++;	  
	}else{
	  I(anomalyAddrIt!=anomalyInstAddr.end());
	  I(raceAddrIt!=raceInstAddr.end());
	  I(*anomalyAddrIt==*raceAddrIt);
	  addr=*raceAddrIt;
	  type="RA";
	  raceAddrIt++; anomalyAddrIt++;
	}
	printf("Instr %08lx %2s\n",addr,type);
      }
    }
    BlockVersions::staticDestructor();
    printf("Begun %lu, ended %lu threads\n",numBegThread,numEndThread);
    for(EpochList::iterator epochIt=allEpochs.begin();epochIt!=allEpochs.end();epochIt++){
      I(0);
      printf("Epoch %ld:%d still remains\n",(*epochIt)->mySClock,(*epochIt)->getTid());
    }
    Checkpoint::staticDestructor();
  }

  VClock::IndexCounts VClock::indexCounts(0);
  VClock::VClockList VClock::allVClocks;
  VClock VClock::initial;
  VClock VClock::infinite;

  Thread::ThreadVector Thread::threadVector(0);
  Thread::IndexSet  Thread::freeIndices;

  Thread::Thread(ThreadID tid)
    : myID(tid),
      threadEpochs(),
      threadSafe(threadEpochs.end()),
      threadSafeClk(),
      raceFrontier(threadEpochs.end()),
      nextFrontierClk(initialClockValue),
      raceFrontierHolders(0),
      exitAtEnd(false), myExitCode(0),
      threadSafeAvailable(true){
    ID(lastCommitEpoch=0);
    ID(lastFrontierCandidate=0);
    if(freeIndices.empty()){
      myIndex=threadVector.size();
      threadVector.push_back(this);
    }else{
      IndexSet::iterator minIndexIt=freeIndices.begin();
      I(minIndexIt!=freeIndices.end());
      myIndex=*minIndexIt;
      freeIndices.erase(minIndexIt);
      I(myIndex<threadVector.size());
      I(threadVector[myIndex]==0);
      threadVector[myIndex]=this;
    }
  }

  Thread::~Thread(void){
    I(threadVector[myIndex]==this);
    I(!freeIndices.count(myIndex));
    if(myIndex+1==threadVector.size()){
      while(freeIndices.count(myIndex-1))
	myIndex--;
      threadVector.resize(myIndex);
    }else{
      threadVector[myIndex]=0;
      freeIndices.insert(myIndex);
    }
    // If thread has not exited already, resume its execution
    if(!exitAtEnd)
      osSim->unstop(myID);
  }

  EpochList::iterator Thread::addEpoch(Epoch *epoch){
    // If this is the very first epoch in this thread,
    // it can have races with any epoch in each of the
    // other threads.
    if(threadEpochs.empty()){
      I(nextFrontierClk==initialClockValue);
      for(ThreadVector::iterator it=threadVector.begin();
	  it!=threadVector.end();it++){
	Thread *thread=*it;
	// If the other thread exists and has a frontier candidate
	// (which means there are frontier holders), now it has one
	// more holder (this thread)
	if(thread){
	  if(thread->raceFrontierHolders){
	    I(thread->noRacesMissed[myIndex]==initialClockValue);
	    thread->raceFrontierHolders++;
	  }else{
	    threadSafeClk[thread->myIndex]=thread->nextFrontierClk;
	    thread->noRacesMissed[myIndex]=thread->nextFrontierClk;
	  }
	}
      }
    }
    // Find the position of the new epoch in threadEpochs
    EpochList::iterator listPos=threadEpochs.begin();
    while((listPos!=threadEpochs.end())&&(epoch->mySClock<(*listPos)->mySClock))
      listPos++;
    // Except for the very first epoch, there should be older epochs
    // in this thread already (the partent of the new epoch is one such epoch)
    I(threadEpochs.empty()||(listPos!=threadEpochs.end()));
    // We are either at the end, or in front of an older epoch
    I((listPos==threadEpochs.end())||(epoch->mySClock>(*listPos)->mySClock));
    // Insert and return the iterator to the inserted position
    return threadEpochs.insert(listPos,epoch);
  }

  bool Thread::removeEpoch(Epoch *epoch){
    EpochList::iterator listPos=epoch->myThreadEpochsPos;
    I(epoch->myState!=Epoch::State::ThreadSafe);
    if(listPos==threadSafe){
      // A ThreadSafe epoch can not be removed until it also commits
      I(threadSafeAvailable);
      I(epoch->myState>=Epoch::State::Committed);
      threadSafe++;
    }
    if(listPos==raceFrontier)
      raceFrontier++;
    threadEpochs.erase(listPos);
    ID(if(epoch==lastFrontierCandidate) lastFrontierCandidate=0);
    ID(if(epoch==lastCommitEpoch) lastCommitEpoch=0);
    // Is this the last epoch in this thread?
    if(threadEpochs.empty()){
      I(epoch->myState==Epoch::State::Committed);
      I(epoch->myState==Epoch::State::Completed);
      I(epoch->myState==Epoch::State::Merging);
      // If thread not exited already, update its context so it can proceed
      if(!exitAtEnd){
	// Transfer context of the epoch to the thread
	ThreadContext *epochContext=
	  osSim->getContext(epoch->myPid);
	ThreadContext *threadContext=
	  osSim->getContext(myID);
	// Update thread context with stuff from the last epoch's context
	threadContext->copy(epochContext);
      }
      return true;
    }
    return false;
  }  
  
  void Thread::moveThreadSafe(void){
    I(threadSafe!=threadEpochs.begin());
    threadSafe--;
    I((*threadSafe)->myState==Epoch::State::ThreadSafe);
    changeThreadSafeVClock((*threadSafe)->myVClock);
  }

  void Thread::changeThreadSafeVClock(const VClock &newVClock){
    // Update the no-races-missed vector of each thread
    for(VClock::size_type i=0;i<VClock::size();i++){
      ClockValue newValue=newVClock[i];
      ClockValue oldValue=threadSafeClk[i];
      // Only update forward
      if(newValue<=oldValue)
	continue;
      threadSafeClk[i]=newValue;
      Thread *otherThread=threadVector[i];
      if(!otherThread)
        continue;
      // In the other thread, the epoch with a VClock value
      // of newValue has not missed any races with this thread
      I(otherThread->noRacesMissed[myIndex]==oldValue);
      otherThread->noRacesMissed[myIndex]=newValue;
      // If the frontier candidate already could miss no
      // races with this thread, it is still a candidate for
      // some other reason, so we do nothing else now
      if(oldValue>=otherThread->nextFrontierClk)
	continue;
      // If the frontier candidate can still miss races
      // with this thread, we still can not move the frontier
      if(newValue<otherThread->nextFrontierClk)
	continue;
      // This thread is no longer holding the race frontier in
      // the other thread. If nothing else holds it, move it.
      I(otherThread->raceFrontierHolders>0);
      otherThread->raceFrontierHolders--;
      if(!otherThread->raceFrontierHolders)
	otherThread->moveRaceFrontier();
    }
  }

  void Thread::commitEpoch(Epoch *epoch){
    I(epoch!=lastCommitEpoch);
    if(epoch->myThreadEpochsPos==threadSafe){
      // When last thread commits, thread safe clock become infinite
      // because no running epoch in this thread can participate in a race
      I(epoch->myThreadEpochsPos==threadEpochs.begin());
      changeThreadSafeVClock(VClock::getInfinite());
    }
    if(nextFrontierClk>=epoch->myVClock.value())
      return;
    I(epoch!=lastFrontierCandidate);
    ID(lastCommitEpoch=epoch);
    EpochList::reverse_iterator nextFrontier(raceFrontier);
    if(epoch==*nextFrontier){
      I(!raceFrontierHolders);
      newFrontierCandidate();
    }
  }

  void Thread::moveRaceFrontier(void){
    I(!raceFrontierHolders);
    I(raceFrontier!=threadEpochs.begin());
    raceFrontier--;
#if (defined DEBUG)
    I(nextFrontierClk==(*raceFrontier)->myVClock.value());
    for(VClock::size_type i=0;i<VClock::size();i++){
      // If no thread with this index, do not count it
      if(!threadVector[i])
	continue;
      I(nextFrontierClk<=noRacesMissed[i]);
    }    
#endif
    raceFrontierHolders=0;
    EpochList::reverse_iterator nextFrontier(raceFrontier);
    if((nextFrontier!=threadEpochs.rend())&&((*nextFrontier)->myState>=Epoch::State::Committed))
      newFrontierCandidate();
    // Try to merge epochs that are past the race frontier
    EpochList::reverse_iterator mergeItR;
    for(mergeItR=Epoch::allEpochs.rbegin();
	mergeItR!=Epoch::allEpochs.rend();mergeItR++){
      Epoch *mergeEpoch=(*mergeItR);
      Thread *mergeThread=mergeEpoch->myThread;
      EpochList::reverse_iterator nextFrontier(mergeThread->raceFrontier);
      if((nextFrontier!=mergeThread->threadEpochs.rend())&&
	 ((*nextFrontier)==mergeEpoch))
	break;
    }
    if(mergeItR.base()!=Epoch::allEpochs.end()){
      I((*(mergeItR.base()))->myState==Epoch::State::Committed);
      (*(mergeItR.base()))->requestActiveMerge();
    }
  }
  
  void Thread::newFrontierCandidate(void){
    I(!raceFrontierHolders);
    EpochList::reverse_iterator nextFrontier(raceFrontier);
    I(nextFrontier!=threadEpochs.rend());
    I((*nextFrontier)->myState>=Epoch::State::Committed);
    I(nextFrontierClk<(*nextFrontier)->myVClock.value());
    nextFrontierClk=(*nextFrontier)->myVClock.value();
    ID(lastFrontierCandidate=(*nextFrontier));
    for(VClock::size_type i=0;i<VClock::size();i++){
      // If no thread with this index, do not count it
      if(!threadVector[i])
	continue;
      // If next-frontier has no races missed with indexed thread,
      // the indexed thread is not a frontier holder
      if(nextFrontierClk<=noRacesMissed[i])
	continue;
      raceFrontierHolders++;
    }
    if(!raceFrontierHolders)
      moveRaceFrontier();
  }

  size_t Epoch::createdNotSafeCount=0;
  size_t Epoch::safeNotAnalyzingCount=0;
  InstrCount Epoch::safeNotAnalyzingInstr=(InstrCount)0;
  size_t Epoch::safeNotAnalyzingBufferBlocks=0;
  size_t Epoch::analyzingNotMergingCount=0;
  InstrCount Epoch::analyzingNotMergingInstr=(InstrCount)0;
  size_t Epoch::analyzingNotMergingBufferBlocks=0;
  size_t Epoch::mergingNotDestroyedCount=0;

  EpochList Epoch::stoppedForAnalysis;

  EpochSet Epoch::analysisNeeded;
  EpochSet Epoch::analysisReady;
  EpochSet Epoch::analysisDone;


#if (defined CollectVersionCountStats)
  Epoch::VersionCounts Epoch::detWinVersCounts;
#endif // CollectVersionCountStats

  bool Epoch::doingAnalysis=false;
  
  Epoch *Epoch::activeMerging=0;
  EpochSet Epoch::activeWaitMerge;
  EpochSet Epoch::endedWaitSafe;
  EpochSet Epoch::endedWaitMerge;
  
  Epoch::CmpResult Epoch::compareSClock(const Epoch *otherEpoch) const{
    if(myThread==otherEpoch->myThread){
      if(mySClock<otherEpoch->mySClock)
	return StrongAfter;
      if(mySClock>otherEpoch->mySClock)
	return StrongBefore;
      I(mySClock!=otherEpoch->mySClock);
    }
    if(mySClock+syncClockDelta<otherEpoch->mySClock)
      return StrongAfter;
    if(mySClock-syncClockDelta>otherEpoch->mySClock)
      return StrongBefore;
    if(mySClock<otherEpoch->mySClock)
      return WeakAfter;
    if(mySClock>otherEpoch->mySClock)
      return WeakBefore;
    return Unordered;
  }
  void Epoch::advanceSClock(const Epoch *predEpoch,bool sync){
    // cout << "beg advanceSClock " << mySClock << ":" << getTid() << " ";
    //    printEpochs();
    I(myState==State::NotSucceeded);
    I(myState!=State::GlobalSafe);
    if(pendInstrCount){
      runtimeSClockAdjustments++;
      BufBlocksAdjusted.addSample(bufferBlocks.size());
    }
    I(predEpoch!=this);
    ClockValue clkInc=1;
    if(sync&&(myThread!=predEpoch->myThread))
      clkInc+=syncClockDelta;
    mySClock=predEpoch->mySClock+clkInc;
    I(clkInc>0);
    I(mySClock>predEpoch->mySClock);
    // Advance all buffered blocks
    for(BufferBlocks::const_iterator blockIt=bufferBlocks.begin();
	blockIt!=bufferBlocks.end();blockIt++){
      BufferBlock *block=blockIt->second;
      block->getVersions()->advance(block);
    }
    // Find new place in my thread's threadEpochs list
    if(myThreadEpochsPos!=myThread->threadEpochs.begin()){
      ID(EpochList::reverse_iterator newPosRIt(myThreadEpochsPos));
      I((*newPosRIt)->mySClock>mySClock);
    }
    // Find new place in the allEpochs list
    if(myAllEpochsPos!=allEpochs.begin()){
      EpochList::reverse_iterator newPosRIt(myAllEpochsPos);
      while((newPosRIt!=allEpochs.rend())&&(mySClock>(*newPosRIt)->mySClock)){
	// Should not advance past any epochs in the same thread
	I(myThread!=(*newPosRIt)->myThread);
	newPosRIt++;
      }
      // If new place not the same as old, splice into new place
      if(myAllEpochsPos!=newPosRIt.base()){
	EpochList::iterator oldNextPos=myAllEpochsPos;
	oldNextPos--;
	I(myState!=State::GlobalSafe);
	allEpochs.splice(newPosRIt.base(),allEpochs,myAllEpochsPos);
	ID(EpochList::iterator newPos=newPosRIt.base());
	ID(newPos--);
	I(myAllEpochsPos==newPos);
	(*oldNextPos)->tryGlobalSave();
      }
    }
    // cout << "end advanceSClock " << mySClock << ":" << getTid() << " ";
    // printEpochs();
  }

  Epoch::Epoch(ThreadID tid)
    : myPid(-1),
      beginDecisionExe(0), endDecisionExe(0),
      beginCurrentExe(0), endCurrentExe(0),
      pendInstrCount(0), execInstrCount(0), doneInstrCount(0), maxInstrCount(0),
      myThread(new Thread(tid)),
      myVClock(myThread->getIndex()),
      parentSClock(0), mySClock(syncClockDelta),
      runtimeSClockAdjustments(0), myCheckpoint(0),
      myAllEpochsPos(allEpochs.insert(findPos(mySClock),this)),
      myThreadEpochsPos(myThread->addEpoch(this)),
      numSrcAnomalies(0), numDstAnomalies(0),
      numSrcRaces(0), numDstRaces(0),
     myState(), sysCallLog(), sysCallLogPos(sysCallLog.begin()){
    numBegThread++;
    // The original context of the thread is frozen while its epochs execute
    osSim->stop(static_cast<Pid_t>(tid));
    // Initial epoch is always ThreadSafe
    threadSave();
    // Create my SESC context
    createContext(osSim->getContext(tid));
  }
  
  Epoch::Epoch(Epoch *parentEpoch)
    : myPid(-1),
      beginDecisionExe(0), endDecisionExe(0),
      beginCurrentExe(0), endCurrentExe(0),
      pendInstrCount(0), execInstrCount(0), doneInstrCount(0), maxInstrCount(0),
      myThread(parentEpoch->myThread),
      myVClock(parentEpoch->myVClock),
      parentSClock(parentEpoch->mySClock), mySClock(parentSClock+1),
      runtimeSClockAdjustments(0),
      myAllEpochsPos(allEpochs.insert(findPos(mySClock),this)),
      myThreadEpochsPos(myThread->addEpoch(this)),
      numSrcAnomalies(0), numDstAnomalies(0),
      numSrcRaces(0), numDstRaces(0),
      myState(), sysCallLog(), sysCallLogPos(sysCallLog.begin()){
    // Create my SESC context
    createContext(osSim->getContext(parentEpoch->myPid));
  }
  
  Epoch *Epoch::initialEpoch(ThreadID tid){
    return new Epoch(tid);
  }

  Epoch *Epoch::spawnEpoch(void){
    I(myState==State::Running);
    I(myState!=State::Atomic);
    Epoch *newEpoch=0;
    // Try to find a matching epoch already spawned in a previous execution
    if(myState==State::WasSucceeded){
      EpochList::reverse_iterator searchIt(myThreadEpochsPos);
      while(searchIt!=myThread->threadEpochs.rend()){
	I((*searchIt)->myThread==myThread);
	if((*searchIt)->parentSClock!=mySClock)
	  continue;
	if((*searchIt)->myState!=State::Spawning)
	  continue;
	newEpoch=*searchIt;
      }
    }
    // If not found, create the new epoch
    if(!newEpoch)
      newEpoch=new Epoch(this);
    // Parent epoch is succeeded by the child and child preceeded by parent
    succeeded(newEpoch);
    return newEpoch;
  }
  
  void Epoch::beginAcquire(void){
    I(myState==State::PreAccess);
    myState=State::Acquire;
    I(bufferBlocks.empty());
  }

  void Epoch::retryAcquire(void){
    I(myState==State::Acquire);
    I(!(myState>=State::GlobalSafe));
    I(myState==State::Initial);
    I(myState==State::NotSucceeded);
    squash();
  }

  void Epoch::endAcquire(void){
    I(myState==State::Acquire);
    myState=State::Access;
    AcquireBlocksNeeded.addSample(bufferBlocks.size());
    // Being in Acquire precludes being GlobalSafe
    tryGlobalSave();
  }
  void Epoch::skipAcquire(void){
    I(myState==State::PreAccess);
    myState=State::Access;
    // Being in Acquire precludes being GlobalSafe
    tryGlobalSave();
  }
  void Epoch::beginRelease(void){
    I(myState==State::Access);
    myState=State::Release;
  }
  void Epoch::endRelease(void){
    I(myState==State::Release);
    myState=State::PostAccess;
  }
  void Epoch::skipRelease(void){
    I(myState==State::Access);
    myState=State::PostAccess;
  }

  void Epoch::succeeded(Epoch *succEpoch){
    if(myState==State::NotSucceeded){
      myState=State::WasSucceeded;
      // Being NotSucceeded could disqualify us for GlobalSafe status
      // Now we are Ordered, so see if we qualify now
      tryGlobalSave();
    }
  }

  void Epoch::threadSave(void){
    I(myState==State::Spec);
    I(myThread->threadSafeAvailable==true);
    myState=State::ThreadSafe;
    myThread->threadSafeAvailable=false;
    myThread->moveThreadSafe();
    if(myState==State::WaitThreadSafe){
      myState=State::Running;
      myState=State::NoWait;
      osSim->unstop(myPid);
    }
    // Now try to become GlobalSafe
    if(globalSafeAvailable)
      tryGlobalSave();
  }
  
  void Epoch::complete(void){
    I(myState==State::Running);
    ID(printf("Ending %ld:%d numInstr %lld\n",mySClock,getTid(),pendInstrCount));
    // Remember the end time
    endCurrentExe=globalClock;
    // Disable SESC execution of this epoch
    osSim->stop(myPid);
    I(sysCallLogPos==sysCallLog.end());
    // Change the epoch's execution state to completed
    myState=State::Completed;
    // Completion ends ordering, if it is not already ended
    if(myState==State::Access)
      skipRelease();
    // If FullReplay, adjust event times in the trace
    if(myState==State::FullReplay){
      // Adjust the event times in the trace
      for(TraceList::iterator traceIt=myTrace.begin();
	  traceIt!=myTrace.end();traceIt++){
	TraceEvent *traceEvent=*traceIt;
	traceEvent->adjustTime(this);
      }
      for(EventSet::iterator eventIt=accessEvents.begin();
	  eventIt!=accessEvents.end();eventIt++){
	TraceAccessEvent *accessEvent=*eventIt;
	accessEvent->adjustTime(this);
      }
    }
    if(myState==State::GlobalSafe){
      tryCommit();
    }else{
      tryGlobalSave();
    }
  }
  
  void Epoch::advanceGlobalSafe(void){
    globalSafeAvailable=true;
    if(globalSafe!=allEpochs.begin()){
      EpochList::reverse_iterator globalSafeRIt(globalSafe);
      I(globalSafeRIt!=allEpochs.rend());
      (*globalSafeRIt)->tryGlobalSave();
    }
  }

  void Epoch::tryGlobalSave(void){
    if(!globalSafeAvailable)
      return;
    if(myState!=State::ThreadSafe)
      return;
    // Acquire can increment the clok, and a GlobalSafe epoch must not do that
    if(myState<=State::Acquire)
      return;
    EpochList::iterator globalSaveIt=myAllEpochsPos;
    globalSaveIt++;
    if((globalSaveIt==allEpochs.end())||
       ((*globalSaveIt)->myState>=State::Committed))
      globalSave();
  }

  void Epoch::globalSave(void){
    I(globalSafeAvailable);
    I(myState==State::ThreadSafe);
    I(myState>State::Acquire);
    myState=State::GlobalSafe;
    I(myState!=State::Merging);
    if(myState==State::WaitGlobalSafe){
      myState=State::Running;
      myState=State::NoWait;
      osSim->unstop(myPid);
    }else if(myState>=State::WaitFullyMerged){
      myState=State::ActiveMerge;
    }
    if(myState==State::ActiveMerge){
      beginMerge();
    }
    globalSafeAvailable=false;
    globalSafe--;
    I((*globalSafe)==this);
    // Try to also commit
    tryCommit();
  }

  void Epoch::tryCommit(void){
    I(myState!=State::Committed);
    I((myState==State::Completed)||(myState==State::GlobalSafe));
    if(myState!=State::Completed)
      return;
    if(myState!=State::GlobalSafe)
      return;
    I(pendInstrCount>=execInstrCount);
    I(execInstrCount>=doneInstrCount);
    if(doneInstrCount!=pendInstrCount)
      return;
    commit();
  }

  void Epoch::commit(void){
    ID(printf("Committing %ld:%d numInstr %lld\n",mySClock,getTid(),pendInstrCount));
    ID(Pid_t myPidTmp=myPid);
    I(myState==State::GlobalSafe);
    //If the epoch is already fully merged, it will be destroyed when commit is done
    bool destroyNow=((myState==State::Merging)&&bufferBlocks.empty());
    if(myState==State::FullReplay){
      I(pendInstrCount==maxInstrCount);
      I(beginDecisionExe);
      I(endDecisionExe);
    }else{
      I((!maxInstrCount)||(pendInstrCount<=maxInstrCount));
      I(!beginDecisionExe);
      I(!endDecisionExe);
      // Update Statistics
      NumRunSClockAdj.addSample(runtimeSClockAdjustments);
      I(numSrcRaces+numDstRaces||!(numSrcAnomalies+numDstAnomalies));
      SrcAnomaliesPerEpoch.addSample(numSrcAnomalies);
      DstAnomaliesPerEpoch.addSample(numDstAnomalies);
      SrcRacesPerEpoch.addSample(numSrcRaces);
      DstRacesPerEpoch.addSample(numDstRaces);
      if(numSrcRaces){
	SrcRacesPerSrcRaceEpoch.addSample(numSrcRaces);
        SrcAnomaliesPerSrcRaceEpoch.addSample(numSrcAnomalies);
	AnomaliesPerSrcRaceEpoch.addSample(numSrcAnomalies+numDstAnomalies);
      }
      if(numSrcRaces+numDstRaces){
	RacesPerRaceEpoch.addSample(numSrcRaces+numDstRaces);
	AnomaliesPerRaceEpoch.addSample(numSrcAnomalies+numDstAnomalies);
      }
      // Remember the start and end time
      beginDecisionExe=beginCurrentExe;
      endDecisionExe=endCurrentExe;
    }
    I(beginCurrentExe);
    I(beginCurrentExe<=endCurrentExe);
    // Remeber the instruction count
    maxInstrCount=pendInstrCount;
    // Transition into the Committed state
    myState=State::Committed;
    // We held the GlobalSafe and ThreadSafe status
    myThread->threadSafeAvailable=true;
    // Pass on the ThreadSafe status
    EpochList::reverse_iterator threadSaveRIt(myThreadEpochsPos);
    if(threadSaveRIt!=myThread->threadEpochs.rend()){
      (*threadSaveRIt)->threadSave();
    }
    myThread->commitEpoch(this);
    I((destroyNow&&(pidToEpoch[myPidTmp]==this))||
      ((!destroyNow)&&((pidToEpoch[myPidTmp]==0)||(myState!=State::Merging)||(!bufferBlocks.empty()))));
    if(destroyNow)
      delete this;
    advanceGlobalSafe();
  }

  void Epoch::squash(void){
    I(myState!=State::Merging);
    // Roll back the Spec state if needed
    if(myState==State::Committed){
      bool moveGlobalSafe=false;
      EpochList::reverse_iterator currRIt(myAllEpochsPos);
      for(currRIt--;currRIt!=allEpochs.rend();currRIt++){
	Epoch *currEpoch=(*currRIt);
	if(currEpoch->myState!=State::Spec){
          if(currEpoch->myState==State::Committed){
            moveGlobalSafe=true;
            currEpoch->myState=State::FullReplay;
	  }else if(currEpoch->myState==State::GlobalSafe){
            moveGlobalSafe=true;
	    currEpoch->myThread->threadSafeAvailable=true;
            currEpoch->myState=State::PartReplay;
	  }else if(currEpoch->myState==State::ThreadSafe){
	    currEpoch->myThread->threadSafeAvailable=true;
	  }
	  currEpoch->myState=State::Spec;
	}
      }
      if(moveGlobalSafe){
        globalSafeAvailable=true;
        globalSafe=myAllEpochsPos;
        I(!((*globalSafe)->myState>=State::GlobalSafe));
        globalSafe++;
        I((globalSafe==allEpochs.end())||((*globalSafe)->myState==State::Committed));
      }
    }
    if(myState==State::NotSucceeded){
      squashLocal<false>();
    }else{
      // Determine which epochs are unspawned and which are restarted
      typedef std::pair<Thread *,ClockValue> EpochDescriptor;
      typedef std::multiset<EpochDescriptor> EpochDescriptors;
      EpochList specSpawns;
      EpochList prevSpawns;
      EpochDescriptors specSpawners;
      EpochDescriptors safeSpawners;
      EpochList::reverse_iterator currRIt(myAllEpochsPos);
      for(currRIt--;currRIt!=allEpochs.rend();currRIt++){
	Epoch *currEpoch=(*currRIt);
	EpochDescriptor currDesc(currEpoch->myThread,currEpoch->mySClock);
	EpochDescriptor prevDesc(currEpoch->myThread,currEpoch->parentSClock);
	if(currEpoch->myState==State::Initial){
	  specSpawners.insert(currDesc);
	}else{
	  safeSpawners.insert(currDesc);
	}
	if(specSpawners.count(prevDesc)){
	  I(currEpoch->myState==State::Spec);
	  specSpawns.push_front(currEpoch);
	}else{
	  if(!safeSpawners.count(prevDesc))
	    prevSpawns.push_back(currEpoch);
	}
      }
      for(EpochList::iterator specSpawnIt=specSpawns.begin();
	  specSpawnIt!=specSpawns.end();specSpawnIt++)
	(*specSpawnIt)->unspawn();
      EpochList::reverse_iterator squashRIt(myAllEpochsPos);
      for(squashRIt--;squashRIt!=allEpochs.rend();squashRIt++){
	Epoch *squashEpoch=*squashRIt;
	if(squashEpoch==prevSpawns.front()){
	  squashEpoch->squashLocal<false>();
	  prevSpawns.pop_front();
	}else{
	  squashEpoch->squashLocal<false>();
	}
      }
      I(prevSpawns.empty());
    }
  }

  template<bool parentSquashed>
  void Epoch::squashLocal(void){
    // Roll back the Exec state
    if(myState==State::Running)
      osSim->stop(myPid);
    myState=State::Spawning;
    myState=State::NoWait;
    // Roll back the predecessor-ordering state
    myState=State::PreAccess;
    // Spec state already rolled back in the global squash method
    I(myState!=State::Committed);
    // Erase buffered blocks
    eraseBuffer();
    // Restore initial SESC execution context
    ThreadContext *context=osSim->getContext(myPid);
    *context=myContext;

    doneInstrCount-=pendInstrCount;
    execInstrCount-=pendInstrCount;
    pendInstrCount=0;

    beginCurrentExe=0;
    endCurrentExe=0;
    if(myState==State::Initial){
      sysCallLog.clear();
    }
    sysCallLogPos=sysCallLog.begin();
    // Try to restart execution and transition into less speculative states
    if(!parentSquashed){
      if(myThread->threadSafeAvailable){
	EpochList::iterator checkIt=myThreadEpochsPos;
	checkIt++;
	if((checkIt==myThread->threadEpochs.end())||((*checkIt)->myState==State::Committed))
	  threadSave();
      }
      run();
    }else{
      I(myState==State::Spec);
    }
  }

  void Epoch::unspawn(void){
    I(myState==State::Spec);
    I(myState==State::Initial);
    if(myState==State::Running){
      myState=State::Spawning;
      osSim->stop(myPid);
    }
    delete this;
  }

  void Epoch::requestActiveMerge(void){
    if(myState==State::Merging){
      beginActiveMerge();
    }else{
      myState=State::ActiveMerge;
      if(myState>=State::GlobalSafe)
	beginMerge();
    }
  }
  
  void Epoch::beginMerge(void){
    ID(Pid_t myPidTmp=myPid);
    I(myState!=State::Merging);
    I(myState>=State::GlobalSafe);
    // Predecessors also become merging
    EpochList::iterator prevIt=myAllEpochsPos;
    prevIt++;
    if((prevIt!=allEpochs.end())&&((*prevIt)->myState!=State::Merging))
      (*prevIt)->beginMerge();
    // Update per-epoch statistics
    SysCallCount.addSample(sysCallLog.size());
    EpochInstObserver.addSample(pendInstrCount);
    EpochBuffObserver.addSample(bufferBlocks.size()*blockSize);
    // Now my state is Merging
    myState=State::Merging;
    // Find the checkpoint to merge into and start the merging process
    myCheckpoint=Checkpoint::mergeInit(this);
    I(pidToEpoch[myPidTmp]==this);
    if(bufferBlocks.empty()){
      if(myState==State::Committed){
	I(myState==State::NoWait);
	delete this;
	return;
      }
      if(myState==State::WaitFullyMerged)
	becomeFullyMerged();
    }
    if(myState==State::ActiveMerge)
      beginActiveMerge();
  }
  
  void Epoch::beginActiveMerge(void){
    I(myState>=State::GlobalSafe);
    I(myState==State::Merging);
    // Predecessors also enter ActiveMerge
    EpochList::iterator prevIt=myAllEpochsPos;
    prevIt++;
    if((prevIt!=allEpochs.end())&&((*prevIt)->myState!=State::ActiveMerge))
      (*prevIt)->beginActiveMerge();
    myState=State::ActiveMerge;
    // For now we immediatelly merge the entire buffer on ActiveMerge
    mergeBuffer();
  }

  Epoch::~Epoch(void){
    ID(printf("Deleting %ld:%d numInstr %lld\n",mySClock,getTid(),pendInstrCount));
    I(pendInstrCount);
    I(execInstrCount==pendInstrCount);
    I(doneInstrCount==pendInstrCount);
    I(myState!=State::Running);
    I((myState==State::Merging)||(!myCheckpoint));
    if(myState==State::Merging){
      I(myCheckpoint);
      myCheckpoint->mergeDone(this);
    }
    // Remove epoch from the allEpochs list
    I(myState!=State::GlobalSafe);
    if(myAllEpochsPos==globalSafe)
      globalSafe++;
    allEpochs.erase(myAllEpochsPos);
    // Remove epoch from its thread, delete the thread
    // if this was the last epoch
    if(myThread->removeEpoch(this)){
      delete myThread;
      numEndThread++;
    }
    // Remove pid-to-epoch mapping
    I(pidToEpoch[myPid]==this);
    pidToEpoch[myPid]=0;
    // The SESC thread for this epoch ends now
    osSim->eventExit(myPid,0);
    ID(myPid=-1);
  }
  
//     ID(lastSafe--);
//     I(lastSafe==myAllEpochsPos);
//     lastSafe=myAllEpochsPos;
//     createdNotSafeCount--;
//     // Become the last safe epoch
//     safeNotAnalyzingCount++;
//     safeNotAnalyzingInstr+=(InstrCount)pendInstrCount;
//     safeNotAnalyzingBufferBlocks+=bufferBlocks.size();
//     // See if now also Decided
//     if(myState>=Decidable){
//       becomeDecided();
//       // If the first safe epoch in its thread,
//       // this epoch is also the analyzedUb
//       if(!myThread->hasAnalyzedUb())
// 	myThread->updateAnalyzedUb(this);
//       // Try to propagate the safe status
//       if(lastSafe!=allEpochs.begin()){
// 	EpochList::reverse_iterator nextIt(lastSafe);
// 	(*nextIt)->tryToBecomeSafe();
//       }
//     }else{
//       // If the first safe epoch in its thread,
//       // this epoch is also the analyzedUb
//       if(!myThread->hasAnalyzedUb())
// 	myThread->updateAnalyzedUb(this);
//     }
//   }
//   void Epoch::becomeDecided(){
//     if(myState>=Decided){
//       // Verify the instruction count
//       I(maxInstrCount==pendInstrCount);
//       I(beginDecisionExe);
//       I(beginDecisionExe<endDecisionExe);
//       I(endDecisionExe<=beginCurrentExe);
//       I(beginCurrentExe<endCurrentExe);
//       // Update statistics
//       double originalTime=endDecisionExe-beginDecisionExe;
//       double replayTime=endCurrentExe-beginCurrentExe;
//       ReplayRelSpeed.addSample(replayTime/originalTime);
//       RaceEpochAnomalies.addSample(numAnomalies);
//       // Done analyzing this epoch
//       I(!analysisDone.count(this));
//       analysisDone.insert(this);
//       // Is this the last epoch to analyze?
//       if(analysisDone.size()==analysisNeeded.size()){
// 	completeAnalysis();
//       }else{
// 	EpochList::reverse_iterator analyzeRIt(myAllEpochsPos);
// 	while(!analysisNeeded.count(*analyzeRIt)){
// 	  I(analyzeRIt!=allEpochs.rend());
// 	  analyzeRIt++;
// 	}
// 	I(analyzeRIt!=allEpochs.rend());
// 	I((*analyzeRIt)->myState<Merging);
// 	I((*analyzeRIt)->myState>=Decided);
// 	I((*analyzeRIt)->myState*Execution==Complete);
// 	I((*analyzeRIt)->myState>=Safe);
// 	(*analyzeRIt)->doSquash();
// 	I((*analyzeRIt)->isExecutable());
// 	(*analyzeRIt)->becomeExecuting();
// 	I((*analyzeRIt)->myState*Execution!=None);
//       }
//     }else{
//       I((!maxInstrCount)||(pendInstrCount<=maxInstrCount));
//       I(!beginDecisionExe);
//       I(!endDecisionExe);
//       I(beginCurrentExe);
//       I(beginCurrentExe<=endCurrentExe);
//       // Update Statistics
//       NumRunSClockAdj.addSample(runtimeSClockAdjustments);
//       // Remeber the instruction count
//       maxInstrCount=currInstrCount;
//       // Change the upper bound for decided epochs in this thread
//       myState-=DecidedUb;
//       if(immedSeqSucc){
// 	immedSeqSucc->myState+=DecidedUb;
// 	myThread->updateSpawnSafe(immedSeqSucc->myVClock);
//       }else{
// 	myThread->updateSpawnSafe(VClock::getInfiniteVClock());
//       }
//       // Try to transition to the Analyzing state
//       I(lastSafe!=lastAnalyzing);
//       EpochList::reverse_iterator nextIt(lastAnalyzing);
//       if(*nextIt==this)
// 	tryToAnalyze(false);
//     }
//   }

//   bool Epoch::isAnalyzable(bool missRaces) const{
//     I(!missRaces);
//     if(lastAnalyzing==lastSafe)
//       return false;
//     EpochList::reverse_iterator nextIt(lastAnalyzing);
//     if(*nextIt!=this)
//       return false;
//     if((!missRaces)&&(!myThread->canAnalyze(this)))
//       //    if((!missRaces)&&(!preventingAnalysis.empty()))
//       return false;
//     if(doingAnalysis)
//       return false;
//     return true;
//   }

//   void Epoch::tryToAnalyze(bool missRaces){
//     if(!isAnalyzable(missRaces))
//       return;
//     // Schedule the analysis
//     analyzeEpochs.insert(this);
//     hasPostExecActions=true;
//   }

//   void Epoch::becomeAnalyzing(void){
//     I(myState>=Decidable);
//     I(myState>=Decided);
//     I(myState<Merging);
//     //    ID(printf("Analyzing %d(%d)\n",getCid(),myEid));
//     // Update statistics variables
//     safeNotAnalyzingCount--;
//     safeNotAnalyzingInstr-=(InstrCount)currInstrCount;
//     safeNotAnalyzingBufferBlocks-=bufferBlocks.size();
//     analyzingNotMergingCount++;
//     analyzingNotMergingInstr+=(InstrCount)currInstrCount;
//     analyzingNotMergingBufferBlocks+=bufferBlocks.size();
//     // This epoch is the last to become Analyzing
//     ID(EpochList::iterator checkIt=lastAnalyzing);
//     I(checkIt!=allEpochs.begin());
//     ID(checkIt--);
//     I(checkIt==myAllEpochsPos);
//     lastAnalyzing=myAllEpochsPos;
//     // Remove this epoch from the analyze set
//     I(analyzeEpochs.count(this));
//     analyzeEpochs.erase(this);
//     //    // Races missed if preventingMerge not empty
//     //    I(preventingAnalysis.empty());
//     // Races missed if thread thinks epoch can not be analyzed
//     I(myThread->canAnalyze(this));
//     //    // No longer prevented from merging
//     //    preventingAnalysis.clear();
//     // Normally, an epoch needs no analysis
//     bool needAnalysis=false;
//     // Does it have backward races?
//     if(!myBackRacesByAddrEp.empty()){
//       // Need to trace all race addresses
//       for(RaceByAddrEp::iterator backAddrIt=myBackRacesByAddrEp.begin();
// 	  backAddrIt!=myBackRacesByAddrEp.end();backAddrIt++){
// 	Address dAddrV=backAddrIt->first;
// 	traceDataAddresses.insert(dAddrV);
// #if (defined DEBUG)
// 	const RaceByEp &backRacesByEp=backAddrIt->second->raceByEp;
// 	for(RaceByEp::const_iterator backEpIt=backRacesByEp.begin();
// 	    backEpIt!=backRacesByEp.end();backEpIt++){
// 	  I(analysisNeeded.count(backEpIt->first));
// 	}
// #endif // (defined DEBUG)
//       }
//       // Analysis is needed
//       needAnalysis=true;
// #if (defined DEBUG)
//       for(RaceByEpAddr::iterator backEpochIt=myBackRacesByEpAddr.begin();
// 	  backEpochIt!=myBackRacesByEpAddr.end();backEpochIt++){
// 	Epoch *epoch=backEpochIt->first;
// 	I(analysisNeeded.count(epoch));
// 	const RaceByAddr &backRacesByAddr=backEpochIt->second;
// 	for(RaceByAddr::const_iterator backAddrIt=backRacesByAddr.begin();
// 	    backAddrIt!=backRacesByAddr.end();backAddrIt++){
// 	  I(traceDataAddresses.count(backAddrIt->first));
// 	}
//       }
// #endif // (defined DEBUG)
//     }
//     // Does the epoch have forward races?
//     if(!myForwRacesByAddrEp.empty()){
//       // Need tot trace all race addresses
//       for(RaceByAddrEp::iterator forwIt=myForwRacesByAddrEp.begin();
// 	  forwIt!=myForwRacesByAddrEp.end();forwIt++){
// 	Address dAddrV=forwIt->first;
// 	traceDataAddresses.insert(dAddrV);
//       }
//       // Analysis is needed
//       needAnalysis=true;
//       // We also need to analyze all successor epochs
//       // that have races with this one  
//       for(RaceByEpAddr::iterator epochIt=myForwRacesByEpAddr.begin();
// 	  epochIt!=myForwRacesByEpAddr.end();epochIt++){
// 	Epoch *epoch=epochIt->first;
// 	analysisNeeded.insert(epoch);
//       }
//     }
//     // Position in the analyzingToMerge list where this sepoch goes
//     //    EpochList::iterator insBefore;
//     if(needAnalysis){
//       // Analysis is needed
//       analysisNeeded.insert(this);
//       // And the epoch is ready to be analyzed
//       analysisReady.insert(this);
//       // The epoch goes to the end of the list
//       //      insBefore=analyzingToMerge.end();
//     }else{
// #if !(defined SHORTEN_ANALYSIS_WINDOW)
//       // The epoch goes to the end of the list
//       //      insBefore=analyzingToMerge.end();
// #else // SHORTEN_ANALYSIS_WINDOW
//       I(0);
//       // Try to go ahead of all analysisReady epochs, if any
//       size_t passNeeded=analysisReady.size();
//       if(passNeeded){
// 	//	insBefore=analyzingToMerge.end();
// 	while(true){
// 	  EpochList::iterator tmpIt=insBefore;
// 	  tmpIt--;
// 	  Epoch *tmpEpoch=*tmpIt;
// 	  VClokc::CmpResult cmpRes=compareVClock(tmpEpoch);
// 	  // Safe before this => can not come after this
// 	  I(cmpRes<WeakAfter);
// 	  if(cmpRes<=WeakBefore)
// 	    break;
// 	  insBefore=tmpIt;
// 	  if(analysisReady.count(tmpEpoch)){
// 	    passNeeded--;
// 	    if(!passNeeded)
// 	      break;
// 	  }
// 	  //	  I(insBefore!=analyzingToMerge.begin());
// 	}
// 	if(passNeeded){
// 	  //	  insBefore=analyzingToMerge.end();
// #if (defined DEBUG)
// 	  bool foundOne=false;
// 	  for(EpochSet::const_iterator checkIt=analysisReady.begin();
// 	      checkIt!=analysisReady.end();checkIt++){
// 	    I(compareVClock(*checkIt)<WeakAfter);
// 	    if(compareVClock(*checkIt)<=WeakBefore)
// 	      foundOne=true;
// 	  }
// 	  I(foundOne);
// #endif
// 	}else{
// 	  printf("Fast-analyzing epoch %d(%d)\n",getCid(),myEid);
// 	}
//       }
// #endif // SHORTEN_ANALYSIS_WINDOW
//     }
//     if(needAnalysis){
//       // If all needed epochs are ready, do the analysis
//       if(analysisNeeded.size()==analysisReady.size())
// 	beginAnalysis();
//     }else{
//       // No analysis needed, try to merge
//       I(lastMerging!=lastAnalyzing);
//       EpochList::reverse_iterator nextIt(lastMerging);
//       I(nextIt!=allEpochs.rend());
//       if(*nextIt==this)
// 	tryToMerge();
//     }
//     // Update the upper bound for analyzed epochs in this thread
//     myThread->updateAnalyzedUb(immedSeqSucc);
//     // Try to propagate the Analyzing status
//     if(lastAnalyzing!=lastSafe){
//       EpochList::reverse_iterator nextIt(lastAnalyzing);
//       I(nextIt!=allEpochs.rend());
//       (*nextIt)->tryToAnalyze(false);
//     }
//   }

//   void Epoch::beginAnalysis(void){
//     printf("Analysis begins\n");
//     I(analysisReady.size()>1);
//     // Clear the ready set
//     analysisReady.clear();
//     // No non-Safe epoch should be Executing during analysis
//     for(EpochList::reverse_iterator epochRIt(lastSafe);
// 	epochRIt!=allEpochs.rend();epochRIt++){
//       Epoch *epoch=*epochRIt;
//       I(epoch->myState<Safe);
//       if(epoch->myState*Execution==Complete)
// 	continue;
//       if(epoch->myState*Execution==None)
// 	continue;
//       if(epoch->currInstrCount){
// 	epoch->truncateEpoch(true);
//       }else{
// 	epoch->myState-=Execution;
// 	Pid_t pid=static_cast<Pid_t>(epoch->myEid);
// 	osSim->stop(pid);
// 	stoppedForAnalysis.push_back(epoch);
//       }
//     }
// #if (defined DEBUG)
//     for(EpochList::iterator stopIt=stoppedForAnalysis.begin();
// 	stopIt!=stoppedForAnalysis.end();stopIt++){
//       printf("Stopped %d(%d) for analysis\n",(*stopIt)->getTid(),(*stopIt)->myEid);
//     }
//     for(EpochList::iterator epochIt=allEpochs.begin();
// 	epochIt!=lastSafe;epochIt++){
//       Epoch *epoch=*epochIt;
//       I((epoch->myState*Execution==Complete)||(epoch->myState*Execution==None));
//     }
// #endif // (defined DEBUG)
//     // Now switch to analysis mode
//     doingAnalysis=true;
//     if(dataRacesIgnored){
//       completeAnalysis();
//       return;
//     }
//     EpochList::reverse_iterator analyzeRIt(lastMerging);
//     while(!analysisNeeded.count(*analyzeRIt)){
//       I(analyzeRIt!=allEpochs.rend());
//       analyzeRIt++;
//     }
//     I(analyzeRIt!=allEpochs.rend());
//     I((*analyzeRIt)->myState<Merging);
//     I((*analyzeRIt)->myState>=Decidable);    
//     I((*analyzeRIt)->myState>=Decided);
//     (*analyzeRIt)->doSquash();
//     I((*analyzeRIt)->isExecutable());
//     (*analyzeRIt)->becomeExecuting();
//     I((*analyzeRIt)->myState*Execution==Ordered);
//   }
  
//   void Epoch::completeAnalysis(void){
//     I(analysisReady.empty());
//     EpochList::reverse_iterator analyzingBeginRIt(lastMerging);
//     EpochList::reverse_iterator analyzingEndRIt(lastAnalyzing);
//     printf("Analysis trace begins\n");
//     // The last Analyzing epochs should be one of the analyzed epochs
//     // The first Analyzing epochs should be one of the analyzed epochs
//     I(analysisDone.count(*lastAnalyzing));
//     I(analysisDone.count(*analyzingBeginRIt));
//     for(EpochList::reverse_iterator epochRIt=analyzingBeginRIt;
// 	epochRIt!=analyzingEndRIt;epochRIt++){
//       Epoch *epochTraceEpoch=*epochRIt;
//       if(!analysisDone.count(epochTraceEpoch))
// 	continue;
//       I(!epochTraceEpoch->traceDataAddresses.empty());
//       I(!epochTraceEpoch->traceCodeAddresses.empty());
//       printf("Thread %d (SClock %d) has races at code addresses",
// 	     epochTraceEpoch->getTid(),epochTraceEpoch->mySClock);
//       for(AddressSet::iterator codeAddrIt=epochTraceEpoch->traceCodeAddresses.begin();
// 	  codeAddrIt!=epochTraceEpoch->traceCodeAddresses.end();codeAddrIt++){
// 	printf(" %08lx",*codeAddrIt);
//       }
//       printf("\n");
//       if(epochTraceEpoch->numAnomalies)
// 	printf("(Anomalies were also found)\n");
//       epochTraceEpoch->traceCodeAddresses.clear();
//       TraceList &epochTrace=epochTraceEpoch->myTrace;
//       if(epochTrace.size()<30){
// 	for(TraceList::iterator traceIt=epochTrace.begin();
// 	    traceIt!=epochTrace.end();traceIt++){
// 	  TraceEvent *traceEvent=*traceIt;
// 	  traceEvent->print();
// 	  printf("\n");
// 	}
//       }
//     }
//     printf("Analysis trace ends\n");
//     {
//       size_t foundAnomalies=0;
//       for(EpochSet::iterator epochIt=analysisDone.begin();
// 	  epochIt!=analysisDone.end();epochIt++){
// 	Epoch *epoch=*epochIt;
// 	if(epoch->numAnomalies){
// 	  foundAnomalies+=epoch->numAnomalies;
// 	  epoch->numAnomalies=0;
// 	}
//       }
//       RaceGroupAnomalies.addSample(foundAnomalies);
//     }      
//     // Compute and update race group window statistics
//     {
//       // Start with all the epochs in the analysis window
//       size_t raceGrpWinEpochs=analyzingNotMergingCount;
//       InstrCount raceGrpWinInstr=analyzingNotMergingInstr;
//       size_t raceGrpWinBlocks=analyzingNotMergingBufferBlocks;
// #if (defined CollectVersionCountStats)
//       VersionCounts versionCounts;
// #endif // CollectVersionCountStats
//       // Now remove any epochs we can
//       for(EpochList::reverse_iterator epochRIt=analyzingBeginRIt;
// 	  epochRIt!=analyzingEndRIt;epochRIt++){
// 	Epoch *epoch=*epochRIt;
// 	// Can not remove epochs that require analysis
// 	if(analysisDone.count(epoch)){
// #if (defined CollectVersionCountStats)
// 	  versionCounts.add(epoch);
// #endif // CollectVersionCountStats
// 	  continue;
// 	}
// 	// Of the remaining epochs, can remove only those that are
// 	// 1) before all the require-analysis epochs, or
// 	// 2) after all the reqire-analysis epochs
// 	bool beforeAll=true;
// 	bool afterAll=true;
// 	for(EpochSet::iterator raceIt=analysisDone.begin();
// 	    (raceIt!=analysisDone.end())&&(beforeAll||afterAll);raceIt++){
// 	  Epoch *raceEpoch=*raceIt;
// 	  CmpResult cmpRes=epoch->compareSClock(raceEpoch);
// 	  // If race epoch is before epoch, epoch can not be before all race epochs
// 	  if(cmpRes<=WeakBefore)
// 	    beforeAll=false;
// 	  // If race epoch is after epoch, epoch can not be after all race epochs
// 	  if(cmpRes>=WeakAfter)
// 	    afterAll=false;
// 	}
// 	beforeAll=afterAll=false;
// 	if((!beforeAll)&&(!afterAll)){
// #if (defined CollectVersionCountStats)
// 	  versionCounts.add(epoch);
// #endif // CollectVersionCountStats
// 	  continue;
// 	}
// 	// Epoch can be removed from the window, update stats
// 	raceGrpWinEpochs--;
// 	raceGrpWinInstr-=(InstrCount)epoch->currInstrCount;
// 	raceGrpWinBlocks-=epoch->bufferBlocks.size();
//       }
//       RaceGrpWinEpochObserver.addSample(raceGrpWinEpochs);
//       RaceGrpWinInstObserver.addSample(raceGrpWinInstr);
//       RaceGrpWinBuffObserver.addSample(raceGrpWinBlocks*blockSize);
// #if (defined CollectVersionCountStats)
//       RaceGrpWinVersObserver.addSample(versionCounts.getMaxNotRdOnly());
// #endif // CollectVersionCountStats
//     }
//     // Compute and update race group initial race statistics
//     {
//       // Find the earliest access that is the second access in a race
//       Time_t firstBackRaceTime=globalClock;
//       Epoch *firstBackRaceEpoch=0;
//       for(EpochSet::iterator raceIt=analysisDone.begin();
// 	  raceIt!=analysisDone.end();raceIt++){
// 	Epoch *epoch=*raceIt;
// 	for(RaceByAddrEp::iterator backRaceAddrIt=epoch->myBackRacesByAddrEp.begin();
// 	    backRaceAddrIt!=epoch->myBackRacesByAddrEp.end();backRaceAddrIt++){
// 	  RaceAddrInfo *raceAddrInfo=backRaceAddrIt->second;
// 	  if(raceAddrInfo->readAccess){
// 	    Time_t readTime=raceAddrInfo->readAccess->getTime();
// 	    if(firstBackRaceTime>readTime){
// 	      firstBackRaceTime=readTime;
// 	      firstBackRaceEpoch=epoch;
// 	    }
// 	  }
// 	  if(raceAddrInfo->writeAccess){
// 	    Time_t writeTime=raceAddrInfo->writeAccess->getTime();
// 	    if(firstBackRaceTime>writeTime){
// 	      firstBackRaceTime=writeTime;
// 	      firstBackRaceEpoch=epoch;
// 	    }
// 	  }
// 	}
//       }
//       // Initially include only the firstBackRaceEpoch in the stats
//       size_t iniRaceWinEpochs=1;
//       InstrCount iniRaceWinInstr=(InstrCount)(firstBackRaceEpoch->currInstrCount);
//       size_t iniRaceWinBlocks=firstBackRaceEpoch->bufferBlocks.size();
// #if (defined CollectVersionCountStats)
//       VersionCounts versionCounts;
//       versionCounts.add(firstBackRaceEpoch);
// #endif // CollectVersionCountStats
//       // Now get stats for the range from start of window to the first-race epoch
//       for(EpochList::reverse_iterator epochRIt=analyzingBeginRIt;
// 	  (*epochRIt)!=firstBackRaceEpoch;epochRIt++){
// 	Epoch *epoch=*epochRIt;
// 	if(epoch->endDecisionExe>firstBackRaceTime)
// 	  continue;
// 	iniRaceWinEpochs++;
// 	iniRaceWinInstr+=(InstrCount)(epoch->currInstrCount);
// 	iniRaceWinBlocks+=epoch->bufferBlocks.size();
// #if (defined CollectVersionCountStats)
// 	versionCounts.add(epoch);
// #endif // CollectVersionCountStats
//       }
//       RaceGrpIniEpochObserver.addSample(iniRaceWinEpochs);
//       RaceGrpIniInstObserver.addSample(iniRaceWinInstr);
//       RaceGrpIniBuffObserver.addSample(iniRaceWinBlocks*blockSize);
// #if (defined CollectVersionCountStats)
//       RaceGrpIniVersObserver.addSample(versionCounts.getMaxNotRdOnly());
// #endif // CollectVersionCountStats
//     }
//     // Compute and update race group shortest race statistics
//     {
//       Time_t shortRaceDist=globalClock;
//       Time_t shortRaceFirstTime=0;
//       Time_t shortRaceSecondTime=0;
//       Epoch *shortRaceFirstEpoch=0;
//       Epoch *shortRaceSecondEpoch=0;
//       for(EpochSet::iterator firstEpochIt=analysisDone.begin();
// 	  firstEpochIt!=analysisDone.end();firstEpochIt++){
// 	Epoch *firstEpoch=*firstEpochIt;
// 	for(RaceByAddrEp::iterator forwRaceAddrIt=firstEpoch->myForwRacesByAddrEp.begin();
// 	    forwRaceAddrIt!=firstEpoch->myForwRacesByAddrEp.end();forwRaceAddrIt++){
// 	  Address dataAddress=forwRaceAddrIt->first;
// 	  RaceAddrInfo *forwRaceAddrInfo=forwRaceAddrIt->second;
// 	  if(forwRaceAddrInfo->readAccess){
// 	    Time_t firstTime=forwRaceAddrInfo->readAccess->getTime();
// 	    for(RaceByEp::iterator secondEpochIt=forwRaceAddrInfo->raceByEp.begin();
// 		secondEpochIt!=forwRaceAddrInfo->raceByEp.end();secondEpochIt++){
// 	      RaceInfo *raceInfo=secondEpochIt->second;
// 	      // First access is a read, must be anti-type race
// 	      if(!(raceInfo->raceType&RaceInfo::Anti))
// 		continue;
// 	      Epoch *secondEpoch=secondEpochIt->first;
// 	      RaceByAddrEp::iterator
// 		backRaceAddrIt=secondEpoch->myBackRacesByAddrEp.find(dataAddress);
// 	      I(backRaceAddrIt!=secondEpoch->myBackRacesByAddrEp.end());
// 	      RaceAddrInfo *backRaceAddrInfo=backRaceAddrIt->second;
// 	      I(backRaceAddrInfo->writeAccess);
// 	      Time_t secondTime=backRaceAddrInfo->writeAccess->getTime();
// 	      Time_t diffTime=(secondTime>firstTime)?(secondTime-firstTime):0;
// 	      if(diffTime<shortRaceDist){
// 		shortRaceDist=diffTime;
// 		shortRaceFirstTime=firstTime;
// 		shortRaceSecondTime=secondTime;
// 		shortRaceFirstEpoch=firstEpoch;
// 		shortRaceSecondEpoch=secondEpoch;
// 	      }
// 	    }
// 	  }
// 	  if(forwRaceAddrInfo->writeAccess){
// 	    Time_t firstTime=forwRaceAddrInfo->writeAccess->getTime();
// 	    for(RaceByEp::iterator secondEpochIt=forwRaceAddrInfo->raceByEp.begin();
// 		secondEpochIt!=forwRaceAddrInfo->raceByEp.end();secondEpochIt++){
// 	      RaceInfo *raceInfo=secondEpochIt->second;
// 	      // First access is a write, must be flow- or output-type race
// 	      if(!(raceInfo->raceType&(RaceInfo::Flow|RaceInfo::Output)))
// 		continue;
// 	      Epoch *secondEpoch=secondEpochIt->first;
// 	      RaceByAddrEp::iterator
// 		backRaceAddrIt=secondEpoch->myBackRacesByAddrEp.find(dataAddress);
// 	      I(backRaceAddrIt!=secondEpoch->myBackRacesByAddrEp.end());
// 	      RaceAddrInfo *backRaceAddrInfo=backRaceAddrIt->second;
// 	      I(backRaceAddrInfo);
// 	      Time_t secondTime=globalClock;
// 	      Time_t diffTime=globalClock;
// 	      if(raceInfo->raceType&RaceInfo::Flow){
// 		I(backRaceAddrInfo->readAccess);
// 		secondTime=backRaceAddrInfo->readAccess->getTime();
// 	      }
// 	      if(raceInfo->raceType&RaceInfo::Output){
// 		I(backRaceAddrInfo->writeAccess);
// 		Time_t secondTimeTmp=backRaceAddrInfo->writeAccess->getTime();
// 		if(secondTimeTmp<secondTime)
// 		  secondTime=secondTimeTmp;
// 	      }
// 	      diffTime=(secondTime>firstTime)?(secondTime-firstTime):0;
// 	      if(diffTime<shortRaceDist){
// 		shortRaceDist=diffTime;
// 		shortRaceFirstTime=firstTime;
// 		shortRaceSecondTime=secondTime;
// 		shortRaceFirstEpoch=firstEpoch;
// 		shortRaceSecondEpoch=secondEpoch;
// 	      }
// 	    }
// 	  }
// 	}
//       }
//       I(shortRaceFirstEpoch);
//       I(shortRaceSecondEpoch);
//       // Get to the first epoch in the analyzingToMerge list
//       EpochList::reverse_iterator epochRIt=analyzingBeginRIt;
//       while(*epochRIt!=shortRaceFirstEpoch){
// 	epochRIt++;
// 	I(epochRIt!=analyzingEndRIt);
//       }
//       I(*epochRIt==shortRaceFirstEpoch);
//       // Stats initially have only the second epoch
//       size_t epochCount=1;
//       InstrCount instrCount=(InstrCount)(shortRaceSecondEpoch->currInstrCount);
//       size_t blockCount=shortRaceSecondEpoch->bufferBlocks.size();
// #if (defined CollectVersionCountStats)
//       VersionCounts versionCounts;
//       versionCounts.add(shortRaceSecondEpoch);
// #endif // CollectVersionCountStats
//       // Traverse from first to second epoch and accumulate stats
//       while(*epochRIt!=shortRaceSecondEpoch){
// 	Epoch *epoch=*epochRIt;
// 	if(epoch->endDecisionExe<shortRaceSecondTime){
// 	  epochCount++;
// 	  instrCount+=(InstrCount)(epoch->currInstrCount);
// 	  blockCount+=epoch->bufferBlocks.size();
// #if (defined CollectVersionCountStats)
// 	  versionCounts.add(epoch);
// #endif // CollectVersionCountStats
// 	}
// 	epochRIt++;
// 	I(epochRIt!=analyzingEndRIt);
//       }
//       I(*epochRIt==shortRaceSecondEpoch);
//       // Update the stat observers
//       RaceGrpShortEpochObserver.addSample(epochCount);
//       RaceGrpShortInstObserver.addSample(instrCount);
//       RaceGrpShortBuffObserver.addSample(blockCount*blockSize);
// #if (defined CollectVersionCountStats)
//       RaceGrpShortVersObserver.addSample(versionCounts.getMaxNotRdOnly());
// #endif // CollectVersionCountStats
//     }
//     // Clear the traces of all analyzed epochs
//     for(EpochSet::iterator clearIt=analysisDone.begin();
// 	clearIt!=analysisDone.end();clearIt++){
//       Epoch *clearEpoch=*clearIt;
//       clearEpoch->clearTrace();
//     }
//     // Clear the analysis done set to prepare for the next analysis
//     analysisDone.clear();
//     // Clear the analysis needed set to end the current analysis
//     analysisNeeded.clear();
//     // End the analysis phase
//     doingAnalysis=false;
//     // Restart epochs we have stopped for analysis
//     while(!stoppedForAnalysis.empty()){
//       Epoch *epoch=stoppedForAnalysis.back();
//       stoppedForAnalysis.pop_back();
//       I(!epoch->currInstrCount);
//       I(epoch->myState*Execution==None);
//       I(epoch->isExecutable());
//       epoch->becomeExecuting();
//     }
//     // Analysis is over, try merging
//     I(lastAnalyzing!=lastMerging);
//     EpochList::reverse_iterator nextMergeIt(lastMerging);
//     I(nextMergeIt!=allEpochs.rend());
//     (*nextMergeIt)->tryToMerge();
//     // Continue to propagate the Analyzing status
//     // (the propagation was stopped for analysis)
//     if(lastAnalyzing!=lastSafe){
//       EpochList::iterator nextIt=lastAnalyzing;
//       I(nextIt!=allEpochs.begin());
//       nextIt--;
//       (*nextIt)->tryToAnalyze(false);
//     }
//   }

//   bool Epoch::isMergeable(void) const{
//     if(lastMerging==lastAnalyzing)
//       return false;
//     EpochList::reverse_iterator nextIt(lastMerging);
//     I(nextIt!=allEpochs.rend());
//     if(*nextIt!=this)
//       return false;
//     // If analysis still needed, can not merge
//     if(analysisNeeded.count(const_cast<Epoch *>(this)))
//       return false;
//     return true;
//   }

//   void Epoch::tryToMerge(){
//     I(myState<Merging);
//     if(!isMergeable())
//       return;
//     // Schedule the merging
//     mergeEpochs.insert(this);
//     hasPostExecActions=true;
//   }

//   void Epoch::becomeMerging(void){
//     I(!numAnomalies);
//     I(myState>=Decidable);
//     I(myState>=Decided);
//     I(myState<Merging);
//     // Update per-epoch statistics
//     SysCallCount.addSample(sysCallLog.size());
//     // Analyzing epochs that are dependent on the last-merging epoch
//     EpochSet dependEpochs;
//     {
//       EpochSet dependEpochs;
//       size_t numIndepSuccs=0;
//       ClockValue distIndepSucc=0;
//       for(EpochList::reverse_iterator epochRIt=EpochList::reverse_iterator(lastMerging);
// 	  epochRIt!=EpochList::reverse_iterator(lastAnalyzing);epochRIt++){
// 	Epoch *epoch=*epochRIt;
// 	if(!dependEpochs.count(epoch)){
// 	  numIndepSuccs++;
// 	  distIndepSucc=epoch->mySClock-mySClock+1;
// 	}
// 	for(EpochOrder::const_iterator succIt=epoch->succEpochs.begin();
// 	  succIt!=epoch->succEpochs.end();succIt++){
// 	  if(succIt->second.hasAnyOf(OrderEntry::InduceSq))
// 	    dependEpochs.insert(succIt->first);
// 	}
//       }
//       AccuNumIndepEpochs.addSample(numIndepSuccs);
//       AccuFarIndepEpoch.addSample(distIndepSucc);
//     }
//     {
//       Thread::ThreadIndex numIndices=Thread::getIndexUb();
//       ClockValue threadLimits[numIndices];
//       bool limitReached[numIndices];
//       size_t numIndepSuccs=0;
//       ClockValue distIndepSucc=0;
//       for(Thread::ThreadIndex ti=0;ti<numIndices;ti++){
// 	threadLimits[ti]=0;
// 	limitReached[ti]=false;
//       }
//       for(EpochList::reverse_iterator epochRIt=EpochList::reverse_iterator(lastMerging);
// 	  epochRIt!=EpochList::reverse_iterator(lastAnalyzing);epochRIt++){
// 	Epoch *epoch=*epochRIt;
// 	Thread::ThreadIndex epochIndex=epoch->myThread->getIndex();
// 	for(Thread::ThreadIndex ti=0;ti<numIndices;ti++){
// 	  if(epochIndex==ti){
// 	    if(epoch->sameThreadFlowSucc){
// 	      if(threadLimits[ti]){
// 		threadLimits[ti]=min(threadLimits[ti],epoch->sameThreadFlowSucc);
// 	      }else{
// 		threadLimits[ti]=epoch->sameThreadFlowSucc;
// 	      }
// 	    }
// 	  }else{
// 	    if(epoch->diffThreadFlowSucc){
// 	      if(threadLimits[ti]){
// 		threadLimits[ti]=min(threadLimits[ti],epoch->diffThreadFlowSucc);
// 	      }else{
// 		threadLimits[ti]=epoch->diffThreadFlowSucc;
// 	      }
// 	    }
// 	  }
// 	  limitReached[ti]=threadLimits[ti]&&(threadLimits[ti]<=epoch->mySClock);
// 	}
// 	if(!limitReached[epochIndex]){
// 	  I(!dependEpochs.count(epoch));
// 	  numIndepSuccs++;
// 	  distIndepSucc=epoch->mySClock-mySClock+1;
// 	}
//       }
//       TwoLimNumIndepEpochs.addSample(numIndepSuccs);
//       TwoLimFarIndepEpoch.addSample(distIndepSucc);      
//     }
//     {
//       Thread::ThreadIndex numIndices=Thread::getIndexUb();
//       ClockValue threadLimits[numIndices];
//       size_t numIndepSuccs=0;
//       ClockValue distIndepSucc=0;
//       for(Thread::ThreadIndex ti=0;ti<numIndices;ti++)
// 	threadLimits[ti]=0;
//       for(EpochList::reverse_iterator epochRIt=EpochList::reverse_iterator(lastMerging);
// 	  epochRIt!=EpochList::reverse_iterator(lastAnalyzing);epochRIt++){
// 	Epoch *epoch=*epochRIt;
// 	Thread::ThreadIndex epochIndex=epoch->myThread->getIndex();
// 	if((!threadLimits[epochIndex])||(threadLimits[epochIndex]>epoch->mySClock)){
// 	  I(!dependEpochs.count(epoch));
// 	  numIndepSuccs++;
// 	  distIndepSucc=epoch->mySClock-mySClock+1;
// 	}
// 	for(Thread::ThreadIndex ti=0;ti<numIndices;ti++){
// 	  if(epochIndex==ti){
// 	    if(epoch->sameThreadFlowSucc){
// 	      if(threadLimits[ti]){
// 		threadLimits[ti]=min(threadLimits[ti],epoch->sameThreadFlowSucc);
// 	      }else{
// 		threadLimits[ti]=epoch->sameThreadFlowSucc;
// 	      }
// 	    }
// 	  }else{
// 	    if(epoch->diffThreadFlowSucc){
// 	      if(threadLimits[ti]){
// 		threadLimits[ti]=min(threadLimits[ti],epoch->diffThreadFlowSucc);
// 	      }else{
// 		threadLimits[ti]=epoch->diffThreadFlowSucc;
// 	      }
// 	    }
// 	  }
// 	}
//       }
//       OneLimNumIndepEpochs.addSample(numIndepSuccs);
//       OneLimFarIndepEpoch.addSample(distIndepSucc);      
//     }
//     EpochInstObserver.addSample(currInstrCount);
//     EpochBuffObserver.addSample(bufferBlocks.size()*blockSize);
//     I(createdNotSafeCount+safeNotAnalyzingCount+
//       analyzingNotMergingCount+mergingNotDestroyedCount==
//       allEpochs.size());
//     size_t detWinEpochNum=
//       createdNotSafeCount+safeNotAnalyzingCount+analyzingNotMergingCount;
//     DetWinEpochObserver.addSample(detWinEpochNum);
//     InsDetWinEpochObserver.addSamples(detWinEpochNum,currInstrCount);
//     InstrCount detWinInstNum=safeNotAnalyzingInstr+analyzingNotMergingInstr;
//     for(EpochList::iterator epochIt=allEpochs.begin();
// 	epochIt!=lastSafe;epochIt++){
//       Epoch *epoch=*epochIt;
//       I(epoch->myState<Merging);
//       I(epoch->myState<Safe);
//       I(epoch->myState<Analyzing);
//       detWinInstNum+=epoch->currInstrCount;
//     }
//     DetWinInstObserver.addSample(detWinInstNum);
//     InsDetWinInstObserver.addSamples(detWinInstNum,currInstrCount);
//     DetWinBuffObserver.addSample(BufferBlock::getBlockCount()*blockSize);
//     InsDetWinBuffObserver.addSamples(BufferBlock::getBlockCount()*blockSize,currInstrCount);
// #if (defined CollectVersionCountStats)
//     DetWinVersObserver.addSample(detWinVersCounts.getMaxNotRdOnly());
//     InsDetWinVersObserver.addSamples(detWinVersCounts.getMaxNotRdOnly(),currInstrCount);
// #endif // CollectVersionCountStats
//     // Now this epoch is Merging
//     myState+=Merging;
//     ID(EpochList::reverse_iterator nextIt(lastMerging));
//     I(nextIt!=allEpochs.rend());
//     I(*nextIt==this);
//     lastMerging=myAllEpochsPos;
//     // Remove this epoch from the merge set
//     I(mergeEpochs.count(this));
//     mergeEpochs.erase(this);
//     // Remove the race info about this epoch
//     {
//       // Back races should have been cleaned when
//       // the forward races of the first epoch in each race were cleaned
//       I(myBackRacesByEpAddr.empty());
//       I(myBackRacesByAddrEp.empty());
//       // Now remove info about my forward races
//       // Iterate through all epochs we had forward races with
//       for(RaceByEpAddr::iterator epochIt=myForwRacesByEpAddr.begin();
// 	  epochIt!=myForwRacesByEpAddr.end();epochIt++){
// 	Epoch *epoch=epochIt->first;
// 	RaceByAddr &raceByAddr=epochIt->second;
// 	// Iterate through all addresses on which we had data races with that epoch
// 	for(RaceByAddr::iterator addrIt=raceByAddr.begin();
// 	    addrIt!=raceByAddr.end();addrIt++){
// 	  Address addr=addrIt->first;
// 	  RaceInfo *raceInfo=addrIt->second;
// 	  // Erase raceInfo from other epoch's myBackRacesByAddrEp map
// 	  RaceByAddrEp::iterator backAddrEpIt=epoch->myBackRacesByAddrEp.find(addr);
// 	  I(backAddrEpIt->second->raceByEp.count(this));
// 	  I(backAddrEpIt->second->raceByEp[this]==raceInfo);
// 	  backAddrEpIt->second->raceByEp.erase(this);
// 	  if(backAddrEpIt->second->raceByEp.empty()){
// 	    delete backAddrEpIt->second;
// 	    epoch->myBackRacesByAddrEp.erase(backAddrEpIt);
// 	  }
// 	  // Entries should also exist in these two maps (cleared soon afterwards)
// 	  I(epoch->myBackRacesByEpAddr[this][addr]==raceInfo);
// 	  I(myForwRacesByAddrEp[addr]->raceByEp[epoch]==raceInfo);
// 	  // Get rid of raceInfo
// 	  delete raceInfo;
// 	}
// 	// Erase all entries for this epoch in the other's myBackRacesByEpAddr map
// 	epoch->myBackRacesByEpAddr.erase(this);
//       }
//       // Delete all forward RaceAddrInfo entries
//       for(RaceByAddrEp::iterator forwAddrEpIt=myForwRacesByAddrEp.begin();
// 	  forwAddrEpIt!=myForwRacesByAddrEp.end();forwAddrEpIt++){
// 	delete forwAddrEpIt->second;
//       }
//       // Clear the forward race maps
//       myForwRacesByAddrEp.clear();
//       myForwRacesByEpAddr.clear();
//     }
//     I(myTrace.empty());
//     //    clearTrace();
//     if(myState>=Committable){
//       becomeCommitted();
//     }
//     I(analyzingNotMergingCount);
//     I((lastMerging!=allEpochs.end())&&(*lastMerging==this));
//     analyzingNotMergingCount--;
//     analyzingNotMergingInstr-=(InstrCount)currInstrCount;
//     analyzingNotMergingBufferBlocks-=bufferBlocks.size();
//     detWinVersCounts.remove(this);
//     // But waiting to be destroyed
//     mergingNotDestroyedCount++;

//     // ToDo: let the backend know it can now merge the state of this epoch
//     // For now, we just do the merge here and then try to destroy the epoch
//     mergeBuffer();
//     eraseBuffer();
//     tryToDestroy();

//     if(lastAnalyzing!=lastMerging){
//       EpochList::reverse_iterator nextMergeIt(lastMerging);
//       (*nextMergeIt)->tryToMerge();
//     }
//   }
  
//   void Epoch::becomeCommitted(void){
//     I(myState>=Committable);
//     if(activeMerging==this){
//       activeMerging=0;
//       // ToDo: Allow others to become Safe and/or Merging
//       // For now, just assume this is not needed
//       I(activeWaitMerge.empty());
//       I(endedWaitMerge.empty());
//       I(endedWaitSafe.empty());
//     }
//     // If all the buffer blocks already merged, try to destroy this epoch
//     if(!bufferBlocks.empty())
//       tryToDestroy();
//   }  

//   void Epoch::mergeActively(void){
//     // ToDo: Start actively merging this epoch with architectural state
//   }
  
//   void Epoch::tryToDestroy(void){
//     // Must be committed
//     if(myState>=Committable)
//       return;
//     // Must have no more buffer blocks unmerged
//     if(!bufferBlocks.empty())
//       return;
//     // Can not destroy an epoch that still has non-destroyed predecessors
//     if(!predEpochs.empty())
//       return;
//     EpochList::iterator nextPos=myAllEpochsPos;
//     nextPos++;
//     if(nextPos!=allEpochs.end())
//       return;
//     destroyEpochs.insert(this);
//     hasPostExecActions=true;
//   }
  
//   void Epoch::becomeDestroyed(Epoch *destroyEpoch){
//     // Remove this epoch from destroyEpochs
//     I(destroyEpochs.count(destroyEpoch));
//     destroyEpochs.erase(destroyEpoch);
//     EpochID destroyEid=destroyEpoch->myEid;
//     // Get the SESC context for this epoch
//     ThreadContext *epochContext=
//       osSim->getContext(static_cast<Pid_t>(destroyEid));
//     // Let the synchronization know that the epoch is gone 
//     if(destroyEpoch->releasedSync)
//       destroyEpoch->releasedSync->relEpochDestroyed(destroyEpoch);
//     // Remove this epoch from all ordering
//     I(destroyEpoch->predEpochs.empty());
//     for(EpochOrder::iterator succIt=destroyEpoch->succEpochs.begin();
// 	succIt!=destroyEpoch->succEpochs.end();succIt++){
//       Epoch *succEpoch=succIt->first;
//       succEpoch->predEpochs.erase(destroyEpoch);
//     }
//     // Is this the last epoch in its thread
//     if(!destroyEpoch->immedSeqSucc){
//       Thread *destroyThread=destroyEpoch->myThread;
//       ThreadID destroyTID=destroyThread->getID();
//       ThreadContext *originalContext=
// 	osSim->getContext(static_cast<Pid_t>(destroyTID));
//       // Update thread context with stuff from the last epoch's context
//       mint_sesc_update_original(epochContext,originalContext);
//       // Resume the thread context
//       osSim->unstop(static_cast<Pid_t>(destroyTID));
//       delete destroyThread;
//     }
//     // Remove the epoch from the map of valid epochs
//     I(destroyEpoch->myState>=Committable);
//     I(eidToEpoch[destroyEid]==destroyEpoch);
//     eidToEpoch[destroyEid]=0;
//     mergingNotDestroyedCount--;
//     I(*(destroyEpoch->myAllEpochsPos)==destroyEpoch);
//     ID(EpochList::iterator testPos=destroyEpoch->myAllEpochsPos);
//     ID(testPos++);
// #if (defined DEBUG)
//     if(testPos!=allEpochs.end()){
//       for(EpochList::iterator testIt=allEpochs.begin();
// 	  testIt!=allEpochs.end();testIt++){
// 	if(testIt==lastSafe)
// 	  printf("Safe: ");
// 	printf("%d(%d)(%d) ",(*testIt)->getTid(),(*testIt)->myEid,(*testIt)->mySClock);
//       }
//       printf("End\n");
//     }
//     I(testPos==allEpochs.end());
// #endif
//     if(lastMerging==destroyEpoch->myAllEpochsPos){
//       lastMerging=allEpochs.end();
//       if(lastAnalyzing==destroyEpoch->myAllEpochsPos){
// 	lastAnalyzing=allEpochs.end();
// 	if(lastSafe==destroyEpoch->myAllEpochsPos)
// 	  lastSafe=allEpochs.end();
//       }else{
// 	I(lastSafe!=destroyEpoch->myAllEpochsPos);
//       }
//     }else{
//       I(lastAnalyzing!=destroyEpoch->myAllEpochsPos);
//       I(lastSafe!=destroyEpoch->myAllEpochsPos);
//     }
//     allEpochs.erase(destroyEpoch->myAllEpochsPos);
//     // Destroy the SESC context and delete TLS info for this epoch
//     mint_sesc_die(epochContext);
//     delete destroyEpoch;
//     // Try to destroy the next epoch
//     if(lastAnalyzing!=allEpochs.end()){
//       EpochList::reverse_iterator nextIt(allEpochs.end());
//       (*nextIt)->tryToDestroy();
//     }
//   }
  
  Epoch::PidToEpoch Epoch::pidToEpoch;
  EpochList Epoch::allEpochs;

//   EpochList::iterator Epoch::lastAnalyzing(allEpochs.end());
//   EpochList::iterator Epoch::lastMerging(allEpochs.end());

  bool Epoch::globalSafeAvailable=true;
  EpochList::iterator Epoch::globalSafe(allEpochs.end());

  Epoch::BlockVersions::BlockAddrToVersions Epoch::BlockVersions::blockAddrToVersions;

  void Epoch::BlockVersions::check(Address baseAddr){
    BlockAddrToVersions::iterator addrIt=blockAddrToVersions.find(baseAddr);
    I(addrIt!=blockAddrToVersions.end());
    I(addrIt->first==baseAddr);
    I(addrIt->second==this);
    BufferBlockList::iterator acIt=accessors.begin();
    BufferBlockList::iterator wrIt=writers.begin();
    ClockValue lastClock=infiniteClockValue;
    while(acIt!=accessors.end()){
      I((*acIt)->isAccessed());
      I((*acIt)->epoch->getClock()<=lastClock);
      I((*acIt)->myVersions==this);
      I((*acIt)->accessorsPos==acIt);
      I((*acIt)->writersPos==wrIt);
      I((*acIt)->baseAddr==baseAddr);
      I((*acIt)->epoch->bufferBlocks.find(baseAddr)!=(*acIt)->epoch->bufferBlocks.end());
      I((*acIt)->epoch->bufferBlocks.find(baseAddr)->second==(*acIt));
      lastClock=(*acIt)->epoch->getClock();
      I((*acIt)->isWritten()==((wrIt!=writers.end())&&(*wrIt==*acIt)));
      if((*acIt)->isWritten())
	wrIt++;
      acIt++;
    }
    I(wrIt==writers.end());
    I(acIt==accessors.end());
  }
  
  void Epoch::BlockVersions::checkAll(void){
    for(BlockAddrToVersions::iterator addrIt=blockAddrToVersions.begin();
        addrIt!=blockAddrToVersions.end();addrIt++){
      Address baseAddr=addrIt->first;
      BlockVersions *blockVers=addrIt->second;
      blockVers->check(baseAddr);
    }
  }

  Epoch::BlockVersions::BlockPosition
  Epoch::BlockVersions::findBlockPosition(const BufferBlock *block){
    I(block->myVersions==this);
    ID(check(block->baseAddr));
    BlockPosition retVal=block->accessorsPos;
    // No need to search if block already correctly positioned
    if(retVal==accessors.end()){
      // Search starts from most recent writers
      retVal=accessors.begin();      
      // Use writer positions to jump forward while we can
      for(BufferBlockList::iterator wrPos=writers.begin();
	  wrPos!=writers.end();wrPos++){
	if((*wrPos)->epoch->mySClock<=block->epoch->mySClock)
	  break;
	retVal=(*wrPos)->accessorsPos;
      }
      // No more writer positions can be used, continue
      // search using accessors list only
      while(retVal!=accessors.end()){
	if((*retVal)->epoch->mySClock<=block->epoch->mySClock)
	  break;
	retVal++;
      }
    }
    return retVal;
  }
//   Epoch::BlockVersions::BlockPositionInfo Epoch::BlockVersions::findBlockPosition(const BufferBlock *block){
//     I(block->myVersions==this);
//     BufferBlockList::iterator wrPos=block->writersPos;
//     BufferBlockList::iterator acPos=block->accessorsPos;
//     if(wrPos==writers.end()){
//       // Find the first writer whose clock is is not strictly after ours
//       for(wrPos=writers.begin();wrPos!=writers.end();wrPos++){
//         I((*wrPos)->myVersions==this);
// 	if((*wrPos)->epoch->mySClock<=block->epoch->mySClock){
// 	  // We found it
// 	  if(((*wrPos)->epoch->mySClock==block->epoch->mySClock)&&
// 	     (acPos!=accessors.end())){
// 	    // The writer we found has SClock equal to ours and we are already in the accessors list
// 	    // Our position in writer's list is before the first writer that is after us in the accessors list
// 	    for(BufferBlockList::iterator wrAcPos=acPos;wrAcPos!=accessors.end();wrAcPos++){
// 	      // Our writers position is before this accessor's accessors position
// 	      wrPos=(*wrAcPos)->writersPos;
// 	      // If this accessor is also a writer, we are done
// 	      if(wrPos!=writers.end())
// 		break;
// 	    }
// 	    // If no writer was found until the end of the list, wrPos is writers.end(), which is good
// 	  }
// 	  break;
// 	}
//       }
//     }
//     I((wrPos==writers.end())||((*wrPos)->myVersions==this));
//     if(acPos==accessors.end()){
//       // Now try to use position in writers list as a hint for the accessors list
//       if(wrPos!=writers.end()){
// 	BufferBlockList::reverse_iterator acPosR((*wrPos)->accessorsPos);
// 	for(;acPosR!=accessors.rend();acPosR++){
// 	  I((*acPosR)->myVersions==this);
// 	  I((*acPosR)->baseAddr==block->baseAddr);
// 	  if((*acPosR)->epoch->mySClock>block->epoch->mySClock)
// 	    break;
// 	}
// 	acPos=acPosR.base();
//       }else{
// 	// Can not use writers position as a hint, must search
// 	// We start the search from most recent accessors
// 	for(acPos=accessors.begin();acPos!=accessors.end();acPos++){
// 	  if((*acPos)->epoch->mySClock<=block->epoch->mySClock)
// 	    break;
// 	}
//       }
//     }
//     return BlockPositionInfo(acPos,wrPos);
//   }
  
  Epoch::ChunkBitMask
  Epoch::BlockVersions::findReadConflicts(const BufferBlock *currBlock, size_t chunkIndx,
					  BlockPosition blockPos,
					  ChunkBitMask beforeMask, ChunkBitMask afterMask,
					  ConflictList &writesBefore, ConflictList &writesAfter){
    BufferBlockList::iterator wrPos=
      (blockPos==accessors.end())?writers.end():(*blockPos)->writersPos;
    if(afterMask){
      BufferBlockList::iterator afterIt=wrPos;
      while(afterIt!=writers.begin()){
	afterIt--;
	BufferBlock *confBlock=*afterIt;
	ChunkBitMask confMask=confBlock->wrMask[chunkIndx]&afterMask;
	if(confMask){
	  writesAfter.push_front(ConflictInfo(confBlock,confMask));
	  afterMask^=confMask;
	  if(!afterMask)
	    break;
	}
      }
    }
    if(beforeMask){
      BufferBlockList::iterator beforeIt=wrPos;
      if((beforeIt!=writers.end())&&(*beforeIt==currBlock))
	beforeIt++;
      while(beforeIt!=writers.end()){
	BufferBlock *confBlock=*beforeIt;
	ChunkBitMask confMask=confBlock->wrMask[chunkIndx]&beforeMask;
	if(confMask){
	  writesBefore.push_front(ConflictInfo(confBlock,confMask));
	  beforeMask^=confMask;
	  if(!beforeMask)
	    break;
	}
	beforeIt++;
      }
    }
    return beforeMask;
  }

//   Epoch::ChunkBitMask
//   Epoch::BlockVersions::findReadConflicts(const BufferBlock *currBlock, size_t chunkIndx,
// 					  BlockPositionInfo &posInfo,
// 					  ChunkBitMask beforeMask, ChunkBitMask afterMask,
// 					  ConflictList &writesBefore, ConflictList &writesAfter){
//     if(afterMask){
//       BufferBlockList::iterator afterIt=posInfo.writersPos;
//       while(afterIt!=writers.begin()){
// 	afterIt--;
// 	BufferBlock *confBlock=*afterIt;
// 	ChunkBitMask confMask=confBlock->wrMask[chunkIndx]&afterMask;
// 	if(confMask){
// 	  writesAfter.push_front(ConflictInfo(confBlock,confMask));
// 	  afterMask^=confMask;
// 	  if(!afterMask)
// 	    break;
// 	}
//       }
//     }
//     if(beforeMask){
//       BufferBlockList::iterator beforeIt=posInfo.writersPos;
//       if(currBlock->isProducer&&beforeIt==currBlock->writersPos)
// 	beforeIt++;
//       while(beforeIt!=writers.end()){
// 	BufferBlock *confBlock=*beforeIt;
// 	ChunkBitMask confMask=confBlock->wrMask[chunkIndx]&beforeMask;
// 	if(confMask){
// 	  writesBefore.push_front(ConflictInfo(confBlock,confMask));
// 	  beforeMask^=confMask;
// 	  if(!beforeMask)
// 	    break;
// 	}
// 	beforeIt++;
//       }
//     }
//     return beforeMask;
//   }

  void Epoch::BlockVersions::findWriteConflicts(const BufferBlock *currBlock, size_t chunkIndx,
						BlockPosition blockPos,
						ChunkBitMask beforeMask, ChunkBitMask afterMask,
						ConflictList &readsBefore, ConflictList &writesBefore,
						ConflictList &writesAfter, ConflictList &readsAfter){
    if(afterMask){
      BufferBlockList::iterator afterIt=blockPos;
      I(blockPos!=accessors.end());
      while(afterIt!=accessors.begin()){
	afterIt--;
	BufferBlock *confBlock=*afterIt;
	ChunkBitMask rdConfMask=confBlock->xpMask[chunkIndx]&afterMask;
	if(rdConfMask){
	  readsAfter.push_front(ConflictInfo(confBlock,rdConfMask));
	}
	ChunkBitMask wrConfMask=confBlock->wrMask[chunkIndx]&afterMask;
	if(wrConfMask){
	  writesAfter.push_front(ConflictInfo(confBlock,wrConfMask));
	  afterMask^=wrConfMask;
	  if(!afterMask)
	    break;
	}
      }
    }
    if(beforeMask){
      BufferBlockList::iterator beforeIt=blockPos;
      I((blockPos!=accessors.end())&&(*blockPos==currBlock));
      beforeIt++;
      while(beforeIt!=accessors.end()){
	BufferBlock *confBlock=*beforeIt;
	ChunkBitMask wrConfMask=confBlock->wrMask[chunkIndx]&beforeMask;
	if(wrConfMask){
	  writesBefore.push_front(ConflictInfo(confBlock,wrConfMask));
	  beforeMask^=wrConfMask;
	  if(!beforeMask)
	    break;
	}
	ChunkBitMask rdConfMask=confBlock->xpMask[chunkIndx]&beforeMask;
	if(rdConfMask){
	  readsBefore.push_front(ConflictInfo(confBlock,rdConfMask));
	}
	beforeIt++;
      }
    }
  }
//   void Epoch::BlockVersions::findWriteConflicts(const BufferBlock *currBlock, size_t chunkIndx,
// 						BlockPositionInfo &posInfo,
// 						ChunkBitMask beforeMask, ChunkBitMask afterMask,
// 						ConflictList &readsBefore, ConflictList &writesBefore,
// 						ConflictList &writesAfter, ConflictList &readsAfter){
//     if(afterMask){
//       BufferBlockList::iterator afterIt=posInfo.accessorsPos;
//       while(afterIt!=accessors.begin()){
// 	afterIt--;
// 	BufferBlock *confBlock=*afterIt;
// 	ChunkBitMask rdConfMask=confBlock->xpMask[chunkIndx]&afterMask;
// 	if(rdConfMask){
// 	  readsAfter.push_front(ConflictInfo(confBlock,rdConfMask));
// 	}
// 	ChunkBitMask wrConfMask=confBlock->wrMask[chunkIndx]&afterMask;
// 	if(wrConfMask){
// 	  writesAfter.push_front(ConflictInfo(confBlock,wrConfMask));
// 	  afterMask^=wrConfMask;
// 	  if(!afterMask)
// 	    break;
// 	}
//       }
//     }
//     if(beforeMask){
//       BufferBlockList::iterator beforeIt=posInfo.accessorsPos;
//       beforeIt++;
//       while(beforeIt!=accessors.end()){
// 	BufferBlock *confBlock=*beforeIt;
// 	ChunkBitMask wrConfMask=confBlock->wrMask[chunkIndx]&beforeMask;
// 	if(wrConfMask){
// 	  writesBefore.push_front(ConflictInfo(confBlock,wrConfMask));
// 	  beforeMask^=wrConfMask;
// 	  if(!beforeMask)
// 	    break;
// 	}
// 	ChunkBitMask rdConfMask=confBlock->xpMask[chunkIndx]&beforeMask;
// 	if(rdConfMask){
// 	  readsBefore.push_front(ConflictInfo(confBlock,rdConfMask));
// 	}
// 	beforeIt++;
//       }
//     }
//   }
  
  void Epoch::BlockVersions::advance(BufferBlock *block){
    I(block->isAccessed()==(block->accessorsPos!=accessors.end()));
    if(block->isAccessed()){
      BufferBlockList::iterator currAcPos=block->accessorsPos;
      BufferBlockList::iterator currWrPos=block->writersPos;
      BufferBlockList::iterator newAcPos=block->accessorsPos;
      BufferBlockList::iterator newWrPos=block->writersPos;
      // This is a write block. Because read blocks point to the
      // first predecessor writer block, when we advance this
      // block we must adjust the write position of read blocks
      // we pass. We do this until we either reach our new position
      // or we pass a write block. Once we pass a write block we
      // continue the advance (if needed) just like a read block
      if(block->isWritten()){
	BufferBlockList::iterator predWrPos=currWrPos; predWrPos++;
	while(newAcPos!=accessors.begin()){
	  newAcPos--;
	  if(((*newAcPos)->epoch->mySClock>=block->epoch->mySClock)||
	     ((*newAcPos)->writersPos!=currWrPos)){
	    newAcPos++;
	    break;
	  }
	  (*newAcPos)->writersPos=predWrPos;
	}
      }
      // Advance faster using the writers list until we hit the
      // write that will become our new immediate successor
      while(newWrPos!=writers.begin()){
	newWrPos--;
	if((*newWrPos)->epoch->mySClock>=block->epoch->mySClock){
	  newWrPos++;
	  break;
	}
	newAcPos=(*newWrPos)->accessorsPos;
      }
      // Now advance the rest of the way using the accessors list
      while(newAcPos!=accessors.begin()){
	newAcPos--;
	if((*newAcPos)->epoch->mySClock>=block->epoch->mySClock){
	  newAcPos++;
	  break;
	}
      }
      // Insert the block into its new accessors position
      accessors.splice(newAcPos,accessors,currAcPos);
      I(*currAcPos==block);
      // If write block, insert into its new writers position and
      // adjust the write pointers of successor read blocks
      // If not a write block, adjust its own write pointer
      if(block->isWritten()){
	writers.splice(newWrPos,writers,currWrPos);
	I(*currWrPos==block);
	BufferBlockList::iterator succAcPos=currAcPos;
	while(succAcPos!=accessors.begin()){
	  succAcPos--;
	  if((*succAcPos)->writersPos!=newWrPos)
	    break;
	  (*succAcPos)->writersPos=currWrPos;
	}
      }else{
	block->writersPos=newWrPos;
      }
    }
    ID(check(block->baseAddr));
  }

  void Epoch::BlockVersions::access(bool isWrite, BufferBlock *currBlock, BlockPosition &blockPos){
    I(currBlock->myVersions==this);
    ID(check(currBlock->baseAddr));
    if(!currBlock->isAccessed()){
      I(currBlock->accessorsPos==accessors.end());
      I(currBlock->writersPos==writers.end());
      accessorsCount++;
      if(blockPos==accessors.end()){
	currBlock->writersPos=writers.end();
      }else{
	currBlock->writersPos=(*blockPos)->writersPos;
      }
      blockPos=currBlock->accessorsPos=accessors.insert(blockPos,currBlock);
    }else{
      I(blockPos==currBlock->accessorsPos);
    }
    if(isWrite){
      if(!currBlock->isWritten()){
	// Mark the block as written
	currBlock->becomeProducer();
	currBlock->writersPos=writers.insert(currBlock->writersPos,currBlock);
	BufferBlockList::iterator lastSuccAcPos;
	if(currBlock->writersPos==writers.begin()){
	  lastSuccAcPos=accessors.begin();
	}else{
	  lastSuccAcPos=currBlock->writersPos; lastSuccAcPos--;
	  lastSuccAcPos=(*lastSuccAcPos)->accessorsPos; lastSuccAcPos++;
	}
	for(BufferBlockList::iterator succAcPos=lastSuccAcPos;
	    succAcPos!=currBlock->accessorsPos;succAcPos++)
	  (*succAcPos)->writersPos=currBlock->writersPos;
      }
    }else{
      // Mark the block as exposed-read
      currBlock->becomeConsumer();
    }
    ID(check(currBlock->baseAddr));
  }
  
  void Epoch::BlockVersions::remove(BufferBlock *block){
    I(block->myVersions==this);
    ID(check(block->baseAddr));
    if(block->isWritten()){
      I(*(block->writersPos)==block);
      if(block->accessorsPos!=accessors.begin()){
	BufferBlockList::iterator lastSuccAcPos;
	if(block->writersPos==writers.begin()){
	  lastSuccAcPos=accessors.begin();
	}else{
	  lastSuccAcPos=block->writersPos; lastSuccAcPos--;
	  lastSuccAcPos=(*lastSuccAcPos)->accessorsPos; lastSuccAcPos++;
	}
	BufferBlockList::iterator predWrPos=block->writersPos; predWrPos++;
	for(BufferBlockList::iterator succAcPos=lastSuccAcPos;
	    succAcPos!=block->accessorsPos;succAcPos++)
	  (*succAcPos)->writersPos=predWrPos;
      }
      writers.erase(block->writersPos);
      ID(block->writersPos=writers.end());
    }
    if(block->isAccessed()){
      I(block->accessorsPos!=accessors.end());
      I(*(block->accessorsPos)==block);
      accessors.erase(block->accessorsPos);
      accessorsCount--;
      ID(block->accessorsPos=accessors.end());
    }
    ID(check(block->baseAddr));
  }
  
  size_t Epoch::BufferBlock::blockCount=0;

#if ((E_RIGHT != 0x8) || (E_LEFT !=0x4) || (E_SIZE != 0x3))
#error "OpFlags does not have the proper structure!"
#endif

  // accessBitMask[OpFlags][Offset]
  //   Where OpFlags is 0..16 and Offset is 0..7
  //   The OpFlags represents for bits, from MSB (bit 3) to LSB (bit 0):
  //     E_RIGHT, E_LEFT, and 2 bits for E_SIZE
  //   The bits of BitMask represent which bytes the memory operation accesses
  //     (MSB is lowest byte, LSB is highest byte)
  const Epoch::ChunkBitMask Epoch::accessBitMask[16][chunkSize]={
    // E_RIGHT is 0, E_LEFT is 0, E_SIZE is 0..3
    {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01},
    {0xC0,0x00,0x30,0x00,0x0C,0x00,0x03,0x00},
    {0xF0,0x00,0x00,0x00,0x0F,0x00,0x00,0x00},
    {0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // E_RIGHT is 0, E_LEFT is 1, E_SIZE should be 0
    {0xF0,0x70,0x30,0x10,0x0F,0x07,0x03,0x01},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // E_RIGHT is 1, E_LEFT is 0, E_SIZE should be 0
    {0x80,0xC0,0xE0,0xF0,0x08,0x0C,0x0E,0x0F},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // E_RIGHT is 1, E_LEFT is 1, this should not happen
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
  };


  void Epoch::appendBuffer(const Epoch *succEpoch){
    I(myState>=State::GlobalSafe);
    I(myState==State::Committed);
    for(BufferBlocks::const_iterator succBlockIt=succEpoch->bufferBlocks.begin();
	succBlockIt!=succEpoch->bufferBlocks.end();succBlockIt++){
      Address blockBase=succBlockIt->first;
      const BufferBlock *succBlock=succBlockIt->second;
      // Find my corresponding block, create anew one if needed
      BufferBlocks::iterator myBlockIt=bufferBlocks.find(blockBase);
      if(myBlockIt==bufferBlocks.end()){
	std::pair<BufferBlocks::iterator,bool> insertResult=
	  bufferBlocks.insert(BufferBlocks::value_type(blockBase,new BufferBlock(this,blockBase)));
	I(insertResult.second==true);
	myBlockIt=insertResult.first;
      }
      BufferBlock *myBlock=myBlockIt->second;
      // Append successor block to my block
      myBlock->append(succBlock);
    }
  }

  void Epoch::cleanBuffer(void){
    I(myState==State::FullReplay);
    for(BufferBlocks::iterator blockIt=bufferBlocks.begin();
	blockIt!=bufferBlocks.end();blockIt++){
      blockIt->second->clean();
    }
  }

  void Epoch::eraseBlock(Address baseAddr){
    I(bufferBlocks.find(baseAddr)!=bufferBlocks.end());
    I(bufferBlocks[baseAddr]);
    I(bufferBlocks[baseAddr]->epoch==this);
    delete bufferBlocks[baseAddr];
    bufferBlocks.erase(baseAddr);
  }

  bool Epoch::mergeBlock(Address baseAddr, bool initialMerge){
    I(myState==State::Merging);
    BufferBlocks::iterator blockIt=bufferBlocks.find(baseAddr);
    I(blockIt!=bufferBlocks.end());
    I(blockIt->first==baseAddr);
    BufferBlock *block=blockIt->second;
    I(block&&(block->epoch==this));
    // Merge previous versions of the block, if there are any
    BufferBlock *prevVersion=block->getPrevVersion(!initialMerge);
    I((prevVersion!=block)&&((!prevVersion)||(prevVersion->epoch!=this)));
    if(prevVersion)
      prevVersion->epoch->mergeBlock(baseAddr,false);
    I(!block->getPrevVersion(!initialMerge));
    // Prepare the checkpoint for a write to this block
    I(myCheckpoint==Checkpoint::getCheckpoint(mySClock));
    myCheckpoint->write(baseAddr);
    // Merge and destroy this block
    block->merge();
    eraseBlock(baseAddr);
    // If this was the last block in a committed epoch,
    // the epoch itself should also be destroyed
    if(bufferBlocks.empty()){
      if(myState==State::Committed){
	I(myState==State::NoWait);
	delete this;
	return true;
      }
      if(myState>=State::WaitFullyMerged)
	becomeFullyMerged();
    }
    return false;
  }

  void Epoch::eraseBuffer(void){
    // Erasing a block invalidates the iterators in
    // bufferBlocks. Thus, we first find the addresses
    // of all the blocks, then erase them by address
    typedef std::vector<Address> AddressVector;
    AddressVector addrVect;
    for(BufferBlocks::iterator blockIt=bufferBlocks.begin();
	blockIt!=bufferBlocks.end();blockIt++){
      addrVect.push_back(blockIt->first);
      I(blockIt->second->isAccessed());
    }
    for(AddressVector::iterator addrIt=addrVect.begin();
	addrIt!=addrVect.end();addrIt++)
      eraseBlock(*addrIt);
  }

  void Epoch::mergeBuffer(void){
    // Merging a block will also erase it, and erasing
    // a block invalidates the iterators in bufferBlocks.
    // Thus, we first find the addresses of all the blocks,
    // then merge them by address
    I(myState==State::Merging);
    I(myState==State::ActiveMerge);
    typedef std::list<Address> AddressList;
    AddressList addrList;
    for(BufferBlocks::iterator blockIt=bufferBlocks.begin();
	blockIt!=bufferBlocks.end();blockIt++){
      addrList.push_back(blockIt->first);
      I(blockIt->second->isAccessed());
    }
    // The epoch may be destroyed when its last block is
    // merged. Thus, after this loop we can not use "this"
    // pointer or the epoch's instance members any more
    for(AddressList::iterator addrIt=addrList.begin();
	addrIt!=addrList.end();addrIt++)
      mergeBlock(*addrIt,true);
  }

//   EpochSet  Epoch::squashEpochSet;
//   EpochList Epoch::squashEpochList;
//   bool Epoch::doingSquashes=false;
  
//   void Epoch::queueSquash(void){
//     if(!squashEpochSet.count(this)){
//       ID(printf("Squashing %d(%d)\n",getTid(),myEid));
//       squashEpochSet.insert(this);
//       squashEpochList.push_back(this);
//     }
//   }
//   void Epoch::squashQueued(void){
//     if(!doingSquashes){
//       if(!squashEpochSet.empty())
// 	doSquashes();
//     }
//   }
//   // Internal function.
//   // Initiates a squash of all the epochs in the squashEpochList 
//   void Epoch::doSquashes(void){
//     I(!doingSquashes);
//     doingSquashes=true;
//     // Squash all the epochs in the set
//     for(EpochList::iterator squashIt=squashEpochList.begin();
// 	squashIt!=squashEpochList.end();squashIt++){
//       Epoch *squashEpoch=*squashIt;
//       squashEpoch->doSquash();
//     }
//     // Now try to re-activate them
//     for(EpochList::iterator exeIt=squashEpochList.begin();
// 	exeIt!=squashEpochList.end();exeIt++){
//       Epoch *exeEpoch=*exeIt;
//       exeEpoch->tryToExecute();
//     }
//     squashEpochSet.clear();
//     squashEpochList.clear();
//     doingSquashes=false;
//   }
  
//   void Epoch::doSquash(void){
//     I(myState*Executing!=None);
//     // Remove the epoch from the truncate set if it is there
//     truncateEpochs.erase(this);
//     // Epoch can not be in the merge set
//     I(!mergeEpochs.count(this));
//     // Erase buffered blocks and update block number stats
//     detWinVersCounts.remove(this);
//     eraseBuffer();
//     // Stop execution if still running
//     Pid_t myPid=static_cast<Pid_t>(myEid);
//     if(myState*Execution!=Complete)
//       osSim->stop(myPid);
//     // Restore the SESC execution context
//     ThreadContext *context=osSim->getContext(myPid);
//     *context=myContext;
//     if(myState<Decided){
//       I(myState<Safe);
//       // Eliminate runtime order between this epoch and its successors
//       EpochSet succGone;
//       for(EpochOrder::iterator succIt=succEpochs.begin();
// 	  succIt!=succEpochs.end();succIt++){
// 	Epoch *succEpoch=succIt->first;
// 	OrderEntry &succOrder=succIt->second;
// 	// If squash-inducing order, add successor to squash queue
// 	if(succOrder.hasAnyOf(OrderEntry::InduceSq)){
// 	  if(succEpoch->currInstrCount){
// 	    succEpoch->queueSquash();
// 	    I(doingSquashes);
// 	  }
// 	}
// 	// Eliminate runtime order from succOrder
// 	succOrder.remove(OrderEntry::Runtime);
// 	// Is there any ordering left?
// 	if(succOrder.hasAnyOf(OrderEntry::Any)){
// 	  // Yes, remove runtime ordering from successor's predOrder
// 	  succEpoch->predEpochs[this].remove(OrderEntry::Runtime);
// 	}else{
// 	  // No, remove succOrder entry and successor's predOrder entry
// 	  I(myState<Decided);
// 	  succGone.insert(succEpoch);
// 	  succEpoch->predEpochs.erase(this);
// 	}
//       }
//       I((myState<Safe)||succGone.empty());
//       for(EpochSet::iterator succGoneIt=succGone.begin();
// 	  succGoneIt!=succGone.end();succGoneIt++){
// 	I((*succGoneIt)->myState<Safe);
// 	succEpochs.erase(*succGoneIt);
//       }
//       // Eliminate runtime order between this epoch and its predecessors
//       EpochSet predGone;
//       for(EpochOrder::iterator predIt=predEpochs.begin();
// 	  predIt!=predEpochs.end();predIt++){
// 	Epoch *predEpoch=predIt->first;
// 	OrderEntry &predOrder=predIt->second;
// 	// Eliminate runtime order from predOrder
// 	predOrder.remove(OrderEntry::Runtime);
// 	// Is there any ordering left?
// 	if(predOrder.hasAnyOf(OrderEntry::Any)){
// 	  // Yes, remove runtime ordering from predecessor's succOrder
// 	  predEpoch->succEpochs[this].remove(OrderEntry::Runtime);
// 	}else{
// 	  // No, remove predOrder entry and predecessor's succOrder entry
// 	  I(myState<Analyzing);
// 	  predGone.insert(predEpoch);
// 	  predEpoch->succEpochs.erase(this);
// 	}
//       }
//       I((myState<Safe)||succGone.empty());
//       for(EpochSet::iterator predGoneIt=predGone.begin();
// 	  predGoneIt!=predGone.end();predGoneIt++){
// 	I((*predGoneIt)->myState<Safe);
// 	predEpochs.erase(*predGoneIt);
//       }
//     }
//     I(myState<Merging);
//     bool wasEnded=(myState*Execution==Complete);
//     myState-=SquashKills;
//     // If epoch was Ended, we just undid that
//     if(wasEnded){
//       unBecomeEnded();
//     }
//     conditionMode=false;
//     noRaceMode=false;
//     currInstrCount=0;
//     beginCurrentExe=0;
//     endCurrentExe=0;
//     rewindSpawnedEpochs();
//     if(myState>=Decided){
//       I(sysCallLogPos==sysCallLog.end());
//       sysCallLogPos=sysCallLog.begin();
//     }else{
//       assert(0);
//     }
//     I(myTrace.empty());
//     //clearTrace();
//   }
  
  Address Epoch::read(Address iAddrV, short iFlags,
		      Address dAddrV, Address dAddrR){
    I(iFlags&E_READ);
    I(myState!=State::Merging);
    I(myState!=State::Completed);
    I(myState!=State::PostAccess);
    if(myState==State::PreAccess)
      skipAcquire();
    if((myState==State::FullReplay)&&traceDataAddresses.count(dAddrV)){
      traceCodeAddresses.insert(iAddrV);
      // Add access to the trace
      if(myTrace.size()>16){
	TraceAccessEvent *prevEvent=0;
	if(!myTrace.empty())
	  prevEvent=dynamic_cast<TraceAccessEvent *>(myTrace.back());
	TraceAccessEvent *myEvent;
	if(prevEvent){
	  myEvent=prevEvent->newAccess(this,dAddrV,TraceAccessEvent::Read);
	}else{
	  myEvent=new TraceAccessEvent(this,dAddrV,TraceAccessEvent::Read);
	  I(iAddrV==(Address)(osSim->eventGetInstructionPointer(myPid)->addr));
	}
	if(myEvent){
	  myTrace.push_back(myEvent);
	}
      }
      // Update the forward and backward race info of this access
      RaceByAddrEp::iterator forwRaceByAddrEpIt=
	myForwRacesByAddrEp.find(dAddrV);
      if(forwRaceByAddrEpIt!=myForwRacesByAddrEp.end())
	forwRaceByAddrEpIt->second->addReadAccess(this,dAddrV);
      RaceByAddrEp::iterator backRaceByAddrEpIt=
	myBackRacesByAddrEp.find(dAddrV);
      if(backRaceByAddrEpIt!=myBackRacesByAddrEp.end())
	backRaceByAddrEpIt->second->addReadAccess(this,dAddrV);
      I((forwRaceByAddrEpIt!=myForwRacesByAddrEp.end())||
	(backRaceByAddrEpIt!=myBackRacesByAddrEp.end()));
    }
    // Find block base address and offset for this access 
    size_t blockOffs=dAddrR&blockAddrMask;
    Address blockBase=dAddrR-blockOffs;
    // Find the buffer block to access
    BufferBlock *bufferBlock=getBufferBlock(blockBase);
    // No block => no access
    if(!bufferBlock){
      I(myState!=State::Atomic);
      I(myState==State::Access);
      return 0;
    }
    I(bufferBlock->baseAddr==blockBase);
    // Index of the chunk within the block and the offset within the chunk
    size_t chunkIndx=blockOffs>>logChunkSize;
    size_t chunkOffs=dAddrR&chunkAddrMask;
    // Access mask contains the bytes read by this access
    ChunkBitMask accMask=accessBitMask[iFlags&(E_RIGHT|E_LEFT|E_SIZE)][chunkOffs];
    // Back mask contains bytes we need to find from predecessors because we don't already have them
    ChunkBitMask backMask=accMask&(~((bufferBlock->wrMask[chunkIndx])|(bufferBlock->xpMask[chunkIndx])));
    // Forward mask contains bytes that may be obsolete.
    // If the data in the block is stale, everything may be obsolete
    // If the data already in the block is not stale, only newly accessed bytes may be obsolete
    ChunkBitMask forwMask=bufferBlock->isStale()?accMask:backMask;
    // If back or forward mask is nonempty, other versions must be looked at
    if(backMask||forwMask){
      BlockVersions *versions=bufferBlock->getVersions();
      bool isAcquire=(myState==State::Acquire);
      // Can not advance if (same conditions as for a write)
      // 1) In full replay, or
      // 2) If already dependence-succeeded by another epoch, or
      // 3) If globally safe
      bool canAdvance=
	(myState!=State::FullReplay)&&
	(myState==State::NotSucceeded)&&
	(myState!=State::GlobalSafe);
      // Can not truncate if full replay or if there are same-thread successors
      bool canTruncate=(!canAdvance)&&
	(myState!=State::FullReplay)&&(myState==State::Access)&&
	(EpochList::reverse_iterator(myThreadEpochsPos)==myThread->threadEpochs.rend());
      if(canAdvance||canTruncate){
	// Find the new SClock value
	ClockValue newClock=mySClock;
	BlockVersions::BufferBlockList::iterator wrPos=versions->writers.begin();
	ClockValue clkDiff=isAcquire?syncClockDelta:0;
	for(;(wrPos!=versions->writers.end())&&((*wrPos)->epoch->mySClock>=mySClock-clkDiff);wrPos++){
	  // Skip self
	  if(wrPos==bufferBlock->writersPos)
	    continue;
	  // First conflict determines the new clock
	  if((*wrPos)->wrMask[chunkIndx]&forwMask){
	    // Conflicts only result from dependences between different threads
	    if(myThread!=(*wrPos)->epoch->myThread){
	      newClock=(*wrPos)->epoch->mySClock+1+clkDiff;
	    }else{
	      // Same-thread dependences should already be properly ordered
	      I(mySClock>(*wrPos)->epoch->mySClock);
	    }
	    break;
	  }
	}
	I(newClock>=mySClock);
	if(newClock>mySClock){
	  // Non-acquire read operation conflicting with a non-synchronized write is an anomaly
	  if(!isAcquire){
	    // Go thorough all conflicting writes not synchronizaed with this read, mark them as anomalies
	    BlockVersions::BufferBlockList::iterator wrPosRace=wrPos;
	    for(;(wrPosRace!=versions->writers.end())&&((*wrPosRace)->epoch->mySClock>=mySClock-clkDiff);wrPosRace++){
	      // Skip self
	      if(wrPosRace==bufferBlock->writersPos)
		continue;
	      // Skip non-conflicting blocks
	      if(((*wrPosRace)->wrMask[chunkIndx]&forwMask)==0)
		continue;
	      (*wrPosRace)->epoch->dstAnomalyFound();
	    }
	  }
	  if(!canAdvance){
	    I(canTruncate);
	    I(!isAcquire);
	    // Eliminate the buffer block if it was not accessed
	    if(!bufferBlock->isAccessed())
	      eraseBlock(blockBase);
	    Epoch *nextEpoch=changeEpoch();
// 	    anomalyInstAddr.insert(iAddrV);
// 	    nextEpoch->srcAnomalyFound();
	    nextEpoch->advanceSClock((*wrPos)->epoch,false);
	    return 0;
	  }
	  I(!canTruncate);
	  advanceSClock((*wrPos)->epoch,isAcquire);
	}
	// Now there are no remaining forward conflicts
	// But for debugging we leave the old mask to check
	ID(ChunkBitMask oldForwMask=forwMask);
	forwMask=0;
	ID(forwMask=oldForwMask);
      }
      // Now find the position of the block
      //      BlockVersions::BlockPositionInfo blockPos=versions->findBlockPosition(bufferBlock);
      BlockVersions::BlockPosition blockPos=versions->findBlockPosition(bufferBlock);
      // Now find conflicting accesses for this read
      BlockVersions::ConflictList writesBefore,writesAfter;
      ChunkBitMask memMask=versions->findReadConflicts(bufferBlock,chunkIndx,blockPos,
						       backMask,forwMask,writesBefore,writesAfter);
      // If we could have advanced or truncated, we should have avoided forward conflicts
      I((!canAdvance&&!canTruncate)||writesAfter.empty());
      // Process the writes that precede this read and do the copy-in
      if(memMask||(!writesBefore.empty())){
	// Perform the read access to the block
	versions->access(false,bufferBlock,blockPos);
	// Destination address for data copy-in
	unsigned char *dstDataPtr=
	  (unsigned char *)(bufferBlock->wkData)+blockOffs-chunkOffs;	
	for(BlockVersions::ConflictList::const_iterator wrBeforeIt=writesBefore.begin();
	    wrBeforeIt!=writesBefore.end();wrBeforeIt++){
	  BufferBlock *wrBeforeBlock=wrBeforeIt->block;
	  ChunkBitMask wrBeforeMask=wrBeforeIt->mask;
	  // The predecesor block has sourced data for this flow dependence
	  wrBeforeBlock->becomeFlowSource();
	  // Source data for copy-in is in the predecessor's buffer
	  unsigned char *srcDataPtr=
	    (unsigned char *)(wrBeforeBlock->wkData)+blockOffs-chunkOffs;
	  BufferBlock::maskedChunkCopy(srcDataPtr,dstDataPtr,wrBeforeMask);
	  // Add the bytes to exposed read mask, unless we are in condition mode
	  bufferBlock->xpMask[chunkIndx]|=wrBeforeMask;
	  Epoch *wrBeforeEpoch=wrBeforeBlock->epoch;
	  // Data race on in-order raw?
	  if(compareSClock(wrBeforeEpoch)!=StrongBefore){
	    I(!isAcquire);
	    I(myVClock.compare(wrBeforeEpoch->myVClock)==VClock::Unordered);
	    anomalyInstAddr.insert(iAddrV);
	    srcAnomalyFound();
	    wrBeforeEpoch->dstAnomalyFound();
	  }
	  if(myVClock.compare(wrBeforeEpoch->myVClock)!=VClock::Before){
	    I(myVClock.compare(wrBeforeEpoch->myVClock)==VClock::Unordered);
	    if(isAcquire){
	      myVClock.succeed(wrBeforeEpoch->myVClock);
	      if(myState>=State::ThreadSafe){
		I(myState!=State::Committed);
		I(!myThread->threadSafeAvailable);
		myThread->changeThreadSafeVClock(myVClock);
	      }
	    }else{
	      raceInstAddr.insert(iAddrV);
	      srcRaceFound();
	      wrBeforeEpoch->dstRaceFound();
	      addRace(wrBeforeEpoch,this,dAddrV,RaceInfo::Flow);
	    }
	  }
	  // This epoch succeeds the predecessor write in a flow dependence
	  wrBeforeEpoch->succeeded(this);
	}
	if(memMask){
	  // Source data for copy-in is in main memory
	  unsigned char *srcDataPtr=(unsigned char *)(dAddrR-chunkOffs);
	  BufferBlock::maskedChunkCopy(srcDataPtr,dstDataPtr,memMask);
	  // Add the bytes to exposed read mask, unless we are in condition mode
	  bufferBlock->xpMask[chunkIndx]|=memMask;
	}
      }
      // Process the writes that logically succeed this read
      if(!writesAfter.empty()){
	// Data block is stale because overwriters already exist
	bufferBlock->becomeStale();
	// Should not be reading stale data when doing Acquire
	I(myState!=State::Acquire);
	// Create ordering and detect races, if needed
	for(BlockVersions::ConflictList::const_iterator wrAfterIt=writesAfter.begin();
	    wrAfterIt!=writesAfter.end();wrAfterIt++){
	  Epoch *wrAfterEpoch=wrAfterIt->block->epoch;
	  if(compareSClock(wrAfterEpoch)!=StrongAfter){
	    I(myVClock.compare(wrAfterEpoch->myVClock)==VClock::Unordered);
	    anomalyInstAddr.insert(iAddrV);
	    srcAnomalyFound();
	    wrAfterEpoch->dstAnomalyFound();
	  }
	  if(myVClock.compare(wrAfterEpoch->myVClock)!=VClock::After){
	    I(0); // Should never happen due to epoch truncations
	    addRace(this,wrAfterEpoch,dAddrV,RaceInfo::Anti);
	  }
	  succeeded(wrAfterEpoch);
	}
      }
    }
    // Read will access the buffered block
    return ((Address)(bufferBlock->wkData))+blockOffs;
  }
  
  Address Epoch::write(Address iAddrV, short iFlags,
		       Address dAddrV, Address dAddrR){
    I(iFlags&E_WRITE);
    I(myState!=State::Merging);
    I(myState!=State::Completed);
    if(myState==State::PreAccess)
      skipAcquire();
    if((myState==State::FullReplay)&&traceDataAddresses.count(dAddrV)){
      traceCodeAddresses.insert(iAddrV);
      if(myTrace.size()>16){
	// Add access to the trace
	TraceAccessEvent *myEvent=
	  new TraceAccessEvent(this,dAddrV,TraceAccessEvent::Write);
	I(iAddrV==(Address)(osSim->eventGetInstructionPointer(myPid)->addr));
	myTrace.push_back(myEvent);
      }
      // Update the forward and backward race info of this access
      RaceByAddrEp::iterator forwRaceByAddrEpIt=
	myForwRacesByAddrEp.find(dAddrV);
      if(forwRaceByAddrEpIt!=myForwRacesByAddrEp.end())
	forwRaceByAddrEpIt->second->addWriteAccess(this,dAddrV);
      RaceByAddrEp::iterator backRaceByAddrEpIt=
	myBackRacesByAddrEp.find(dAddrV);
      if(backRaceByAddrEpIt!=myBackRacesByAddrEp.end())
	backRaceByAddrEpIt->second->addWriteAccess(this,dAddrV);
      I((forwRaceByAddrEpIt!=myForwRacesByAddrEp.end())||
	(backRaceByAddrEpIt!=myBackRacesByAddrEp.end()));
    }
    // Find block base address and offset for this access 
    size_t blockOffs=dAddrR&blockAddrMask;
    Address blockBase=dAddrR-blockOffs;
    // Find the buffer block to access
    BufferBlock *bufferBlock=getBufferBlock(blockBase);
    // No block => no access
    if(!bufferBlock){
      I(myState!=State::Atomic);
      I(myState==State::Access);
      return 0;
    }
    I(bufferBlock->baseAddr==blockBase);
    // Index of the chunk within the block and the offset within the chunk
    size_t chunkIndx=blockOffs>>logChunkSize;
    size_t chunkOffs=dAddrR&chunkAddrMask;
    // Access mask initially contains the bytes written by this access
    ChunkBitMask accMask=accessBitMask[iFlags&(E_RIGHT|E_LEFT|E_SIZE)][chunkOffs];
    // This write needs to be ordered after reads and writes whose data it overwrites
    // If we have already written to the same location, the order already exists
    ChunkBitMask backMask=accMask&(~(bufferBlock->wrMask[chunkIndx]));
    // Forward mask contains bytes that may be obsolete or prematurely read
    // If the data is stale or a source for a flow dependence, everything must be checkd
    // Othrwise, only newly stored bytes need be checked
    ChunkBitMask forwMask=
      (bufferBlock->isFlowSource()||bufferBlock->isStale())?accMask:(accMask&(~(bufferBlock->wrMask[chunkIndx])));
    // If back or forward mask is nonempty, other versions must be looked at
    if(backMask||forwMask){
      BlockVersions *versions=bufferBlock->getVersions();
      // Can not advance if (same conditions as for a read)
      // 1) In full replay, or
      // 2) if already dependence-succeeded by another epoch, or
      // 3) If globally safe
      bool canAdvance=
	(myState!=State::FullReplay)&&
	(myState==State::NotSucceeded)&&
	(myState!=State::GlobalSafe);
      // Can not truncate if full replay or if there are same-thread successors
      bool canTruncate=(!canAdvance)&&
	(myState!=State::FullReplay)&&(myState==State::Access)&&
	(EpochList::reverse_iterator(myThreadEpochsPos)==myThread->threadEpochs.rend());
      if(canAdvance||canTruncate){
	// Find the new SClock value
	ClockValue newClock=mySClock;
	BlockVersions::BufferBlockList::iterator acPos=versions->accessors.begin();
	while((acPos!=versions->accessors.end())&&((*acPos)->epoch->mySClock>=mySClock)){
	  // Skip self
	  if(acPos!=bufferBlock->accessorsPos){
	    // First conflict determines the new clock
	    if(((*acPos)->wrMask[chunkIndx]|(*acPos)->xpMask[chunkIndx])&forwMask){
	      if((*acPos)->epoch->myState==State::Acquire){
		BlockVersions::BufferBlockList::iterator oldAcPos=acPos;
		acPos++;
		I((*oldAcPos)->epoch->myState==State::NotSucceeded);
		(*oldAcPos)->epoch->squash();
		continue;
	      }else{
		newClock=(*acPos)->epoch->mySClock+1;
		break;
	      }
	    }
	  }
	  acPos++;
	}
	I(newClock>=mySClock);
	if(newClock>mySClock){
	  // Go thorough all conflicting accesses not synchronized with this write, mark them as anomalies
	  BlockVersions::BufferBlockList::iterator acPosRace=acPos;
	  for(;(acPosRace!=versions->accessors.end())&&((*acPosRace)->epoch->mySClock>=mySClock);acPosRace++){
	    // Skip self
	    if(acPosRace==bufferBlock->accessorsPos)
	      continue;
	    // Skip non-conflicting blocks
	    if((((*acPosRace)->wrMask[chunkIndx]|(*acPosRace)->xpMask[chunkIndx])&forwMask)==0)
	      continue;
	    (*acPosRace)->epoch->dstAnomalyFound();
	  }
	  if(!canAdvance){
	    I(canTruncate);
	    // Eliminate the buffer block if it was not accessed
	    if(!bufferBlock->isAccessed())
	      eraseBlock(blockBase);
	    Epoch *nextEpoch=changeEpoch();
// 	    anomalyInstAddr.insert(iAddrV);
// 	    nextEpoch->srcAnomalyFound();
	    nextEpoch->advanceSClock((*acPos)->epoch,false);
	    return 0;
	  }
	  I(!canTruncate);
	  advanceSClock((*acPos)->epoch,false);
	}
	// Now there are no remaining forward conflicts
	// But for debugging we leave the old mask to check
	ID(ChunkBitMask oldForwMask=forwMask);
	forwMask=0;
	ID(forwMask=oldForwMask);
      }
      I(bufferBlock->baseAddr==blockBase);
      I(bufferBlock->myVersions==versions);
      // Now find the position of the block
      BlockVersions::BlockPosition blockPos=versions->findBlockPosition(bufferBlock);
      // Perform the write access to the block
      versions->access(true,bufferBlock,blockPos);
      // Check for squashes and successor writes
      if(forwMask){
	// Go thorugh the successor versions in the accessors list
	BlockVersions::BufferBlockList::reverse_iterator forwItR(blockPos);
	while(forwItR!=versions->accessors.rend()){
	  BufferBlock *otherBlock=*forwItR;
	  if(otherBlock->xpMask[chunkIndx]&forwMask){
	    // A conflicting successor reader is squashed
	    otherBlock->epoch->squash();
	    // Repeat with the same reverse iterator
	    // Its base iterator is the predecessor of
	    // the block we just removed, so the reverse
	    // iterator again points to the next block
	    continue;
	  }else if(otherBlock->wrMask[chunkIndx]&forwMask){
	    // This block is stale because there is a successor overwriter
	    bufferBlock->becomeStale();
	    // This epoch is already succeeded
	    I(myState!=State::Acquire);
	    succeeded(otherBlock->epoch);
	    if(compareSClock(otherBlock->epoch)!=StrongAfter){
	      I(myVClock.compare(otherBlock->epoch->myVClock)==VClock::Unordered);
	      anomalyInstAddr.insert(iAddrV);
	      srcAnomalyFound();
	      otherBlock->epoch->dstAnomalyFound();
	    }
	    I(myVClock.compare(otherBlock->epoch->myVClock)!=VClock::Before);
	    if(myVClock.compare(otherBlock->epoch->myVClock)!=VClock::After){
	      raceInstAddr.insert(iAddrV);
	      srcRaceFound();
	      otherBlock->epoch->dstRaceFound();
	      addRace(this,otherBlock->epoch,dAddrV,RaceInfo::Output);
	    }
	  }
	  forwItR++;
	}
      }
      // Check for obsolete versions
      if(backMask){
	BlockVersions::BufferBlockList::iterator backIt=blockPos;
	I(backIt==bufferBlock->accessorsPos);
	backIt++;
	while(backMask&&(backIt!=versions->accessors.end())){
	  BufferBlock *otherBlock=*backIt;
	  backIt++;
	  ChunkBitMask wrConf=otherBlock->wrMask[chunkIndx]&backMask;
	  backMask^=wrConf;
	  ChunkBitMask rdConf=otherBlock->xpMask[chunkIndx]&backMask;
	  if(wrConf|rdConf){
	    if(otherBlock->epoch->myState==State::Acquire){
	      if((myState==State::Release)||((myState==State::Acquire)&&!wrConf)){
		I(otherBlock->epoch->myState==State::NotSucceeded);
		otherBlock->epoch->squash();
		continue;
	      }else{
		I(myState==State::Acquire);
		I(wrConf);
                I(myState==State::NotSucceeded);
		return 0;
	      }
	    }
	    otherBlock->becomeStale();
	    otherBlock->epoch->succeeded(this);
	    if(compareSClock(otherBlock->epoch)!=StrongBefore){
	      I(myVClock.compare(otherBlock->epoch->myVClock)==VClock::Unordered);
	      anomalyInstAddr.insert(iAddrV);
	      srcAnomalyFound();
	      otherBlock->epoch->dstAnomalyFound();
	    }
	    I(myVClock.compare(otherBlock->epoch->myVClock)!=VClock::After);
	    if(myVClock.compare(otherBlock->epoch->myVClock)!=VClock::Before){
	      raceInstAddr.insert(iAddrV);
	      srcRaceFound();
	      otherBlock->epoch->dstRaceFound();
	      if(wrConf)
		addRace(otherBlock->epoch,this,dAddrV,RaceInfo::Output);
	      if(rdConf)
		addRace(otherBlock->epoch,this,dAddrV,RaceInfo::Anti);
	    }
	  }
	}
      }
    }
    // Add bytes to write mask, unless we are in condition mode
    bufferBlock->wrMask[chunkIndx]|=accMask;
    // Write will access the buffered block
    return ((Address)(bufferBlock->wkData))+blockOffs;
  }

//   Epoch *Epoch::truncateEpoch(bool regDep){
//     // Do all kinds of checks
//     I(myState*Execution==Ordered);
//     I(currInstrCount);
//     I((myState>=Decided)||spawnedEpochs.empty());
//     I((myState>=Decided)||!releasedSync);
//     I((myState>=Decided)||(!maxInstrCount)||(currInstrCount<=maxInstrCount));
//     I((myState<Decided)||(currInstrCount==maxInstrCount));
//     // Create a continuation of the epoch
//     Epoch *newEpoch=createNextEpoch(mySeqSpec,mySyncSpec,0);
//     ID(printf("Truncating epoch %d(%d->%d)\n",
// 	      getTid(),myEid,newEpoch->myEid));
//     if(regDep){
//       // Epoch created in the middle of the dynamic instruction stream.
//       // Register dependences are possible, but we do not detect them
//       // Thus, conservatively assume there is a register dependence
//       newEpoch->succeed(this,OrderEntry::RegDep);
//     }
//     completeEpoch();
//     return newEpoch;
//   }
  
//   void Epoch::postExecActions(void){
//     while(!truncateEpochs.empty()){
//       // Get the first epoch in the truncate set
//       EpochSet::iterator truncIt=truncateEpochs.begin();
//       Epoch *oldEpoch=*truncIt;
//       oldEpoch->truncateEpoch(oldEpoch->myState<Decided);
//       // Should no longer be in the truncate set
//       I(!truncateEpochs.count(oldEpoch));
//     }
//     while(!analyzeEpochs.empty()){
//       // Get the first epoch in the analyze set
//       EpochSet::iterator analyzeIt=analyzeEpochs.begin();
//       Epoch *analyzeEpoch=*analyzeIt;
//      // Make the epoch analyzable
//       analyzeEpoch->becomeAnalyzing();
//       // Should no longer be in the analyze set
//       I(!analyzeEpochs.count(analyzeEpoch));
//     } 
//     while(!mergeEpochs.empty()){
//       // Get the first epoch in the merge set
//       EpochSet::iterator mergeIt=mergeEpochs.begin();
//       Epoch *mergeEpoch=*mergeIt;
//      // Make the epoch mergeable
//       mergeEpoch->becomeMerging();
//       // Should no longer be in the merge set
//       I(!mergeEpochs.count(mergeEpoch));
//     }
//     while(!destroyEpochs.empty()){
//       // Get the first epoch in the destroy set
//       EpochSet::iterator destroyIt=destroyEpochs.begin();
//       Epoch *destroyEpoch=*destroyIt;
//       // Make the epoch destroyed
//       becomeDestroyed(destroyEpoch);
//       // Should no longer be in the destroy set
//       I(!destroyEpochs.count(destroyEpoch));
//     }
//   }

}
