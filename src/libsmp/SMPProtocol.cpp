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

#include "SMPProtocol.h"
#include "SMPCache.h"

SMPProtocol::SMPProtocol(SMPCache *cache)
  : pCache(cache)
{
}

SMPProtocol::~SMPProtocol() 
{
  // nothing to do
}

void SMPProtocol::read(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::write(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::writeBack(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::returnAccess(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::sendReadMiss(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::sendWriteMiss(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::sendInvalidate(MemRequest *mreq)
{
  I(0);
}

void SMPProtocol::sendReadMissAck(SMPMemRequest *sreq)
{
  I(0);
}

void SMPProtocol::sendWriteMissAck(SMPMemRequest *sreq)
{
  I(0);
}

void SMPProtocol::sendInvalidateAck(SMPMemRequest *sreq)
{
  I(0);
}

void SMPProtocol::sendData(SMPMemRequest *sreq)
{
  I(0);
}

MESIProtocol::MESIProtocol(SMPCache *cache)
  : SMPProtocol(cache)
{
  protocolType = MESI_Protocol;
}

MESIProtocol::~MESIProtocol()
{
  // nothing to do
}

void MESIProtocol::read(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();

  // time for this has already been accounted in SMPCache::read
  SMPCache::Line *l = pCache->getLine(addr);

  // if line is in transient state, read should not have been called
  GI(l, !l->isTransient()); 

  if(!l)
    l = pCache->allocateLine(addr, doReadCB::create(this, mreq));

  if(!l) {
    // not possible to allocate a line, will be called back later
    GLOG(SMPDBG_MSGS, "MESIProtocol::read cache %s for addr 0x%x, type = %d, no line to alloc", 
	 pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
    return;
  }

  GLOG(SMPDBG_MSGS, "MESIProtocol::read cache %s for addr 0x%x, type = %d, state=%d, line alloc", 
       pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());

  l->changeStateTo(MESI_TRANS);
  doRead(mreq);
}

void MESIProtocol::doRead(MemRequest *mreq)
{
  SMPCache::Line *l = pCache->getLine(mreq->getPAddr());

  GLOG(SMPDBG_MSGS, "MESIProtocol::doRead cache %s for addr 0x%x, type = %d", 
       pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  I(l->isTransient());

  // go into transient state and send request out
  l->changeStateTo(MESI_TRANS_RD);

  sendReadMiss(mreq);
}

void MESIProtocol::write(MemRequest *mreq)
{
  SMPCache::Line *l = pCache->getLine(mreq->getPAddr());

  // if line is in transient state, write should not have been called
  GI(l, !l->isTransient() && l->getState() == MESI_SHARED); 

  // hit in shared state
  if (l && !l->canBeWritten()) {
    GLOG(SMPDBG_MSGS, "MESIProtocol::write cache %s (hs) for addr 0x%x, type = %d, state = 0x%08x", 
	 pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation(), (uint) l->getState());

    l->changeStateTo(MESI_TRANS_WR);
    sendInvalidate(mreq);
    return;
  }

  // miss - check other caches
  I(!l);
  if (mreq->getMemOperation() != MemReadW)
    mreq->mutateWriteToRead();

  l = pCache->allocateLine(mreq->getPAddr(),
			   doWriteCB::create(this, mreq));


  if(!l) {
    // not possible to allocate a line, will be called back later
    GLOG(SMPDBG_MSGS, "MESIProtocol::write cache %s (m) for addr 0x%x, type = %d, no line to alloc", 
	 pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());
    return;
  }

  GLOG(SMPDBG_MSGS, "MESIProtocol::write cache %s (m) for addr 0x%x, type = %d, line allocated", 
       pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());


  l->changeStateTo(MESI_TRANS);
  doWrite(mreq);
}

void MESIProtocol::doWrite(MemRequest *mreq)
{
  SMPCache::Line *l = pCache->getLine(mreq->getPAddr());

  GLOG(SMPDBG_MSGS, "MESIProtocol::doWrite cache %s for addr 0x%x, type = %d", 
       pCache->getSymbolicName(), (uint) mreq->getPAddr(), (uint) mreq->getMemOperation());

  I(l->isTransient());

  l->changeStateTo(MESI_TRANS_WR);

  sendWriteMiss(mreq);
}

void MESIProtocol::writeBack(MemRequest *mreq)
{
  I(0);
}

void MESIProtocol::returnAccess(SMPMemRequest *sreq)
{
  I(0); // should not be executed for now.
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);

  if (sreq->getMemOperation() == MemRead) {
    if(sreq->getState() == MESI_INVALID)
      l->changeStateTo(MESI_EXCLUSIVE);
    else
      l->changeStateTo(MESI_SHARED);
  }
  else if (sreq->getMemOperation() == MemWrite) {
    l->changeStateTo(MESI_MODIFIED);
  }
  else I(0); // special ops not implemented yet

  pCache->concludeAccess(sreq->getOriginalRequest());
  sreq->destroy();
}

void MESIProtocol::sendReadMiss(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  doSendReadMiss(mreq);
}

void MESIProtocol::doSendReadMiss(MemRequest *mreq)
{
  I(pCache->getLine(mreq->getPAddr()));
  SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, true);

  GLOG(SMPDBG_MSGS, "MESIProtocol::doSendReadMiss cache %s addr = 0x%08x now",
       pCache->getSymbolicName(), (uint) mreq->getPAddr());
  
  pCache->sendBelow(sreq); 
}

void MESIProtocol::sendWriteMiss(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  doSendWriteMiss(mreq);
}

void MESIProtocol::doSendWriteMiss(MemRequest *mreq)
{
  I(pCache->getLine(mreq->getPAddr()));
  SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, true);
    
  GLOG(SMPDBG_MSGS, "MESIProtocol::doSendWriteMiss cache %s addr = 0x%08x now",
       pCache->getSymbolicName(), (uint) mreq->getPAddr());
  
  pCache->sendBelow(sreq); 
}

void MESIProtocol::sendInvalidate(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr();
  doSendInvalidate(mreq);
}

void MESIProtocol::doSendInvalidate(MemRequest *mreq)
{
  I(pCache->getLine(mreq->getPAddr()));
  SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, false);
    
  GLOG(SMPDBG_MSGS, "MESIProtocol::doSendInvalidate cache %s addr = 0x%08x now",
       pCache->getSymbolicName(), (uint) mreq->getPAddr());
    
  pCache->sendBelow(sreq);
}

void MESIProtocol::sendReadMissAck(SMPMemRequest *sreq)
{
  pCache->respondBelow(sreq);
}

void MESIProtocol::sendWriteMissAck(SMPMemRequest *sreq)
{
  pCache->respondBelow(sreq);
}

void MESIProtocol::sendInvalidateAck(SMPMemRequest *sreq)
{
  pCache->respondBelow(sreq);
}

void MESIProtocol::readMissHandler(SMPMemRequest *sreq) 
{
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);
  
  if(l && !l->isTransient()) {
    GLOG(SMPDBG_MSGS, "MESIProtocol::readMissHandler cache %s addr = 0x%08x, state = 0x%08x",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr(), (uint) l->getState());

    MESIState_t newResponse = combineResponses((MESIState_t) sreq->getState(),
					       l->getState());
    sreq->setState(newResponse);
    l->changeStateTo(MESI_SHARED);
  } else {
    GLOG(SMPDBG_MSGS, "MESIProtocol::readMissHandler cache %s addr = 0x%08x not found",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr());
  }

  sendReadMissAck(sreq);
  //sendData(sreq);
}

void MESIProtocol::writeMissHandler(SMPMemRequest *sreq) 
{
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);
  
  if(l) {
    GLOG(SMPDBG_MSGS, "MESIProtocol::writeMissHandler cache %s addr = 0x%08x, state = 0x%08x",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr(), (uint) l->getState());

    MESIState_t newResponse = combineResponses((MESIState_t) sreq->getState(),
					       l->getState());
    sreq->setState(newResponse);
    pCache->invalidateLine(addr, sendWriteMissAckCB::create(this, sreq));
    return;
  } else {
    GLOG(SMPDBG_MSGS, "MESIProtocol::writeMissHandler cache %s addr = 0x%08x not found",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr());
    sendWriteMissAck(sreq);
  } 

  //sendData(sreq);
}

void MESIProtocol::invalidateHandler(SMPMemRequest *sreq) 
{
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);
  
  if(l) {
    GLOG(SMPDBG_MSGS, "MESIProtocol::invalidateHandler cache %s addr = 0x%08x, state = 0x%08x",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr(), (uint) l->getState());
    
    MESIState_t newResponse = combineResponses((MESIState_t) sreq->getState(),
					       l->getState());
    sreq->setState(newResponse);
    pCache->invalidateLine(addr, sendInvalidateAckCB::create(this, sreq));
    return;
  } else {
    GLOG(SMPDBG_MSGS, "MESIProtocol::invalidateHandler cache %s addr = 0x%08x not found",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr());
    sendInvalidateAck(sreq);
  } 
 
}

void MESIProtocol::readMissAckHandler(SMPMemRequest *sreq) 
{
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);

  GLOG(SMPDBG_MSGS, "MESIProtocol::readMissAckHandler cache %s addr = 0x%08x result = 0x%08x",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr(), (uint) sreq->getState());

  if(sreq->getState() == MESI_INVALID)
    l->changeStateTo(MESI_EXCLUSIVE);
  else
    l->changeStateTo(MESI_SHARED);

  pCache->writeLine(addr);
  pCache->concludeAccess(sreq->getOriginalRequest());
  sreq->destroy();
}

