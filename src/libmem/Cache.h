/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Luis Ceze
                  Karin Strauss
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

#ifndef CACHE_H
#define CACHE_H

#include <queue>

#include "estl.h"
#include "CacheCore.h"
#include "GStats.h"
#include "Port.h"
#include "MemObj.h"
#include "MemorySystem.h"
#include "MemRequest.h"
#include "MSHR.h"
#include "Snippets.h"
#include "ThreadContext.h"

#ifdef TASKSCALAR
#include "HVersion.h"
#include "VMemReq.h"
#endif

class CState : public StateGeneric<> {
private:
  bool valid;
  bool dirty;
  bool locked;
public:
  CState() {
    valid = false;
    dirty = false;
    locked = false;
    clearTag();
  }
  bool isLocked() const {
    return (locked == true);
  }
  void lock() {
    I(valid);
    locked = true;
  }
  bool isDirty() const {
    return dirty;
  }
  void makeDirty() {
    dirty = true;
  }
  void makeClean() {
    dirty = false;
  }
  bool isValid() const {
    return valid;
  }
  void validate() {
    I(getTag());
    valid = true;
    locked = false;
  }
  void invalidate() {
    valid = false;
    dirty = false;
    locked = false;
    clearTag();
  }
};

class Cache: public MemObj
{
protected:
  typedef CacheGeneric<CState,PAddr> CacheType;
  typedef CacheGeneric<CState,PAddr>::CacheLine Line;

  const bool inclusiveCache;


  CacheType *cache;

  MSHR<PAddr> *mshr;
  
  typedef HASH_MAP<PAddr, int> WBuff; 

  WBuff wbuff;  // write buffer
  int maxPendingWrites;
  int pendingWrites;

  class Entry {
  public:
    int outsResps;        // outstanding responses: number of caches 
                          // that still need to acknowledge invalidates
    CallbackBase *cb;
    Entry() {
      outsResps = 0;
      cb = 0;
    }
  };

  typedef HASH_MAP<PAddr, Entry> PendInvTable;

  PendInvTable pendInvTable; // pending invalidate table

  PortGeneric *cachePort;

  long defaultMask;
  TimeDelta_t missDelay;
  TimeDelta_t hitDelay;
  TimeDelta_t fwdDelay;

  // BEGIN Statistics
  GStatsCntr readHalfMiss;
  GStatsCntr writeHalfMiss;
  GStatsCntr writeMiss;
  GStatsCntr readMiss;
  GStatsCntr readHit;
  GStatsCntr writeHit;
  GStatsCntr writeBack;
  GStatsCntr lineFill;
  GStatsCntr linePush;
  GStatsCntr nForwarded;
  GStatsCntr nWBFull;
  GStatsTimingAvg avgPendingWrites;
  GStatsAvg  avgMissLat;
  GStatsCntr rejectedHits;
  // END Statistics

  Time_t nextSlot() {
    return cachePort->nextSlot();
  }

  virtual void sendMiss(MemRequest *mreq) = 0;

  void doRead(MemRequest *mreq);
  void doReadQueued(MemRequest *mreq);
  virtual void doWrite(MemRequest *mreq);
  void doWriteQueued(MemRequest *mreq);
  void activateOverflow(MemRequest *mreq);

  void readMissHandler(MemRequest *mreq);
  void writeMissHandler(MemRequest *mreq);

  void wbuffAdd(PAddr addr);
  void wbuffRemove(PAddr addr);
  bool isInWBuff(PAddr addr);

  Line *allocateLine(PAddr addr, CallbackBase *cb);
  void doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb);
  void doAllocateLineRetry(PAddr addr, CallbackBase *cb);

  void doReturnAccess(MemRequest *mreq);

  virtual void doWriteBack(PAddr addr) = 0;
  virtual void inclusionCheck(PAddr addr) { }

  typedef CallbackMember1<Cache, MemRequest *, &Cache::doRead> 
    doReadCB;

  typedef CallbackMember1<Cache, MemRequest *, &Cache::doWrite> 
    doWriteCB;

  typedef CallbackMember1<Cache, MemRequest *, &Cache::doReadQueued> 
    doReadQueuedCB;  

  typedef CallbackMember1<Cache, MemRequest *, &Cache::doWriteQueued> 
    doWriteQueuedCB;

  typedef CallbackMember1<Cache, MemRequest *, &Cache::activateOverflow> 
    activateOverflowCB;

  typedef CallbackMember1<Cache, MemRequest *,
                         &Cache::doReturnAccess> doReturnAccessCB;

  typedef CallbackMember3<Cache, PAddr, PAddr, CallbackBase *,
                         &Cache::doAllocateLine> doAllocateLineCB;

  typedef CallbackMember2<Cache, PAddr, CallbackBase *,
                         &Cache::doAllocateLineRetry> doAllocateLineRetryCB;

public:
  Cache(MemorySystem *gms, const char *descr_section, 
	const char *name = NULL);
  virtual ~Cache();

  void access(MemRequest *mreq);
  virtual void read(MemRequest *mreq);
  virtual void write(MemRequest *mreq);
  virtual void pushLine(MemRequest *mreq) = 0;
  virtual void specialOp(MemRequest *mreq);
  virtual void returnAccess(MemRequest *mreq);

  virtual bool canAcceptStore(PAddr addr);
  virtual bool canAcceptLoad(PAddr addr);
  
  bool isInCache(PAddr addr) const;

  // same as above plus schedule callback to doInvalidate
  void invalidate(PAddr addr, ushort size, MemObj *oc);
  void doInvalidate(PAddr addr, ushort size);

  virtual const bool isCache() const { return true; }

  void dump() const;

  Time_t getNextFreeCycle() const;
};

class WBCache : public Cache {
protected:
  void sendMiss(MemRequest *mreq);
  void doWriteBack(PAddr addr); 

public:
  WBCache(MemorySystem *gms, const char *descr_section, 
	  const char *name = NULL);
  ~WBCache();

  void pushLine(MemRequest *mreq);
};

class WTCache : public Cache {
protected:
  void doWrite(MemRequest *mreq);
  void sendMiss(MemRequest *mreq);
  void doWriteBack(PAddr addr);
  void writePropagateHandler(MemRequest *mreq);
  void propagateDown(MemRequest *mreq);
  void reexecuteDoWrite(MemRequest *mreq);
  
  typedef CallbackMember1<WTCache, MemRequest *, &WTCache::reexecuteDoWrite> 
    reexecuteDoWriteCB;

  void inclusionCheck(PAddr addr);

public:
  WTCache(MemorySystem *gms, const char *descr_section, 
	  const char *name = NULL);
  ~WTCache();

  void pushLine(MemRequest *mreq);
};

class NICECache : public Cache
{
// a 100% hit cache, used for debugging or as main memory
protected:
  void sendMiss(MemRequest *mreq);
  void doWriteBack(PAddr addr);

public:
  NICECache(MemorySystem *gms, const char *section, const char *name = NULL);
  ~NICECache();

  void read(MemRequest *mreq);
  void write(MemRequest *mreq);
  void pushLine(MemRequest *mreq);
  void returnAccess(MemRequest *mreq);
  void specialOp(MemRequest *mreq);
};


#endif
