/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
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

#include "nanassert.h"

#include "GProcessor.h"
#include "OSSim.h"
#include "SescConf.h"
#include "BPred.h"
#include "ExecutionFlow.h"

#include "mintapi.h"

#ifdef TLS
#include "Epoch.h"
#endif

#ifdef TASKSCALAR
#include "TaskContext.h"
#endif

#ifdef VALUEPRED
#include "ValueTable.h"
#endif

static long long NES=0; // Number of exits pending to scape

long rsesc_usecs(void)
{
  // as in 1000000 clocks per second report (Linux is 100)

  double usecs = 1000000*globalClock;
  usecs /= osSim->getFrequency();
  
  return (long)usecs;
}

/*
 * SESC support severals types of events. To all the events it is
 * possible to pass a pointer to some data in the code. In the
 * original code it's possible to insert the following
 * instrumentation.
 *
 * sesc_preevent(long vaddr, long type, void *sptr);
 *
 * Remember that if you pass an address * through val, it needs to be
 * converted to physical with virt2real. Much better if you use the
 * *sptr.
 */


/**************************************************
 * As soon as the instructions is fetched this event is called. Its
 * called before anything is done inside the sesc simulator.
 */
void rsesc_preevent(int pid, long vaddr, long type, void *sptr)
{
  osSim->preEvent(pid, vaddr, type, sptr);
}

/**************************************************
 * Called when the instruction is dispatched. The instruction has no
 * dependence with any previous instructions. Internally in the sesc
 * the event takes resources in the same way that a NOP (consumes
 * fetch, issue, dispatch width, and occupy an instruction window
 * slot). It also enforces the dependences for the registers R4, R6.
 */
void rsesc_postevent(int pid, long vaddr, long type, void *sptr)
{
  postEventCB *cb = postEventCB::create(osSim, pid, vaddr, type, sptr);
    
  osSim->pid2GProcessor(pid)->addEvent(PostEvent, cb, 0);
}

/**************************************************
 * Called when the instruction is dispatched. It's the same that
 * sesc_postevent, with the difference that it is not dispatched until
 * all the previous memory operations have been performed.
 */
void rsesc_memfence(int pid, long vaddr)
{
  osSim->pid2GProcessor(pid)->addEvent(MemFenceEvent, 0, vaddr);
}


/**************************************************
 * Release consistency acquire. The vaddr is the address inside the
 * simulator. If you want to access the data you must call
 * virt2real(vaddr). Accessing the data, you must be careful, so that
 * the ENDIAN is respected (this is a problem in little endian
 * machines like x86)
 */
void rsesc_acquire(int pid, long vaddr)
{
#ifndef JOE_MUST_DO_IT_OR_ELSE
  return;
#endif
  osSim->pid2GProcessor(pid)->addEvent(AcquireEvent, 0, vaddr);
}

/**************************************************
 * Release consistency release. The vaddr is the address inside the
 * simulator. If you want to access the data you must call
 * virt2real(vaddr). Accessing the data, you must be careful, so that
 * the ENDIAN is respected (this is a problem in little endian
 * machines like x86)
 */
void rsesc_release(int pid, long vaddr)
{
#ifndef JOE_MUST_DO_IT_OR_ELSE
  return;
#endif
  osSim->pid2GProcessor(pid)->addEvent(ReleaseEvent, 0, vaddr);
}

/**************************************************
 * Called each time that mint has created a new thread. Currently only
 * spawn is supported.
 *
 * pid is the Thread[pid] where the new context have been created.ExecutionFlow
 * constructor must use this parameter. ppid is the parent thread
 */
#if (!defined TASKSCALAR)
void rsesc_spawn(int ppid, int cpid, long flags)
{
  osSim->eventSpawn(ppid, cpid, flags);
}

int rsesc_exit(int cpid, int err)
{
  osSim->eventExit(cpid, err);
  return 1;
}
#endif

void rsesc_finish(int pid)
{

#if 0
  ProcessId *proc = ProcessId::getProcessId(pid);
  if (proc) {
    if (proc->getState() == RunningState) {
      osSim->stop(pid);
      proc->destroy();
    }
  }
#else
  ProcessId::destroyAll();
#endif

  osSim->stopSimulation();
  osSim->simFinish();

  exit(0);
}