void MESIProtocol::writeMissAckHandler(SMPMemRequest *sreq) 
{
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);

  GLOG(SMPDBG_MSGS, "MESIProtocol::writeMissAckHandler cache %s addr = 0x%08x result = 0x%08x",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr(), (uint) sreq->getState());

  I(l);
  I(l->getState() == MESI_TRANS_WR);
  GLOG(l->getState() != MESI_TRANS_WR, 
       "MESIProtocol::writeMissAckHandler cache %s addr = 0x%08x, state=0x%08x", 
       pCache->getSymbolicName(), (uint) addr, (uint) l->getState());
  l->changeStateTo(MESI_MODIFIED);

  pCache->writeLine(addr);
  pCache->concludeAccess(sreq->getOriginalRequest());
  sreq->destroy();
}

void MESIProtocol::invalidateAckHandler(SMPMemRequest *sreq) 
{
  PAddr addr = sreq->getPAddr();
  SMPCache::Line *l = pCache->getLine(addr);

  GLOG(SMPDBG_MSGS, "MESIProtocol::invalidateAckHandler cache %s addr = 0x%08x result = 0x%08x",
	 pCache->getSymbolicName(), (uint) sreq->getPAddr(), (uint) sreq->getState());


  I(l);
  l->changeStateTo(MESI_MODIFIED);

  pCache->concludeAccess(sreq->getOriginalRequest());
  sreq->destroy();
}

void MESIProtocol::sendData(SMPMemRequest *sreq)
{
  I(0);
}

void MESIProtocol::dataHandler(SMPMemRequest *sreq) 
{
  // this should cause an access to the cache
  // for now, I am assuming data comes with response, 
  // so the access is done in the ack handlers
  I(0);
}

MESIState_t MESIProtocol::combineResponses(MESIState_t currentResponse, 
					   MESIState_t localState)
{
  MESIState_t newResponse = currentResponse;

  if((localState == MESI_INVALID) || (localState & MESI_TRANS))
    return currentResponse;

  if(localState == MESI_SHARED) {
    I(currentResponse != MESI_EXCLUSIVE && currentResponse != MESI_MODIFIED);
    return localState;
  }

  I(localState == MESI_EXCLUSIVE || localState == MESI_MODIFIED);
  I(currentResponse != MESI_EXCLUSIVE && currentResponse != MESI_MODIFIED);
  return localState;
}
