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
#ifndef DSMPROTOCOL_H
#define DSMPROTOCOL_H

#include "ProtocolBase.h"
#include "MemRequest.h"
#include "NetAddrMap.h"
#include "PMessage.h"
#include "estl.h"

#include "DSMDebug.h"

#include "pool.h"

class DSMCache;

enum Protocol_t {
  DSMControl = 0,
  DSMData
}; 

class MsgRecord {
 private:
   Time_t startTime;
   bool cancelled;
   Message *msg;
   MemOperation reqType;
 public:
   MsgRecord() {
     cancelled = false;
     msg = 0;
   }
   ~MsgRecord() { // nothing to do 
   }
   Time_t getStartTime() {
     return startTime;
   }
   void setStartTime(Time_t time) {
     startTime = time;
   }
   bool isCancelled() {
     return cancelled;
   }
   void cancel() {
     cancelled = true;
   }
   Message *getMessage() {
     return msg;
   }
   void setMessage(Message *message, MemOperation memop) {
     I(message);
     startTime = globalClock;
     cancelled = false;
     msg       = message;
     reqType   = memop;
   }
   MemOperation getReqType() {
     return reqType;
   }
   void setMemOperation(MemOperation memop) {
     reqType = memop;
   }
};

class DSMProtocol : public ProtocolBase {
private:
protected:
  
  static pool<MsgRecord> recordPool;
  friend class pool<MsgRecord>;

  typedef HASH_MAP<PAddr, MsgRecord *> MsgRecByAddrType;
  static MsgRecByAddrType msgsInService;

  typedef HASH_MAP<Message *, MsgRecord *, MsgPtrHashFunc> MsgRecByMsgType;
  static MsgRecByMsgType msgsToCancel;

  Protocol_t protocolType;
  
  DSMCache *pCache;

public:

  Protocol_t getProtocolType() {
    return protocolType;
  }

  DSMProtocol(DSMCache *cache, InterConnection *network, 
	      RouterID_t rID, PortID_t pID) 
    : ProtocolBase(network, rID, pID)
    , pCache(cache)
    {
    
    }

  virtual ~DSMProtocol();

  // BEGIN interface with cache
  virtual void read(MemRequest *mreq);
  virtual void write(MemRequest *mreq);
  virtual void writeBack(MemRequest *mreq);
  virtual void returnAccessBelow(MemRequest *mreq);
  // END interface with cache

  // general purpose routines
  void sendMemMsg(MemRequest *mreq, Time_t when, MessageType msgType);
  void addToActiveList(Message *message);
  void addToCancelList(MsgRecord *msgRec);
  void removeFromActiveList(Message *message);
  void removeFromCancelList(Message *message);
  bool isInActiveList(Message *message);
  bool isInCancelList(Message *message);

  // BEGIN interface of ControlProtocol

  // memory access interface
  virtual void sendMemRead(MemRequest *mreq, Time_t when);
  virtual void sendMemWrite(MemRequest *mreq, Time_t when);
  virtual void sendMemReadAck(MemRequest *mreq, Time_t when);
  virtual void sendMemWriteAck(MemRequest *mreq, Time_t when);
  
  virtual void memReadHandler(Message *msg) { I(0); }
  virtual void memWriteHandler(Message *msg) { I(0); }
  virtual void memReadAckHandler(Message *msg) { I(0); }
  virtual void memWriteAckHandler(Message *msg) { I(0); }

  // coherent cache access interface
  virtual void sendReadMiss(MemRequest *mreq, Time_t when);
  virtual void sendWriteMiss(MemRequest *mreq, Time_t when);
  virtual void sendInvalidate(MemRequest *mreq, Time_t when);
  virtual void sendReadMissAck(MemRequest *mreq, Time_t when);
  virtual void sendWriteMissAck(MemRequest *mreq, Time_t when);
  virtual void sendInvalidateAck(MemRequest *mreq, Time_t when);

  virtual void readMissHandler(Message *msg) { I(0); }
  virtual void writeMissHandler(Message *msg) { I(0); }
  virtual void invalidateHandler(Message *msg) { I(0); }
  virtual void readMissAckHandler(Message *msg) { I(0); }
  virtual void writeMissAckHandler(Message *msg) { I(0); }
  virtual void invalidateAckHandler(Message *msg) { I(0); }

