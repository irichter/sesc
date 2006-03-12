
#include "icode.h"
#include "ThreadContext.h"
#include "globals.h"
#include "opcodes.h"

#ifdef TASKSCALAR
#include "MemBuffer.h"
#include "TaskContext.h"
#endif

#include "Events.h"

#if (defined TLS)
#include "Epoch.h"
#endif

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

RAddr ThreadContext::DataStart;
RAddr ThreadContext::DataEnd;
MINTAddrType ThreadContext::DataMap;

RAddr ThreadContext::HeapStart;
RAddr ThreadContext::HeapEnd;

RAddr ThreadContext::StackStart;
RAddr ThreadContext::StackEnd;

RAddr ThreadContext::PrivateStart;
RAddr ThreadContext::PrivateEnd;


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

void ThreadContext::initAddressing(MINTAddrType rMap, MINTAddrType mMap, MINTAddrType sTop)
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
#if defined (__x86_64__)
  DataMap -= Data_start;
#endif
    
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

  RAddr privSize = PrivateEnd-PrivateStart;
  
  MSG("Data[0x%x-0x%x]->[0x%x-...] Private[%p-%p] heap[%p-%p] stack[%p-%p]"
      ,(unsigned)DataStart     ,(unsigned)DataEnd     ,(unsigned)DataStart-DataMap
      ,(void*)PrivateStart ,(void*)PrivateEnd
      ,(void*)HeapStart     ,(void*)HeapEnd
      ,(void*)StackStart    ,(void*)StackEnd
      );

  if( (PrivateEnd > DataStart  && PrivateEnd < DataEnd)
                || (PrivateStart > DataStart  && PrivateStart < DataEnd) ) {
    MSG("There is an overlap between private and shared memory");
    MSG("This is not allowed, try increase/decrease -h parameter");
    MSG("or change mint memory allocation scheme");
    exit(-1);
  }
}

void ThreadContext::dump()
{
  int i, j;

  printf("thread 0x%p:\n", this);
  for (i = 0; i < 32; ) {
    for (j = 0; j < 4; j++, i++)
      printf("  r%d: %s 0x%08x", i, i < 10 ? " " : "", reg[i]);
    printf("\n");
  }
  printf("  lo:   0x%08x  hi:   0x%08x\n", lo, hi);

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
      printf("  $w%d:%s 0x%08x", i, i < 10 ? " " : "", getWFPNUM(i));
    printf("\n");
  }

  printf("  fcr0 = 0x%x, fcr31 = 0x%x\n", fcr0, fcr31);

  printf("  target = 0x%p, pid = %d\n", target, pid);

  printf("  parent = 0x%p, youngest = 0x%p, sibling = 0x%p\n",
         parent, youngest,  sibling);
}

/* dump the stack for stksize number of words */
void ThreadContext::dumpStack()
{
  int stksize = Stack_size;
  VAddr sp;
  VAddr i;
  int j;
  unsigned char c;

  sp = getREGNUM(29);
  printf("sp = 0x%08x\n", (unsigned) sp);
  for (i = sp + (stksize - 1) * 4; i >= sp; i -= 4) {
    printf("0x%08x (+%d): 0x%x (%f)  ",
           i,  (i - sp), *(int *) this->virt2real(i), *(float *) this->virt2real(i));
    for (j = 0; j < 4; j++) {
      c = *((char *) this->virt2real(i) + j);
      if (c < ' ')
        c += 'A' - 1;
      else if (c >= 0x7f)
        c = '?';
      printf("^%c", c);
    }
    printf("\n");
  }
}

void ThreadContext::useSameAddrSpace(thread_ptr pthread)
{
  setHeapManager(pthread->getHeapManager());
  
  lo = pthread->lo;
  hi = pthread->hi;
  for (int i = 0; i < 32; i++)
    reg[i] = pthread->reg[i];


  fcr0 = pthread->fcr0;
  fcr31 = pthread->fcr31;
  for (int i = 0; i < 32; i++)
    fp[i] = pthread->fp[i];

  stacktop = pthread->stacktop;

#if (defined TLS) || (defined TASKSCALAR)
  fd = pthread->fd;
#endif
}

