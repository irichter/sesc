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

#include "DSMProtocol.h"
#include "DSMCache.h"

DSMProtocol::~DSMProtocol() 
{
  // nothing to do
}

void DSMProtocol::read(MemRequest *mreq)
{
  I(0);
}

void DSMProtocol::write(MemRequest *mreq)
{
  I(0);
}

void DSMProtocol::writeBack(MemRequest *mreq)
{
  I(0);
}

void DSMProtocol::returnAccessBelow(MemRequest *mreq)
{
  I(0);
}

void DSMProtocol::sendMemMsg(MemRequest *mreq, Time_t when, MessageType msgType)
{
  PAddr paddr = mreq->getPAddr();
  
  NetDevice_t dstID = NetAddrMap::getMapping(paddr);
  ProtocolBase *dstPB = getProtocolBase(dstID);
  
  PMessage *msg = PMessage::createMsg(msgType, this, dstPB, mreq);

  mreq->setMessage(msg);
  
  sendMsgAbs(when, msg);  
}

void DSMProtocol::sendMemRead(MemRequest *mreq, Time_t when) 
{ 
  I(0);
}

void DSMProtocol::sendMemWrite(MemRequest *mreq, Time_t when) 
{ 
  I(0);
}

void DSMProtocol::sendMemReadAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendMemWriteAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendReadMiss(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendWriteMiss(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendInvalidate(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendReadMissAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendWriteMissAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendInvalidateAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMProtocol::sendData(MemRequest *mreq, Time_t when)
{
  I(0);
}

DSMControlProtocol::~DSMControlProtocol()
{
  // nothing to do
}

void DSMControlProtocol::read(MemRequest *mreq)
{
  sendMemRead(mreq, globalClock); 
  // TODO: make it a real protocol decision + timing
}

void DSMControlProtocol::write(MemRequest *mreq)
{
  sendMemWrite(mreq, globalClock); 
  // TODO: make it a real protocol decision + timing  
}

void DSMControlProtocol::writeBack(MemRequest *mreq)
{
  I(0);
}

void DSMControlProtocol::returnAccessBelow(MemRequest *mreq)
{
  if (mreq->getMemOperation() == MemRead) {

    sendMemReadAck(mreq, globalClock);
    DSMProtocol *dp = pCache->getDataProtocol();
    I(dp->getProtocolType() == DSMData);
    dp->sendData(mreq, globalClock + 1);
  
  } else if (mreq->getMemOperation() == MemWrite) {

    sendMemWriteAck(mreq, globalClock);
    // write, no need to send data

  } else I(0); // request should be one of types above  
}

void DSMControlProtocol::sendMemRead(MemRequest *mreq, Time_t when) 
{
  sendMemMsg(mreq, when, DSMMemReadMsg);
}

void DSMControlProtocol::sendMemWrite(MemRequest *mreq, Time_t when) 
{
  sendMemMsg(mreq, when, DSMMemWriteMsg);
}

void DSMControlProtocol::sendMemReadAck(MemRequest *mreq, Time_t when)
{
  PMessage *msg = static_cast<PMessage *>(mreq->getMessage());

  msg->reverseDirection();
  msg->setType(DSMMemReadAckMsg);

  sendMsgAbs(when, msg);  
}

void DSMControlProtocol::sendMemWriteAck(MemRequest *mreq, Time_t when)
{
  PMessage *msg = static_cast<PMessage *>(mreq->getMessage());

  msg->reverseDirection();
  msg->setType(DSMMemWriteAckMsg);

  sendMsgAbs(when, msg);  
}

void DSMControlProtocol::memReadHandler(Message *msg) 
{
  GLOG(DSMDBG_MSGS, "DSMControlProtocol::memReadHandler invoked");
  
  pCache->readBelow((static_cast<PMessage *>(msg))->getMemRequest());
}

void DSMControlProtocol::memWriteHandler(Message *msg) 
{
  GLOG(DSMDBG_MSGS, "DSMControlProtocol::memWriteHandler invoked.");
  
  pCache->writeBelow((static_cast<PMessage *>(msg))->getMemRequest());
}

void DSMControlProtocol::memReadAckHandler(Message *msg)
{
  GLOG(DSMDBG_MSGS, "DSMControlProtocol::memReadAckHandler invoked.");
  
  pCache->concludeAccess((static_cast<PMessage *>(msg))->getMemRequest());
  msg->garbageCollect();
}

void DSMControlProtocol::memWriteAckHandler(Message *msg)
{
  GLOG(DSMDBG_MSGS, "DSMControlProtocol::memWriteAckHandler invoked.");

  pCache->concludeAccess((static_cast<PMessage *>(msg))->getMemRequest());
  msg->garbageCollect();
}

void DSMControlProtocol::sendReadMiss(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMControlProtocol::sendWriteMiss(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMControlProtocol::sendInvalidate(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMControlProtocol::sendReadMissAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMControlProtocol::sendWriteMissAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMControlProtocol::sendInvalidateAck(MemRequest *mreq, Time_t when)
{
  I(0);
}

void DSMControlProtocol::readMissHandler(Message *msg) 
{
  I(0);
}

void DSMControlProtocol::writeMissHandler(Message *msg) 
{
  I(0);
}

void DSMControlProtocol::invalidateHandler(Message *msg) 
{
  I(0);
}

void DSMControlProtocol::readMissAckHandler(Message *msg) 
{
  I(0);
}

void DSMControlProtocol::writeMissAckHandler(Message *msg) 
{
  I(0);
}

void DSMControlProtocol::invalidateAckHandler(Message *msg) 
{
  I(0);
}

DSMDataProtocol::~DSMDataProtocol()
{
  // nothing to do
}

void DSMDataProtocol::sendData(MemRequest *mreq, Time_t when)
{
  //I(0);
}

void DSMDataProtocol::dataHandler(Message *msg) 
{
  I(0);
}
