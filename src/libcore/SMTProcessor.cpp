/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Milos Prvulovic

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

#include "SMTProcessor.h"

#include "SescConf.h"

#include "FetchEngine.h"
#include "ExecutionFlow.h"

SMTProcessor::Fetch::Fetch(GMemorySystem *gm, CPU_t cpuID, int cid, FetchEngine *fe)
  : IFID(cpuID, cid, gm)
  ,pipeQ(cpuID)
{
}

SMTProcessor::Fetch::~Fetch()
{
}

SMTProcessor::SMTProcessor(GMemorySystem *gm, CPU_t i)
  :GProcessor(gm,i,SescConf->getLong("cpucore", "smtContexts",i))
  ,smtContexts(SescConf->getLong("cpucore", "smtContexts",i))
  ,smtFetchs4Clk(SescConf->getLong("cpucore", "smtFetchs4Clk",i))
  ,smtDecodes4Clk(SescConf->getLong("cpucore", "smtDecodes4Clk",i))
  ,smtIssues4Clk(SescConf->getLong("cpucore", "smtIssues4Clk",i))
{
  SescConf->isLong("cpucore", "smtContexts",Id);
  SescConf->isGT("cpucore", "smtContexts", 1,Id);

  SescConf->isLong("cpucore", "smtFetchs4Clk",Id);
  SescConf->isBetween("cpucore", "smtFetchs4Clk", 1,smtContexts,Id);

  SescConf->isLong("cpucore", "smtDecodes4Clk",Id);
  SescConf->isBetween("cpucore", "smtDecodes4Clk", 1,smtContexts,Id);

  SescConf->isLong("cpucore", "smtIssues4Clk",Id);
  SescConf->isBetween("cpucore", "smtIssues4Clk", 1,smtContexts,Id);

  flow.resize(smtContexts);

  Fetch *f = new Fetch(gm, Id, Id*smtContexts);
  flow[0] = f;
  for(int i = 1; i < smtContexts; i++) {
    flow[i] = new Fetch(gm, Id,Id*smtContexts+i, &(f->IFID));
  }

  cFetchId =0;
  cDecodeId=0;
  cIssueId =0;

  spaceInInstQueue = InstQueueSize;
}

SMTProcessor::~SMTProcessor()
{
  for(FetchContainer::iterator it = flow.begin();
      it != flow.end();
      it++) {
    delete *it;
  }
}

SMTProcessor::Fetch *SMTProcessor::findFetch(Pid_t pid) const
{
  for(FetchContainer::const_iterator it = flow.begin();
      it != flow.end();
      it++) {
    if( (*it)->IFID.getPid() == pid ) {
      return *it;
    }
  }
  return 0;
}

FetchEngine *SMTProcessor::currentFlow()
{
  return &flow[cFetchId]->IFID;
}

void SMTProcessor::saveThreadContext(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);
  I(fetch);
  I(fetch->IFID.getPid()==pid);
  fetch->IFID.saveThreadContext();
}

void SMTProcessor::loadThreadContext(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);
  I(fetch);
  I(fetch->IFID.getPid()==pid);
  fetch->IFID.loadThreadContext();
}

ThreadContext *SMTProcessor::getThreadContext(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);
  I(fetch);
  I(fetch->IFID.getPid()==pid);
  return fetch->IFID.getThreadContext();
}

void SMTProcessor::setInstructionPointer(Pid_t pid, icode_ptr picode)
{
  Fetch *fetch = findFetch(pid);
  I(fetch);
  
  fetch->IFID.setInstructionPointer(picode);
}

icode_ptr SMTProcessor::getInstructionPointer(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);
  I(fetch);

  return fetch->IFID.getInstructionPointer();;
}

void SMTProcessor::switchIn(Pid_t pid)
{
  for(FetchContainer::iterator it = flow.begin();
      it != flow.end();
      it++) {
    if( (*it)->IFID.getPid() < 0 ) {
      // Free FetchEngine
      (*it)->IFID.switchIn(pid);
      return;
    }
  }

  I(0); // No free entries??
}

void SMTProcessor::switchOut(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);

  I(fetch); // Not found??

  fetch->IFID.switchOut(pid);
}

