
#include "icode.h"
#include "ThreadContext.h"
#include "globals.h"
#include "opcodes.h"

#ifdef TASKSCALAR
#include "MemBuffer.h"
#include "TaskContext.h"
#endif

#include "Events.h"

extern icode_ptr Idone1;         /* calls terminator1() */
// void malloc_share(thread_ptr child, thread_ptr parent);

Pid_t ThreadContext::baseActualPid;
Pid_t ThreadContext::nextActualPid;
ThreadContext::ContextVector ThreadContext::actualPool;

Pid_t ThreadContext::baseClonedPid;
Pid_t ThreadContext::nextClonedPid;
ThreadContext::ContextVector ThreadContext::clonedPool;

size_t ThreadContext::nThreads;

ThreadContext::ContextVector ThreadContext::pid2context;

ThreadContext *ThreadContext::mainThreadContext;

ulong ThreadContext::DataStart;
ulong ThreadContext::DataEnd;
signed long ThreadContext::DataMap;

ulong ThreadContext::HeapStart;
ulong ThreadContext::HeapEnd;

ulong ThreadContext::StackStart;
ulong ThreadContext::StackEnd;

ulong ThreadContext::PrivateStart;
ulong ThreadContext::PrivateEnd;


void ThreadContext::staticConstructor(void)
{
  nextActualPid=baseActualPid=0;
  nextClonedPid=baseClonedPid=1024;

  mainThreadContext=newActual();
  nThreads = 1;
}

void ThreadContext::copy(const ThreadContext *src)
{
  for(size_t i=0;i<32;i++) {
    reg[i]=src->reg[i];
    fp[i]=src->fp[i];
  }

  lo=src->lo;
  hi=src->hi;

  fcr31=src->fcr31;
  picode=src->picode;
  target=src->target;
  parent=src->parent;
  youngest=src->youngest;
  sibling=src->sibling;
  stacktop=src->stacktop;
}

ThreadContext *ThreadContext::getContext(Pid_t pid)
{
  I(pid>=0);
  I((size_t)pid<pid2context.size());
  return pid2context[pid];
}

size_t ThreadContext::size() 
{
  return nThreads;
}

ThreadContext *ThreadContext::getMainThreadContext(void)
{
  I(mainThreadContext);
  I(mainThreadContext->pid==0);
  I(pid2context.size()>0);
  I(pid2context[0]==mainThreadContext);
  return mainThreadContext;
}

ThreadContext *ThreadContext::newActual(void)
{
  ThreadContext *context;
  if(actualPool.empty()) {
    I(nextActualPid<Max_nprocs);

    context=static_cast<ThreadContext *>(calloc(1,sizeof(ThreadContext)));
    // Initialize the actual context for the first time
    context->pid=nextActualPid++;
    //    context->pll_lock=&LL_lock[context->pid];
    context->fd=(char *)malloc(MAX_FDNUM*sizeof(char));
    I(context->fd);

    if(pid2context.size()<(size_t)nextActualPid)
      pid2context.resize(nextActualPid,0);
  }else{
    context=actualPool.back();
    actualPool.pop_back();
  }
  pid2context[context->pid]=context;
#if (defined TLS)
  context->myEpoch=0;
#endif
  nThreads++;
  return context;
}

ThreadContext *ThreadContext::newActual(Pid_t pid)
{
  I(pid2context[pid]==0);
  ContextVector::iterator poolIt=actualPool.begin();
  while((*poolIt)->pid!=pid) {
    I(poolIt!=actualPool.end());
    poolIt++;
  }
  ThreadContext *context=*poolIt;
  *poolIt=actualPool.back();
  actualPool.pop_back();
  I(context->pid==pid);
  pid2context[pid]=context;
#if (defined TLS)
  context->myEpoch=0;
#endif
  nThreads++;
  return context;
}

