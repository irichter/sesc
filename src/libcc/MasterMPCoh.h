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
#ifndef MASTERMPCOH_H
#define MASTERMPCOH_H

#include "CacheCoherence.h"
#include "MemRequest.h"
#include "MemObj.h"
#include "callback.h"
#include "MemorySystem.h"

/*  This class implements the home cache protocol intended for M3T.
 *  Ensures coherence among L1's using the CCSlaveProt
 */

class InterConnection;

class MasterMPCoh : public MPCacheCoherence, public MemObj {
private:
  static std::vector<CacheCoherenceProt *> ccProtMap;
  size_t localCacheID;

protected:

  GStatsCntr readHandlerCnt;
  GStatsCntr failedRdCnt;
  GStatsCntr relaunchedRdCnt;
  GStatsCntr rdLineIsBusyCnt;

  GStatsCntr rmLineReqCnt;
  GStatsCntr rmLineAckCnt;
  GStatsAvg  rmLineSharers;

  GStatsCntr readReqCnt;
  GStatsCntr readAckCnt;
  GStatsCntr writeReqCnt;
  GStatsCntr writeAckCnt;

  GStatsCntr readHits;
  GStatsCntr writeHits;

  GStatsCntr readSharers;
  GStatsCntr writeSharers;
  GStatsCntr readNoSharers;
  GStatsCntr writeNoSharers;
  
  MPDirCache *directory;
  MPDirCache *victim;
  ushort kVictimSize;
  ushort kVictimSlack;
  ushort victimSize;

  bool stalled;
  std::list<ulong> stalledAddrList;

  void reLaunch(ulong addr);
  void updateMsgQueue(ulong addr);
  void addSharer(ulong addr, CacheCoherenceMsg *ccm);
  void addNewOwner(ulong addr, CacheCoherenceMsg *ccm);

public:
  MasterMPCoh(MemorySystem* current, InterConnection *network, RouterID_t rID, 
	      const char *descr_section=0, const char *symbolic_name = 0);

  ~MasterMPCoh();

  static NetDevice_t mapAddr2NetID(ulong addr);

private:

  NetDevice_t getHomeNodeID(ulong addr) { //FIXME
    return mapAddr2NetID(addr);
  }

public:

  /* Should only be called by an Invalidate request */
  void access(MemRequest *mreq);
  void returnAccess(MemRequest *mreq);
  Time_t getNextFreeCycle() { return globalClock; } //FIXME: model correctly
  void invalidate(PAddr addr,ushort size,MemObj *oc) {}
  bool canAcceptStore(PAddr addr) const { return true; }

  void readHandler(Message *m);
  void writeHandler(Message *m);
  void writeBackHandler(Message *m);
  void invHandler(Message *m);
  void nackHandler(Message *m) { I(0); }

  void newSharerHandler(Message *m);
  void newOwnerHandler(Message *m);
  void rmSharerHandler(Message *m);
  void failedRdHandler(Message *m);
  void failedWrHandler(Message *m);

  void readAckHandler(Message *m);
  void readXAckHandler(Message *m);
  void writeAckHandler(Message *m);
  void writeBackAckHandler(Message *m);
  void invAckHandler(Message *m);
  void newSharerAckHandler(Message *m) { I(0); m->garbageCollect(); }
  void newOwnerAckHandler(Message *m)  { I(0); m->garbageCollect(); }
  void rmSharerAckHandler(Message *m)  { I(0); m->garbageCollect(); }

  void rmLineAckHandler(Message *m);
  void rmLineHandler(Message *m)   { I(0); GLOG(true,"BAD!!"); m->garbageCollect(); }

  void rmLineReq(ulong addr);

protected:

  static void StaticMissingLineAck(IntlMemRequest *ireq);

  void readAck(IntlMemRequest *ireq);
  void writeAck(IntlMemRequest *ireq);
  void writeBackAck(CacheCoherenceMsg *ccm);


  void readModifiedLineHandler(CacheCoherenceMsg *ccmsg);
  void writeModifiedLineHandler(CacheCoherenceMsg *ccmsg);

  void readExclusiveLineHandler(CacheCoherenceMsg *ccmsg);
  void writeExclusiveLineHandler(CacheCoherenceMsg *ccmsg);

  void writeSharedLineHandler(CacheCoherenceMsg *ccmsg);

  void addSharer(ulong addr, NetDevice_t netID, CacheState ccState=INV);
  int  getNumSharers(ulong addr);
  void setOwner(ulong addr, NetDevice_t ownerID);
  void removeAllSharers(ulong addr);
  void removeSharer(ulong addr, NetDevice_t netID);

  void invAck(IntlMemRequest *ireq);
  void missingLineAck(IntlMemRequest *ireq);

  //bool directoryIsBusy(ulong addr);
  void setMissingBit(ulong addr);

  MPDirCache::Line *findLine(ulong addr);

  bool isInVictimDir(ulong addr) {
    if( victim->findLine(addr) != NULL )
      return true;
    else
      return false;
  }

  void addToVictimDir(ulong addr, MPDirCache::Line *l);

  bool removeLineFromVictimDir(ulong addr);  
  bool checkVictimDir(ulong addr);

};


#endif
