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

#include "SMPCache.h"
#include "MESICacheState.h"
#include "Cache.h"

// This cache works under the assumption that caches above it in the memory
// hierarchy are write-through caches

// Otherwise, a write in an upper level would be mutated into a readW and 
// this readW would be treated as a read

MSHR<PAddr> *SMPCache::mutExclBuffer = NULL;

SMPCache::SMPCache(SMemorySystem *dms, const char *section, const char *name)
  : MemObj(section, name)
  , readHit("%s:readHit", name)
  , writeHit("%s:writeHit", name)
  , readMiss("%s:readMiss", name)
  , writeMiss("%s:writeMiss", name)  
  , lineFill("%s:lineFill", name) 
{
  MemObj *lowerLevel = NULL;

  I(dms);
  lowerLevel = dms->declareMemoryObj(section, "lowerLevel");

  if (lowerLevel != NULL)
    addLowerLevel(lowerLevel);

  cache = CacheType::create(section, "", name);
  I(cache);

  protocol = new MESIProtocol(this);

  SescConf->isLong(section, "numPorts");
  SescConf->isLong(section, "portOccp");

  cachePort = PortGeneric::create(name, 
				  SescConf->getLong(section, "numPorts"), 
				  SescConf->getLong(section, "portOccp"));

  char *outsReqName = (char *) malloc(strlen(name) + 2);

  sprintf(outsReqName, "%s", name);

  outsReq = MSHR<PAddr>::create(outsReqName, 
			     SescConf->getCharPtr(section, "MSHRtype"),
			     SescConf->getLong(section, "MSHRsize"),
			     SescConf->getLong(section, "bsize"));
  if (mutExclBuffer == NULL)
    mutExclBuffer = MSHR<PAddr>::create("mutExclBuffer", 
			     SescConf->getCharPtr(section, "MSHRtype"),
					32000,
			     SescConf->getLong(section, "bsize"));

  SescConf->isLong(section, "hitDelay");
  hitDelay = SescConf->getLong(section, "hitDelay");

  SescConf->isLong(section, "missDelay");
  missDelay = SescConf->getLong(section, "missDelay");

}

SMPCache::~SMPCache() 
{
  // do nothing
}

Time_t SMPCache::getNextFreeCycle()
{
  return cachePort->calcNextSlot();
}

bool SMPCache::canAcceptStore(PAddr addr) const
{
  return outsReq->canAcceptRequest(addr);
}

