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
				   const char *type, int size, int lineSize, int nse)
{
  MSHR *mshr;

  if(strcmp(type, "none") == 0) {
    mshr = new NoMSHR<Addr_t>(name, size, lineSize);
  } else if(strcmp(type, "nodeps") == 0) {
    mshr = new NoDepsMSHR<Addr_t>(name, size, lineSize);
  } else if(strcmp(type, "full") == 0) {
    mshr = new FullMSHR<Addr_t>(name, size, lineSize);
  } else if(strcmp(type, "single") == 0) {
    mshr = new SingleMSHR<Addr_t>(name, size, lineSize, nse);
  } else {
    MSG("WARNING:MSHR: type \"%s\" unknown, defaulting to \"none\"", type);
    mshr = new NoMSHR<Addr_t>(name, size, lineSize);
  }
  
  return mshr;
}

template<class Addr_t>
MSHR<Addr_t> *MSHR<Addr_t>::create(const char *name, const char *section)
{
  MSHR *mshr;
  int nse = 16;
  int nse2 = 16;

  const char *type = SescConf->getCharPtr(section, "MSHRtype");
  int size = SescConf->getLong(section, "MSHRsize");
  int lineSize = SescConf->getLong(section, "bsize");

  if(SescConf->checkLong(section, "MSHRrpl")) 
    nse = SescConf->getLong(section, "MSHRrpl");

  if(SescConf->checkLong(section, "MSHRrpl2")) 
    nse2 = SescConf->getLong(section, "MSHRrpl2");

  if(strcmp(type, "banked") == 0) {
    int nb = SescConf->getLong(section, "MSHRbanks");
    mshr = new BankedMSHR<Addr_t>(name, size, lineSize, nb, nse);
  } else {
    mshr = MSHR<PAddr>::create(name, type, size, lineSize, nse);
  }

  return mshr;
}

template<class Addr_t>
MSHR<Addr_t>::MSHR(const char *name, int size, int lineSize)
  :nEntries(size)
  ,Log2LineSize(log2i(lineSize))
  ,nUse("%s_MSHR:nUse", name)
  ,nUseReads("%s_MSHR:nUseReads", name)
  ,nUseWrites("%s_MSHR:nUseWrites", name)
  ,nOverflows("%s_MSHR:nOverflows", name)
  ,maxUsedEntries("%s_MSHR_maxUsedEntries", name)
  ,nCanAccept("%s_MSHR:nCanAccept", name)
  ,nCanNotAccept("%s_MSHR:nCanNotAccept", name)
  ,occupancyHistogram("%s_MSHR:occupancy",name)
  ,pendingReqsTimingHist("%s_MSHR:PendingReqsTimeHist",name)
  ,entriesOnReqHist("%s_MSHR:PendingReqsHist",name)
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
void NoDepsMSHR<Addr_t>::addEntry(Addr_t paddr, CallbackBase *c, 
				  CallbackBase *ovflwc, MemOperation mo)
{
  // by definition, calling addEntry in the NoDepsMSHR is overflowing
  OverflowField f;
  f.paddr   = paddr;
  f.cb      = c;
  f.ovflwcb = ovflwc;
  f.mo      = mo;
  overflow.push_back(f);

  if(nFreeEntries <= 0)
    nOverflows.inc();

  return;
}

