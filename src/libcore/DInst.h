/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau

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
#ifndef DINST_H
#define DINST_H

#include "pool.h"
#include "nanassert.h"

#include "ThreadContext.h"
#include "Instruction.h"
#include "callback.h"

#include "Snippets.h"

#ifdef TASKSCALAR
#include "GLVID.h"
enum DataDepViolationAt { DataDepViolationAtExe=0, DataDepViolationAtFetch, 
			  DataDepViolationAtRetire, DataDepViolationAtMax };
#endif

#if (defined TLS)
#include "Epoch.h"
#endif

class Resource;
class FetchEngine;
class FReg;
class BPredictor;

class DInst {
 public:
  // In a typical RISC processor MAX_PENDING_SOURCES should be 2
  static const int MAX_PENDING_SOURCES=2;

private:
  class DInstNext {
  public:
    DInst     *dinst;
    DInstNext *nextDep;
    bool       isUsed; // true while non-satisfied RAW dependence
  };

  static pool<DInst> dInstPool;

  DInstNext pend[MAX_PENDING_SOURCES];
  DInstNext *last;
  DInstNext *first;
  
  int cpuId;

  bool loadForwarded;
  bool issued;
  bool executed;
  bool depsAtRetire;
  bool deadStore;

#ifdef SESC_MISPATH
  bool fake;
#endif

#ifdef TS_CHERRY
  bool earlyRecycled;
  bool canBeRecycled;
  bool memoryIssued;
#endif

#ifdef BPRED_UPDATE_RETIRE
  BPredictor *bpred;
  InstID oracleID;
#endif

  const Instruction *inst;
  VAddr vaddr;
  Resource    *resource;
  FetchEngine *fetch;

#ifdef TASKSCALAR
  int         dataDepViolationID;
  HVersion   *restartVer;
  HVersion   *lvidVersion;
  GLVID      *lvid;
  SubLVIDType subLVID;

#endif

  CallbackBase *pendEvent;

#if (defined TLS)
  tls::Epoch *myEpoch;
#endif // (defined TLS)

#ifdef DEBUG
  char nDeps;              // 0, 1 or 2 for RISC processors
  static long currentID;
  long ID; // static ID, increased every create (currentID). pointer to the
  // DInst may not be a valid ID because the instruction gets recycled
#endif
		
protected:
public:
  DInst();

  void doAtExecuted();
  StaticCallbackMember0<DInst,&DInst::doAtExecuted> doAtExecutedCB;

  static DInst *createInst(InstID pc, VAddr va, int cpuId);
  static DInst *createDInst(const Instruction *inst, VAddr va, int cpuId);

  void killSilently();
  void scrap(); // Destroys the instruction without any other effects
  void destroy();

  void setResource(Resource *res) {
    I(!resource);
    resource = res;
  }
  Resource *getResource() const { return resource; }

#ifdef BPRED_UPDATE_RETIRE
  void setBPred(BPredictor *bp, InstID oid) {
    I(oracleID==0);
    I(bpred == 0);
    bpred     = bp;
    oracleID = oid;
  }
#endif

#ifdef TASKSCALAR
  void addDataDepViolation(const HVersion *ver);
  // API for libvmem to notify the detection of the invalid memory
  // access
  void notifyDataDepViolation(DataDepViolationAt dAt=DataDepViolationAtExe, bool val=false);
  bool hasDataDepViolation() const {
    return restartVer != 0;
  }
  const HVersion *getRestartVerRef() const { return restartVer; }
  void setLVID(GLVID *b, HVersion *lvidV) {
    I(lvid==0);
    lvid        = b;
    subLVID     = lvid->getSubLVID();

    I(lvidVersion==0);
    lvidVersion = lvidV->duplicate();
    lvidVersion->incOutsReqs();
  }
  GLVID   *getLVID() const { return lvid; }
  SubLVIDType getSubLVID() const { return subLVID; }
  const HVersion *getVersionRef() const { return lvidVersion; }


#endif //TASKSCALAR

#if (defined TLS)
  void setEpoch(tls::Epoch *epoch){
    myEpoch=epoch;
  }
#endif

