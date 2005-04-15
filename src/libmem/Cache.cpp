/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Luis Ceze
                  Jose Renau
		  Karin Strauss

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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <iostream>

#include "nanassert.h"

#include "SescConf.h"
#include "MemorySystem.h"
#include "Cache.h"
#include "MSHR.h"
#include "OSSim.h"
#ifdef TS_TIMELINE
#include "TraceGen.h"
#include "HVersion.h"
#endif

#include "SMPCache.h"

static const char 
  *k_numPorts="numPorts", *k_portOccp="portOccp",
  *k_hitDelay="hitDelay", *k_missDelay="missDelay";

Cache::Cache(MemorySystem *gms, const char *section, const char *name)
  : MemObj(section, name)
    // FIXME    inclusiveCache(SescConf->getBool(section, "inclusive"))
  ,inclusiveCache(true)
  ,readHalfMiss("%s:readHalfMiss", name)
  ,writeHalfMiss("%s:writeHalfMiss", name)
  ,writeMiss("%s:writeMiss", name)
  ,readMiss("%s:readMiss", name)
  ,readHit("%s:readHit", name)
  ,writeHit("%s:writeHit", name)
  ,writeBack("%s:writeBack", name)
  ,lineFill("%s:lineFill", name)
  ,linePush("%s:linePush", name)
  ,nForwarded("%s:nForwarded", name)
  ,nWBFull("%s:nWBFull", name)
  ,avgPendingWrites("%s:avgPendingWrites", name)
  ,avgMissLat("%s_avgMissLat", name)
  ,rejected("%s:rejected", name)
  ,rejectedHits("%s:rejectedHits", name)
{
  MemObj *lower_level = NULL;
  char busName[512];
  
  cache = CacheType::create(section, "", name);
  
  mshr = MSHR<PAddr>::create(name, section);
  
  I(gms);
  lower_level = gms->declareMemoryObj(section, k_lowerLevel);

  if(SescConf->checkBool(section,"SetMSHRL2")) {
    const char *cName = SescConf->getCharPtr(section,"l2cache");
    Cache *temp = dynamic_cast<Cache*>(gms->searchMemoryObj(true,cName));
    SingleMSHR<PAddr>::setL2Cache(temp);
  }

  if (lower_level != NULL)
      addLowerLevel(lower_level);

  // OK for a cache that always hits
  GMSG(lower_level == NULL, 
       "You are defining a cache with void as lowerLevel\n");

  SescConf->isLong(section, k_numPorts);
  SescConf->isLong(section, k_portOccp);

  cachePort = PortGeneric::create(name,SescConf->getLong(section, k_numPorts), 
				  SescConf->getLong(section, k_portOccp));

  SescConf->isLong(section, k_hitDelay);
  hitDelay = SescConf->getLong(section, k_hitDelay);

  SescConf->isLong(section, k_missDelay);
  missDelay = SescConf->getLong(section, k_missDelay);

  if (SescConf->checkLong(section, "fwdDelay")) 
    fwdDelay = SescConf->getLong(section, "fwdDelay");
  else
    fwdDelay = missDelay;

  pendingWrites = 0;
  if (SescConf->checkLong(section, "maxWrites")) 
    maxPendingWrites = SescConf->getLong(section, "maxWrites");
  else
    maxPendingWrites = -1;
  
  defaultMask  = ~(cache->getLineSize()-1);

}

Cache::~Cache()
{
  // nothing to do
}

void Cache::access(MemRequest *mreq) 
{
  I(mreq->getPAddr() > 1024);
  GMSG(mreq->getPAddr() <= 1024
       ,"mreq dinst=%p paddr=0x%x vaddr=0x%x memOp=%d ignored"
       ,mreq->getDInst()
       ,(uint) mreq->getPAddr()
       ,(uint) mreq->getVaddr()
       ,mreq->getMemOperation()
       );

  mreq->setClockStamp((Time_t) - 1);
  if(mreq->getPAddr() <= 1024) { // TODO: need to implement support for fences
    mreq->goUp(0); 
    return;
  }

  switch(mreq->getMemOperation()){
  case MemReadW:
  case MemRead:    read(mreq);       break;
  case MemWrite:   write(mreq);      break;
  case MemPush:    pushLine(mreq);   break;
  default:         specialOp(mreq);  break;
  }
}

