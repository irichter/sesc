#ifndef CACHECOHERENCE_H
#define CACHECOHERENCE_H

#include <vector>
#include <queue>
#include <list>

#include "MPCache.h"
#include "CacheCore.h"
#include "PMessage.h"
#include "ProtocolBase.h"
#include "ProtocolCB.h"
#include "MemRequest.h"
#include "estl.h"

// TODO: There is LOTS OF CODE in this .h. Please, someone move it to
// .cpp files. (Maybe split in several files)

enum CacheState {INV,    //Invalidate 
                 SHRD,   //Shared
                 EX,     //Exclusive
                 MOD,    //Modified, globally correct value
                 SMOD};  //Stale Modified, higher level has more recent value


class CacheCoherenceMsg : public PMessage {
private:
  static pool<CacheCoherenceMsg> ccMsgPool;

  static ulong gXActionIDCnt;

  ulong xActionID;

protected:
  ulong dstAddr;
  NetDevice_t originID;

  ushort numAcks;
  ushort incomingInvs; //this is really a boolean

public:

  ushort numFailures;
  NetDevice_t failedSrcID;

  static CacheCoherenceMsg *createCCMsg(ProtocolBase *srcPB, 
				MessageType mtype, 
					NetDevice_t dstID, long addr, 
					MemRequest *mreq=0);

  static CacheCoherenceMsg *createCCMsg(ProtocolBase *srcPB,
					MessageType mtype,
					NetDevice_t dstID,
					CacheCoherenceMsg *ccm);
  
  void transform2CCRdAck();
  void transform2CCRdXAck();
  void transform2CCWrAck(ushort acks=0);
  void transform2CCInvAck();
  void transform2CCWrBackAck();
  void transform2CCNack();
  void transform2CCNewSharerAck();
  void transform2CCNewOwnerAck();
  void transform2CCRmSharerAck();
  void transform2CCRmLineAck();
  void transform2CCFailedRd();
  void transform2CCFailedWr();
  void reSend(MessageType mtype, NetDevice_t dstID);

  void dump() {
  }

  ulong getDstAddr() const {
    return dstAddr;
  }

  NetDevice_t getDstNetID() const {
    return dstPB->getNetDeviceID();
  }

  NetDevice_t getSrcNetID() const {
    return srcPB->getNetDeviceID();
  }

  PortID_t getDstPortID() const {
    return dstPB->getPortID();
  }

  NetDevice_t getOriginID() const {
    return originID;
  }

  void redirectToOrigin() {
    redirect(originID);
  }

  void redirectMsg(NetDevice_t netID) {
    redirect(netID);
  }

  void reverseMsg() {
    reverseDirection();
  }

  void setNumAcks(ushort num) {
    numAcks = num;
    if(num!=0)
      incomingInvs = 1;
  }

  bool invsExpected() {
    return (bool)incomingInvs;
  }

  void clrInvsExpected() {
    incomingInvs = 0;
  }

  ushort getNumAcks() const {
    return numAcks;
  }

  void garbageCollect() {
    refCount--;
    if (refCount == 0)
      ccMsgPool.in(this);
  }

  MemRequest *getMemRequest() {
    return mreq;
  }

  ulong getXActionID() {
    return xActionID;
  }
  
};

class CacheCoherenceProt : public ProtocolBase {
protected:

  virtual NetDevice_t getHomeNodeID(ulong addr) = 0;

  ulong addrMask;

public:

  //FIXME: clean up
  class CCActiveMsg {
  public:
    MessageType mtype; //This is type specified on entry only!!
    bool   staleRmSharer;
    bool   staleRd;
    bool   doNotRemoveFromQueue;
    ushort nackBackOff;
    bool   localReq;
    bool   launched;
    ushort acksRcvd;   //Initialize to zero!
                       //Slave: Allows us to avoid writebacks, 
                       //       using a CCRdXAck
                       //Slave: When we are waiting for a CCRdAck, 
                       //       we may get a
    ushort numSharers; //CCInv. We go ahead and ack the CCInv, and when the
                       //data arrives, we give needed word to the processor,
                       //but the line is not cached. This prevents repeated
                       //loads, and complies with strictest models of memory
                       //consistency.

    CacheCoherenceMsg *ccmsg; 

    ID(Time_t start_time);
    
