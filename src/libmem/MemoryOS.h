/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Basilio Fraguela
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
#ifndef MEMORYOS_H
#define MEMORYOS_H

#include "nanassert.h"

#include <queue>
#include <iostream>

#include "estl.h"
#include "GStats.h"
#include "GProcessor.h"
#include "MemRequest.h"
#include "MemObj.h"
#include "MemorySystem.h"

#include "TLB.h"

typedef HASH_MAP<unsigned long, unsigned long> PageMapper;

class MemoryOS : public GMemoryOS {
public:
  static ulong numPhysicalPages;
protected:
  TLB DTLB;
  TLB ITLB;
  GMemorySystem *memorySystem;

  GStatsCntr missesDTLB;
  GStatsCntr missesITLB;

  GStatsCntr TLBTime;
  Time_t     startTime;

  TimeDelta_t period;
  bool busy;

  void fillDTLB(long vAddr, long phPage) {
    DTLB.insert(vAddr, GMemorySystem::calcFullPage(phPage));
  }

  void fillITLB(long vAddr, long phPage){
    ITLB.insert(vAddr, GMemorySystem::calcFullPage(phPage));
  }

  void launchReq(MemRequest *mreq, long phaddr) const {
     mreq->setPAddr(phaddr);
     mreq->getCurrentMemObj()->access(mreq);
  }

  void completeReq(MemRequest *mreq, long vAddr, long phPage) {
    GLOG(DEBUGCONDITION,"[C %llu] %li. %lu -> %lu", globalClock, mreq->getVaddr(), GMemorySystem::calcPage(vAddr), phPage);
    if(mreq->isDataReq()) 
      fillDTLB(vAddr, phPage);
    else 
      fillITLB(vAddr, phPage);

    // address translation is turned off
    // launchReq(mreq, GMemorySystem::calcPAddr(GMemorySystem::calcFullPage(phPage), mreq->getVaddr()));
    launchReq(mreq,vAddr);
  }

public:
  MemoryOS(GMemorySystem *ms, const char *section);

  virtual ~MemoryOS() { }

  long TLBTranslate(long unsigned vAddr);
  long ITLBTranslate(long unsigned vAddr);

};

class PageTable {
public:
  typedef struct {
    Time_t lastTimeToTLB;
    unsigned long virtualPage;
    short int status;
  } IntlPTEntry;

protected:

  enum {
    ValidPageStatus  = 1,
    SystemPageStatus = 2,
    PinnedPageStatus = 4,
    MigrPageStatus   = 8
  };

  typedef struct {
    unsigned long physicalPage;
    short int status;
  } L1IntlPTEntry;

  PageMapper vToPMap;
  IntlPTEntry   *invertedPT;
  L1IntlPTEntry *L1PT;

  ulong maskEntriesPage;
  ulong log2EntriesPage;

  GStatsCntr numOSPages;
  GStatsCntr numUsrPages;

/* We will assume 32 bit page table entries */
  static const unsigned short int BytesPerEntry = 4;

  IntlPTEntry *getReplCandidate(unsigned short int first_bank, unsigned short int search_step);

public:
  PageTable();
  ~PageTable();

  IntlPTEntry * getReplPhPageL2Entry() {
    return getReplCandidate(0, 1);
  }

  unsigned long getL1EntryNum(const unsigned long vPage) const {
    return (vPage >> log2EntriesPage);
  }

  void assignL1Entry(unsigned long vPage, unsigned long pPage);
  void assignL2Entry(unsigned long vPage, unsigned long pPage);
  void evictL2Entry(IntlPTEntry *p);

  unsigned long L2PTEntryToPhPage(const IntlPTEntry *p) const {
    return p - invertedPT;
  }

  unsigned long getL1PTEntryPhysicalAddress(const unsigned long vPage) const {
    return getL1EntryNum(vPage) * BytesPerEntry + 0xFFFF;
  }

  long getL2PTEntryPhysicalAddress(const unsigned long vPage) const {
    L1IntlPTEntry *p = L1PT + getL1EntryNum(vPage);
    if (!(p->status & ValidPageStatus) )
      return -1;
    else 
      return (long)((p->physicalPage) | ((vPage & maskEntriesPage) * BytesPerEntry)) + 0xFFFF;
  }

  void stampPhPage(const unsigned long pPage) {
    // This should really imply a write op, but...
    invertedPT[pPage].lastTimeToTLB = globalClock;
  }

  long translate(const unsigned long vPage) {
    PageMapper::const_iterator it = vToPMap.find(vPage);
    if (it == vToPMap.end())
      return -1;
    else {
      stampPhPage((*it).second);
      return (long)(*it).second;
    }
  }
};

class StdMemoryOS : public MemoryOS {
 private:
  PageTable::IntlPTEntry *getReplPTEntry();

  void accessL1PT(MemRequest *origReq, PAddr paddr);
  void accessL2PT(MemRequest *origReq, PAddr paddr);

  StaticCallbackMember2<StdMemoryOS, MemRequest *, PAddr, &StdMemoryOS::accessL1PT> accessL1PTCB;
  StaticCallbackMember2<StdMemoryOS, MemRequest *, PAddr, &StdMemoryOS::accessL2PT> accessL2PTCB;

  StaticCBMemRequest accessL1PTReq;
  StaticCBMemRequest accessL2PTReq;

protected:
  static PageTable *PT; // Shared by all the MemoryOS

  std::vector<MemRequest *> pendingReqs;
  ulong     nextPhysicalPage; //Marker to find new free physical pages
  MemObj   *cacheObj;
  TimeDelta_t minTLBMissDelay;

  unsigned long int getFreePhysicalPage();
  void serviceRequest(MemRequest*);
  void attemptToEmptyQueue(ulong vaddr, ulong phPage);

public:
  StdMemoryOS(GMemorySystem *ms, const char *descr_section);
  ~StdMemoryOS();

  /*long translate(long vAddr);*/

  void solveRequest(MemRequest *r);
  void boot();
  void report(const char *str) {};
};


#endif
