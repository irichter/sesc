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

// TODO: timing: should the protocol decision take any time?

#ifndef SMPPROTOCOL_H
#define SMPPROTOCOL_H


#include "SMPMemRequest.h"
#include "MSHR.h"
#include "estl.h"

#include "SMPDebug.h"

class SMPCache;

enum Protocol_t {
  MESI_Protocol = 10
}; 

class SMPProtocol {
private:
protected:
  
  Protocol_t protocolType;
  
  SMPCache *pCache;

public:

  Protocol_t getProtocolType() {
    return protocolType;
  }

  SMPProtocol(SMPCache *cache);
  virtual ~SMPProtocol();

  // BEGIN interface with cache
  virtual void read(MemRequest *mreq);
  virtual void write(MemRequest *mreq);
  virtual void writeBack(MemRequest *mreq);
  virtual void returnAccess(MemRequest *mreq);
  // END interface with cache

  // BEGIN interface of Protocol

  // coherent cache access interface
  virtual void sendReadMiss(MemRequest *mreq);
  virtual void sendWriteMiss(MemRequest *mreq);
  virtual void sendInvalidate(MemRequest *mreq);
  virtual void sendReadMissAck(SMPMemRequest *sreq);
  virtual void sendWriteMissAck(SMPMemRequest *sreq);
  virtual void sendInvalidateAck(SMPMemRequest *sreq);

  virtual void readMissHandler(SMPMemRequest *sreq) { I(0); }
  virtual void writeMissHandler(SMPMemRequest *sreq) { I(0); }
  virtual void invalidateHandler(SMPMemRequest *sreq) { I(0); }
  virtual void readMissAckHandler(SMPMemRequest *sreq) { I(0); }
  virtual void writeMissAckHandler(SMPMemRequest *sreq) { I(0); }
  virtual void invalidateAckHandler(SMPMemRequest *sreq) { I(0); }

  virtual void sendData(SMPMemRequest *sreq);
  virtual void dataHandler(SMPMemRequest *sreq) { I(0); }

  // END interface of Protocol
};

class MESIProtocol : public SMPProtocol {

protected:

public:  
  MESIProtocol(SMPCache *cache); 
  ~MESIProtocol();

  void read(MemRequest *mreq);
  void doRead(MemRequest *mreq);
  typedef CallbackMember1<MESIProtocol, MemRequest *, 
                         &MESIProtocol::doRead> doReadCB;

  void write(MemRequest *mreq);
  void doWrite(MemRequest *mreq);
  typedef CallbackMember1<MESIProtocol, MemRequest *, 
                         &MESIProtocol::doWrite> doWriteCB;
    
  void writeBack(MemRequest *mreq);
  void returnAccess(SMPMemRequest *sreq);

  void sendReadMiss(MemRequest *mreq);
  void sendWriteMiss(MemRequest *mreq);
  void sendInvalidate(MemRequest *mreq);

  void doSendReadMiss(MemRequest *mreq);
  void doSendWriteMiss(MemRequest *mreq);
  void doSendInvalidate(MemRequest *mreq);

  typedef CallbackMember1<MESIProtocol, MemRequest *,
                         &MESIProtocol::doSendReadMiss> doSendReadMissCB;
  typedef CallbackMember1<MESIProtocol, MemRequest *,
                         &MESIProtocol::doSendWriteMiss> doSendWriteMissCB;
  typedef CallbackMember1<MESIProtocol, MemRequest *,
                         &MESIProtocol::doSendInvalidate> doSendInvalidateCB;

  void sendReadMissAck(SMPMemRequest *sreq);
  void sendWriteMissAck(SMPMemRequest *sreq);
  void sendInvalidateAck(SMPMemRequest *sreq);

  void readMissHandler(SMPMemRequest *sreq);
  void writeMissHandler(SMPMemRequest *sreq); 
  void invalidateHandler(SMPMemRequest *sreq); 

  typedef CallbackMember1<MESIProtocol, SMPMemRequest *,
                   &MESIProtocol::sendWriteMissAck> sendWriteMissAckCB;
  typedef CallbackMember1<MESIProtocol, SMPMemRequest *,
                   &MESIProtocol::sendInvalidateAck> sendInvalidateAckCB;
  typedef CallbackMember1<MESIProtocol, SMPMemRequest *,
                   &MESIProtocol::writeMissHandler> writeMissHandlerCB;
  typedef CallbackMember1<MESIProtocol, SMPMemRequest *,
                   &MESIProtocol::invalidateHandler> invalidateHandlerCB;

  void readMissAckHandler(SMPMemRequest *sreq); 
  void writeMissAckHandler(SMPMemRequest *sreq); 
  void invalidateAckHandler(SMPMemRequest *sreq); 


  // data related
  void sendData(SMPMemRequest *sreq);
  void dataHandler(SMPMemRequest *sreq);

  MESIState_t combineResponses(MESIState_t currentResponse, MESIState_t localState);
};

#endif // SMPPROTOCOL_H
