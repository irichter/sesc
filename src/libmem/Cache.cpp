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
#ifdef TS_TIMELINE
#include "TraceGen.h"
#include "HVersion.h"
#endif

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
{
  MemObj *lower_level = NULL;
  char busName[512];
  
  cache = CacheType::create(section, "", name);
  
  mshr = MSHR<PAddr>::create(name, 
			     SescConf->getCharPtr(section, "MSHRtype"),
			     SescConf->getLong(section, "MSHRsize"),
			     SescConf->getLong(section, "bsize"));
  
  I(gms);
  lower_level = gms->declareMemoryObj(section, k_lowerLevel);

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

  defaultMask  = ~(cache->getLineSize()-1);

}

Cache::~Cache()
{
  // nothing to do
}

void Cache::access(MemRequest *mreq) 
{
  GMSG(mreq->getPAddr() <= 1024
       ,"mreq dinst=%p paddr=0x%x vaddr=0x%x memOp=%d ignored"
       ,mreq->getDInst()
       ,(uint) mreq->getPAddr()
       ,(uint) mreq->getVaddr()
       ,mreq->getMemOperation()
       );
       
  I(mreq->getPAddr() > 1024);

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
  if(isInWBuff(mreq->getPAddr())) {
    nForwarded.inc();
    mreq->goUp(hitDelay);
    return;
  }

  doReadCB::scheduleAbs(nextSlot(), this, mreq);
}

void Cache::doRead(MemRequest *mreq)
{
  Line *l = cache->readLine(mreq->getPAddr());

  if (l == 0) {
    readMissHandler(mreq);
    return;
  }

  readHit.inc();

  mreq->goUp(hitDelay);
}

void Cache::readMissHandler(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if(!mshr->issue(addr)) {
    mshr->addEntry(addr, doReadQueuedCB::create(this, mreq), 
		   activateOverflowCB::create(this, mreq));
    return;
  }

  readMiss.inc();
  sendMiss(mreq);
}

void Cache::doReadQueued(MemRequest *mreq)
{
  readHalfMiss.inc();
  mreq->goUp(hitDelay);

  // the request came from the MSHR, we need to retire it
  mshr->retire(mreq->getPAddr());
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
    mreq->goUp(hitDelay);
    
    // the request came from the MSHR overflow, we need to retire it
    mshr->retire(mreq->getPAddr());
    
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
  l->dirty = true;

  mreq->goUp(hitDelay);
  
  // the request came from the MSHR overflow, we need to retire it
  wbuffRemove(mreq->getPAddr());
  mshr->retire(mreq->getPAddr());
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

  l->dirty = true;

  mreq->goUp(hitDelay);
}

void Cache::writeMissHandler(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  wbuffAdd(addr);
  if(!mshr->issue(addr)) {
    mshr->addEntry(addr, doWriteQueuedCB::create(this, mreq),
		   activateOverflowCB::create(this, mreq));
    return;
  }

  writeMiss.inc();
  sendMiss(mreq);
}

void Cache::doWriteQueued(MemRequest *mreq)
{
  writeHalfMiss.inc();
  mreq->goUp(hitDelay);

  wbuffRemove(mreq->getPAddr());

  // the request came from the MSHR, we need to retire it
  mshr->retire(mreq->getPAddr());
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
    l = allocateLine(mreq->getPAddr(), doReturnAccessCB::create(this, mreq));
    
    if(l == 0) { 
      // not possible to allocate a line, will be called back later
      return;
    }
  }

  l->valid = true;
      
  if(mreq->getMemOperation() == MemRead)
    l->dirty = false;
  else {
    l->dirty = true;
    wbuffRemove(addr);
  }
  
  mreq->goUp(0);
  
  mshr->retire(addr);
}

void Cache::doReturnAccess(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  Line *l = cache->findLine(addr); // TODO: count for energy? it shouldn't
  I(l);

  l->valid = true;
      
  if(mreq->getMemOperation() == MemRead)
    l->dirty = false;
  else {
    l->dirty = true;
    wbuffRemove(addr);
  }
  
  mreq->goUp(0);
  
  mshr->retire(addr);   
}

