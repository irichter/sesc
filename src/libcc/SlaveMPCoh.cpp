/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by James Tuck
                  Jose Renau

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
#include "SlaveMPCoh.h"

#if (defined DEBUG) && (DEBUG2==1)
#define DEBUGCCPROT 1
#define DEBUGREQ 1
#define DEBUGACK 1
//#define  HASH_DEBUG 1
#else
#define DEBUGCCPROT 0
#define DEBUGREQ 0
#define DEBUGACK 0
#endif

//This debugging variable tracks every address access. If the 
//coherence protocol is loosing messages, the address will show up in
//this hash table as an entry with > 0 outstanding entries.  Because
//it is extended from GStats, you can wait for the error to occur, then
//kill the program using a supported signal. Use HASH_DEBUG to turn it on.
ID(GDebugCC SlaveMPCoh::readReqHash("{Slave}readReqHash"));
ID(GDebugCC SlaveMPCoh::writeReqHash("{Slave}writeReqHash"));
ID(GDebugCC SlaveMPCoh::rmSharerReqHash("{Slave}rmSharerReqHash"));
ID(GDebugCC SlaveMPCoh::wrBackReqHash("{Slave}wrBackReqHash"));
ID(GDebugCC SlaveMPCoh::newOwnerReqHash("{Slave}newOwnerReqHash"));
ID(GDebugCC SlaveMPCoh::invReqHash("{Slave}invReqHash"));
ID(GDebugCC SlaveMPCoh::updateReqHash("{Slave}updateReqHash"));
ID(GDebugCC SlaveMPCoh::memReqHash("{Slave}memReqHash"));

ulong SlaveMPCoh::initCnt=0;

SlaveMPCoh::SlaveMPCoh(MemorySystem* current, 
		       InterConnection *network, RouterID_t rID,
		       const char *descr_section, const char *symbolic_name)
  :MPCacheCoherence(network,rID,LOCAL_PORT1), MemObj(descr_section, symbolic_name),
   readReqCnt("(S%lu)readReqCnt",initCnt),
   readAckCnt("(S%lu)readAckCnt",initCnt),
   writeReqCnt("(S%lu)writeReqCnt",initCnt),
   writeAckCnt("(S%lu)writeAckCnt",initCnt),
   wrBackCnt("(S%lu)wrBackCnt",initCnt),
   wrBackAckCnt("(S%lu)wrBackAckCnt",initCnt),
   remRdReqCnt("(S%lu)remRdReqCnt",initCnt),
   remRdAckCnt("(S%lu)remRdAckCnt",initCnt),
   remWrReqCnt("(S%lu)remWrReqCnt",initCnt),
   remWrAckCnt("(S%lu)remWrAckCnt",initCnt),
   invReqCnt("(S%lu)invReqCnt",initCnt)
{
  char name[50];
  initCnt++;
  sprintf(name,"SlaveMPCoh_CachePort_%d",myID);
  cachePort = PortGeneric::create(name,1,1);
}

SlaveMPCoh::~SlaveMPCoh()
{
}

// FIXME: move this to constructor
// FIXME: JAMES HELP!!!
MPCache::Line * SlaveMPCoh::getLineUpperLevel(PAddr addr) {
  if(cache == NULL) { 
    cache = (MPCache *)(upperLevel[0]);
  }
  return cache->findLine(addr);
}

void SlaveMPCoh::access(MemRequest *mreq) 
{
  MemOperation memOp = mreq->getMemOperation();

  GLOG(DEBUG2 && DEBUGCCPROT,"[C %llu] SlaveMPCoh::access addr %lu",
                             globalClock,
                             mreq->getPAddr()&addrMask);

#ifdef HASH_DEBUG
  memReqHash.inc(mreq->getPAddr()&addrMask);
#endif

  switch(memOp){
  case MemRead:     readReq(mreq);      break;
  case MemReadW:    writeReq(mreq);     break;
  case MemWrite:    writeBackReq(mreq); break;
  case MemRmSharer: rmSharerReq(mreq);  break;
  default:          I(0);               break;
  }
}

void SlaveMPCoh::returnAccess(MemRequest *mreq)
{ 
  I(0); 
}

Time_t SlaveMPCoh::getNextFreeCycle()
{
  //FIXME: model correctly
  return globalClock; 
}

void SlaveMPCoh::invalidate(PAddr addr, ushort size, MemObj *oc)
{
}

bool SlaveMPCoh::canAcceptStore(PAddr addr) const 
{ 
  return true; 
}

