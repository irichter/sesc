/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

   Contributed by Jose Renau

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

#include "Cluster.h"

#include "GEnergy.h"
#include "GMemorySystem.h"
#include "GProcessor.h"
#include "Port.h"
#include "Resource.h"
#include "SescConf.h"

#include "estl.h"

// Begin: Fields used during constructions

struct UnitEntry {
  PortGeneric *gen;
  long num;
  long occ;
};

//Class for comparison to be used in hashes of char * where the content is to be compared
class eqstr {
public:
  inline bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) == 0;
  }
};

typedef HASH_MAP<const char *,UnitEntry, HASH<const char*>, eqstr> UnitMapType;

static UnitMapType unitMap;

Cluster::~Cluster()
{
    // Nothing to do
}

Cluster::Cluster(const char *clusterName, GProcessor *gp)
  : window(gp->getId(),clusterName)
  ,MaxWinSize(SescConf->getLong(clusterName,"winSize"))
  ,windowSize(SescConf->getLong(clusterName,"winSize")) 
  ,gproc(gp)
  ,winNotUsed("Proc(%d)_%s_winNotUsed",gp->getId(), clusterName)
{
    bzero(res,sizeof(Resource *)*MaxInstType);
}

void Cluster::buildUnit(const char *clusterName
			,GMemorySystem *ms
			,Cluster *cluster
			,InstType type
			,GStatsEnergyCGBase *ecgbase
)
{
  const char *unitType = Instruction::opcode2Name(type);
  
  char utUnit[1024];
  char utLat[1024];
  sprintf(utUnit,"%sUnit",unitType);
  sprintf(utLat,"%sLat",unitType);
  
  if( !SescConf->checkCharPtr(clusterName,utUnit) )
    return;

  const char *unitName = SescConf->getCharPtr(clusterName,utUnit);
  
  TimeDelta_t lat = SescConf->getLong(clusterName,utLat);
  PortGeneric *gen;

  SescConf->isBetween(clusterName,utLat,0,1024);
  UnitMapType::const_iterator it = unitMap.find(unitName);
    
  if( it != unitMap.end() ) {
    gen = it->second.gen;
  }else{
    UnitEntry e;
    e.num = SescConf->getLong(unitName,"Num");
    SescConf->isLT(unitName,"Num",1024);
      
    e.occ = SescConf->getLong(unitName,"occ");
    SescConf->isBetween(unitName,"occ",0,1024);
      
    char name[1024];
    sprintf(name,"%s(%d)", unitName, (int)gproc->getId());
    e.gen = PortGeneric::create(name,e.num,e.occ);

    unitMap[unitName] = e;
    gen = e.gen;
  }

  Resource *r=0;
  GStatsEnergyCG *eng;
  char *strtmp=0;

  char name[100];
  sprintf(name, "Cluster(%d)", (int)gproc->getId());

  switch(type) {
  case iALU:
    strtmp = strdup("iALUEnergy");
  case iMult:
    if(!strtmp) strtmp = strdup("iMultEnergy");
  case iDiv:
    if(!strtmp) strtmp = strdup("iDivEnergy");
   eng = new GStatsEnergyCG(static_cast<const char*>(strtmp)
			    ,name
			    ,gproc->getId()
			    ,IAluEnergy
			    ,EnergyMgr::get("iALUEnergy",gproc->getId())
			    ,ecgbase);

   r = new FUGeneric(cluster, gen, lat, eng);
   break;
  case fpALU:
   strtmp = strdup("fpALUEnergy");
  case fpMult:
   if(!strtmp) strtmp=strdup("fpMultEnergy");
  case fpDiv:
   if(!strtmp) strtmp=strdup("fpDivEnergy");
  eng = new GStatsEnergyCG(static_cast<const char*>(strtmp)
			   ,name
			   ,gproc->getId()
			   ,FPAluEnergy
			   ,EnergyMgr::get("fpALUEnergy", gproc->getId())
			   ,ecgbase);
 
  r = new FUGeneric(cluster, gen, lat, eng);
  break ;
  case iBJ:
    {
      int MaxBranches = SescConf->getLong("cpucore", "maxBranches", gproc->getId());
      if( MaxBranches == 0 )
	MaxBranches = INT_MAX;
	
      r = new FUBranch(cluster, gen, lat, MaxBranches);
    }
    break;
  case iLoad:
    {
      TimeDelta_t ldstdelay=SescConf->getLong("cpucore", "stForwardDelay",gproc->getId());
      SescConf->isLong("cpucore", "maxLoads",gproc->getId());
      SescConf->isBetween("cpucore", "maxLoads", 0, 256*1024, gproc->getId());
      int maxLoads=SescConf->getLong("cpucore", "maxLoads",gproc->getId());
      if( maxLoads == 0 )
	maxLoads = 256*1024;
	
      r = new FULoad(cluster, gen, lat, ldstdelay, ms, maxLoads, gproc->getId());
    }
    break;
  case iStore:
    {
      SescConf->isLong("cpucore", "maxStores",gproc->getId());
      SescConf->isBetween("cpucore", "maxStores", 0, 256*1024, gproc->getId());
      int maxStores=SescConf->getLong("cpucore", "maxStores",gproc->getId());
      if( maxStores == 0 )
	maxStores = 256*1024;
	
      r = new FUStore(cluster, gen, lat, ms, maxStores, gproc->getId());

      // Those resources go together with the store unit (TODO: LD/ST unit
      // should go to the same cluster)
      if(res[iFence]==0) {
	res[iFence] = new FUMemory(cluster, ms, gproc->getId());
	I(res[iEvent]==0);
	res[iEvent] = new FUEvent(cluster);
      }
    }
    break;
  default:
    I(0);
    MSG("Unknown unit type [%d] [%s]",type,Instruction::opcode2Name(type));
  }
  I(r);
  I(res[type] == 0);
  res[type] = r;
}

