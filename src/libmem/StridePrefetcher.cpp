/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Luis Ceze

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

/* prefetching is the art of learning from you missStakes */

/* this prefecther is based on the ideas in Palacharla and Kessler[1994] */

#include "nanassert.h"
#include "SescConf.h"

#include "MemorySystem.h"
#include "StridePrefetcher.h"

static pool < std::queue<MemRequest *> > activeMemReqPool(32);

StridePrefetcher::StridePrefetcher(MemorySystem* current
	 ,const char *section
	 ,const char *name)
  : MemObj(section, name)
  ,halfMiss("%s:halfMiss", name)
  ,miss("%s:miss", name)
  ,hit("%s:hits", name)
  ,unitStrideStreams("%s:unitStrideStreams", name)
  ,nonUnitStrideStreams("%s:nonUnitStrideStreams", name)
  ,ignoredStreams("%s:ignoredStreams", name)
{
  MemObj *lower_level = NULL;

  SescConf->isLong(section, "depth");
  depth = SescConf->getLong(section, "depth");

  SescConf->isLong(section, "missWindow");
  missWindow = SescConf->getLong(section, "missWindow");

  SescConf->isLong(section, "maxStride");
  maxStride = SescConf->getLong(section, "maxStride");

  SescConf->isLong(section, "hitDelay");
  hitDelay = SescConf->getLong(section, "hitDelay");

  SescConf->isLong(section, "missDelay");
  missDelay = SescConf->getLong(section, "missDelay");

  SescConf->isLong(section, "learnHitDelay");
  learnHitDelay = SescConf->getLong(section, "learnHitDelay");

  SescConf->isLong(section, "learnMissDelay");
  learnMissDelay = SescConf->getLong(section, "learnMissDelay");

  I(depth > 0);

  const char *buffSection = SescConf->getCharPtr(section, "buffCache");
  if (buffSection) {
    buff = BuffType::create(buffSection, "", name);
    
    SescConf->isLong(buffSection, "numPorts");
    numBuffPorts = SescConf->getLong(buffSection, "numPorts");
    
    SescConf->isLong(buffSection, "portOccp");
    buffPortOccp = SescConf->getLong(buffSection, "portOccp");
  }

  const char *streamSection = SescConf->getCharPtr(section, "streamCache");
  if (streamSection) {
    char tableName[128];
    sprintf(tableName, "%sPrefTable", name);
    table = PfTable::create(streamSection, "", tableName);

    GMSG(pEntrySize != SescConf->getLong(streamSection, "BSize"),
	 "The prefetch buffer streamBSize field in the configuration file should be %d.", pEntrySize);

    SescConf->isLong(streamSection, "numPorts");
    numTablePorts = SescConf->getLong(streamSection, "numPorts");

    SescConf->isLong(streamSection, "portOccp");
    tablePortOccp = SescConf->getLong(streamSection, "portOccp");
  }

  char portName[128];
  sprintf(portName, "%s_buff", name);
  buffPort  = PortGeneric::create(portName, numBuffPorts, buffPortOccp);
  sprintf(portName, "%s_table", name);
  tablePort = PortGeneric::create(portName, numTablePorts, tablePortOccp);

  defaultMask  = ~(buff->getLineSize()-1);

  I(current);
  lower_level = current->declareMemoryObj(section, k_lowerLevel);   
  if (lower_level != NULL)
    addLowerLevel(lower_level);
}

void StridePrefetcher::read(MemRequest *mreq)
{
  ulong paddr = mreq->getPAddr() & defaultMask;
  bLine *l = buff->readLine(paddr);

  if(l) { //hit
    LOG("SP: hit on %08lx", paddr);
    hit.inc();
    mreq->goUpAbs(nextBuffSlot() + hitDelay); 
    learnHit(paddr);
    return;
  }

  penFetchSet::iterator it = pendingFetches.find(paddr);
  if(it != pendingFetches.end()) { // half-miss
    LOG("SP: half-miss on %08lx", paddr);
    halfMiss.inc();
    penReqMapper::iterator itR = pendingRequests.find(paddr);

    if (itR == pendingRequests.end()) {
      pendingRequests[paddr] = activeMemReqPool.out();
      itR = pendingRequests.find(paddr);
    }

    I(itR != pendingRequests.end());
    
    (*itR).second->push(mreq);
    learnHit(paddr); // half-miss is a hit from the learning point of view
    return;
  }

  LOG("SP:miss on %08lx", paddr);
  miss.inc();
  learnMiss(paddr);
  mreq->goDownAbs(nextBuffSlot() + missDelay, lowerLevel[0]); 
}

void StridePrefetcher::learnHit(PAddr addr)
{
  ulong paddr = addr & defaultMask;
  pEntry *pe = table->readLine(paddr);
  Time_t lat = nextTableSlot() - globalClock;

  if(pe == 0) // this hit in the buffer came from data 
    return;   // from a no longer active stream

  prefetch(pe, lat + learnHitDelay); 
  pe->setTag(table->calcTag(pe->nextAddr(table)));
  LOG("SP:prefetching more: addr=%08lx", paddr + pe->stride);
}

