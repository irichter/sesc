/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by James Tuck
                  Jose Renau

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
#ifndef GDEBUGCC_H
#define GDEBUGCC_H

#include "estl.h"
#include "GStats.h"
#include "nanassert.h"

class GDebugCC : public GStats {
private:
  const char *my_desc;

  class MyInfo {
  public:
    MyInfo() : count(0), max(0) {}

    MyInfo(long icount, ulong imax) :count(icount), max(imax) {}

    MyInfo operator=(const MyInfo &m) {
      count = m.count;
      max = m.max;
      return m;
    }

    MyInfo(const MyInfo &m) {
      count = m.count;
      max = m.max;
    }

    long count;
    ulong max;
  };

  typedef HASH_MAP<ulong,MyInfo> AddrInfoType;

  AddrInfoType addrInfo;

public:
  GDebugCC(const char *desc);
  ~GDebugCC();

  void reportValue() const;

  void inc(ulong addr);
  void dec(ulong addr);
};

#endif
