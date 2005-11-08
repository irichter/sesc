#ifndef THREADCONTEXT_H
#define THREADCONTEXT_H

#include <vector>
#include "event.h"
#include "HeapManager.h"
#include "icode.h"

#if (defined TLS)
namespace tls {
  class Epoch;
}
#endif

typedef ulong VAddr; // Virtual Addresses inside the simulator
typedef ulong RAddr; // Real Addresses. Where is the data mapped
typedef ulong PAddr; // Phisical translation (memory subsystem view of VAddr)

/* For future support of the exec() call. Information needed about
 * the object file goes here.
 */
typedef struct objfile {
  long rdata_size;
  long db_size;
  char *objname;
} objfile_t, *objfile_ptr;

/* The maximum number of file descriptors per thread. */
#define MAX_FDNUM 20

typedef long ValueGPR;
enum NameGPR {
  RetValGPR  = 2,
  Arg1stGPR  = 4,
  Arg2ndGPR  = 5,
  Arg3rdGPR  = 6,
  Arg4thGPR  = 7,
  RetAddrGPR = 31
};


class ThreadContext {
  // Static variables
  typedef std::vector<ThreadContext *> ContextVector;
  static ContextVector pid2context;
  
  static Pid_t baseClonedPid;
  static Pid_t nextClonedPid;
  static ContextVector clonedPool;

  static size_t nThreads;

  static Pid_t baseActualPid;
  static Pid_t nextActualPid;
  static ContextVector actualPool;
  static ThreadContext *mainThreadContext;

  // Memory Mapping
  static ulong DataStart;
  static ulong DataEnd;
  static signed long DataMap;  // Must be signed because it may increase or decrease

  static ulong HeapStart;
  static ulong HeapEnd;
  
  static ulong StackStart;
  static ulong StackEnd;

  static ulong PrivateStart;
  static ulong PrivateEnd;

  // Local Variables
 public:
  ValueGPR reg[33];	// The extra register is for writes to r0
  long lo;
  long hi;
 private:
  long fcr0;		// floating point control register 0
  float fp[32];	        // floating point (and double) registers
  unsigned long fcr31;	// floating point control register 31
  icode_ptr picode;	// pointer to the next instruction (the "pc")
  int pid;		// process id
  RAddr raddr;		// real address computed by an event function
  icode_ptr target;	// place to store branch target during delay slot

  /* Addresses MUST be signed */
  HeapManager *heapManager; // Heap manager for this thread
  ThreadContext *parent;    // pointer to parent
  ThreadContext *youngest;  // pointer to youngest child
  ThreadContext *sibling;   // pointer to next older sibling
  RAddr stacktop;	    // lowest address allowed in the stack
  int *perrno;	            // pointer to the errno variable
  int rerrno;		    // most recent errno for this thread

  char      *fd;	    // file descriptors; =1 means open, =0 means closed

private:
  static bool isDataVAddr(VAddr addr)  {
    return addr >= ((ulong) DataStart) && addr <= ((ulong) DataEnd);
  }

#ifdef TASKSCALAR
  void badSpecThread(VAddr addr, short opflags) const;
  bool checkSpecThread(VAddr addr, short opflags) const {
    if (isValidVAddr(addr))
      return false;

#if 0
    bool badAlign = (MemBufferEntry::calcAccessMask(opflags
						    ,MemBufferEntry::calcChunkOffset(addr))
		     ) == 0;

    if (!badAddr && !badAlign)
      return false;
#endif
    badSpecThread(addr, opflags);
    return true;
  }
#endif

#if (defined TLS)
  tls::Epoch *myEpoch;
#endif

public:
  ValueGPR getGPR(NameGPR name) const {
    return reg[name];
  }

  void setGPR(NameGPR name, ValueGPR value) {
    reg[name]=value;
  }

  icode_ptr getIP(void) const {
    I(pid!=-1);
    return picode;
  }
  void setIP(icode_ptr newIP) {
    I(pid!=-1);
    picode=newIP;
  }
  
  // Returns the pid of the thread (what would be returned by a getpid call)
  // In TLS, many contexts can share the same actual thread pid
  Pid_t getThreadPid(void) const;
  // Returns the pid of the context
  Pid_t getPid(void) const { return pid; }
  // Sets the pid of the context
  void setPid(Pid_t newPid){
    pid=newPid;
  }

  int getErrno(void){
    return *perrno;
  }

  void setErrno(int newErrno){
    I(perrno);
    *perrno=newErrno;
  }

#if (defined TLS)
 void setEpoch(tls::Epoch *epoch){
   myEpoch=epoch;
 }
 tls::Epoch *getEpoch(void) const{
   return myEpoch;
 }
#endif // (defined TLS)

  bool isCloned(void) const{ return (pid>=baseClonedPid); }

  void copy(const ThreadContext *src);

  unsigned long getFPUControl31() const { return fcr31; }
  void setFPUControl31(unsigned long v) {
    fcr31 = v;
  }

  long getFPUControl0() const { return fcr0; }
  void setFPUControl0(long v) {
    fcr0 = v;
  }

  long getREG(icode_ptr pi, int R) { return *(long   *) ((long)(reg) + pi->args[R]);}
  void setREG(icode_ptr pi, int R, long val) { 
    (*(long   *) ((long)(reg) + pi->args[R])) = val;
  }

  void setREGFromMem(icode_ptr pi, int R, long *addr) {
    long *pos = (long   *) ((long)(reg) + pi->args[R]);
    
#ifdef LENDIAN
    *pos = SWAP_WORD(*addr);
#else
    *pos = *addr;
#endif
  }

