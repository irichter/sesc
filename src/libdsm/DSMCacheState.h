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

#ifndef DSMCACHESTATE_H
#define DSMCACHESTATE_H

#include "CacheCore.h"

enum DSMState_t {
  DSM_INVALID            = 0x00000000, // data is invalid
  DSM_TRANS              = 0x10000000, // all transient states start w/ 0x1
  DSM_TRANS_RD           = 0x12000000, // transient state, just started rd
  DSM_TRANS_WR           = 0x13000000, // transient state, just started wr
  DSM_TRANS_RD_NEED_DATA = 0x14000000, // transient state, rd ack was received
  DSM_TRANS_WR_NEED_DATA = 0x15000000, // transient state, wr ack was received
  DSM_TRANS_RD_NEED_ACK  = 0x16000000, // transient state, rd data was received
  DSM_TRANS_WR_NEED_ACK  = 0x17000000, // transient state, wr data was received
  DSM_TRANS_RD_RETRY     = 0x18000000, // transient state, need to retry later
  DSM_TRANS_WR_RETRY     = 0x19000000, // transient state, need to retry later
  DSM_TRANS_RD_MEM       = 0x1a000000, // transient state, read from memory
  DSM_TRANS_WR_MEM       = 0x1b000000, // transient state, write to memory
  DSM_VALID              = 0x00100000, // data is valid
  DSM_EXCLUSIVE          = 0x00100001, // data is only present in local cache
  DSM_DIRTY              = 0x00100010, // data is different from memory
  DSM_OWNED              = 0x00100100  // cache is responsible for data
};

class DSMCacheState : public StateGeneric<DSMCacheState> {

private:
  DSMState_t state;
protected:
public:
  DSMCacheState() 
    : StateGeneric<DSMCacheState>()
  {
    state = DSM_INVALID;
  }
  ~DSMCacheState() { }
  
  // BEGIN CacheCore interface
  bool isInvalid() const {
    return (state == DSM_INVALID);
  }
  
  void invalidate() {
    I(!isLocked()); // cannot invalidate if line is locked
    StateGeneric<DSMCacheState>::invalidate();
    state = DSM_INVALID;
  }

  bool isLocked() const {
    return (state & DSM_TRANS); // state has to be transient to be locked
  }

  bool isDirty() const {
    return (state & DSM_DIRTY);
  }

  // END CacheCore interface

  bool canBeRead() {
    return (state & DSM_VALID);
  }

  bool canBeWritten() {
    return (state & DSM_EXCLUSIVE);
  }

  bool isTransient() {
    return (state & DSM_TRANS);
  }

  DSMState_t getState() {
    return state;
  }
  void changeStateTo(DSMState_t newstate) {
    I(newstate != DSM_INVALID);
    state = newstate;
  }

};

#endif // DSMCACHESTATE_H
