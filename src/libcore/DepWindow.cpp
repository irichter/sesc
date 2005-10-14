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
#ifdef SESC_SEED
  ,Banks(SescConf->getLong(clusterName,"banks"))
  ,DepTableEntries(SescConf->getLong(clusterName, "DepTableEntries"))
  ,DepTableNumPorts(SescConf->getLong(clusterName,"DepTableNumPorts"))
  ,DepTableDelay(SescConf->getLong(clusterName,   "DepTableDelay"))
#ifdef SESC_SEED_OVERFLOW
  ,MaxOverflowing(SescConf->getLong(clusterName,  "MaxOverflowing"))
  ,MaxUnderflowing(SescConf->getLong(clusterName, "MaxUnderflowing"))
  ,nOverflowing(0)
#endif
  ,depPred(gp->getId(),"depPred", SescConf->getLong(clusterName, "depPredSize"), 2)
  ,nDepsCorrect("Proc(%d)_%s_depTable:nDepsCorrect", gp->getId(), clusterName)
  ,nDepsMiss("Proc(%d)_%s_depTable:nDepsMiss", gp->getId(), clusterName)
  ,nDepsOverflow("Proc(%d)_%s_depTable:nDepsOverflow", gp->getId(), clusterName)
  ,nDepsUnderflow("Proc(%d)_%s_depTable:nDepsUnderflow", gp->getId(), clusterName)
