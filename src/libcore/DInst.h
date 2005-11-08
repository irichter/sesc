/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Luis Ceze

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
#include "GStats.h"

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

#if (defined SESC_SEED)
#define DINST_PARENT 1
#endif

// FIXME: do a nice class. Not so public
class DInstNext {
 private:
  DInst *dinst;
#ifdef DINST_PARENT
  DInst *parentDInst;
#endif
 public:
  DInstNext() {
    dinst = 0;
  }

  DInstNext *nextDep;
  bool       isUsed; // true while non-satisfied RAW dependence
  const DInstNext *getNext() const { return nextDep; }
  DInstNext *getNext() { return nextDep; }
  void setNextDep(DInstNext *n) {
    nextDep = n;
  }

  void init(DInst *d) {
    I(dinst==0);
    dinst = d;
  }

  DInst *getDInst() const { return dinst; }

#ifdef DINST_PARENT
  DInst *getParentDInst() const { return parentDInst; }
  void setParentDInst(DInst *d) {
    GI(d,isUsed);
    parentDInst = d;
  }
#else
  void setParentDInst(DInst *d) { }
#endif
};

class DInst {
 public:
  // In a typical RISC processor MAX_PENDING_SOURCES should be 2
  static const int MAX_PENDING_SOURCES=2;

private:

  static pool<DInst> dInstPool;

  DInstNext pend[MAX_PENDING_SOURCES];
  DInstNext *last;
  DInstNext *first;
  
  int cId;

  // BEGIN Boolean flags
  bool loadForwarded;
  bool issued;
  bool executed;
  bool depsAtRetire;
  bool deadStore;
  bool deadInst;
  bool waitOnMemory;

  bool resolved; // For load/stores when the address is computer, for
		 // the rest of instructions when it is executed

#ifdef SESC_CHERRY
  bool earlyRecycled;
  bool canBeRecycled;
  bool memoryIssued;
  bool registerRecycled;
#endif

#ifdef SESC_MISPATH
  bool fake;
#endif
  // END Boolean flags

  // BEGIN Time counters
  Time_t wakeUpTime;
  // END Time counters

#ifdef SESC_BAAD
  static int fetchQSize;
  static int issueQSize;
  static int schedQSize;
  static int exeQSize;
  static int retireQSize;

  static GStatsTimingHist *fetchQHist1;
  static GStatsTimingHist *issueQHist1;
  static GStatsTimingHist *schedQHist1;
  static GStatsTimingHist *exeQHist1;
  static GStatsTimingHist *retireQHist1;
  
  static GStatsHist *fetchQHist2;
  static GStatsHist *issueQHist2;
  static GStatsHist *schedQHist2;
  static GStatsHist *exeQHist2;
  static GStatsHist *retireQHist2;

  static GStatsHist *fetchQHistUp;
  static GStatsHist *issueQHistUp;
  static GStatsHist *schedQHistUp;
  static GStatsHist *exeQHistUp;
  static GStatsHist *retireQHistUp;

  static GStatsHist *fetchQHistDown;
  static GStatsHist *issueQHistDown;
  static GStatsHist *schedQHistDown;
  static GStatsHist *exeQHistDown;
  static GStatsHist *retireQHistDown;

  static GStatsHist **avgFetchQTime;
  static GStatsHist **avgIssueQTime;
  static GStatsHist **avgSchedQTime;
  static GStatsHist **avgExeQTime;
  static GStatsHist **avgRetireQTime;
  Time_t fetchTime;
  Time_t renameTime;
  Time_t issueTime;
  Time_t schedTime;
  Time_t exeTime;
#endif

#ifdef BPRED_UPDATE_RETIRE
  BPredictor *bpred;
  InstID oracleID;
#endif

  const Instruction *inst;
  VAddr vaddr;
  Resource    *resource;
  DInst      **RATEntry;
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

#ifdef SESC_SEED
  DInst *predParent;
  int    nDepTableEntries;
  long   bank;
  long   xtraBank;
  bool   predParentSrc1;
  bool   overflowing;
  bool   stallOnLoad;
#endif

  char nDeps;              // 0, 1 or 2 for RISC processors

#ifdef DEBUG
 public:
  static long currentID;
  long ID; // static ID, increased every create (currentID). pointer to the
  // DInst may not be a valid ID because the instruction gets recycled
#endif
 protected:
 public:
  DInst();

  void doAtSimTime();
  StaticCallbackMember0<DInst,&DInst::doAtSimTime>  doAtSimTimeCB;

  void doAtSelect();
  StaticCallbackMember0<DInst,&DInst::doAtSelect>  doAtSelectCB;

  DInst *clone();

  void doAtExecuted();
  StaticCallbackMember0<DInst,&DInst::doAtExecuted> doAtExecutedCB;


  static DInst *createInst(InstID pc, VAddr va, int cId);
  static DInst *createDInst(const Instruction *inst, VAddr va, int cId);

  void killSilently();
  void scrap(); // Destroys the instruction without any other effects
  void destroy();

  void setResource(Resource *res) {
    I(!resource);
    resource = res;
  }
  Resource *getResource() const { return resource; }

  void setRATEntry(DInst **rentry) {
    I(!RATEntry);
    RATEntry = rentry;
  }

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

#ifdef DINST_PARENT
  DInst *getParentSrc1() const { 
    if (pend[0].isUsed)
      return pend[0].getParentDInst(); 
    return 0;
  }
  DInst *getParentSrc2() const { 
    if (pend[1].isUsed)
      return pend[1].getParentDInst();
    return 0;
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
    DInst *n = first->getDInst();

    I(n);

    I(n->nDeps > 0);
    n->nDeps--;

    first->isUsed = false;
    first->setParentDInst(0);
    first = first->getNext();

    return n;
  }

