
/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by  Smruti Sarangi

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

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "EnergyMgr.h"
#include "ReportGen.h"
#include <iostream>

GStatsEnergy::EProcStoreType  GStatsEnergy::eProcStore;
GStatsEnergy::EGroupStoreType GStatsEnergy::eGroupStore; 
GStatsEnergy::EPartStoreType  GStatsEnergy::ePartStore;

double GStatsEnergyNull::getDouble() const
{
  return 0;
}

void GStatsEnergyNull::inc()
{
}

void GStatsEnergyNull::add(int v)
{
}


GStatsEnergy::GStatsEnergy(const char *field, const char *blk
			   ,int procId, EnergyGroup grp
			   ,double energy, const char *part)
  :StepEnergy(energy)
  ,steps(0)
  ,gid(grp)
{
  if( eProcStore.size() <= static_cast<size_t>(procId) )
    eProcStore.resize(procId+1);
  eProcStore[procId].push_back(this);

  if(eGroupStore.size() <= static_cast<size_t>(grp) )
    eGroupStore.resize(grp + 1);
  eGroupStore[grp].push_back(this);

  if(part != "") 
    ePartStore[part].push_back(this);
    
  char cadena[1024];
  sprintf(cadena,"%s:%s", blk, field) ;
  name = strdup(cadena);
  subscribe();
}


GStatsEnergy::~GStatsEnergy()
{
  unsubscribe();
  free(name);
}

void GStatsEnergy::dump(int procId)
{
  double pVals[MaxPowerGroup];
  for(int c=0; c < MaxPowerGroup; c++) 
    pVals[c] = 0.0;

  I((unsigned int)procId < eProcStore.size());

  // calculate the values
  for(size_t i=0;i<eProcStore[procId].size();i++) {
    GStatsEnergy *e = eProcStore[procId][i];

    double pwr = EnergyMgr::etop(e->getDouble());
    EnergyGroup eg = static_cast<EnergyGroup>(e->getGid()) ;
    PowerGroup pg = EnergyStore::getPowerGroup(eg);
   
    pVals[static_cast<unsigned int>(pg)] += pwr;
  }

  // dump the values
  for(int j=1; j < ClockPower;j++)
    Report::field("Proc(%d):%s=%g",procId, EnergyStore::getStr(static_cast<PowerGroup>(j)), pVals[j]);
}

void GStatsEnergy::dump()
{
  double pVals[MaxPowerGroup];
  for(int c=0; c < ClockPower; c++) 
    pVals[c] = 0.0;

  // calculate the values
  for(size_t i=1;i< MaxEnergyGroup ;i++) {
    double pwr = EnergyMgr::etop(GStatsEnergy::getTotalGroup(static_cast<EnergyGroup>(i)));
    int pg = static_cast<int>(EnergyStore::getPowerGroup(static_cast<EnergyGroup>(i)));
    pVals[pg] += pwr;
  }

  // dump the values
  for(int j=1; j < ClockPower;j++)
    Report::field("PowerMgr:%s=%g",EnergyStore::getStr(static_cast<PowerGroup>(j)),pVals[j]);
  for(int j=1; j < ClockPower;j++)
    Report::field("EnergyMgr:%s=%g",EnergyStore::getEnergyStr(static_cast<PowerGroup>(j)),EnergyMgr::ptoe(pVals[j]));

}

void GStatsEnergy::reportValue() const
{
  Report::field("%s=%g", name, getDouble());
}

double GStatsEnergy::getTotalProc(int procId)
{
  double total=0;

  I((unsigned int)procId < eProcStore.size());

  for(size_t i=0;i<eProcStore[procId].size();i++) {
    GStatsEnergy *e = eProcStore[procId][i];
    total += e->getDouble();
  }

  return total;
}

double GStatsEnergy::getTotalGroup(EnergyGroup grp)
{
  double total=0;

  if(eGroupStore.size() <= static_cast<size_t>(grp))
    return 0.0;

  for(size_t i=0;i<eGroupStore[grp].size();i++) {
    total += eGroupStore[grp][i]->getDouble();
  }

  return total;
}

