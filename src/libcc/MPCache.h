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

#ifndef MPCACHE_H
#define MPCACHE_H

#include <queue>

#include "estl.h"
#include "CacheCore.h"
#include "GStats.h"
#include "Port.h"
#include "MemRequest.h"
#include "MemObj.h"
#include "Snippets.h"
#include "ThreadContext.h"
#include "UglyMemRequest.h"

class MemorySystem;

class MPCState : public StateGeneric<MPCState> {
private:
  ulong state;
public:
  ID(Time_t start);

  static const int MP_INV        = 0x00000000;
  static const int MP_SHRD       = 0x00000001;
  static const int MP_EX         = 0x00000002;
  static const int MP_MOD        = 0x00000003;
  static const int MP_DIRTY      = 0x00000004;
  static const int MP_STATE_MASK = 0x00000003;

  static const int MP_RDLOCKED   = 0x80000000;    
  static const int MP_WRLOCKED   = 0x40000000;
  static const int MP_GLOCKED    = 0x20000000;
  static const int MP_INVREQ     = 0x08000000;
  static const int MP_UPDREQ     = 0x04000000;
  static const int MP_UPLEVEL    = 0x00800000;

  MPCState(ulong istate = MP_INV) { state = istate; }

  bool isState(ulong test) const { return ((state&MP_STATE_MASK) == test); }
  void setState(ulong st) { state = ( ((~MP_STATE_MASK)&state)|st ); }
  MPCState getState() const { return MPCState(state); }

  bool isLocked()    const { return( isRdLocked() || isWrLocked() || isGLocked() ); }
  bool isWrLocked()  const { return( (state&MP_WRLOCKED) ); }
  bool isRdLocked()  const { return( (state&MP_RDLOCKED) ); }
  bool isGLocked()   const { return( (state&MP_GLOCKED)); }
  bool isDirty()     const { return(state&MP_DIRTY);}
  bool isInvReq()    const { return(state&MP_INVREQ);}
  bool isUpdateReq() const { return(state&MP_UPDREQ);}

  void setWrLock()    { state |= MP_WRLOCKED;}
  void setRdLock()    { state |= MP_RDLOCKED;}
  void setGLock()     { state |= MP_GLOCKED;}
  void setInvReq()    { state |= MP_INVREQ;}
  void setUpdateReq() { state |= MP_UPDREQ; }
  void setLocks()     { state |= (MP_RDLOCKED | MP_WRLOCKED);}
  void setDirty()     { state |= MP_DIRTY;}

  void clrWrLock()    { state &= (~MP_WRLOCKED);}
  void clrRdLock()    { state &= (~MP_RDLOCKED);}
  void clrGLock()     { state &= (~MP_GLOCKED);}
  void clrLocks()     { state &= (~(MP_RDLOCKED|MP_WRLOCKED));}
  void clrDirty()     { state &= (~MP_DIRTY);}
  void clrUpdateReq() { state &= (~MP_UPDREQ);}
  void clrInvReq()    { state &= (~MP_INVREQ);}

  bool operator==(MPCState b) const {
    return ( state == b.state );
  }
  
  MPCState &operator=(const MPCState &other) {
    this->state = other.state;
    return *this;
  }
};

class MPCache: public MemObj {
public:
  typedef CacheGeneric<MPCState,PAddr> CacheGenericType;

protected:

  CacheGenericType *cache;

  Time_t      busyUntil;

  PortGeneric *cachePort;
  TimeDelta_t occ;
  long defaultMask;
  TimeDelta_t missDelay;
  TimeDelta_t hitDelay;
  ushort outsLoads;
  ushort MaxOutsLoads;
  ushort ldStBufSize;
  ushort log2WrBufTag;
  ushort ldStBufStall;
  bool   notifyOfEviction;

  //Types of cache stall
  bool   lineFillStall;

  MPCache *mpLower;

  MemRequest *stalledMreq;
  PAddr stalledSet;

  /* MPCState For self invalidations */
  typedef HASH_MAP<int, MPCState> AddrStateMap;
  AddrStateMap selfInvMap; 
  /*Mapping an address to its state before invalidation*/

  /* BBF: Enqueued requests */
  typedef HASH_MAP<int, std::queue<MemRequest *> *> PenReqMapper;
  PenReqMapper pLdStReqs;
  PenReqMapper pInvUpdReqs;

  std::queue<MemRequest *> waitList;
  
  /* BBF: original Cache from here */
  
  GStatsCntr readHit;
  GStatsCntr readHalfMiss;
  GStatsCntr readMiss;
  GStatsCntr writeHit;
  GStatsCntr writeMiss;
  GStatsCntr writeBack;
  GStatsCntr lineFill;
  
  Time_t occupyPort() {
    return cachePort->nextSlot();
  }

  void issueMemReq(MemRequest *mreq, TimeDelta_t waitTime);
  void issueWrBackReq(PAddr paddr, TimeDelta_t waitTime);

  void registerWaitingReq(MemRequest *mreq) {
    GLOG(DEBUGCONDITION,"[C %llu] %s enqueues [%u]", globalClock, symbolicName, mreq->id());
    waitList.push(mreq);
  }

  void unBlockRequests(PAddr addr);
  void checkLineFillLock(PAddr addr);

  void checkBufferLock();

  void commonFastMiss(MemRequest *mreq, bool is_read);

  inline void setLineFillLock() {
    lineFillStall = true;
  }

  inline void unsetLineFillLock() {
    lineFillStall = false;
  }

  inline bool isStalled() const {
    return lineFillStall;
  }

public:
  typedef CacheGeneric<MPCState,PAddr>::CacheLine Line;

  MPCache(MemorySystem* current, const char *descr_section,
	  const char *name = NULL);
  virtual ~MPCache();

  void access(MemRequest *mreq);
  void returnAccess(MemRequest *mreq);

  void invalidate(PAddr addr, ushort size, MemObj *oc);
  bool canAcceptStore(PAddr addr) const;

  Line *findLine(PAddr addr) {
    return cache->findLine(addr);
  }
  
  //MemOperation virtual functions. These provide basic
  //functionality for all derived classes.
  virtual void read(MemRequest *mreq);
  virtual void write(MemRequest *mreq);
  virtual void invalidate(MemRequest *mreq);
  virtual void update(MemRequest *mreq);

  virtual void returnRead(MemRequest *mreq);
  virtual void returnInvalidate(MemRequest *mreq);
  virtual void returnUpdate(MemRequest *mreq);

  /*virtual void handleUpdate(MemRequest *mreq);*/
  virtual void handleInvalidate(MemRequest *mreq);

  void mpFreeLine(PAddr addr);

  //Basic cache functionality
  Time_t getNextFreeCycle() {
    return cachePort->calcNextSlot();
  }
  static void StaticInvalidateReturn(IntlMemRequest *ireq) {
    ((MPCache*)ireq->getWorkData())->handleInvalidate(ireq);
  }
  
protected:

  virtual void upperLevelInvalidate(PAddr paddr, MPCState st);

};

#endif


