#include "Checkpoint.h"

namespace tls{

  // A record of the execution sequence of one thread
  class Checkpoint::ExeOrder::ThreadExeOrder{
    // A record of an epoch - a single atomic block of execution in one thread
    struct ExeOrderEntry{
      ClockValue myClk;  // Global clock of this epoch
      size_t     iCount; // Number of instructions in this epoch
      ClockValue parClk; // Global clock of parent of this epoch
      SysCallLog sysCallLog; // System call record during this epoch
      // Constructor for a new epoch
      ExeOrderEntry(ClockValue myClk, ClockValue parClk)
	: myClk(myClk), iCount(0), parClk(parClk), sysCallLog(){
      }
      ExeOrderEntry(const ExeOrderEntry &other){
	I(0);
      }
    };
    // Context of this thread just before checkpoint time
    ThreadContext *initialContext;
    typedef std::list<ExeOrderEntry *> ExeOrderList;
    // Sequence of epochs in this thread
    ExeOrderList orderList;
    // Position of next epoch to be started
    ExeOrderList::iterator startOrderPos;
    // Position of next epoch to be merged
    ExeOrderList::iterator mergeOrderPos;
  public:
    ThreadExeOrder(void)
      : initialContext(0), orderList(),
	startOrderPos(orderList.begin()),
	mergeOrderPos(orderList.begin()){
    }
    ThreadExeOrder(const ThreadExeOrder &other){
      I(0);
    }
    ~ThreadExeOrder(void){
      I(startOrderPos==orderList.end());
      I(mergeOrderPos==orderList.end());
      if(initialContext)
	delete initialContext;
      for(ExeOrderList::iterator listIt=orderList.begin();
	  listIt!=orderList.end();listIt++)
	delete *listIt;
    }
    void spliceIntoPrevious(ThreadExeOrder &prev){
      I(startOrderPos==orderList.end());
      I(mergeOrderPos==orderList.end());
      I(prev.startOrderPos==prev.orderList.end());
      I(prev.mergeOrderPos==prev.orderList.end());
      if(!prev.initialContext){
	prev.initialContext=initialContext;
	initialContext=0;
      }
      prev.orderList.splice(prev.orderList.end(),orderList);
      I(startOrderPos==orderList.end());
      I(mergeOrderPos==orderList.end());
      I(prev.startOrderPos==prev.orderList.end());
      I(prev.mergeOrderPos==prev.orderList.end());
    }
    bool hasNextEpoch(ClockValue parClk){
      if(startOrderPos==orderList.end())
	return false;
      I(((startOrderPos==orderList.begin())&&(parClk==0))||
	((*startOrderPos)->parClk==parClk));
      return true;
    }
    void getNextEpoch(ClockValue parClk,
		      ClockValue &myClk, size_t &iCount,
		      SysCallLog &myLog, ThreadContext *myContext){
      I(startOrderPos!=orderList.end());
      I(((startOrderPos==orderList.begin())&&(parClk==0))||
	((*startOrderPos)->parClk==parClk));
      myClk=(*startOrderPos)->myClk;
      iCount=(*startOrderPos)->iCount;
      I(myLog.empty());
      myLog.swap((*startOrderPos)->sysCallLog);
      if(startOrderPos==orderList.begin()){
	I(initialContext);
	myContext->copy(initialContext);
      }
      startOrderPos++;
    }
    void mergeEpochInit(ClockValue parClk, ClockValue myClk,
		        const ThreadContext *myContext){
      if(mergeOrderPos==orderList.end()){
	if(!initialContext){
	  I(orderList.empty());
	  initialContext=new ThreadContext();
	  initialContext->copy(myContext);
	}
	I(startOrderPos==orderList.end());
	orderList.insert(startOrderPos,new ExeOrderEntry(myClk,parClk));
	I(mergeOrderPos==orderList.end());
	I(startOrderPos==orderList.end());
      }else{
	I((*mergeOrderPos)->myClk==myClk);
	I((*mergeOrderPos)->parClk==parClk);
	I((*mergeOrderPos)->sysCallLog.empty());
        mergeOrderPos++;	
      }
    }
    void mergeEpochDone(ClockValue myClk, size_t iCount, SysCallLog &myLog){
      ExeOrderList::reverse_iterator mergeRIt(mergeOrderPos);
      I(mergeRIt!=orderList.rend());
      while((*mergeRIt)->myClk!=myClk){
        I(mergeRIt!=orderList.rend());
        mergeRIt++;
      }
      I(((*mergeRIt)->iCount==0)||((*mergeRIt)->iCount==iCount));
      (*mergeRIt)->iCount=iCount;
      (*mergeRIt)->sysCallLog.swap(myLog);
    }
    void rewind(void){
      I(startOrderPos==orderList.end());
      I(mergeOrderPos==orderList.end());
      startOrderPos=orderList.begin();
      mergeOrderPos=orderList.begin();
    }
  };
  
