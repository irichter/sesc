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

#include "OSSim.h"
#include "ExecutionFlow.h"
#include "DInst.h"
#include "Events.h"
#include "GMemoryOS.h"
#include "TraceGen.h"
#include "GMemorySystem.h"
#include "MemRequest.h"


#include "mintapi.h"
#include "opcodes.h"

ExecutionFlow::ExecutionFlow(int cId, int i, GMemorySystem *gmem)
  : GFlow(i, cId, gmem)
{
  picodePC = 0;
  ev = NoEvent;

#ifdef TS_TIMELINE
  verID = 0;
#endif

  goingRabbit = false;

  // Copy the address space from initial context
  memcpy(&thread,ThreadContext::getMainThreadContext(),sizeof(ThreadContext));

  thread.setPid(-1);
  thread.setPicode(&invalidIcode);
 
  pendingDInst = 0;
}

long ExecutionFlow::exeInst(void)
{
  // Instruction address
  int iAddr = picodePC->addr;
  // Instruction flags
  short opflags=picodePC->opflags;
  // For load/store instructions, will contain the virtual address of data
  // For other instuctions, set to non-zero to return success at the end
  VAddr dAddrV=1;
  // For load/store instructions, the Real address of data
  // The Real address is the actual address in the simulator
  //   where the data is found. With versioning, the Real address
  //   actually points to the correct version of the data item
  RAddr dAddrR=0;

#ifdef TASKSCALAR
  // is a context switch it should look for a new TaskContext
  TaskContext *tc = TaskContext::getTaskContext(thread.getPid());
  if (tc==0) {
    if(picodePC->func == mint_exit) {
      do{
	picodePC=(picodePC->func)(picodePC, &thread);
      }while(picodePC->addr==iAddr);
    }
    return 0;
  }
  I(tc);
  I(thread.getPid() != -1);

  RAddr origdAddrR=0;
#endif
  
  // For load/store instructions, need to translate the data address
  if(opflags&E_MEM_REF) {
    // Get the Virtual address
    dAddrV = (*(long *)((long)(thread.reg) + picodePC->args[RS])) + picodePC->immed;
    // Get the Real address
    dAddrR = thread.virt2real(dAddrV, opflags);

    
#ifdef TASKSCALAR
    if (dAddrR == 0)   // happens if spec thread had a bad addr translation
      return 0;
    I(tc); 
    I(tc->getVersionRef());
    if (!tc->getVersionRef()->isSafe() && gmos->TLBTranslate(dAddrV) == -1) {
      // Past:  tc->syncBecomeSafe();
      return 0; // Do not advance unsafe threads that thave a TLB miss
    }
#endif
    
#if (defined TLS)
    tls::Epoch *epoch=tls::Epoch::getEpoch(thread.getPid());
    if(epoch) {
      if(opflags&E_READ)
	dAddrR=epoch->read(iAddr, opflags, dAddrV, dAddrR);
      if(opflags&E_WRITE)
	dAddrR=epoch->write(iAddr, opflags, dAddrV, dAddrR);
      if(!dAddrR)
	return 0;
    }
#endif
    
#ifdef TASKSCALAR
    if(opflags&E_READ){
      I(!(opflags&E_WRITE));
      
      dAddrR=tc->read(iAddr, opflags, dAddrR);
    }else if(opflags&E_WRITE) {
      I(!(opflags&E_READ));
      
      origdAddrR = dAddrR;
      dAddrR = tc->preWrite(dAddrR);
    }
#endif
    
    // Put the Real data address in the thread structure
    thread.setRAddr(dAddrR);
  }
  
  do{
#if (defined TLS)
    tls::Epoch *epoch=thread.getEpoch();
    I(epoch==tls::Epoch::getEpoch(thread.getPid()));
    if(epoch){
      I(picodePC->getReplayClass()!=OpExposed);
    }
#endif
    picodePC=(picodePC->func)(picodePC, &thread);
  }while(picodePC->addr==iAddr);

#ifdef TASKSCALAR
  if(opflags&E_WRITE) {
    I(origdAddrR);
    I(restartVer==0);
    restartVer = tc->postWrite(iAddr, opflags, origdAddrR);
  }
#endif
#ifdef ATOMIC
#if 0
  if(opflags&E_READ) {
    I(restartVer==0);
    restartVer = tc->postRead();
  }
#endif
#endif
  return dAddrV;
}

