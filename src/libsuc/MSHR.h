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

#include "BloomFilter.h"

// This is an extremly fast MSHR implementation.  Given a number of
// outstanding MSHR entries, it does a hash function to find a random
// entry (always the same for a given address).
//
// This can have a little more contention than a fully associative
// structure because it can serialize requests, but it should not
// affect performance much.


template<class Addr_t>
class OverflowFieldT {
 public:
  Addr_t paddr;
  CallbackBase *cb;
  CallbackBase *ovflwcb;
  MemOperation mo;
};

//
// base MSHR class
//

template<class Addr_t>
class MSHR {
 private:
 protected:
  const int nEntries;
  int nFreeEntries; 

  const size_t Log2LineSize;

  GStatsCntr nUse;
  GStatsCntr nUseReads;
  GStatsCntr nUseWrites;
  GStatsCntr nOverflows;
  GStatsMax  maxUsedEntries;
  GStatsCntr nCanAccept;
  GStatsCntr nCanNotAccept;
  GStatsTimingHist occupancyHistogram;
  GStatsTimingHist pendingReqsTimingHist;
  GStatsHist       entriesOnReqHist;      

 public:
  virtual ~MSHR() { }
  MSHR(const char *name, int size, int lineSize);

  static MSHR<Addr_t> *create(const char *name, const char *type, 
			      int size, int lineSize, int nse = 16);

  static MSHR<Addr_t> *create(const char *name, const char *section);

  void destroy() {
    delete this;
  }

  virtual bool canAcceptRequest(Addr_t paddr) = 0;

  virtual bool issue(Addr_t paddr, MemOperation mo = MemRead) = 0;

  virtual void addEntry(Addr_t paddr, CallbackBase *c, 
			CallbackBase *ovflwc = 0, MemOperation mo = MemRead) = 0;

  virtual void retire(Addr_t paddr) = 0;

  Addr_t calcLineAddr(Addr_t paddr) const { return paddr >> Log2LineSize; }

  void updateOccHistogram() 
  { 
    occupancyHistogram.sample(nEntries-nFreeEntries); 
  }
  void updatePendingReqsHistogram(int pendingReqs)
  {
    //pendingReqsTimingHist.sample(pendingReqs);
  }

};

//
// MSHR that lets everything go
//
template<class Addr_t>
class NoMSHR : public MSHR<Addr_t> {
  using MSHR<Addr_t>::nEntries;
  using MSHR<Addr_t>::nFreeEntries; 
  using MSHR<Addr_t>::Log2LineSize;
  using MSHR<Addr_t>::nUse;
  using MSHR<Addr_t>::nUseReads;
  using MSHR<Addr_t>::nUseWrites;
  using MSHR<Addr_t>::nOverflows;
  using MSHR<Addr_t>::maxUsedEntries;
  using MSHR<Addr_t>::nCanAccept;
  using MSHR<Addr_t>::nCanNotAccept;
  using MSHR<Addr_t>::occupancyHistogram;
  using MSHR<Addr_t>::pendingReqsTimingHist;
  using MSHR<Addr_t>::entriesOnReqHist;      
  using MSHR<Addr_t>::updateOccHistogram;      
  using MSHR<Addr_t>::updatePendingReqsHistogram;

 protected:
  friend class MSHR<Addr_t>;
  NoMSHR(const char *name, int size, int lineSize);

 public:
  virtual ~NoMSHR() { }
  bool canAcceptRequest(Addr_t paddr) { return true; }

  bool issue(Addr_t paddr, MemOperation mo = MemRead) { nUse.inc(); return true; }

  void addEntry(Addr_t paddr, CallbackBase *c, 
		CallbackBase *ovflwc = 0, MemOperation mo = MemRead) { I(0); }

  void retire(Addr_t paddr) { }
};

//
// MSHR that just queues the reqs, does NOT enforce address dependencies
//
template<class Addr_t>
class NoDepsMSHR : public MSHR<Addr_t> {
  using MSHR<Addr_t>::nEntries;
  using MSHR<Addr_t>::nFreeEntries; 
  using MSHR<Addr_t>::Log2LineSize;
  using MSHR<Addr_t>::nUse;
  using MSHR<Addr_t>::nUseReads;
  using MSHR<Addr_t>::nUseWrites;
  using MSHR<Addr_t>::nOverflows;
  using MSHR<Addr_t>::maxUsedEntries;
  using MSHR<Addr_t>::nCanAccept;
  using MSHR<Addr_t>::nCanNotAccept;
  using MSHR<Addr_t>::occupancyHistogram;
  using MSHR<Addr_t>::pendingReqsTimingHist;
  using MSHR<Addr_t>::entriesOnReqHist;      
  using MSHR<Addr_t>::updateOccHistogram;      
  using MSHR<Addr_t>::updatePendingReqsHistogram;

