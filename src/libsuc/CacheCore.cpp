/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  James Tuck
                  Smruti Sarangi
		  Luis Ceze
		  Karin Strauss

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

#define CACHECORE_CPP

#include <stddef.h>
#include <stdarg.h>

#include "CacheCore.h"
#include "SescConf.h"
#include "EnergyMgr.h"

#define k_RANDOM     "RANDOM"
#define k_LRU        "LRU"

//
// Class CacheGeneric, the combinational logic of Cache
//
template<class State, class Addr_t, bool Energy>
CacheGeneric<State, Addr_t, Energy> *CacheGeneric<State, Addr_t, Energy>::create(int size, int assoc, int bsize, const char *pStr)
{
  GLOG(size == (assoc * bsize), "Do you want a faster simulation. Then be nice and implement CacheFA");

  CacheGeneric *cache;

  if (assoc==1) {
    // Direct Map cache
    cache = new CacheDM<State, Addr_t, Energy>(size, bsize, pStr);
  }else{
    // Associative Cache
    cache = new CacheAssoc<State, Addr_t, Energy>(size, assoc, bsize, pStr);
  }

  I(cache);
  return cache;
}

template<class State, class Addr_t, bool Energy>
EnergyGroup CacheGeneric<State, Addr_t, Energy>::getRightStat(EnergyGroup eg, const char* type)
{
  if(type == 0) 
    return eg;

  if(!strcmp(type,"cache")) 
    return eg;

  if(!strcmp(type,"dtlb")) 
    return DTLBEnergy;

  if(!strcmp(type,"itlb")) 
    return ITLBEnergy;

  if(!strcmp(type,"icache")) {
    switch(eg) {
    case RdHitEnergy:
      return IRdHitEnergy;
    case RdMissEnergy:
      return IRdMissEnergy;
    case WrHitEnergy:
      return IWrHitEnergy;
    case WrMissEnergy:
      return IWrMissEnergy;
    default:
      I(0);
    }
  }

  // default case
  return eg;
}

template<class State, class Addr_t, bool Energy>
void CacheGeneric<State, Addr_t, Energy>::createStats(const char *section, const char *name)
{
  // get the type
  bool typeExists = SescConf->checkCharPtr(section, "deviceType");
  const char *type = 0;
  if (typeExists)
    type = SescConf->getCharPtr(section, "deviceType");

  int procId = 0;
  if ( name[0] == 'P' && name[1] == '(' ) {
    // This structure is assigned to an specific processor
    const char *number = &name[2];
    procId = atoi(number);
  }

  if (Energy) {
    EnergyGroup eg;
    eg = getRightStat(RdHitEnergy, type);
    rdEnergy[0] = new GStatsEnergy("rdHitEnergy",name
				   ,procId
				   ,eg
				   ,EnergyMgr::get(section,"rdHitEnergy"),section);

    eg = getRightStat(RdMissEnergy, type);
    rdEnergy[1] = new GStatsEnergy("rdMissEnergy",name
				   ,procId
				   ,eg
				   ,EnergyMgr::get(section,"rdMissEnergy"),section);

    eg = getRightStat(WrHitEnergy, type);
    wrEnergy[0]  = new GStatsEnergy("wrHitEnergy",name
				    ,procId
				    ,eg
				    ,EnergyMgr::get(section,"wrHitEnergy"),section);

    eg = getRightStat(WrMissEnergy, type);
    wrEnergy[1] = new GStatsEnergy("wrMissEnergy",name
				   ,procId
				   ,eg
				   ,EnergyMgr::get(section,"wrMissEnergy"),section);

  }else{
    rdEnergy[0]  = 0;
    rdEnergy[1]  = 0;
    wrEnergy[0]  = 0;
    wrEnergy[1]  = 0;
  }
}