void SlaveMPCoh::readReq(MemRequest *mreq) {
  ulong addr = mreq->getPAddr()  ;
  NetDevice_t dstID = getHomeNodeID(addr);

  readReqCnt.inc();
#ifdef HASH_DEBUG
  readReqHash.inc(addr&addrMask);
#endif

  CacheCoherenceMsg *ccmsg = 
    CacheCoherenceMsg::createCCMsg(this, CCRd, dstID, addr, mreq);

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::readReq AT(addr):%lu ON(myID):%d ID:%lu",
       globalClock, addr&addrMask,(int) myID, ccmsg->getXActionID());

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  //Regardless, must update list because the message cannot be 
  //be serviced immediately
  CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg);

  //Signifies request originates from this L1 memory (lowerLevel[0])
  ccam->localReq = true;

  if(ccamq == NULL) {
    //If there are no messages currently waiting
    GLOG(DEBUGREQ,"[C %llu] SlaveMPCoh::readReq AT(addr):%lu ON(myID):%d To NID:%d MsgID:%lu",
    globalClock,addr&addrMask,(int)myID,getHomeNodeID(addr)-16,ccmsg->getXActionID());

    //Notifies reLaunch that this is already launched
    ccam->launched = true;

    //Send Message
    sendMsgAbs(netPort->nextSlot(), ccmsg);
  } 
}

void SlaveMPCoh::writeReq(MemRequest *mreq) {
  ulong addr = mreq->getPAddr()  ;

  writeReqCnt.inc();
#ifdef HASH_DEBUG
  writeReqHash.inc(addr&addrMask);
#endif
  
  NetDevice_t dstID = getHomeNodeID(addr);

  CacheCoherenceMsg *ccmsg = 
    CacheCoherenceMsg::createCCMsg(this, CCWr, dstID, addr, mreq);

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::writeReq addr %lu netID %d MsgID:%lu",
       globalClock,addr&addrMask,(int)myID,ccmsg->getXActionID());

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  //Update busy message list
  CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg);
  //locally generated message
  ccam->localReq = true;

  //If no pending messages, go ahead and launch
  if(ccamq == NULL) {
    GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::writeReq addr %lu netID %d MsgID:%lu",
	 globalClock,addr&addrMask,(int)myID,ccmsg->getXActionID());
    ccam->launched = true;
    sendMsgAbs(netPort->nextSlot(),ccmsg);
  }
}


void SlaveMPCoh::writeBackReq(MemRequest *mreq) {
  ulong addr = mreq->getPAddr();

  wrBackCnt.inc();

  NetDevice_t dstID = getHomeNodeID(addr);

  CacheCoherenceMsg *ccmsg = 
    CacheCoherenceMsg::createCCMsg(this, CCWrBack, dstID, addr, mreq);

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::writeBackReq addr %lu myID %d MsgId:%lu",
       globalClock,addr&addrMask,myID,ccmsg->getXActionID());


  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  //If there is a pending rmLine, void the write backd
  //This exhaustive search may be redundant...check
  //the way/when other parts check the list

  if(ccamq != NULL) {
    evictRemoteReqs(addr);
    CCActiveMsgQueue::iterator it = ccamq->begin();
    while(it!=ccamq->end()) {
      if( (*it)->mtype == CCRmLine ) {
	ccmsg->garbageCollect();
#ifdef HASH_DEBUG
	memReqHash.dec(addr&addrMask);
#endif
	mreq->ack(0);
	return;
      }
      it++;
    }
  }

  CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg);
  ccam->localReq = true; 

#ifdef HASH_DEBUG
  wrBackReqHash.inc(addr&addrMask);
#endif

  if(ccamq == NULL) {
    GLOG(DEBUGREQ,"[C %llu] SlaveMPCoh::writeBackReq addr %lu myID %d MsgID:%lu",
	 globalClock,addr&addrMask,myID,ccmsg->getXActionID());
    ccam->launched = true;
    sendMsgAbs(netPort->nextSlot(),ccmsg);
  } 
}

//obsolete
void SlaveMPCoh::waitingWrBack(CacheCoherenceMsg *ccm) {
  ulong addr = ccm->getDstAddr();
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam->ccmsg == ccm);
  ccam->launched = true;
  sendMsgAbs(netPort->nextSlot(),ccm);
}

void SlaveMPCoh::rmSharerReq(MemRequest *mreq) {
  ulong addr = mreq->getPAddr();
  NetDevice_t dstID = getHomeNodeID(addr);

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::rmSharerReq addr %lu myID %d",
       globalClock,addr&addrMask,myID);

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  if(ccamq != NULL) {
    evictRemoteReqs(addr);
    CCActiveMsgQueue::iterator it = ccamq->begin();
    it++;
    while(it!=ccamq->end()) {
      MessageType mtype = (*it)->mtype;
      if( mtype == CCRmLine || mtype == CCInv ) {
#ifdef HASH_DEBUG
	memReqHash.dec(addr&addrMask);
#endif 
	mreq->ack(0); //ack rmSharerReq, not needed
	return;
      }
      it++;
    }
  }


#ifdef HASH_DEBUG
  rmSharerReqHash.inc(addr&addrMask);
#endif

  CacheCoherenceMsg *ccmsg = 
    CacheCoherenceMsg::createCCMsg(this, CCRmSharer, dstID, addr, mreq);

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::rmSharerReq addr %lu myID %d MsgId %lu",
       globalClock,addr&addrMask,myID,ccmsg->getXActionID());

  CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg);
  ccam->localReq = true;

  if(ccamq == NULL) {

    GLOG(DEBUGREQ,"[C %llu] SlaveMPCoh::rmSharerReq addr %lu myID %d LAUNCHED MsgID:%lu",
	 globalClock,addr&addrMask,myID,ccmsg->getXActionID());
    sendMsgAbs(netPort->nextSlot(),ccmsg);

    ccam->launched = true;
  }
}