  // END interface of ControlProtocol

  // BEGIN interface of DataProtocol

  virtual void sendData(MemRequest *mreq, Time_t when);
  virtual void dataHandler(Message *msg) { I(0); }

  // END interface of DataProtocol
};

class DSMControlProtocol : public DSMProtocol {

public:  
  DSMControlProtocol(DSMCache *cache, InterConnection *network, 
		     RouterID_t rID, PortID_t pID) 
    : DSMProtocol(cache, network, rID, pID) 
    {
      protocolType = DSMControl;
      
      ProtocolCBBase *pcb;
      
      // register all types of messages
      pcb = new ProtocolCB<DSMProtocol, 
	                   &DSMProtocol::memReadHandler>(this);
      registerHandler(pcb, DSMMemReadMsg);
      
      pcb = new ProtocolCB<DSMProtocol, 
	                   &DSMProtocol::memWriteHandler>(this);
      registerHandler(pcb, DSMMemWriteMsg);

      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::memReadAckHandler>(this);
      registerHandler(pcb, DSMMemReadAckMsg);
      
      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::memWriteAckHandler>(this);
      registerHandler(pcb, DSMMemWriteAckMsg);

      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::readMissHandler>(this);
      registerHandler(pcb, DSMReadMissMsg);

      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::writeMissHandler>(this);
      registerHandler(pcb, DSMWriteMissMsg);

      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::invalidateHandler>(this);
      registerHandler(pcb, DSMInvalidateMsg);
      
      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::readMissAckHandler>(this);
      registerHandler(pcb, DSMReadMissAckMsg);
      
      pcb = new ProtocolCB<DSMProtocol, 
	                   &DSMProtocol::writeMissAckHandler>(this);
      registerHandler(pcb, DSMWriteMissAckMsg);
      
      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::invalidateAckHandler>(this);
      registerHandler(pcb, DSMInvalidateAckMsg);
    }

  ~DSMControlProtocol();

  void read(MemRequest *mreq);
  void write(MemRequest *mreq);
  void writeBack(MemRequest *mreq);
  void returnAccessBelow(MemRequest *mreq);

  void sendMemRead(MemRequest *mreq, Time_t when); 
  void sendMemWrite(MemRequest *mreq, Time_t when);
  void sendMemReadAck(MemRequest *mreq, Time_t when);
  void sendMemWriteAck(MemRequest *mreq, Time_t when);

  void memReadHandler(Message *msg);
  void memWriteHandler(Message *msg);
  void memReadAckHandler(Message *msg);
  void memWriteAckHandler(Message *msg);

  void sendReadMiss(MemRequest *mreq, Time_t when);
  void sendWriteMiss(MemRequest *mreq, Time_t when);
  void sendInvalidate(MemRequest *mreq, Time_t when);
  void sendReadMissAck(MemRequest *mreq, Time_t when);
  void sendWriteMissAck(MemRequest *mreq, Time_t when);
  void sendInvalidateAck(MemRequest *mreq, Time_t when);

  void readMissHandler(Message *msg);
  void writeMissHandler(Message *msg); 
  void invalidateHandler(Message *msg); 
  void readMissAckHandler(Message *msg); 
  void writeMissAckHandler(Message *msg); 
  void invalidateAckHandler(Message *msg); 
};

class DSMDataProtocol : public DSMProtocol {

public:
  DSMDataProtocol(DSMCache *cache, InterConnection *network, 
		  RouterID_t rID, PortID_t pID) 
    : DSMProtocol(cache, network, rID, pID) 
    {
      protocolType = DSMData;

      ProtocolCBBase *pcb;

      pcb = new ProtocolCB<DSMProtocol, 
	                  &DSMProtocol::dataHandler>(this);
      registerHandler(pcb, DSMDataMsg);

    }
  
  ~DSMDataProtocol();

  void sendData(MemRequest *mreq, Time_t when);
  void dataHandler(Message *msg);


};

#endif // DSMPROTOCOL_H
