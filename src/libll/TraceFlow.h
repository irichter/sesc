/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

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

#ifndef TRACEFLOW_H
#define TRACEFLOW_H

class GMemoryOS;
class GMemorySystem;
class MemObj;


class TraceFlow : public GFlow {
 private:
  bool hasTrace;
  InstID nextPC;
  
 protected:
 public:
  TraceFlow(int cId, int i, GMemorySystem *gms);

  InstID getNextID() const {
    return nextPC;
  }

  void addEvent(EventType e, CallbackBase *cb, long addr) {
    I(0);
  }

  ThreadContext *getThreadContext(void);

  void saveThreadContext(int pid);

  void loadThreadContext(int pid);

  icode_ptr getInstructionPointer(void);

  void setInstructionPointer(icode_ptr picode);

  void switchIn(int i);
  void switchOut(int i);

  int currentPid(void);
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
  DInst *executePC();

  void goRabbitMode(long long n2skip=0);
  void dump(const char *str) const;

  static bool isGoingRabbit();
  static long long getnExecRabbit();
  static void dump();
};

#endif