 private:  
  typedef OverflowFieldT<Addr_t> OverflowField;
  typedef std::deque<OverflowField> Overflow;
  Overflow overflow;
 protected:
  friend class MSHR<Addr_t>;
  NoDepsMSHR(const char *name, int size, int lineSize);
  
 public:
  virtual ~NoDepsMSHR() { }

  bool canAcceptRequest(Addr_t paddr) { return (nFreeEntries > 0); }

  bool issue(Addr_t paddr, MemOperation mo = MemRead); 

  void addEntry(Addr_t paddr, CallbackBase *c, 
		CallbackBase *ovflwc = 0, MemOperation mo = MemRead);

  void retire(Addr_t paddr);


};

//
// regular full MSHR, address deps enforcement and overflowing capabilities
//

template<class Addr_t>
class FullMSHR : public MSHR<Addr_t> {
  using MSHR<Addr_t>::nEntries;
  using MSHR<Addr_t>::nFreeEntries; 
  using MSHR<Addr_t>::Log2LineSize;
  using MSHR<Addr_t>::nUse;
  using MSHR<Addr_t>::nUseReads;
  using MSHR<Addr_t>::nUseWrites;
  using MSHR<Addr_t>::nOverflows;
  using MSHR<Addr_t>::maxUsedEntries;
  using MSHR<Addr_t>::nCanAccept;
  using MSHR<Addr_t>::nCanNotAccept;
  using MSHR<Addr_t>::occupancyHistogram;
  using MSHR<Addr_t>::pendingReqsTimingHist;
  using MSHR<Addr_t>::entriesOnReqHist;      
  using MSHR<Addr_t>::updateOccHistogram;      
  using MSHR<Addr_t>::updatePendingReqsHistogram;

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

  typedef OverflowFieldT<Addr_t> OverflowField;
  typedef std::deque<OverflowField> Overflow;
  Overflow overflow;

 protected:
  friend class MSHR<Addr_t>;
  FullMSHR(const char *name, int size, int lineSize);

 public:
  virtual ~FullMSHR() { delete entry; }

  bool canAcceptRequest(Addr_t paddr) { return (nFreeEntries>0); }

  bool issue(Addr_t paddr, MemOperation mo = MemRead);

  void addEntry(Addr_t paddr, CallbackBase *c, 
		CallbackBase *ovflwc = 0, MemOperation mo = MemRead);

  void retire(Addr_t paddr);

};

template<class Addr_t>
class MSHRentry {
 private:
  int nSubEntries;
  int nFreeSubEntries;
  int maxUsedSubEntries;
  CallbackContainer cc;
  Addr_t reqLineAddr;
  bool displaced;
  Time_t whenAllocated;
 public:
  MSHRentry() {
    reqLineAddr = 0;
    nFreeSubEntries = 0; 
    nSubEntries = 0;
    maxUsedSubEntries = 0;
    displaced = false;
    whenAllocated = globalClock;
  }

  ~MSHRentry() {
    if(displaced)
      cc.makeEmpty();

    GI(!displaced, nFreeSubEntries == nSubEntries);
  }

  PAddr getLineAddr() { return reqLineAddr; }

  bool addRequest(Addr_t reqAddr, CallbackBase *rcb, MemOperation mo);
  void firstRequest(Addr_t addr, int nse) { 
    I(nFreeSubEntries == 0);
    I(nSubEntries == 0);

    nSubEntries = nse;
    nFreeSubEntries = nSubEntries - 1;
    maxUsedSubEntries = 1;
    reqLineAddr = addr;
  }

  bool canAcceptRequest() const { return nFreeSubEntries > 0; }

  bool retire();
  int getMaxUsedSubEntries() { return maxUsedSubEntries; }
  int getUsedSubEntries()    { return (nSubEntries - nFreeSubEntries); }

  void adjustSubEntries(int nse) {
    nFreeSubEntries += (nse - nSubEntries);
    nSubEntries = nse;
  }
  
  void displace() { displaced = true; }
  Time_t getWhenAllocated() { return whenAllocated; }
};

template<class Addr_t> class HrMSHR;

//
// SingleMSHR
//

template<class Addr_t>
class SingleMSHR : public MSHR<Addr_t> {
  using MSHR<Addr_t>::nEntries;
  using MSHR<Addr_t>::nFreeEntries; 
  using MSHR<Addr_t>::Log2LineSize;
  using MSHR<Addr_t>::nUse;
  using MSHR<Addr_t>::nUseReads;
  using MSHR<Addr_t>::nUseWrites;
  using MSHR<Addr_t>::nOverflows;
  using MSHR<Addr_t>::maxUsedEntries;
  using MSHR<Addr_t>::nCanAccept;
  using MSHR<Addr_t>::nCanNotAccept;
  using MSHR<Addr_t>::occupancyHistogram;
  using MSHR<Addr_t>::pendingReqsTimingHist;
  using MSHR<Addr_t>::entriesOnReqHist;      
  using MSHR<Addr_t>::updateOccHistogram;      
  using MSHR<Addr_t>::updatePendingReqsHistogram;