  void setFetch(FetchEngine *fe) {
    I(!isFake());

    fetch = fe;
  }

  FetchEngine *getFetch() const {
    return fetch;
  }

  void addEvent(CallbackBase *cb) {
    I(inst->isEvent());
    I(pendEvent == 0);
    pendEvent = cb;
  }

  CallbackBase *getPendEvent() {
    return pendEvent;
  }

  DInst *getNextPending() {
    I(first);
    DInst *n = first->dinst;

    I(n);
    I(n->nDeps > 0);

    IS(n->nDeps--);
    first->isUsed = false;
    first = first->nextDep;

    return n;
  }

  void addSrc1(DInst * d) {
    I(d->nDeps < MAX_PENDING_SOURCES);
    IS(d->nDeps++);
    
    DInstNext *n = &d->pend[0];
    I(!n->isUsed);
    n->isUsed = true;
    
    I(n->dinst == d);
    if (first == 0) {
      first = n;
    } else {
      last->nextDep = n;
    }
    n->nextDep = 0;
    last = n;
  }

  void addSrc2(DInst * d) {
    I(d->nDeps < MAX_PENDING_SOURCES);
    IS(d->nDeps++);
    
    DInstNext *n = &d->pend[1];
    I(!n->isUsed);
    n->isUsed = true;

    I(n->dinst == d);
    if (first == 0) {
      first = n;
    } else {
      last->nextDep = n;
    }
    n->nextDep = 0;
    last = n;
  }

  bool isSrc1Ready() const { return !pend[0].isUsed; }
  bool isSrc2Ready() const { return !pend[1].isUsed; }
  bool hasDeps()     const { return pend[0].isUsed || pend[1].isUsed; }
  bool hasPending()  const { return first != 0;  }
  const DInst *getLastPending() const { return last->dinst; }
  const DInst *getFirstPending() const { return first->dinst; }

  const Instruction *getInst() const { return inst; }

  VAddr getVaddr() const { return vaddr;  }

  int getCPUId() const { return cpuId; }

  void dump(const char *id);

  // methods required for LDSTBuffer
  bool isLoadForwarded() const { return loadForwarded; }
  void setLoadForwarded() {
    I(!loadForwarded);
    loadForwarded=true;
  }

  bool isIssued() const { return issued; }
  void markIssued() {
    I(!issued);
    I(!executed);
    issued = true;
  }

  bool isExecuted() const { return executed; }
  void markExecuted() {
    I(issued);
    I(!executed);
    executed = true;
  }

  bool isDeadStore() const { return deadStore; }
  void setDeadStore() { 
    I(!deadStore);
    I(!hasPending());
    deadStore = true; 
  }

  bool hasDepsAtRetire() const { return depsAtRetire; }
  void setDepsAtRetire() { 
    I(!depsAtRetire);
    depsAtRetire = true;
  }
  void clearDepsAtRetire() { 
    I(depsAtRetire);
    depsAtRetire = false;
  }

#ifdef SESC_MISPATH
  void setFake() { 
    I(!fake);
    fake = true; 
  }
  bool isFake() const  { return fake; }
#else
  bool isFake() const  { return false; }
#endif

#ifdef TS_CHERRY
  bool isEarlyRecycled() const { return earlyRecycled; }
  void setEarlyRecycled() { earlyRecycled = true; }

  bool hasCanBeRecycled() const { return canBeRecycled; }
  void setCanBeRecycled() { canBeRecycled = true; }

  bool isMemoryIssued() const { return memoryIssued; }
  void setMemoryIssued() {  memoryIssued  = true; }
#else
  bool isEarlyRecycled() const { return false; }
#endif

  void awakeRemoteInstructions();

#ifdef DEBUG
  long getID() const { return ID; }
#endif
};

#endif   // DINST_H