//obsolete
void SlaveMPCoh::waitingRmSharer(CacheCoherenceMsg *ccm) {
  ulong addr = ccm->getDstAddr();
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam->ccmsg == ccm);
  ccam->launched = true;
  sendMsgAbs(netPort->nextSlot(),ccm);
}

/****************************************************************
 *SlaveMPCoh::readHandler
 *  Handles incoming CCRd meaning that it is currently
 *  holding a line, and another node 
 *  wishes to obtain a copy of that node for sharing
 *
 *  We must be careful of the case that a CCRmSharer has
 *  already been sent to L2h, but no Ack recieved yet.
 ****************************************************************/
void SlaveMPCoh::readHandler(Message *m) {

  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  ulong addr = ccm->getDstAddr()  ;

  GLOG(DEBUGCCPROT,
       "[C %llu] SlaveMPCoh::readHandler  adr %lu myID %d orig %d msgID %lu",
       globalClock,addr&addrMask,(int)myID,ccm->getOriginID(),
       ccm->getXActionID());

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);  

  MPCache::Line *l = getLineUpperLevel(addr);

  if( l == NULL || (l!=NULL && l->isLocked())) { 

    GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::readHandler Line not Found!! addr %lu msgID %lu",
	 globalClock,addr&addrMask,ccm->getXActionID());
    removeFromQueueIfPresent(addr,ccm);

    ccm->transform2CCFailedRd(); 
    sendMsgAbs(netPort->nextSlot(),ccm); 

    reLaunch(addr);
    return;
  }
  
  I(l);
  //If there is a pending request that has not yet populated the
  //cache, like a pending write waiting for acks, then we want
  //to wait until we get all the acks.

  if( lineIsBusy(addr,ccm) ) {
    addMsgToBusyList(addr,ccm);
    return;
  }

  remRdReqCnt.inc();

  addSharer(addr,ccm);
  CCActiveMsg *ccam = addMsgToBusyList(addr,ccm);
  ccam->numSharers+=2;
  ccam->launched = true;
  ccm->transform2CCRdAck();

#ifdef HASH_DEBUG
  updateReqHash.inc(addr&addrMask);
#endif

  IntlMemRequest::create(upperLevel[0],addr,MemUpdate,
			 cachePort->nextSlotDelta()+1,
			 &CacheCoherenceProt::StaticReadAck,this);
}

void SlaveMPCoh::readWaitingHandler(CacheCoherenceMsg *ccm) 
{
  ulong addr = ccm->getDstAddr()  ;
  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::readWaitingHandler  addr %lu netID %d dstID %d origin %d MsgID:%lu",globalClock,addr&addrMask,(int)myID,(int)ccm->getDstNetID(),ccm->getOriginID(),ccm->getXActionID());
  
  MPCache::Line *l = getLineUpperLevel(addr);

  if( l == NULL || ( l !=NULL && l->isLocked()) ) {
    ccm->transform2CCFailedRd();
    sendMsgAbs(netPort->nextSlot(),ccm);
    updateMsgQueue(addr);
    return;
  }

  remRdReqCnt.inc();

  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  ccam->launched = true;
  ccam->numSharers += 2;
  ccm->transform2CCRdAck();

#ifdef HASH_DEBUG
  updateReqHash.inc(addr&addrMask);
#endif

  IntlMemRequest::create(upperLevel[0],addr,MemUpdate,
			 netPort->nextSlotDelta(),
			 &CacheCoherenceProt::StaticReadAck,this);
  addSharer(addr,ccm);
}


/****************************************************************
 *SlaveMPCoh::writeHandler
 *  Handles incoming CCOwnerWr meaning that it is currently
 *  holding a line in modified state, and another node 
 *  wishes to obtain an exclusive copy for writing
 *
 *  Definitely different from Master
 ****************************************************************/
void SlaveMPCoh::writeHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);
  I(ccm);
  ulong addr = ccm->getDstAddr();

  GLOG(DEBUGCCPROT,
       "[C %llu] SlaveMPCoh::writeHandler addr %lu myID %d REQ %d msgID %lu",
       globalClock,addr&addrMask,(int)myID,(int)ccm->getOriginID(),ccm->getXActionID());


  MPCache::Line *l = getLineUpperLevel(addr);
  //This could be a source of bugs...
  if(l == NULL || (l!=NULL && (l->isLocked()))) {
   
    GLOG(DEBUGCCPROT,
	 "[C %llu] SlaveMPCoh::writeHandler addr %lu MsgID:%lu LINE NOT FOUND OR IS SHARED",
	 globalClock,addr&addrMask,ccm->getXActionID());

    ccm->transform2CCFailedWr();  //FIXME: make sure transform is correct!
    sendMsgAbs(netPort->nextSlot(),ccm);

    removeFromQueueIfPresent(addr,ccm);
    return;
  }

  if(lineIsBusy(addr,ccm)) {
    addMsgToBusyList(addr,ccm);
    return;
  }

  remWrReqCnt.inc();

  addNewOwner(addr,ccm);
  CCActiveMsg *ccam = addMsgToBusyList(addr, ccm);
  ccam->launched = true;
  ccam->numSharers+=2; //1 for newOwnerReq and 1 for MemInvalidate

  if(ccm->invsExpected())
    ccm->transform2CCWrAck(ccm->getNumAcks());
  else
    ccm->transform2CCWrAck(1);

  IntlMemRequest::create(upperLevel[0],addr,MemInvalidate,
			 cachePort->nextSlotDelta()+1, 
			 &CacheCoherenceProt::StaticWriteAck, this);
}