    void init(CacheCoherenceMsg *accmsg, ushort aacksRcvd=0) {
      nackBackOff = 0;
      numSharers = 0;
      acksRcvd = aacksRcvd;
      ccmsg = accmsg;
      doNotRemoveFromQueue = false;
      staleRmSharer = false;
      localReq = false;
      launched = false;
      staleRd = false;
      IS(start_time = globalClock);
      if(accmsg!=NULL)
	mtype = accmsg->getType();
      else
	mtype = DefaultMessage;
    }
    
  };

  class DirectoryState : public StateGeneric<DirectoryState,PAddr> {
  public:
    CacheState             ccState;
    bool                   nIncl; //not inclusive
    std::list<NetDevice_t> sharers;

    DirectoryState()
      : ccState(INV)
	,nIncl(false) {
    }

    void addSharer(NetDevice_t netID) {
      sharers.push_front(netID);
      //sharers.unique();
    }

    void rmSharer(NetDevice_t netID) {
      //FIXME: compare this to own implementation for performance.
      //       if size() is always small, then it is probably fine.
      sharers.remove(netID);
    }

    void printSharers(ulong addr) {
      std::list<NetDevice_t>::iterator it;
      fprintf(stderr,"[C %llu] addr: %lu state %X shr: ",globalClock,addr,ccState);
      for(it=sharers.begin(); it!=sharers.end(); it++)
	fprintf(stderr,"%d ",*it);

      fprintf(stderr,"\n");
    }

    NetDevice_t removeNextSharer() {
      NetDevice_t temp = sharers.front();
      sharers.pop_front();
      return temp;
    }

    NetDevice_t getNextSharer() const {
      return sharers.front();
    }

    bool empty() const {
      return sharers.empty();
    }

  };
  typedef std::list<CCActiveMsg *> CCActiveMsgQueue;
  typedef HASH_MAP<ulong, CCActiveMsgQueue *> BusyCCMsgHash;

protected:
  static pool <CCActiveMsg> ccActiveMsgPool; 
  static pool <CCActiveMsgQueue> ccActiveMsgQueuePool;

  BusyCCMsgHash busyMsgHash;

  CCActiveMsg *addMsgToBusyList(ulong addr, CacheCoherenceMsg *ccmsg, ushort acksRcvd=0) { 
    CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

    if(ccamq == NULL) {
      ccamq = ccActiveMsgQueuePool.out();
      busyMsgHash[addr&addrMask] = ccamq;
    } else if( ccamq->front()->ccmsg == ccmsg )
      return ccamq->front();

    CCActiveMsg *ccam = ccActiveMsgPool.out();
    ccam->init(ccmsg,acksRcvd);

    I(ccamq);
    ccamq->push_back(ccam);

    return ccam;
  }

  void removeFromQueueIfPresent(ulong addr, CacheCoherenceMsg *ccm) {
    CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
    if(ccamq!=NULL && ccamq->size() > 0) {
      if( ccamq->front()->ccmsg == ccm ) {
	removeFrontBusyMsgQueue(ccamq);
      } else {
	CCActiveMsgQueue::iterator it = ccamq->begin();
	while(it != ccamq->end()) {
	  if( (*it)->ccmsg == ccm ) {
	    ccActiveMsgPool.in(*it);
	    it = ccamq->erase(it);
	  } else
	    it++;
	}
      }
      if(ccamq->size() == 0)
	removeBusyMsgQueue(addr);
    }
  }

  CCActiveMsgQueue *getBusyMsgQueue(ulong addr) {
    addr = addr & addrMask;
    BusyCCMsgHash::iterator it = busyMsgHash.find(addr);
    if(it != busyMsgHash.end())
      return it->second;
    else
      return NULL;
  }

  CCActiveMsg *getFrontBusyMsgQueue(ulong addr) {
    CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
    if(ccamq != NULL && ccamq->size() > 0)
      return ccamq->front();
    else
      return NULL;
  }

  void removeFrontBusyMsgQueue(ulong addr) {
    addr = addr & addrMask;
    CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
    removeFrontBusyMsgQueue(ccamq);
  }

  void removeFrontBusyMsgQueue(CCActiveMsgQueue *ccamq) {
    I(ccamq->size() >= 1);
    ccActiveMsgPool.in(ccamq->front());
    ccamq->pop_front();
  }

  void removeBusyMsgQueue(ulong addr) {
    addr = addr & addrMask;
    CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
    if(ccamq->size() == 1)
      removeFrontBusyMsgQueue(ccamq);

    I(ccamq->size() == 0);
    ccActiveMsgQueuePool.in(ccamq);
    busyMsgHash.erase(addr);
  }

