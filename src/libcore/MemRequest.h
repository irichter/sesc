/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela

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

#ifndef MEMREQUEST_H
#define MEMREQUEST_H

#include <vector>
#include <stack>

#include "nanassert.h"
#include "ThreadContext.h"
#include "DInst.h"
#include "Message.h"

// TOFIX: remove MemOperation, and convert it to a bool (read/write)
// If someone needs more state, it should extend the class, not add it
// here

enum MemOperation {
  MemRead = 0,
  MemWrite,
  MemPush,
  MemFetchOp,   // Atomic instruction with read/write. Must behave like a read,
		// but acquire cache line in exclusive

  // Cache Coherence Specific
  MemReadW,                  // Preserve store, return in modify or exclusive.
  MemReadX,                  // Provide Info for Exclusive Read
  MemInvalidate,
  MemUpdate,
  MemRmSharer,
};

#ifndef DEBUGCONDITION
#ifdef DEBUG
#define DEBUGCONDITION 0
#else
#define DEBUGCONDITION 0
#endif
#endif

class MemObj;
class GMemorySystem;
class IBucket;

#ifdef TASKSCALAR
class VMemReq;
#endif


// MemRequest has lots of functionality that it is memory backend
// dependent, some fields are used by some backends, while others are
// used by other backends. This kind of specific backend functionality
// should be hidden from the shared interface.
//
// TOFIX: Someone remove the mutate (it is ugly as hell)

class MemRequest {
private:
  virtual void destroy() = 0;
protected:

  ID(static int numMemReqs;)

  // Called through callback
  void access();
  void returnAccess();
  std::stack<MemObj*> memStack;


  DInst *dinst;
  MemObj *currentMemObj;
  PAddr  pAddr; // physical address
  MemOperation memOp;

  bool dataReq; // data or icache request
  bool prefetch;
  ID(bool acknowledged;)

#ifdef SESC_DSM // libdsm stuff
  bool local;      // local or remote request
  Message *msg;  // if remote, which msg was used to request data
#endif

  void setFields(DInst *d, MemOperation mop, MemObj *mo) {
    dinst = d;
    memOp = mop;
    currentMemObj = mo;
  }

#ifdef TASKSCALAR
  VMemReq *vmemReq;
#endif

  long wToRLevel;

  ID(int reqId;) // It can be used as a printf trigger

  MemRequest();
  virtual ~MemRequest();

  StaticCallbackMember0<MemRequest, &MemRequest::access> accessCB;
  StaticCallbackMember0<MemRequest, &MemRequest::returnAccess> returnAccessCB;

public:
  ID(int id() const { return reqId; })

  MemOperation getMemOperation() const { return memOp; }

  //Call for WB cache BEFORE pushing next level
  void mutateWriteToRead() {
    if (memOp == MemWrite) {
      I(wToRLevel == -1);
      memOp = MemReadW;
      wToRLevel = memStack.size();
    }
  }

  bool isDataReq() const { return dataReq; }
  bool isPrefetch() const {return prefetch; }
  void markPrefetch() {
    I(!prefetch);
    prefetch = true;
  }
 
#ifdef SESC_DSM // libdsm stuff
  // local is true by default
  bool isLocal() const { return local; }
  void setLocal()  { local = true;  }
  void setRemote() { local = false; }

  // msg is empty by default
  Message *getMessage() {
    return msg;
  }
  void setMessage(Message *newmsg) {
    msg = newmsg;
  }
#endif

  bool isWrite() const { return wToRLevel != -1 || memOp == MemWrite; }

  void mutateReadToReadX() {
    I( memOp == MemRead );
    memOp = MemReadX;
  }

  void goDown(TimeDelta_t lat, MemObj *newMemObj) {
    memStack.push(currentMemObj);
    currentMemObj = newMemObj;
    accessCB.schedule(lat);
  }

  void goDownAbs(Time_t time, MemObj *newMemObj) {
    memStack.push(currentMemObj);
    currentMemObj = newMemObj;
    accessCB.scheduleAbs(time);
  }

  bool isTopLevel() const { return memStack.empty();  }