/*
 * writeBackHandler: since not a home, can never recieve a writeback
 */
void SlaveMPCoh::writeBackHandler(Message *m) { 
  I(0);
  GLOG(DEBUGCONDITION,
       "[C %llu] SlaveMPCoh::writeBackHandler SHOULD NEVER BE CALLED!",
       globalClock);
}


/****************************************************************
 *SlaveMPCoh::invHandler
 *  Handles incoming CCInv. Invalidates dstAddr from upper levels
 *  of memory.
 *
 ****************************************************************/
void SlaveMPCoh::invHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);
  MessageType mtype = ccm->getType();
  I( mtype == CCInv );
  ulong addr = ccm->getDstAddr();

  invReqCnt.inc();

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::invHandler addr %lu myID %d MsgId %lu",
       globalClock,addr&addrMask,myID,ccm->getXActionID());

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  if(lineIsBusy(addr,ccm)) {
    CCActiveMsg *ccam = ccamq->front();
    
    if(ccam->mtype == CCRmSharer) {
      GLOG(DEBUGCCPROT,"[C %llu] invHandler status:CCRmSharer addr: %lu MsgId %lu",
	   globalClock,addr&addrMask,ccm->getXActionID());
      invStaleRdAck(ccm);
      if(ccam->staleRmSharer) {
	ccam->ccmsg->garbageCollect();
	updateMsgQueue(addr);
      } else
	ccam->staleRmSharer = true;
    } else if(ccam->mtype == CCWrBack) { 
      GLOG(DEBUGCCPROT,"[C %llu] invHandler status:CCWrBack addr: %lu MsgId %lu",
	   globalClock,addr&addrMask,ccm->getXActionID());
      invStaleRdAck(ccm);
    } else if( ccam->mtype == CCWr ) {

      ccam = ccActiveMsgPool.out();
      ccam->launched = true;
      ccam->init(ccm);
      ccamq->push_front(ccam);

#ifdef HASH_DEBUG
      invReqHash.inc(addr&addrMask);
#endif

      IntlMemRequest::create(upperLevel[0],addr, MemInvalidate, 
			     cachePort->nextSlotDelta()+1, 
			     &SlaveMPCoh::StaticInvAck, this);
    } else if( ccam->mtype == CCRd ) {
      GLOG(DEBUGCCPROT,"[C %llu] invHandler status:CCRd addr: %lu MsgId:%lu",
	   globalClock,addr&addrMask,ccm->getXActionID());
      if( ccam->localReq ) {
	ccam->staleRd = true;
      } else {
	IntlMemRequest::create(upperLevel[0],addr, MemInvalidate, 
			       cachePort->nextSlotDelta()+1);
      }
      //FIXME: maybe???
      //Note, this invalidate with no return adds some inprecision
      //into the memory subsystem. If we are very persnickity about
      //memory consistency models, then this is incorrect. But, it
      //happens so infrequently, that even in that case, I don't
      //think it would affect performance or correctness very much.

      invStaleRdAck(ccm);
    } else {
      addMsgToBusyList(addr,ccm);
    }
  } else {   
    CCActiveMsg *ccam = addMsgToBusyList(addr, ccm);
    ccam->launched = true;

#ifdef HASH_DEBUG
    invReqHash.inc(addr&addrMask);
#endif

    IntlMemRequest::create(upperLevel[0],addr, MemInvalidate, 
			   cachePort->nextSlotDelta()+1, 
			   &SlaveMPCoh::StaticInvAck, this);
  }  
}

/****************************************************************
 *SlaveMPCoh::readAckHandler
 *  Handles satisfaction of issued read request via return
 *  of a CCRdAck.  This may come either from the home node
 *  or from a dirty node.
 ****************************************************************/
void SlaveMPCoh::readAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  ulong addr = ccm->getDstAddr()&addrMask;

  readAckCnt.inc();
#ifdef HASH_DEBUG
  readReqHash.dec(addr);
#endif

  GLOG(DEBUGACK,"[C %llu] SlaveMPCoh::readAckHandler [addr %lu][myID %d][msgID %lu] dst %d src %d orig %d",globalClock, addr&addrMask,myID,ccm->getXActionID(),ccm->getDstNetID(), ccm->getSrcNetID(),ccm->getOriginID());

  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I( ccam );
  I( ccam->ccmsg == ccm );
  I( ccm->getMemRequest() );