ThreadContext *ThreadContext::newCloned(void)
{
  ThreadContext *context;
  if(clonedPool.empty()) {
    context=static_cast<ThreadContext *>(calloc(1,sizeof(ThreadContext)));
    context->pid=nextClonedPid++;
    pid2context.resize(nextClonedPid,0);
  }else{
    context=clonedPool.back();
    clonedPool.pop_back();
  }
  pid2context[context->pid]=context;
#if (defined TLS)
  context->myEpoch=0;
#endif
  nThreads++;
  return context;
}  

void ThreadContext::free(void)
{
  // The main context has PID zero and should not be freed until the end
  // If it is freed it can be reused, and no child process should have
  // pid zero because it can not be distinguished from the parent in
  // fork-like function calls (which return child's pid in the parent and
  // zero in the child process).
  I((this!=mainThreadContext)||(nThreads==1));
  I(pid>=0);
  I((size_t)pid<pid2context.size());
  I(pid2context[pid]==this);
  pid2context[pid]=0;
  heapManager->delReference();
  heapManager=0;
  if(isCloned()) {
    clonedPool.push_back(this);
  }else{
    actualPool.push_back(this);
  }
  nThreads--;
}

void ThreadContext::initAddressing(signed long rMap, signed long mMap, signed long sTop) 
{
  stacktop   = sTop;

  // Segments order: (Rdata|Data)...Bss...Heap...Stacks
  if(Data_start <= Rdata_start) {
    DataStart = Data_start;
    DataMap   = mMap;
  }else{
    DataStart = Rdata_start;
    DataMap   = rMap;
  }
    
  DataEnd   = Data_end;
  // Rdata should between
  I(Rdata_start >= (signed)DataStart && Rdata_end <= (signed)DataEnd);
  // Data should between
  I(Data_start >= (signed)DataStart && Data_end <= (signed)DataEnd);
  // BSS should between
  I(Bss_start >= (signed)DataStart && (Bss_start + (signed)Bss_size) <= (signed)DataEnd);

  HeapStart = Heap_start;
  HeapEnd   = Heap_end;

  StackStart = Stack_start;
  StackEnd   = Stack_end;
  I(StackStart >= HeapEnd);

  PrivateStart = Private_start;
  PrivateEnd   = Private_end;

  I(StackEnd <= PrivateEnd);
  
  // Processor MAP:  (as long as there is not overlap, it is correct)
  //
  // Opt1: DataStart-DataEnd ... PrivateStart-PrivateEnd
  //
  // Opt2: PrivateStart-PrivateEnd ... DataStart-DataEnd

  I(0x60000000 > DataEnd);
  I(0x60000000 > PrivateEnd);

  ulong privSize = PrivateEnd-PrivateStart;
  
  MSG("Data[0x%x-0x%x]->[0x%x-...] Private[0x%x-0x%x] heap[0x%x-0x%x] stack[0x%x-0x%x]"
      ,(unsigned)DataStart     ,(unsigned)DataEnd     ,(unsigned)DataStart
      ,(unsigned)PrivateStart ,(unsigned)PrivateEnd
      ,(unsigned)HeapStart     ,(unsigned)HeapEnd
      ,(unsigned)StackStart    ,(unsigned)StackEnd
      );

  if( ((ulong)PrivateEnd > DataStart  && (ulong)PrivateEnd < DataEnd)
      || ((ulong)PrivateStart > DataStart  && (ulong)PrivateStart < DataEnd) ) {
    MSG("There is an overlap between private and shared memory");
    MSG("This is not allowed, try increase/decrease -h parameter");
    MSG("or change mint memory allocation scheme");
    exit(-1);
  }
}