void Cache::read(MemRequest *mreq)
{ 
  doReadCB::scheduleAbs(nextSlot(), this, mreq);
}

void Cache::doRead(MemRequest *mreq)
{
  Line *l = cache->readLine(mreq->getPAddr());

  if (l == 0) {
    if(isInWBuff(mreq->getPAddr())) {
      nForwarded.inc();
      mreq->goUp(fwdDelay);
      return;
    }
    readMissHandler(mreq);
    return;
  }

  readHit.inc();

  mreq->goUp(hitDelay);
}

void Cache::readMissHandler(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if(!mshr->issue(addr, MemRead)) {
    mreq->setClockStamp(globalClock);
    mshr->addEntry(addr, doReadQueuedCB::create(this, mreq), 
		   activateOverflowCB::create(this, mreq), MemRead);
    return;
  }

  // Added a new MSHR entry, now send request to lower level
  readMiss.inc();
  sendMiss(mreq);
}

void Cache::doReadQueued(MemRequest *mreq)
{
  PAddr paddr = mreq->getPAddr();
  readHalfMiss.inc();


  avgMissLat.sample(globalClock - mreq->getClockStamp());
  mreq->goUp(hitDelay);

  // the request came from the MSHR, we need to retire it
  mshr->retire(paddr);
}

// this is here just because we have no feedback from the memory
// system to the processor telling it to stall the issue of a request
// to the memory system. this will only be called if an overflow entry
// is activated in the MSHR and the is *no* pending request for the
// same line.  therefore, sometimes the callback associated with this
// function might not be called.
void Cache::activateOverflow(MemRequest *mreq)
{
  if(mreq->getMemOperation() == MemRead || mreq->getMemOperation() == MemReadW) {

    Line *l = cache->readLine(mreq->getPAddr());
    
    if (l == 0) {
      // no need to add to the MSHR, it is already there
      // since it came from the overflow
      readMiss.inc();
      sendMiss(mreq);
      return;
    }

    readHit.inc();
    PAddr paddr = mreq->getPAddr();
    mreq->goUp(hitDelay);
    
    // the request came from the MSHR overflow, we need to retire it
    mshr->retire(paddr);
    
    return;
  }
    
  I(mreq->getMemOperation() == MemWrite);

  Line *l = cache->writeLine(mreq->getPAddr());

  if (l == 0) {
    // no need to add to the MSHR, it is already there
    // since it came from the overflow
    writeMiss.inc();
    sendMiss(mreq);
    return;
  }

  writeHit.inc();
#ifndef SMP
  l->makeDirty();
#endif

  PAddr paddr = mreq->getPAddr();
  mreq->goUp(hitDelay);
  
  // the request came from the MSHR overflow, we need to retire it
  wbuffRemove(paddr);
  mshr->retire(paddr);
}

void Cache::write(MemRequest *mreq)
{ 
  doWriteCB::scheduleAbs(nextSlot(), this, mreq);
}

void Cache::doWrite(MemRequest *mreq)
{
  Line *l = cache->writeLine(mreq->getPAddr());

  if (l == 0) {
    writeMissHandler(mreq);
    return;
  }

  writeHit.inc();
  l->makeDirty();

  mreq->goUp(hitDelay);
}

void Cache::writeMissHandler(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if(!mshr->issue(addr, MemWrite)) {
    mreq->setClockStamp(globalClock);
    mshr->addEntry(addr, doWriteQueuedCB::create(this, mreq),
		   activateOverflowCB::create(this, mreq), MemWrite);
    return;
  }

  wbuffAdd(addr);
  writeMiss.inc();
  sendMiss(mreq);
}

void Cache::doWriteQueued(MemRequest *mreq)
{
  PAddr paddr = mreq->getPAddr();

  writeHalfMiss.inc();

  avgMissLat.sample(globalClock - mreq->getClockStamp());
  mreq->goUp(hitDelay);

  wbuffRemove(paddr);

  // the request came from the MSHR, we need to retire it
  mshr->retire(paddr);
}

void Cache::specialOp(MemRequest *mreq)
{
  mreq->goUp(1);
}