#ifdef HASH_DEBUG
  memReqHash.dec(ccm->getMemRequest()->getPAddr()&addrMask);
#endif 

  ccm->getMemRequest()->goUpAbs(cachePort->nextSlot()+1); 

  //FIXME: 
  //We need to do this a little better. This will work, but, it
  //would be cleaner if there were a way to send a single message
  //that would prevent caching. Note, we cannot "not Ack" the the
  //upper level cache due to internal bookkeeping in the cache.
  if( ccam->staleRd ) {
    IntlMemRequest::create(upperLevel[0],addr, MemInvalidate, 
			   cachePort->nextSlotDelta()+1);
  } 

  ccm->garbageCollect();
  
  updateMsgQueue(addr);
}

/****************************************************************
 *SlaveMPCoh::readXAckHandler
 *  Handles satisfaction of issued read request via return
 *  of a CCRdXAck.  This may come from home or previous owner 
 *  node.
 ****************************************************************/
void SlaveMPCoh::readXAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  MessageType mtype = ccm->getType();
  ulong addr = ccm->getDstAddr();

  readAckCnt.inc();
#ifdef HASH_DEBUG
  readReqHash.dec(addr&addrMask);
#endif
  GLOG(DEBUGACK,"[C %llu] SlaveMPCoh::readXAckHandler [addr %lu][msgID %lu][myID %d][srcID %d][srcPort %d][dstPort %d][srcRID %d][dstRID %d]",
       globalClock,addr&addrMask,ccm->getXActionID(),myID,ccm->getSrcNetID(),ccm->getSrcPortID(),ccm->getDstPortID(),getRouterID(),getProtocolBase(ccm->getSrcNetID())->getRouterID());

  I( ccm->getMemRequest() );
#ifdef HASH_DEBUG
  memReqHash.dec(ccm->getMemRequest()->getPAddr()&addrMask);
#endif 

  ccm->getMemRequest()->goUpAbs(cachePort->nextSlot()+1); 
  ccm->garbageCollect();

  updateMsgQueue(addr);
}

//FIXME: need to setup structure appropriately!!!
void SlaveMPCoh::writeAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  MessageType mtype = ccm->getType();
  ulong addr = ccm->getDstAddr()  ;
  
  writeAckCnt.inc();
#ifdef HASH_DEBUG
  writeReqHash.dec(addr&addrMask);
#endif

  GLOG(DEBUGACK,
       "[C %llu] SlaveMPCoh::writeAckHandler addr%lu myID%d REQ%d msgID%lu numAcks %d",
       globalClock,addr&addrMask,myID,ccm->getOriginID(),ccm->getXActionID(),ccm->getNumAcks());

  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam);
  I(ccm->getNumAcks() >= 1);
  ccam->acksRcvd += ccm->getNumAcks();
  ccam->acksRcvd--;

  if(ccam->acksRcvd == 0) { 
    I( ccm->getMemRequest() );  
#ifdef HASH_DEBUG
    memReqHash.dec(ccm->getMemRequest()->getPAddr()&addrMask);
#endif 
    ccm->getMemRequest()->goUpAbs(cachePort->nextSlot()+1); //FIXME: timing
    ccm->garbageCollect();
    updateMsgQueue(addr);
  } else {
    ccam->doNotRemoveFromQueue = false;
  }
}

void SlaveMPCoh::writeBackAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  ulong addr = ccm->getDstAddr()  ;
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);

  wrBackAckCnt.inc();

  GLOG(DEBUGACK,"[C %llu] SlaveMPCoh::writeBackAckHandler addr %lu msgID %lu",
       globalClock,addr&addrMask,ccm->getXActionID());
#ifdef HASH_DEBUG
  memReqHash.dec(ccm->getMemRequest()->getPAddr()&addrMask);
#endif 

  ccm->getMemRequest()->ack(0);

#ifdef HASH_DEBUG
  wrBackReqHash.dec(addr&addrMask);
#endif

  updateMsgQueue(addr);  
  ccm->garbageCollect();
}

//must handle incoming invalidates
void SlaveMPCoh::invAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  ulong addr = ccm->getDstAddr()  ;
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);

  GLOG(DEBUGCCPROT,
  "[C %llu] SlaveMPCoh::invAckHandler adr%lu srcID%d myID%d ACKS%d msgID %lu",
  globalClock, addr&addrMask, ccm->getSrcNetID(),myID,ccam->acksRcvd,ccm->getXActionID());

  I(ccam);
  I(ccam->ccmsg);
  ccam->acksRcvd--;
  if(ccam->acksRcvd == 0 && !ccam->doNotRemoveFromQueue) {
#ifdef HASH_DEBUG
    memReqHash.dec(ccam->ccmsg->getMemRequest()->getPAddr()&addrMask);
#endif 

    ccam->ccmsg->getMemRequest()->goUpAbs(cachePort->nextSlot()+1);
    ccam->ccmsg->garbageCollect(); //Put off garbageCollect to last possible time   
    updateMsgQueue(addr);   //to make compatible with pool class
  }
  ccm->garbageCollect();
} 