Cluster *Cluster::create(const char *clusterName, GMemorySystem *ms, GProcessor *gproc)
{
  // Note: Clusters do NOT share functional units. This breaks the cluster
  // principle. If someone were interested in doing that, the UnitMap sould be
  // cleared (unitMap.clear()) by the PendingWindow instead of buildCluster. If
  // the objective is to share the FUs even between Cores, UnitMap must be
  // declared static in that class (what a mess!)

  unitMap.clear();

  // Constraints
  SescConf->isCharPtr(clusterName,"recycleAt");
  SescConf->isInList(clusterName,"recycleAt","Execute","Retire");
  const char *recycleAt = SescConf->getCharPtr(clusterName,"recycleAt");
  
  SescConf->isLong(clusterName,"winSize");
  SescConf->isBetween(clusterName,"winSize",1,262144);
  
  Cluster *cluster;
  if( strcasecmp(recycleAt,"retire") == 0) {
    cluster = new RetiredCluster(clusterName, gproc);
  }else{
    I( strcasecmp(recycleAt,"execute") == 0);
    cluster = new ExecutedCluster(clusterName, gproc);
  }

  char strtmp[128];
  sprintf(strtmp,"%s_energy",clusterName);
  GStatsEnergyCGBase *ecgbase = new GStatsEnergyCGBase(strtmp,gproc->getId());


  cluster->buildUnit(clusterName,ms,cluster,iALU,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,iMult,ecgbase);  
  cluster->buildUnit(clusterName,ms,cluster,iDiv,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,iBJ,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,iLoad,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,iStore,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,fpALU,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,fpMult,ecgbase);
  cluster->buildUnit(clusterName,ms,cluster,fpDiv,ecgbase);

 // Do not leave useless garbage
  unitMap.clear();

  return cluster;
}

void ExecutedCluster::executed(DInst *dinst)
{
  dinst->markExecuted();

  delEntry();

  window.executed(dinst);
}

void ExecutedCluster::retire(DInst *dinst)
{
  window.retire(dinst);
  winNotUsed.sample(windowSize);
  // Nothing
}

void RetiredCluster::executed(DInst *dinst)
{
  dinst->markExecuted();

  window.executed(dinst);
}

void RetiredCluster::retire(DInst *dinst)
{
  window.retire(dinst);
  winNotUsed.sample(windowSize);
  delEntry();
}

ClusterManager::ClusterManager(GMemorySystem *ms, GProcessor *gproc)
{
  bzero(res, sizeof(Resource *) * MaxInstType);

  const char *coreSection = SescConf->getCharPtr("","cpucore",gproc->getId());
  if(coreSection == 0) 
    return;  // No core section, bad conf

  int nClusters = SescConf->getRecordSize(coreSection,"cluster");
  
  for(int i=0;i<nClusters;i++) {
    const char *clusterName = SescConf->getCharPtr(coreSection,"cluster",i);
    SescConf->isCharPtr(coreSection,"cluster",i);
    
    Cluster *cluster = Cluster::create(clusterName, ms, gproc);

    for(int t=0;t<MaxInstType;t++) {
      Resource *r = cluster->getResource(static_cast<InstType>(t));
      
      if (r) {
	if (res[t]) {
	  MSG("Multiple cluster have same FUs. Implement replicated clustering");
	  exit(0);
	}
	
	res[t] = r;
      }
    }
  }

  ID(int i);
  IN(forall((i=int(iALU);i<int(MaxInstType);i++),res[i]!=0));
}

#ifdef SESC_MISPATH
void ClusterManager::misBranchRestore()
{
  // If there are multiple clusters with Load/Stores, should do it in
  // all of them
  static_cast<FULoad*>(res[iLoad])->misBranchRestore();
  static_cast<FUStore*>(res[iStore])->misBranchRestore();
}
#endif