void ExecutionFlow::exeInstFast()
{
  I(goingRabbit);
  
  // This exeInstFast can be called when the for sure there are not speculative
  // threads (TLS, or TASKSCALAR)
  int iAddr   =picodePC->addr;
  short iFlags=picodePC->opflags;
#ifdef PARPROF
  unsigned dvAddr = 0;
#endif


  if (trainCache) {
    // 10 advance clock. IPC of 0.1 is supported now
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
    EventScheduler::advanceClock();
  }

  if(iFlags&E_MEM_REF) {
    // Get the Logical address
    VAddr vaddr = (*(long *)((long)(thread.reg) + picodePC->args[RS])) + picodePC->immed;
#ifdef PARPROF
    dvAddr = vaddr;
#endif
    // Get the Real address
    thread.setRAddr(thread.virt2real(vaddr, iFlags));
    if (trainCache)
      CBMemRequest::create(0, trainCache, MemRead, vaddr, 0);

#ifdef TS_PROFILING
    if (osSim->enoughMarks1()) {
      if(iFlags&E_WRITE) {
        long value = *(long *)((long)(thread.reg) + picodePC->args[RT]);
        if ((unsigned long)value == SWAP_WORD(*(long *)thread.getRAddr())) {
          //silent store
          //LOG("silent store@0x%lx, %ld", picodePC->addr, value);
          osSim->getProfiler()->recWrite(vaddr, picodePC, true);
        } else {
          osSim->getProfiler()->recWrite(vaddr, picodePC, false);
        }
      }
      if(iFlags&E_READ) {
        osSim->getProfiler()->recRead(vaddr, picodePC);
      }
    }
#endif    
  }

#ifdef PARPROF
  const Instruction *eInst = Instruction::getInst(picodePC->instID);
  osSim->getParProf()->processInst(eInst, dvAddr);
#endif  

  do{
    picodePC=(picodePC->func)(picodePC, &thread);
  }while(picodePC->addr==iAddr);
}

void ExecutionFlow::switchIn(int i)
{
  I(thread.getPid() == -1);
  I(thread.getPicode()==&invalidIcode);
  loadThreadContext(i);
  I(thread.getPid() == i);

#ifdef TS_TIMELINE
  TaskContext *tc = TaskContext::getTaskContext(i);
  I(tc);
  I(verID == 0);
  verID = tc->getVersionRef()->getId();
  TraceGen::add(verID,"in=%lld",globalClock);
#endif

  GLOG(DEBUG2,"ExecutionFlow[%d] switchIn pid(%d) 0x%lx @%lld"
       , fid, i, picodePC->addr, globalClock);

  //I(!pendingDInst);
  if( pendingDInst ) {
#if (defined TLS)
    tls::Epoch *epoch=tls::Epoch::getEpoch(i);
    if(epoch) epoch->execInstr();
#endif
    pendingDInst->scrap();
    pendingDInst = 0;
  }
}

void ExecutionFlow::switchOut(int i)
{
#ifdef TS_TIMELINE
  TraceGen::add(verID,"out=%lld",globalClock);
  verID = 0;
#endif

  GLOG(DEBUG2,"ExecutionFlow[%d] switchOut pid(%d) 0x%lx @%lld", 
       fid, i, picodePC->addr, globalClock);

  // Must be running thread i
  I(thread.getPid() == i);
  // Save the context of the thread
  saveThreadContext(i);
  // Now running no thread at all
  thread.setPid(-1);
  thread.setPicode(&invalidIcode);

  //  I(!pendingDInst);
  if( pendingDInst ) {
#if (defined TLS)
    tls::Epoch *epoch=tls::Epoch::getEpoch(i);
    if(epoch) epoch->execInstr();
#endif
    pendingDInst->scrap();
    pendingDInst = 0;
  }
}

void ExecutionFlow::dump(const char *str) const
{
  if(picodePC == 0) {
    MSG("Flow(%d): context not ready", fid);
    return;
  }

  LOG("Flow(%d):pc=0x%x:%s", fid, (int)getPCInst()    // (int) because fake warning in gcc
      , str);

  LOG("Flowd:id=%3d:addr=0x%x:%s", fid, static_cast < unsigned >(picodePC->addr),
      str);
}