void SlaveMPCoh::newSharerAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);

  ulong addr = ccm->getDstAddr()  ;
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam);

  GLOG(DEBUGACK,
       "[C %llu] SlaveMPCoh::newSharerAckHandler addr %lu myID %d msgID %lu orig %d",
       globalClock,addr&addrMask,myID,ccm->getXActionID(),ccm->getOriginID());

  ccam->numSharers--;
  ccm->garbageCollect();

  if(ccam->numSharers == 0) {
    remRdAckCnt.inc();
    sendMsgAbs(netPort->nextSlot(),ccam->ccmsg);
    updateMsgQueue(addr);
  }
}

void SlaveMPCoh::readAck(IntlMemRequest *ireq) {
  ulong addr = ireq->getPAddr();

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  I(ccamq);
  CCActiveMsg *ccam = ccamq->front();
  I(ccam);
  CacheCoherenceMsg *ccm = ccam->ccmsg;
  GLOG(DEBUGCCPROT,
       "[C %llu] CCM3TBaseProt::readAck addr %lu origin %d myID %d MsgId:%lu",
       globalClock,addr&addrMask,ccam->ccmsg->getOriginID(),myID,ccam->ccmsg->getXActionID());
  
  ccam->numSharers--;

#ifdef HASH_DEBUG
      updateReqHash.dec(addr&addrMask);
#endif

  if(ccam->numSharers == 0) {
    remRdAckCnt.inc();
    sendMsgAbs(netPort->nextSlot(), ccm);
    updateMsgQueue(addr); //performs cleanup if needed
  }
}

void SlaveMPCoh::writeAck(IntlMemRequest *ireq) {
  ulong addr = ireq->getPAddr();
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam);
  GLOG(DEBUGCCPROT,"[C %llu] CCM3TBaseProt::writeAck %lu myId %d origin %d MsgID %lu",
       globalClock,addr&addrMask,myID,ccam->ccmsg->getOriginID(),ccam->ccmsg->getXActionID());

  ccam->numSharers--;
  
  if(ccam->numSharers==0) {
    remWrAckCnt.inc();
    sendMsgAbs(netPort->nextSlot(),ccam->ccmsg);
    updateMsgQueue(addr);
  }
}

void SlaveMPCoh::newOwnerAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);  
  ulong addr = ccm->getDstAddr();
  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::newOwnerAckHandler addr %lu netID %d origin %d msgID %lu",globalClock,addr&addrMask,myID,ccm->getOriginID(),ccm->getXActionID());

  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  ccam->numSharers--;
  ccm->garbageCollect();

#ifdef HASH_DEBUG
  newOwnerReqHash.dec(addr&addrMask);
#endif

  if(ccam->numSharers==0) {
    remWrAckCnt.inc();
    sendMsgAbs(netPort->nextSlot(),ccam->ccmsg);
    updateMsgQueue(addr);
  }
}

void SlaveMPCoh::addSharer(ulong addr, CacheCoherenceMsg *ccm) {
  CacheCoherenceMsg *newshr=0;
  newshr = CacheCoherenceMsg::createCCMsg(this, CCNewSharer, 
					  getHomeNodeID(addr), ccm);
  sendMsgAbs(netPort->nextSlot(), newshr);  
}

void SlaveMPCoh::readXAck(CacheCoherenceMsg *ccm) {
  I(0);
  GLOG(DEBUGCCPROT,"SlaveMPCoh::readXAck SHOULD NEVER BE CALLED!");
}

void SlaveMPCoh::addNewOwner(ulong addr, CacheCoherenceMsg *ccm) {
#ifdef HASH_DEBUG
  newOwnerReqHash.inc(addr&addrMask);
#endif

  CacheCoherenceMsg *newOwner = 
    CacheCoherenceMsg::createCCMsg(this,CCNewOwner,getHomeNodeID(addr),ccm);
  sendMsgAbs(netPort->nextSlot(), newOwner);
}

//Slave nodes can't send writeback acks, because they
//can't recieve writebacks.
void SlaveMPCoh::writeBackAck(CacheCoherenceMsg *ccm) {
  I(0);
  GLOG(DEBUGCCPROT,"SlaveMPCoh::writeBackAck SHOULD NEVER BE CALLED!");
}

void SlaveMPCoh::invAck(IntlMemRequest *ireq) {
  ulong addr = ireq->getPAddr();

  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::invAck %lu FROM %d TO %d MsgID %lu",
       globalClock,addr&addrMask,myID,ccam->ccmsg->getOriginID(),ccam->ccmsg->getXActionID());

#ifdef HASH_DEBUG
  invReqHash.dec(addr&addrMask);
#endif

  ccam->ccmsg->transform2CCInvAck();
  sendMsgAbs(netPort->nextSlot(), ccam->ccmsg);

  //TODO: add ability to remove pending rmSharer from waiting list!!

  updateMsgQueue(addr);
}