/* Same as rsesc_spawn, except that the new thread does not become ready */
void rsesc_spawn_stopped(int cpid, int pid, long flags){
  osSim->eventSpawn(cpid, pid, flags, true);
}

/* Returns the number of CPUs (cores) the system has */ 
int rsesc_get_num_cpus(void){
  return osSim->getNumCPUs();
}

void rsesc_sysconf(int cpid, int pid, long flags)
{
  osSim->eventSysconf(cpid, pid, flags);
}

void rsesc_wait(int cpid)
{
  osSim->eventWait(cpid);
}

int rsesc_pseudoreset_cnt = 0;

void rsesc_pseudoreset(int pid)
{ 
  if (pid==0 && rsesc_pseudoreset_cnt==0)
    {
      rsesc_pseudoreset_cnt++;
      
      osSim->pseudoReset();
    }
  else if (pid==0 && rsesc_pseudoreset_cnt > 0) {
    GLOG(1,"rsesc_pseudoreset called multiple times.");
  }
}

int rsesc_suspend(int cpid, int pid)
{
  return osSim->eventSuspend(cpid,pid);
}

int rsesc_resume(int cpid, int pid)
{
  return osSim->eventResume(cpid, pid);
}

int rsesc_yield(int cpid, int pid)
{
  return osSim->eventYield(cpid,pid);
}

void rsesc_fast_sim_begin(int pid)
{
  LOG("Begin Rabbit mode (embeded)");
  osSim->pid2GProcessor(pid)->addEvent(FastSimBeginEvent, 0, 0);
}

void mint_termination(int pid)
{
  LOG("mint_termination(%d) received (NES=%lld)\n", pid, NES);

  if (GFlow::isGoingRabbit()) {
#ifdef TS_PROFILING  
    osSim->getProfiler()->recTermination(pid);
#endif    

    Report::field("OSSim:rabbit2=%lld", ExecutionFlow::getnExecRabbit());
    osSim->report("Rabbit--Final");
  }else
    rsesc_finish(pid);

  exit(0);
}

void rsesc_simulation_mark(int pid)
{
  if (GFlow::isGoingRabbit()) {
    MSG("sesc_simulation_mark %ld (rabbit) inst=%lld", osSim->getSimulationMark(), GFlow::getnExecRabbit());
    GFlow::dump();
  }else
    MSG("sesc_simulation_mark %ld (simulated) @%lld", osSim->getSimulationMark(), globalClock);

  osSim->eventSimulationMark();

#ifdef TS_PROFILING
  if (ExecutionFlow::isGoingRabbit()) {
    if (osSim->enoughMarks1() && osSim->getProfiler()->notStart()) {
      osSim->getProfiler()->recStartInst();
      osSim->getProfiler()->recInitial(pid);
    }  
  }
#endif  

  if (osSim->enoughMarks2()) {
    mint_termination(pid);
  }
}

void rsesc_simulation_mark_id(int pid, int id)
{
  if (GFlow::isGoingRabbit()) {
    MSG("sesc_simulation_mark(%d) %ld (rabbit) inst=%lld", id, osSim->getSimulationMark(id), GFlow::getnExecRabbit());
    GFlow::dump();
  }else
    MSG("sesc_simulation_mark(%d) %ld (simulated) @%lld", id, osSim->getSimulationMark(id), globalClock);

  osSim->eventSimulationMark(id,pid);

#ifdef TS_PROFILING
  if (ExecutionFlow::isGoingRabbit()) {
    if (osSim->enoughMarks1() && osSim->getProfiler()->notStart()) {
      osSim->getProfiler()->recStartInst();
      osSim->getProfiler()->recInitial(pid);
    }  
  }
#endif  

  if (osSim->enoughMarks2(id)) {
    mint_termination(pid);
  }
}

void rsesc_fast_sim_end(int pid)
{
  osSim->pid2GProcessor(pid)->addEvent(FastSimEndEvent, 0, 0);
  LOG("End Rabbit mode (embeded)");
}

