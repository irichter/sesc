/*
 * Routines for the main execution loop, thread queues, and all the event
 * generating functions.
 *
 * Copyright (C) 1993 by Jack E. Veenstra (veenstra@cs.rochester.edu)
 * 
 * This file is part of MINT, a MIPS code interpreter and event generator
 * for parallel programs.
 * 
 * MINT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 * 
 * MINT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MINT; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "icode.h"
#include "ThreadContext.h"
#include "globals.h"
#include "opcodes.h"
#include "mendian.h"

#include "mintapi.h"

/* icode structures for the terminator functions */

/* Terminator1 is called when the thread calls exit(). Terminator1
 * generates an event so that the accumulated time for this thread
 * (as calculated in the inner execution loop) will be added to its
 * time value before it reaps its children in terminator2().
 */
OP(terminator1)
{
  /* This is called ONLY when the whole execution has finished */
  fprintf(stderr,"main returned, so program finished\n");
  
  rsesc_exit(pthread->getPid(), 0);

  return Itext[0];
}

OP(mint_sesc_simulation_mark)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_simulation_mark(pthread->getPid());
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_simulation_mark_id)
{
  int id = pthread->getGPR(Arg1stGPR);
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_simulation_mark_id(pthread->getPid(),id);
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_fast_sim_begin)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_fast_sim_begin(pthread->getPid());
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_fast_sim_end)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_fast_sim_end(pthread->getPid());
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_suspend)
{
  // Set things up for the return to from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Remember pid of the current thread then do the call
  Pid_t pid=pthread->getPid();
  ValueGPR retVal=rsesc_suspend(pid,pthread->getGPR(Arg1stGPR));
  // Context switch is likely, must look up thread context again

  ThreadContext *context=rsesc_get_thread_context(pid);

  context->setGPR(RetValGPR,retVal);
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_resume)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  ValueGPR retVal=rsesc_resume(pthread->getPid(),pthread->getGPR(Arg1stGPR));
  I(pthread->getPid()==thePid);
  // Set the return value
  pthread->setGPR(RetValGPR,retVal);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_yield)
{
  // Set things up for the return to from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Remember pid of the current thread then do the call
  Pid_t pid=pthread->getPid();
  ValueGPR retVal=rsesc_yield(pthread->getPid(),pthread->getGPR(Arg1stGPR));
  // Context switch is likely, must look up thread context again
  ThreadContext *context=rsesc_get_thread_context(pid);
  context->setGPR(RetValGPR,retVal);
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_preevent)
{
  // Set things up for the return to from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Get arguments for this call (sptr is a real address)
  int  vaddr=pthread->getGPR(Arg1stGPR);
  int  type=pthread->getGPR(Arg2ndGPR);
  void *sptr=(void *)(pthread->virt2real(pthread->getGPR(Arg3rdGPR)));
  // Do the actual call 
  rsesc_preevent(pthread->getPid(),vaddr,type,sptr);
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_postevent)
{
  // Get arguments for this call (sptr is a real address)
  int  vaddr=pthread->getGPR(Arg1stGPR);
  int  type=pthread->getGPR(Arg2ndGPR);
  void *sptr=(void *)(pthread->virt2real(pthread->getGPR(Arg3rdGPR)));
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_postevent(pthread->getPid(),vaddr,type,sptr);
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_memfence)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_memfence(pthread->getPid(),pthread->getGPR(Arg1stGPR));
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_acquire)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_acquire(pthread->getPid(),pthread->getGPR(Arg1stGPR));
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_sesc_release)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  rsesc_release(pthread->getPid(),pthread->getGPR(Arg1stGPR));
  I(pthread->getPid()==thePid);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

