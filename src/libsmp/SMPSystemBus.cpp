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

#include "SMPSystemBus.h"
#include "SMemorySystem.h"
#include "SMPCache.h"
#include "SMPDebug.h"

SMPSystemBus::SMPSystemBus(SMemorySystem *dms, const char *section, const char *name)
  : MemObj(section, name)
{
  MemObj *lowerLevel = NULL;

  I(dms);
  lowerLevel = dms->declareMemoryObj(section, "lowerLevel");

  if (lowerLevel != NULL)
    addLowerLevel(lowerLevel);

  SescConf->isLong(section, "numPorts");
  SescConf->isLong(section, "portOccp");

  char cadena[100];
  sprintf(cadena,"Data%s", name);
  cachePort = PortGeneric::create(cadena, 
				  SescConf->getLong(section, "numPorts"), 
				  SescConf->getLong(section, "portOccp"));

  // TODO: have a control and a data port separetely
}

SMPSystemBus::~SMPSystemBus() 
{
  // do nothing
}

Time_t SMPSystemBus::getNextFreeCycle() const
{
  return cachePort->calcNextSlot();
}

bool SMPSystemBus::canAcceptStore(PAddr addr) const
{
  return true;
}

void SMPSystemBus::access(MemRequest *mreq)
{
  GMSG(mreq->getPAddr() < 1024,
       "mreq dinst=0x%p paddr=0x%x vaddr=0x%x memOp=%d",
       mreq->getDInst(),
       (unsigned int) mreq->getPAddr(),
       (unsigned int) mreq->getVaddr(),
       mreq->getMemOperation());
  
  I(mreq->getPAddr() > 1024); 

  GLOG(SMPDBG_MSGS, "SMPSystemBus::access addr = 0x%08x, type = %d", 
       (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  switch(mreq->getMemOperation()){
  case MemRead:     read(mreq);      break;
  case MemReadW:    
  case MemWrite:    write(mreq);     break;
  case MemPush:     push(mreq);      break;
  default:          specialOp(mreq); break;
  }

  // MemRead means I need to read the data
  // MemReadW means I need to write the data, but I don't have it
  // MemWrite means I need to write the data, but I don't have permission
  // MemPush means I don't have space to keep the data, send it to memory
}

void SMPSystemBus::read(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  if( ( (SMPMemRequest*) mreq)->isNew())
    doReadCB::scheduleAbs(nextSlot(), this, mreq);
  else
    doRead(mreq);
}

void SMPSystemBus::write(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  if( ( (SMPMemRequest*) mreq)->isNew())
    doWriteCB::scheduleAbs(nextSlot(), this, mreq);
  else
    doWrite(mreq);
}

void SMPSystemBus::push(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  doPushCB::scheduleAbs(nextSlot(), this, addr);  
}

void SMPSystemBus::specialOp(MemRequest *mreq)
{
  I(0);
  mreq->goUp(1); // TODO: implement atomic ops?
}

void SMPSystemBus::doRead(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if( ( (SMPMemRequest*) mreq)->isNew()) {
    I(pendReqsTable.find(addr) == pendReqsTable.end());
    ((SMPMemRequest*) mreq)->setNotNew();
    GLOG(SMPDBG_MSGS, 
	 "SMPSystemBus::doRead new access addr = 0x%08x, type = %d", 
	 (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

    // operation is starting now, add it to the pending requests buffer
    pendReqsTable[addr] = upperLevel.size() - 1;
    // distribute requests to other caches, wait for responses
    for(ulong i = 0; i < upperLevel.size(); i++) {
      if(upperLevel[i] != static_cast<SMPMemRequest *>(mreq)->getRequestor())
	upperLevel[i]->returnAccess(mreq);
    }
  } 
  else {
    // operation has already been sent to other caches, receive responses

    I(pendReqsTable[addr] > 0);
    I(pendReqsTable[addr] < (int) upperLevel.size());

    pendReqsTable[addr]--;
    if(pendReqsTable[addr] != 0) {
      // this is an intermediate response, request is not serviced yet
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doRead interm. response addr = 0x%08x, type = %d",
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
      return;
    }
    // this is the final response, request can go up now
    pendReqsTable.erase(addr);
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    if(sreq->getState() == MESI_INVALID) {
      // could not find data anywhere, go to memory
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doRead response (INVALID) addr = 0x%08x, type = %d",
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
      
      mreq->goDown(1, lowerLevel[0]);
      return;
    }

    if(sreq->getState() == MESI_MODIFIED) {
      // if data was dirty, it will go to memory too
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doRead response (MODIFIED) addr = 0x%08x, type = %d, response = 0x%08x",
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) static_cast<SMPMemRequest *>(mreq)->getState());
      
      doPush(addr);
    } else {

      I(sreq->getState() == MESI_SHARED || sreq->getState() == MESI_EXCLUSIVE);
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doRead response (SH. OR EXCL.) addr = 0x%08x, type = %d, response = 0x%08x",
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) static_cast<SMPMemRequest *>(mreq)->getState());
      
    }
    sreq->goUp(1);
  }
}

