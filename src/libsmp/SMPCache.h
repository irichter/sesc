/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Karin Strauss

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

#ifndef SMPCACHE_H
#define SMPCACHE_H

#include "MemObj.h"
#include "SMemorySystem.h"
#include "SMPProtocol.h"
#include "SMPMemRequest.h"
#include "MESICacheState.h"
#include "MSHR.h"
#include "Port.h"

#include "vector"

class SMPCache : public MemObj {
public:
  typedef CacheGeneric<MESICacheState, PAddr, false>            CacheType;
  typedef CacheGeneric<MESICacheState, PAddr, false>::CacheLine Line;

private:
protected:
  CacheType *cache;

  PortGeneric *cachePort;
  TimeDelta_t missDelay;
  TimeDelta_t hitDelay;

  MSHR<PAddr> *outsReq;  // buffer for requests coming from upper levels
  static MSHR<PAddr> *mutExclBuffer;

  class Entry {
  public:
    int outsResps;        // outstanding responses: number of caches 
                          // that still need to acknowledge invalidates
    bool invalidate;
    CallbackBase *cb;
    Entry() {
      outsResps = 0;
      cb = 0;
      invalidate = false;
    }
  };

  typedef HASH_MAP<PAddr, Entry> PendInvTable;

  PendInvTable pendInvTable; // pending invalidate table

  // BEGIN statistics
  GStatsCntr readHit;
  GStatsCntr writeHit;
  GStatsCntr readMiss;
  GStatsCntr writeMiss;
  GStatsCntr lineFill;
  
  // TODO: fill this up
  // END statistics

  SMPProtocol *protocol;

  // interface with upper level
  void read(MemRequest *mreq);
  void write(MemRequest *mreq);
  void pushline(MemRequest *mreq);
  void specialOp(MemRequest *mreq);

  typedef CallbackMember1<SMPCache, MemRequest *,
                          &SMPCache::read> readCB;
  typedef CallbackMember1<SMPCache, MemRequest *,
                          &SMPCache::write> writeCB;
  typedef CallbackMember1<SMPCache, MemRequest *,
                          &SMPCache::specialOp> specialOpCB;

  // port usage accounting
  Time_t nextSlot() {
    return cachePort->nextSlot();
  }

  // local routines
  void doRead(MemRequest *mreq);
  void doWrite(MemRequest *mreq);
  void doPushLine(MemRequest *mreq);
  void doWriteBack(PAddr addr) const;
  void sendRead(MemRequest* mreq);
  void sendWrite(MemRequest* mreq);

  typedef CallbackMember1<SMPCache, MemRequest *, 
                         &SMPCache::doRead> doReadCB;
  typedef CallbackMember1<SMPCache, MemRequest *,
                         &SMPCache::doWrite> doWriteCB;
  typedef CallbackMember1<SMPCache, MemRequest *,
                         &SMPCache::doPushLine> doPushLineCB;
  typedef CallbackMember1<SMPCache, MemRequest *,
                         &SMPCache::sendRead> sendReadCB;
  typedef CallbackMember1<SMPCache, MemRequest *,
                         &SMPCache::sendWrite> sendWriteCB;

public:
  SMPCache(SMemorySystem *gms, const char *section, const char *name);
  ~SMPCache();

  // BEGIN MemObj interface
  
  // port usage accounting
  Time_t getNextFreeCycle() const;

  // interface with upper level
  bool canAcceptStore(PAddr addr) const;
  void access(MemRequest *mreq);
  
  // interface with lower level
  void returnAccess(MemRequest *mreq);

  void invalidate(PAddr addr, ushort size, MemObj *oc);
  void doInvalidate(PAddr addr, ushort size);
  void realInvalidate(PAddr addr, ushort size);

  bool canAcceptStore(PAddr addr);

  // END MemObj interface

  ulong getLineSize() {
    return cache->getLineSize();
  }

  // BEGIN protocol interface 
  
  // access to lower level
  void sendBelow(SMPMemRequest *sreq);
  void respondBelow(SMPMemRequest *sreq);
  void receiveFromBelow(SMPMemRequest *sreq);

  // protocol has decided what to do and wants to finish the coherence
  // transaction, calling concludeAccess
  void concludeAccess(MemRequest *mreq);

  // line management - actions protocol can perform on cache lines
  Line *allocateLine(PAddr addr, CallbackBase *cb);
  void doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb);

  typedef CallbackMember3<SMPCache, PAddr, PAddr, CallbackBase *,
                         &SMPCache::doAllocateLine> doAllocateLineCB;
 
  Line *getLine(PAddr addr);
  void updateLine(PAddr addr, MESIState_t state);
  void SMPCache::writeLine(PAddr addr);
  void invalidateLine(PAddr addr, CallbackBase *cb);

  // interface to get pointer to another protocol
  // this should be extended for more complex protocols
  SMPProtocol *getProtocol() { return protocol; }

  // END protocol interface

  // Debug function
  Line* findLine(PAddr addr) { return cache->findLine(addr); }
  void inclusionCheck(PAddr addr);
};

#endif // SMPCACHE_H