template<class State, class Addr_t, bool Energy>
CacheGeneric<State, Addr_t, Energy> *CacheGeneric<State, Addr_t, Energy>::create(const char *section, const char *append, const char *format, ...)
{
  CacheGeneric *cache=0;
  char size[STR_BUF_SIZE];
  char bsize[STR_BUF_SIZE];
  char assoc[STR_BUF_SIZE];
  char repl[STR_BUF_SIZE];

  snprintf(size ,STR_BUF_SIZE,"%sSize" ,append);
  snprintf(bsize,STR_BUF_SIZE,"%sBsize",append);
  snprintf(assoc,STR_BUF_SIZE,"%sAssoc",append);
  snprintf(repl ,STR_BUF_SIZE,"%sReplPolicy",append);

  int s = SescConf->getLong(section, size);
  int a = SescConf->getLong(section, assoc);
  int b = SescConf->getLong(section, bsize);

  const char *pStr = SescConf->getCharPtr(section, repl);

  if(SescConf->isGT(section, size, 0) &&
     SescConf->isGT(section, bsize, 0) &&
     SescConf->isGT(section, assoc, 0) &&
     SescConf->isPower2(section, size) && 
     SescConf->isPower2(section, bsize) &&
     SescConf->isPower2(section, assoc) &&
     SescConf->isInList(section, repl, k_RANDOM, k_LRU)) {

    cache = create(s, a, b, pStr);
  } else {
    // this is just to keep the configuration going, 
    // sesc will abort before it begins
    cache = new CacheAssoc<State, Addr_t, Energy>(2, 
						  1, 
						  1, 
						  pStr);
  }

  I(cache);

  char cacheName[STR_BUF_SIZE];
  {
    va_list ap;
    
    va_start(ap, format);
    vsprintf(cacheName, format, ap);
    va_end(ap);
  }
  cache->createStats(section, cacheName);

  return cache;
}

/*********************************************************
 *  CacheAssoc
 *********************************************************/

template<class State, class Addr_t, bool Energy>
CacheAssoc<State, Addr_t, Energy>::CacheAssoc(int size, int assoc, int blksize, const char *pStr) 
  : CacheGeneric<State, Addr_t, Energy>(size, assoc, blksize) 
{
  I(numLines>0);
  
  if (strcasecmp(pStr, k_RANDOM) == 0) 
    policy = RANDOM;
  else if (strcasecmp(pStr, k_LRU)    == 0) 
    policy = LRU;
  else {
    MSG("Invalid cache policy [%s]",pStr);
    exit(0);
  }

  mem     = new Line[numLines + 1];
  content = new Line* [numLines + 1];

  for(ulong i = 0; i < numLines; i++) {
    content[i] = &mem[i];
  }
  
  irand = 0;
}

template<class State, class Addr_t, bool Energy>
CacheAssoc<State, Addr_t, Energy>::~CacheAssoc<State, Addr_t, Energy>()
{
  delete content;
  delete mem;
}


template<class State, class Addr_t, bool Energy>
typename CacheAssoc<State, Addr_t, Energy>::Line *CacheAssoc<State, Addr_t, Energy>::findLineTag(Addr_t tag)
{
  GI(Energy, goodInterface); // If modeling energy. Do not use this
			     // interface directly. use readLine and
			     // writeLine instead. If it is called
			     // inside debugging only use
			     // findLineDebug instead

  Line **theSet = &content[calcIndex4Tag(tag)];

  // Check most typical case
  if ((*theSet)->getTag() == tag) {
    I((*theSet)->isValid());
    return *theSet;
  }

  Line **lineHit=0;
  Line **setEnd = theSet + assoc;

  // For sure that position 0 is not (short-cut)
  {
    Line **l = theSet + 1;
    while(l < setEnd) {
      if ((*l)->getTag() == tag) {
	lineHit = l;
	break;
      }
      l++;
    }
  }

  if (lineHit == 0)
    return 0;

  I((*lineHit)->isValid());

  // No matter what is the policy, move lineHit to the *theSet. This
  // increases locality
  Line *tmp = *lineHit;
  {
    Line **l = lineHit;
    while(l > theSet) {
      Line **prev = l - 1;
      *l = *prev;;
      l = prev;
    }
    *theSet = tmp;
  }

  return tmp;
}