long rsesc_fetch_op(int pid, enum FetchOpType op, long addr, long *data, long val)
{
  I(addr);

  osSim->pid2GProcessor(pid)->addEvent(FetchOpEvent, 0, addr);
  long odata = SWAP_WORD(*data);

  if (odata)
    osSim->pid2GProcessor(pid)->nLockContCycles.inc();

  switch(op){
  case FetchIncOp:
    *data = SWAP_WORD(odata+1);
    break;
  case FetchDecOp:
    *data = SWAP_WORD(odata-1);
    break;
  case FetchSwapOp:
    *data = SWAP_WORD(val);
    break;
  default:
    MSG("sesc_fetch_op(%d,0x%p,%ld) has invalid Op",(int)op,data,val);
    exit(-1);
  }

/*  MSG("%d. sesc_fetch_op: %d@0x%lx [%ld]->[%ld] ",pid, (int)op,addr, odata,SWAP_WORD(*data)); */
 
  return odata;
}

void rsesc_do_unlock(long* data, int val)
{
  I(data);
  *data = SWAP_WORD(val);
}

typedef CallbackFunction2<long*, int, &rsesc_do_unlock> do_unlockCB;

void rsesc_unlock_op(int pid, long addr, long *data, int val)
{
  I(addr);
  osSim->pid2GProcessor(pid)->addEvent(UnlockEvent, 
				       do_unlockCB::create(data, val), addr);
  osSim->pid2GProcessor(pid)->nLocks.inc();
}

ThreadContext *rsesc_get_thread_context(int pid)
{
  return osSim->getContext(pid);
}

void rsesc_set_instruction_pointer(int pid, icode_ptr picode)
{
  osSim->eventSetInstructionPointer(pid,picode);
}

icode_ptr rsesc_get_instruction_pointer(int pid)
{
  return osSim->eventGetInstructionPointer(pid);
}

void rsesc_spawn_opcode(int pid, const int *params, int nParams)
{
  osSim->eventSpawnOpcode(pid,params,nParams);
}

void rsesc_fatal(void)
{
  MSG("Dumping Partial Statistics from rsesc_fatal!");
  osSim->reportOnTheFly();
}

#if (defined TLS)
  
  // Transitions from normal execution to epoch-based execution
//   void rsesc_begin_epochs(Pid_t pid){
//     tls::Epoch *iniEpoch=tls::Epoch::initialEpoch(static_cast<tls::ThreadID>(pid));
//     iniEpoch->run();
//   }
  
  // Creates a sequential successor epoch by cloning current epoch
  // Return value in simulated execution is similar to fork:
  // Successor epoch returns with return value 0
  // Calling epoch returns with return value that is context ID of successor
  void rsesc_future_epoch(Pid_t oldPid){
    tls::Epoch *oldEpoch=tls::Epoch::getEpoch(oldPid);
    I(oldEpoch);
    // Set the return value to 0, then create the new epoch with that
    ThreadContext *oldContext=osSim->getContext(oldPid);
    oldContext->reg[2]=0;
    tls::Epoch *newEpoch=oldEpoch->spawnEpoch();
    Pid_t newPid=newEpoch->getPid();
    // Set the return value of the old epoch to the SESC pid of the new epoch
    oldContext=osSim->getContext(oldPid);
    oldContext->reg[2]=newPid;
    // Run the new epoch
    newEpoch->run();
  }

  void rsesc_future_epoch_jump(Pid_t oldPid, icode_ptr jump_icode){
    // Save the actual instruction pointer of the old epoch
    icode_ptr currInstrPtr=osSim->eventGetInstructionPointer(oldPid);
    // Create the new epoch with the wanted instruction pointer
    osSim->eventSetInstructionPointer(oldPid,jump_icode);
    tls::Epoch *oldEpoch=tls::Epoch::getEpoch(oldPid);
    I(oldEpoch);
    tls::Epoch *newEpoch=oldEpoch->spawnEpoch();
    // Restore the old epoch's instruction pointer
    osSim->eventSetInstructionPointer(oldPid,currInstrPtr);
    // Run the new epoch
    newEpoch->run();
  }
  
  void rsesc_acquire_begin(Pid_t pid){
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      epoch->beginAcquire();
  }
  
  void rsesc_acquire_retry(Pid_t pid){
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      epoch->retryAcquire();
  }
  
  void rsesc_acquire_end(Pid_t pid){
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      epoch->endAcquire();
  }
  
  void rsesc_release_begin(Pid_t pid){
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      epoch->beginRelease();
  }
  
  void rsesc_release_end(Pid_t pid){
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      epoch->endRelease();
  }
  
  void rsesc_nonspec_epoch(Pid_t pid){
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch){
      I(0);
    }
  }

  // Ends an epoch for which a future has already been created
