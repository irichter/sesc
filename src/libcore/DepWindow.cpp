/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of California Santa Cruz

   Contributed by Jose Renau
                  Basilio Fraguela
                  Smruti Sarangi

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

#include "DepWindow.h"

#include "DInst.h"
#include "LDSTBuffer.h"
#include "Resource.h"
#include "SescConf.h"
#include "GProcessor.h"

#ifdef SESC_DDIS
GStatsCntr *DepWindow::nDeps[3] = {0,0,0};
#endif

DepWindow::DepWindow(GProcessor *gproc, const char *clusterName)
  :Id(gproc->getId())
  ,InterClusterLat(SescConf->getLong("cpucore", "interClusterLat",gproc->getId()))
#ifndef SESC_DDIS
  ,WakeUpDelay(SescConf->getLong(clusterName, "wakeupDelay"))
#endif
  ,SchedDelay(SescConf->getLong(clusterName, "schedDelay"))
  ,RegFileDelay(SescConf->getLong("cpucore", "regFileDelay"))
  ,InOrderCore(SescConf->getBool("cpucore","inorder",gproc->getId()))
#ifdef SESC_DDIS
  ,ddis(gproc->getId(),clusterName)
#endif
{
  char cadena[100];
  sprintf(cadena,"Proc(%d)_%s", Id, clusterName);
  
  resultBusEnergy = new GStatsEnergy("resultBusEnergy", cadena , Id, ResultBusEnergy
				     ,EnergyMgr::get("resultBusEnergy",Id));
  
  forwardBusEnergy = new GStatsEnergy("forwardBusEnergy", cadena , Id, ForwardBusEnergy
				      ,EnergyMgr::get("forwardBusEnergy",Id));

  windowSelEnergy  = new GStatsEnergy("windowSelEnergy",cadena, Id, WindowSelEnergy
				      ,EnergyMgr::get("windowSelEnergy",Id));

  windowRdWrEnergy = new GStatsEnergy("windowRdWrEnergy", cadena , Id, WindowRdWrEnergy
				      ,EnergyMgr::get("windowRdWrEnergy",Id));
  
  windowCheckEnergy = new GStatsEnergy("windowCheckEnergy", cadena, Id, WindowCheckEnergy
				       ,EnergyMgr::get("windowCheckEnergy",Id));

#ifndef SESC_DDIS
  sprintf(cadena,"Proc(%d)_%s_wakeUp", Id, clusterName);
  wakeUpPort = PortGeneric::create(cadena
				 ,SescConf->getLong(clusterName, "wakeUpNumPorts")
				 ,SescConf->getLong(clusterName, "wakeUpPortOccp"));

  SescConf->isLong(clusterName, "wakeupDelay");
  SescConf->isBetween(clusterName, "wakeupDelay", 0, 1024);
#endif

  sprintf(cadena,"Proc(%d)_%s_sched", Id, clusterName);
  schedPort = PortGeneric::create(cadena
				  ,SescConf->getLong(clusterName, "SchedNumPorts")
				  ,SescConf->getLong(clusterName, "SchedPortOccp"));

  // Constraints
  SescConf->isLong(clusterName    , "schedDelay");
  SescConf->isBetween(clusterName , "schedDelay", 0, 1024);

  SescConf->isLong("cpucore"    , "interClusterLat",Id);
  SescConf->isBetween("cpucore" , "interClusterLat", 0, 1024,Id);

  SescConf->isLong("cpucore"    , "regFileDelay");
  SescConf->isBetween("cpucore" , "regFileDelay", 0, 1024);

  RAT = gproc->getRAT();

#ifdef SESC_DDIS
  if (nDeps[0] == 0) {
    nDeps[0] = new GStatsCntr("DepWindow:nDeps_0");
    nDeps[1] = new GStatsCntr("DepWindow:nDeps_1");
    nDeps[2] = new GStatsCntr("DepWindow:nDeps_2");
  }
#endif
}

DepWindow::~DepWindow()
{
}

bool DepWindow::canIssue(DInst *dinst) const
{
  const Instruction *inst = dinst->getInst();

  if( InOrderCore ) {
    if(RAT[inst->getSrc1()] != 0 || RAT[inst->getSrc2()] != 0) {
      return false;
    }
  }
#ifdef SESC_DDIS
  bool can = true;
  if (RAT[inst->getSrc1()])
     can = ddis.canIssue(RAT[inst->getSrc1()]);
  if (can && RAT[inst->getSrc2()])
     can = ddis.canIssue(RAT[inst->getSrc2()]);
  return can;
#else
  return true;
#endif
}

void DepWindow::addInst(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();

  I(dinst->getResource() != 0); // Resource::schedule must set the resource field

  if(!dinst->isSrc2Ready()) {
    // It already has a src2 dep. It means that it is solved at
    // retirement (Memory consistency. coherence issues)
    if( RAT[inst->getSrc1()] )
      RAT[inst->getSrc1()]->addSrc1(dinst);
  }else{
    if( RAT[inst->getSrc1()] )
      RAT[inst->getSrc1()]->addSrc1(dinst);

    if( RAT[inst->getSrc2()] )
      RAT[inst->getSrc2()]->addSrc2(dinst);
  }

  RAT[inst->getDest()] = dinst;

  windowRdWrEnergy->inc();  // Add entry

#ifdef SESC_DDIS
  ddis.addInst(dinst);

  if (!dinst->hasDeps())
    select(dinst);
#else
  if (!dinst->hasDeps()) {
    dinst->setWakeUpTime(wakeUpPort->nextSlot() + WakeUpDelay);
    select(dinst);
  }
#endif
}

