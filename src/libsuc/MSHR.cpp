/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

   Contributed by Jose Renau
                  Luis Ceze

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.21

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define MSHR_CPP

#include "Snippets.h"
#include "MSHR.h"


// TODO: Add energy model

// 
// basic MSHR class
//

template<class Addr_t>
MSHR<Addr_t> *MSHR<Addr_t>::create(const char *name, 
				   const char *type, int size, int lineSize)
{
  MSHR *mshr;

  if(strcmp(type, "none") == 0) {
    mshr = new NoMSHR<Addr_t>(name, size, lineSize);
  } else if(strcmp(type, "nodeps") == 0) {
    mshr = new NoDepsMSHR<Addr_t>(name, size, lineSize);
  } else if(strcmp(type, "full") == 0) {
    mshr = new FullMSHR<Addr_t>(name, size, lineSize);
  } else {
    MSG("WARNING:MSHR: type \"%s\" unknown, defaulting to \"none\"", type);
    mshr = new NoMSHR<Addr_t>(name, size, lineSize);
  }
  
  return mshr;
  
}

template<class Addr_t>
MSHR<Addr_t>::MSHR(const char *name, int size, int lineSize)
  :MaxEntries(size)
  ,Log2LineSize(log2i(lineSize))
  ,nUse("%s_MSHR:nUse", name)
  ,nStallFull("%s_MSHR:nStallFull", name)
  ,maxUsedEntries("%s_MSHR:maxUsedEntries", name)
{
  I(size>0 && size<1024*32); 

  nFreeEntries = size;
}

// 
// NoMSHR class
//

template<class Addr_t>
NoMSHR<Addr_t>::NoMSHR(const char *name, int size, int lineSize)
  : MSHR<Addr_t>(name, size, lineSize)
{
  //nothing to do  
}


// 
// NoDepsMSHR class
//

template<class Addr_t>
NoDepsMSHR<Addr_t>::NoDepsMSHR(const char *name, int size, int lineSize)
  : MSHR<Addr_t>(name, size, lineSize)
{
  //nothing to do

}

template<class Addr_t>
bool NoDepsMSHR<Addr_t>::issue(Addr_t paddr, MemOperation mo)
{
  nUse.inc();

  nFreeEntries--;

  if(!overflow.empty() || nFreeEntries < 0) {
    return false;
  }


  return true;
}


template<class Addr_t>
void NoDepsMSHR<Addr_t>::addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc)
{
  // by definition, calling addEntry in the NoDepsMSHR is overflowing
  OverflowField f;
  f.paddr = paddr;
  f.cb      = c;
  f.ovflwcb = ovflwc;
  overflow.push_back(f);

  if(nFreeEntries <= 0)
    nStallFull.inc();

  return;
}

template<class Addr_t>
void NoDepsMSHR<Addr_t>::retire(Addr_t paddr)
{
  maxUsedEntries.sample(MaxEntries - nFreeEntries);

  nFreeEntries++;


  if(!overflow.empty()) {
    OverflowField f = overflow.front();
    overflow.pop_front();

    if(f.ovflwcb) {
      f.ovflwcb->call();
      f.cb->destroy(); // the accessQueuedCallback will bever be called.
    } else
      f.cb->call();  // temporary until vmem uses the ovflw callback FIXME
  } 
}

// 
// FullMSHR class
//

template<class Addr_t>
FullMSHR<Addr_t>::FullMSHR<Addr_t>(const char *name, int size, int lineSize)
  : MSHR<Addr_t>(name, size, lineSize)
  ,nStallConflict("%s_MSHR:nStallConflict", name)
  ,MSHRSize(roundUpPower2(size)*4)
  ,MSHRMask(MSHRSize-1)
    
{
  I(lineSize>=0 && Log2LineSize<(8*sizeof(Addr_t)-1));
  overflowing  = false;
  
  entry = new EntryType[MSHRSize];

  for(int i=0;i<MSHRSize;i++) {
    entry[i].inUse = false;
    I(entry[i].cc.empty());
  }
}

template<class Addr_t>
bool FullMSHR<Addr_t>::issue(Addr_t paddr, MemOperation mo)
{
  nUse.inc();

  if (overflowing || nFreeEntries == 0) {
    overflowing = true;
    return false;
  }

  nFreeEntries--;
  I(nFreeEntries>=0);

  int pos = calcEntry(paddr);
  if (entry[pos].inUse)
    return false;

  entry[pos].inUse = true;
  I(entry[pos].cc.empty());

  return true;
}

template<class Addr_t>
void FullMSHR<Addr_t>::addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc)
{
  I(nFreeEntries>=0);
  I(nFreeEntries <= MaxEntries);

  if (overflowing) {
    OverflowField f;
    f.paddr = paddr;
    f.cb    = c;
    f.ovflwcb = ovflwc;
    overflow.push_back(f);

    nStallFull.inc();
    return;
  }

  if(ovflwc)
    ovflwc->destroy();

  nStallConflict.inc();

  int pos = calcEntry(paddr);

  I(entry[pos].inUse);

  entry[pos].cc.add(c);
}

template<class Addr_t>
void FullMSHR<Addr_t>::retire(Addr_t paddr)
{
  maxUsedEntries.sample((MaxEntries - nFreeEntries) + overflow.size());
  
  if (overflowing) {
    I(!overflow.empty());
    OverflowField f = overflow.front();
    overflow.pop_front();
    overflowing = !overflow.empty();

    int opos = calcEntry(f.paddr);
    if (entry[opos].inUse) {
      entry[opos].cc.add(f.cb);
      // we did not need the overflow callback here, since there was a
      // pending line already. but we need to destroy the callback to
      // avoid leaks.
      if(f.ovflwcb)
	f.ovflwcb->destroy();
    }else{
      entry[opos].inUse = true;
      if(f.ovflwcb) {
	f.ovflwcb->call();
	f.cb->destroy(); // the retire callback will never be called
      } else 
	f.cb->call(); // temporary until vmem uses the ovflw callback FIXME
    }
  } else
    nFreeEntries++;

  I(nFreeEntries>=0);
  I(nFreeEntries <= MaxEntries);
  GMSG(nFreeEntries > MaxEntries, "free[%d] max[%d] overflow[%d]",nFreeEntries, MaxEntries, overflow.size());

  int pos = calcEntry(paddr);

  I(entry[pos].inUse);

  if (!entry[pos].cc.empty()) {
    entry[pos].cc.callNext();
    return;
  }

  entry[pos].inUse = false;
}