void SlaveMPCoh::invStaleRdAck(CacheCoherenceMsg *ccm) {
  ulong addr = ccm->getDstAddr();
  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::invStaleRdAck addr %lu myID %d MsgId %lu",
       globalClock,addr&addrMask,myID,ccm->getXActionID());
  ccm->transform2CCInvAck();
  sendMsgAbs(netPort->nextSlot(), ccm);
}

void SlaveMPCoh::reLaunch(ulong addr) {
  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::reLaunch addr %lu myID %d",
       globalClock,addr&addrMask,myID);

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  if(ccamq == NULL)
    return;

  I(ccamq);
  I( (ccamq->size() >= 1) );
    
  if(ccamq->front()->launched)
    return;

  CacheCoherenceMsg *newmsg = ccamq->front()->ccmsg;

  if(ccamq->front()->localReq) {

    I(!ccamq->front()->launched);
    
    if(ccamq->front()->mtype == CCWr)
      evictRemoteReqs(addr);

    ccamq->front()->launched = true;
    sendMsgAbs(netPort->nextSlot(),newmsg); //jt hack
    //netPort->occupyUntil(netPort->nextSlot()+32);

    return;
  }

  switch(newmsg->getType()) {
  case CCInv : invHandler(newmsg); break;
  case CCWr: writeHandler(newmsg); break;
  case CCRd: readWaitingHandler(newmsg); break;
  case CCRmLine: waitingRmLineMsg(newmsg); break;
  default: I(0); printf("Undefined reLaunch at %d\n",__LINE__); exit(0);break;
  }
}

void SlaveMPCoh::rmSharerAckHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccm->getDstAddr();

  GLOG(DEBUGACK,"[C %llu] SlaveMPCoh::rmSharerAckHandler addr %lu myID %d MsgID %lu",
       globalClock,(addr&addrMask),myID,ccm->getXActionID());

  ID(CCActiveMsg *ccam = getFrontBusyMsgQueue(addr));
  I(ccam);
  I(ccam->ccmsg == ccm);
  
#ifdef HASH_DEBUG
  rmSharerReqHash.dec(addr&addrMask);
#endif
#ifdef HASH_DEBUG
  memReqHash.dec(ccm->getMemRequest()->getPAddr()&addrMask);
#endif 

  ccm->getMemRequest()->goUpAbs(cachePort->nextSlot()+1);
  ccm->garbageCollect(); //jmt 2-07

  updateMsgQueue(addr);
}

void SlaveMPCoh::updateMsgQueue(ulong addr) {
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  I(ccamq);
  I(ccamq->size());

  GLOG(DEBUGCCPROT,
       "[C %llu] SlaveMPCoh::updateMsgQueue at %lu ON(myID) %d size %d numSharers %d",
       globalClock,addr&addrMask,myID,ccamq->size(),ccamq->front()->numSharers);

  if(ccamq->front()->numSharers == 0) {   
    removeFrontBusyMsgQueue(ccamq);  
    if( ccamq->size() == 0) {
      removeBusyMsgQueue(addr);
    } else
      reLaunch(addr);
  }
}

void SlaveMPCoh::rmLineHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);  
  ulong addr = ccm->getDstAddr();
  
  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::rmLineHandler adr %lu ON %d MsgId %lu",
       globalClock,addr&addrMask,myID,ccm->getXActionID());

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  CCActiveMsgQueue::iterator it;

  if(ccamq == NULL) {
    CCActiveMsg *ccam = addMsgToBusyList(addr,ccm);
    ccam->launched = true;
    IntlMemRequest::create(upperLevel[0],addr,MemInvalidate,
			   cachePort->nextSlotDelta()+1,
			   &SlaveMPCoh::StaticRmLineAck,this);
  } else {
    it = ccamq->begin();
    CCActiveMsg *ccam = ccActiveMsgPool.out();
    ccam->init(ccm);
    it++;
    ccamq->insert(it,ccam);
  }
}

void SlaveMPCoh::waitingRmLineMsg(CacheCoherenceMsg *ccm) {
  ulong addr = ccm->getDstAddr();
  
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  ccam->launched = true;

  IntlMemRequest::create(upperLevel[0],addr&addrMask,MemInvalidate,
			 cachePort->nextSlotDelta()+1,
			 &SlaveMPCoh::StaticRmLineAck,this);
}

void SlaveMPCoh::StaticRmLineAck(IntlMemRequest *ireq) {
  ((SlaveMPCoh *)(ireq->getWorkData()))->rmLineAck(ireq);
}

