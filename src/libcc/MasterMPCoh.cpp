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

#include "MasterMPCoh.h"
#include "CacheCore.h"
#include "Cache.h"
#include "MemorySystem.h"
#include "NetAddrMap.h"

/*  MasterMPCoh Class Implementation
 *
 */

#if (defined DEBUG) && (DEBUG2==1)
#define DEBUGCCPROT 1
#else
#define DEBUGCCPROT 0
#endif

std::vector<CacheCoherenceProt *> MasterMPCoh::ccProtMap;

static const char *k_directory = "directory",
                  *k_victimDir = "victimDir";

MasterMPCoh::MasterMPCoh(MemorySystem* current, 
			 InterConnection *network, RouterID_t rID, 
			 const char *descr_section, const char *symbolic_name)
  :MPCacheCoherence(network,rID,LOCAL_PORT2), 
   MemObj(descr_section, symbolic_name),
   readHandlerCnt("(M%d):readHandlerCnt",ccProtMap.size()),
   failedRdCnt("(M%d):failedRdCt",ccProtMap.size()),
   relaunchedRdCnt("(M%d):reLaunchedRdCnt",ccProtMap.size()),
   rdLineIsBusyCnt("(M%d):rdLineIsBusyCnt",ccProtMap.size()),
   rmLineReqCnt("(M%d):rmLineReqCnt",ccProtMap.size()),
   rmLineAckCnt("(M%d):rmLineAckCnt",ccProtMap.size()),
   rmLineSharers("(M%d):rmLineSharers",ccProtMap.size()),
   readReqCnt("(M%d):readReqCnt",ccProtMap.size()),
   readAckCnt("(M%d):readAckCnt",ccProtMap.size()),
   writeReqCnt("(M%d):writeReqCnt",ccProtMap.size()),
   writeAckCnt("(M%d):writeAckCnt",ccProtMap.size()),
   readHits("(M%d):readHits",ccProtMap.size()),
   writeHits("(M%d):writeHits",ccProtMap.size()),
   readSharers("(M%d):readSharers",ccProtMap.size()),
   writeSharers("(M%d):writeSharers",ccProtMap.size()),
   readNoSharers("(M%d):readNoSharers",ccProtMap.size()),
   writeNoSharers("(M%d):writeNoSharers",ccProtMap.size()),
   stalled(false)
{
  MemObj *lower_level;

  I(current);
  lower_level = current->declareMemoryObj(descr_section, k_lowerLevel);
  I(lower_level);
  addLowerLevel(lower_level);

  localCacheID = ccProtMap.size(); //number of caches in system    
  ccProtMap.push_back(this);

  const char *victimName = SescConf->getCharPtr(descr_section, k_victimDir);
  directory =  new MPDirCache(SescConf->getCharPtr(descr_section,k_directory)
			      ,"MPCoh_directory(%d):",ccProtMap.size());

  kVictimSize = SescConf->getLong(victimName, "size") / SescConf->getLong(victimName, "bsize");
  kVictimSlack = kVictimSize-SescConf->getLong(victimName, "slack");

  victimSize = 0;

  victim = new MPDirCache(victimName,"MPCoh_victim(%d):",ccProtMap.size());

  I(victim);
  I(directory);
  cache = NULL;
}

MasterMPCoh::~MasterMPCoh() 
{ 
  delete directory; 
}

void MasterMPCoh::access(MemRequest *mreq) 
{
  MemOperation memOp = mreq->getMemOperation();

  PAddr paddr = mreq->getPAddr() & 0xFFFFFFE0;

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::access adr %lu",globalClock,paddr);

  if(memOp == MemInvalidate) {
    setMissingBit(mreq->getPAddr());
  }
  mreq->goUpAbs(globalClock+1);
}

void MasterMPCoh::returnAccess(MemRequest *mreq) 
{
  I(0);
}
/*****************************************************************
 * readHandler(Message *m)
 *   1:This will be called when a read request issued
 *     to memory on the network arrives at its home.
 *
 *   2:Once the protocol has it, proceeds to examine state of 
 *     of address in cache and take appropriate action.
 *
 *****************************************************************/