void Cache::returnAccess(MemRequest *mreq)
{
  if (mreq->getMemOperation() == MemPush) {
    mreq->goUp(0);
    return;
  }


  PAddr addr = mreq->getPAddr();

  Line *l = cache->writeLine(addr);
  if (l == 0) {
    nextSlot();
    CallbackBase *cb = doReturnAccessCB::create(this, mreq);
    l = allocateLine(mreq->getPAddr(), cb);
    
    if(l != 0) { 
      // the allocation was successfull, no need for the callback
      cb->destroy();
    } else {
      // not possible to allocate a line, will be called back later
      return;
    }
  }
  doReturnAccess(mreq);
}

void Cache::doReturnAccess(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  Line *l = cache->findLineTagNoEffect(addr);
  I(l);

  if(mreq->getMemOperation() != MemRead) {
#ifndef SMP
    l->makeDirty();
#endif
    wbuffRemove(addr);
  }
  
  avgMissLat.sample(globalClock - mreq->getClockStamp());
  mreq->goUp(0);
  
  mshr->retire(addr);
}

Cache::Line *Cache::allocateLine(PAddr addr, CallbackBase *cb)
{
  PAddr rpl_addr=0;
  I(cache->findLineDebug(addr) == 0);
  Line *l = cache->fillLine(addr, rpl_addr);
  lineFill.inc();

  if(l == 0) {
    doAllocateLineRetryCB::scheduleAbs(globalClock + 100, this, addr, cb);
    return 0;
  }

  if(!l->isValid()) {
    l->validate();
    return l;
  }

  // line was valid
  if(isHighestLevel()) { // at the L1, just write back if dirty and use line
    if(l->isDirty()) {
      doWriteBack(rpl_addr);
      l->makeClean();
    }
    l->validate();
    return l;
  }

  // not highest level, must invalidate old line, which may take a while
  l->lock();
  I(pendInvTable.find(rpl_addr) == pendInvTable.end());
  pendInvTable[rpl_addr].outsResps = getNumCachesInUpperLevels();
  pendInvTable[rpl_addr].cb = doAllocateLineCB::create(this, addr, rpl_addr, cb);
  invUpperLevel(rpl_addr, cache->getLineSize(), this);
  return 0;
}

void Cache::doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb)
{
  Line *l = cache->findLineTagNoEffect(addr);
  I(l);

  if(l->isDirty()) {
    doWriteBack(rpl_addr);
    l->makeClean();
  }

  l->validate();

  I(cb);
  cb->call();
}

void Cache::doAllocateLineRetry(PAddr addr, CallbackBase *cb)
{
  Line *l = allocateLine(addr, cb);
  if(l) 
    cb->call();
}

bool Cache::canAcceptStore(PAddr addr)
{
  if(maxPendingWrites > 0 && pendingWrites >= maxPendingWrites) {
    nWBFull.inc();
    return false;
  }

  bool canAcceptReq = mshr->canAcceptRequest(addr);

  rejectedHits.add(!canAcceptReq && isInCache(addr) ? 1: 0);
  rejected.add(!canAcceptReq);

  return canAcceptReq;
}

bool Cache::canAcceptLoad(PAddr addr) 
{
  bool canAcceptReq = mshr->canAcceptRequest(addr);

  rejectedHits.add(!canAcceptReq && isInCache(addr) ? 1: 0);
  rejected.add(!canAcceptReq);

  return canAcceptReq;
}

bool Cache::isInCache(PAddr addr) const
{
  unsigned int index = cache->calcIndex4Addr(addr);
  unsigned long tag = cache->calcTag(addr);

  // check the cache not affecting the LRU state
  for(unsigned int i = 0; i < cache->getAssoc(); i++) {
    Line *l = cache->getPLine(index+i);
    if(l->getTag() == tag)
      return true;
  }
  return false;
}

void Cache::invalidate(PAddr addr, ushort size, MemObj *lowerCache)
{ 
  I(lowerCache);
  I(pendInvTable.find(addr) == pendInvTable.end());
  pendInvTable[addr].outsResps = getNumCachesInUpperLevels(); 
  pendInvTable[addr].cb = doInvalidateCB::create(lowerCache, addr, size);

  if (isHighestLevel()) {     // highest level, we have only to 
    doInvalidate(addr, size); // invalidate the current cache
    return;
  }
    
  invUpperLevel(addr, size, this);
}