double GStatsEnergy::getTotalPart(const char *pname)
{
  double total=0;

  if (ePartStore.find(pname) == ePartStore.end())
    return 0;

  std::vector<GStatsEnergy *> vv = ePartStore[pname];

  for(size_t i=0;i<vv.size();i++) {
    total += vv[i]->getDouble();
  }

  return total;
}

double GStatsEnergy::getDouble() const
{
#ifndef ACCESS
  return StepEnergy*steps;
#else
  return static_cast<double>(steps) ;
#endif
}

void GStatsEnergy::inc() 
{
  steps++;
}

void GStatsEnergy::add(int v)
{
  I(v >= 0);
  steps += v;
}

/*****************************************************
  *           GStatsEnergyCGBase
  ****************************************************/

GStatsEnergyCGBase::GStatsEnergyCGBase(const char* str,int procId)
  : lastCycleUsed(0) 
  ,numCycles(0)
{
  id = procId;
  name = strdup(str);
}

void GStatsEnergyCGBase::use()
{
  if(lastCycleUsed != globalClock) {
    numCycles++;
    lastCycleUsed = globalClock;
  }
}

/*****************************************************
  *           GStatsEnergyCG
  ****************************************************/
GStatsEnergyCG::GStatsEnergyCG(const char *name, 
			       const char* block, 
			       int procId, 
			       EnergyGroup grp, 
			       double energy, 
			       GStatsEnergyCGBase *b,
			       const char *pt)
  : GStatsEnergy(name, block, procId, grp, energy, pt) 
{
  eb = b;
  localE = energy*0.95;
  clockE = energy*0.05;
}

double GStatsEnergyCG::getDouble() const
{
#ifndef ACCESS
  return (steps * localE + eb->getNumCycles()*clockE);
#else
  return static_cast<double>(steps);
#endif
}

void GStatsEnergyCG::inc()
{
  steps++;
  eb->use();
}

void GStatsEnergyCG::add(int v)
{
  I(v >= 0);
  steps += v;
  eb->use();
}

// Energy Store functions
EnergyStore::EnergyStore() 
{
  proc = SescConf->getCharPtr("","cpucore",0) ;
}

double EnergyStore::get(const char *name, int procId)
{
  return get(proc, name, procId);
}

double EnergyStore::get(const char *block, const char *name, int procId)
{
  return SescConf->getDouble(block, name);
}

PowerGroup EnergyStore::getPowerGroup(EnergyGroup e)
{
  switch(e) { 
  case BPredEnergy:
  case RasEnergy:
  case BTBEnergy:
  case ITLBEnergy:
  case IRdHitEnergy:
  case IRdMissEnergy:
  case IWrHitEnergy:
  case IWrMissEnergy:
    return FetchPower;

    // processor core resources that scale energy as the issue width
    // increases
  case WindowRdWrEnergy:
  case WindowSelEnergy:
  case WindowCheckEnergy:
#ifdef SESC_DDIS
  case WinDepsEnergy:
  case DepTableEnergy:
#endif
  case RenameEnergy: 
  case WrRegEnergy:
  case RdRegEnergy:
  case LDQCheckEnergy:
  case LDQRdWrEnergy:
  case STQCheckEnergy:
  case STQRdWrEnergy:
  case ROBEnergy:
    return IssuePower;
  
    // processor core resources that DO NOT scale energy as the issue
    // width increases
  case IAluEnergy:
  case FPAluEnergy:
  case ResultBusEnergy:
  case ForwardBusEnergy:
    return ExecPower;
   
  case DTLBEnergy:
  case RdHitEnergy:
  case RdMissEnergy:
  case RdHalfHitEnergy:
  case WrHitEnergy:
  case WrMissEnergy:
  case WrHalfHitEnergy:
  case RdRevLVIDEnergy:
  case WrRevLVIDEnergy:
  case RdLVIDEnergy:
  case WrLVIDEnergy:
  case CombWriteEnergy:
  case LVIDTableRdEnergy:
  case BusEnergy:

    return MemPower;
  default:
  case NOT_VALID_ENERGY:
    I(0);
    break;
  }

  return Not_Valid_Power;
}

