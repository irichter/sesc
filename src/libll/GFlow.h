/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

   Contributed by Luis Ceze
                  Jose Renau

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

#ifndef GFLOW_H
#define GFLOW_H

#include "nanassert.h"
#include "Events.h"
#include "callback.h"
#include "DInst.h"
#include "Snippets.h"

class GProcessor;
class GMemorySystem;
class GMemoryOS;
class MemObj;

class GFlow {
 private:
 protected:
  static bool goingRabbit;
  static long long nExec;

  static MemObj *trainCache;

  const int fid;
  const int cpuId;

  GMemorySystem *gms;
  GMemoryOS *gmos;
  
 public:
  GFlow(int i, int cId, GMemorySystem *gmem);

  virtual ~GFlow() {
    // Nothing
  }
  
  virtual InstID getNextID() const = 0;

  virtual void addEvent(EventType e, CallbackBase *cb, int addr) = 0;

  virtual ThreadContext *getThreadContext(void) = 0;

  virtual void saveThreadContext(int pid) = 0;

  virtual void loadThreadContext(int pid) = 0;

  virtual icode_ptr getInstructionPointer(void) = 0;

  virtual void setInstructionPointer(icode_ptr picode) = 0;

  virtual void switchIn(int i) = 0;
  virtual void switchOut(int i) = 0;

  virtual int currentPid(void) = 0;

  virtual void goRabbitMode(long long n2skip=0) = 0;
  virtual void dump(const char *str) const = 0;

  // needed by TraceFlow
  virtual bool hasWork() const { return true; }

  static bool isGoingRabbit() { return goingRabbit; }
  static long long getnExecRabbit() { return nExec; }

  static void dump();
};
#endif