//   void rsesc_commit_epoch(Pid_t oldPid){
//     tls::Epoch *epoch=tls::Epoch::getEpoch(oldPid);
//     if(epoch)
//       epoch->complete();
//   }
  
  // Transitions from one epoch into its (newly created) sequential successor
//   void rsesc_change_epoch(Pid_t pid){
//     tls::Epoch *oldEpoch=tls::Epoch::getEpoch(pid);
//     if(oldEpoch)
//       oldEpoch->changeEpoch();
//   }
  
  // Transitions from epoch-based into normal execution
//   void rsesc_end_epochs(Pid_t pid){
//     tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
//     if(epoch)
//       epoch->complete();
//   }
  
  long rsesc_OS_read(int pid, int addr, int iAddr, long flags) {
    ThreadContext *pthread = ThreadContext::getContext(pid);
    I((flags & E_WRITE) == 0);
    flags = (E_READ | flags); 
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      return epoch->read(iAddr, flags, addr, pthread->virt2real(addr, flags));
    return pthread->virt2real(addr, flags);
  }
  
  long rsesc_OS_prewrite(int pid, int addr, int iAddr, long flags){
    ThreadContext *pthread = ThreadContext::getContext(pid);
    I((flags & E_READ) == 0);
    flags = (E_WRITE | flags); 
    tls::Epoch *epoch=tls::Epoch::getEpoch(pid);
    if(epoch)
      return epoch->write(iAddr, flags, addr, pthread->virt2real(addr, flags));
    return pthread->virt2real(addr, flags);
  }
  
  void rsesc_OS_postwrite(int pid, int addr, int iAddr, long flags){
    // Do nothing
  }
  
#endif
  
#ifdef VALUEPRED
int rsesc_get_last_value(int pid, int index)
{
  if (!osSim->enoughMarks1())
    return 0;

  return ValueTable::readLVPredictor(index);
}

void rsesc_put_last_value(int pid, int index, int val)
{
  if (!osSim->enoughMarks1())
    return;

  ValueTable::updateLVPredictor(index, val);
}

int rsesc_get_stride_value(int pid, int index)
{
  if (!osSim->enoughMarks1())
    return 0;

  return ValueTable::readSVPredictor(index);
}

void rsesc_put_stride_value(int pid, int index, int val)
{
  if (!osSim->enoughMarks1())
    return;

  ValueTable::updateSVPredictor(index, val);
}

int rsesc_get_incr_value(int pid, int index, int lval)
{
  if (!osSim->enoughMarks1())
    return 0;

  return ValueTable::readIncrPredictor(index, lval);
}

void  rsesc_put_incr_value(int pid, int index, int incr)
{
  if (!osSim->enoughMarks1())
    return;

  ValueTable::updateIncrPredictor(index, incr);
}

void  rsesc_verify_value(int pid, int rval, int pval)
{
  if (!osSim->enoughMarks1())
    return;

  ValueTable::verifyValue(rval, pval);
}
#endif

#if (defined TASKSCALAR) || (defined TLS)
// Memory protection subroutines. Those are invoqued by mint (mainly subst.c) to
// access/update data that has been versioned