DInst *ExecutionFlow::executePC()
{
#if (defined TLS)
  tls::Epoch *epoch=tls::Epoch::getEpoch(thread.getPid());
#endif
  // If there is an already executed instruction, just return it
  if(pendingDInst) {
    DInst *dinst = pendingDInst;
    pendingDInst = 0;
#if (defined TLS)
    if(epoch) epoch->execInstr();
#endif
    return dinst;
  }

#ifdef TASKSCALAR
  restartVer = 0;
#endif

  DInst *dinst=0;
  // We will need the original picodePC later
  icode_ptr origPIcode=picodePC;
  I(origPIcode);
  // Need to get the pid here because exeInst may switch to another thread
  Pid_t     origPid=thread.getPid();
  // Determine whether the instruction has a delay slot
  bool hasDelay= ((picodePC->opflags) == E_UFUNC);
  bool isEvent=false;

#ifdef TASKSCALAR
  //this is a little hacky. TODO:check why sometimes some function
  //calls that has NO_SPEC do not fall on the hasDelay &&
  //picodePC->target check
  if(picodePC->opflags == E_NO_SPEC && !rsesc_is_safe(origPid)) {
    rsesc_become_safe(origPid);
    return 0;
  }
#endif
  // Library calls always have a delay slot and the target precalculated
  if(hasDelay && picodePC->target) {
    // If it is an event, set the flag
    isEvent=Instruction::getInst(picodePC->target->instID)->isEvent();
#ifdef TASKSCALAR
    if (picodePC->target->opflags == E_NO_SPEC && !rsesc_is_safe(origPid)) {
      rsesc_become_safe(origPid);
      return 0; // Nothing
    }else if (picodePC->target->func == mint_sesc_fork_successor) {
      exeInst();
      propagateDepsIfNeeded();
      
      exeInst();
      propagateDepsIfNeeded();
      hasDelay=false;

      // None of the exeInst() executed up to now can generate a
      // restart. if they do so the assumption that those instructions
      // are the "equivalent" of the spawn instruction is not correct
      I(restartVer==0);
    }
#endif
  }
  // Remember the ID of the instruction
  InstID cPC = getPCInst();

#if (defined TLS)
  if(epoch) epoch->pendInstr();
#endif // (defined TLS)

  long vaddr = exeInst();

#if (defined TASKSCALAR) || (defined TLS)
  // No instruction executed?
  if(vaddr==0 || cPC==0) {
#if (defined TLS)
    if(epoch) epoch->unPendInstr();
#endif // (defined TLS)
#if (defined TASKSCALAR)
    // restartVer only can be set by executed stores (always return != 0)
    I(restartVer==0);
    propagateDepsIfNeeded();
#endif // (defined TASKSCALAR)
    return 0;
  }
#endif // (defined TASKSCALAR) || (defined TLS)
 
  dinst=DInst::createInst(cPC, vaddr, origPid);

#if (defined TLS)
  dinst->setEpoch(epoch);
#endif // (defined TLS)

#ifdef TASKSCALAR
  if (restartVer) {
    dinst->addDataDepViolation(restartVer);
    restartVer=0;
  }
#endif

  // If no delay slot, just return the instruction
  if(!hasDelay) {
#if (defined TLS)
    if(epoch) epoch->execInstr();
#endif
    return dinst;
  }


  // The delay slot must execute
  I(thread.getPid()==origPid);
  I(getPCInst()==cPC+1);
  cPC = getPCInst();
#if (defined TLS)
  if(epoch) epoch->pendInstr();
#endif // (defined TLS)
  vaddr = exeInst();
  if(vaddr==0) {
#if (defined TLS)
    if(epoch) epoch->unPendInstr();
#endif // (defined TLS)

    dinst->scrap();
#ifdef TASKSCALAR
    propagateDepsIfNeeded();
#endif
    return 0;
  }

  I(vaddr);
  I(pendingDInst==0);
  // If a non-event branch with a delay slot, we reverse the order of the branch
  // and its delay slot, and first return the delay slot. The branch is still in
  // "pendingDInst".
  pendingDInst=dinst;
  dinst = DInst::createInst(cPC, vaddr, origPid);

#if (defined TLS)
  dinst->setEpoch(epoch);
#endif // (defined TLS)

#ifdef TASKSCALAR
  if (restartVer) {
    dinst->addDataDepViolation(restartVer);
    restartVer=0;
  }
#endif


  if(!isEvent) {
    // Reverse the order between the delay slot and the iBJ
#if (defined TLS)
    if(epoch) epoch->execInstr();
#endif
    return dinst;
  }

  // Execute the actual event (but do not time it)
  I(thread.getPid()==origPid);
  vaddr = exeInst();
  I(vaddr);
#ifdef TASKSCALAR
  propagateDepsIfNeeded();
  const Instruction *eInst = Instruction::getInst(origPIcode->target->instID);
  if (eInst->getEvent() == SpawnEvent) {
    eInst = Instruction::getInst(picodePC->instID);
    while (!eInst->isBJCond() ) {
      exeInst();
      propagateDepsIfNeeded();
      eInst = Instruction::getInst(picodePC->instID);
    }
    exeInst(); // Also the BJ itself
    propagateDepsIfNeeded();
  }
#endif
  if( ev == NoEvent ) {
    // This was a PreEvent (not notified see Event.cpp)
    // Only the delay slot instruction is "visible"
#if (defined TLS)
    if(epoch) epoch->execInstr();
#endif
    return dinst;
  }

  I(ev != PreEvent);
  if(ev == FastSimBeginEvent ) {
#if (defined TLS)
    I(!epoch);
#endif
    goRabbitMode();
    ev = NoEvent;
    return dinst;
  }
  I(ev != FastSimBeginEvent);
  I(ev != FastSimEndEvent);
  I(pendingDInst);
#if (defined TLS)  
  if(epoch){
    epoch->execInstr(); // For the scrapped pendingDInst
    epoch->pendInstr(); // For the fake event instruction
  }
#endif  
  // In the case of the event, the original iBJ dissapears
  pendingDInst->scrap();
  // Create a fake dynamic instruction for the event
  pendingDInst = DInst::createInst(Instruction::getEventID(ev), evAddr, origPid);

#if (defined TLS)
  pendingDInst->setEpoch(epoch);
#endif // (defined TLS)
  if( evCB )
    pendingDInst->addEvent(evCB);
  ev=NoEvent;
  // Return the delay slot instruction. The fake event instruction will come next.
#if (defined TLS)
  if(epoch) epoch->execInstr();
#endif
  return dinst;
}

