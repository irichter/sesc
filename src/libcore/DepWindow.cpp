/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of California Santa Cruz

   Contributed by Jose Renau
                  Basilio Fraguela
                  Smruti Sarangi
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

#include "DepWindow.h"

#include "DInst.h"
#include "LDSTBuffer.h"
#include "Resource.h"
#include "SescConf.h"
#include "GProcessor.h"

DepWindow::DepWindow(GProcessor *gp, const char *clusterName)
  :gproc(gp)
  ,Id(gp->getId())
  ,InOrderCore(SescConf->getBool("cpucore","inorder",gp->getId()))
  ,InterClusterLat(SescConf->getLong("cpucore", "interClusterLat",gp->getId()))
  ,WakeUpDelay(SescConf->getLong(clusterName, "wakeupDelay"))
  ,SchedDelay(SescConf->getLong(clusterName, "schedDelay"))
  ,RegFileDelay(SescConf->getLong("cpucore", "regFileDelay"))
{
  char cadena[100];
  sprintf(cadena,"Proc(%d)_%s", Id, clusterName);
  
  resultBusEnergy = new GStatsEnergy("resultBusEnergy", cadena , Id, ResultBusEnergy
                                     ,EnergyMgr::get("resultBusEnergy",Id));
  
  forwardBusEnergy = new GStatsEnergy("forwardBusEnergy", cadena , Id, ForwardBusEnergy
                                      ,EnergyMgr::get("forwardBusEnergy",Id));

 

#ifdef SESC_INORDER
  windowSelEnergyOutOrder  = new GStatsEnergy("windowSelEnergy",cadena, Id, WindowSelEnergy
                                      ,EnergyMgr::get("windowSelEnergy",Id));

  windowRdWrEnergyOutOrder = new GStatsEnergy("windowRdWrEnergy", cadena , Id, WindowRdWrEnergy
                                      ,EnergyMgr::get("windowRdWrEnergy",Id));
  
  windowCheckEnergyOutOrder = new GStatsEnergy("windowCheckEnergy", cadena, Id, WindowCheckEnergy
                                       ,EnergyMgr::get("windowCheckEnergy",Id));

  windowSelEnergyInOrder  = new GStatsEnergyNull();
  windowRdWrEnergyInOrder = new GStatsEnergyNull();
  windowCheckEnergyInOrder = new GStatsEnergyNull();
                                     
  windowSelEnergy   = windowSelEnergyOutOrder;
  windowRdWrEnergy  = windowRdWrEnergyOutOrder;
  windowCheckEnergy = windowCheckEnergyOutOrder;

  InOrderMode = true;
  OutOrderMode = false;
  currentMode = OutOrderMode;

#else
  windowSelEnergy  = new GStatsEnergy("windowSelEnergy",cadena, Id, WindowSelEnergy
                                      ,EnergyMgr::get("windowSelEnergy",Id));

  windowRdWrEnergy = new GStatsEnergy("windowRdWrEnergy", cadena , Id, WindowRdWrEnergy
                                      ,EnergyMgr::get("windowRdWrEnergy",Id));
  
  windowCheckEnergy = new GStatsEnergy("windowCheckEnergy", cadena, Id, WindowCheckEnergy
                                       ,EnergyMgr::get("windowCheckEnergy",Id));
#endif


  sprintf(cadena,"Proc(%d)_%s_wakeUp", Id, clusterName);
  wakeUpPort = PortGeneric::create(cadena
                                 ,SescConf->getLong(clusterName, "wakeUpNumPorts")
                                 ,SescConf->getLong(clusterName, "wakeUpPortOccp"));

  SescConf->isLong(clusterName, "wakeupDelay");
  SescConf->isBetween(clusterName, "wakeupDelay", 0, 1024);

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
}

DepWindow::~DepWindow()
{
}

#ifdef SESC_INORDER
void DepWindow::setMode(bool mode)
{
  if(currentMode == mode)
    return;

  currentMode = mode;

  if(mode == InOrderMode){
    windowSelEnergy   = windowSelEnergyInOrder;
    windowRdWrEnergy  = windowRdWrEnergyInOrder;
    windowCheckEnergy = windowCheckEnergyInOrder;
  }else{
    I(currentMode == OutOrderMode);

    windowSelEnergy   = windowSelEnergyOutOrder;
    windowRdWrEnergy  = windowRdWrEnergyOutOrder;
    windowCheckEnergy = windowCheckEnergyOutOrder;
  }
}
#endif


StallCause DepWindow::canIssue(DInst *dinst) const 
{ 

  return NoStall;
}

void DepWindow::addInst(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();
  
  I(dinst->getResource() != 0); // Resource::schedule must set the resource field

  windowRdWrEnergy->inc();  // Add entry



  if (!dinst->hasDeps()) {
    dinst->setWakeUpTime(wakeUpPort->nextSlot() + WakeUpDelay);
    preSelect(dinst);
  }
}

// Look for dependent instructions on the same cluster (do not wakeup,
// just get the time)
void DepWindow::wakeUpDeps(DInst *dinst)
{
  I(!dinst->hasDeps());

  windowRdWrEnergy->inc();  // check deps

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
    
    if (!dstRes)
      continue;


    if (dstRes->getCluster() == srcCluster && dstReady->getWakeUpTime() < wakeUpTime)
      dstReady->setWakeUpTime(wakeUpTime);
  }
}

void DepWindow::preSelect(DInst *dinst)
{
  // At the end of the wakeUp, we can start to read the register file
  I(dinst->getWakeUpTime());

  Time_t wakeTime = dinst->getWakeUpTime() + RegFileDelay;

  IS(dinst->setWakeUpTime(0));

  if (wakeTime > globalClock) {
    dinst->doAtSelectCB.scheduleAbs(wakeTime);
  }else{
    select(dinst);
  }
}

void DepWindow::select(DInst *dinst)
{
  I(!dinst->getWakeUpTime());

#ifdef SESC_BAAD
  dinst->setIssueTime();
#endif

  Time_t schedTime = schedPort->nextSlot() + SchedDelay;

  windowSelEnergy->inc();

  dinst->doAtSimTimeCB.scheduleAbs(schedTime);
}

// Called when dinst finished execution. Look for dependent to wakeUp
void DepWindow::executed(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();

#ifdef SESC_BAAD
  dinst->setExeTime();
#endif

  //  MSG("execute [0x%x] @%lld",dinst, globalClock);

  I(!dinst->hasDeps());

  resultBusEnergy->inc();
  windowCheckEnergy->inc();
  windowRdWrEnergy->inc();  // Remove the entry

  if (!dinst->hasPending())
    return;

  if (dinst->isStallOnLoad())
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

      LOG("across cluster dependence enforcement pc=0x%x [addr=0x%x] vs pc=0x%x [addr=0x%x]"
          ,(int)dinst->getInst()->getAddr()   , (int)dinst->getVaddr()
          ,(int)dstReady->getInst()->getAddr(), (int)dstReady->getVaddr());

      dinst->addFakeSrc(dstReady); // Requeue the instruction at the end

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

      if (dstCluster != srcCluster)
        forwardBusEnergy->inc();

      Time_t when = wakeUpPort->nextSlot() + InterClusterLat;
      dstReady->setWakeUpTime(when);

      preSelect(dstReady);
    }

  }
}

