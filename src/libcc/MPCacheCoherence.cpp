
#include "CacheCoherence.h"

#if (defined DEBUG) && (DEBUG2==1)
#define DEBUGCCPROT 1
#else
#define DEBUGCCPROT 0
#endif

//Assuming ccm has already been converted
void MPCacheCoherence::readAck(IntlMemRequest *ireq) 
{
  ulong addr = ireq->getPAddr();

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  I(ccamq);
  CCActiveMsg *ccam = ccamq->front();
  I(ccam);
  CacheCoherenceMsg *ccm = ccam->ccmsg;
  GLOG(DEBUGCCPROT,
       "[C %llu] MPCacheCoherence::readAck addr %lu origin %d myID %d",
       globalClock,addr&addrMask,ccam->ccmsg->getOriginID(),myID);
  
  sendMsgAbs(netPort->nextSlot(), ccm);
  ccam->numSharers++;
  ccam->ccmsg = NULL;

  updateMsgQueue(addr); //performs cleanup if needed
}


void MPCacheCoherence::writeAck(IntlMemRequest *ireq) 
{
  ulong addr = ireq->getPAddr();
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam);
  GLOG(DEBUGCCPROT,"[C %llu] MPCacheCoherence::writeAck %lu myId %d origin %d",
       globalClock,addr&addrMask,myID,ccam->ccmsg->getOriginID());

  ccam->numSharers--;

  sendMsgAbs(netPort->nextSlot(),ccam->ccmsg);
  updateMsgQueue(addr);

}

void MPCacheCoherence::evictRemoteReqs(ulong addr) 
{
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  I(ccamq);

  CCActiveMsgQueue::iterator it = ccamq->begin();
  it++;
  while(it!=ccamq->end()){
    if( !(*it)->localReq && (*it)->mtype != CCRmLine && (*it)->mtype != CCInv) {
      if((*it)->mtype == CCRd)
	(*it)->ccmsg->transform2CCFailedRd();
      else if( (*it)->mtype == CCWr )
	(*it)->ccmsg->transform2CCFailedWr();
      else
	(*it)->ccmsg->transform2CCNack();

      sendMsgAbs(netPort->nextSlot(),(*it)->ccmsg);
      ccActiveMsgPool.in(*it);
      it = ccamq->erase(it);
    } else
      it++;
  }
}