void ThreadContext::shareAddrSpace(thread_ptr pthread, int share_all, int copy_stack)
{
  // The address space has already been allocated

  I(share_all); // share_all is the only supported

  setHeapManager(pthread->getHeapManager());

  /* copy all the registers */
  for (int i = 0; i < 32; i++)
    reg[i] = pthread->reg[i];
  lo = pthread->lo;
  hi = pthread->hi;
  for (int i = 0; i < 32; i++)
    fp[i] = pthread->fp[i];
  fcr0 = pthread->fcr0;
  fcr31 = pthread->fcr31;

  if (copy_stack) {
#if (defined TLS)
    // Need to use TLS read/write methods to properly copy the stack
    I(!copy_stack);
#endif
    /* copy the stack */
    unsigned pthread_sp = pthread->reg[29];

    I(pthread->reg[29] >= Stack_start && pthread->reg[29] <= Stack_end);

    int *dest = (int *) (pthread_sp + (pid - pthread->pid) * Stack_size);
    int *last = (int *) (Stack_start + (pthread->pid + 1) * Stack_size);
    int *src = (int *) this->virt2real(pthread_sp);
    while (src < last)
      *dest++ = *src++;

    /* change the stack pointer to point into the child's stack copy */
    reg[29] = pthread->reg[29] + (pid - pthread->pid) * Stack_size;

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
    LOG("speculative thread using bad pointer. stopping thread. pid = %d. Iaddr=0x%08x"
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

  GMSG(badAlign,"(failed) bad aligment 0x%3x [flags=0x%x] in safe thread. pid=%d. pc=0x%08x  (R31=0x%08x) (SA=0x%08x)"
       , (int)addr, (int)opflags, pid, osSim->eventGetInstructionPointer(pid)->addr, osSim->getContextRegister(pid, 31),
       spawnAddr);


  GMSG(badAddr, "(failed) bad pointer 0x%3x [flags=0x%x] in safe thread. pid=%d. pc=0x%08x  (R31=0x%08x) (SA=0x%08x)" 
       , (int)addr, (int)opflags, pid, osSim->eventGetInstructionPointer(pid)->addr, osSim->getContextRegister(pid, 31),
       spawnAddr);

  if(!badAddr)  // temporary ISCA03 hack
    return;

  mint_termination(pid);
}
#endif

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
  entry_index = (Text_entry - Text_start) / sizeof(int);
  pthread->picode = Itext[entry_index];

  /* initialize the icodes for the terminator functions */
  /* set up this picode so that terminator1() gets called */
  Idone1->func = terminator1;
  Idone1->opnum = terminate_opn;

  /* Set up the return address so that terminator1() gets called.
   * This probably isn't necessary since exit() gets called instead */
  pthread->reg[31]=(int)icode2addr(Idone1);
}

void ThreadContext::newChild(ThreadContext *child)
{
  child->parent = this;
 
  child->sibling = youngest;
  youngest = child;
}

Pid_t ThreadContext::getThreadPid(void) const{
#if (defined TLS)
  I(getEpoch());
  return getEpoch()->getTid();
#else
  return getPid();
#endif
}

unsigned long long ThreadContext::getMemValue(RAddr p, unsigned dsize) {
  unsigned long long value = 0;
  switch(dsize) {
  case 1:
    value = *((unsigned char *) p);
    break;
  case 2:
    value = *((unsigned short *) p);
#ifdef LENDIAN
    value = SWAP_SHORT(value);
#endif      
    break;
  case 4:
    value = *((unsigned *) p);
#ifdef LENDIAN
    value = SWAP_WORD(value);
#endif      
    break;
  case 8:
    value = *((unsigned long long*) p);
#ifdef LENDIAN
    value = SWAP_LONG(value);
#endif      
    break;
  default:
    MSG("ThreadContext:warning, getMemValue with bad (%d) data size.", dsize);
    value = 0;
    break;
  }
  
  return value;
}

RAddr ThreadContext::virt2real(VAddr vaddr, short opflags) const {
#ifdef __LP64__
    MINTAddrType m = (MINTAddrType)vaddr + DataMap;
    RAddr r = static_cast<RAddr>(m);
    MSG("SESC64: %p->virt2real(0x%x) = (0x%x + %p) = %p\n", (void*)this, vaddr, vaddr, (void*)DataMap, (void*)r);
    fflush(stdout);
    
    if(!(m >= Data_start && m <= Private_end-Data_start)) {
      MSG("SESC64: virtual address %p is out of bounds", (void*)m);
      exit(1);
    }
    
#ifdef TASKSCALAR
#error "Can not compile TLS/TaskScalar with 64bit architectures. Still not working"
#endif
#else
#ifdef TASKSCALAR
    if(checkSpecThread(vaddr, opflags))
      return 0;
#endif
#if (defined TLS)
    if(!isValidVAddr(vaddr))
      return 0;    
#endif             
    RAddr r = (vaddr >= DataStart && vaddr < DataEnd) ? 
      static_cast<RAddr>(((signed)vaddr+ DataMap)) : vaddr;
    
    I(isPrivateRAddr(r)); // Once it is translated, it should map to
                          // the beginning of the private map
#endif

#ifdef __LP64__
      printf("\n%p->virt2real(0x%x) = %p", (void*)this, vaddr, (void*)r);
      fflush(stdout);
#endif
    
    return r;
}
  
// This assumes the address "addr" is known to be private.  Shared
// addresses do not need to be translated since the virtual-to-
// physical mapping is the identity function.
VAddr ThreadContext::real2virt(RAddr uaddr) const {
#ifdef __x86_64__
  uaddr = uaddr - DataMap;

  return (VAddr)uaddr;
#else
  signed int addr = (signed int)uaddr;

  // Direct mapping for heap and stack
  if (addr >= Heap_start && addr <= Private_end)
    return addr;

  addr = addr - DataMap;

  I(addr >= Data_start && addr < Data_end);

  return addr;
#endif
}

int MintFuncArgs::getInt32(void) {
  int retVal; 
  I(sizeof(retVal)==4);
  I((curPos % 4)==0);
  if(curPos<16){
    I(curPos % 4==0);
    retVal=myContext->getIntReg((IntRegName)(4+curPos/4));
  }else{
    RAddr addr=myContext->virt2real(myContext->getStkPtr())+curPos;
#if (defined TASKSCALAR) || (defined TLS)
    int *ptr =(int *)(rsesc_OS_read(myContext->getPid(),myIcode->addr,addr,E_WORD));
#else
    int *ptr=(int *)addr;
#endif
    retVal=SWAP_WORD(*ptr);
  }                                     
  curPos+=4;
  return retVal;
}           

long long int MintFuncArgs::getInt64(void){
  long long int retVal;
  I(sizeof(retVal)==8);
  I((curPos%4)==0);
  // Align current position                  
  if(curPos%8!=0)
    curPos+=4;               
  I(curPos%8==0);
  if(curPos<16){
    retVal=myContext->getIntReg((IntRegName)(4+curPos/4));
    retVal=(retVal<<32)&0xFFFFFFFF00000000llu;
    retVal|=myContext->getIntReg((IntRegName)(4+curPos/4+1))&0xFFFFFFFFllu;
  }else{
    RAddr addr=myContext->virt2real(myContext->getStkPtr())+curPos;
#if (defined TASKSCALAR) || (defined TLS)
    long long int *ptr =
      (long long int *)(rsesc_OS_read(myContext->getPid(),myIcode->addr,addr,E_DWORD));
#else
    long long int *ptr=(long long int*)addr;
#endif
    retVal=SWAP_LONG(*ptr);
  }
  curPos+=8;
  return retVal;
}