void SlaveMPCoh::rmLineAck(IntlMemRequest *ireq) {
  ulong addr = ireq->getPAddr();
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  CCActiveMsgQueue::iterator it = ccamq->begin();
  CacheCoherenceMsg *ccm = (*it)->ccmsg;
  I(ccm->getType() == CCRmLine);
  ccm->transform2CCRmLineAck();

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::rmLineAck adr %lu FROM %d MsgId %lu",globalClock,addr&addrMask,myID,ccm->getXActionID());

  it++; //skip over first entry
  
  //send nacks to everything behind me in the queue that could
  //create a cyclic dependence
  while(it != ccamq->end()) {
    if((*it)->localReq == false) {
      ccActiveMsgPool.in(*it);
      if( (*it)->ccmsg->getType() == CCRd ) {
	(*it)->ccmsg->transform2CCFailedRd();
      } else if( (*it)->ccmsg->getType() == CCWr ) {
	(*it)->ccmsg->transform2CCFailedWr();
      } else {
	I(0);
	exit(0);
	(*it)->ccmsg->transform2CCNack();
      }
      sendMsgAbs(netPort->nextSlot(),(*it)->ccmsg);
      it = ccamq->erase(it);
    } else if ( (*it)->localReq == true && 
		     ( (*it)->ccmsg->getType() == CCWrBack || 
		       (*it)->ccmsg->getType() == CCRmSharer )  ){
      (*it)->ccmsg->garbageCollect();
      ccActiveMsgPool.in(*it);
      it = ccamq->erase(it);
    } else {
      it++;
    }
  }

  sendMsgAbs(netPort->nextSlot(), ccm);
  updateMsgQueue(addr);
} 

void SlaveMPCoh::nackHandler(Message *m) {
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);  
  ulong addr = ccm->getDstAddr();
  NetDevice_t dstID = getHomeNodeID(addr);

  GLOG(DEBUGCCPROT,"[C %llu] SlaveMPCoh::nackHandler adr%lu On %d MsgId %lu",
       globalClock,addr&addrMask,myID,ccm->getXActionID());
  
  I(getBusyMsgQueue(addr));
  I(getBusyMsgQueue(addr)->front());
  I(getBusyMsgQueue(addr)->front()->ccmsg == ccm);


  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  CCActiveMsgQueue::iterator it = ccamq->begin();
  CCActiveMsg *ccam = *it;
  it++;


  IS( 
       if (globalClock - ccam->start_time > 100000) {
         GLOG(true,"[C %llu] nackHandler OVER 100000 waiting on %lu since %llu MSGID %lu",globalClock,addr&addrMask,ccam->start_time,ccm->getXActionID()); 
	 exit(1);
       }
     );


  MessageType mtype;

  I(ccam->mtype != CCRmSharer);

  /*if(ccam->mtype == CCRmSharer) {
    rmSharerNack(ccm);
    I(0);
    exit(0);
    return;
    }*/
    
  switch(ccam->mtype){
  case CCRd:     mtype = CCRd;       break;
  case CCWr:    
    mtype = CCWr;       
    if( ccm->invsExpected() ) {
      ccam->acksRcvd += ccm->getNumAcks();
      ccam->acksRcvd--;

      ccm->setNumAcks(0);
      ccm->clrInvsExpected();

      if( ccam->acksRcvd != 0 ) {
	ccam->doNotRemoveFromQueue = true;
      }
    }

    break;
  default: printf("mtype is %d\n",ccam->mtype);exit(0);
  };

  ccm->reSend(mtype,dstID);
  //ccm->reSend(ccam->mtype,dstID);

  if( it != ccamq->end() && (*it)->ccmsg->getType() == CCRmLine ) {
    CCActiveMsg *ccamtmp = *it;   
    ccamq->erase(it);
    ccamq->insert(ccamq->begin(),ccamtmp);
    ccam->launched = false;
    reLaunch(addr);
  } else {

    //Look for extra entries that could be causing deadlock...
    //i.e. remote read requests...
    if( ccamq->size() > 1 ) {
      while(it!= ccamq->end()) {
	if( (*it)->mtype == CCRd && !(*it)->localReq ) {
	  (*it)->ccmsg->transform2CCFailedRd();
	  sendMsgAbs(netPort->nextSlot(),(*it)->ccmsg);
	  ccActiveMsgPool.in(*it);
	  it = ccamq->erase(it);
	} else
	  it++;
      }
    }

    if(ccam->nackBackOff >= 128)
      ccam->nackBackOff = 0;

    if(ccam->nackBackOff == 0)
      ccam->nackBackOff = 1;
    else
      ccam->nackBackOff *= 2;

    // Comment to James:
    // In the new API for Port (also in the old one). The following code would block the port
    // for a given number of cycles. Notice that this port becomes block for all kind of traffic.
    // Are you sure that this is the desired functionality?
    I(ccam->nackBackOff < MaxDeltaTime);
    //netPort->lock4nCycles(ccam->nackBackOff);
    sendMsgAbs(netPort->nextSlot()+ccam->nackBackOff, ccm);
  }
}

//This can only happen if the directory state has changed
//unexpectedly to modified. This means an CCInv is in route
//or has already arrived.
//obsolete
void SlaveMPCoh::rmSharerNack(CacheCoherenceMsg *ccm) {
  ulong addr = ccm->getDstAddr();
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  
  if(ccam->staleRmSharer) {
    updateMsgQueue(addr);
    ccm->garbageCollect();
  } else
    ccam->staleRmSharer = true;
}