void ThreadContext::dump()
{
  int i, j;

  printf("thread 0x%x:\n", (int)this);
  for (i = 0; i < 32; ) {
    for (j = 0; j < 4; j++, i++)
      printf("  r%d: %s 0x%08lx", i, i < 10 ? " " : "", reg[i]);
    printf("\n");
  }
  printf("  lo:   0x%08lx  hi:   0x%08lx\n", lo, hi);

  /* print out floats and doubles */
  for (i = 0; i < 32; ) {
    for (j = 0; j < 4; j++, i++)
      printf("  $f%d:%s %10.6f", i, i < 10 ? " " : "", getFPNUM(i));
    printf("\n");
  }

  for (i = 0; i < 32; ) {
    for (j = 0; j < 4; j++, i += 2)
      printf("  $d%d:%s %10.6f", i, i < 10 ? " " : "", getDPNUM(i));
    printf("\n");
  }

  for (i = 0; i < 32; ) {
    for (j = 0; j < 4; j++, i++)
      printf("  $w%d:%s 0x%08lx", i, i < 10 ? " " : "", getWFPNUM(i));
    printf("\n");
  }

  printf("  fcr0 = 0x%lx, fcr31 = 0x%lx\n", fcr0, fcr31);

  printf("  target = 0x%x, pid = %d\n", (int)(target), pid);

  printf("  parent = 0x%x, youngest = 0x%x, sibling = 0x%x\n",
	 (unsigned) parent, (unsigned) youngest, (unsigned) sibling);
}

/* dump the stack for stksize number of words */
void ThreadContext::dumpStack()
{
  int stksize = Stack_size;
  long sp;
  int i, j;
  unsigned char c;

  sp = getREGNUM(29);
  printf("sp = 0x%08x\n", (unsigned) sp);
  for (i = sp + (stksize - 1) * 4; i >= sp; i -= 4) {
    printf("0x%08x (+%ld): 0x%x (%f)  ",
	   i,  (i - sp), *(int *) i, *(float *) i);
    for (j = 0; j < 4; j++) {
      c = *((char *) i + j);
      if (c < ' ')
	c += 'A' - 1;
      else if (c >= 0x7f)
	c = '?';
      printf("^%c", c);
    }
    printf("\n");
  }
}

void ThreadContext::useSameAddrSpace(thread_ptr parent)
{
  setHeapManager(parent->getHeapManager());
  
  lo = parent->lo;
  hi = parent->hi;
  for (int i = 0; i < 32; i++)
    reg[i] = parent->reg[i];


  fcr0 = parent->fcr0;
  fcr31 = parent->fcr31;
  for (int i = 0; i < 32; i++)
    fp[i] = parent->fp[i];

  stacktop = parent->stacktop;

#if (defined TLS) || (defined TASKSCALAR)
  fd = parent->fd;
#endif
}

void ThreadContext::shareAddrSpace(thread_ptr parent, int share_all, int copy_stack)
{
  // The address space has already been allocated

  I(share_all); // share_all is the only supported

  setHeapManager(parent->getHeapManager());

  /* copy all the registers */
  for (int i = 0; i < 32; i++)
    reg[i] = parent->reg[i];
  lo = parent->lo;
  hi = parent->hi;
  for (int i = 0; i < 32; i++)
    fp[i] = parent->fp[i];
  fcr0 = parent->fcr0;
  fcr31 = parent->fcr31;

  if (copy_stack) {
    /* copy the stack */
    unsigned parent_sp = parent->reg[29];

    I(parent->reg[29] >= Stack_start && parent->reg[29] <= Stack_end);

    long *dest = (long *) (parent_sp + (pid - parent->pid) * Stack_size);
    long *last = (long *) (Stack_start + (parent->pid + 1) * Stack_size);
    long *src = (long *) parent_sp;
    while (src < last)
      *dest++ = *src++;

    /* change the stack pointer to point into the child's stack copy */
    reg[29] = parent->reg[29] + (pid - parent->pid) * Stack_size;

    I(reg[29] >= Stack_start && reg[29] <= Stack_end);

    stacktop = Stack_start + pid * Stack_size;
  } else {
    /* change the stack pointer to point to the top of the child's stack */
    reg[29] = Stack_start + (pid + 1) * Stack_size - FRAME_SIZE;
    /* Align stack (becuase of FRAME_SIZE)*/
    reg[29] = ((reg[29] + 0x1f) & ~0x1f);
    stacktop = Stack_start + pid * Stack_size;
    I(reg[29] >= Stack_start && reg[29] <= Stack_end);
  }
}

