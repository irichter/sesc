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
#ifndef SLAVEMPCOH_H
#define SLAVEMPCOH_H

#include "CacheCoherence.h"
#include "MemRequest.h"
#include "MemObj.h"
#include "callback.h"
#include "MasterMPCoh.h"
#include "MPCache.h"
#include "GStats.h"
#include "GDebugCC.h"
#include "estl.h"

/*  This class is the counterpart to CCMasterProt. It will be located
 *  in M3T's L1 controller.
 */

class InterConnection;

class SlaveMPCoh : public MPCacheCoherence, public MemObj {
 protected:
  static ulong initCnt;

  ID(static GDebugCC   readReqHash);
  ID(static GDebugCC   writeReqHash);
  ID(static GDebugCC   rmSharerReqHash);
  ID(static GDebugCC   wrBackReqHash);
  ID(static GDebugCC   newOwnerReqHash);
  ID(static GDebugCC   invReqHash);
  ID(static GDebugCC   updateReqHash);
  ID(static GDebugCC   memReqHash);

  GStatsCntr readReqCnt;
  GStatsCntr readAckCnt;

  GStatsCntr writeReqCnt;
  GStatsCntr writeAckCnt;

  GStatsCntr wrBackCnt;
  GStatsCntr wrBackAckCnt;

  GStatsCntr remRdReqCnt;
  GStatsCntr remRdAckCnt;

  GStatsCntr remWrReqCnt;
  GStatsCntr remWrAckCnt;
  GStatsCntr invReqCnt;

  NetDevice_t getHomeNodeID(ulong addr) {
    return MasterMPCoh::mapAddr2NetID(addr);
  }

  typedef HASH_MAP<ulong, std::queue<CacheCoherenceMsg *> *> CCMsgMapper;
  CCMsgMapper pendingCCMsgs;

  PortGeneric *cachePort;

 public:

  SlaveMPCoh(MemorySystem* current, InterConnection *network, RouterID_t rID,
	     const char *descr_section=0, const char *symbolic_name = 0);

  virtual ~SlaveMPCoh();

  /* Should only be called by an Invalidate request */
  void access(MemRequest *mreq);
  void returnAccess(MemRequest *mreq);
  Time_t getNextFreeCycle();

  void invalidate(PAddr addr, ushort size, MemObj *oc);
  bool canAcceptStore(PAddr addr) const;

  void readReq(MemRequest *mreq);
  void writeReq(MemRequest *mreq);
  void writeBackReq(MemRequest *mreq);
  void rmSharerReq(MemRequest *mreq);

  void readHandler(Message *m);
  void readWaitingHandler(CacheCoherenceMsg *ccm);
  void waitingWrBack(CacheCoherenceMsg *ccm);
  void waitingRmSharer(CacheCoherenceMsg *ccm);
  
  void writeHandler(Message *m);
  void writeBackHandler(Message *m);
  void invHandler(Message *m);
  void nackHandler(Message *m);
  void rmLineHandler(Message *m);

  static void StaticRmLineAck(IntlMemRequest *ireq);
  void rmLineAck(IntlMemRequest *ireq);
  void waitingRmLineMsg(CacheCoherenceMsg *ccm);

  void newSharerHandler(Message *m) {
    I(0);
    m->garbageCollect();
  }

  void newOwnerHandler(Message *m) {
    I(0);
    m->garbageCollect();
  }

  void rmSharerHandler(Message *m) {
    I(0);
    m->garbageCollect();
  }
  
  void rmLineAckHandler(Message *m) {
    I(0);
    m->garbageCollect();
  }

  void failedRdHandler(Message *m) {
    I(0);
    m->garbageCollect();
  }

  void failedWrHandler(Message *m) {
    I(0);
    m->garbageCollect();
  }

  void readAckHandler(Message *m);
  void readXAckHandler(Message *m);
  void writeAckHandler(Message *m);
  void writeBackAckHandler(Message *m);
  void invAckHandler(Message *m);
  void newSharerAckHandler(Message *m);
  void newOwnerAckHandler(Message *m);
  void rmSharerAckHandler(Message *m);
  

 protected:

  void readAck(IntlMemRequest *ireq);
  void readXAck(CacheCoherenceMsg *ccm);
  void writeAck(IntlMemRequest *ireq);
  void writeBackAck(CacheCoherenceMsg *ccm);
  void invAck(IntlMemRequest *ireq);
  void invStaleRdAck(CacheCoherenceMsg *ccm);

  void addSharer(ulong addr, CacheCoherenceMsg *ccm);
  void addNewOwner(ulong addr, CacheCoherenceMsg *ccm);
  void reLaunch(ulong addr);
  void updateMsgQueue(ulong addr);

  void rmSharerNack(CacheCoherenceMsg *ccm);
  MPCache::Line * SlaveMPCoh::getLineUpperLevel(PAddr addr);
};

#endif
