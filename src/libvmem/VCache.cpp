/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
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

#include "VCache.h"
#include "EnergyMgr.h"
#include "Port.h"
#include "MemorySystem.h"

//VCache::SystemCache VCache::systemCache;

VCache::VCache(MemorySystem *gms, const char *section, const char *name)
  : VMemObj(gms, section, name)
  ,lvidTable(section, name)
    // Begin Statistics
  ,lvidTableRdEnergy("lvidTableRdEnergy" // TODO: real energy
		     ,"VCache"
		     ,gms->getId() // cpuID
		     ,RdHitEnergy
		     ,EnergyMgr::get(section,"rdHitEnergy"),section)
    // Read
  ,rdHit("%s:readHit"     , name)
  ,rdMiss("%s:readMiss"    , name)
  ,rdHalfMiss("%s:readHalfMiss", name)
  ,rdHalfHit("%s:readHalfHit" , name) 
    // Write
  ,wrHit("%s:writeHit"     , name) 
  ,wrMiss("%s:writeMiss"    , name)
  ,wrHalfMiss("%s:writeHalfMiss", name)
  ,wrHalfHit("%s:writeHalfHit" , name)
    // Write
  ,pushHit("%s:pushHit"     , name) 
  ,pushMiss("%s:pushMiss"    , name)
  ,pushHalfMiss("%s:pushHalfMiss", name)
{
  SescConf->isLong(section   , "MSHRSize");
  SescConf->isBetween(section, "MSHRSize", 1, 32768);
  
  mshr = MSHR<PAddr,VCache>::create(name,
			     SescConf->getCharPtr(section, "MSHRtype"),
			     SescConf->getLong(section, "MSHRSize"),
			     SescConf->getLong(section, "bsize"));
  
  SescConf->isLong(section    , "hitDelay");
  hitDelay = SescConf->getLong(section, "hitDelay");

  SescConf->isLong(section    , "missDelay");
  missDelay = SescConf->getLong(section, "missDelay");

  SescConf->isLong(section    , "bsize");
  SescConf->isPower2(section  , "bsize");
  SescConf->isBetween(section , "bsize",1,128); // 32bis in VMemState
  
  // all cache line sizes should be the same across the versioned
  // system
  I(SescConf->getLong(section, "bsize") == 
    SescConf->getLong("TaskScalar", "bsize"));

  SescConf->isLong(section, "numPorts");
  SescConf->isLong(section, "portOccp");

  NumUnits_t  num = SescConf->getLong(section, "numPorts");
  TimeDelta_t occ = SescConf->getLong(section, "portOccp");
  
  cachePort = PortGeneric::create(name, num, occ);

  // Reverse LVID 
  if(SescConf->checkCharPtr(section,"revLVIDTable")){
    const char *revBlock = SescConf->getCharPtr(section,"revLVIDTable");
    rdRevLVIDEnergy = new GStatsEnergy("rdRevLVIDEnergy",name,0,RdRevLVIDEnergy,EnergyMgr::get(revBlock,"rdHitEnergy"),section);
    wrRevLVIDEnergy = new GStatsEnergy("wrRevLVIDEnergy",name,0,WrRevLVIDEnergy,EnergyMgr::get(revBlock,"wrHitEnergy"),section);
  }
  else{
    rdRevLVIDEnergy = 0;
    wrRevLVIDEnergy = 0;
  }
  // LVID 
  rdLVIDEnergy    = new GStatsEnergy("rdLVIDEnergy",name,0,RdLVIDEnergy,EnergyMgr::get(section,"rdLVIDEnergy"),section); 
  wrLVIDEnergy    = new GStatsEnergy("wrLVIDEnergy",name,0,WrLVIDEnergy,EnergyMgr::get(section,"wrLVIDEnergy"),section);

  // Cache Access
  rdHitEnergy = new GStatsEnergy("rdHitEnergy",name
				 ,gms->getId()
				 ,MemPower
				 ,EnergyMgr::get(section,"rdHitEnergy"));
  rdMissEnergy = new GStatsEnergy("rdMissEnergy",name
				  ,gms->getId()
				  ,MemPower
				  ,EnergyMgr::get(section,"rdMissEnergy"));

  wrHitEnergy  = new GStatsEnergy("wrHitEnergy"
				  ,name
				  ,gms->getId()
				  ,MemPower
				  ,EnergyMgr::get(section,"wrHitEnergy"));
  wrMissEnergy = new GStatsEnergy("wrMissEnergy"
				  ,name
				  ,gms->getId()
				  ,MemPower
				  ,EnergyMgr::get(section,"wrMissEnergy"));
}

VCache::~VCache()
{
}

void VCache::invalidate(PAddr addr, ushort size, MemObj *oc)
{
  // just ignore the coherence. A real machine should ask for all the version
  // sharing that address and invalidate it (it may generate task restarts)
}

Time_t VCache::getNextFreeCycle() const
{
  return cachePort->calcNextSlot();
}

bool VCache::canAcceptStore(PAddr paddr)
{
  // If there are no pending entries for that address
  return mshr->canAcceptRequest(paddr);
}

