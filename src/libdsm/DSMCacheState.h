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
  DSM_INVALID     = 0x00000000,
  DSM_TRANS_READ  = 0x00000002,
  DSM_TRANS_WRITE = 0x00000003,
  // TODO: complete list
};

class DSMCacheState : public StateGeneric<DSMCacheState> {

private:
  unsigned int state;
  bool locked;
protected:
public:
  DSMCacheState() {
    state = DSM_INVALID;
    locked = false;
  }
  ~DSMCacheState() { }
  
  // BEGIN CacheCore interface
  bool isInvalid() const {
    if (state == DSM_INVALID)
      return true;
    
    return false;
  }
  
  void invalidate() {
    I(!locked);
    state = DSM_INVALID;
    locked = false;
  }

  bool isLocked() const {
    return locked;
  }

  bool isValid() const {
    return !(state == DSM_INVALID);
  }

  bool isDirty() const {
    return false; // TODO: implement the real dirty state and behavior
  }
  void setDirty() {
  }
  void setClean() {
  }

  // END CacheCore interface

};

#endif // DSMCACHESTATE_H