  void addSrc1(DInst * d) {
    I(d->nDeps < MAX_PENDING_SOURCES);
    d->nDeps++;
    
    DInstNext *n = &d->pend[0];
    I(!n->isUsed);
    n->isUsed = true;
    n->setParentDInst(this);

    I(n->getDInst() == d);
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
    d->nDeps++;

    I(!d->waitOnMemory); // pend[1] reused on memory ops. Not both! 

    DInstNext *n = &d->pend[1];
    I(!n->isUsed);
    n->isUsed = true;
    n->setParentDInst(this);

    I(n->getDInst() == d);
    if (first == 0) {
      first = n;
    } else {
      last->nextDep = n;
    }
    n->nextDep = 0;
    last = n;
  }

  void addFakeSrc(DInst * d) {
    I(d->nDeps < MAX_PENDING_SOURCES);
    d->nDeps++;
    I(!d->waitOnMemory);
    d->waitOnMemory = true;

    DInstNext *n = &d->pend[1];
    I(!n->isUsed);
    n->isUsed = true;
    n->setParentDInst(this);

    I(n->getDInst() == d);
    if (first == 0) {
      first = n;
    } else {
      last->nextDep = n;
    }
    n->nextDep = 0;
    last = n;
  }

  char getnDeps() const { return nDeps; }

#ifdef SESC_SEED
  void addDepTableEntry() {
    nDepTableEntries++;
  }
  int getnDepTableEntries() const { return nDepTableEntries; }
  long getXtraBank() const { return xtraBank; }
  void setXtraBank(long i) {
    xtraBank = i;
  }
  long getBank() const { return bank; }
  void setBank(long i) {
    bank = i;
  }
  // No getPredParent because predParent can be an stale pointer. Only parent
  // should access (it may retire)
  bool isOverflowing() const {  return overflowing; }
  void setOverflowing()   {  overflowing = true; }

  bool isStallOnLoad() const {  
    return inst->isLoad() && stallOnLoad; 
  }
  void setStallOnLoad() {
    I(inst->isLoad());
    stallOnLoad = true; 
  }

  bool isPredParentSrc1() const { 
    I(predParent); 
    return predParentSrc1; 
  }
  DInst *getPredParent() const { return predParent; }
  void setPredParent(DInst *p, bool src1=false) {
    predParent = p;
    predParentSrc1 = src1;
  }
#else
  bool isStallOnLoad() const {  return false; }
#endif

  bool isSrc1Ready() const { return !pend[0].isUsed; }
  bool isSrc2Ready() const { return !pend[1].isUsed; }
  bool isJustWaitingOnMemory() const { return !pend[0].isUsed && waitOnMemory; }
  bool hasDeps()     const { 
    GI(!pend[0].isUsed && !pend[1].isUsed, nDeps==0);
    return nDeps!=0;
  }
  bool hasPending()  const { return first != 0;  }
  const DInst *getFirstPending() const { return first->getDInst(); }
  const DInstNext *getFirst() const { return first; }

  const Instruction *getInst() const { return inst; }

  VAddr getVaddr() const { return vaddr;  }

  int getContextId() const { return cId; }

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
#ifdef SESC_CHERRY
    resolved = true;
#endif
  }

  bool isDeadStore() const { return deadStore; }
  void setDeadStore() { 
    I(!deadStore);
    I(!hasPending());
    deadStore = true; 
  }

  void setDeadInst() { deadInst = true; }
  bool isDeadInst() { return deadInst; }
  
  bool hasDepsAtRetire() const { return depsAtRetire; }
  void setDepsAtRetire() { 
    I(!depsAtRetire);
    depsAtRetire = true;
  }
  void clearDepsAtRetire() { 
    I(depsAtRetire);
    depsAtRetire = false;
  }

  bool isResolved() const { return resolved; }
  void markResolved() { 
    resolved = true; 
  }

#ifdef SESC_CHERRY
  bool isEarlyRecycled() const { return earlyRecycled; }
  void setEarlyRecycled() { earlyRecycled = true; }
  bool hasRegisterRecycled() const { return registerRecycled; }
  void setRegisterRecycled() { registerRecycled = true; }
#else
  bool isEarlyRecycled() const { return false; }
#endif

#ifdef SESC_MISPATH
  void setFake() { 
    I(!fake);
    fake = true; 
  }
  bool isFake() const  { return fake; }
#else
  bool isFake() const  { return false; }
#endif

#ifdef SESC_CHERRY
  bool hasCanBeRecycled() const { return canBeRecycled; }
  void setCanBeRecycled() { canBeRecycled = true; }

  bool isMemoryIssued() const { return memoryIssued; }
  void setMemoryIssued() {  memoryIssued  = true; }
#endif

  void awakeRemoteInstructions();

  void setWakeUpTime(Time_t t)  { 
    // ??? FIXME: Why fails?I(wakeUpTime <= t); // Never go back in time
    //I(wakeUpTime <= t);
    wakeUpTime = t;
  }

  Time_t getWakeUpTime() const { return wakeUpTime; }

#ifdef SESC_BAAD
  void setFetchTime();
  void setRenameTime();
  void setIssueTime();
  void setSchedTime();
  void setExeTime();
  void setRetireTime();
#endif

#ifdef DEBUG
  long getID() const { return ID; }
#endif
};

class Hash4DInst {
 public: 
  size_t operator()(const DInst *dinst) const {
    return (size_t)(dinst);
  }
};

#endif   // DINST_H