void MasterMPCoh::readHandler(Message *m)
{
  CacheCoherenceMsg *ccmsg = dynamic_cast<CacheCoherenceMsg *>(m);
  
  ulong addr = ccmsg->getDstAddr(); 

  readHandlerCnt.inc();

  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::readHandler addr %lu myID %d orig %d msgID %lu",
       globalClock, addr&addrMask, (int)myID, ccmsg->getOriginID(),
       ccmsg->getXActionID());

  if(lineIsBusy(ccmsg->getDstAddr(),ccmsg)) {
    GLOG(DEBUGCCPROT,"master read handler: Line is Busy on %lu MsgId %lu",addr&addrMask,ccmsg->getXActionID());
    readHits.inc();
    rdLineIsBusyCnt.inc();
    addMsgToBusyList(addr,ccmsg);
    return;
  }

  MPDirCache::Line *l = findLine(addr);

  if (l) {
  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::readHandler addr %lu myID %d orig %d msgID %lu sz %d state %d",
       globalClock, addr&addrMask, (int)myID, ccmsg->getOriginID(),
       ccmsg->getXActionID(),l->sharers.size(),l->ccState);
  }

  if (l != NULL) { 
    readHits.inc(); 

    if (l->sharers.size() > 0)
      readSharers.add(l->sharers.size());
    else
      readNoSharers.inc();
  }

  if( l == NULL ) {
    readNoSharers.inc();

    GLOG(DEBUGCCPROT,"readHandler  Line is NULL %lu MsgId %lu!!!!",addr&addrMask,ccmsg->getXActionID());

    if(stalled) {
      rdLineIsBusyCnt.inc();
      addMsgToBusyList(addr,ccmsg);
      stalledAddrList.push_back(addr);
      return;
    }

    ulong rplcAddr = (addr&addrMask);
    l = directory->fillLine(addr, rplcAddr, true);

    I(l);
    if(rplcAddr != (addr&addrMask))
      addToVictimDir(rplcAddr,l);

    l->ccState = EX;
    l->wtSharers = 0;
    l->rmLine = 0;
    l->rmLineState = false;
    l->newOwnerRq = false;

    addSharer(addr, ccmsg->getOriginID()); //jt hack tbd 8/22
    I(l->sharers.size() == 1);
    ccmsg->transform2CCRdXAck();

    CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg);
    ccam->launched = true;

    I(lowerLevel[0]);

    readReqCnt.inc();

    IntlMemRequest::create(lowerLevel[0],addr,MemRead,0,&CacheCoherenceProt::StaticReadAck,this);
  } else if(l->rmLineState) {
    GLOG(DEBUGCCPROT,"TRYING TO REMOVE LINE %lu MsgId %lu",addr,ccmsg->getXActionID());
    removeFromQueueIfPresent(addr,ccmsg);
    ccmsg->transform2CCNack();
    failedRdCnt.inc();
    sendMsgAbs(netPort->nextSlot(),ccmsg);
  } else if(l->newOwnerRq) {
    GLOG(DEBUGCCPROT,"HAVE A ENTRY IN QUEUE FOR ADDR %lu MsgId %lu",addr&addrMask,ccmsg->getXActionID());
    rdLineIsBusyCnt.inc();
    addMsgToBusyList(addr,ccmsg);
  } else if( l->ccState == SHRD ) {//FIXME: model latency properly
    int sz = l->sharers.size();

    GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::readHandler addr %lu myID %d orig %d msgID %lu FOUND SHARED LINE",       globalClock, addr&addrMask, (int)myID, ccmsg->getOriginID(),
       ccmsg->getXActionID());

    if(l->wtSharers > 1) {
      addMsgToBusyList(addr,ccmsg);
      return;
    }

    if(l->nIncl) {
      if(sz>0) {
	GLOG(DEBUGCCPROT,
	     "[C %llu] MasterMPCoh::readHandler addr %lu myID %d orig %d msgID %lu NON_INCLUSIVE SET WITH SHARERS",
	     globalClock, addr&addrMask, (int)myID, ccmsg->getOriginID(),
	     ccmsg->getXActionID());

	removeFromQueueIfPresent(addr,ccmsg);
	l->wtSharers++;
	ccmsg->redirectMsg(l->sharers.front());
	sendMsgAbs(netPort->nextSlot(), ccmsg);
      } else {

	  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::readHandler addr %lu myID %d orig %d msgID %lu NON_INCLUSIVE WITH NO SHARERS",
       globalClock, addr&addrMask, (int)myID, ccmsg->getOriginID(),
       ccmsg->getXActionID());

	ccmsg->transform2CCRdXAck();
	CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg, 0);      
	ccam->launched = true;
	readReqCnt.inc();
	IntlMemRequest::create(lowerLevel[0],addr,MemRead,0,&CacheCoherenceProt::StaticReadAck,this);
	addSharer(addr,ccmsg->getOriginID());
      }
    } else {
      if(sz == 0) {
	l->ccState = EX;
	ccmsg->transform2CCRdXAck();
      } else {
	ccmsg->transform2CCRdAck();
      }
      CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg, 0);      
      ccam->launched = true;
      readReqCnt.inc();
      IntlMemRequest::create(lowerLevel[0],addr,MemRead,0,&CacheCoherenceProt::StaticReadAck,this);
      addSharer(addr, ccmsg->getOriginID());
    }
  } else if(l->ccState == SMOD || l->ccState == MOD) {
	  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::readHandler addr %lu myID %d orig %d msgID %lu CALLNG MOD LINE HANDLER",       globalClock, addr&addrMask, (int)myID, ccmsg->getOriginID(),
       ccmsg->getXActionID());
    readModifiedLineHandler(ccmsg);
  } else if( l->ccState == EX ) {
    readExclusiveLineHandler(ccmsg);
  } 
}