  Checkpoint::ExeOrder::ThreadExeOrder *Checkpoint::ExeOrder::lookupThreadExeOrder(ThreadID tid){
    ThreadExeOrders::iterator threadIt=threadExeOrders.find(tid);
    if(threadIt==threadExeOrders.end())
      return threadExeOrders[tid]=new ThreadExeOrder();
    I(threadIt->second);
    return threadIt->second;
  }
  
  Checkpoint::ExeOrder::~ExeOrder(void){
    for(ThreadExeOrders::iterator threadIt=threadExeOrders.begin();
	threadIt!=threadExeOrders.end();threadIt++){
      delete threadIt->second;
    }
  }
  
  bool Checkpoint::ExeOrder::hasNextEpoch(ThreadID tid, ClockValue parClk){
    ThreadExeOrders::iterator threadIt=threadExeOrders.find(tid);
    if(threadIt==threadExeOrders.end())
      return false;
    I(threadIt->second);
    return threadIt->second->hasNextEpoch(parClk);
  }
  
  void Checkpoint::ExeOrder::getNextEpoch(ThreadID tid, ClockValue parClk,
					  ClockValue &myClk, size_t &iCount, SysCallLog &myLog,
					  ThreadContext *myContext){
    I(hasNextEpoch(tid,parClk));
    ThreadExeOrder *threadExeOrder=lookupThreadExeOrder(tid);
    threadExeOrder->getNextEpoch(parClk,myClk,iCount,myLog,myContext);
  }

  void Checkpoint::ExeOrder::mergeEpochInit(ThreadID tid, ClockValue parClk,
					    ClockValue myClk, const ThreadContext *myContext){
    ThreadExeOrder *threadExeOrder=lookupThreadExeOrder(tid);
    threadExeOrder->mergeEpochInit(parClk,myClk,myContext);
  }
  
  void Checkpoint::ExeOrder::mergeEpochDone(ThreadID tid, ClockValue myClk,
                                            size_t iCount, SysCallLog &myLog){
    ThreadExeOrder *threadExeOrder=lookupThreadExeOrder(tid);
    threadExeOrder->mergeEpochDone(myClk,iCount,myLog);
  }
  
  void Checkpoint::ExeOrder::spliceIntoPrevious(ExeOrder &prev){
    for(ThreadExeOrders::iterator threadIt=threadExeOrders.begin();
	threadIt!=threadExeOrders.end();threadIt++){
      ThreadID tid=threadIt->first;
      threadIt->second->spliceIntoPrevious(*(prev.threadExeOrders[tid]));
      delete threadIt->second;
      threadIt->second=0;
    }
    threadExeOrders.clear();
  }

  void Checkpoint::ExeOrder::rewind(void){
    for(ThreadExeOrders::iterator threadIt=threadExeOrders.begin();
	threadIt!=threadExeOrders.end();threadIt++){
      threadIt->second->rewind();
    }
  }

  class Checkpoint::BlockData{
    unsigned long long data[blockSize/sizeof(unsigned long long)];
  public:
    // Copies data from baseAddr in memory to this block
    BlockData(Address baseAddr){
      unsigned long long *src=(unsigned long long *)baseAddr;
      unsigned long long *srcEnd=(unsigned long long *)(baseAddr+blockSize);
      unsigned long long *dst=data;
      while(src!=srcEnd){
	*dst=*src;
	src++;
	dst++;
	I((Address)src<=(Address)srcEnd);
      }
    }
    // Copies data from this block to baseAddr in memory
    void restore(Address baseAddr){
      unsigned long long *dst=(unsigned long long *)baseAddr;
      unsigned long long *dstEnd=(unsigned long long *)(baseAddr+blockSize);
      unsigned long long *src=data;
      while(dst!=dstEnd){
	*dst=*src;
	src++;
	dst++;
	I((Address)dst<=(Address)dstEnd);
      }
    }
    // Returns true iff
    // data at baseAddr in memory and in this block is the same
    bool verify(Address baseAddr){
      unsigned long long *src=(unsigned long long *)baseAddr;
      unsigned long long *srcEnd=(unsigned long long *)(baseAddr+blockSize);
      unsigned long long *dst=data;
      while(src!=srcEnd){
	if(*dst!=*src)
	  return false;
	src++;
	dst++;
      }
      return true;
    }
  };

  Checkpoint::CheckpointList Checkpoint::allCheckpoints;

