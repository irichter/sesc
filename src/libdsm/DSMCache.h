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

#ifndef DSMCACHE_H
#define DSMCACHE_H

#include "MemRequest.h"
#include "MemObj.h"
#include "DMemorySystem.h"
#include "DSMProtocol.h"
#include "DSMCacheState.h"
#include "MSHR.h"

#include "vector"

class DSMCache : public MemObj {
public:
  typedef CacheGeneric<DSMCacheState, PAddr, false>            CacheType;
  typedef CacheGeneric<DSMCacheState, PAddr, false>::CacheLine Line;

private:
protected:
  CacheType *cache;

  PortGeneric *cachePort;
  TimeDelta_t missDelay;
  TimeDelta_t hitDelay;

  MSHR<PAddr> *mshrIN;  // MSHR for requests coming from upper levels
  MSHR<PAddr> *mshrOUT; // MSHR for requests coming from remote nodes (protocol)

  // BEGIN statistics
  // TODO: fill this up
  // END statistics

  unsigned int numNetworks;

  typedef std::vector<DSMProtocol *> ProtocolHolderList;
  ProtocolHolderList protocolHolder;

  // interface with upper level
  void read(MemRequest *mreq);
  void write(MemRequest *mreq);
  void specialOp(MemRequest *mreq);

  // port usage accounting
  Time_t nextSlot() {
    return cachePort->nextSlot();
  }

  // local routines
  void doRead(MemRequest *mreq);
  void doWrite(MemRequest *mreq);
  void doWriteBack(PAddr addr) const;

  typedef CallbackMember1<DSMCache, MemRequest *, 
                         &DSMCache::doRead> doReadCB;
  typedef CallbackMember1<DSMCache, MemRequest *,
                         &DSMCache::doWrite> doWriteCB;

public:
  DSMCache(DMemorySystem *gms, const char *section, const char *name);
  ~DSMCache();

  // interface for attaching/detaching networks+protocols
  void attachNetwork(Protocol_t protocol, 
		     InterConnection *net, 
		     RouterID_t rID, 
		     PortID_t pID);
  void detachNetworks();


  // BEGIN MemObj interface
  
  // port usage accounting
  Time_t getNextFreeCycle() const;

  // interface with upper level
  bool canAcceptStore(PAddr addr);
  void access(MemRequest *mreq);
  void invalidate(PAddr addr, ushort size, MemObj *oc);
  void doInvalidate(PAddr addr, ushort size);
  
  // interface with lower level
  void returnAccess(MemRequest *mreq);

  // END MemObj interface

  // BEGIN protocol interface 
  
  // access to lower level
  void readBelow(MemRequest *mreq);
  void writeBelow(MemRequest *mreq);

  // protocol has decided what to do and wants to finish the coherence
  // transaction, calling concludeAccess
  void concludeAccess(MemRequest *mreq);

  // line management - actions protocol can perform on cache lines
  Line *allocateLine(PAddr addr);
  Line *getLine(PAddr addr);
  void updateLine(PAddr addr);
  void invalidateLine(PAddr addr);

  // interface to get pointer to another protocol
  // this should be extended for more complex protocols
  DSMProtocol *getControlProtocol() { return protocolHolder[0]; }
  DSMProtocol *getDataProtocol() { return protocolHolder[1]; }

  // END protocol interface


};

#endif // DSMCACHE_H
