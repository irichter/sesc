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

DepWindow::DepWindow(int i, const char *clusterName)
  :Id(i)
  ,InterClusterLat(SescConf->getLong("cpucore", "interClusterLat",i))
  ,WakeUpDelay(SescConf->getLong(clusterName, "wakeupDelay"))
  ,SchedDelay(SescConf->getLong(clusterName, "schedDelay"))
  ,InOrderCore(SescConf->getBool("cpucore","inorder",i))
{
  char cadena[100];
  sprintf(cadena,"Proc(%d)_%s", i, clusterName);
  
  renameEnergy = new GStatsEnergy("renameEnergy", cadena, i, RenameEnergy
				  ,EnergyMgr::get("renameEnergy",i));

  resultBusEnergy = new GStatsEnergy("resultBusEnergy", cadena , i, ResultBusEnergy
				     ,EnergyMgr::get("resultBusEnergy",i));
  
  forwardBusEnergy = new GStatsEnergy("forwardBusEnergy", cadena , i, ForwardBusEnergy
				      ,EnergyMgr::get("forwardBusEnergy",i));

  windowPregEnergy = new GStatsEnergy("windowPregEnergy", cadena , i, WindowPregEnergy
				      ,EnergyMgr::get("windowPregEnergy",i));
  
  wakeupEnergy = new GStatsEnergy("wakeupEnergy", cadena, i, WakeupEnergy
				  ,EnergyMgr::get("wakeupEnergy",i));


  sprintf(cadena,"Proc(%d)_%s_wakeUp", i, clusterName);
  wakeUpPort = PortGeneric::create(cadena
				 ,SescConf->getLong(clusterName, "wakeUpNumPorts")
				 ,SescConf->getLong(clusterName, "wakeUpPortOccp"));

  sprintf(cadena,"Proc(%d)_%s_sched", i, clusterName);
  schedPort = PortGeneric::create(cadena
				  ,SescConf->getLong(clusterName, "SchedNumPorts")
				  ,SescConf->getLong(clusterName, "SchedPortOccp"));

  // Constraints
  SescConf->isLong(clusterName, "wakeupDelay");
  SescConf->isBetween(clusterName, "wakeupDelay", 1, 64);

  SescConf->isLong(clusterName    , "schedDelay");
  SescConf->isBetween(clusterName , "schedDelay", 0, 1024);

  SescConf->isLong("cpucore"    , "interClusterLat",Id);
  SescConf->isBetween("cpucore" , "interClusterLat", 0, 1024,Id);

  bzero(RAT, sizeof(DInst *) * NumArchRegs);
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


  return true;
}


void DepWindow::addInst(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();

  renameEnergy->inc();

  I(dinst->getResource() != 0); // Resource::schedule must set the resource field

  if( RAT[inst->getSrc1()] ) {
    RAT[inst->getSrc1()]->addSrc1(dinst);
    if( RAT[inst->getSrc2()] ) {
      RAT[inst->getSrc2()]->addSrc2(dinst);
    }
    RAT[inst->getDest()] = dinst;
    return;
  }

  if(!dinst->isSrc2Ready()) {
    RAT[inst->getDest()] = dinst;
    return;
  }

  // src2 may not be ready for memory ops See LDSTBuffer
  if( RAT[inst->getSrc2()] ) {
    RAT[inst->getSrc2()]->addSrc2(dinst);
    RAT[inst->getDest()] = dinst;
    return;
  }

  RAT[inst->getDest()] = dinst;
  I(!dinst->hasDeps());
  
  dinst->doAtSimTimeCB.scheduleAbs(wakeUpPort->nextSlot() + WakeUpDelay);
}

void DepWindow::simTimeAck(DInst *dinst) 
{
  const Instruction *inst = dinst->getInst();

  I(!dinst->hasDeps());

  RegType dest = inst->getDest();
  if(RAT[dest] == dinst)
    RAT[dest] = 0;

  resultBusEnergy->inc();
  windowPregEnergy->inc();
  wakeupEnergy->inc();

  Time_t wakeUpTime = wakeUpPort->nextSlot();

  if (!dinst->hasPending())
    return;

  I(!InOrderCore);

  Resource *srcRes = dinst->getResource();
  I(srcRes);
  const Cluster *srcCluster = srcRes->getCluster();

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

    Time_t schedTime = schedPort->nextSlot();
    schedTime = schedTime > wakeUpTime ? schedTime : wakeUpTime; // max sched, wakeUp
#ifdef BFWIN
    Time_t bfwinTime = bfwin.wakeUp(dstReady);
    schedTime = schedTime > bfwinTime ? schedTime : bfwinTime; // max time
#endif

    if (!dstReady->hasDeps()) {
      // Check dstRes because dstReady may not be issued
      Resource *dstRes = dstReady->getResource();
      I(dstRes);
      const Cluster *dstCluster = dstRes->getCluster();
      I(dstCluster);

      if (dstCluster == srcCluster) {
	schedTime += SchedDelay;
      }else{
	schedTime += InterClusterLat;	// Forward data to another cluster
	forwardBusEnergy->inc();
      }

      dstReady->doAtSimTimeCB.scheduleAbs(schedTime);
    }
  }
}