  bool lineIsBusy(ulong addr, CacheCoherenceMsg *ccmsg=NULL) {
    CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
    return ( ccamq!=NULL && ccamq->front()->ccmsg != ccmsg );
  }
 
public:

  CacheCoherenceProt(InterConnection *network, RouterID_t rID, PortID_t pID) 
    :ProtocolBase(network,rID,pID)  {

    addrMask = (32 - 1) ^ 0xffffffff;

    ProtocolCBBase *pcb;
      
    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::readHandler>(this);
    registerHandler(pcb,CCRd);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::writeHandler>(this);
    registerHandler(pcb,CCWr);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, 
                 &CacheCoherenceProt::writeBackHandler>(this);
    registerHandler(pcb,CCWrBack);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, 
                 &CacheCoherenceProt::writeBackAckHandler>(this);
    registerHandler(pcb,CCWrBackAck);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::invHandler>(this);
    registerHandler(pcb,CCInv);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::readAckHandler>(this);
    registerHandler(pcb,CCRdAck);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, 
                 &CacheCoherenceProt::readXAckHandler>(this);
    registerHandler(pcb,CCRdXAck);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::writeAckHandler>(this);
    registerHandler(pcb,CCWrAck);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::invAckHandler>(this);
    registerHandler(pcb,CCInvAck);

    pcb = new 
      ProtocolCB<CacheCoherenceProt, &CacheCoherenceProt::nackHandler>(this);
    registerHandler(pcb,CCNack);

  }

  virtual ~CacheCoherenceProt() {};

  virtual void readHandler(Message *m) = 0;
  virtual void writeHandler(Message *m) = 0;
  virtual void writeBackHandler(Message *m) = 0;
  virtual void invHandler(Message *m) = 0;
  virtual void nackHandler(Message *m) = 0;

  virtual void readAckHandler(Message *m) = 0;
  virtual void readXAckHandler(Message *m) = 0;
  virtual void writeAckHandler(Message *m) = 0;
  virtual void writeBackAckHandler(Message *m) = 0;
  virtual void invAckHandler(Message *m) = 0;

  static void StaticWriteAck(IntlMemRequest *ireq) {
    ((CacheCoherenceProt *)(ireq->getWorkData()))->writeAck(ireq);
  }

  static void StaticReadAck(IntlMemRequest *ireq) {
    ((CacheCoherenceProt *)(ireq->getWorkData()))->readAck(ireq);
  }

  static void StaticInvAck(IntlMemRequest *ireq) {
    ((CacheCoherenceProt *)(ireq->getWorkData()))->invAck(ireq);
  }

protected:

  virtual void readAck(IntlMemRequest *ireq) = 0;
  virtual void writeAck(IntlMemRequest *ireq) = 0;
  virtual void invAck(IntlMemRequest *ireq) = 0;
  
};

//This is the cache coherence protocol used in our multiprocessors.

class MPCacheCoherence : public CacheCoherenceProt {
public:

  class MPDirState : public DirectoryState {
  public:
    ushort rmLine;
    bool   rmLineState;
    ushort wtSharers;
    bool   newOwnerRq;

    void initialize(CacheGeneric<MPDirState, PAddr> *c) { MPDirState(); }
    
    bool operator==(MPDirState a) const {
      return ccState == a.ccState;
    }

    MPDirState()
      :DirectoryState()
      ,rmLine(0)
      ,rmLineState(false)
      ,wtSharers(0)
      ,newOwnerRq(false) {
    }

  };

  class MPDirCache {
  private:
    typedef CacheGeneric<MPDirState,PAddr> MPDirCacheType;

    MPDirCacheType *cache; 
    
  public:
    typedef CacheGeneric<MPDirState,PAddr>::CacheLine Line;

    MPDirCache(const char *section, const char *format, int i) {
      cache = MPDirCacheType::create(section, "", format, i);
    }