void MasterMPCoh::failedRdHandler(Message *m) 
{
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccmsg->getDstAddr();
  MPDirCache::Line *l = findLine(addr);
  I(l);
  NetDevice_t srcID = ccmsg->getSrcNetID();
  ccmsg->setType(CCRd);

  failedRdCnt.inc();

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::failedRdHandler adr %lu myID %d msgID %lu",globalClock,addr&addrMask,myID,ccmsg->getXActionID());

  l->wtSharers--;

  //IS(l->printSharers(addr&addrMask));

  //Should call reLaunch immediately because there cannot be a
  //pending request!

  if(l->rmLineState && l->wtSharers == 0 && l->rmLine == 0){ 
    removeLineFromVictimDir(addr);
    //return;
  } else { 
    if(l->wtSharers == 0 && !l->newOwnerRq)
      reLaunch(addr);
  }
  //readHandler(m);
  ccmsg->transform2CCNack();
  sendMsgAbs(netPort->nextSlot(),ccmsg);  
}

void MasterMPCoh::failedWrHandler(Message *m) 
{
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccmsg->getDstAddr();
  MPDirCache::Line *l = findLine(addr);

  NetDevice_t srcID = ccmsg->getSrcNetID();

  //IS(l->printSharers(addr&addrMask));

  ccmsg->setType(CCWr);

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::failedWrHandler adr %lu myID %d MsgId %lu",globalClock,addr&addrMask,myID,ccmsg->getXActionID());

  if(l != NULL) {
    l->newOwnerRq = false;
    reLaunch(addr);   
  }
  //writeHandler(m);
  ccmsg->transform2CCNack();
  sendMsgAbs(netPort->nextSlot(),ccmsg);
}

/*****************************************************************
 * writeHandler(Message *m)
 *   1:This will be called when a write request made to line
 *     not present in local caches, reaches either the home
 *     node or the owner node.
 *
 *   2:Once the protocol has it, proceeds to examine state of 
 *     of address in cache and take appropriate action.
 *
 *****************************************************************/
void MasterMPCoh::writeHandler(Message *m)
{
  I(m);
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  I(ccmsg);
  ulong addr = ccmsg->getDstAddr();
  
  writeReqCnt.inc();

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::writeHandler addr %lu REQUESTOR %d myID %d msgID %lu",globalClock, addr&addrMask, ccmsg->getSrcNetID(),(int)myID,ccmsg->getXActionID());

  if(lineIsBusy(addr,ccmsg)) {
    addMsgToBusyList(addr,ccmsg);
    writeHits.inc();
    return;
  }

  MPDirCache::Line *l = findLine(addr);

  if (l != NULL) { 
    writeHits.inc(); 

    if (l->sharers.size() > 0)
      writeSharers.add(l->sharers.size());
    else
      writeNoSharers.inc();

  } else {
    writeNoSharers.inc();
  }

  if(l == NULL || (l!=NULL && l->ccState == INV)) { //|| (l != NULL && l->ccState == INV)) {

    GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::writeHandler adr %lu myID %d NULL LINE MsgId %lu",globalClock,addr&addrMask,myID,ccmsg->getXActionID());

    if(stalled) {
      addMsgToBusyList(addr,ccmsg);
      stalledAddrList.push_back(addr);
      return;
    }

    CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg); 
    ccam->launched = true;
    I(getBusyMsgQueue(addr));

    ccmsg->transform2CCWrAck(1);
    ulong rpladdr = addr;
    l = directory->fillLine(addr, rpladdr, true);
    I(l);
    if(rpladdr != addr)
      addToVictimDir(rpladdr, l);

    GLOG(DEBUGCCPROT,"[C %llu] writeHandler addr %lu %lu SET TO MOD MsgID %lu",globalClock,addr&addrMask,l->getTag() << 5,ccmsg->getXActionID());

    l->ccState = MOD;
    l->nIncl = false;
    l->wtSharers = 0;
    l->rmLine = 0;
    l->rmLineState = false;
    l->newOwnerRq = false;

    GLOG(DEBUGCCPROT,"[C %llu] writeHandler addr %lu addsharer %d MsgID %lu",globalClock,addr&addrMask,ccmsg->getOriginID(),ccmsg->getXActionID());

    l->addSharer(ccmsg->getOriginID());

    IntlMemRequest::create(lowerLevel[0],addr,MemReadW,0,&MasterMPCoh::StaticMissingLineAck,this);
  } else if(l->rmLineState) {
    GLOG(DEBUGCCPROT,"Nack because currently removing line MsgId:%lu",ccmsg->getXActionID());
    ccmsg->transform2CCNack();
    sendMsgAbs(netPort->nextSlot(),ccmsg);
    removeFromQueueIfPresent(addr,ccmsg);
  } else if(l->wtSharers > 0 || l->newOwnerRq ) {
    addMsgToBusyList(addr,ccmsg);
  } else if(l->ccState == MOD || l->ccState == SMOD) {
    writeModifiedLineHandler(ccmsg);
  } else if( l->ccState == EX ) {
    writeExclusiveLineHandler(ccmsg);
  } else if( l->ccState == SHRD ) {
    //IS(l->printSharers(addr&addrMask));
    writeSharedLineHandler(ccmsg);
  }
}