  void goUp(TimeDelta_t lat) {
    if (memStack.empty()) {
      ack(lat);
      destroy();
      return;
    }
    currentMemObj = memStack.top();
    memStack.pop();
    
    returnAccessCB.schedule(lat);
  }

  void goUpAbs(Time_t time) {
    if (memStack.empty()) {
      ack(time - globalClock);
      destroy();
      return;
    }
    currentMemObj = memStack.top();
    memStack.pop();
    
    returnAccessCB.scheduleAbs(time);
  }

#if 0
  //  use getVersionRef
  Pid_t getPid() const {
    I(dinst);
    return dinst->getPid();
  }
#endif

  DInst *getDInst() {
    return dinst;
  }

  MemObj *getCurrentMemObj() const { return currentMemObj; }

#ifdef TASKSCALAR
  void setCurrentMemObj(MemObj *obj) {
    currentMemObj = obj;
  }
  
  void setVMemReq(VMemReq *req) {
    vmemReq = req;
  }
  VMemReq *getVMemReq() const {
    return vmemReq;
  }
  GLVID *getLVID() const { 
    I(dinst);
    return dinst->getLVID();   
  }
  SubLVIDType getSubLVID() const { 
    I(dinst);
    return dinst->getSubLVID();   
  }
  void notifyDataDepViolation() {
    I(dinst);
    dinst->notifyDataDepViolation(DataDepViolationAtExe,true);
  }
  bool hasDataDepViolation() const {
    I(dinst);
    return dinst->hasDataDepViolation();
  }
  const HVersion *getRestartVerRef() const { 
    I(dinst);
    return dinst->getRestartVerRef();
  }
  const HVersion *getVersionRef() const { 
    I(dinst);
    return dinst->getVersionRef();
  }
#endif

  PAddr getPAddr() const { return pAddr; }
  void  setPAddr(PAddr a) {
    pAddr = a; 
  }

  virtual VAddr getVaddr() const=0;
  virtual void ack(TimeDelta_t lat) =0;
};

class DMemRequest : public MemRequest {
  // MemRequest specialized for dcache
 private:
  static pool<DMemRequest, true> actPool;
  friend class pool<DMemRequest, true>;

  void destroy();
  static void dinstAck(DInst *dinst, MemOperation memOp, TimeDelta_t lat);

 protected:
 public:
  static void create(DInst *dinst, GMemorySystem *gmem, MemOperation mop);

  VAddr getVaddr() const;
  void ack(TimeDelta_t lat);
};

class IMemRequest : public MemRequest {
  // MemRequest specialed for icache
 private:
  static pool<IMemRequest, true> actPool;
  friend class pool<IMemRequest, true>;

  IBucket *buffer;

  void destroy();

 protected:
 public:
  static void create(DInst *dinst, GMemorySystem *gmem, IBucket *buffer);

  VAddr getVaddr() const;
  void ack(TimeDelta_t lat);
};

class CBMemRequest : public MemRequest {
  // Callback MemRequest. Ideal for internal memory requests
 private:
  static pool<CBMemRequest, true> actPool;
  friend class pool<CBMemRequest, true>;

  CallbackBase *cb;

  void destroy();

 protected:
 public:
  static CBMemRequest *create(TimeDelta_t lat, MemObj *m
			      ,MemOperation mop, PAddr addr, CallbackBase *cb);

  VAddr getVaddr() const;
  void ack(TimeDelta_t lat);
};

class StaticCBMemRequest : public MemRequest {
  // Callback MemRequest. Ideal for internal memory requests
 private:
  void destroy();

  StaticCallbackBase *cb;
  bool ackDone;

 protected:
 public:
  StaticCBMemRequest(StaticCallbackBase *cb);

  void launch(TimeDelta_t lat, MemObj *m, MemOperation mop, PAddr addr);

  VAddr getVaddr() const;
  void ack(TimeDelta_t lat);
};

class MemRequestHashFunc {
public: 
  size_t operator()(const MemRequest *mreq) const {
    size_t val = (size_t)mreq;
    return val>>2;
  }
};

#endif   // MEMREQUEST_H
