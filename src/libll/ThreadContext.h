#ifndef THREADCONTEXT_H
#define THREADCONTEXT_H

#include <stdint.h>
#include <vector>
#include "event.h"
#if (defined ADDRESS_SPACES)
#include "AddressSpace.h"
#endif // (defined ADDRESS_SPACES)
#include "HeapManager.h"
#include "icode.h"

#if (defined TLS)
namespace tls {
  class Epoch;
}
#endif

/* For future support of the exec() call. Information needed about
 * the object file goes here.
 */
typedef struct objfile {
  int rdata_size;
  int db_size;
  char *objname;
} objfile_t, *objfile_ptr;

/* The maximum number of file descriptors per thread. */
#define MAX_FDNUM 20

// Simulated machine has 32-bit registers
typedef int32_t IntRegValue;

enum IntRegName{
  RetValReg   =  2,
  RetValHiReg =  2,
  RetValLoReg =  3,
  IntArg1Reg  =  4,
  IntArg2Reg  =  5,
  IntArg3Reg  =  6,
  IntArg4Reg  =  7,
  JmpPtrReg   = 25,
  GlobPtrReg  = 28,
  StkPtrReg   = 29,
  RetAddrReg  = 31
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

#if !(defined ADDRESS_SPACES)
  // Lower and upper bound for valid data addresses
  VAddr dataVAddrLb;
  VAddr dataVAddrUb;
  // Lower and upper bound for stack addresses in all threads
  VAddr allStacksAddrLb;
  VAddr allStacksAddrUb;
#endif //!(defined ADDRESS_SPACES)
  // Lower and upper bound for stack addresses in this thread
  VAddr myStackAddrLb;
  VAddr myStackAddrUb;

#if !(defined ADDRESS_SPACES)
  // Real address is simply virtual address plus this offset
  RAddr virtToRealOffset;
#endif //!(defined ADDRESS_SPACES)

  // Local Variables
 public:
  IntRegValue reg[33];	// The extra register is for writes to r0
  int lo;
  int hi;
 private:
  unsigned int fcr0;	// floating point control register 0
  float fp[32];	        // floating point (and double) registers
  unsigned int fcr31;	// floating point control register 31
  icode_ptr picode;	// pointer to the next instruction (the "pc")
  int pid;		// process id
  RAddr raddr;		// real address computed by an event function
  icode_ptr target;	// place to store branch target during delay slot

  /* Addresses MUST be signed */
#if (defined ADDRESS_SPACES)
  AddressSpace *addressSpace; // Address space for this thread
#endif // (defined ADDRESS_SPACES)
  HeapManager  *heapManager;  // Heap manager for this thread

  ThreadContext *parent;    // pointer to parent
  ThreadContext *youngest;  // pointer to youngest child
  ThreadContext *sibling;   // pointer to next older sibling
  int *perrno;	            // pointer to the errno variable
  int rerrno;		    // most recent errno for this thread