/*****************************************************************
 * writeBackHandler(Message *m)
 *   1:This will be called when a write back request is made
 *     due to expulsion from a higher level cache.
 *
 *   2:Adds data back to memory sets the cache line as
 *     clean, non-shared, up-to-date. Line must be in modified
 *     state when a write-back happens.
 *
 *   NB: if we up
 *****************************************************************/
void MasterMPCoh::writeBackHandler(Message *m)
{
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccmsg->getDstAddr();
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::writeBackHandler addr %lu myID %d MsgID %lu",globalClock, addr&addrMask, myID, ccmsg->getXActionID());

  removeSharer(addr,ccmsg->getOriginID());

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  IntlMemRequest::create(lowerLevel[0],addr,MemWrite,0);

  writeBackAck(ccmsg);
}

void MasterMPCoh::invHandler(Message *m)
{
  I(0); 
}

void MasterMPCoh::newSharerHandler(Message *m) 
{
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccmsg->getDstAddr();
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::newSharerHandler %lu orig %d sender %d MsgID %lu",globalClock,addr&addrMask,ccmsg->getOriginID(),ccmsg->getSrcNetID(),ccmsg->getXActionID());

  MPDirCache::Line *l = findLine(addr);
  l->wtSharers--;

  addSharer(addr, ccmsg->getOriginID(),SHRD);

  ccmsg->transform2CCNewSharerAck();
  sendMsgAbs(netPort->nextSlot(), ccmsg);

  if( l->rmLineState ) {
    l->rmLine++;
    CacheCoherenceMsg *nccmsg = 
      CacheCoherenceMsg::createCCMsg(this,CCRmLine,ccmsg->getOriginID(),addr);
    sendMsgAbs(netPort->nextSlot(), nccmsg);
  }

  I(!l->newOwnerRq);

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  if(l->wtSharers == 0) {
    reLaunch(addr);
  }
}

void MasterMPCoh::newOwnerHandler(Message *m) 
{
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccmsg->getDstAddr()  ;
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::newOwnerHandler %lu MsgId %lu",
       globalClock,addr&addrMask, ccmsg->getXActionID());

  removeAllSharers(addr);
  addSharer(addr, ccmsg->getOriginID());
  ccmsg->transform2CCNewOwnerAck();
  sendMsgAbs(netPort->nextSlot(), ccmsg);

  MPDirCache::Line *l = findLine(addr);
  l->ccState = MOD;

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::newOwnerHandler %lu %lu SET TO MOD MsgId %lu",
       globalClock,addr&addrMask,l->getTag() << 5,ccmsg->getXActionID());

  if( l->rmLineState ) {
    l->rmLine++;
    CacheCoherenceMsg *nccmsg = 
      CacheCoherenceMsg::createCCMsg(this,CCRmLine,ccmsg->getOriginID(),addr);
    sendMsgAbs(netPort->nextSlot(),nccmsg);
  }

  l->newOwnerRq = false;
  reLaunch(addr);
}

void MasterMPCoh::rmSharerHandler(Message *m) 
{
  CacheCoherenceMsg *ccmsg = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccmsg->getDstAddr()  ;
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::rmSharerHandler addr %lu from %d MsgID %lu",globalClock,addr&addrMask,ccmsg->getSrcNetID(),ccmsg->getXActionID());

  removeSharer(addr, ccmsg->getSrcNetID());

  ccmsg->transform2CCRmSharerAck();

  sendMsgAbs(netPort->nextSlot(),ccmsg);
}

void MasterMPCoh::addSharer(ulong addr, CacheCoherenceMsg *ccm) 
{
  addSharer(addr,ccm->getOriginID());
}

void MasterMPCoh::addNewOwner(ulong addr, CacheCoherenceMsg *ccm) 
{
  //FIXME: Make me more efficient
  removeAllSharers(addr);
  addSharer(addr,ccm->getOriginID());
}

void MasterMPCoh::writeBackAck(CacheCoherenceMsg *ccm)
{
  ulong addr = ccm->getDstAddr()  ;
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::writeBackAck addr %lu MsgID %lu",globalClock,addr&addrMask,ccm->getXActionID());
  ccm->transform2CCWrBackAck();
  sendMsgAbs(netPort->nextSlot(), ccm);
}

void MasterMPCoh::writeBackAckHandler(Message *m) 
{
  I(0);
  GLOG(DEBUGCCPROT,"MasterMPCoh::writeBackAckHandler SHOULD NEVER BE CALLED!");
}

void MasterMPCoh::invAck(IntlMemRequest *ireq)
{
  I(0);
  GLOG(DEBUGCCPROT,"MasterMPCoh::invAck NOT IMPLEMENTED");
}
  