// Copy data from srcStart (logical) to version memory (can generate squash/restart)
void rsesc_OS_write_block(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size){
  long addr;
  const unsigned char *src = (const unsigned char *)srcStart;
  const unsigned char *end;
  unsigned char *dst = (unsigned char *)dstStart;
  
  addr = (long)dst;
  
  addr = addr & (~0UL ^ 0x7); // 3 bits 2^3 = 8 bytes align (DWORD)
  addr = addr + 0x08;
  end  = (unsigned char *)addr;
  if ( end > &dst[size])
    end = &dst[size];
  
  // BYTE copy (8 bits)
  while(dst < end) {
    addr = rsesc_OS_prewrite(pid, (int)dst, iAddr,  E_BYTE);
    memcpy((void *) addr,  src, 1);
    rsesc_OS_postwrite(pid, (int)dst, iAddr, E_BYTE);
    size--;
    src++;
    dst++;
  }
  
  GI(size, ((long)dst & 0x7) == 0); // DWORD align
  // DWORD copy (64 bits)
  while(size) {
    long flags;
    long bsize;
    
    if (size >= 8) {
      flags = E_DWORD;
      bsize = 8;
    }else if (size >= 4) {
      flags = E_WORD;
      bsize = 4;
    } else if (size >= 2) {
      flags = E_HALF;
      bsize = 2;
    }else{
      bsize = 1;
      flags = E_BYTE;
    }
    
    addr = rsesc_OS_prewrite(pid, (int)dst, iAddr, flags);
    memcpy((void *) addr,  src, bsize);
    rsesc_OS_postwrite(pid, (int)dst, iAddr, flags);

    size-= bsize;
    src += bsize;
    dst += bsize;
  }
}

void rsesc_OS_read_string(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size){
  long addr;
  const unsigned char *src = (const unsigned char *)srcStart;
  const unsigned char *end;
  unsigned char *dst = (unsigned char *)dstStart;
  
  end = &dst[size-1];
  
  // BYTE copy (8 bits)
  while(dst < end) {
    addr = rsesc_OS_read(pid, (int)src, iAddr, E_BYTE);
    memcpy(dst, (void *) addr, 1);
    if (*dst == 0)
      break;
    size--;
    src++;
    dst++;
  }
  *dst=0;
}

// Copy from srcStart (Logical) to dstStart (Real). dstStart should be some
// stack allocated memory that is discarded briefly after. The reason is that if
// a restart is generated, it can not recopy the data.
//
// TODO: Maybe it should not track the read (screw the restart!)
void rsesc_OS_read_block(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size){
  long addr;
  const unsigned char *src = (unsigned char *)srcStart;
  const unsigned char *end;
  unsigned char *dst = (unsigned char *)dstStart;
  
  addr = (long)src;
  
  addr = addr & (~0UL ^ 0x7); // 3 bits 2^3 = 8 bytes align (DWORD)
  addr = addr + 0x08;
  end  = (unsigned char *)addr;
  if ( end > &src[size])
    end = &src[size];
  
  // BYTE copy (8 bits)
  while(src < end) {
    addr = rsesc_OS_read(pid, (int)src, iAddr, E_BYTE);
    memcpy(dst, (void *) addr, 1);
    size--;
    src++;
    dst++;
  }
  
  GI(size, ((long)src & 0x7) == 0); // DWORD align
  // DWORD copy (64 bits)
  while(size) {
    long flags;
    long bsize;
    
    if (size >= 8) {
      flags = E_DWORD;
      bsize = 8;
    }else if (size >= 4) {
      flags = E_WORD;
      bsize = 4;
    } else if (size >= 2) {
      flags = E_HALF;
      bsize = 2;
    }else{
      bsize = 1;
      flags = E_BYTE;
    }
    
    addr = rsesc_OS_read(pid, (int)src, iAddr, flags);
    memcpy(dst, (void *) addr, bsize);

    size-= bsize;
    src += bsize;
    dst += bsize;
  }
}

#endif // END of either TaskScalar or VersionMem or TLS

/****************** TaskScalar *******************/


#ifdef TASKSCALAR

#if (defined TLS)
#error "Taskscalar is incompatible with TLS"
#endif

