/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Milos Prvulovic
                  James Tuck
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
#ifndef EXECUTIONFLOW_H
#define EXECUTIONFLOW_H

#include "nanassert.h"

#include "GFlow.h"
#include "Events.h"
#include "callback.h"
#include "DInst.h"
#include "Snippets.h"
#include "ThreadContext.h"
#include "globals.h"

#ifdef TLS
#include "Epoch.h"
#endif

#ifdef TASKSCALAR
#include "TaskContext.h"
#endif

class GMemoryOS;
class GMemorySystem;
class MemObj;

class ExecutionFlow : public GFlow {
private:

  ThreadContext thread;

#ifdef TS_TIMELINE
  int verID;
#endif

  // picodePC==0 means no thread in this execution flow
  icode_ptr picodePC;

  EventType ev;
  CallbackBase *evCB;
  long evAddr;

  DInst *pendingDInst;

#ifdef TASKSCALAR
  const HVersion *restartVer;
  void propagateDepsIfNeeded() {
    if (restartVer==0)
      return;
    TaskContext *tc=restartVer->getTaskContext();
    if(tc) {
      int rID = tc->addDataDepViolation();
      tc->invalidMemAccess(rID, DataDepViolationAtFetch);
    }
    restartVer = 0;
  }
#else
  void propagateDepsIfNeeded() { }
#endif

  InstID getPCInst() const {
    I(picodePC);
    return picodePC->instID;
  }

  void exeInstFast();

  // Executes a single instruction. Return value:
  //   If no instruction could be executed, returns 0 (zero)
  //   If an instruction was executed, returns non-zero
  //   For executed load/store instructions, the returned (non-zero)
  //   value is the virtual address of the data accessed. Note that
  //   load/store accesses to a virtual address of zero are not allowed.
  long exeInst();

  // Returns the next dynamic instruction, 0 if none can be executed
  // Executes instruction(s) as needed to generate the return value

protected:
public:
  InstID getNextID() const {
    I(picodePC);
    return picodePC->instID;
  }

  void addEvent(EventType e, CallbackBase *cb, long addr) {
    ev     = e;
    evCB   = cb;
    evAddr = addr;
  }

  ExecutionFlow(int cId, int i, GMemorySystem *gms);
  ThreadContext *getThreadContext(void) {
    I(thread.getPid()!=-1);
    return &thread;
  }

  void saveThreadContext(int pid) {
    I(thread.getPid()==pid);
    thread.setPicode(picodePC);
    ThreadContext::getContext(thread.getPid())->copy(&thread);
#if (defined TLS)
    I(thread.getEpoch()==ThreadContext::getContext(pid)->getEpoch());
#endif
  }

  void loadThreadContext(int pid) {
    I((thread.getPid()==-1)||(thread.getPid()==pid));
    thread.copy(ThreadContext::getContext(pid));
    thread.setPid(pid); // Not in copyContext
    picodePC=thread.getPicode();
#if (defined TLS)
    thread.setEpoch(ThreadContext::getContext(pid)->getEpoch());
#endif
  }

  icode_ptr getInstructionPointer(void); 
  void setInstructionPointer(icode_ptr picode) ;

  void switchIn(int i);
  void switchOut(int i);

  int currentPid(void) { return thread.getPid();  }
  DInst *executePC();

  void goRabbitMode(long long n2skip=0);
  void dump(const char *str) const;
};

#endif   // EXECUTIONFLOW_H