void MasterMPCoh::readAckHandler(Message *m)
{
  I(0);
  GLOG(DEBUGCCPROT,"MasterMPCoh::readAckHandler SHOULD NEVER BE CALLED!");
}

void MasterMPCoh::readXAckHandler(Message *m)
{
  I(0);
  GLOG(DEBUGCCPROT,"MasterMPCoh::readXAckHandler SHOULD NEVER BE CALLED!");
}

void MasterMPCoh::writeAckHandler(Message *m)
{
  I(0);
  GLOG(DEBUGCCPROT,"MasterMPCoh::writeAckHandler SHOULD NEVER BE CALLED!");
}

void MasterMPCoh::invAckHandler(Message *m)
{
  I(0);
  GLOG(DEBUGCCPROT,"MasterMPCoh::invAckHandler SHOULD NEVER BE CALLED!");
}

void MasterMPCoh::readModifiedLineHandler(CacheCoherenceMsg *ccmsg) 
{
  ulong addr = ccmsg->getDstAddr();
  MPDirCache::Line *l = findLine(addr);
  I(l);
  I(l->sharers.size());

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::readModLineH addr %lu myID %d sharer %d MsgId %lu",globalClock,addr&addrMask,myID,l->getNextSharer(),ccmsg->getXActionID());
  I(l->sharers.size());
  l->wtSharers++;

  ccmsg->redirectMsg(l->getNextSharer()); 
  sendMsgAbs(netPort->nextSlot(), ccmsg);
  removeFromQueueIfPresent(addr,ccmsg);

  reLaunch(addr);
}

void MasterMPCoh::readExclusiveLineHandler(CacheCoherenceMsg *ccmsg) 
{
  ulong addr = ccmsg->getDstAddr();
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::readExclusiveLineHandler adr %lu MsgId %lu",
       globalClock,addr&addrMask,ccmsg->getXActionID());
  readModifiedLineHandler(ccmsg);
}

void MasterMPCoh::writeModifiedLineHandler(CacheCoherenceMsg *ccmsg) 
{
  ulong addr = ccmsg->getDstAddr();
  //Send CCWr to owner
  MPDirCache::Line *l = findLine(addr);
  l->newOwnerRq=true;

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::writeModifiedLineHandler adr %lu MsgID %lu",
       globalClock,addr&addrMask,ccmsg->getXActionID());

  I(l->sharers.size());

  //BIGTIME HACK...FIXME: SHOULD NOT NEED THIS CODE!
  //if(l->sharers.size()==0) {
  //  l->ccState=SHRD;
  //  writeHandler(ccmsg);
  //  return;
  //}

  ccmsg->redirectMsg(l->getNextSharer());

  sendMsgAbs(netPort->nextSlot(),ccmsg);
  removeFromQueueIfPresent(addr,ccmsg);  
  reLaunch(addr);

}

void MasterMPCoh::writeExclusiveLineHandler(CacheCoherenceMsg *ccmsg) 
{
  writeModifiedLineHandler(ccmsg);
}