void Cache::doInvalidate(PAddr addr, ushort size)
{
  I(pendInvTable.find(addr) != pendInvTable.end());
  CallbackBase *cb = 0;

  PendInvTable::iterator it = pendInvTable.find(addr);
  Entry *record = &(it->second);
  record->outsResps--;

  if(record->outsResps <= 0) {
     cb = record->cb;
     pendInvTable.erase(addr);
  }

  signed long leftSize = size; // use signed because cacheline can be bigger
  while (leftSize > 0) {
    Line *l = cache->readLine(addr);
    
    if(l){
      nextSlot();
      I(l->isValid());
      if(l->isDirty()) {
	doWriteBack(addr);
	l->makeClean();
      }
      l->invalidate();
    } 
    addr     += cache->getLineSize();
    leftSize -= cache->getLineSize();
  }

  // finished sending dirty lines to lower level, 
  // wake up callback
  // should take at least as much as writeback (1)
  if(cb)
    EventScheduler::schedule((TimeDelta_t) 2,cb);
}

Time_t Cache::getNextFreeCycle() const // TODO: change name to calcNextFreeCycle
{
  return cachePort->calcNextSlot();
}

void Cache::doWriteBack(PAddr addr)
{
  I(0);
}

void Cache::sendMiss(MemRequest *mreq)
{
  I(0);
}

void Cache::wbuffAdd(PAddr addr)
{
  ID2(WBuff::const_iterator it =wbuff.find(addr));

  wbuff[addr]++;
  pendingWrites++;
  avgPendingWrites.sample(pendingWrites);

  GI(it == wbuff.end(), wbuff[addr] == 1);
}

void Cache::wbuffRemove(PAddr addr)
{
  WBuff::iterator it = wbuff.find(addr);
  if(it == wbuff.end())
    return;

  pendingWrites--;
  I(pendingWrites >= 0);

  I(it->second > 0);
  it->second--;
  if(it->second == 0)
    wbuff.erase(addr);
}

bool Cache::isInWBuff(PAddr addr)
{
  return (wbuff.find(addr) != wbuff.end());
}

void Cache::dump() const
{
  double total =   readMiss.getDouble()  + readHit.getDouble()
    + writeMiss.getDouble() + writeHit.getDouble() + 1;

  MSG("%s:total=%8.0f:rdMiss=%8.0f:rdMissRate=%5.3f:wrMiss=%8.0f:wrMissRate=%5.3f"
      ,symbolicName
      ,total
      ,readMiss.getDouble()
      ,100*readMiss.getDouble()  / total
      ,writeMiss.getDouble()
      ,100*writeMiss.getDouble() / total
      );
}

//
// WBCache: Write back cache, only writes to lower level on displacements
//
WBCache::WBCache(MemorySystem *gms, const char *descr_section, 
	const char *name) 
  : Cache(gms, descr_section, name) 
{
  // nothing to do
}
WBCache::~WBCache() 
{
  // nothing to do
}

void WBCache::pushLine(MemRequest *mreq)
{
  // here we are pretending there is an outstanding pushLine 
  // buffer so operation finishes immediately

  // Line has to be pushed from a higher level to this level
  I(!isHighestLevel());

  //FIXME: a line should be allocated if the cache is not inclusive
  // since cache is inclusive (want to implement the 
  // non-inclusive one? =), request has to hit
  I(inclusiveCache);

  linePush.inc();

  Line *l = cache->writeLine(mreq->getPAddr());

  // l == 0 if the upper level is sending a push due to a
  // displacement of the lower level
  if (l != 0) {
    l->validate();
    l->makeDirty();
  }
  
  nextSlot();
  mreq->goUp(0);
}

void WBCache::sendMiss(MemRequest *mreq)
{
#ifdef TS_TIMELINE
  if (mreq->getDInst())
    TraceGen::add(mreq->getVersionRef()->getId(),"%sMiss=%lld"
		  ,symbolicName
		  ,globalClock
		  );
//     TraceGen::add(mreq->getVersionRef()->getId(),"%sMiss=%lld:%sPC=0x%x:%sAddr=0x%x"
// 		  ,symbolicName
// 		  ,globalClock
// 		  ,symbolicName
// 		  ,mreq->getDInst()->getInst()->getAddr()
// 		  ,symbolicName
// 		  ,mreq->getPAddr()
// 		  );
#endif

  mreq->mutateWriteToRead();
  mreq->goDown(missDelay, lowerLevel[0]);
}