template<class Addr_t>
void NoDepsMSHR<Addr_t>::retire(Addr_t paddr)
{
  maxUsedEntries.sample(nEntries - nFreeEntries);

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
void FullMSHR<Addr_t>::addEntry(Addr_t paddr, CallbackBase *c, CallbackBase *ovflwc, MemOperation mo)
{
  I(nFreeEntries>=0);
  I(nFreeEntries <= nEntries);

  if (overflowing) {
    OverflowField f;
    f.paddr = paddr;
    f.cb    = c;
    f.ovflwcb = ovflwc;
    overflow.push_back(f);

    nOverflows.inc();
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
  maxUsedEntries.sample((nEntries - nFreeEntries) + overflow.size());
  
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
  I(nFreeEntries <= nEntries);
  GMSG(nFreeEntries > nEntries, "free[%d] max[%d] overflow[%d]",nFreeEntries, nEntries, overflow.size());

  int pos = calcEntry(paddr);

  I(entry[pos].inUse);

  if (!entry[pos].cc.empty()) {
    entry[pos].cc.callNext();
    return;
  }

  entry[pos].inUse = false;
}


// SingleMSHR

template<class Addr_t> Cache* SingleMSHR<Addr_t>::L2Cache = NULL;

template<class Addr_t>
SingleMSHR<Addr_t>::SingleMSHR(const char *name, int size, int lineSize, int nse)
  : MSHR<Addr_t>(name, size, lineSize),
    nSubEntries(nse),
    bf(4, 8, 256, 6, 64, 6, 64, 6, 64),
    avgOverflowConsumptions("%s_MSHR_avgOverflowConsumptions", name),
    maxOutsReqs("%s_MSHR_maxOutsReqs", name),
    avgReqsPerLine("%s_MSHR_avgReqsPerLine", name),
    nIssuesNewEntry("%s_MSHR:nIssuesNewEntry", name),
    nL2HitsNewEntry("%s_MSHR:nL2HitsNewEntry", name),
    subEntriesHist("%s_MSHR:usedSubEntries", name),
    nCanNotAcceptSubEntryFull("%s_MSHR:nCanNotAcceptSubEntryFull", name)

{
  nFreeEntries = nEntries;
  checkingOverflow = false;
  nOutsReqs = 0;
}

template<class Addr_t>
bool SingleMSHR<Addr_t>::issue(Addr_t paddr, MemOperation mo)
{
  MSHRit it = ms.find(calcLineAddr(paddr));

  nUse.inc();
  if(mo == MemRead)
    nUseReads.inc();
  
  if(mo == MemWrite)
    nUseWrites.inc();

  if(!overflow.empty()) {
    return false;
  }

  I(nFreeEntries >= 0 && nFreeEntries <=nEntries);

  if(it == ms.end()) {
    if(nFreeEntries > 0) {
      entriesOnReqHist.sample(nEntries-nFreeEntries,1);
      ms[calcLineAddr(paddr)].firstRequest(calcLineAddr(paddr), nSubEntries);
      bf.insert(calcLineAddr(paddr));
      nFreeEntries--;
      updateOccHistogram();
      nOutsReqs++;
      updatePendingReqsHistogram(nOutsReqs);
      nIssuesNewEntry.inc();
      updateL2HitStat(paddr);
      return true;
    } 
  }

  return false;
}

template<class Addr_t>
void SingleMSHR<Addr_t>::toOverflow(Addr_t paddr, CallbackBase *c, 
				    CallbackBase *ovflwc, MemOperation mo)
{
  OverflowField f;
  f.paddr = paddr;
  f.cb    = c;
  f.ovflwcb = ovflwc;
  f.mo      = mo;

  overflow.push_back(f);
  nOverflows.inc();
}

template<class Addr_t>
void SingleMSHR<Addr_t>::checkOverflow()
{
  if(overflow.empty()) //nothing to do
    return;
  
  if(checkingOverflow) // i am already checking the overflow
    return;

  I(!overflow.empty());
  I(!checkingOverflow);

  checkingOverflow = true;

  int nConsumed = 0;

  do {
    OverflowField f = overflow.front();
    MSHRit it = ms.find(calcLineAddr(f.paddr));
    
    if(it == ms.end()) {
      if(nFreeEntries > 0) {
	entriesOnReqHist.sample(nEntries-nFreeEntries,1);
	ms[calcLineAddr(f.paddr)].firstRequest(calcLineAddr(f.paddr), nSubEntries);
	bf.insert(calcLineAddr(f.paddr));
	nFreeEntries--;
	updateOccHistogram();
	nConsumed++;
	nOutsReqs++;
	updatePendingReqsHistogram(nOutsReqs);
	f.ovflwcb->call();
	f.cb->destroy();
	overflow.pop_front();
	nIssuesNewEntry.inc();
	updateL2HitStat(f.paddr);
      } else {
	break;
      }
    } else { // just try to add the entry
      if((*it).second.addRequest(f.paddr, f.cb, f.mo)) {
	// succesfully accepted entry, but no need to call the callback
	// since there was already an entry pending for the same line
	f.ovflwcb->destroy();
	overflow.pop_front();
	nOutsReqs++;
	updatePendingReqsHistogram(nOutsReqs);
      } else {
	break;
      }
    }
  } while(!overflow.empty());

  if(nConsumed)
    avgOverflowConsumptions.sample(nConsumed);

  checkingOverflow = false;
}

template<class Addr_t>
void SingleMSHR<Addr_t>::addEntry(Addr_t paddr, CallbackBase *c, 
				  CallbackBase *ovflwc, MemOperation mo)
{
  MSHRit it = ms.find(calcLineAddr(paddr));
  I(ovflwc); // for single MSHR, overflow handler REQUIRED!

  if(!overflow.empty()) {
    toOverflow(paddr, c, ovflwc, mo);
    return;
  }

  if(it == ms.end())  {// we must be overflowing because the issue did not happen
    toOverflow(paddr, c, ovflwc, mo);
    return;
  }

  I(it != ms.end());

  if((*it).second.addRequest(paddr, c, mo)) {
    // ok, the addrequest succeeded, the request was added
    nOutsReqs++;
    updatePendingReqsHistogram(nOutsReqs);
    ovflwc->destroy(); // there was no overflow, so the callback needs to be destroyed
    return;
  } else {
    // too many oustanding requests to the same line already. send to overflow
    toOverflow(paddr, c, ovflwc, mo);
    return;
  }
}

template<class Addr_t>
void SingleMSHR<Addr_t>::retire(Addr_t paddr)
{
  MSHRit it = ms.find(calcLineAddr(paddr));
  I(it != ms.end());
  I(calcLineAddr(paddr) == (*it).second.getLineAddr());

  maxOutsReqs.sample(nOutsReqs);
  nOutsReqs--;
  updatePendingReqsHistogram(nOutsReqs);
  if((*it).second.retire()) {
    // the last pending request for the MSHRentry was completed
    // recycle the entry
    avgReqsPerLine.sample((*it).second.getMaxUsedSubEntries());
    subEntriesHist.sample((*it).second.getMaxUsedSubEntries(), 1);
    maxUsedEntries.sample(nEntries - nFreeEntries);

    nFreeEntries++;
    updateOccHistogram();
    bf.remove((*it).second.getLineAddr());
    ms.erase(it);
  }

  checkOverflow();
}

template<class Addr_t>
bool SingleMSHR<Addr_t>::canAcceptRequest(Addr_t paddr)
{
  if(!overflow.empty()) {
    nCanNotAccept.inc();
    return false;
  }

  const_MSHRit it = ms.find(calcLineAddr(paddr));
  I(nFreeEntries >= 0 && nFreeEntries <= nEntries);

  if(it == ms.end()) {
    if(nFreeEntries <= 0) {
      nCanNotAccept.inc();
      return false;
    }
    nCanAccept.inc();
    return true;
  }

  I(it != ms.end());
  
  bool canAccept = (*it).second.canAcceptRequest();
  if(canAccept)
    nCanAccept.inc();
  else {
    nCanNotAccept.inc();
    nCanNotAcceptSubEntryFull.inc();
  }

  return canAccept;
}

template<class Addr_t>
void SingleMSHR<Addr_t>::dropEntry(Addr_t paddr)
{
  MSHRit it = ms.find(paddr);
  I(it != ms.end());
  (*it).second.displace();
  
  nOutsReqs -= (*it).second.getUsedSubEntries();
  ms.erase(it);
  bf.remove(paddr);
  updatePendingReqsHistogram(nOutsReqs);

  nFreeEntries++;
  updateOccHistogram();
}

template<class Addr_t>
void SingleMSHR<Addr_t>::putEntry(MSHRentry<Addr_t> &me)
{
  I(ms.find(me.getLineAddr()) == ms.end());
  
  I(nFreeEntries > 0);

  ms[me.getLineAddr()] = me;
  bf.insert(me.getLineAddr());
  nOutsReqs += me.getUsedSubEntries();
  updatePendingReqsHistogram(nOutsReqs);
  updateL2HitStat(me.getLineAddr() << Log2LineSize );

  entriesOnReqHist.sample(nEntries-nFreeEntries,1);
  nFreeEntries--;
  updateOccHistogram();

  I(nFreeEntries >=0);
}

//
// BankedMSHR
//

template<class Addr_t>
BankedMSHR<Addr_t>::BankedMSHR(const char *name, int size, int lineSize, int nb, int nse)
  : MSHR<Addr_t>(name, size, lineSize),
    nBanks(nb),
    maxOutsReqs("%s_MSHR_maxOutsReqs", name),
    avgOverflowConsumptions("%s_MSHR_avgOverflowConsumptions", name)
{
  mshrBank = (SingleMSHR<Addr_t> **) malloc(sizeof(SingleMSHR<Addr_t> *) * nBanks);
  for(int i = 0; i < nBanks; i++) {
    char mName[512];
    sprintf(mName, "%s_B%d", name, i);
    mshrBank[i] = new SingleMSHR<Addr_t>(mName, size, lineSize, nse);
  }

  nOutsReqs = 0;
  checkingOverflow = false;
}

template<class Addr_t>
bool BankedMSHR<Addr_t>::canAcceptRequest(Addr_t paddr)
{
  if(!overflow.empty()) {
    nCanNotAccept.inc();
    return false;
  }

  bool canAccept = mshrBank[calcBankIndex(paddr)]->canAcceptRequest(paddr);
  if(canAccept)
    nCanAccept.inc();
  else
    nCanNotAccept.inc();

  return canAccept;
}

template<class Addr_t>
bool BankedMSHR<Addr_t>::issue(Addr_t paddr, MemOperation mo) 
{
  nUse.inc();

  if(mo == MemRead)
    nUseReads.inc();
  
  if(mo == MemWrite)
    nUseWrites.inc();
  
  if(!overflow.empty())
    return false;

  bool issued = mshrBank[calcBankIndex(paddr)]->issue(paddr, mo);

  if(issued) {
    nOutsReqs++;
    updatePendingReqsHistogram(nOutsReqs);
  }

  return issued;
}

template<class Addr_t>
void BankedMSHR<Addr_t>::addEntry(Addr_t paddr, CallbackBase *c, 
				  CallbackBase *ovflwc, MemOperation mo)
{
  if(!overflow.empty()) {
    toOverflow(paddr, c, ovflwc, mo);
    return;
  }

  if(mshrBank[calcBankIndex(paddr)]->canAcceptRequest(paddr)) {
    nOutsReqs++;
    updatePendingReqsHistogram(nOutsReqs);
    mshrBank[calcBankIndex(paddr)]->addEntry(paddr, c, ovflwc, mo);
    return;
  }

  toOverflow(paddr, c, ovflwc, mo);
}

template<class Addr_t>
void BankedMSHR<Addr_t>::retire(Addr_t paddr)
{
  maxOutsReqs.sample(nOutsReqs);
  mshrBank[calcBankIndex(paddr)]->retire(paddr);
  nOutsReqs--;
  updatePendingReqsHistogram(nOutsReqs);
  checkOverflow();
}

template<class Addr_t>
void BankedMSHR<Addr_t>::toOverflow(Addr_t paddr, CallbackBase *c, 
				    CallbackBase *ovflwc, MemOperation mo)
{
  OverflowField f;
  f.paddr = paddr;
  f.cb    = c;
  f.ovflwcb = ovflwc;
  f.mo      = mo;

  overflow.push_back(f);
}

template<class Addr_t>
void BankedMSHR<Addr_t>::checkOverflow()
{
  if(overflow.empty()) //nothing to do
    return;

  if(checkingOverflow) // i am already checking the overflow
    return;

  I(!overflow.empty());
  I(!checkingOverflow);

  checkingOverflow = true;

  int nConsumed = 0;

  do {
    OverflowField f = overflow.front();
    SingleMSHR<Addr_t> *mb = mshrBank[calcBankIndex(f.paddr)];

    if(!mb->canAcceptRequest(f.paddr))
      break;

    overflow.pop_front();
    nOutsReqs++;
    updatePendingReqsHistogram(nOutsReqs);
    nConsumed++;

    if(mb->issue(f.paddr, f.mo)) {
      f.ovflwcb->call();
      f.cb->destroy();
      continue;
    }

    mb->addEntry(f.paddr, f.cb, f.ovflwcb, f.mo);

  } while(!overflow.empty());

  if(nConsumed)
    avgOverflowConsumptions.sample(nConsumed);

  checkingOverflow = false;
}


//
// MSHRentry stuff
//

template<class Addr_t>
bool MSHRentry<Addr_t>::addRequest(Addr_t reqAddr, CallbackBase *rcb, MemOperation mo) 
{
  I(nFreeSubEntries >= 0);
  I(nFreeSubEntries <= nSubEntries);

  if(nFreeSubEntries == 0) 
    return false;

  nFreeSubEntries--;

  maxUsedSubEntries = maxUsedSubEntries < (nSubEntries - nFreeSubEntries) ?
    (nSubEntries - nFreeSubEntries) : maxUsedSubEntries;

  cc.add(rcb);

  return true;
}

template<class Addr_t>
bool MSHRentry<Addr_t>::retire()
{
  nFreeSubEntries++;

  if(!cc.empty()) {
    cc.callNext();
    return false;
  }

  // if we are here, we have to be the last pending entry in the pending
  // MSHR
  I(nFreeSubEntries == nSubEntries);

  return true;
}