size_t SMTProcessor::availableFlows() const 
{
  size_t freeEntries=0;
  
  for(FetchContainer::const_iterator it = flow.begin();
      it != flow.end();
      it++) {
    if( (*it)->IFID.getPid() < 0 )
      freeEntries++;
  }

  return freeEntries;
}

long long SMTProcessor::getAndClearnGradInsts(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);

  I(fetch); // Not found??

  return fetch->IFID.getAndClearnGradInsts();
}

long long SMTProcessor::getAndClearnWPathInsts(Pid_t pid)
{
  Fetch *fetch = findFetch(pid);

  I(fetch); // Not found??

  return fetch->IFID.getAndClearnWPathInsts();
}

Pid_t SMTProcessor::findVictimPid() const
{
  // TODO: look for a Pid (random?)
  return flow[cFetchId]->IFID.getPid();
}

void SMTProcessor::goRabbitMode(long long n2Skip)
{
  flow[cFetchId]->IFID.goRabbitMode(n2Skip);
}

void SMTProcessor::selectFetchFlow()
{
  // ROUND-ROBIN POLICY
  for(int i=0;i<smtContexts;i++){
    cFetchId = (cFetchId+1) % smtContexts;
    if( flow[cFetchId]->IFID.getPid() >= 0 )
      return;
  }

  cFetchId = -1;
  I(hasWork());
}

void SMTProcessor::selectDecodeFlow()
{
  // ROUND-ROBIN POLICY
  cDecodeId = (cDecodeId+1) % smtContexts;
}

void SMTProcessor::selectIssueFlow()
{
  // ROUND-ROBIN POLICY
  cIssueId = (cIssueId+1) % smtContexts;
}

void SMTProcessor::advanceClock()
{
  clockTicks++;
  
  // Fetch Stage
  for(int i=0;i<smtFetchs4Clk;i++) {
    selectFetchFlow();
    if (cFetchId >=0) {
      I(flow[cFetchId]->IFID.hasWork());

      IBucket *bucket = flow[cFetchId]->pipeQ.pipeLine.newItem();
      if( bucket ) {
	flow[cFetchId]->IFID.fetch(bucket);
	// readyItem would be called once the bucket is fetched
      }
    }else{
      I(!flow[0]->IFID.hasWork());
    }
  }

  // ID Stage (insert to instQueue)
  for(int i=0;i<smtDecodes4Clk && spaceInInstQueue >= FetchWidth ;i++) {
    selectDecodeFlow();
    
    IBucket *bucket = flow[cDecodeId]->pipeQ.pipeLine.nextItem();
    if( bucket ) {
      I(!bucket->empty());
      
      spaceInInstQueue -= bucket->size();
      
      flow[cDecodeId]->pipeQ.instQueue.push(bucket);
    }
  }

  // RENAME Stage
  int totalIssuedInsts=0;
  for(int i=0;i<smtIssues4Clk && totalIssuedInsts < IssueWidth ;i++) {
    selectIssueFlow();
    
    if( flow[cIssueId]->pipeQ.instQueue.empty() )
      continue;
    
    int issuedInsts = issue(flow[cIssueId]->pipeQ);
    
    totalIssuedInsts += issuedInsts;
  }
  spaceInInstQueue += totalIssuedInsts;
  
  retire();
}

bool SMTProcessor::hasWork() const 
{
  if (!ROB.empty())
    return true;

  for(FetchContainer::const_iterator it = flow.begin();
      it != flow.end();
      it++) {
    if ((*it)->IFID.hasWork() || (*it)->pipeQ.hasWork())
      return true;
  }
  
  return false;
}

#ifdef SESC_MISPATH
void SMTProcessor::misBranchRestore(DInst *dinst)
{
  clusterManager.misBranchRestore();

  for (unsigned i = 0 ; i < INSTRUCTION_MAX_DESTPOOL; i++)
    misRegPool[i] = 0;

  for(FetchContainer::const_iterator it = flow.begin();
      it != flow.end();
      it++) {
    if( &(*it)->IFID == dinst->getFetch() ) {
      (*it)->pipeQ.pipeLine.cleanMark();
      break;
    }
  }

  // TODO: try to remove from ROB
}
#endif
