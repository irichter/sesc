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

#ifndef MESICACHESTATE_H
#define MESICACHESTATE_H

#include "CacheCore.h"

enum MESIState_t {
  MESI_INVALID            = 0x00000000, // data is invalid
  MESI_TRANS              = 0x10000000, // all transient states start w/ 0x1
  MESI_TRANS_RD           = 0x12000000, // transient state, just started rd
  MESI_TRANS_WR           = 0x13000000, // transient state, just started wr
  MESI_TRANS_DISP         = 0x14000000, // transient state, invalidation
                                        // due to displacement
  MESI_VALID              = 0x00100000, // data is valid
  MESI_EXCLUSIVE          = 0x00100001, // data is present only in local cache
  MESI_MODIFIED           = 0x00100010, // data is different from memory 
  MESI_SHARED             = 0x00100100, // data is shared
};

class MESICacheState : public StateGeneric<> {

private:
  MESIState_t state;
protected:
public:
  MESICacheState() 
    : StateGeneric<>()
  {
    state = MESI_INVALID;
  }
  ~MESICacheState() { }
  
  // BEGIN CacheCore interface
  bool isValid() const {
    return (state!=MESI_INVALID);
  }
  
  void invalidate() {
    // cannot invalidate if line is locked
    GI(isLocked(), state == MESI_TRANS); 
    clearTag();
    state = MESI_INVALID;
  }

  void validate(bool exclusive = false) {
    I(state == MESI_TRANS);
    if (exclusive)
      changeStateTo(MESI_EXCLUSIVE);
    else
      changeStateTo(MESI_SHARED);
  }

  bool isLocked() const {
    return (state & MESI_TRANS); // state has to be transient to be locked
  }

  void lock() {
    I(state != MESI_TRANS);
    changeStateTo(MESI_TRANS);
  }

  bool isDirty() const {
    return (state & MESI_MODIFIED);
  }

  void makeDirty() {
    I(state == MESI_EXCLUSIVE || state == MESI_MODIFIED);
    changeStateTo(MESI_MODIFIED);
  }

  // END CacheCore interface

  bool canBeRead() {
    return (state & MESI_VALID);
  }

  bool canBeWritten() {
    return (state == MESI_EXCLUSIVE || state == MESI_MODIFIED);
  }

  bool isTransient() {
    return (state & MESI_TRANS);
  }

  MESIState_t getState() {
    return state;
  }
  void changeStateTo(MESIState_t newstate) {
    // should use invalidate interface 
    I(newstate != MESI_INVALID);

    state = newstate;
  }

};

#endif // MESICACHESTATE_H