void SMPCache::access(MemRequest *mreq)
{
  PAddr addr;
  IS(addr = mreq->getPAddr());
  
  GMSG(mreq->getPAddr() < 1024,
       "mreq dinst=0x%p paddr=0x%x vaddr=0x%x memOp=%d ignored",
       mreq->getDInst(),
       (unsigned int) mreq->getPAddr(),
       (unsigned int) mreq->getVaddr(),
       mreq->getMemOperation());
  
  I(addr > 1024); 

  GLOG(SMPDBG_MSGS, "SMPCache::access cache %s for addr 0x%x, type = %d",
       getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  switch(mreq->getMemOperation()){
  case MemRead:  read(mreq);          break; 
  case MemWrite: /*I(cache->findLine(mreq->getPAddr())); will be transformed
		   to MemReadW later */
  case MemReadW: write(mreq);         break; 
  case MemPush:  pushline(mreq);      break;
  default:       specialOp(mreq);     break;
  }
  // MemRead  means "I want to read"
  // MemReadW means "I want to write, and I missed"
  // MemWrite means "I want to write, and I hit"
  // MemPush  means "I am writing data back"
}

void SMPCache::read(MemRequest *mreq)
{  
  PAddr addr = mreq->getPAddr();

  if (!outsReq->issue(addr)) {
    outsReq->addEntry(addr, doReadCB::create(this, mreq), 
		      doReadCB::create(this, mreq));
    return;
  }

  doReadCB::scheduleAbs(nextSlot(), this, mreq);
}

void SMPCache::doRead(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  Line *l = cache->readLine(addr);

  if (l && l->canBeRead()) {

    GLOG(SMPDBG_MSGS, "SMPCache::doRead cache %s (h) for addr 0x%x, type = %d, state = 0x%08x", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());

    readHit.inc();
    outsReq->retire(addr);
    mreq->goUp(hitDelay);
    return;
  }
    
  if (l && (l->getState() == MESI_TRANS || l->getState() == MESI_TRANS_DISP)) {
    GLOG(SMPDBG_MSGS, "SMPCache::doRead cache %s (t) for addr 0x%x, type = %d, state = 0x%08x", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());
    doReadCB::scheduleAbs(nextSlot(), this, mreq);
    return;
  }

  GI(l, !l->isTransient());

  GLOG(SMPDBG_MSGS, "SMPCache::doRead cache %s (m) for addr 0x%x, type = %d", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  readMiss.inc(); 
  if (!mutExclBuffer->issue(addr)) {
    mutExclBuffer->addEntry(addr, sendReadCB::create(this, mreq),
			    sendReadCB::create(this, mreq));
    return;
  }
  sendRead(mreq);
}

void SMPCache::sendRead(MemRequest* mreq)
{
  protocol->read(mreq);
}

void SMPCache::write(MemRequest *mreq)
{  
  PAddr addr = mreq->getPAddr();
  
  if (!outsReq->issue(addr)) {
    outsReq->addEntry(addr, doWriteCB::create(this, mreq), 
		      doWriteCB::create(this, mreq));
    return;
  }
  
  doWriteCB::scheduleAbs(nextSlot(), this, mreq);
}

void SMPCache::doWrite(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  Line *l = cache->writeLine(addr);

  if (l && l->canBeWritten()) {

    GLOG(SMPDBG_MSGS, "SMPCache::doWrite cache %s (h) for addr 0x%x, type = %d, state = 0x%08x", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());

    writeHit.inc();
    outsReq->retire(addr);
    mreq->goUp(hitDelay);  
    return;
  }

  if (l && (l->getState() == MESI_TRANS || l->getState() == MESI_TRANS_DISP)) {
    GLOG(SMPDBG_MSGS, "SMPCache::doWrite cache %s (t) for addr 0x%x, type = %d, state = 0x%08x", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());    
    mreq->mutateWriteToRead();
    doWriteCB::scheduleAbs(nextSlot(), this, mreq);
    return;
  }

  GI(l, !l->isTransient());

  if (mreq->getMemOperation() == MemWrite && !l)
    mreq->mutateWriteToRead();

  if(l)
    GLOG(SMPDBG_MSGS, "SMPCache::doWrite cache %s (m) for addr 0x%x, type = %d, state = 0x%08x", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());
  else
    GLOG(SMPDBG_MSGS, "SMPCache::doWrite cache %s (m) for addr 0x%x, type = %d", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  writeMiss.inc();
  if (!mutExclBuffer->issue(addr)) {
    mutExclBuffer->addEntry(addr, sendWriteCB::create(this, mreq),
			    sendWriteCB::create(this, mreq));
    return;
  }
  sendWrite(mreq);
}

void SMPCache::sendWrite(MemRequest* mreq)
{
  protocol->write(mreq);
}

void SMPCache::pushline(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if (!outsReq->issue(addr)) {
    outsReq->addEntry(addr, doPushLineCB::create(this, mreq),
		      doPushLineCB::create(this, mreq));
    return;
  }
  
  doPushLineCB::scheduleAbs(nextSlot(), this, mreq); 
}

void SMPCache::doPushLine(MemRequest *mreq)
{
  I(!isHighestLevel());

  // assuming inclusive cache
  // should always find line
  Line *l = cache->writeLine(mreq->getPAddr());
  I(l);
  I(!l->isTransient());

  GLOG(SMPDBG_MSGS, "SMPCache::doPushLine cache %s for addr 0x%x, type = %d, state = 0x%08x", 
       getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());

  l->changeStateTo(MESI_MODIFIED);
  I(0); //untested never used code
  outsReq->retire(mreq->getPAddr());
  mreq->goUp(0);
}

void SMPCache::doWriteBack(PAddr addr) const
{
  CBMemRequest::create(1, lowerLevel[0], MemPush, addr, 0);
}

void SMPCache::specialOp(MemRequest *mreq)
{
  mreq->goUp(1); // TODO: implement atomic ops?
}

void SMPCache::invalidate(PAddr addr, ushort size, MemObj *oc)
{
  I(oc);
  I(pendInvTable.find(addr) == pendInvTable.end());
  pendInvTable[addr].outsResps = getUpperLevelSize();
  pendInvTable[addr].cb = doInvalidateCB::create(oc, addr, size);
  pendInvTable[addr].invalidate = true;

  GLOG(SMPDBG_MSGS, "SMPCache::invalidate cache %s for addr 0x%x", 
       getSymbolicName(), (uint) addr);

  if (!isHighestLevel()) {
    invUpperLevel(addr, size, this);
    return;
  }

  doInvalidate(addr, size);
}

void SMPCache::doInvalidate(PAddr addr, ushort size)
{
  I(pendInvTable.find(addr) != pendInvTable.end());
  CallbackBase *cb = 0;
  bool invalidate = false;

  PendInvTable::iterator it = pendInvTable.find(addr);
  Entry *record = &(it->second);
  if(--(record->outsResps) <= 0) {
    cb = record->cb;
    invalidate = record->invalidate;
    pendInvTable.erase(addr);
  }

  GLOG(SMPDBG_MSGS, "SMPCache::doInvalidate cache %s for addr 0x%x", 
       getSymbolicName(), (uint) addr);

  if(invalidate)
    realInvalidate(addr, size);

  if(cb)
    EventScheduler::schedule((TimeDelta_t) 2,cb);
}

void SMPCache::realInvalidate(PAddr addr, ushort size)
{
  GLOG(SMPDBG_MSGS, "SMPCache::realInvalidate cache %s for addr 0x%x", 
       getSymbolicName(), (uint) addr);

  while(size) {

    Line *l = cache->findLine(addr);
    
    if (l) {
      nextSlot(); // counts for occupancy to invalidate line
      I(l->isValid());
      if (l->isDirty())
	doWriteBack(addr);
      l->invalidate();
    }
    addr += cache->getLineSize();
    size -= cache->getLineSize();
  }
}

// interface with protocol
void SMPCache::sendBelow(SMPMemRequest *sreq)
{
  GLOG(SMPDBG_MSGS, "SMPCache::sendBelow cache %s req down for addr 0x%x, type = %d",
       getSymbolicName(), (uint) sreq->getPAddr(), (uint) sreq->getMemOperation());

  sreq->goDown(missDelay, lowerLevel[0]);
}

void SMPCache::respondBelow(SMPMemRequest *sreq)
{
  PAddr addr = sreq->getPAddr();

  GLOG(SMPDBG_MSGS, "SMPCache::respondBelow cache %s req down for addr 0x%x, type = %d",
       getSymbolicName(), (uint) sreq->getPAddr(), (uint) sreq->getMemOperation());

  lowerLevel[0]->access(sreq);
}

void SMPCache::receiveFromBelow(SMPMemRequest *sreq)
{
  MemOperation memOp = sreq->getMemOperation();

  GLOG(SMPDBG_MSGS, "SMPCache::receiveFromBelow cache %s req up for addr 0x%x, type = %d",
       getSymbolicName(), (uint) sreq->getPAddr(), (uint) sreq->getMemOperation());

  // request from other caches, need to process them accordingly

  if(memOp == MemRead) {
    protocol->readMissHandler(sreq);
    return;
  }

  if(memOp == MemReadW) {
    if(sreq->needsData()) 
      protocol->writeMissHandler(sreq);
    else
      protocol->invalidateHandler(sreq);
    return;
  }

  if(memOp == MemWrite) {
    I(!sreq->needsData());
    protocol->invalidateHandler(sreq);
    return;
  }
    
  I(0); // this routine should not be called for any other memory request
        // TODO: change this to implement synch operations
}

void SMPCache::returnAccess(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
  MemOperation memOp = sreq->getMemOperation();

  if(sreq->getRequestor() == this) {
    // request from this cache (response is ready)
    GLOG(SMPDBG_MSGS, "SMPCache::returnAccess cache %s for address 0x%x, type = %d", 
	 getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
    mutExclBuffer->retire(addr);

    if(memOp == MemRead) {
      protocol->readMissAckHandler(sreq);
    } 
    else if(memOp == MemReadW) {
      if(sreq->needsData()) 
	protocol->writeMissAckHandler(sreq);
      else
	protocol->invalidateAckHandler(sreq);
    } 
    else if(memOp == MemWrite) {
      I(!sreq->needsData());
      protocol->invalidateAckHandler(sreq);
    }
  } 
  else {
  receiveFromBelow(sreq);
  }
}

void SMPCache::concludeAccess(MemRequest *mreq)
{
  GLOG(SMPDBG_MSGS, "SMPCache::concludeAccess cache %s req down for addr 0x%x, type = %d",
       getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  outsReq->retire(mreq->getPAddr());
  mreq->mutateReadToWrite(); /*
			      Hack justification: It makes things much
			      nicer if we can call mutateWriteToRead()
			      in write() if the line is displaced from
			      the cache between the time the write is
			      processed in the L1 to the time it is
			      processed in the L2.

			      BUT from the L2, we don't call
			      mreq->goDown(), so we can't rely on
			      mreq->goUp() to restore the memOp.
			    */
  mreq->goUp(0);
}

SMPCache::Line *SMPCache::allocateLine(PAddr addr, CallbackBase *cb)
{
  PAddr rpl_addr = 0;
  I(cache->findLineDebug(addr) == 0);
  Line *l = cache->findLine2Replace(addr);
  I(l);
  rpl_addr = cache->calcAddr4Tag(l->getTag());
    //cache->fillLine(addr, rpl_addr);
  lineFill.inc();

  nextSlot(); // have to do an access to check which line is free

  if(l)
    GLOG(SMPDBG_MSGS, "SMPCache::allocateLine cache %s req down for addr 0x%x, state = 0x%08x",
	 getSymbolicName(), (uint) addr, (uint) l->getState());
  else
    GLOG(SMPDBG_MSGS, "SMPCache::allocateLine cache %s req down for addr 0x%x, no line found",
	 getSymbolicName(), (uint) addr);

  if(!l->isValid()) {
    cb->destroy();
    cache->fillLine(addr);
    return l;
  }
  
  if(isHighestLevel()) {
    if(l->isDirty())
      doWriteBack(rpl_addr);

    cb->destroy();
    cache->fillLine(addr);
    return l;
  }

  I(pendInvTable.find(rpl_addr) == pendInvTable.end());
  pendInvTable[rpl_addr].outsResps = getUpperCacheLevelSize();
  pendInvTable[rpl_addr].cb = doAllocateLineCB::create(this, addr, rpl_addr, cb);
  pendInvTable[rpl_addr].invalidate = false;

  l->changeStateTo(MESI_TRANS_DISP);
  invUpperLevel(rpl_addr, cache->getLineSize(), this);

  return 0;
}

void SMPCache::doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb)
{
  // FIXME: this fill line is being done for the second time
  Line *l = cache->findLine(rpl_addr); 
  I(l && l->isLocked());

  GLOG(SMPDBG_MSGS, "SMPCache::doAllocateLine cache %s req down for addr 0x%x, state = 0x%08x",
       getSymbolicName(), (uint) addr, (uint) l->getState());

  if(l->isDirty())
    doWriteBack(rpl_addr);

  I(cb);
  l->setTag(cache->calcTag(addr));
  cb->call();
}

SMPCache::Line *SMPCache::getLine(PAddr addr)
{
  nextSlot(); 
  //return cache->readLine(addr); // TODO: this should only be an access to tag
  return cache->findLine(addr);
}

void SMPCache::updateLine(PAddr addr, MESIState_t newState)
{
  nextSlot();
  //cache->writeLine(addr);  // TODO: this should only be an access to tag
  Line *l = cache->findLine(addr);
  I(l);
  l->changeStateTo(newState);
}

void SMPCache::writeLine(PAddr addr) {
  Line *l = cache->writeLine(addr);
  I(l);
}

void SMPCache::invalidateLine(PAddr addr, CallbackBase *cb) 
{
  Line *l = cache->findLine(addr);
  
  I(l);
  GLOG(SMPDBG_MSGS, "SMPCache::invalidateLine cache %s for addr 0x%x, state = 0x%08x",
       getSymbolicName(), (uint) addr, (uint) l->getState());

  I(pendInvTable.find(addr) == pendInvTable.end());
  pendInvTable[addr].outsResps = getUpperCacheLevelSize();
  pendInvTable[addr].cb = cb;
  pendInvTable[addr].invalidate = true;

  l->changeStateTo(MESI_TRANS);
  invUpperLevel(addr, cache->getLineSize(), this);
}

void SMPCache::inclusionCheck(PAddr addr) {
  const LevelType* la  = getUpperLevel();
  MemObj*    c  = (*la)[0];
  const LevelType* lb = c->getUpperLevel();
  MemObj*    cc = (*lb)[0];
  I(! ((Cache*)cc)->isInCache(addr));
}

bool SMPCache::canAcceptStore(PAddr addr) 
{
  return outsReq->canAcceptRequest(addr);
}