    Line *getLineToReplace(ulong addr) {

      Line *l;
      Line *keep=NULL;
      
      long i;
      
      ulong numLines = cache->getNumLines();

      for(i=(long)numLines-1; i >= 0; i--) {
	l = cache->getPLine(i);
	if(l->sharers.size()  == 0 
	   &&  l->rmLine      == 0 
	   &&  l->wtSharers   == 0 
	   &&  !l->rmLineState ) {
	  l->invalidate();
	} else if(l->ccState==MOD && l->rmLine==0 && l->wtSharers==0 && !l->rmLineState && !l->newOwnerRq) {
	  keep = l;
	} else if( keep==NULL && l->ccState==EX && l->rmLine==0 && l->wtSharers==0 && !l->rmLineState && !l->newOwnerRq) {
	  keep = l;
	} else if( keep==NULL && l->ccState==SHRD && l->rmLine==0 && l->wtSharers==0 && !l->rmLineState && !l->newOwnerRq && l->sharers.size() < 3 ) {
	  keep = l;
	}

	if(keep!=NULL) {
	  return keep;
	}
      }

      l = findLine(addr);
      return l;
    }

    Line *findLine(PAddr addr) {
      return cache->findLine(addr);
    }
    Line *readLine(PAddr addr){
      return cache->readLine(addr);
    }
    Line *writeLine(PAddr addr){
      return cache->writeLine(addr);
    }
    Line *fillLine(PAddr addr, PAddr &rplcAddr, bool ignoreLocked = false){
      return cache->fillLine(addr, rplcAddr, ignoreLocked);
    }
    Line *fillLine(PAddr addr) {
      return cache->fillLine(addr);
    }
    PAddr getLineAddr(Line *l) {
      return cache->calcAddr4Tag(l->getTag());
    }
  };

protected:
  MPCache *cache;
  PortGeneric *netPort;
  
  //FIXME:
  virtual void updateMsgQueue(ulong addr) = 0;
  virtual void reLaunch(ulong addr) = 0;

  ulong getMaskAddr(ulong addr) {
    return addr & addrMask;
  }

  virtual void reLaunchMsg(CacheCoherenceMsg *ccm) {
    switch(ccm->getType()){
    case CCRd:     readHandler(ccm);      break;
    case CCWr:     writeHandler(ccm);     break;
    case CCWrBack: writeBackHandler(ccm); break;
    default: GLOG(true,"type: %d",ccm->getType());I(0); exit(0);
    }
  }

  virtual void addSharer(ulong addr, CacheCoherenceMsg *ccm) = 0;
  virtual void addNewOwner(ulong addr, CacheCoherenceMsg *ccm) = 0;

public:

  MPCacheCoherence(InterConnection *network, RouterID_t rID, PortID_t pID) 
    :CacheCoherenceProt(network,rID,pID),cache(NULL)
    {

      char name[25];
      sprintf(name,"MPProtNetPort_%d",myID);
      netPort = PortGeneric::create(name,0,0);

      ProtocolCBBase *pcb;

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::newSharerHandler>(this);
      registerHandler(pcb,CCNewSharer);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::newOwnerHandler>(this);
      registerHandler(pcb,CCNewOwner);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::rmSharerHandler>(this);
      registerHandler(pcb,CCRmSharer);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::rmSharerAckHandler>(this);
      registerHandler(pcb,CCRmSharerAck);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::newSharerAckHandler>(this);
      registerHandler(pcb,CCNewSharerAck);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::newOwnerAckHandler>(this);
      registerHandler(pcb,CCNewOwnerAck);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::rmLineAckHandler>(this);
      registerHandler(pcb,CCRmLineAck);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::rmLineHandler>(this);
      registerHandler(pcb,CCRmLine);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::failedRdHandler>(this);
      registerHandler(pcb,CCFailedRd);

      pcb = new 
	ProtocolCB<MPCacheCoherence, 
	&MPCacheCoherence::failedWrHandler>(this);
      registerHandler(pcb,CCFailedWr);
    }

  //Used only by Master
  virtual void newSharerHandler(Message *m) = 0;
  virtual void newOwnerHandler(Message *m) = 0;
  virtual void rmSharerHandler(Message *m) = 0;
  virtual void rmLineAckHandler(Message *m) = 0;
  virtual void failedRdHandler(Message *m) = 0;
  virtual void failedWrHandler(Message *m) = 0;
  
  //Used Only by Slave
  virtual void newSharerAckHandler(Message *m) = 0;
  virtual void newOwnerAckHandler(Message *m) = 0;
  virtual void rmSharerAckHandler(Message *m) = 0;
  virtual void rmLineHandler(Message *m) = 0;

protected:

  void readAck(IntlMemRequest *ireq);
  void writeAck(IntlMemRequest *ireq);

  void evictRemoteReqs(ulong addr);

};

#endif