  float getFP( icode_ptr pi, int R) { return *(float  *) ((long)(fp) + pi->args[R]); }
  void  setFP( icode_ptr pi, int R, float val) { 
    (*(float  *) ((long)(fp) + pi->args[R])) = val; 
  }
  void  setFPFromMem( icode_ptr pi, int R, float *addr) { 
    float *pos = (float  *) ((long)(fp) + pi->args[R]);
#ifdef LENDIAN
    unsigned long v1;
    v1 = *(unsigned long *)addr;
    v1 = SWAP_WORD(v1);
    *pos = *(float *)&v1;
#else
    *pos = *addr;
#endif
  }
  
  double getDP( icode_ptr pi, int R) { return *(double *) ((long)(fp) + pi->args[R]); }
  void   setDP( icode_ptr pi, int R, double val) { 
    (*(double *) ((long)(fp) + pi->args[R])) = val; 
  }
  void   setDPFromMem( icode_ptr pi, int R, double *addr) { 
    double *pos = (double *) ((long)(fp) + pi->args[R]);
#ifdef LENDIAN
    unsigned long long v1;
    v1 = *(unsigned long long *)(addr);
    v1 = SWAP_LONG(v1);
    *pos = *(double *)&v1;
#else
    *pos = *addr;
#endif
  }

  long getWFP(icode_ptr pi, int R) { return *(long   *) ((long)(fp) + pi->args[R]); }
  void setWFP(icode_ptr pi, int R, long val) { 
    (*(long   *) ((long)(fp) + pi->args[R])) = val ; 
  }

  // Methods used by ops.m4 and coproc.m4 (mostly)
  long getREGNUM(int R) { return (* (long *) ((long)(reg) + ((R) << 2))); }
  void setREGNUM(int R, long val) {
    (* (long *) ((long)(reg) + ((R) << 2))) = val;
  }

  double getDPNUM(int R) {return (* (double *) ((long)(fp) + ((R) << 2))); }
  void   setDPNUM(int R, double val) {
    (* (double *) ((long)(fp) + ((R) << 2))) = val; 
  }

  // for constant (or unshifted) register indices

#if defined(LENDIAN)
  float getFPNUM(int i) {
    return (* (float *) ((long)(fp) + ((i) << 2)));
  }
  long getWFPNUM(int i) {
    return (* (long *) ((long)(fp) + ((i) << 2)));
  }
#else
  float getFPNUM(int i) {
    return (* (float *) ((long)(fp) + ((i^1) << 2)));
  }
  long getWFPNUM(int i) {
    return (* (long *) ((long)(fp) + ((i^1) << 2)));
  }
#endif

  RAddr getRAddr() const { return raddr; }
  void setRAddr(RAddr a) {
    raddr = a;
  }


  void dump();
  void dumpStack();

  static void staticConstructor(void);

  static size_t size();

  static ThreadContext *getContext(Pid_t pid);
  static ThreadContext *getMainThreadContext(void);

  static ThreadContext *newActual(void);
  static ThreadContext *newActual(Pid_t pid);
  static ThreadContext *newCloned(void);
  void free(void);

  static unsigned long long getMemValue(RAddr p, unsigned dsize); 

  // BEGIN Memory Mapping
  static bool isPrivateVAddr(VAddr addr)  {
    return addr >= ((ulong) PrivateStart) &&  addr <= ((ulong) PrivateEnd);
  }

  static bool isValidVAddr(VAddr addr)  {
    return isPrivateVAddr(addr) || isDataVAddr(addr);
  }

  void setHeapManager(HeapManager *newHeapManager) {
    I(heapManager==0);
    heapManager=newHeapManager;
    heapManager->addReference();
  }

  HeapManager *getHeapManager(void) {
    I(heapManager);
    return heapManager;
  }

  void initAddressing(signed long rMap, signed long mMap, signed long sTop);

  RAddr virt2real(VAddr vaddr, short opflags =E_READ | E_BYTE) const {
#ifdef TASKSCALAR
    if(checkSpecThread(vaddr, opflags))
      return 0;
#endif
    RAddr r = (vaddr >= DataStart && vaddr < DataEnd) ? 
      static_cast<RAddr>(((signed)vaddr+ DataMap)) : vaddr;
    
    I(isPrivateVAddr(r)); // Once it is translated, it should map to
			  // the beginning of the private map
    
    return r;
  }
  VAddr real2virt(RAddr addr);

  bool isHeapData(RAddr addr) {
    return addr >= HeapStart && addr <= HeapEnd;
  }

  bool isStackData(RAddr addr) {
    return addr >= StackStart && addr <= StackEnd;
  }

  RAddr getStackTop() const {
    return stacktop;
  }
  // END Memory Mapping

  ThreadContext *getParent() const { return parent; }

  void useSameAddrSpace(ThreadContext *parent);
  void shareAddrSpace(ThreadContext *parent, int share_all, int copy_stack);

  void init();

  void newChild(ThreadContext *child);

  icode_ptr getPicode() const { return picode; }
  void setPicode(icode_ptr p) {
    picode = p;
  }

  icode_ptr getTarget() const { return target; }
  void setTarget(icode_ptr p) {
    target = p;
  }

  long getperrno() const { return *perrno; }
  void setperrno(int v) {
    I(perrno);
    *perrno = v;
  }

  int getFD(int id) const { return fd[id]; }
  void setFD(int id, int val) {
    fd[id] = val;
  }
  
  static void initMainThread();
};

typedef ThreadContext mint_thread_t;

#define REGNUM(R) (* (long *) ((long)(pthread->reg) + ((R) << 2)))

#endif // THREADCONTEXT_H