template<class State, class Addr_t, bool Energy>
typename CacheAssoc<State, Addr_t, Energy>::Line 
*CacheAssoc<State, Addr_t, Energy>::findLine2Replace(Addr_t addr, bool ignoreLocked)
{ 
  Addr_t tag    = calcTag(addr);
  Line **theSet = &content[calcIndex4Tag(tag)];

  // Check most typical case
  if ((*theSet)->getTag() == tag) {
    GI(tag,(*theSet)->isValid());
    return *theSet;
  }

  Line **lineHit=0;
  Line **lineFree=0; // Order of preference, invalid, locked
  Line **setEnd = theSet + assoc;
  
  // Start in reverse order so that get the youngest invalid possible,
  // and the oldest isLocked possible (lineFree)
  {
    Line **l = setEnd -1;
    while(l >= theSet) {
      if ((*l)->getTag() == tag) {
	lineHit = l;
	break;
      }
      if (!(*l)->isValid())
	lineFree = l;
      else if (lineFree == 0 && !(*l)->isLocked())
	lineFree = l;

      // If line is invalid, isLocked must be false
      GI(!(*l)->isValid(), !(*l)->isLocked()); 
      l--;
    }
  }
  GI(lineFree, !(*lineFree)->isValid() || !(*lineFree)->isLocked());

  if (lineHit)
    return *lineHit;
  
  I(lineHit==0);

  if(lineFree == 0 && !ignoreLocked)
    return 0;

  if (lineFree == 0) {
    I(ignoreLocked);
    if (policy == RANDOM) {
      lineFree = &theSet[irand];
      irand = (irand + 1) & maskAssoc;
    }else{
      I(policy == LRU);
      // Get the oldest line possible
      lineFree = setEnd-1;
    }
  }else if(ignoreLocked) {
    if (policy == RANDOM && (*lineFree)->isValid()) {
      lineFree = &theSet[irand];
      irand = (irand + 1) & maskAssoc;
    }else{
      //      I(policy == LRU);
      // Do nothing. lineFree is the oldest
    }
  }

  I(lineFree);
  GI(!ignoreLocked, !(*lineFree)->isValid() || !(*lineFree)->isLocked());

  if (lineFree == theSet)
    return *lineFree; // Hit in the first possition

  // No matter what is the policy, move lineHit to the *theSet. This
  // increases locality
  Line *tmp = *lineFree;
  {
    Line **l = lineFree;
    while(l > theSet) {
      Line **prev = l - 1;
      *l = *prev;;
      l = prev;
    }
    *theSet = tmp;
  }

  return tmp;
}

/*********************************************************
 *  CacheDM
 *********************************************************/

template<class State, class Addr_t, bool Energy>
CacheDM<State, Addr_t, Energy>::CacheDM(int size, int blksize, const char *pStr) 
  : CacheGeneric<State, Addr_t, Energy>(size, 1, blksize) 
{
  I(numLines>0);
  
  mem     = new Line[numLines + 1];
  content = new Line* [numLines + 1];

  for(ulong i = 0; i < numLines; i++) {
    content[i] = &mem[i];
  }
}

template<class State, class Addr_t, bool Energy>
CacheDM<State, Addr_t, Energy>::~CacheDM<State, Addr_t, Energy>()
{
  delete content;
  delete mem;
}

template<class State, class Addr_t, bool Energy>
typename CacheDM<State, Addr_t, Energy>::Line *CacheDM<State, Addr_t, Energy>::findLineTag(Addr_t tag)
{
  GI(Energy, goodInterface); // If modeling energy. Do not use this
			     // interface directly. use readLine and
			     // writeLine instead. If it is called
			     // inside debugging only use
			     // findLineDebug instead

  Line *line = content[calcIndex4Tag(tag)];

  if (line->getTag() == tag) {
    I(line->isValid());
    return line;
  }

  return 0;
}

template<class State, class Addr_t, bool Energy>
typename CacheDM<State, Addr_t, Energy>::Line 
*CacheDM<State, Addr_t, Energy>::findLine2Replace(Addr_t addr, bool ignoreLocked)
{ 
  Addr_t tag    = calcTag(addr);
  Line *line = content[calcIndex4Tag(tag)];

  if (ignoreLocked)
    return line;

  if (line->getTag() == tag) {
    GI(tag,line->isValid());
    return line;
  }

  if (line->isLocked())
    return 0;
  
  return line;
}

