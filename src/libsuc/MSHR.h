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

#ifndef MSHR_H
#define MSHR_H

#include <queue>

#include "MemRequest.h"
#include "callback.h"
#include "nanassert.h"
#include "GStats.h"
#include "pool.h"

// This is an extremly fast MSHR implementation.  Given a number of
// outstanding MSHR entries, it does a hash function to find a random
// entry (always the same for a given address).
//
// This can have a little more contention than a fully associative
// structure because it can serialize requests, but it should not
// affect performance much.

//
// base MSHR class
//

template<class Addr_t>
class MSHR {
 private:
 protected:
  const int MaxEntries;
  int nFreeEntries; 

  const size_t Log2LineSize;

  GStatsCntr nUse;
  GStatsCntr nStallFull;
  GStatsMax  maxUsedEntries;

 public:
  virtual ~MSHR() { }
  MSHR(const char *name, int size, int lineSize);

  static MSHR<Addr_t> *create(const char *name, const char *type, int size, int lineSize);

  void destroy() {
    delete this;
  }

  virtual bool canIssue(Addr_t paddr) const = 0;

  virtual bool canIssue() const = 0;

  virtual bool issue(Addr_t paddr, MemOperation mo = MemRead) = 0;

  virtual void addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc = 0) = 0;

  virtual void retire(Addr_t paddr) = 0;
};

//
// MSHR that lets everything go
//
template<class Addr_t>
class NoMSHR : public MSHR<Addr_t> {

 protected:
  friend class MSHR<Addr_t>;
  NoMSHR(const char *name, int size, int lineSize);

 public:
  virtual ~NoMSHR() { }
  bool canIssue(Addr_t paddr) const { return true; }
  bool canIssue()             const { return true; }

  bool issue(Addr_t paddr, MemOperation mo = MemRead) { nUse.inc(); return true; }

  void addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc = 0) { I(0); }

  void retire(Addr_t paddr) { }
};

//
// MSHR that just queues the reqs, does NOT enforce address dependencies
//
template<class Addr_t>
class NoDepsMSHR : public MSHR<Addr_t> {
 private:  
  class OverflowField {
  public:
    Addr_t paddr;
    CallbackBase *cb;
    CallbackBase *ovflwcb;
  };



  typedef std::deque<OverflowField> Overflow;
  Overflow overflow;
 protected:
  friend class MSHR<Addr_t>;
  NoDepsMSHR(const char *name, int size, int lineSize);
  
 public:
  virtual ~NoDepsMSHR() { }

  bool canIssue(Addr_t paddr) const { return canIssue(); }

  bool canIssue() const { return (nFreeEntries > 0); }
  
  bool issue(Addr_t paddr, MemOperation mo = MemRead); 

  void addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc = 0);

  void retire(Addr_t paddr);


};

//
// regular full MSHR, address deps enforcement and overflowing capabilities
//

template<class Addr_t>
class FullMSHR : public MSHR<Addr_t> {
 private:
  GStatsCntr nStallConflict;

  const int    MSHRSize;
  const int    MSHRMask;

  bool overflowing;

  // If a non-integer type is defined, the MSHR template should accept
  // a hash function as a parameter

  // Running crafty, I verified that the current hash function
  // performs pretty well (due to the extra space on MSHRSize). It
  // performs only 5% worse than an oversize prime number (noise).
  int calcEntry(Addr_t paddr) const {
    ulong p = paddr >> Log2LineSize;
    return (p ^ (p>>11)) & MSHRMask;
  }

  class EntryType {
  public:
    CallbackContainer cc;
    bool inUse;
  };

  EntryType *entry;

  class OverflowField {
  public:
    Addr_t paddr;
    CallbackBase *cb;
    CallbackBase *ovflwcb;
  };

  typedef std::deque<OverflowField> Overflow;
  Overflow overflow;

 protected:
  friend class MSHR<Addr_t>;
  FullMSHR(const char *name, int size, int lineSize);

 public:
  virtual ~FullMSHR() { delete entry; }

  bool canIssue(Addr_t paddr) const { 
    return (!entry[calcEntry(paddr)].inUse); 
  }

  bool canIssue() const             { return (nFreeEntries>0); }

  bool issue(Addr_t paddr, MemOperation mo = MemRead);

  void addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc = 0);

  void retire(Addr_t paddr);

};


#ifndef MSHR_CPP
#include "MSHR.cpp"
#endif

#endif // MSHR_H