  void Checkpoint::staticConstructor(void){
    // Create initial checkpoint
    new Checkpoint(static_cast<ClockValue>(0));
  }

  void Checkpoint::staticDestructor(void){
    I(!allCheckpoints.empty());
    //#if (defined DEBUG)
    size_t cpCount=0;
    size_t cpBlocks=0;
    for(CheckpointList::iterator listIt=allCheckpoints.begin();
	listIt!=allCheckpoints.end();listIt++){
      cpCount++;
      cpBlocks+=(*listIt)->myBlocks.size();
    }
    printf("Checkpoints %d, buffer size: earliest %d all %d\n",
	   cpCount,blockSize*allCheckpoints.front()->myBlocks.size(),blockSize*cpBlocks);
    //#endif // (defined DEBUG)
  }

  Checkpoint::Checkpoint(ClockValue myClock)
    : myClock(myClock), mergingEpochs(0), complete(false),
      myPos(allCheckpoints.insert(allCheckpoints.begin(),this)),
      exeOrder(){
    // My clock must be larger than that of the previous checkpoint
    ID(CheckpointList::iterator prevPos=myPos);
    ID(prevPos++);
    I((prevPos==allCheckpoints.end())||(myClock>(*prevPos)->myClock));
  }
  
  void Checkpoint::write(Address addr){
    Address blockBase=addr&blockBaseMask;
    if(myBlocks.find(blockBase)==myBlocks.end()){
      I(!complete);
      myBlocks.insert(BlocksMap::value_type(blockBase,new BlockData(blockBase)));
    }
  }
  
  Checkpoint *Checkpoint::getCheckpoint(ClockValue epochClock){
    I(!allCheckpoints.empty());
    CheckpointList::iterator ckpIt=allCheckpoints.begin();
    while((*ckpIt)->myClock>epochClock){
      ckpIt++;
      I(ckpIt!=allCheckpoints.end());
    }
    return *ckpIt;
  }

  Checkpoint *Checkpoint::mergeInit(Epoch *epoch){
    Checkpoint *retVal=getCheckpoint(epoch->getClock());
    I(epoch->getClock()>=retVal->myClock);
    retVal->exeOrder.mergeEpochInit(epoch->getTid(),
				    epoch->parentSClock,epoch->getClock(),
				    &(epoch->myContext));
    I(!retVal->complete);
    retVal->mergingEpochs++;
    return retVal;
  }
  
  void Checkpoint::mergeDone(Epoch *epoch){
    exeOrder.mergeEpochDone(epoch->getTid(),epoch->getClock(),
                            epoch->pendInstrCount,epoch->sysCallLog);
    mergingEpochs--;
  }

  void Checkpoint::merge(void){
    CheckpointList::iterator predPos=myPos;
    predPos++;
    I(predPos!=allCheckpoints.end());
    Checkpoint *pred=*predPos;
    I(complete&&pred->complete);
    I(pred->myClock<myClock);
    // Destroy my blocks, merging those pred doesn't have already
    BlocksMap &predBlocks=pred->myBlocks;
    for(BlocksMap::iterator blockIt=myBlocks.begin();blockIt!=myBlocks.end();blockIt++){
      if(predBlocks.find(blockIt->first)==predBlocks.end()){
	predBlocks.insert(*blockIt);
      }else{
	delete blockIt->second;
      }
    }
    myBlocks.clear();
    // Splice my execution order into the previous one
    exeOrder.spliceIntoPrevious(pred->exeOrder);
    // This checkpoint is no more
    delete this;
  }
  
  void Checkpoint::rollback(ClockValue &currClock, ClockValue targClock){
    CheckpointList::iterator cpIt=allCheckpoints.begin();
    I(cpIt!=allCheckpoints.end());
    I(currClock>targClock);
    do{
      I(cpIt!=allCheckpoints.end());
      (*cpIt)->rewind();
      currClock=(*cpIt)->myClock;
    }while(currClock>targClock);
    // Restore threads to checkpoint state
    // TODO
  }

  void Checkpoint::rewind(void){
    // Restore memory to checkpoint state
    for(BlocksMap::iterator blocksIt=myBlocks.begin();
	blocksIt!=myBlocks.end();blocksIt++)
      blocksIt->second->restore(blocksIt->first);
    // Rewind execution logs
    exeOrder.rewind();
  }

  Checkpoint::~Checkpoint(void){
    I(complete);
    // Free all the memory blocks
    for(BlocksMap::iterator blocksIt=myBlocks.begin();
	blocksIt!=myBlocks.end();blocksIt++)
      delete blocksIt->second;
    // Remove from list of checkpoints
    allCheckpoints.erase(myPos);
    // Excution order is destroyed automatically, so we do nothing about it
  }
}