void rsesc_exception(int pid)
{
  if (ExecutionFlow::isGoingRabbit()) {
    MSG("exception in rabbit mode pc=0x%x r31=0x%x"
	,(int)osSim->eventGetInstructionPointer(pid)->addr, (int)osSim->getContextRegister(pid,31));
    exit(0);
  }

  I(pid>=0);
  TaskContext *tc =TaskContext::getTaskContext(pid);
  I(tc);
  I(!tc->getVersionRef()->isSafe());
  GMSG(tc->getVersionRef()->isSafe(),
       "(failed) safe thread exception. stopping thread. pid = %d. Iaddr=0x%08lx"
       , pid, osSim->eventGetInstructionPointer(pid)->addr);

  tc->exception();
}

void rsesc_spawn(int ppid, int cpid, long flags)
{
  //if (ExecutionFlow::isGoingRabbit()) {
  //  MSG("spawn not supported in rabbit mode. Sorry");
  //  exit(-1);
  //}

#ifdef TC_PARTIALORDER
  osSim->eventSpawn(ppid, cpid, flags);
  TaskContext::normalForkNewDomain(cpid);
#else
  // New thread must share the same TaskContext
  TaskContext::getTaskContext(ppid)->normalFork(cpid);
  osSim->eventSpawn(ppid, cpid, flags);
#endif
}

int rsesc_exit(int cpid, int err)
{
  if (ExecutionFlow::isGoingRabbit()) {
    MSG("Real exit called (NES=%lld) pc=0x%x", NES, (int)osSim->getContextRegister(cpid,31));
    exit(0);
    return 1;
  }

  TaskContext *tc=TaskContext::getTaskContext(cpid);
  if (tc) {
    tc->endTaskExecuted(cpid);
    return 0; // Do not recycle the Thread[cpid]
  }else
    osSim->eventExit(cpid, err);
  return 1;
}

int rsesc_version_can_exit(int pid)
{
  if (ExecutionFlow::isGoingRabbit()) {
    GLOG(DEBUG2, "can_exit (NES=%lld) pc=0x%x", NES, (int)osSim->getContextRegister(pid,31));
    if (NES > 0) {
      NES--;
      return 1;
    }
    return 0;
  }
  // 0 means proceed normal exit code (call rsesc_exit)
  // 1 means ignore exit
  TaskContext *tc=TaskContext::getTaskContext(pid);
  if (tc)
    return tc->ignoreExit();

  return 0; // Proceed normal exit
}

void rsesc_prof_commit(int pid, int tid)
{
#ifdef TS_PROFILING
  if (ExecutionFlow::isGoingRabbit()) {
    osSim->getProfiler()->recCommit(pid, tid);
  }
#endif
}

void rsesc_fork_successor(int ppid, long where, int tid)
{
  if (ExecutionFlow::isGoingRabbit()) {
#ifdef TS_PROFILING  
    if (osSim->enoughMarks1())
      osSim->getProfiler()->recSpawn(ppid);
#endif      

    ThreadContext *pthread=osSim->getContext(ppid);
    GLOG(DEBUG2, "fork_successor (NES=%lld) pc=0x%x", NES, (int)pthread->reg[31]);
    NES++;
    pthread->reg[2]= -1;
    return;
  }

  TaskContext *pTC = TaskContext::getTaskContext(ppid);
  I(pTC);
  
  bool canNotSpawn = pTC->canMergeNext();
#ifdef TS_MERGENEXT
  if(canNotSpawn) {
    ThreadContext *pthread=osSim->getContext(ppid);
    pthread->reg[2]= -1;
    return;
  }
#endif

  ThreadContext *parentThread=osSim->getContext(ppid);

  int childPid=mint_sesc_create_clone(parentThread); // Child

  ThreadContext *childThread=osSim->getContext(childPid);

  childThread->reg[2]  =0;
  parentThread->reg[2] =childPid;

  // Swallow the first two (should not exist) instructions from the thread (branch + nop)

  for(int i=0; i<0; i++) {
    int iAddr = childThread->getPicode()->addr;
    I( ((childThread->getPicode()->opflags)&E_MEM_REF) == 0);
    do{
      childThread->setPicode(childThread->getPicode()->func(childThread->getPicode(), childThread));
    }while(childThread->getPicode()->addr==iAddr);
  }

  LOG("@%lld fork_successor ppid %d child %d (r31=0x%08x)", globalClock, ppid, childPid, parentThread->reg[31]);

  pTC->spawnSuccessor(childPid, parentThread->reg[31]);

  // The child was forked stopped, it can resume now 
  osSim->unstop(childPid);

#ifdef TS_MERGENEXT
  I(!canNotSpawn);
#else
  if (canNotSpawn) {
    TaskContext *childTC = TaskContext::getTaskContext(childPid);
    I(childTC);
    childTC->syncBecomeSafe(true);
  }
#endif

#if 0 // def SPREAD_SPAWNING
  ProcessId *proc = ProcessId::getProcessId(childPid);
  
  unsigned short r = (unsigned short)pTC->getSpawnAddr();
  r = (r>>3) ^ (r>>5);
  r = r % (osSim->getNumCPUs()+1);
  proc->sysconf(SESC_FLAG_MAP| r);
  // proc->sysconf(SESC_FLAG_MAP| 0);
#endif
}

