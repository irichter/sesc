/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  James Tuck
                  Milos Prvulovic
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

#include "SescConf.h"

#include "Processor.h"

#include "FetchEngine.h"
#include "ExecutionFlow.h"
#include "OSSim.h"


//#define SESC_INORDER

Processor::Processor(GMemorySystem *gm, CPU_t i)
  :GProcessor(gm, i, 1)
  ,IFID(i, i, gm)
   ,pipeQ(i)
{
  spaceInInstQueue = InstQueueSize;

  bzero(RAT,sizeof(DInst*)*NumArchRegs);

#ifdef SESC_INORDER
  bzero(RATTIME,sizeof(Time_t*)*NumArchRegs);

  latencyVal[iALU]       = SescConf->getLong("FXClusterIssueX", "iALULat");    
  latencyVal[iMult]      = SescConf->getLong("FXClusterIssueX", "iMultLat");          
  latencyVal[iDiv]       = SescConf->getLong("FXClusterIssueX", "iDivLat");          
  latencyVal[iBJ]        = SescConf->getLong("FXClusterIssueX", "iBJLat");        
  latencyVal[iLoad]      = SescConf->getLong("FXClusterIssueX", "iLoadLat");        
  latencyVal[iStore]     = SescConf->getLong("FXClusterIssueX", "iStorelat");       
  latencyVal[fpALU]      = SescConf->getLong("FPClusterIssueX", "fpALULat");        
  latencyVal[fpMult]     = SescConf->getLong("FPClusterIssueX", "fpMultLat");       
  latencyVal[fpDiv]      = SescConf->getLong("FPClusterIssueX", "fpDivLat");         
#endif

}

Processor::~Processor()
{
  // Nothing to do
}

DInst **Processor::getRAT(const DInst *dinst)
{
  return RAT;
}

FetchEngine *Processor::currentFlow()
{
  return &IFID;
}

void Processor::saveThreadContext(Pid_t pid)
{
  I(IFID.getPid()==pid);
  IFID.saveThreadContext();
}

void Processor::loadThreadContext(Pid_t pid)
{
  I(IFID.getPid()==pid);
  IFID.loadThreadContext();
}

ThreadContext *Processor::getThreadContext(Pid_t pid)
{
  I(IFID.getPid()==pid);
  return IFID.getThreadContext();
}

void Processor::setInstructionPointer(Pid_t pid, icode_ptr picode)
{
  I(IFID.getPid()==pid);
  IFID.setInstructionPointer(picode);
}

icode_ptr Processor::getInstructionPointer(Pid_t pid)
{
  I(IFID.getPid()==pid);
  return IFID.getInstructionPointer();  
}

void Processor::switchIn(Pid_t pid)
{
  GLOG(DEBUG2,"Processor(%ld):switchIn %d", Id, pid);
    
  IFID.switchIn(pid);
}

void Processor::switchOut(Pid_t pid)
{
  GLOG(DEBUG2,"Processor(%ld):switchOut %d", Id, pid);

  IFID.switchOut(pid);
}

size_t Processor::availableFlows() const 
{
  return IFID.getPid() < 0 ? 1 : 0;
}

long long Processor::getAndClearnGradInsts(Pid_t pid)
{
  I(IFID.getPid() == pid);
  
  return IFID.getAndClearnGradInsts();
}

long long Processor::getAndClearnWPathInsts(Pid_t pid)
{
  I(IFID.getPid() == pid);
  
  return IFID.getAndClearnWPathInsts();
}

Pid_t Processor::findVictimPid() const
{
  return IFID.getPid();
}

void Processor::goRabbitMode(long long n2Skip)
{
  IFID.goRabbitMode(n2Skip);
}

void Processor::advanceClock()
{
  clockTicks++;
  
  //  GMSG(!ROB.empty(),"robTop %d Ul %d Us %d Ub %d",ROB.getIdFromTop(0)
  //       ,unresolvedLoad, unresolvedStore, unresolvedBranch);

  // Fetch Stage
  if (IFID.hasWork()) {
    IBucket *bucket = pipeQ.pipeLine.newItem();
    if( bucket ) {
      IFID.fetch(bucket);
      // readyItem would be called once the bucket is fetched
    }
  }

  // ID Stage (insert to instQueue)
  if ( spaceInInstQueue >= FetchWidth ) {
    IBucket *bucket = pipeQ.pipeLine.nextItem();
    if( bucket ) {
      I(!bucket->empty());
      //      I(bucket->top()->getInst()->getAddr());
      
      spaceInInstQueue -= bucket->size();
      
      pipeQ.instQueue.push(bucket);
    }
  }

  // RENAME Stage
  if ( !pipeQ.instQueue.empty() ) {
    spaceInInstQueue += issue(pipeQ);
  }
  
  retire();
}

StallCause Processor::addInst(DInst *dinst) 
{
  const Instruction *inst = dinst->getInst();

#ifdef SESC_INORDER
  if (InOrderCore) {
    // Update the RATTIME for the destiantion register
    Time_t newtime = globalClock;
    newtime += latencyVal[inst->getOpcode()];
    
    if (RAT[inst->getSrc1()] != 0) {
     const Instruction *tempinst = RAT[inst->getSrc1()]->getInst();
     if (tempinst->getOpcode() == iLoad)
       return SmallWinStall;
    }	
      
    if (RAT[inst->getSrc2()] != 0) {
      const Instruction  *tempinst = RAT[inst->getSrc2()]->getInst();
     if (tempinst->getOpcode() == iLoad)
       return SmallWinStall;
    }

    if (RAT[inst->getDest()] != 0) {
     const Instruction  *tempinst = RAT[inst->getDest()]->getInst();
     if(tempinst->getOpcode() == iLoad)
       return SmallWinStall;
    } 
    
    if (RATTIME[inst->getSrc1()] > globalClock
       || RATTIME[inst->getSrc2()] > globalClock
       || RATTIME[inst->getDest()] > globalClock 
	) {
      I(RAT[inst->getSrc1()] != 0 || RAT[inst->getSrc2()] != 0 || RAT[inst->getDest()] != 0);
      return SmallWinStall;
    }

    // No Stalls occured  
    RATTIME[inst->getDest()] = newtime;
  } 
 
#else
  if (InOrderCore) {
    if(RAT[inst->getSrc1()] != 0 || RAT[inst->getSrc2()] != 0 
       || RAT[inst->getDest()] != 0
       ) {
      return SmallWinStall;
    }
  }
#endif

  StallCause sc = sharedAddInst(dinst);
  if (sc != NoStall)
    return sc;

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

  dinst->setRATEntry(&RAT[inst->getDest()]);

  RAT[inst->getDest()] = dinst;

  I(dinst->getResource());
  dinst->getResource()->getCluster()->addInst(dinst);

  return NoStall;
}

bool Processor::hasWork() const 
{
  return IFID.hasWork() || !ROB.empty() || pipeQ.hasWork();
}

#ifdef SESC_MISPATH
void Processor::misBranchRestore(DInst *dinst)
{
  clusterManager.misBranchRestore();

  for (unsigned i = 0 ; i < INSTRUCTION_MAX_DESTPOOL; i++)
    misRegPool[i] = 0;

  I(dinst->getFetch() == &IFID);

  pipeQ.pipeLine.cleanMark();

  // TODO: try to remove from ROB
}
#endif