void WBCache::doWriteBack(PAddr addr) 
{
  writeBack.inc();
  //FIXME: right now we are assuming all lines are the same size
  CBMemRequest::create(1, lowerLevel[0], MemPush, addr, 0); 
}

// WTCache: Write through cache, always propagates writes down

WTCache::WTCache(MemorySystem *gms, const char *descr_section, 
		 const char *name) 
  : Cache(gms, descr_section, name) 
{
  // nothing to do 
}

WTCache::~WTCache()
{
  // nothing to do
}

void WTCache::doWrite(MemRequest *mreq)
{
  Line *l = cache->writeLine(mreq->getPAddr());

  if(l == 0) {
    writeMissHandler(mreq);
    return;
  }

  writeHit.inc();

  //  GLOG(SMPDBG_MSGS, "WTCache::doWrite cache %s addr = 0x%08x, state = %d", 
  //     getSymbolicName(), (uint) mreq->getPAddr(), l->isInvalid());

  writePropagateHandler(mreq); // this is not a proper miss, but a WT cache 
                               // always propagates writes down
}

void WTCache::writePropagateHandler(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  wbuffAdd(addr);
  if(!mshr->issue(addr))
    mshr->addEntry(addr, reexecuteDoWriteCB::create(this, mreq),
		   /*doWriteQueuedCB::create(this, mreq)*/
		   reexecuteDoWriteCB::create(this, mreq));
  else
    propagateDown(mreq);

  //GLOG(SMPDBG_MSGS, "WTCache::writePropagateHandler cache %s addr = 0x%08x", 
  //     getSymbolicName(), (uint) mreq->getPAddr());
}

void WTCache::reexecuteDoWrite(MemRequest *mreq)
{
  Line *l = cache->findLine(mreq->getPAddr());

  if(l == 0)
    sendMiss(mreq);
  else
    propagateDown(mreq);

  //GLOG(SMPDBG_MSGS, "WTCache::reexecuteDoWrite cache %s addr = 0x%08x", 
  //     getSymbolicName(), (uint) mreq->getPAddr());
}

void WTCache::propagateDown(MemRequest *mreq)
{
  mreq->goDown(missDelay, lowerLevel[0]);
}

void WTCache::sendMiss(MemRequest *mreq)
{
  Line *l = cache->readLine(mreq->getPAddr());
  if (!l)
    mreq->mutateWriteToRead();
  mreq->goDown(missDelay, lowerLevel[0]);

  //GLOG(SMPDBG_MSGS, "WTCache::sendMiss cache %s addr = 0x%08x", 
  //     getSymbolicName(), (uint) mreq->getPAddr());
}

void WTCache::doWriteBack(PAddr addr) 
{
  I(0);
  // nothing to do
}

void WTCache::pushLine(MemRequest *mreq)
{
  I(0); // should never be called
}

void WTCache::inclusionCheck(PAddr addr) {
  const LevelType* la  = getLowerLevel();
  MemObj*    c  = (*la)[0];
  const LevelType* lb = c->getLowerLevel();
  MemObj*    cc = (*lb)[0];
  I(((SMPCache*)cc)->findLine(addr));
}

// NICECache is a cache that always hits

NICECache::NICECache(MemorySystem *gms, const char *section, const char *name)
  : Cache(gms, section, name) 
{
}

NICECache::~NICECache()
{
  // Nothing to do
}

void NICECache::read(MemRequest *mreq)
{
  readHit.inc(); 
  mreq->goUp(hitDelay);
}

void NICECache::write(MemRequest *mreq)
{
  writeHit.inc();
  mreq->goUp(hitDelay);
}

void NICECache::returnAccess(MemRequest *mreq)
{
  I(0);
}

void NICECache::sendMiss(MemRequest *mreq)
{
  I(0);
}

void NICECache::doWriteBack(PAddr addr)
{
  I(0);
}

void NICECache::specialOp(MemRequest *mreq)
{
  mreq->goUp(1);
}

void NICECache::pushLine(MemRequest *mreq)
{
  linePush.inc();
  mreq->goUp(0);
}