int rsesc_become_safe(int pid)
{
  if (ExecutionFlow::isGoingRabbit())
    return 1;

  LOG("@%lld become_safe for thread %d",globalClock, pid);

  // Itself deciding to become safe sycall otherwise rsesc_exception
  TaskContext *tc = TaskContext::getTaskContext(pid);
  I(tc);
  
  tc->syncBecomeSafe();
  
  return 1;
}

int rsesc_is_safe(int pid)
{
  if (ExecutionFlow::isGoingRabbit())
    return 1;

  return TaskContext::getTaskContext(pid)->getVersionRef()->isSafe() ? 1 : 0;
}

ID(static bool preWriteCalled=false);

long rsesc_OS_prewrite(int pid, int addr, int iAddr, long flags)
{
  ThreadContext *pthread = ThreadContext::getContext(pid);

  I(!preWriteCalled);
  IS(preWriteCalled=true);
  I((flags & E_READ) == 0);
  flags = (E_WRITE | flags); 

  if (ExecutionFlow::isGoingRabbit())
    return pthread->virt2real(addr, flags);

  return (long)TaskContext::getTaskContext(pid)->preWrite(pthread->virt2real(addr, flags));
}

void rsesc_OS_postwrite(int pid, int addr, int iAddr, long flags)
{
  I(preWriteCalled);
  IS(preWriteCalled=false);

  I((flags & E_READ) == 0);
  flags = (E_WRITE | flags); 

  if (ExecutionFlow::isGoingRabbit())
    return;

  ThreadContext *pthread = ThreadContext::getContext(pid);

  TaskContext::getTaskContext(pid)->postWrite(iAddr, flags, pthread->virt2real(addr, flags));
}

long rsesc_OS_read(int pid, int addr, int iAddr, long flags)
{
  I((flags & E_WRITE) == 0);
  flags = (E_READ | flags); 

  ThreadContext *pthread = ThreadContext::getContext(pid);

  if (ExecutionFlow::isGoingRabbit())
    return pthread->virt2real(addr, flags);

  return TaskContext::getTaskContext(pid)->read(iAddr, flags, pthread->virt2real(addr, flags));
}

int rsesc_is_versioned(int pid)
{
  return ExecutionFlow::isGoingRabbit()? 0 : 1;
}

#endif // TASKSCALAR

#ifdef SESC_LOCKPROFILE
static GStatsCntr* lockTime = 0;
static GStatsCntr* lockCount = 0;
static GStatsCntr* lockOccTime = 0;

extern "C" void rsesc_startlock(int pid)
{
  if (!lockTime) {
    lockTime    = new GStatsCntr("","LOCK:Time");
    lockOccTime = new GStatsCntr("","LOCK:OccTime");
    lockCount   = new GStatsCntr("","LOCK:Count");
  }
  lockTime->add(-globalClock);
  lockCount->inc();
}

extern "C" void rsesc_endlock(int pid)
{
  lockTime->add(globalClock);
}

extern "C" void rsesc_startlock2(int pid)
{
  lockOccTime->add(-globalClock);
}

extern "C" void rsesc_endlock2(int pid)
{
  lockOccTime->add(globalClock);
}

#endif