void SMPSystemBus::doWrite(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  if( ( (SMPMemRequest*) mreq)->isNew()) {
    I(pendReqsTable.find(addr) == pendReqsTable.end());
    ((SMPMemRequest*) mreq)->setNotNew();
    GLOG(SMPDBG_MSGS, "SMPSystemBus::doWrite new access addr = 0x%08x, type = %d", 
	 (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

    // operation is starting now, add it to the pending requests buffer
    pendReqsTable[addr] = upperLevel.size() - 1;
    // distribute requests to other caches, wait for responses
    for(ulong i = 0; i < upperLevel.size(); i++) {
      if(upperLevel[i] != static_cast<SMPMemRequest *>(mreq)->getRequestor())
	upperLevel[i]->returnAccess(mreq);
    }
  } 
  
  else {
    // operation has already been sent to other caches, receive responses
    I(pendReqsTable[addr] > 0);
    I(pendReqsTable[addr] < (int) upperLevel.size());

    pendReqsTable[addr]--;
    if(pendReqsTable[addr] != 0) {
      // this is an intermediate response, request is not serviced yet
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doWrite interm. response addr = 0x%08x, type = %d", 
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
      return;
    }
    // this is the final response, request can go up now
    pendReqsTable.erase(addr);
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    if(sreq->getState() == MESI_INVALID && 
       sreq->getMemOperation() == MemReadW) {
      // could not find data anywhere, go to memory
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doWrite response (INVALID) addr = 0x%08x, type = %d",
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
      
      mreq->goDown(1, lowerLevel[0]);
      return;
    }

    if(sreq->getState() == MESI_MODIFIED) {
      // if data was dirty, it will go to memory too
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doWrite response (MODIFIED) addr = 0x%08x, type = %d", 
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

      doPush(addr);
    } else {

      I((sreq->getState() == MESI_INVALID && 
	 sreq->getMemOperation() == MemWrite) ||
	sreq->getState() == MESI_EXCLUSIVE || sreq->getState() == MESI_SHARED);
      GLOG(SMPDBG_MSGS, 
	   "SMPSystemBus::doWrite response (INV., SH. OR EXCL.) addr = 0x%08x, type = %d, state = 0x%08x",
	   (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) static_cast<SMPMemRequest *>(mreq)->getState());
    }
    sreq->goUp(1);  
  }
}
 
void SMPSystemBus::doPush(PAddr addr)
{
  CBMemRequest::create(1, lowerLevel[0], MemPush, addr, 0);
  // TODO: need to include bw consuption
}

void SMPSystemBus::invalidate(PAddr addr, ushort size, MemObj *oc)
{
  invUpperLevel(addr, size, oc);
}

void SMPSystemBus::doInvalidate(PAddr addr, ushort size)
{
  I(0);
}

void SMPSystemBus::returnAccess(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  GLOG(SMPDBG_MSGS, "SMPSystemBus::returnAccess for address 0x%x", 
       (uint) mreq->getPAddr());

  mreq->goUp(1);
}

