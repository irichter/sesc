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

#ifndef SMTPROCESSOR_H
#define SMTPROCESSOR_H

#include <vector>
#include <queue>

#include "nanassert.h"

#include "GProcessor.h"
#include "Pipeline.h"
#include "FetchEngine.h"

class SMTProcessor:public GProcessor {
private:
  int cFetchId;
  int cDecodeId;
  int cIssueId;
  
  // how many contexts has the machine
  const int smtContexts;

  // From how many contexts can fetch in a single cycle
  const int smtFetchs4Clk;

  // how many contexts can enter in the instQueue in a single cycle
  const int smtDecodes4Clk;

  // how many contexts can enter in the ExeEngine in a single cycle
  const int smtIssues4Clk;

  class Fetch {
  public:
    Fetch(GMemorySystem *gm, CPU_t cpuID, int cid, FetchEngine *fe=0);
    ~Fetch();

    FetchEngine IFID;
    PipeQueue   pipeQ;
  };

  signed int spaceInInstQueue;

  typedef std::vector<Fetch *> FetchContainer;
  FetchContainer flow;

  Fetch *findFetch(Pid_t pid) const;

  void selectFetchFlow();
  void selectDecodeFlow();
  void selectIssueFlow();

protected:
  // BEGIN VIRTUAL FUNCTIONS of GProcessor
  FetchEngine *currentFlow();

  void saveThreadContext(Pid_t pid);
  void loadThreadContext(Pid_t pid);
  ThreadContext *getThreadContext(Pid_t pid);

  void setInstructionPointer(Pid_t pid, icode_ptr picode);
  icode_ptr getInstructionPointer(Pid_t pid);

  void switchIn(Pid_t pid);
  void switchOut(Pid_t pid);

  size_t  availableFlows() const;

  long long getAndClearnGradInsts(Pid_t pid);
  long long getAndClearnWPathInsts(Pid_t pid);


  void goRabbitMode(long long n2Skip);

  Pid_t findVictimPid() const;
  bool hasWork() const;

  void advanceClock();
  // END VIRTUAL FUNCTIONS of GProcessor

public:
  virtual ~SMTProcessor();
  SMTProcessor(GMemorySystem *gm, CPU_t i);
  
#ifdef SESC_MISPATH
  void misBranchRestore(DInst *dinst);
#endif
};

#endif   // SMTPROCESSOR_H