#ifdef TASKSCALAR
void ThreadContext::badSpecThread(VAddr addr, short opflags) const
{
  I(pid != -1);

  GLOG(pid == -1, "failed Addressing Invalid thread");

  if(!rsesc_is_safe(pid) && rsesc_is_versioned(pid)) {
    LOG("speculative thread using bad pointer. stopping thread. pid = %d. Iaddr=0x%08lx"
	, pid, osSim->eventGetInstructionPointer(pid)->addr);
    rsesc_exception(pid);
    return;
  }

  I(0); // warning, bad code?

  PAddr spawnAddr = 0xdeadbeef; // not valid address
  spawnAddr = TaskContext::getTaskContext(pid)->getSpawnAddr();

  bool badAddr  = !isValidVAddr(addr);
  bool badAlign = (
		   MemBufferEntry::calcAccessMask(opflags
						  ,MemBufferEntry::calcChunkOffset(addr))
		   ) == 0;

  GMSG(badAlign,"(failed) bad aligment 0x%3x [flags=0x%x] in safe thread. pid=%d. pc=0x%08lx  (R31=0x%08lx) (SA=0x%08lx)"
       , (int)addr, (int)opflags, pid, osSim->eventGetInstructionPointer(pid)->addr, osSim->getContextRegister(pid, 31),
       spawnAddr);


  GMSG(badAddr, "(failed) bad pointer 0x%3x [flags=0x%x] in safe thread. pid=%d. pc=0x%08lx  (R31=0x%08lx) (SA=0x%08lx)" 
       , (int)addr, (int)opflags, pid, osSim->eventGetInstructionPointer(pid)->addr, osSim->getContextRegister(pid, 31),
       spawnAddr);

  if(!badAddr)  // temporary ISCA03 hack
    return;

  mint_termination(pid);
}
#endif

// This assumes the address "addr" is known to be private.  Shared
// addresses do not need to be translated since the virtual-to-
// physical mapping is the identity function.
VAddr ThreadContext::real2virt(RAddr uaddr)
{
  signed long addr = (signed long)uaddr;

  // Direct mapping for heap and stack
  if (addr >= Heap_start && addr <= Private_end)
    return addr;

  addr = addr - DataMap;

  I(addr >= Data_start && addr < Data_end);

  return addr;
}

void ThreadContext::init()
{
  int i;

  reg[0]    = 0;
  rerrno    = 0;
  youngest  = NULL;

  /* map in the global errno */
  if (Errno_addr)
    perrno = (int *) virt2real(Errno_addr);
  else
    perrno = &rerrno;

  /* init all file descriptors to "closed" */
  for (i = 0; i < MAX_FDNUM; i++)
    fd[i] = 0;
}

void ThreadContext::initMainThread()
{
  int i, entry_index;
  thread_ptr pthread=getMainThreadContext();

  Maxpid = 0;

  pthread->init();

  pthread->parent = NULL;
  pthread->sibling = NULL;
#ifdef MIPS2_FNATIVE
  pthread->fcr31 = s_get_fcr31();
#else
  pthread->fcr31 = 0;
#endif

  /* Set the picode to the first executable instruction. */
  if (Text_entry == 0)
    Text_entry = Text_start;
  entry_index = (Text_entry - Text_start) / sizeof(long);
  pthread->picode = Itext[entry_index];

  /* initialize the icodes for the terminator functions */
  /* set up this picode so that terminator1() gets called */
  Idone1->func = terminator1;
  Idone1->opnum = terminate_opn;

  /* Set up the return address so that terminator1() gets called.
   * This probably isn't necessary since exit() gets called instead */
  pthread->reg[31]=(long)icode2addr(Idone1);
}

void ThreadContext::newChild(ThreadContext *child)
{
  child->parent = this;
 
  child->sibling = youngest;
  youngest = child;
}