void MasterMPCoh::writeSharedLineHandler(CacheCoherenceMsg *ccmsg) 
{
  //Send CCInv to all sharers;
  ulong addr = ccmsg->getDstAddr()  ;
  NetDevice_t srcID = ccmsg->getOriginID();

  MPDirCache::Line *l = findLine(addr);

  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::writeSharedLineH addr %lu origin %d MsgID %lu",
       globalClock,addr&addrMask, srcID, ccmsg->getXActionID());

  //removeFromQueueIfPresent(addr,ccmsg);

  I(l->ccState == SHRD);

  //FIXME: consider non-inclusive case

  int numInvs = l->sharers.size();
  if(numInvs == 0) {
    CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg);
    ccam->launched = true;
    ccmsg->transform2CCWrAck(1);
    IntlMemRequest::create(lowerLevel[0],addr,MemReadW,0,
			     &CacheCoherenceProt::StaticWriteAck,this);
    l->ccState = MOD;
  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::writeSharedLineH addr %lu origin %d %lu SET TO MOD MsgID %lu",
       globalClock,addr&addrMask, srcID, directory->getLineAddr(l),ccmsg->getXActionID());
    addSharer(addr,srcID);
    return;
  }

  int sz = numInvs-1;
  l->sharers.sort();
  l->sharers.unique();
  I(l);

  NetDevice_t netID = l->removeNextSharer();
  NetDevice_t firstID = netID;

  bool foundReq = false;

  if( netID == srcID ) {
    foundReq = true;
  } 

  ID2(NetDevice_t prev = netID);

  for(int i=0; i<sz; i++) {
    netID = l->removeNextSharer();
    
    I(i==0 || (i != 0 && netID != prev));

    if( netID != srcID ) {
      CacheCoherenceMsg *newmsg = 
	CacheCoherenceMsg::createCCMsg(this, CCInv, netID, ccmsg);
      sendMsgAbs(netPort->nextSlot(), newmsg);
      GLOG(DEBUGCCPROT,"ad %lu SEND INVALIDATE TO %d MsgId:%lu",addr&addrMask,netID,ccmsg->getXActionID());
    } else {
      foundReq = true;
    }
    IS(prev = netID);
  }

  I(l->sharers.size() == 0);

  GLOG(DEBUGCCPROT,"addr %lu NUMBER OF INVALIDATES TO SEND: %d MsgID %lu",addr&addrMask,
       numInvs, ccmsg->getXActionID());

  if(l->nIncl && !foundReq) {

    // ( (NOT in the L2) && (Requestor Does NOT have copy) )
    removeFromQueueIfPresent(addr,ccmsg);
    ccmsg->redirectMsg(firstID);
    l->newOwnerRq = true;
    l->addSharer(firstID);
    ccmsg->setNumAcks(numInvs);
    sendMsgAbs(netPort->nextSlot(), ccmsg);
    reLaunch(addr);

  } else {

    l->addSharer(srcID);
    l->ccState = MOD;

    GLOG(DEBUGCCPROT,"writeSharedLineHandler addr %lu %lu SET TO MOD MsgID %lu",addr&addrMask,l->getTag() << 5, ccmsg->getXActionID());

    if(firstID != srcID) {
      CacheCoherenceMsg *newmsg = 
	CacheCoherenceMsg::createCCMsg(this, CCInv, firstID, ccmsg);
      sendMsgAbs(netPort->nextSlot(), newmsg);
    }

    if(foundReq) {

      // ( [(NOT in the L2)||(In the L2)] && (Requestor Does have copy) )

      ccmsg->transform2CCWrAck(numInvs);

      removeFromQueueIfPresent(addr,ccmsg);
      sendMsgAbs(netPort->nextSlot(), ccmsg);
      reLaunch(addr);

    } else {
    
      // ( (In the L2) && (Requestor Does NOT have copy) )
      ccmsg->transform2CCWrAck(numInvs+1);
      
      CCActiveMsg *ccam = addMsgToBusyList(addr, ccmsg, numInvs);
      ccam->launched = true;
      IntlMemRequest::create(lowerLevel[0],addr,MemReadW,0,
			       &CacheCoherenceProt::StaticWriteAck,this); 
    }
  }
}

void MasterMPCoh::addSharer(ulong addr, NetDevice_t netID, CacheState ccState) 
{
  MPDirCache::Line *l = findLine(addr);
  I(l);
  l->addSharer(netID);

  if(ccState != INV)
    l->ccState = ccState;
}

int  MasterMPCoh::getNumSharers(ulong addr) 
{
  MPDirCache::Line *l = findLine(addr);
  return l->sharers.size();
}

//not used
void MasterMPCoh::setOwner(ulong addr, NetDevice_t ownerID) 
{
  I(!getNumSharers(addr));
  addSharer(addr,ownerID);
}

void MasterMPCoh::removeAllSharers(ulong addr) 
{
  MPDirCache::Line *l = findLine(addr);
  I(l);
  
  for(int i = l->sharers.size(); i>0; i--)
    l->removeNextSharer();
}

void MasterMPCoh::removeSharer(ulong addr, NetDevice_t netID) 
{
  MPDirCache::Line *l = findLine(addr);

  GLOG(DEBUGCCPROT,"[C %llu] removeSharer ad %lu nid %d",globalClock,addr,netID);

  I(l);
  l->rmSharer(netID);

  //GLOG(!l->sharers.empty(),
  //     "[C %llu] %s MasterMPCoh::writeBackHandler WARNING: Writeback race->Line status updated before writeback completed.",
  //     globalClock, symbolicName);

  if(l->sharers.size() == 0) {
    GLOG(DEBUGCCPROT,"[C %llu] removeSharer ad %lu nid %d myId %d SET TO SHARED",globalClock,addr,netID,myID);
    l->ccState = SHRD;
  }

  //IS(l->printSharers(addr&addrMask));
}

void MasterMPCoh::setMissingBit(ulong addr) 
{
  MPDirCache::Line *l = findLine(addr&addrMask);

  if(l!=NULL) {
    PAddr paddr = (addr & addrMask); 

    GLOG(DEBUGCCPROT,"[C %llu] setMissingBit adr %lu state %d",globalClock,paddr,l->ccState);

    l->nIncl = true;
  }
}

//Assuming ccm has already been converted
void MasterMPCoh::readAck(IntlMemRequest *ireq) 
{
  ulong addr = ireq->getPAddr();

  readAckCnt.inc();

  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  I(ccamq);
  CCActiveMsg *ccam = ccamq->front();
  I(ccam);
  CacheCoherenceMsg *ccm = ccam->ccmsg;
  GLOG(DEBUGCCPROT,
       "[C %llu] CCM3TBaseProt::readAck addr %lu origin %d myID %d MsgId %lu",
       globalClock,addr&addrMask,ccam->ccmsg->getOriginID(),myID,ccam->ccmsg->getXActionID());
  
  sendMsgAbs(netPort->nextSlot(), ccm);
  ccam->numSharers++;
  ccam->ccmsg = NULL;

  updateMsgQueue(addr); //performs cleanup if needed
}