void StridePrefetcher::learnMiss(PAddr addr)
{
  ulong paddr = addr & defaultMask;
  Time_t lat = nextTableSlot() - globalClock;
  bool foundUnitStride = false;
  ulong newStride = 0;
  ulong minDelta = (ulong) -1;
  bool goingUp = true;

  if(lastMissesQ.empty()) {
    lastMissesQ.push_back(paddr);
    return;
  }

  // man, this is baad. i have to do a better search here
  std::deque<PAddr>::iterator it = lastMissesQ.begin();
  while(it != lastMissesQ.end()) {

    ulong delta;
    if(paddr < (*it)) {
      goingUp = false;
      delta = (*it) - paddr;
    } else {
      goingUp = true;
      delta = paddr - (*it);
    }
    minDelta = (delta < minDelta ? delta : minDelta);

    if((*it) == paddr - buff->getLineSize() || (*it) == paddr + buff->getLineSize()) {
      foundUnitStride = true;
      break;
    }

    it++;
  }
  
  // putting the new miss in the queue after we computed the stride
  lastMissesQ.push_back(paddr);
  if(lastMissesQ.size() > missWindow)
    lastMissesQ.pop_front();
  
  if(foundUnitStride) {
    unitStrideStreams.inc();
    newStride = buff->getLineSize();
  } else {
    nonUnitStrideStreams.inc();
    newStride = minDelta;
  }

  LOG("minDelta = %ld", minDelta);
  
  if(newStride == 0 || newStride == (ulong) -1 || newStride > maxStride) {
    ignoredStreams.inc();
    return;
  }

  PAddr nextAddr = goingUp ? paddr + newStride : paddr - newStride;

  // TODO: do a better check if there is an overlapping stream
  if(!table->readLine(nextAddr) && !table->readLine(paddr)) {
    pEntry *pe = table->fillLine(paddr);
    pe->stride = newStride;
    pe->goingUp = goingUp;
    LOG("SP: new stream. stride=%d paddr=%08lx nextAddr=%08lx %s", (int) newStride, paddr, nextAddr, goingUp ? "UP" : "DOWN");
    prefetch(pe, lat + learnMissDelay); 
    pe->setTag(table->calcTag(pe->nextAddr(table)));
  }
}

void StridePrefetcher::prefetch(pEntry *pe, Time_t lat)
{
  ulong bsize = buff->getLineSize();
  PAddr prefAddr = pe->nextAddr(table);

  for(int i = 0; i < depth; i++) {
    if(!buff->readLine(prefAddr)) { // it is not in the buff
      penFetchSet::iterator it = pendingFetches.find(prefAddr);
      if(it == pendingFetches.end()) {
	CBMemRequest *r;
	r = CBMemRequest::create(lat, lowerLevel[0], MemRead, prefAddr, 
				 processAckCB::create(this, prefAddr));
	pendingFetches.insert(prefAddr);
      }
    }
    prefAddr += i * pe->stride;
  }
}

void StridePrefetcher::access(MemRequest *mreq)
{
  ulong paddr = mreq->getPAddr() & defaultMask;
  LOG("SP:access addr=%08lx", paddr);

  // TODO: should i really consider all these read types? 
  if (mreq->getMemOperation() == MemRead
      || mreq->getMemOperation() == MemReadW
      || mreq->getMemOperation() == MemReadX) {
    read(mreq);
  } else {
    LOG("SP:ignoring access addr=%08lx type=%d", paddr, mreq->getMemOperation());
    nextBuffSlot();
    
    bLine *l = buff->readLine(paddr);
    if(l)
      l->invalidate();

    mreq->goDown(0, lowerLevel[0]);
  }
}

void StridePrefetcher::returnAccess(MemRequest *mreq)
{
  ulong paddr = mreq->getPAddr() & defaultMask;
  LOG("SP:returnAccess addr=%08lx", paddr);

  mreq->goUp(0);
}

void StridePrefetcher::processAck(PAddr addr)
{
  ulong paddr = addr & defaultMask;

  penFetchSet::iterator itF = pendingFetches.find(paddr);
  if(itF == pendingFetches.end()) 
    return;

  bLine *l = buff->fillLine(paddr);

  penReqMapper::iterator it = pendingRequests.find(paddr);

  if(it != pendingRequests.end()) {
    LOG("SP:returnAccess addr=%08lx", paddr);
    std::queue<MemRequest *> *tmpReqQueue;
    tmpReqQueue = (*it).second;
    while (tmpReqQueue->size()) {
      tmpReqQueue->front()->goUpAbs(nextBuffSlot());
      tmpReqQueue->pop();
    }
    pendingRequests.erase(paddr);
    activeMemReqPool.in(tmpReqQueue);
  }
  pendingFetches.erase(paddr);
}

bool StridePrefetcher::canAcceptStore(PAddr addr)
{
  return true;
}

void StridePrefetcher::invalidate(PAddr addr,ushort size,MemObj *oc)
{ 
   ulong paddr = addr & defaultMask;
   nextBuffSlot();

   bLine *l = buff->readLine(paddr);
   if(l)
     l->invalidate();

   //invUpperLevel(addr,size,cb); 
}

Time_t StridePrefetcher::getNextFreeCycle() const
{
  return buffPort->calcNextSlot();
}

