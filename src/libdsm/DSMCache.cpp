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

#include "DSMCache.h"
#include "DSMCacheState.h"

DSMCache::DSMCache(DMemorySystem *dms, const char *section, const char *name)
  : MemObj(section, name)
{
  MemObj *lowerLevel = NULL;

  I(dms);
  lowerLevel = dms->declareMemoryObj(section, "lowerLevel");

  if (lowerLevel != NULL)
    addLowerLevel(lowerLevel);

  numNetworks = 0;

  cache = CacheType::create(section, "", name);
  I(cache);

  SescConf->isLong(section, "numPorts");
  SescConf->isLong(section, "portOccp");

  cachePort = PortGeneric::create(name, 
				  SescConf->getLong(section, "numPorts"), 
				  SescConf->getLong(section, "portOccp"));

  char *mshrINName = (char *) malloc(strlen(name) + 2);
  char *mshrOUTName = (char *) malloc(strlen(name) + 3);

  sprintf(mshrINName, "%sIN", name);
  sprintf(mshrOUTName, "%sOUT", name);

  // TODO: have 2 sections for different MSHRs
  mshrIN  = MSHR<PAddr>::create(mshrINName, 
				SescConf->getCharPtr(section, "MSHRtype"),
				SescConf->getLong(section, "MSHRsize"),
				SescConf->getLong(section, "bsize"));

  mshrOUT = MSHR<PAddr>::create(mshrOUTName, 
				SescConf->getCharPtr(section, "MSHRtype"),
				SescConf->getLong(section, "MSHRsize"),
				SescConf->getLong(section, "bsize"));

  SescConf->isLong(section, "hitDelay");
  hitDelay = SescConf->getLong(section, "hitDelay");

  SescConf->isLong(section, "missDelay");
  missDelay = SescConf->getLong(section, "missDelay");
}

DSMCache::~DSMCache() 
{
  // do nothing
}

void DSMCache::attachNetwork(Protocol_t protocol, 
			     InterConnection *net, 
			     RouterID_t rID, 
			     PortID_t pID)
{
  DSMProtocol *prot = NULL;

  if (protocol == DSMControl)
    prot = new DSMControlProtocol(this, net, rID, pID);
  else if (protocol == DSMData)
    prot = new DSMDataProtocol(this, net, rID, pID);
  else I(0); // wrong configuration should have been filtered previously

  protocolHolder.push_back(prot);
  numNetworks++;
}

void DSMCache::detachNetworks()
{
  for(ProtocolHolderList::iterator it = protocolHolder.begin(); 
      it != protocolHolder.end(); 
      it++) 
    delete *it;
  
  protocolHolder.clear();
  numNetworks = 0;
}

Time_t DSMCache::getNextFreeCycle() const
{
  return cachePort->calcNextSlot();
}

bool DSMCache::canAcceptStore(PAddr addr)
{
  return mshrIN->canAcceptRequest(addr);
}

void DSMCache::access(MemRequest *mreq)
{
  GMSG(mreq->getPAddr() < 1024,
       "mreq dinst=0x%p paddr=0x%x vaddr=0x%x memOp=%d",
       mreq->getDInst(),
       (unsigned int) mreq->getPAddr(),
       (unsigned int) mreq->getVaddr(),
       mreq->getMemOperation());
  
  I(mreq->getPAddr() > 1024); // TODO: ask Luis what this is

  switch(mreq->getMemOperation()){
  case MemReadW:
  case MemRead:    read(mreq);       break;
  case MemWrite:   write(mreq);      break;
  default:         specialOp(mreq);  break;
  }

}

void DSMCache::read(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if (!mshrIN->issue(addr)) {
    mshrIN->addEntry(addr, doReadCB::create(this, mreq));
    return;
  }
  
  doReadCB::scheduleAbs(globalClock + hitDelay, this, mreq);
  // TODO: make sure both this call and mshr callback happen after a hit delay
}

void DSMCache::doRead(MemRequest *mreq)
{
  protocolHolder[0]->read(mreq); // TODO: check timing
}

void DSMCache::write(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if (!mshrIN->issue(addr)) {
    mshrIN->addEntry(addr, doWriteCB::create(this, mreq));
    return;
  }
  
  doWriteCB::scheduleAbs(globalClock + hitDelay, this, mreq);
  // TODO: make sure both this call and mshr callback happen after a hit delay
}

void DSMCache::doWrite(MemRequest *mreq)
{
  protocolHolder[0]->write(mreq); // TODO: check timing
}
 
void DSMCache::specialOp(MemRequest *mreq)
{
  mreq->goUp(1); // TODO: implement atomic ops?
}

void DSMCache::invalidate(PAddr addr, ushort size, MemObj *oc)
{
  invUpperLevel(addr, size, oc);
  while (size) {

    nextSlot(); // counts for occupancy to invalidate line

    Line *l = cache->findLine(addr);

    if (l) {
      I(!l->isInvalid());
      if (l->isDirty())
	doWriteBack(addr);
      l->invalidate();
    }
    addr += cache->getLineSize();
    size -= cache->getLineSize();
  }
}

void DSMCache::doInvalidate(PAddr addr, ushort size)
{
  I(0);
}

void DSMCache::returnAccess(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  GLOG(DSMDBG_MSGS, "DSMCache::returnAccess for address 0x%x", 
       (uint) mreq->getPAddr());

  protocolHolder[0]->returnAccessBelow(mreq);
}

void DSMCache::doWriteBack(PAddr addr) const
{
  //I(getLineSize() == (lowerLevel[0])->getLineSize());
  CBMemRequest::create(1, lowerLevel[0], MemPush, addr, 0);
  // TODO: assumed this cache and lowerLevel have same line size, fix it
}

// interface with protocol
void DSMCache::readBelow(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  GLOG(DSMDBG_MSGS, "DSMCache::readBelow pushing req down for addr 0x%x",
       (uint) addr);

  mreq->goDown(missDelay, lowerLevel[0]);
  // TODO: make sure missDelay is the time to generate a request to lower level
}

void DSMCache::writeBelow(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  GLOG(DSMDBG_MSGS, "DSMCache::writeBelow pushing req down for addr 0x%x",
       (uint) addr);

  mreq->goDown(missDelay, lowerLevel[0]);
  // TODO: make sure missDelay is the time to generate a request to lower level
}

void DSMCache::concludeAccess(MemRequest *mreq)
{
  mshrIN->retire(mreq->getPAddr());
  mreq->goUp(0); // TODO: timing
}

DSMCache::Line *DSMCache::allocateLine(PAddr addr)
{
  PAddr rpl_addr = 0;
  I(cache->findLine(addr) == 0);
  Line *l = cache->fillLine(addr, rpl_addr);

  nextSlot(); // have to do an access to check which line is free

  if(l->isInvalid())
    return l;
  
  invUpperLevel(rpl_addr, cache->getLineSize(), this);

  if(l->isDirty())
    doWriteBack(rpl_addr);
  
  l->invalidate();

  return l;
}

DSMCache::Line *DSMCache::getLine(PAddr addr)
{
  nextSlot(); 
  //return cache->readLine(addr); // TODO: this should only be an access to tag
  return cache->findLine(addr);
}

void DSMCache::updateLine(PAddr addr)
{
  nextSlot();
  cache->writeLine(addr);  // TODO: this should only be an access to tag
}

void DSMCache::invalidateLine(PAddr addr)
{
  Line *l = cache->findLine(addr);

  I(!l->isLocked());

  nextSlot();
  l->invalidate();
  cache->writeLine(addr);  // TODO: this should only be an access to tag
  invUpperLevel(addr, cache->getLineSize(), this);
}