void MasterMPCoh::writeAck(IntlMemRequest *ireq) 
{
  ulong addr = ireq->getPAddr();
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);
  I(ccam);
  GLOG(DEBUGCCPROT,"[C %llu] CCM3TBaseProt::writeAck %lu myId %d origin %d MsgID %lu",
       globalClock,addr&addrMask,myID,ccam->ccmsg->getOriginID(),ccam->ccmsg->getXActionID());

  writeAckCnt.inc();

  ccam->numSharers--;

  sendMsgAbs(netPort->nextSlot(),ccam->ccmsg);
  updateMsgQueue(addr);
}

void MasterMPCoh::StaticMissingLineAck(IntlMemRequest *ireq) 
{
  ((MasterMPCoh *)(ireq->getWorkData()))->missingLineAck(ireq);
}

void MasterMPCoh::missingLineAck(IntlMemRequest *ireq) 
{
  ulong addr = ireq->getPAddr()  ;
  CCActiveMsg *ccam = getFrontBusyMsgQueue(addr);

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::missingLineAck adr %lu myID %d",
       globalClock,addr&addrMask,myID);

  sendMsgAbs(netPort->nextSlot(), ccam->ccmsg);
  
  updateMsgQueue(addr);
}

NetDevice_t MasterMPCoh::mapAddr2NetID(ulong addr) 
{
  //int index = (addr >> 15) & 0x3;  
  //index = 0;
  //  return MasterMPCoh::ccProtMap[index]->getNetDeviceID();
  return MasterMPCoh::ccProtMap[NetAddrMap::getMapping(addr)]->getNetDeviceID();
}

//this function means remove first two, then see if
//there is work to be done
void MasterMPCoh::updateMsgQueue(ulong addr) 
{
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  I( ccamq );

  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::updateMsgQueue adr %lu myID %d sz %d",
       globalClock,addr&addrMask,myID,ccamq->size());

  removeFrontBusyMsgQueue(ccamq);  

  if(ccamq->size() == 0) {
    GLOG(DEBUGCCPROT,"update msg q: remove q %lu",addr);
    removeBusyMsgQueue(addr);
  }

  if(!checkVictimDir(addr) && ccamq != NULL) {
      reLaunch(addr);
  } else {
    GLOG(DEBUGCCPROT,"chose not to reLaunch...why??");
  }
}

void MasterMPCoh::reLaunch(ulong addr) 
{
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::reLaunch addr %lu JUST ENTERED",
       globalClock,addr&addrMask);

  if(ccamq == NULL) //for some uses this is necessary 
    return;

  GLOG(DEBUGCCPROT,
       "[C %llu] MasterMPCoh::reLaunch FOR(addr)%lu QSize %d AT(myID):%d",
       globalClock,addr,ccamq->size(),myID);

  //not sure this can ever happen...figure it out...
  if(ccamq->size() == 0) {
    removeBusyMsgQueue(addr);
    return;
  }

  if(ccamq->front()->launched)
    return;

  if(ccamq->front()->localReq) {

    ccamq->front()->launched = true;

    sendMsgAbs(netPort->nextSlot(), ccamq->front()->ccmsg);

    return;
  }

  CacheCoherenceMsg *ccmsg = ccamq->front()->ccmsg;

  //for debugging only
  if(ccmsg->getType() == CCRd)
    relaunchedRdCnt.inc();

  reLaunchMsg(ccmsg);
  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::reLaunch RELAUNCHED",
	 globalClock);  
}

void MasterMPCoh::addToVictimDir(ulong addr, MPDirCache::Line *l) 
{
  ulong rpladdr = addr;

  I(l);

  GLOG(DEBUGCCPROT,"[C %llu] addToVictimDir ad %lu",globalClock,(addr&0xFFFFFFE0));

  if(l->ccState == INV)
    return;

  if( l->sharers.size() == 0 && getBusyMsgQueue(addr) == NULL 
      && l->wtSharers == 0 && !l->rmLineState)
    return;

  MPDirCache::Line *nl = victim->fillLine(addr, rpladdr, true);  
  I(rpladdr == addr);

  nl->ccState     = l->ccState;
  nl->nIncl       = l->nIncl;
  nl->rmLine      = 0;
  nl->rmLineState = l->rmLineState;
  nl->wtSharers   = l->wtSharers;
  nl->newOwnerRq  = l->newOwnerRq;

  int sz = l->sharers.size();

  for(int i=0; i<sz; i++) {
    nl->addSharer(l->removeNextSharer());
  }

  victimSize++;
  
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);

  if(victimSize >= kVictimSlack) {
    MPDirCache::Line *l = victim->getLineToReplace(addr);
    if(l==NULL) {
	victimSize--;
    } else {
      if(victimSize == kVictimSize)
	stalled = true;
      
      GLOG(DEBUGCCPROT,
	   "[C %llu] MasterMPCoh::addToVictimDir addr %lu size %d TAKe EVASIVE ACTION myID %d",
	   globalClock,addr&addrMask,victimSize,myID);
      rmLineReq(victim->getLineAddr(l));
    }
  }
}