  char      *fd;	    // file descriptors; =1 means open, =0 means closed

private:

#ifdef TASKSCALAR
  void badSpecThread(VAddr addr, short opflags) const;
  bool checkSpecThread(VAddr addr, short opflags) const {
    if (isValidDataVAddr(addr))
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
  inline IntRegValue getIntReg(IntRegName name) const {
    return reg[name];
  }
  inline void setIntReg(IntRegName name, IntRegValue value) {
    reg[name]=value;
  }
  
  inline IntRegValue getIntArg1(void) const{
    return getIntReg(IntArg1Reg);
  }
  inline IntRegValue getIntArg2(void) const{
    return getIntReg(IntArg2Reg);
  }
  inline IntRegValue getIntArg3(void) const{
    return getIntReg(IntArg3Reg);
  }
  inline IntRegValue getIntArg4(void) const{
    return getIntReg(IntArg4Reg);
  }
  inline VAddr getGlobPtr(void) const{
    return getIntReg(GlobPtrReg);
  }
  inline void setGlobPtr(VAddr addr){
    setIntReg(GlobPtrReg,addr);
  }
  inline VAddr getStkPtr(void) const{
    return getIntReg(StkPtrReg);
  }
  inline void setStkPtr(int val){
    I(sizeof(val)==4);
    setIntReg(StkPtrReg,val);
  }
  inline void setRetVal(int val){
    I(sizeof(val)==4);
    setIntReg(RetValReg,val);
  }
  inline void setRetVal64(long long int val){
    I(sizeof(val)==8);
    unsigned long long valLo=val;
    valLo&=0xFFFFFFFFllu;
    unsigned long long valHi=val;
    valHi>>=32;
    valHi&=0xFFFFFFFFllu;
    setIntReg(RetValLoReg,(IntRegValue)valLo);
    setIntReg(RetValHiReg,(IntRegValue)valHi);
  }
  

  inline icode_ptr getPCIcode(void) const{
    I((pid!=-1)||(picode==&invalidIcode));
    return picode;
  }
  inline void setPCIcode(icode_ptr nextIcode){
    I((pid!=-1)||(nextIcode==&invalidIcode));
    picode=nextIcode;
  }
  
  inline icode_ptr getRetIcode(void) const{
    return addr2icode(getIntReg(RetAddrReg));
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

  unsigned int getFPUControl31() const { return fcr31; }
  void setFPUControl31(unsigned int v) {
    fcr31 = v;
  }

  unsigned int getFPUControl0() const { return fcr0; }
  void setFPUControl0(unsigned int v) {
    fcr0 = v;
  }

  int getREG(icode_ptr pi, int R) { return reg[pi->args[R]];}
  void setREG(icode_ptr pi, int R, int val) { 
    reg[pi->args[R]] = val;
  }

  void setREGFromMem(icode_ptr pi, int R, int *addr) {
#ifdef LENDIAN
    int val = SWAP_WORD(*addr);
#else
    int val = *addr;
#endif
    setREG(pi, R, val);
  }

  float getFP( icode_ptr pi, int R) { return fp[pi->args[R]]; }
  void  setFP( icode_ptr pi, int R, float val) { 
    fp[pi->args[R]] = val; 
  }
  void  setFPFromMem( icode_ptr pi, int R, float *addr) { 
    float *pos = &fp[pi->args[R]];
#ifdef LENDIAN
    unsigned int v1;
    v1 = *(unsigned int *)addr;
    v1 = SWAP_WORD(v1);
    *pos = *(float *)&v1;
#else
    *pos = *addr;
#endif
  }

  double getDP( icode_ptr pi, int R) const { 
#ifdef SPARC 
  // MIPS supports 32 bit align double access
    unsigned int w1 = *(unsigned int *) &fp[pi->args[R]];
    unsigned int w2 = *(unsigned int *) &fp[pi->args[R]+1];
    static unsigned long long ret = w2;
    ret = w2;
    ret = (ret<<32) | w1;
    return *(double *) (&ret);
#else 
    return *(double *) &fp[pi->args[R]];
#endif
  }

  void   setDP( icode_ptr pi, int R, double val) { 
#ifdef SPARC 
    unsigned int *pos = (unsigned int*)&fp[pi->args[R]];
    unsigned int b1 = ((unsigned int *)&val)[0];
    unsigned int b2 = ((unsigned int *)&val)[1];
    pos[0] = b1;
    pos[1] = b2;	
#else
    *((double *)&fp[pi->args[R]]) = val; 
#endif
  }


  void   setDPFromMem( icode_ptr pi, int R, double *addr) { 
#ifdef SPARC 
    unsigned int *pos = (unsigned int*) ((long)(fp) + pi->args[R]);
    pos[0] = (unsigned int) addr[0];
    pos[1] = (unsigned int) addr[1];
#else
    double *pos = (double *) &fp[pi->args[R]];
#ifdef LENDIAN
    unsigned long long v1;
    v1 = *(unsigned long long *)(addr);
    v1 = SWAP_LONG(v1);
    *pos = *(double *)&v1;
#else
    *pos = *addr;
#endif // LENDIAN
#endif // SPARC
  }

  int getWFP(icode_ptr pi, int R) { return *(int   *)&fp[pi->args[R]]; }
  void setWFP(icode_ptr pi, int R, int val) { 
    *((int   *)&fp[pi->args[R]]) = val; 
  }

  // Methods used by ops.m4 and coproc.m4 (mostly)
  int getREGNUM(int R) const { return reg[R]; }
  void setREGNUM(int R, int val) {
    reg[R] = val;
  }

  // FIXME: SPARC
  double getDPNUM(int R) {return *((double *)&fp[R]); }
  void   setDPNUM(int R, double val) {
    *((double *) &fp[R]) = val; 
  }

  // for constant (or unshifted) register indices
  float getFPNUM(int i) const { return fp[i]; }
  int getWFPNUM(int i) const  { return *((int *)&fp[i]); }

  RAddr getRAddr() const{
    return raddr;
  }
  void setRAddr(RAddr a){
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
  bool isValidDataVAddr(VAddr vaddr) const{
#if !(defined ADDRESS_SPACES)
    return (vaddr>=dataVAddrLb)&&(vaddr<dataVAddrUb);
#else
    return (addressSpace->virtToReal(vaddr)!=0);
#endif
  }

#if (defined ADDRESS_SPACES)
  void setAddressSpace(AddressSpace *newAddressSpace){
    I(!addressSpace);
    addressSpace=newAddressSpace;
    addressSpace->addReference();
  }
  AddressSpace *getAddressSpace(void) const{
    I(addressSpace);
    return addressSpace;
  }
#endif // (defined ADDRESS_SPACES)

  void setHeapManager(HeapManager *newHeapManager){
    I(!heapManager);
    heapManager=newHeapManager;
    heapManager->addReference();
  }

  HeapManager *getHeapManager(void) const{
    I(heapManager);
    return heapManager;
  }

  void setStack(VAddr stackLb, VAddr stackUb){
    myStackAddrLb=stackLb;
    myStackAddrUb=stackUb;
  }

#if !(defined ADDRESS_SPACES)
  void initAddressing(VAddr dataVAddrLb, VAddr dataVAddrUb,
		      MINTAddrType rMap, MINTAddrType mMap, MINTAddrType sTop);
#endif // !(defined ADDRESS_SPACES)

  RAddr virt2real(VAddr vaddr, short opflags=E_READ | E_BYTE) const{
#ifdef TASKSCALAR
    if(checkSpecThread(vaddr, opflags))
      return 0;
#endif
#if (defined TLS)
    if(!isValidDataVAddr(vaddr))
      return 0;    
#endif
#if !(defined ADDRESS_SPACES)
    I(isValidDataVAddr(vaddr));
    return virtToRealOffset+vaddr;
#else // (defined ADDRESS_SPACES)
    return addressSpace->virtToReal(vaddr);
#endif // (defined ADDRESS_SPACES)
  }
  VAddr real2virt(RAddr raddr) const{
#if !(defined ADDRESS_SPACES)
    VAddr vaddr=raddr-virtToRealOffset;
    I(isValidDataVAddr(vaddr));
    return vaddr;
#else // (defined ADDRESS_SPACES)
    return addressSpace->realToVirt(raddr);
#endif // (defined ADDRESS_SPACES)
  }

  bool isHeapData(VAddr addr) const{
    I(heapManager);
    return heapManager->isHeapAddr(addr);
  }

  bool isLocalStackData(VAddr addr) const {
    return (addr>=myStackAddrLb)&&(addr<myStackAddrUb);
  }

  VAddr getStackTop() const{
    return myStackAddrLb;
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

  int getperrno() const { return *perrno; }
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

// This class helps extract function parameters for substitutions (e.g. in subs.cpp)
// First, prepare for parameter extraction by constructing an instance of MintFuncArgs.
// The constructor takes as a parameter the ThreadContext in which a function has just
// been called (i.e. right after the jal and the delay slot have been emulated
// Then get the parameters in order from first to last, using getInt32 or getInt64
// MintFuncArgs automatically takes care of getting the needed parameter from the register
// or from the stack, according to the MIPS III caling convention. In particular, it correctly
// implements extraction of 64-bit parameters, allowing lstat64 and similar functions to work
// The entire process of parameter extraction does not change the thread context in any way
class MintFuncArgs{
 private:
  const ThreadContext *myContext;
  const icode_t *myIcode;
  int   curPos;
 public:
  MintFuncArgs(const ThreadContext *context, const icode_t *picode)
    : myContext(context), myIcode(picode), curPos(0)
    {
    }
  int getInt32(void);
  long long int getInt64(void);
  VAddr getVAddr(void){ return (VAddr)getInt32(); }
};

#define REGNUM(R) (*((int *) &pthread->reg[R]))

#endif // THREADCONTEXT_H