void ExecutionFlow::goRabbitMode(long long n2skip)
{
  int nFastSims = 0;
  if( ev == FastSimBeginEvent ) {
    // Can't train cache in those cases. Cache only be train if the
    // processor did not even started to execute instructions
    trainCache = 0;
    nFastSims++;
  }else{
    I(globalClock==0);
  }

  if (n2skip) {
    I(!goingRabbit);
    goingRabbit = true;
  }

  nExec=0;
  
  do {
    ev=NoEvent;
    if( n2skip > 0 )
      n2skip--;

    nExec++;

    if (goingRabbit)
      exeInstFast();
    else {
      exeInst();
#ifdef TASKSCALAR
      propagateDepsIfNeeded();
#endif
    }

    if(thread.getPid() == -1) {
      ev=NoEvent;
      goingRabbit = false;
      LOG("1.goRabbitMode::Skipped %lld instructions",nExec);
      return;
    }
    
    if( ev == FastSimEndEvent ) {
      nFastSims--;
    }else if( ev == FastSimBeginEvent ) {
      nFastSims++;
    }else if( ev ) {
      if( evCB )
	evCB->call();
      else{
	// Those kind of events have no callbacks because they
	// go through the memory backend. Since we are in
	// FastMode and everything happens atomically, we
	// don't need to notify the backend.
	I(ev == ReleaseEvent ||
	  ev == AcquireEvent ||
	  ev == MemFenceEvent||
	  ev == FetchOpEvent );
      }
    }

    if( n2skip == 0 && goingRabbit && osSim->enoughMarks1() && nFastSims == 0 )
      break;

  }while(nFastSims || n2skip || goingRabbit);

  ev=NoEvent;
  LOG("2.goRabbitMode::Skipped %lld instructions",nExec);

  if (trainCache) {
    // Finish all the outstading requests
    while(!EventScheduler::empty())
      EventScheduler::advanceClock();

    //    EventScheduler::reset();
#ifdef TASKSCALAR
    TaskContext::collectCB.schedule(63023);
#endif
  }

  goingRabbit = false;
}

icode_ptr ExecutionFlow::getInstructionPointer(void)
{
  return picodePC; 
}

void ExecutionFlow::setInstructionPointer(icode_ptr picode) 
{ 
  thread.setPicode(picode);
  picodePC=picode;
}