// Look for dependent instructions on the same cluster (do not wakeup,
// just get the time)
void DepWindow::wakeUpDeps(DInst *dinst)
{
  I(!dinst->hasDeps());

#ifdef SESC_DDIS
  ddis.wakeUpDeps(dinst);
#else
  // Even if it does not wakeup instructions the port is used
  Time_t wakeUpTime= wakeUpPort->nextSlot();

  if (!dinst->hasPending())
    return;

  wakeUpTime += WakeUpDelay;

  I(dinst->getResource());
  const Cluster *srcCluster = dinst->getResource()->getCluster();

  I(dinst->hasPending());
  for(const DInstNext *it = dinst->getFirst();
       it ;
       it = it->getNext() ) {
    DInst *dstReady = it->getDInst();

    const Resource *dstRes = dstReady->getResource();

    if (dstRes) {
      // Wakeup schedule only if the instruction is on the same
      // window, and the schedule is latter than the current time (2
      // sources, may try to be wakeUp at different times.
      if (dstRes->getCluster() == srcCluster 
	  && wakeUpTime > dstReady->getWakeUpTime()
	  && dstReady->getWakeUpTime() == 0)
	dstReady->setWakeUpTime(wakeUpTime);
    }
  }
#endif
}

void DepWindow::select(DInst *dinst)
{
  // At the end of the wakeUp, we can start to read the register file
  I(dinst->getWakeUpTime());

  Time_t wakeTime = dinst->getWakeUpTime() + RegFileDelay;

  Time_t schedTime = schedPort->nextSlot();
  Time_t when  = schedTime >= wakeTime ? schedTime : wakeTime;

  when += SchedDelay;

  windowSelEnergy->inc();

  dinst->doAtSimTimeCB.scheduleAbs(when);
}

// Called when dinst finished execution. Look for dependent to wakeUp
void DepWindow::executed(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();

  //  MSG("execute [0x%x] @%lld",dinst, globalClock);

  I(!dinst->hasDeps());

  RegType dest = inst->getDest();
  if(RAT[dest] == dinst)
    RAT[dest] = 0;

  resultBusEnergy->inc();
  windowCheckEnergy->inc();
  windowRdWrEnergy->inc();  // Remove the entry

  if (!dinst->hasPending())
    return;

  // Maybe the dependent instructions were pre-scheduled, and then
  // another instruction arrived. This means that we have to
  // wakeup the dependents again (just in case)
  wakeUpDeps(dinst);

  I(!InOrderCore);

  I(dinst->getResource());
  const Cluster *srcCluster = dinst->getResource()->getCluster();

  // Only until reaches last. The instructions that are from another processor
  // should be added again to the dependence chain so that MemRequest::ack can
  // awake them (other processor instructions)

  const DInst *stopAtDst = 0;
  
  while (dinst->hasPending()) {

    if (stopAtDst == dinst->getFirstPending())
      break;
    DInst *dstReady = dinst->getNextPending();
    I(dstReady);

#ifdef SESC_DDIS
    int count = (dstReady->isSrc1Ready() ? 0 : 1) + (dstReady->isSrc2Ready() ? 0 : 1);
    nDeps[count]->inc();
#endif

    if (!dstReady->isIssued()) {
      I(dinst->getInst()->isStore());

      // Accross processor dependence
      if (dstReady->hasDepsAtRetire())
	dstReady->clearDepsAtRetire();
      
      I(!dstReady->hasDeps());
      continue;
    }
    if (dstReady->isExecuted()) {
      // The instruction got executed even though it has dependences. This is
      // because the instruction got silently killed (killSilently)
      if (!dstReady->hasDeps())
	dstReady->scrap();
      continue;
    }

    if (dstReady->hasDepsAtRetire() && dinst->getInst()->isStore()) {
      // Means that there was a memory dependence between this two memory
      // access, and the they are performed in different processors

      I(dstReady->isSrc2Ready());
      I(dstReady->getInst()->isLoad());
      I(LDSTBuffer::calcWord(dinst) == LDSTBuffer::calcWord(dstReady));

      LOG("across cluster dependence enforcement pc=0x%x [addr=0x%x] vs pc=0x%x [addr=0x%x]"
	  ,(int)dinst->getInst()->getAddr()   , (int)dinst->getVaddr()
	  ,(int)dstReady->getInst()->getAddr(), (int)dstReady->getVaddr());

      dinst->addSrc2(dstReady); // Requeue the instruction at the end

      if (stopAtDst == 0)
	stopAtDst = dstReady;
      continue;
    }
    GI(dstReady->hasDepsAtRetire(),!dstReady->isSrc2Ready());

    if (!dstReady->hasDeps()) {
      // Check dstRes because dstReady may not be issued
      I(dstReady->getResource());
      const Cluster *dstCluster = dstReady->getResource()->getCluster();
      I(dstCluster);

      if (dstCluster != srcCluster) {
	forwardBusEnergy->inc();

	// FIXME: get call wakeUpDeps for remote cluster.
	Time_t when = wakeUpPort->nextSlot() + InterClusterLat;
	dstReady->setWakeUpTime(when);
      }

      select(dstReady);
    }
  }
}