#endif
{
  char cadena[100];
  sprintf(cadena,"Proc(%d)_%s", Id, clusterName);
  
  resultBusEnergy = new GStatsEnergy("resultBusEnergy", cadena , Id, IssuePower
                                     ,EnergyMgr::get("resultBusEnergy",Id));
  
  forwardBusEnergy = new GStatsEnergy("forwardBusEnergy", cadena , Id, IssuePower
                                      ,EnergyMgr::get("forwardBusEnergy",Id));

 

#ifdef SESC_INORDER
  windowSelEnergyOutOrder  = new GStatsEnergy("windowSelEnergy",cadena, Id, IssuePower
                                      ,EnergyMgr::get("windowSelEnergy",Id));

  windowRdWrEnergyOutOrder = new GStatsEnergy("windowRdWrEnergy", cadena , Id, IssuePower
                                      ,EnergyMgr::get("windowRdWrEnergy",Id));
  
  windowCheckEnergyOutOrder = new GStatsEnergy("windowCheckEnergy", cadena, Id, IssuePower
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
  windowSelEnergy  = new GStatsEnergy("windowSelEnergy",cadena, Id, IssuePower
                                      ,EnergyMgr::get("windowSelEnergy",Id));

  windowRdWrEnergy = new GStatsEnergy("windowRdWrEnergy", cadena , Id, IssuePower
                                      ,EnergyMgr::get("windowRdWrEnergy",Id));
  
  windowCheckEnergy = new GStatsEnergy("windowCheckEnergy", cadena, Id, IssuePower
                                       ,EnergyMgr::get("windowCheckEnergy",Id));
#endif

#ifdef SESC_SEED
  depTableEnergy = new GStatsEnergy("depTableEnergy", cadena, Id, IssuePower
                                    ,EnergyMgr::get("depTableEnergy",Id));

  depTablePort = (PortGeneric **)malloc(sizeof(PortGeneric *)*Banks);

  for(int j=0;j<Banks;j++) {
    char name[1024];
  
    sprintf(name,"Proc(%d)_%s_depTable_%d", Id, clusterName, j);
    depTablePort[j] = PortGeneric::create(name, DepTableNumPorts, 1);
  }

  // nDeps counts the #deps pending when the one of the instruction parent
  // finishes.
  nDepsCntr[0]  = new GStatsCntr("Proc(%d)_%s_depTable:nDeps_0", Id, clusterName);
  nDepsCntr[1]  = new GStatsCntr("Proc(%d)_%s_depTable:nDeps_1", Id, clusterName);
  nDepsCntr[2]  = new GStatsCntr("Proc(%d)_%s_depTable:nDeps_2", Id, clusterName);

  addInstBank = 0;

  SescConf->isLong(clusterName    , "DepTableEntries");
  SescConf->isBetween(clusterName , "DepTableEntries", 1, 1024);
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

#ifdef SESC_SEED
void DepWindow::addParentSrcShared(DInst *child, DInst *parent)
{
  I(parent);

  depTableEnergy->add(2); // Queue on parent (read + write)
  depTablePort[parent->getBank() % Banks]->nextSlot();

#ifdef SESC_SEED_OVERFLOW
  if (!child->isOverflowing()) {
    if (parent->getnDepTableEntries() > DepTableEntries) {
      child->setOverflowing();
      nDepsUnderflow.inc();
      nOverflowing++;
    }
  }
#endif
  if (parent->getnDepTableEntries() == DepTableEntries) {
    // Needs an additional bank

    parent->setXtraBank(getBank());

    depTableEnergy->add(2); // Queue on parent (read + write)
    depTablePort[parent->getXtraBank() % Banks]->nextSlot();

    nDepsUnderflow.inc();
  }else{
    I(parent->getnDepTableEntries() <= 2*DepTableEntries);
  }

  parent->addDepTableEntry();
}

void DepWindow::addParentSrc1(DInst *child)
{
  I(child);
  DInst *parent = child->getParentSrc1();
  
  addParentSrcShared(child, parent);
  child->setPredParent(parent, true);
}

void DepWindow::addParentSrc2(DInst *child)
{
  I(child);
  DInst *parent = child->getParentSrc2();
  
  addParentSrcShared(child, parent);
  child->setPredParent(parent, false);
}

bool DepWindow::hasDepTableSpace(const DInst *dinst) const 
{ 
#ifdef SESC_SEED_OVERFLOW
  if (nOverflowing < MaxUnderflowing)
    return true;
#endif

  return dinst->getnDepTableEntries() <= DepTableEntries;
}
#endif

StallCause DepWindow::canIssue(DInst *dinst) const 
{ 
#ifdef SESC_SEED
  const Instruction *inst = dinst->getInst();

  DInst *src1= gproc->getRATEntry(dinst->getContextId(), inst->getSrc1());
  DInst *src2= gproc->getRATEntry(dinst->getContextId(), inst->getSrc2());

  if (src1) {
    // reserve half xtra entry for overflow
    if (src1->getnDepTableEntries() >= (2*DepTableEntries-1))
      return PortConflictStall;
  }

  if (src2) {
    // reserve half xtra entry for overflow
    if (src2->getnDepTableEntries() >= (2*DepTableEntries-1))
        return PortConflictStall;
  }

  long bank = addInstBank; // updated on addInst
  if (depTablePort[bank]->calcNextSlot() > globalClock) {
    bank = (bank+1) % Banks;
    if (depTablePort[bank]->calcNextSlot() > globalClock)
      return PortConflictStall;
  }
  dinst->setBank(bank); 
#endif

  return NoStall;
}

void DepWindow::addInst(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();
  
  I(dinst->getResource() != 0); // Resource::schedule must set the resource field

#ifdef SESC_SEED
  addInstBank = (dinst->getBank()+1) % Banks;

  depTableEnergy->inc();
  Time_t when = depTablePort[dinst->getBank()]->nextSlot();

  if (dinst->hasDeps()) {
    
    I(dinst->getParentSrc1() || dinst->getParentSrc2());

#ifdef SESC_SEED_DEPPRED_SRC
    bool src1 = depPred.predict(inst->currentID());

    if (src1) {
      if (dinst->getParentSrc1()) {
        // MAYBE: predict src1 & waiting src1 (give it a chance)
        addParentSrc1(dinst);
      }else{
        // NO WAY: predict src1 & NOT waiting src1 (prediction will be changed)
        addParentSrc2(dinst);
      }
    }else{
      if (dinst->getParentSrc2()) { 
        // MAYBE: predict src2 & waiting src2 (give it a chance)
        addParentSrc2(dinst);
      }else{
        // NO WAY: predict src2 & waiting src1
        addParentSrc1(dinst);
      }
    }
#else
#ifdef SESC_SEED_DEPPRED_RAND
    if (dinst->getParentSrc1())
      addParentSrc1(dinst);
    else if (dinst->getParentSrc2())
      addParentSrc2(dinst);
#else
#ifdef SESC_SEED_DEPPRED_BOTH
    bool src1 = depPred.predict(inst->currentID());
    if (src1) {
      // predict both
      if (dinst->getParentSrc2())
        addParentSrc2(dinst);
      if (dinst->getParentSrc1())
        addParentSrc1(dinst);
    }else{
      // random
    if (dinst->getParentSrc1())
      addParentSrc1(dinst);
    else if (dinst->getParentSrc2())
      addParentSrc2(dinst);
    }
#else
    if (dinst->getParentSrc1())
      addParentSrc1(dinst);
    if (dinst->getParentSrc2())
      addParentSrc2(dinst);
#endif
#endif
#endif
    I(dinst->getPredParent());
  }else{
    dinst->setWakeUpTime(when+WakeUpDelay);

    dinst->setPredParent(0); // Don't queue, no deps
  }
#endif

#ifdef SESC_SEED
  nDepsCntr[static_cast<int>(dinst->getnDeps())]->inc();
#endif

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


  // Even if it does not wakeup instructions the port is used
  Time_t wakeUpTime= wakeUpPort->nextSlot();

  if (!dinst->hasPending())
    return;

#ifdef SESC_SEED
  // Check for child
  depTableEnergy->inc();
  Time_t when = depTablePort[dinst->getBank()]->nextSlot();

  if (dinst->getnDepTableEntries() > DepTableEntries) {
    depTableEnergy->inc();
    Time_t t2 = depTablePort[dinst->getXtraBank()]->nextSlot();
    when = when >= t2 ?  when : t2;
  }  
  wakeUpTime = when > wakeUpTime ? when : wakeUpTime;
#endif

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

#ifdef SESC_SEED
#ifdef SESC_SEED_INORDER
    if (wakeUpTime < lastWakeUpTime) {
      // in-order send to scheduler
      wakeUpTime = lastWakeUpTime;
    }
    lastWakeUpTime =wakeUpTime;
#endif
#endif

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

  dinst->doAtSimTimeCB.scheduleAbs(schedTime);
}

// Called when dinst finished execution. Look for dependent to wakeUp
void DepWindow::executed(DInst *dinst)
{
  const Instruction *inst = dinst->getInst();

#ifdef SESC_BAAD
  dinst->setExeTime();
#endif

#ifdef SESC_SEED_OVERFLOW
  if (dinst->isOverflowing())
    nOverflowing--;
#endif

  //  MSG("execute [0x%x] @%lld",dinst, globalClock);

  I(!dinst->hasDeps());

  resultBusEnergy->inc();
  windowCheckEnergy->inc();
  windowSelEnergy->inc();
  windowRdWrEnergy->inc();  // Add entry
  windowRdWrEnergy->inc();  // check deps
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
#ifdef SESC_SEED
    // On missprediction:
    //
    // extra energy/access on DepTable (re-queue).
    //
    // Occupy port
#if (defined SESC_SEED_DEPPRED_SRC) or (defined SESC_SEED_DEPPRED_RAND) or (defined SESC_SEED_DEPPRED_BOTH)
    {
      if (dstReady->getPredParent()) {
        if (dstReady->getPredParent() == dinst) {
          nDepsCorrect.inc();
#ifdef SESC_SEED_DEPPRED_BOTH
          depPred.update(dstReady->getInst()->currentID(), false);
#endif
        }else{
#ifdef SESC_SEED_DEPPRED_BOTH
          depPred.update(dstReady->getInst()->currentID(), true);
#else
          // Toggle prediction (it was wrong)
          depPred.update(dstReady->getInst()->currentID(), !dstReady->isPredParentSrc1());
#endif
          
          depTableEnergy->add(2); // Queue on parent (read,write)

#ifdef SESC_SEED_OVERFLOW
          if( !hasDepTableSpace(dinst) && !dstReady->isOverflowing()) {
            
            dstReady->setOverflowing();
            nOverflowing++;
            if (nOverflowing > MaxOverflowing) {
              nDepsOverflow.inc();
            }
          }
#endif
          if (dinst->getnDepTableEntries() < (2*DepTableEntries)) {
            addParentSrcShared(dstReady, dinst);
          }else{
            nDepsOverflow.inc();
            dinst->addDepTableEntry(); // Not really
          }

          nDepsMiss.inc();
        }
        dstReady->setPredParent(0);
      }
    }
#endif
#endif

  }
}

