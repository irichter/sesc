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
#include "GDebugCC.h"
#include "ReportGen.h"

GDebugCC::GDebugCC(const char *desc)
{
  my_desc = desc;
  subscribe();
}

GDebugCC::~GDebugCC()
{
  unsubscribe();
}

void GDebugCC::reportValue() const 
{
  AddrInfoType::const_iterator it = addrInfo.begin();
  while( it != addrInfo.end() ) {
    Report::field("%s=addr[%lu]count[%d]max[%d]",my_desc,(*it).first,
		 (*it).second.count,(*it).second.max);
    it++;
  }
}

void GDebugCC::inc(ulong addr) 
{
  AddrInfoType::iterator it = addrInfo.find(addr); 
  if( it == addrInfo.end()){
    addrInfo[addr] = MyInfo();
  }
  MyInfo m = addrInfo[addr];
  m.count++;
  m.max++;
  addrInfo[addr]=m;
}

void GDebugCC::dec(ulong addr) 
{
  MyInfo m = addrInfo[addr];
  m.count--;
  addrInfo[addr]=m;

  GLOG(DEBUG2,"[C %llu] %s GDebugCC Addr:%lu is decremented",globalClock,my_desc,addr);
}