char* EnergyStore::getStr(EnergyGroup d)
{
  switch (d) {
  case BPredEnergy:
    return "BPredEnergy";
  case RasEnergy:
    return "RasEnergy";
  case BTBEnergy:
    return "BTBEnergy";
  case WindowRdWrEnergy:
    return "WindowRdWrEnergy";
  case WindowSelEnergy:
    return "WindowSelEnergy";
  case WindowCheckEnergy:
    return "WindowCheckEnergy";
#ifdef SESC_DDIS
  case WinDepsEnergy:
    return "WinDepsEnergy";
  case DepTableEnergy:
    return "DepTableEnergy";
#endif
  case LDQCheckEnergy:
    return "LDQCheckEnergy";
  case LDQRdWrEnergy:
    return "LDQRdWrEnergy";
  case STQCheckEnergy:
    return "STQCheckEnergy";
  case STQRdWrEnergy:
    return "STQRdWrEnergy";
  case RenameEnergy:
    return "RenameEnergy";
  case IAluEnergy:
    return "IAluEnergy";
  case FPAluEnergy:
    return"FPAluEnergy";
  case ResultBusEnergy:
    return "ResultBusEnergy";
  case ForwardBusEnergy:
    return "ForwardBusEnergy";
  case ROBEnergy:
    return "ROBEnergy";
  case WrRegEnergy:
    return "WrRegEnergy";
  case RdRegEnergy:
    return "RdRegEnergy";
  case DTLBEnergy:
    return "DTLBEnergy";
  case RdHitEnergy:
    return "RdHitEnergy";
  case RdMissEnergy:
    return "RdMissEnergy";
  case WrHitEnergy:
    return "WrHitEnergy";
  case WrMissEnergy:
    return "WrMissEnergy";
  case ITLBEnergy:
    return "InstTLBEnergy";
  case IRdHitEnergy:
    return "InstRdHitEnergy";
  case IRdMissEnergy:
    return "InstRdMissEnergy";
  case IWrHitEnergy:
    return "InstWrHitEnergy";
  case IWrMissEnergy:
    return "InstWrMissEnergy";
  case RdRevLVIDEnergy:
    return "RdRevLVIDEnergy";
  case WrRevLVIDEnergy:
    return "WrRevLVIDEnergy";
  case RdLVIDEnergy:
    return "RdLVIDEnergy";
  case WrLVIDEnergy:
    return "WrLVIDEnergy";
  case CombWriteEnergy:
    return "CombWriteEnergy";
  case LVIDTableRdEnergy:
    return "LVIDTableRdEnergy";
  case BusEnergy:
    return "BusEnergy";

  case NOT_VALID_ENERGY:
  default:
    return "NOT_VALID_ENERGY";
  }

  return "invalid";
}

char* EnergyStore::getStr(PowerGroup p)
{
  switch(p) {
  case FetchPower:
    return "fetchPower";
  case IssuePower:
    return "issuePower";
  case MemPower:
    return "memPower";
  case ExecPower:
    return "execPower";
  case ClockPower:
    return "clockPower";
  case  Not_Valid_Power:
  default:
    I(0);
    return "";
  }

  return 0;
}

char *EnergyStore::getEnergyStr(PowerGroup p)
{
   switch(p) {
  case FetchPower:
    return "fetchEnergy";
  case IssuePower:
    return "issueEnergy";
  case MemPower:
    return "memEnergy";
  case ExecPower:
    return "execEnergy";
  case ClockPower:
    return "clockEnergy";
  case  Not_Valid_Power:
  default:
    I(0);
    return "";
  }

  return 0;

}
