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

Time_t DSMCache::getNextFreeCycle()
{
  return cachePort->calcNextSlot();
}

bool DSMCache::canAcceptStore(PAddr addr) const
{
  return mshrIN->canIssue(); // TODO: do something similar with mshrOUT
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
  // TODO: put in mshr
  protocolHolder[0]->read(mreq); // TODO: check timing
}

void DSMCache::write(MemRequest *mreq)
{
  // TODO: put in mshr
  protocolHolder[0]->write(mreq); // TODO: check timing
}
 
void DSMCache::specialOp(MemRequest *mreq)
{
  mreq->goUp(1); // TODO: implement atomic ops?
}

void DSMCache::invalidate(PAddr addr, ushort size, CallbackBase *cb)
{
  // TODO: check this code
  invUpperLevel(addr, size, cb);

  nextSlot();

  while (size) {
    Line *l = cache->findLine(addr);

    if (l) {
      I(l->isValid());
      if (l->isDirty())
	doWriteBack(addr);
      l->invalidate();
    }
    addr += cache->getLineSize();
    size -= cache->getLineSize();
  }
}

void DSMCache::returnAccess(MemRequest *mreq)
{
  GLOG(DSMDBG_MSGS, "DSMCache::returnAccess for address 0x%x", 
       (uint) mreq->getPAddr());

  // TODO: retire mshrOUT

  /*
   if (mreq->getMemOperation() == MemRead)
     protocolHolder[0]->sendMemReadAck(mreq, globalClock);
   else if (mreq->getMemOperation() == MemWrite)
     protocolHolder[0]->sendMemWriteAck(mreq, globalClock);
   else I(0); // request should be one of types above  
  */

  protocolHolder[0]->returnAccessBelow(mreq);
}

void DSMCache::doWriteBack(PAddr addr) const
{
  // TODO: implement this
}

// interface with protocol
void DSMCache::readBelow(MemRequest *mreq)
{
  PAddr paddr = mreq->getPAddr();

  // TODO: put mshr here

  doReadBelowCB::scheduleAbs(nextSlot(), this, mreq, false);
}

void DSMCache::doReadBelow(MemRequest *mreq, bool queued)
{
  GLOG(DSMDBG_MSGS, "DSMCache::doReadBelow pushing request down for address 0x%x",
       (uint) mreq->getPAddr());

  mreq->goDown(0, lowerLevel[0]);
}

void DSMCache::writeBelow(MemRequest *mreq)
{
  PAddr paddr = mreq->getPAddr();

  // TODO: put mshr here

  doWriteBelowCB::scheduleAbs(nextSlot(), this, mreq, false);
}

void DSMCache::doWriteBelow(MemRequest *mreq, bool queued)
{
  GLOG(DSMDBG_MSGS, "DSMCache::doWriteBelow pushing request down for address 0x%x",
       (uint) mreq->getPAddr());

  mreq->goDown(0, lowerLevel[0]);
}

void DSMCache::concludeAccess(MemRequest *mreq)
{
  // TODO: release mshr entry
  // TODO: this guy will do the goUp()
  mreq->goUp(1);
}

DSMCache::Line *DSMCache::allocateLine(PAddr addr)
{
  return NULL; // TODO: implement this
}

DSMCache::Line *DSMCache::getLine(PAddr addr)
{
  return NULL; // TODO: implement this
}

void DSMCache::updateLine(PAddr addr, Line *line)
{
  // TODO: implement this
  // TODO: copy state
}

void DSMCache::invalidateLine(PAddr addr, Line *line)
{
  // TODO: implement this
  // TODO: propagate invalidate up
}
