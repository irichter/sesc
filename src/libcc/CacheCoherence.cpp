#include "CacheCoherence.h"

pool <CacheCoherenceProt::CCActiveMsg> CacheCoherenceProt::ccActiveMsgPool(32);
pool < CacheCoherenceProt::CCActiveMsgQueue > CacheCoherenceProt::ccActiveMsgQueuePool(64);
static pool < std::queue<CacheCoherenceMsg *> > ccMsgQueuePool(64);
pool < CacheCoherenceMsg > CacheCoherenceMsg::ccMsgPool(64);

ulong CacheCoherenceMsg::gXActionIDCnt = 0;

/*  CacheCoherenceMsg Class Implementation
 *
 */

CacheCoherenceMsg *CacheCoherenceMsg::createCCMsg(ProtocolBase *srcPB, 
						  MessageType mtype, 
						  NetDevice_t dstID, 
						  long addr,
						  MemRequest *mreq) {
  CacheCoherenceMsg *msg = ccMsgPool.out();
  msg->xActionID = gXActionIDCnt++;

  ProtocolBase *dstPB = srcPB->getProtocolBase(dstID);
  msg->setupMessage(srcPB,dstPB,mtype);

  msg->dstAddr = addr;
  msg->originID = msg->getSrcNetID();
  msg->mreq = mreq;

  msg->numFailures=0;
  msg->failedSrcID=0;
  msg->incomingInvs=0;

  return msg;
}

CacheCoherenceMsg *CacheCoherenceMsg::createCCMsg(ProtocolBase *srcPB, 
						  MessageType mtype, 
						  NetDevice_t dstID, 
						  CacheCoherenceMsg *ccm) {
  CacheCoherenceMsg *msg = ccMsgPool.out();
  msg->xActionID = ccm->xActionID;

  ProtocolBase *dstPB = srcPB->getProtocolBase(dstID);
  msg->setupMessage(srcPB,dstPB,mtype);

  msg->dstAddr = ccm->dstAddr;
  msg->originID = ccm->originID;;
 
  msg->numFailures  = 0;
  msg->failedSrcID  = 0;
  msg->incomingInvs = 0;

  return msg;
}

void CacheCoherenceMsg::transform2CCRdAck() {
  I(getType() == CCRd);
  redirectToOrigin();
  setType(CCRdAck);
}

void CacheCoherenceMsg::transform2CCRdXAck() {
  I(getType() == CCRd);
  redirectToOrigin();
  setType(CCRdXAck);
  mreq->mutateReadToReadX();
}

void CacheCoherenceMsg::transform2CCWrAck(ushort acks) {
  I(getType() == CCWr);
  redirectToOrigin();
  setType(CCWrAck);
  numAcks = acks;
  incomingInvs = 1;
}

void CacheCoherenceMsg::transform2CCWrBackAck() {
  I(getType() == CCWrBack);
  redirectToOrigin();
  setType(CCWrBackAck);
}

void CacheCoherenceMsg::transform2CCInvAck() {
  I(getType() == CCInv);
  redirectToOrigin();
  setType(CCInvAck);
}

void CacheCoherenceMsg::transform2CCNack() {
  redirectToOrigin();
  setType(CCNack);
}

void CacheCoherenceMsg::transform2CCNewSharerAck() {
  reverseDirection();
  setType(CCNewSharerAck);
}

void CacheCoherenceMsg::transform2CCNewOwnerAck() {
  reverseDirection();
  setType(CCNewOwnerAck);
}

void CacheCoherenceMsg::transform2CCRmSharerAck() {
  reverseDirection();
  setType(CCRmSharerAck);
}

void CacheCoherenceMsg::transform2CCRmLineAck() {
  reverseDirection();
  setType(CCRmLineAck);
}

void CacheCoherenceMsg::transform2CCFailedRd() {
  //  GLOG(1,"********* [C %llu] transform2CCFailedRd %d",globalClock,msgID);

  reverseDirection();
  setType(CCFailedRd);
}

void CacheCoherenceMsg::transform2CCFailedWr() {
  reverseDirection();
  setType(CCFailedWr);
}

void CacheCoherenceMsg::reSend(MessageType mtype,NetDevice_t dstID) {
  redirectMsg(dstID);
  setType(mtype);
}