 private:  
  const int nSubEntries;
  int nOutsReqs;
  BloomFilter bf;

  bool checkingOverflow;

  typedef OverflowFieldT<Addr_t> OverflowField;
  typedef std::deque<OverflowField> Overflow;
  Overflow overflow;

  typedef HASH_MAP<Addr_t, MSHRentry<Addr_t> > MSHRstruct;
  typedef typename MSHRstruct::iterator MSHRit;
  typedef typename MSHRstruct::const_iterator const_MSHRit;
  
  MSHRstruct ms;
  GStatsAvg avgOverflowConsumptions;
  GStatsMax maxOutsReqs;
  GStatsAvg avgReqsPerLine;
  GStatsCntr nIssuesNewEntry;
  GStatsCntr nL2HitsNewEntry;
  GStatsHist subEntriesHist;      
  GStatsCntr nCanNotAcceptSubEntryFull;

  // static Cache *L2Cache; TODO:FIXME, this does not work anymore

 protected:
  friend class MSHR<Addr_t>;
  friend class HrMSHR<Addr_t>;
  
  void toOverflow(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc, 
		  MemOperation mo);

  void checkOverflow();

  bool hasEntry(Addr_t paddr) { return (ms.find(calcLineAddr(paddr)) != ms.end()); }
  bool hasLineReq(Addr_t paddr)  { return (ms.find(paddr) != ms.end()); }

  void dropEntry(Addr_t paddr);
  void putEntry(MSHRentry<Addr_t> &me);

  void updateL2HitStat(Addr_t paddr) 
    {
      // this is broken in gcc-3.4.3.
      // we need to figure out a better way of doing this      
      //      if (L2Cache && L2Cache->isInCache(paddr))
      // nL2HitsNewEntry.inc();
    }

 public:
  SingleMSHR(const char *name, int size, int lineSize, int nse = 16);

  virtual ~SingleMSHR() { }

  bool canAcceptRequest(Addr_t paddr);

  bool issue(Addr_t paddr, MemOperation mo = MemRead); 

  void addEntry(Addr_t paddr, CallbackBase *c, 
		CallbackBase *ovflwc = 0, MemOperation mo = MemRead);

  void retire(Addr_t paddr);

  // we need to fix this. this is here just to make sesc compile with gcc-3.4.2
  // nobody is going to miss this code anyways.
  //static void setL2Cache(Cache *l2Cache) { L2Cache = l2Cache; }
  static void setL2Cache(void *l2Cache) {  }

  int getnSubEntries() { return nSubEntries; }

};

//
// BankedMSHR
//

template<class Addr_t>
class BankedMSHR : public MSHR<Addr_t> {
  using MSHR<Addr_t>::nEntries;
  using MSHR<Addr_t>::nFreeEntries; 
  using MSHR<Addr_t>::Log2LineSize;
  using MSHR<Addr_t>::nUse;
  using MSHR<Addr_t>::nUseReads;
  using MSHR<Addr_t>::nUseWrites;
  using MSHR<Addr_t>::nOverflows;
  using MSHR<Addr_t>::maxUsedEntries;
  using MSHR<Addr_t>::nCanAccept;
  using MSHR<Addr_t>::nCanNotAccept;
  using MSHR<Addr_t>::occupancyHistogram;
  using MSHR<Addr_t>::pendingReqsTimingHist;
  using MSHR<Addr_t>::entriesOnReqHist;      
  using MSHR<Addr_t>::updateOccHistogram;      
  using MSHR<Addr_t>::updatePendingReqsHistogram;

 private:  
  const int nBanks;
  SingleMSHR<Addr_t> **mshrBank;

  GStatsMax maxOutsReqs;
  GStatsAvg avgOverflowConsumptions;

  int nOutsReqs;
  bool checkingOverflow;
  
  int calcBankIndex(Addr_t paddr) const { 
    paddr = calcLineAddr(paddr);
    Addr_t idx = (paddr >> (16 - Log2LineSize/2)) ^ (paddr & 0x0000ffff);
    return  idx % nBanks;  // TODO:move to a mask
  }

  typedef OverflowFieldT<Addr_t> OverflowField;
  typedef std::deque<OverflowField> Overflow;
  Overflow overflow;

  void toOverflow(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc, 
		  MemOperation mo);

  void checkOverflow();
  
 protected:
  friend class MSHR<Addr_t>;
  BankedMSHR(const char *name, int size, int lineSize, int nb, int nse = 16);
  
 public:
  virtual ~BankedMSHR() { }

  bool canAcceptRequest(Addr_t paddr);

  bool issue(Addr_t paddr, MemOperation mo = MemRead); 

  void addEntry(Addr_t paddr, CallbackBase *c, 
		CallbackBase *ovflwc = 0, MemOperation mo = MemRead);

  void retire(Addr_t paddr);
};


#ifndef MSHR_CPP
#include "MSHR.cpp"
#endif

#endif // MSHR_H