bool MasterMPCoh::checkVictimDir(ulong addr) 
{
   if( isInVictimDir(addr) ) {
     GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::checkVictimDir addr %lu",globalClock, addr&addrMask);
     return removeLineFromVictimDir(addr);
     //return true;     
   } else  
     return false;
}

bool MasterMPCoh::removeLineFromVictimDir(ulong addr) 
{
  addr = addr & addrMask;
  MPDirCache::Line *l = findLine(addr);

  int sz = l->sharers.size();

  if(sz == 0 && l->rmLine==0 && l->wtSharers==0 && l->rmLineState) {
    
    l->rmLineState = false;

    GLOG(DEBUGCCPROT,
      "[C %llu] MasterMPCoh::removeLineFromVictimDir %lu size %d REMOVE LINE %d",
      globalClock,addr&addrMask,sz,myID);
    
    MPDirCache::Line *vl = victim->findLine(addr);
    if (vl) {
      victimSize--;
      vl->invalidate();
    }
    
    if(stalled) {
      stalledAddrList.push_back(addr);
      stalled = false;
      
      int sz = stalledAddrList.size();
      while(sz && !stalled) {
	sz--;
	reLaunch(stalledAddrList.front());
	stalledAddrList.pop_front();
      }
    } else {
      reLaunch(addr);
    }
    return true;
  }

  return false;
}

void MasterMPCoh::rmLineReq(ulong addr) 
{
  //Need to find line in victim dir that has no sharers...
  MPDirCache::Line *l = victim->getLineToReplace(addr);
  if(l==NULL){
    stalled=false;
    victimSize--;
    return;
  }

  rmLineReqCnt.inc();

  l->rmLine = (l->sharers.size());
  l->rmLineState = true;

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::rmLineReq adr %lu",globalClock,addr&addrMask);

  l->sharers.sort();
  l->sharers.unique();

  rmLineSharers.sample(l->sharers.size());

  std::list<NetDevice_t>::iterator it = l->sharers.begin(),
                                   it_end = l->sharers.end();
  while(it != it_end) {
    CacheCoherenceMsg *ccm;
    ccm = CacheCoherenceMsg::createCCMsg(this,CCRmLine,*it,addr);
    GLOG(DEBUGCCPROT,"[C %llu] rmLineReq ad %lu from %d",globalClock,addr,*it);
    sendMsgAbs(netPort->nextSlot(), ccm);
    it++;
  }
   
  //Nack every unLaunched req in the ccamq
  CCActiveMsgQueue *ccamq = getBusyMsgQueue(addr);
  CCActiveMsgQueue::iterator cit;
  if(ccamq != NULL) {
    cit = ccamq->begin();
    while(cit != ccamq->end()) {
      if( !(*cit)->launched ) {
	(*cit)->ccmsg->transform2CCNack();
	sendMsgAbs(netPort->nextSlot(),(*cit)->ccmsg);
	ccActiveMsgPool.in(*cit);
	cit = ccamq->erase(cit);
      } else
	cit++;
    }

    if(ccamq->size() == 0)
      removeBusyMsgQueue(addr);
  }

}

void MasterMPCoh::rmLineAckHandler(Message *m) 
{
  CacheCoherenceMsg *ccm = static_cast<CacheCoherenceMsg *>(m);
  ulong addr = ccm->getDstAddr();

  GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::rmLineAckHandler adr %lu orig %d MsgID %lu",globalClock,addr&addrMask,ccm->getOriginID(),ccm->getXActionID());

  MPDirCache::Line *l = findLine(addr);
  I(l);
  I(l->rmLineState);

  l->rmSharer(ccm->getSrcNetID());

  l->rmLine--;                             

  if(l->rmLine == 0 && l->wtSharers == 0) {
    rmLineAckCnt.inc();
    removeLineFromVictimDir(addr);
  }
  ccm->garbageCollect();
}

MPCacheCoherence::MPDirCache::Line *MasterMPCoh::findLine(ulong addr) 
{ 
  MPDirCache::Line *l = directory->findLine(addr);
  if(l==NULL) {
    I(victim);
    GLOG(DEBUGCCPROT,"Gotten from Victim %lu",addr&addrMask);

    l = victim->findLine(addr);
    I((l==0) || (l && victim->getLineAddr(l) == (addr&addrMask)));
    GLOG(DEBUGCCPROT && l,"[C %llu] MasterMPCoh::getLine %lu and %lu",globalClock,addr&addrMask,victim->getLineAddr(l));
    return l;
  } else {
    I(directory->getLineAddr(l) == (addr&addrMask));
    GLOG(DEBUGCCPROT,"[C %llu] MasterMPCoh::getLine %lu and %lu",globalClock,addr&addrMask,directory->getLineAddr(l));

    return l;
  }
} 