Cache::Line *Cache::allocateLine(PAddr addr, CallbackBase *cb)
{
  PAddr rpl_addr=0;
  I(cache->findLineDebug(addr) == 0);
  Line *l = cache->fillLine(addr, rpl_addr);
  lineFill.inc();

  if(!l->valid) {
    cb->destroy();
    return l;
  }

  if(isHighestLevel()) {
    if(l->dirty)
      doWriteBack(rpl_addr);
    cb->destroy();
    return l;
  }

  I(pendInvTable.find(rpl_addr) == pendInvTable.end());
  pendInvTable[rpl_addr].outsResps = getUpperCacheLevelSize();
  pendInvTable[rpl_addr].cb = doAllocateLineCB::create(this, addr, rpl_addr, cb);
  
  l->valid = false;
  l->locked = true;
  invUpperLevel(rpl_addr, cache->getLineSize(), this);
  
  return 0;
}

void Cache::doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb)
{
  Line *l = cache->findLine(addr); // TODO: count for energy? it shouldn't
  I(l);

  if(l->dirty)
    doWriteBack(rpl_addr);
  
  l->locked = false;
  
  I(cb);
  cb->call();
}

bool Cache::canAcceptStore(PAddr addr) const 
{
  if(isInCache(addr))
    return true;

  return mshr->canIssue();
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

void Cache::invalidate(PAddr addr, ushort size, MemObj *oc)
{ 
  I(oc);
  I(pendInvTable.find(addr) == pendInvTable.end());
  pendInvTable[addr].outsResps = getUpperCacheLevelSize(); 
  pendInvTable[addr].cb = doInvalidateCB::create(oc, addr, size);
  
  if (!isHighestLevel()) {
    invUpperLevel(addr, size, this);
    return;
  }

  doInvalidate(addr, size);
}

void Cache::doInvalidate(PAddr addr, ushort size)
{
  I(pendInvTable.find(addr) != pendInvTable.end());
  CallbackBase *cb = 0;

  PendInvTable::iterator it = pendInvTable.find(addr);
  Entry *record = &(it->second);
  if(--(record->outsResps) <= 0) {
     cb = record->cb;
     pendInvTable.erase(addr);
  }

  while (size) {
    Line *l = cache->readLine(addr);
    
    if(l){
      nextSlot();
      I(l->valid);
      if(l->dirty) 
	doWriteBack(addr);
      l->valid = false;
      l->invalidate();
    } 
    addr += cache->getLineSize();
    size -= cache->getLineSize();
  }

  // finished sending dirty lines to lower level, 
  // wake up callback
  // should take at least as much as writeback (1)
  if(cb)
    EventScheduler::schedule(2,cb);
}

Time_t Cache::getNextFreeCycle() // TODO: change name to calcNextFreeCycle
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

  GI(it == wbuff.end(), wbuff[addr] == 1);
}

void Cache::wbuffRemove(PAddr addr)
{
  WBuff::iterator it = wbuff.find(addr);
  if(it == wbuff.end())
    return;

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
    l->valid = true;
    l->dirty = true;
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
    writeMiss.inc();
    writeMissHandler(mreq);
    return;
  }

  writeHit.inc();

  l->dirty = true;

  writePropagateHandler(mreq); // this is not a proper miss, but a WT cache 
                               // always propagates writes down
}

void WTCache::writePropagateHandler(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  wbuffAdd(addr);
  if(!mshr->issue(addr)) {
    mshr->addEntry(addr, doWriteQueuedCB::create(this, mreq),
		   propagateDownCB::create(this, mreq));
    return;
  }

  propagateDown(mreq);
}

void WTCache::propagateDown(MemRequest *mreq)
{
  mreq->goDown(missDelay, lowerLevel[0]);
}

void WTCache::sendMiss(MemRequest *mreq)
{
  mreq->mutateWriteToRead();
  mreq->goDown(missDelay, lowerLevel[0]);
}

void WTCache::doWriteBack(PAddr addr) 
{
  // nothing to do
}

void WTCache::pushLine(MemRequest *mreq)
{
  I(0); // should never be called
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
