#include "LinuxSys.h"
#include "ThreadContext.h"
// Get declaration of fail()
#include "EmulInit.h"
#include "OSSim.h"
#include "ElfObject.h"

#include "MipsRegs.h"
#include "Mips32Defs.h"

#include <map>

//#define DEBUG_FILES
//#define DEBUG_VMEM
//#define DEBUG_SOCKET
enum{
  BaseSimTid = 1000
};

class Tstr{
 private:
  char *s;
 public:
  Tstr(ThreadContext *context, VAddr addr) : s(0){
    ssize_t len=context->readMemString(addr,0,0);
    if(len==-1){
      context->getSystem()->setSysErr(context,LinuxSys::ErrFault);
    }else{
      s=new char[len];
      ssize_t chklen=context->readMemString(addr,len,s);
      I(chklen==len);
    }
  }
  ~Tstr(void){
    delete [] s;
  }
  operator const char *() const{
    return s;
  }
  operator bool() const{
    return (s!=0);
  }
};

static VAddr  sysCodeAddr=AddrSpacPageSize;
static size_t sysCodeSize=6*sizeof(uint32_t);

LinuxSys::ErrCode LinuxSys::getErrCode(int err){
  if(err==ENOENT)  return ErrNoEnt;
  if(err==ENXIO)   return ErrNXIO;
  if(err==ENOEXEC) return ErrNoExec;
  if(err==ECHILD)  return ErrChild;
  if(err==EAGAIN)  return ErrAgain;
  if(err==ENOMEM)  return ErrNoMem;
  if(err==EACCES)  return ErrAccess;
  if(err==EFAULT)  return ErrFault;
  if(err==EINVAL)  return ErrInval;
  if(err==ENOTTY)  return ErrNoTTY;
  if(err==ESPIPE)  return ErrSPipe;
  if(err==ENOSYS)  return ErrNoSys;
  fail("LinuxSys::getErrCode with unsupported native error code %d\n",err);
}

LinuxSys *LinuxSys::create(CpuMode cpuMode){
  switch(cpuMode){
  case Mips32:
    return new Mips32LinuxSys();
  default:
    fail("LinuxSys::create with unsupported cpuMode=%d\n",cpuMode);
  }
  return 0;
}
  
LinuxSys::FuncArgs::FuncArgs(ThreadContext *context)
  : context(context), pos(0)
{
}

template<typename T>
T LinuxSys::FuncArgs::get(void){
  switch(sizeof(T)){
  case sizeof(uint32_t):
    return static_cast<T>(context->getSystem()->getArgI32(context,pos));
  case sizeof(uint64_t):
    return static_cast<T>(context->getSystem()->getArgI64(context,pos));
  default:
    fail("LinuxSys::FuncArgs::get() for type of size %d unsupported\n",sizeof(T));
  }
}

template<typename T>
void LinuxSys::pushScalar(ThreadContext *context, const T &val) const{
  VAddr sp=getStackPointer(context);
  I(stackGrowsDown());
  sp-=sizeof(T);
  context->writeMem<T>(sp,val);
  setStackPointer(context,sp);
}
template<typename T>
T LinuxSys::popScalar(ThreadContext *context) const{
  VAddr sp=getStackPointer(context);
  I(stackGrowsDown());
  setStackPointer(context,sp+sizeof(T));
  return context->readMem<T>(sp);
}
template<typename T>
void LinuxSys::pushStruct(ThreadContext *context, const T &val) const{
  VAddr sp=getStackPointer(context);
  I(stackGrowsDown());
  sp-=sizeof(T);
  val.write(context,sp);
  setStackPointer(context,sp);
}
template<typename T>
T LinuxSys::popStruct(ThreadContext *context) const{
  VAddr sp=getStackPointer(context);
  I(stackGrowsDown());
  setStackPointer(context,sp+sizeof(T));
  return T(context,sp);
}
typedef std::multimap<VAddr,ThreadContext *> ContextMultiMap;
ContextMultiMap futexContexts;

template<typename val_t>
bool LinuxSys::futexCheck(ThreadContext *context, VAddr futex, val_t val){
  bool rv=(context->readMem<val_t>(futex)==val);
  if(!rv)
    context->getSystem()->setSysErr(context,ErrAgain);
  return rv;
}

void LinuxSys::futexWait(ThreadContext *context, VAddr futex){
  futexContexts.insert(ContextMultiMap::value_type(futex,context));                
#if (defined DEBUG_SIGNALS)     
  suspSet.insert(context->gettid());
#endif
  context->suspend();
}

int LinuxSys::futexWake(ThreadContext *context, VAddr futex, int nr_wake){
  int rv_wake=0;
  while((rv_wake<nr_wake)&&(futexContexts.lower_bound(futex)!=futexContexts.upper_bound(futex))){
    ThreadContext *wcontext=futexContexts.lower_bound(futex)->second;
    futexContexts.erase(futexContexts.lower_bound(futex));
    wcontext->resume();
    wcontext->getSystem()->setSysRet(wcontext,0);
    rv_wake++;
  }
  return rv_wake;
}
int LinuxSys::futexMove(ThreadContext *context, VAddr srcFutex, VAddr dstFutex, int nr_move){
  int rv_move=0;
  while((rv_move<nr_move)&&(futexContexts.lower_bound(srcFutex)!=futexContexts.upper_bound(srcFutex))){
    ThreadContext *wcontext=futexContexts.lower_bound(srcFutex)->second;
    futexContexts.erase(futexContexts.lower_bound(srcFutex));
    futexContexts.insert(ContextMultiMap::value_type(dstFutex,wcontext));
    rv_move++;
  }
  return rv_move;
}

bool LinuxSys::handleSignals(ThreadContext *context) const{
  while(context->hasReadySignal()){
    SigInfo *sigInfo=context->nextReadySignal();
    SignalAction sa=handleSignal(context,sigInfo);
    delete sigInfo;
    if(sa!=SigActIgnore)
      return true;
  }
  return false;
}

SignalAction Mips32LinuxSys::handleSignal(ThreadContext *context, SigInfo *sigInfo) const{
  // Pop signal mask if it's been saved
  uint32_t dopop=Mips::getRegAny<Mips32,uint32_t,Mips::RegSys>(context,Mips::RegSys);
  if(dopop){
    Mips::setRegAny<Mips32,uint32_t,Mips::RegSys>(context,Mips::RegSys,0);    
    context->setSignalMask(popStruct<Mips32Defs::Tk_sigset_t>(context));
  }
  SignalDesc &sigDesc=(*(context->getSignalTable()))[sigInfo->signo];
#if (defined DEBUG_SIGNALS)
  printf("Mips::handleSignal for pid=%d, signal=%d, action=0x%08x\n",context->gettid(),sigInfo->signo,sigDesc.handler);
#endif
  SignalAction sact((SignalAction)(sigDesc.handler));
  if(sact==SigActDefault)
    sact=getDflSigAction(sigInfo->signo);
  switch(sact){
  case SigActDefault:
    fail("Mips32LinuxSys::handleSignal signal %d with SigActDefault\n",sigInfo->signo);
  case SigActIgnore: 
    return SigActIgnore;
  case SigActCore:
  case SigActTerm:
    osSim->eventExit(context->gettid(),-1);
    return SigActTerm;
  case SigActStop:
#if (defined DEBUG_SIGNALS)
    suspSet.insert(context->gettid());
#endif
    context->suspend();
    return SigActStop;
  default:
    break;
  }
  // Save PC, then registers
  I(context->getIDesc()==context->virt2inst(context->getIAddr()));
  Mips32Defs::Tintptr_t pc=context->getIAddr();
  pushScalar(context,pc);
  for(RegName r=Mips::RegZero+1;r<=Mips::RegRA;r++){
    uint32_t wrVal=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,r);
    pushScalar(context,wrVal);
  }
  // Save the current signal mask and use sigaction's signal mask
  Mips32Defs::Tk_sigset_t oldMask(context->getSignalMask());
  pushStruct(context,oldMask);
  context->setSignalMask(sigDesc.mask);
  // Set registers and PC for execution of the handler
  Mips::setRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0,Mips32Defs::SigNum(sigInfo->signo).val);
  if(sigDesc.flags&SaSigInfo){
    printf("Mips::handleSignal with SA_SIGINFO not supported yet\n");
  }
  Mips::setRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegT9,sigDesc.handler);
  context->setIAddr(sysCodeAddr);
  return SigActHandle;
}

void Mips32LinuxSys::initSystem(ThreadContext *context) const{
  AddressSpace *addrSpace=context->getAddressSpace();
  sysCodeSize=6*sizeof(uint32_t);
  sysCodeAddr=addrSpace->newSegmentAddr(sysCodeSize);
  addrSpace->newSegment(sysCodeAddr,sysCodeSize,false,true,false,false,false);
  addrSpace->addFuncName("sysCode",sysCodeAddr);
  addrSpace->addFuncName("End of sysCode",sysCodeAddr+sysCodeSize);
  // jalr t9
  context->writeMem<uint32_t>(sysCodeAddr+0*sizeof(uint32_t),0x0320f809);
  // nop
  context->writeMem<uint32_t>(sysCodeAddr+1*sizeof(uint32_t),0x00000000);
  // li v0,4193 (syscall number for rt_sigreturn)
  context->writeMem<uint32_t>(sysCodeAddr+2*sizeof(uint32_t),0x24020000+4193);
  // Syscall     
  context->writeMem<uint32_t>(sysCodeAddr+3*sizeof(uint32_t),0x0000000C);
  // Unconditional branch to itself
  context->writeMem<uint32_t>(sysCodeAddr+4*sizeof(uint32_t),0x1000ffff);
  // Delay slot nop
  context->writeMem<uint32_t>(sysCodeAddr+5*sizeof(uint32_t),0x00000000);
  addrSpace->protectSegment(sysCodeAddr,sysCodeSize,true,false,true);
  // Set RegSys to zero. It is used by system call functions to indicate
  // that a signal mask has been already saved to the stack and needs to be restored
  Mips::setRegAny<Mips32,uint32_t,Mips::RegSys>(context,Mips::RegSys,0);
}

void LinuxSys::createStack(ThreadContext *context) const{
  AddressSpace *addrSpace=context->getAddressSpace();
  I(addrSpace);
  // Stack size starts at 16MB, but can autogrow
  size_t stackSize=0x1000000;
  VAddr  stackStart=addrSpace->newSegmentAddr(stackSize);
  I(stackStart);
  // Stack is created with read/write permissions, and autogrows down
  addrSpace->newSegment(stackStart,stackSize,true,true,false);
  addrSpace->setGrowth(stackStart,true,true);
  context->writeMemWithByte(stackStart,stackSize,0);
  context->setStack(stackStart,stackStart+stackSize);
  setStackPointer(context,stackStart+(stackGrowsDown()?stackSize:0));
}

template<class defs>
void LinuxSys::setProgArgs_(ThreadContext *context, int argc, char **argv, int envc, char **envp) const{
  I(stackGrowsDown());
  typedef typename defs::Tintptr_t    Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  Tintptr_t regSP=getStackPointer(context);
  // We will push arg and env string pointer arrays later, with nulls at end
  Tintptr_t envVAddrs[envc+1];
  I(sizeof(envVAddrs)==(envc+1)*sizeof(Tintptr_t));
  Tintptr_t argVAddrs[argc+1];
  I(sizeof(argVAddrs)==(argc+1)*sizeof(Tintptr_t));
  // Put the env strings on the stack and initialize the envVAddrs array
  for(int envIdx=envc-1;envIdx>=0;envIdx--){
    Tsize_t strSize=strlen(envp[envIdx])+1;
    regSP-=alignUp(strSize,Tsize_t(defs::StrArgAlign));
    context->writeMemFromBuf(regSP,strSize,envp[envIdx]);
    envVAddrs[envIdx]=regSP;
  }
  envVAddrs[envc]=0;
  // Put the arg string on the stack
  for(int argIdx=argc-1;argIdx>=0;argIdx--){
    Tsize_t strSize=strlen(argv[argIdx])+1;
    regSP-=alignUp(strSize,Tsize_t(defs::StrArgAlign));
    context->writeMemFromBuf(regSP,strSize,argv[argIdx]);
    argVAddrs[argIdx]=regSP;
  }
  argVAddrs[argc]=0;
  // Put the aux vector on the stack
  typedef typename defs::Tauxv_t Tauxv_t;
  regSP-=(regSP%sizeof(Tauxv_t)); // Align
  setStackPointer(context,regSP);
  // Push aux vector elements
  pushStruct(context,Tauxv_t(AT_NULL,0));
  pushStruct(context,Tauxv_t(AT_PHNUM,context->getAddressSpace()->getFuncAddr("PrgHdrNum")));
  pushStruct(context,Tauxv_t(AT_PHENT,context->getAddressSpace()->getFuncAddr("PrgHdrEnt")));
  pushStruct(context,Tauxv_t(AT_PHDR,context->getAddressSpace()->getFuncAddr("PrgHdrAddr")));
  pushStruct(context,Tauxv_t(AT_PAGESZ,AddrSpacPageSize));
  pushStruct(context,Tauxv_t(AT_ENTRY,context->getAddressSpace()->getFuncAddr("UserEntry")));
  // Put the envp array (with NULL at the end) on the stack
  for(int envi=envc;envi>=0;envi--)
    pushScalar(context,envVAddrs[envi]);
  // Put the argv array (with NULL at the end) on the stack
  for(int argi=argc;argi>=0;argi--)
    pushScalar(context,argVAddrs[argi]);
  // Put the argc on the stack
  pushScalar(context,Tsize_t(argc));
}

#include "OSSim.h"

template<class defs>
void LinuxSys::sysFutex(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t    Tintptr_t;
  typedef typename defs::Tint    Tint;
  typedef typename defs::Tuint   Tuint;
  typedef typename defs::FutexOp FutexOp;
  typedef typename defs::FutexCmd FutexCmd;
  FuncArgs args(context);
  Tintptr_t futex(args.get<Tintptr_t>());
  FutexOp op(args.get<Tuint>());
  // Ignore FUTEX_PRIVATE_FLAG
  FutexCmd cmd(op.getFutexCmd());
  if(cmd.isFUTEX_WAIT()){
    Tint val(args.get<Tint>());
    Tintptr_t timeout(args.get<Tintptr_t>());
    if(timeout)
      fail("LinuxSys::sysFutex non-zero timeout unsupported for FUTEX_WAIT");
    if(futexCheck(context,futex,val))
      futexWait(context,futex);
  }else if(cmd.isFUTEX_WAKE()){
    Tint nr_wake(args.get<Tint>());
    Tintptr_t timeout(args.get<Tintptr_t>());
    if(timeout)
      fail("LinuxSys::sysFutex non-zero timeout unsupported for FUTEX_WAKE");
    setSysRet(context,futexWake(context,futex,nr_wake));
  }else if(cmd.isFUTEX_REQUEUE()){
    Tint nr_wake(args.get<Tint>());
    Tint nr_move(args.get<Tint>());
    Tintptr_t futex2 (args.get<Tintptr_t>());
    setSysRet(context,futexWake(context,futex,nr_wake)+futexMove(context,futex,futex2,nr_move));
  }else if(cmd.isFUTEX_CMP_REQUEUE()){
    Tint nr_wake(args.get<Tint>());
    Tint nr_move(args.get<Tint>());
    Tintptr_t futex2 (args.get<Tintptr_t>());
    Tint val(args.get<Tint>());
    if(futexCheck<uint32_t>(context,futex,val))
      setSysRet(context,futexWake(context,futex,nr_wake)+futexMove(context,futex,futex2,nr_move));
  }else if(cmd.isFUTEX_WAKE_OP()){
    Tint nr_wake (args.get<Tint>());
    Tint nr_wake2(args.get<Tint>());
    Tintptr_t futex2  (args.get<Tintptr_t>());
    typedef typename defs::FutexWakeOp WakeOp;
    WakeOp wakeop(args.get<Tuint>());
    if(!context->canRead(futex2,sizeof(Tint))||!context->canWrite(futex2,sizeof(Tint)))
      return setSysErr(context,ErrFault);
    typedef typename defs::FutexAtmOp AtmOp;
    typedef typename defs::FutexCmpOp CmpOp;
    AtmOp atmop=wakeop.getAtmOp();
    CmpOp cmpop=wakeop.getCmpOp();
    Tint atmarg=wakeop.getAtmArg();
    Tint cmparg=wakeop.getCmpArg();
    if(wakeop.hasFUTEX_OP_OPARG_SHIFT())
      atmarg=1<<atmarg;
    Tint val=context->readMem<Tint>(futex2);
    bool cond;
    if(cmpop.isFUTEX_OP_CMP_EQ()){cond=(val==cmparg);}
    else if(cmpop.isFUTEX_OP_CMP_NE()){ cond=(val!=cmparg);}
    else if(cmpop.isFUTEX_OP_CMP_LT()){ cond=(val< cmparg);}
    else if(cmpop.isFUTEX_OP_CMP_LE()){ cond=(val<=cmparg);}
    else if(cmpop.isFUTEX_OP_CMP_GT()){ cond=(val> cmparg);}
    else if(cmpop.isFUTEX_OP_CMP_GE()){ cond=(val>=cmparg);}
    else
      return setSysErr(context,ErrNoSys);
    if(atmop.isFUTEX_OP_SET()){ val = atmarg; }
    else if(atmop.isFUTEX_OP_ADD()){ val+= atmarg; }
    else if(atmop.isFUTEX_OP_OR()){ val|= atmarg; }
    else if(atmop.isFUTEX_OP_ANDN()){ val&=~atmarg; }
    else if(atmop.isFUTEX_OP_XOR()){ val^= atmarg; }
    else
      return setSysErr(context,ErrNoSys);
    context->writeMem(futex2,val);
    setSysRet(context,futexWake(context,futex,nr_wake)+(cond?futexWake(context,futex2,nr_wake2):0));
  }else{
    fail("LinuxSys::sysFutex with unsupported op=%d\n",op.val);
  }
}

template<class defs>
void LinuxSys::sysSetRobustList(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t    Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Trobust_list_head Trobust_list_head;
  FuncArgs args(context);
  Tintptr_t    head=args.get<Tintptr_t>();
  Tsize_t len =args.get<Tsize_t>();
  if(len!=sizeof(Trobust_list_head))
    return setSysErr(context,ErrInval);
  context->setRobustList(head);
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::exitRobustList_(ThreadContext *context, VAddr robust_list){
  typedef typename defs::Trobust_list_head Trobust_list_head;
  // Any errors that occur here are silently ignored
  if(!context->canRead(robust_list,sizeof(Trobust_list_head)))
    return;
  Trobust_list_head head(context,robust_list);
  if(head.list.next!=robust_list)
    printf("LinuxSys::exitRobustList called with non-empty robust_list\n");
}
template<class defs>
void LinuxSys::sysSetTidAddress(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tpid_t Tpid_t;
  FuncArgs args(context);
  Tintptr_t tidptr=args.get<Tintptr_t>();
  context->setTidAddress(tidptr);
  return setSysRet(context,Tpid_t(context->gettid()+BaseSimTid));
}

template<class defs>
void LinuxSys::doWait4(ThreadContext *context, InstDesc *inst,
		       typename defs::Tpid_t cpid, typename defs::Tintptr_t status,
		       typename defs::WaitOpts options, typename defs::Tintptr_t rusage){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tint Tint;
  int realCTid=0;
  if(cpid>0){
    realCTid=cpid-BaseSimTid;
    if(!context->isChildID(realCTid))
      return setSysErr(context,ErrChild);
    ThreadContext *ccontext=osSim->getContext(realCTid);
    if((!ccontext->isExited())&&(!ccontext->isKilled()))
      realCTid=0;
  }else if(cpid==-1){
    if(!context->hasChildren())
      return setSysErr(context,ErrChild);
    realCTid=context->findZombieChild();
  }else{
    fail("LinuxSys::doWait4 Only supported for pid -1 or >0\n");
  }
  if(realCTid){
    ThreadContext *ccontext=osSim->getContext(realCTid);
    if(status){
      Tint statVal=0xDEADBEEF;
      if(ccontext->isExited()){
	statVal=(ccontext->getExitCode()<<8);
      }else{
	I(ccontext->isKilled());
	fail("LinuxSys::doWait4 for killed child not supported\n");
      }
      context->writeMem(status,statVal);
    }
    if(rusage)
      fail("LinuxSys::doWait4 with rusage parameter not supported\n");
#if (defined DEBUG_SIGNALS)
    suspSet.erase(pid);
#endif
    ccontext->reap();
    return setSysRet(context,Tpid_t(realCTid+BaseSimTid));
  }
  if(options.hasWNOHANG())
    return setSysRet(context,0);
  context->updIAddr(-inst->aupdate,-1);
  I(inst==context->getIDesc());
  I(inst==context->virt2inst(context->getIAddr()));
#if (defined DEBUG_SIGNALS)
  printf("Suspend %d in sysCall32_wait4(pid=%d,status=%x,options=%x)\n",context->gettid(),pid,status,options.val);
  printf("Also suspended:");
  for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
    printf(" %d",*suspIt);
  printf("\n");
  suspSet.insert(context->gettid());
#endif
  context->suspend();
}

template<class defs>
void LinuxSys::sysWait4(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::WaitOpts WaitOpts;
  typedef typename defs::Tuint Tuint;
  FuncArgs args(context);
  Tpid_t pid=args.get<Tpid_t>();
  Tintptr_t status=args.get<Tintptr_t>();
  WaitOpts options=args.get<Tuint>();
  Tintptr_t rusage= args.get<Tintptr_t>();
  typedef typename defs::Trusage Trusage;
  doWait4<defs>(context,inst,pid,status,options,rusage);
}

template<class defs>
void LinuxSys::sysWaitpid(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::WaitOpts WaitOpts;
  typedef typename defs::Tuint Tuint;
  FuncArgs args(context);
  Tpid_t pid=args.get<Tpid_t>();
  Tintptr_t status=args.get<Tintptr_t>();
  WaitOpts options=args.get<Tuint>();
  typedef typename defs::Trusage Trusage;
  doWait4<defs>(context,inst,pid,status,options,0);
}
void Mips32LinuxSys::sysRtSigReturn(ThreadContext *context, InstDesc *inst){
#if (defined DEBUG_SIGNALS)
  printf("sysCall32_rt_sigreturn pid %d to ",context->gettid());
#endif    
  // Restore old signal mask
  Mips32Defs::Tk_sigset_t oldMask=popStruct<Mips32Defs::Tk_sigset_t>(context);
  // Restore registers, then PC
  for(RegName r=Mips::RegRA;r>Mips::RegZero;r--){
    uint32_t rdVal=popScalar<uint32_t>(context);
    Mips::setRegAny<Mips32,uint32_t,RegTypeGpr>(context,r,rdVal);
  }
  context->setIAddr(popScalar<Mips32Defs::Tintptr_t>(context));
  context->setSignalMask(oldMask);
}
template<class defs>
void LinuxSys::sysRtSigAction(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::SigNum SigNum;
  typedef typename defs::Tk_sigaction Tk_sigaction;
  FuncArgs args(context);
  SigNum sig(args.get<Tuint>());
  Tintptr_t act =args.get<Tintptr_t>();
  Tintptr_t oact =args.get<Tintptr_t>();
  Tsize_t size=args.get<Tsize_t>();
  if(size!=sizeof(typename defs::Tk_sigset_t))
    return setSysErr(context,ErrInval);
  if((act)&&(!context->canRead(act,sizeof(Tk_sigaction))))
    return setSysErr(context,ErrFault);
  if((oact)&&!context->canWrite(oact,sizeof(Tk_sigaction)))
    return setSysErr(context,ErrFault);
  SignalID localSig(sig);
  SignalTable *sigTable=context->getSignalTable();
  SignalDesc  &sigDesc=(*sigTable)[localSig];
  // Get the existing signal handler into oactBuf
  if(oact)
    Tk_sigaction(sigDesc).write(context,oact);
  if(act){
    sigDesc=Tk_sigaction(context,act);
    // Without SA_NODEFER, mask signal out in its own handler
    if(!(sigDesc.flags&SaNoDefer))
      sigDesc.mask.set(localSig);
  }
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysRtSigProcMask(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::MaskHow MaskHow;
  typedef typename defs::Tk_sigset_t Tk_sigset_t;
  FuncArgs args(context);
  MaskHow how=args.get<Tuint>();
  Tintptr_t nset=args.get<Tintptr_t>();
  Tintptr_t oset=args.get<Tintptr_t>();
  Tsize_t size=args.get<Tsize_t>();
  if((nset)&&(!context->canRead(nset,size)))      
    return setSysErr(context,ErrFault);
  if((oset)&&(!context->canWrite(oset,size)))
    return setSysErr(context,ErrFault);
  SignalSet oldMask=context->getSignalMask();
  if(oset)
    Tk_sigset_t(oldMask).write(context,oset);
  if(nset){
    SignalSet lset(Tk_sigset_t(context,nset));
    if(how.isSIG_BLOCK()){
      context->setSignalMask(oldMask|lset);
    }else if(how.isSIG_UNBLOCK()){
      context->setSignalMask((oldMask|lset)^lset);
    }else if(how.isSIG_SETMASK()){
      context->setSignalMask(lset);
    }else
      fail("sysRtSigProcMask: Unsupported value %d of how\n",how.val);
  }
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysRtSigSuspend(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tk_sigset_t Tk_sigset_t;
  typedef typename defs::Tsize_t Tsize_t;
  // If this is a suspend following a wakeup, we need to pop the already-saved mask
  uint32_t dopop=Mips::getRegAny<Mips32,uint32_t,Mips::RegSys>(context,Mips::RegSys);
  if(dopop){
    Mips::setRegAny<Mips32,uint32_t,Mips::RegSys>(context,Mips::RegSys,0);    
    context->setSignalMask(popStruct<Tk_sigset_t>(context));
  }
  FuncArgs args(context);
  Tintptr_t nset=args.get<Tintptr_t>();
  Tsize_t size=args.get<Tsize_t>();
  if(!context->canRead(nset,size))
    return setSysErr(context,ErrFault);
  // Change signal mask while suspended
  SignalSet oldMask=context->getSignalMask();
  SignalSet newMask(Tk_sigset_t(context,nset));
  context->setSignalMask(newMask);
  if(context->hasReadySignal()){
    SigInfo *sigInfo=context->nextReadySignal();
    context->setSignalMask(oldMask);
    handleSignal(context,sigInfo);
    delete sigInfo;
    return setSysErr(context,ErrIntr);
  }
#if (defined DEBUG_SIGNALS)
  typedef typename defs::Tpid_t Tpid_t;
  Tpid_t pid=context->gettid();
  printf("Suspend %d in sysCall32_rt_sigsuspend\n",pid);
  context->dumpCallStack();
  printf("Also suspended:");
  for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
    printf(" %d",*suspIt);
  printf("\n");
  suspSet.insert(context->gettid());
#endif
  // Save the old signal mask on stack so it can be restored
  Tk_sigset_t saveMask(oldMask);
  pushStruct(context,saveMask);
  Mips::setRegAny<Mips32,uint32_t,Mips::RegSys>(context,Mips::RegSys,1);
  // Suspend and redo this system call when woken up
  context->updIAddr(-inst->aupdate,-1);
  I(inst==context->getIDesc());
  I(inst==context->virt2inst(context->getIAddr()));
  context->suspend();
}

template<class defs>
void LinuxSys::sysKill(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::SigNum SigNum;
  FuncArgs args(context);
  Tpid_t pid(args.get<Tpid_t>());
  SigNum sig(args.get<Tuint>());
  if(pid<=0)
    fail("sysKill with pid=%d\n",pid);
  ThreadContext *kcontext=osSim->getContext(pid-BaseSimTid);
  SigInfo *sigInfo=new SigInfo(sig,SigCodeUser);
  sigInfo->pid=context->gettid();
  kcontext->signal(sigInfo);
#if (defined DEBUG_SIGNALS)
  printf("sysCall32_kill: signal %d sent from process %d to %d\n",sig.val,context->gettid(),pid);
#endif
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysTKill(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::SigNum SigNum;
  FuncArgs args(context);
  Tpid_t tid(args.get<Tpid_t>());
  SigNum sig(args.get<Tuint>());
  if(tid<=0)
    fail("sysTKill with tid=%d\n",tid);
  ThreadContext *kcontext=osSim->getContext(tid-BaseSimTid);
  if(!kcontext)
    return setSysErr(context,ErrSrch);
  SigInfo *sigInfo=new SigInfo(sig,SigCodeUser);
  sigInfo->pid=context->gettid();
  kcontext->signal(sigInfo);
#if (defined DEBUG_SIGNALS)
  printf("sysCall32_tkill: signal %d sent from process %d to thread %d\n",sig.val,context->gettid(),tid);
#endif
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysTgKill(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::SigNum SigNum;
  FuncArgs args(context);
  Tpid_t tgid(args.get<Tpid_t>());
  Tpid_t pid(args.get<Tpid_t>());
  SigNum sig(args.get<Tuint>());
  if(pid<=0)
    fail("sysTgKill with pid=%d\n",pid);
  ThreadContext *kcontext=osSim->getContext(pid-BaseSimTid);
  if(!kcontext)
    return setSysErr(context,ErrSrch);
  if(kcontext->gettgid()!=tgid)
    return setSysErr(context,ErrSrch);
  SigInfo *sigInfo=new SigInfo(sig,SigCodeUser);
  sigInfo->pid=context->gettid();
  kcontext->signal(sigInfo);
#if (defined DEBUG_SIGNALS)
  printf("sysCall32_tgkill: signal %d sent from process %d to thread %d in %d\n",sig.val,context->gettid(),pid,tgid);
#endif
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysGetPriority(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint which(args.get<Tint>());
  Tint who(args.get<Tint>());
  printf("sysGetPriority(%d,%d) called (continuing).\n",which,who);
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysSchedGetParam(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tpid_t    pid(args.get<Tpid_t>());
  Tintptr_t param(args.get<Tintptr_t>());;
  printf("sysSchedGetParam(%d,%d) called (continuing with EINVAL).\n",pid,param);
  ThreadContext *kcontext=pid?osSim->getContext(pid-BaseSimTid):context;
  if(!kcontext)
    return setSysErr(context,ErrSrch);
  // TODO: Check if we can write to param
  // TODO: Return a meaningful value. for now, we just reject the call
  setSysErr(context,ErrInval);
}

template<class defs>
void LinuxSys::sysSchedSetScheduler(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tint Tint;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs  args(context);
  Tpid_t    pid(args.get<Tpid_t>());
  Tint      policy(args.get<Tint>());
  Tintptr_t param(args.get<Tintptr_t>());;
  printf("sysSchedSetScheduler(%d,%d,%d) called (continuing).\n",pid,policy,param);
  // TODO: Set the actual scheduling policy
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysSchedGetScheduler(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  FuncArgs args(context);
  Tpid_t    pid(args.get<Tpid_t>());
  printf("sysSchedGetScheduler(%d) called (continuing).\n",pid);
   // TODO: Get the actual scheduling policy, we just return zero now
  setSysRet(context,0); 
}
template<class defs>
void LinuxSys::sysSchedYield(ThreadContext *context, InstDesc *inst){
  osSim->eventYield(context->gettid(),-1);
  return setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysSchedGetPriorityMax(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint policy(args.get<Tint>());
  printf("sysSchedGetPriorityMax(%d) called (continuing with 0).\n",policy);
  return setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysSchedGetPriorityMin(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint policy(args.get<Tint>());
  printf("sysSchedGetPriorityMin(%d) called (continuing with 0).\n",policy);
  return setSysRet(context,0); 
}

template<class defs>
void LinuxSys::sysSchedSetAffinity(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tpid_t    pid(args.get<Tpid_t>());
  Tuint     len(args.get<Tuint>());
  Tintptr_t mask(args.get<Tuint>());;
  if(!context->canRead(mask,len))
    return setSysRet(context,ErrFault);
  ThreadContext *kcontext=pid?osSim->getContext(pid-BaseSimTid):context;
  if(!kcontext)
    return setSysErr(context,ErrSrch);
  // TODO: We need to look at the affinity mask here
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysSchedGetAffinity(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tpid_t Tpid_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tpid_t    pid(args.get<Tpid_t>());
  Tuint     len(args.get<Tuint>());
  Tintptr_t mask(args.get<Tuint>());
  // TODO: Use the real number of CPUs instead of 1024
  if(len<1024/sizeof(uint8_t))
    return setSysRet(context,ErrInval);
  if(!context->canWrite(mask,len))
    return setSysRet(context,ErrFault);
  ThreadContext *kcontext=pid?osSim->getContext(pid-BaseSimTid):context;
  if(!kcontext)
    return setSysErr(context,ErrSrch);
  // TODO: We need to look at the affinity mask here
  // for now, we just return an all-ones mask
  uint8_t buf[len];
  memset(buf,0,len);
  context->writeMemFromBuf(mask,len,buf);
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysAlarm(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tuint Tuint;
  FuncArgs args(context);
  Tuint seconds(args.get<Tuint>());
  // TODO: Clear existing alarms
  Tuint oseconds=0;
  if(seconds){
    // Set new alarm
    fail("sysCall32_alarm(%d): not implemented at 0x%08x\n",seconds,context->getIAddr());
  }
  setSysRet(context,oseconds);
}

// Simulated wall clock time (for time() syscall)
static time_t wallClock=(time_t)-1;
// TODO: This is a hack to get applications working.
// We need a real user/system time estimator
static uint64_t myUsrUsecs=0;
static uint64_t mySysUsecs=0;

template<class defs>
void LinuxSys::sysClockGetRes(ThreadContext *context, InstDesc *inst){
  // TODO: Read the actual parameters
  // for now, we just reject the call
  printf("sysClockGetRes called (continuing with EINVAL).\n");
  setSysErr(context,ErrInval);
}
template<class defs>
void LinuxSys::sysClockGetTime(ThreadContext *context, InstDesc *inst){
  // TODO: Read the actual parameters
  // for now, we just reject the call
  printf("sysClockGettime called (continuing with EINVAL).\n");
  setSysErr(context,ErrInval);
}
template<class defs>
void LinuxSys::sysSetITimer(ThreadContext *context, InstDesc *inst){
   // TODO: Read the actual parameters
  // for now, we just reject the call
  printf("sysSetITimer called (continuing with EINVAL).\n");
  setSysErr(context,ErrInval);
}

template<class defs>
void LinuxSys::sysNanoSleep(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Ttimespec Ttimespec;
  FuncArgs args(context);
  Tintptr_t req=args.get<Tintptr_t>();
  Tintptr_t rem=args.get<Tintptr_t>();
  if(!context->canRead(req,sizeof(Ttimespec)))
    return setSysErr(context,ErrFault);
  if(rem&&(!context->canWrite(rem,sizeof(Ttimespec))))
    return setSysErr(context,ErrFault);
  Ttimespec ts(context,req);
  // TODO: We need to actually suspend this thread for the specified time
  wallClock+=ts.tv_sec;
  if(rem){
    ts.tv_sec=0;
    ts.tv_nsec=0;
    ts.write(context,rem);
  }
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysClockNanoSleep(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Ttimespec Ttimespec;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  // TODO: the type for this should be clockid_t
  Tuint clock_id=args.get<Tuint>();
  Tint  flags=args.get<Tint>();
  Tintptr_t req=args.get<Tintptr_t>();
  Tintptr_t rem=args.get<Tintptr_t>();
  if(!context->canRead(req,sizeof(Ttimespec)))
    return setSysErr(context,ErrFault);
  if(rem&&(!context->canWrite(rem,sizeof(Ttimespec))))
    return setSysErr(context,ErrFault);
  Ttimespec ts(context,req);
  // TODO: We need to actually suspend this thread for the specified time
  wallClock+=ts.tv_sec;
  if(rem){
    ts.tv_sec=0;
    ts.tv_nsec=0;
    ts.write(context,rem);
  }
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysTime(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Ttime_t Ttime_t;
  FuncArgs args(context);
  Tintptr_t tloc=args.get<Tintptr_t>();
  // TODO: This should actually take into account simulated time
  if(wallClock==(time_t)-1)
    wallClock=time(0);
  I(wallClock>0);
  Ttime_t rv=wallClock;
  if(tloc!=(VAddr)0){
    if(!context->canWrite(tloc,sizeof(Ttime_t)))
      return setSysErr(context,ErrFault);
    context->writeMem(tloc,rv);
  }
  return setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysTimes(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tclock_t Tclock_t;
  typedef typename defs::Ttms Ttms;
  FuncArgs args(context);
  Tintptr_t buffer=args.get<Tintptr_t>();
  if(!context->canWrite(buffer,sizeof(Ttms)))
    return setSysErr(context,ErrFault);
  Ttms simTms;
  // TODO: This is a hack. See above.
  myUsrUsecs+=100;
  mySysUsecs+=1;
  simTms.tms_utime=simTms.tms_cutime=myUsrUsecs/1000;
  simTms.tms_stime=simTms.tms_cstime=mySysUsecs/1000;
  Tclock_t rv=(myUsrUsecs+mySysUsecs)/1000;
  simTms.write(context,buffer);
  return setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysGetRUsage(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::RUsageWho RUsageWho;
  typedef typename defs::Ttimeval Ttimeval;
  typedef typename defs::Trusage Trusage;
  FuncArgs args(context);
  RUsageWho who(args.get<typeof(who.val)>());
  Tintptr_t r_usage(args.get<Tintptr_t>());
  if(!context->canWrite(r_usage,sizeof(Trusage)))
    return setSysErr(context,ErrFault);

  // TODO: This is a hack. See definition of these vars
  myUsrUsecs+=100;
  mySysUsecs+=1;
  Trusage(Ttimeval(myUsrUsecs/1000000,myUsrUsecs%1000000),
	  Ttimeval(mySysUsecs/1000000,mySysUsecs%1000000)).write(context,r_usage);

  return setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysGetRLimit(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::T__rlimit_resource T__rlimit_resource;
  typedef typename defs::Trlimit Trlimit;
  FuncArgs args(context);
  T__rlimit_resource resource(args.get<typeof(resource.val)>());
  Tintptr_t rlim(args.get<Tintptr_t>());
  if(!context->canWrite(rlim,sizeof(T__rlimit_resource)))
    return setSysErr(context,ErrFault); 
  Trlimit buf;
  if(resource.isRLIMIT_STACK()){
  }else if(resource.isRLIMIT_NOFILE()){
  }else if(resource.isRLIMIT_DATA()){
  }else if(resource.isRLIMIT_AS()){
  }else if(resource.isRLIMIT_CPU()){
  }else if(resource.isRLIMIT_CORE()){
    buf.rlim_cur=buf.rlim_max=0;
  }else
    printf("sysCall32_getrlimit called for unknown resource %d. Return RLIM_INFINITY.\n",resource.val);
  buf.write(context,rlim);
  return setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysSetRLimit(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::T__rlimit_resource T__rlimit_resource;
  typedef typename defs::Trlimit Trlimit;
  FuncArgs args(context);
  T__rlimit_resource resource(args.get<typeof(resource.val)>());
  Tintptr_t rlim(args.get<Tintptr_t>());
  if(!context->canRead(rlim,sizeof(T__rlimit_resource)))
    return setSysErr(context,ErrFault); 
  Trlimit buf(context,rlim);
  if(resource.isRLIMIT_STACK()){
    // Limit is already RLIM_INFINITY, so we don't care what the new size is
  }else
    fail("sysCall32_setrlimit called for resource %d at 0x%08x\n",resource.val,context->getIAddr());
  return setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysGetTimeOfDay(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Ttimeval Ttimeval;
  typedef typename defs::Ttimezone Ttimezone;
  FuncArgs args(context);
  Tintptr_t tv=args.get<Tintptr_t>();
  Tintptr_t tz=args.get<Tintptr_t>();
  if(tv&&!context->canWrite(tv,sizeof(Ttimeval)))
    return setSysErr(context,ErrFault);
  if(tz&&!context->canWrite(tz,sizeof(Ttimezone)))
    return setSysErr(context,ErrFault);
  if(tv)
    Ttimeval(15,0).write(context,tv);
  if(tz)
    Ttimezone(0,0).write(context,tz);
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysGetPid(ThreadContext *context, InstDesc *inst){
  setSysRet(context,context->gettgid()+BaseSimTid);
}
template<class defs>
void LinuxSys::sysGetTid(ThreadContext *context, InstDesc *inst){
  setSysRet(context,context->gettid()+BaseSimTid);
}
template<class defs>
void LinuxSys::sysGetPPid(ThreadContext *context, InstDesc *inst){
  setSysRet(context,context->getppid()+BaseSimTid);
}
template<class defs>
void LinuxSys::sysFork(ThreadContext *context, InstDesc *inst){
  ThreadContext *newContext=new ThreadContext(*context,false,false,false,false,false,SigChld,0);
  I(newContext!=0);
  // Fork returns an error only if there is no memory, which should not happen here
  // Set return values for parent and child
  setSysRet(context,newContext->gettid()+BaseSimTid);
  setSysRet(newContext,0);
  // Inform SESC that this process is created
  osSim->eventSpawn(-1,newContext->gettid(),0);
}
template<class defs>
void LinuxSys::sysClone(ThreadContext *context, InstDesc *inst){
  typedef typename defs::CloneFlags CloneFlags;
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tpid_t Tpid_t;
  FuncArgs args(context);
  CloneFlags flags=args.get<typeof(flags.val)>();
  Tintptr_t child_stack=args.get<Tintptr_t>();
  Tintptr_t ptid=args.get<Tintptr_t>();
  Tintptr_t tarea=args.get<Tintptr_t>();
  Tintptr_t ctid=args.get<Tintptr_t>();
  if(flags.hasCLONE_VFORK())
    fail("sysCall32_clone with CLONE_VFORK not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags.val);
  if(flags.hasCLONE_NEWNS())
    fail("sysCall32_clone with CLONE_NEWNS not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags.val);
  if(flags.hasCLONE_UNTRACED())
    fail("sysCall32_clone with CLONE_UNTRACED not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags.val);
  if(flags.hasCLONE_STOPPED())
    fail("sysCall32_clone with CLONE_STOPPED not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags.val);
  SignalID sig=flags.hasCLONE_DETACHED()?SigDetached:SignalID(flags.getCSIGNAL());
  ThreadContext *newContext= new ThreadContext(*context,
					       flags.hasCLONE_PARENT(),flags.hasCLONE_FILES(),flags.hasCLONE_SIGHAND(),flags.hasCLONE_VM(),
					       flags.hasCLONE_THREAD(),sig,flags.hasCLONE_CHILD_CLEARTID()?ctid:0);
  I(newContext!=0);
  // Fork returns an error only if there is no memory, which should not happen here
  // Set return values for parent and child
  setSysRet(context,newContext->gettid()+BaseSimTid);
  setSysRet(newContext,0);
  if(child_stack){
    newContext->setStack(newContext->getAddressSpace()->getSegmentAddr(child_stack),child_stack);
    setStackPointer(newContext,child_stack);
#if (defined DEBUG_BENCH)
    newContext->clearCallStack();
#endif
  }else{
    I(!flags.hasCLONE_VM());
  }
  if(flags.hasCLONE_PARENT_SETTID())
    context->writeMem<Tpid_t>(ptid,newContext->gettid()+BaseSimTid);
  if(flags.hasCLONE_CHILD_SETTID())
    newContext->writeMem<Tpid_t>(ctid,newContext->gettid()+BaseSimTid);
  if(flags.hasCLONE_SETTLS())
    setThreadArea(newContext,tarea);
  // Inform SESC that this process is created
  osSim->eventSpawn(-1,newContext->gettid(),0);
}
template<class defs>
InstDesc *LinuxSys::sysExecVe(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr fname(context,args.get<Tintptr_t>());
  if(!fname)
    return inst;
  Tintptr_t argv=args.get<Tintptr_t>();
  Tintptr_t envp=args.get<Tintptr_t>();
  // Check file for errors
  size_t realNameLen=FileSys::FileNames::getFileNames()->getReal(fname,0,0);
  char realName[realNameLen];
  FileSys::FileNames::getFileNames()->getReal(fname,realNameLen,realName);
  FileSys::FileStatus::pointer fs(FileSys::FileStatus::open(realName,O_RDONLY,0));
  if(!fs){
    setSysErr(context,getErrCode(errno));
    return inst;
  }
  ExecMode emode=getExecMode(fs);
  if(emode!=ExecModeMips32){
    setSysErr(context,ErrNoExec);
    return inst;
  }
  // Pipeline flush to avoid mixing old and new InstDesc's in the pipeline
  if(context->getNDInsts()){
    context->updIAddr(-inst->aupdate,-1);
    return 0;
  }
  // Find the length of the argv list
  size_t argvNum=0;
  size_t argvLen=0;
  while(true){
    Tintptr_t argAddr=context->readMem<Tintptr_t>(argv+sizeof(Tintptr_t)*argvNum);
    if(!argAddr)
      break;
    ssize_t argLen=context->readMemString(argAddr,0,0);
    I(argLen>=1);
    argvLen+=argLen;
    argvNum++;
  }
  // Find the length of the envp list
  size_t envpNum=0;
  size_t envpLen=0;
  while(true){
    Tintptr_t envAddr=context->readMem<Tintptr_t>(envp+sizeof(Tintptr_t)*envpNum);
    if(!envAddr)
      break;
    ssize_t envLen=context->readMemString(envAddr,0,0);
    I(envLen>=1);
    envpLen+=envLen;
    envpNum++;
  }
  char *argvArr[argvNum];
  char argvBuf[argvLen];
  char *argvPtr=argvBuf;
  for(size_t arg=0;arg<argvNum;arg++){
    Tintptr_t argAddr=context->readMem<Tintptr_t>(argv+sizeof(Tintptr_t)*arg);
    ssize_t argLen=context->readMemString(argAddr,0,0);
    I(argLen>=1);
    context->readMemString(argAddr,argLen,argvPtr);
    argvArr[arg]=argvPtr;
    argvPtr+=argLen;
  }
  char *envpArr[envpNum];
  char envpBuf[envpLen];
  char *envpPtr=envpBuf;
  for(size_t env=0;env<envpNum;env++){
    Tintptr_t envAddr=context->readMem<Tintptr_t>(envp+sizeof(Tintptr_t)*env);
    ssize_t envLen=context->readMemString(envAddr,0,0);
    I(envLen>=1);
    context->readMemString(envAddr,envLen,envpPtr);
    envpArr[env]=envpPtr;
    envpPtr+=envLen;
  }
#if (defined DEBUG_BENCH)
  printf("execve fname is %s\n",realName);
  for(size_t arg=0;arg<argvNum;arg++){
    printf("execve argv[%ld] is %s\n",(long int)arg,argvArr[arg]);
  }
  for(size_t env=0;env<envpNum;env++){
    printf("execve envp[%ld] is %s\n",(long int)env,envpArr[env]);
  }
#endif
  // Close files that are still open and are cloexec
  context->getOpenFiles()->exec();
  // Clear up the address space and load the new object file
  context->getAddressSpace()->clear(true);
  //  loadElfObject(realName,context);
  // TODO: Use ELF_ET_DYN_BASE instead of a constant here
  loadElfObject(context,fs,0x200000,emode);
  I(context->getMode()==Mips32);
  initSystem(context);
  createStack(context);
  setProgArgs(context,argvNum,argvArr,envpNum,envpArr);
#if (defined DEBUG_BENCH)
  context->clearCallStack();
#endif
  // The InstDesc is gone now and we can't put it through the pipeline
  return 0;
}
template<class defs>
void LinuxSys::sysSetThreadArea(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tintptr_t addr=args.get<Tintptr_t>();
  setThreadArea(context,addr);
  setSysRet(context,0);
}

template<class defs>
InstDesc *LinuxSys::sysExit(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint status=args.get<Tint>();
  if(context->exit(status))
    return 0;
#if (defined DEBUG_SIGNALS)
  printf("Suspend %d in sysExit\n",context->gettid());
  context->dumpCallStack();
  printf("Also suspended:");
  for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
    printf(" %d",*suspIt);
  printf("\n");
  suspSet.insert(context->gettid());
#endif
  // Deliver the exit signal to the parent process if needed
  SignalID exitSig=context->getExitSig();
  // If no signal to be delivered, just wait to be reaped
  if(exitSig==SigDetached){
    context->reap();
    return 0;
  }
  if(exitSig!=SigNone){
    SigInfo *sigInfo=new SigInfo(exitSig,SigCodeChldExit);
    sigInfo->pid=context->gettid();
    sigInfo->data=status;
    ThreadContext::getContext(context->getParentID())->signal(sigInfo);
  }
  return inst;
}
template<class defs>
InstDesc *LinuxSys::sysExitGroup(ThreadContext *context, InstDesc *inst){
  if(context->gettgtids(0,0)>0)
    fail("LinuxSys::sysExitGroup can't handle live thread grup members\n");
  return sysExit<defs>(context,inst);
}

template<class defs>
void LinuxSys::sysBrk(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  FuncArgs args(context);
  Tintptr_t addr=args.get<Tintptr_t>();
  Tintptr_t brkBase=context->getAddressSpace()->getBrkBase();
  Tintptr_t segStart=context->getAddressSpace()->getSegmentAddr(brkBase-1);
  Tsize_t   oldSegSize=context->getAddressSpace()->getSegmentSize(segStart);
  if(!addr)
    return setSysRet(context,segStart+oldSegSize);
  if(addr<brkBase)
    fail("sysCall32_brk: new break 0x%08x below brkBase 0x%08x\n",addr,brkBase);
  Tsize_t   newSegSize=addr-segStart;
  if((newSegSize<=oldSegSize)||context->getAddressSpace()->isNoSegment(segStart+oldSegSize,newSegSize-oldSegSize)){
    context->getAddressSpace()->resizeSegment(segStart,newSegSize);
    return setSysRet(context,addr);
  }
  return setSysErr(context,ErrNoMem);
//   Tsize_t brkSize=context->getAddressSpace()->getSegmentSize(brkBase);
//   if(!addr)
//     return setSysRet(context,brkBase+brkSize);
//   if(addr<=brkBase+brkSize){
//     if(addr<=brkBase)
//       fail("sysCall32_brk: new break 0x%08x below brkBase 0x%08x\n",addr,brkBase);
//     context->getAddressSpace()->resizeSegment(brkBase,addr-brkBase);
//     return setSysRet(context,addr);
//   }
//   if(context->getAddressSpace()->isNoSegment(brkBase+brkSize,addr-(brkBase+brkSize))){
//     context->getAddressSpace()->resizeSegment(brkBase,addr-brkBase);
//     return setSysRet(context,addr);
//   }
//   return setSysErr(context,ErrNoMem);
}
template<class defs, off_t offsmul>
void LinuxSys::sysMMap(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tssize_t Tssize_t;
  typedef typename defs::Tint Tint;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Toff_t Toff_t;
  typedef typename defs::MemProt MemProt;
  typedef typename defs::MMapFlags MMapFlags;
  FuncArgs args(context);
  Tintptr_t start(args.get<Tintptr_t>());
  Tsize_t length(args.get<Tsize_t>());
  MemProt prot(args.get<Tuint>());
  MMapFlags flags(args.get<Tuint>());
  Tint fd(args.get<Tint>());
  Toff_t offset(args.get<Toff_t>());
  I(flags.hasMAP_SHARED()||flags.hasMAP_PRIVATE());
  I(!(flags.hasMAP_SHARED()&&flags.hasMAP_PRIVATE()));
  if(!flags.hasMAP_ANONYMOUS()&&!flags.hasMAP_PRIVATE()&&prot.hasPROT_WRITE())
    fail("sysCall32Mmap: TODO: Mapping of real files supported only without PROT_WRITE or with MAP_PRIVATE\n");
  Tintptr_t rv=0;
  if(flags.hasMAP_FIXED()&&start&&!context->getAddressSpace()->isNoSegment(start,length))
    context->getAddressSpace()->deleteSegment(start,length);
  if(start&&context->getAddressSpace()->isNoSegment(start,length)){
    rv=start;
  }else if(flags.hasMAP_FIXED()){
    return setSysErr(context,ErrInval);
  }else{
    rv=context->getAddressSpace()->newSegmentAddr(length);
    if(!rv)
      return setSysErr(context,ErrNoMem);
  }
  // Create a write-only segment and initialize memory in it
  context->getAddressSpace()->newSegment(rv,length,false,true,false,flags.hasMAP_SHARED());
  Tsize_t initPos=0;
  if(!flags.hasMAP_ANONYMOUS()){
    Tssize_t readRet=context->writeMemFromFile(rv,length,fd,false,true,offset*offsmul);
    if(readRet==-1)
      fail("MMap could not read from underlying file\n");
    I(readRet>=0);
    FileSys::BaseStatus *bs=context->getOpenFiles()->getDesc(fd)->getStatus();
    FileSys::FileStatus *fs=static_cast<FileSys::FileStatus *>(bs);
    ExecMode exmode=getExecMode(fs);
    if(exmode)
      mapFuncNames(context,fs,exmode,rv,readRet,offset*offsmul);
    initPos=readRet;
  }
  if(initPos!=length)
    context->writeMemWithByte(rv+initPos,length-initPos,0);
  context->getAddressSpace()->protectSegment(rv,length,prot.hasPROT_READ(),prot.hasPROT_WRITE(),prot.hasPROT_EXEC());
#if (defined DEBUG_MEMORY)
  printf("sysCall32_mmap addr 0x%08x len 0x%08lx R%d W%d X%d S%d\n",
	 rv,(unsigned long)length,
	 prot.hasPROT_READ(),
	 prot.hasPROT_WRITE(),
	 prot.hasPROT_EXEC(),
	 flags.hasMAP_SHARED());
#endif
#if (defined DEBUG_FILES) || (defined DEBUG_VMEM)
  printf("[%d] mmap %d start 0x%08x len 0x%08x offset 0x%08x prot %c%c%c load 0x%08x to 0x%08x\n",
	 context->gettid(),fd,(uint32_t)start,(uint32_t)length,(uint32_t)offset,
         prot.hasPROT_READ()?'R':' ',prot.hasPROT_WRITE()?'W':' ',prot.hasPROT_EXEC()?'E':' ',
	 (uint32_t)initPos,(uint32_t)rv);
#endif
  return setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysMReMap(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::MReMapFlags MReMapFlags;
  FuncArgs args(context);
  Tintptr_t oldaddr(args.get<Tintptr_t>());
  Tsize_t oldsize(args.get<Tsize_t>());
  Tsize_t newsize(args.get<Tsize_t>());
  MReMapFlags flags(args.get<Tuint>());
  AddressSpace *addrSpace=context->getAddressSpace();
  if(!addrSpace->isSegment(oldaddr,oldsize))
    fail("sysCall32_mremap: old range not a segment\n");
  if(newsize<=oldsize){
    if(newsize<oldsize)
      addrSpace->resizeSegment(oldaddr,newsize);
    return setSysRet(context,oldaddr);
  }
  Tintptr_t rv=0;
  if(addrSpace->isNoSegment(oldaddr+oldsize,newsize-oldsize)){
    rv=oldaddr;
  }else if(flags.hasMREMAP_MAYMOVE()){
    rv=addrSpace->newSegmentAddr(newsize);
    if(rv)
      addrSpace->moveSegment(oldaddr,rv);
  }
  if(!rv)
    return setSysErr(context,ErrNoMem);
  addrSpace->resizeSegment(rv,newsize);
  context->writeMemWithByte(rv+oldsize,newsize-oldsize,0);      
  return setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysMUnMap(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  FuncArgs args(context);
  Tintptr_t addr=args.get<Tintptr_t>();
  Tsize_t len=args.get<Tsize_t>();
  context->getAddressSpace()->deleteSegment(addr,len);
#if (defined DEBUG_MEMORY)
  printf("sysCall32_munmap addr 0x%08x len 0x%08lx\n",             
         addr,(unsigned long)len);

#endif
#if (defined DEBUG_VMEM)
  printf("[%d] munmap addr 0x%08x len 0x%08x\n",
         context->gettid(),(uint32_t)addr,(uint32_t)len);
#endif
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysMProtect(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::MemProt MemProt;
  FuncArgs args(context);
  Tintptr_t addr=args.get<Tintptr_t>();
  Tsize_t len=args.get<Tsize_t>();
  MemProt prot=args.get<Tuint>();
#if (defined DEBUG_MEMORY)
  printf("sysCall32_mprotect addr 0x%08x len 0x%08lx R%d W%d X%d\n",
         (unsigned long)addr,(unsigned long)len,
         prot.hasPROT_READ(),prot.hasPROT_WRITE(),prot.hasPROT_EXEC());
#endif
#if (defined DEBUG_VMEM)
  printf("[%d] mprotect addr 0x%08x len 0x%08x prot %c%c%c\n",
         context->gettid(),(uint32_t)addr,(uint32_t)len,
         prot.hasPROT_READ()?'R':' ',prot.hasPROT_WRITE()?'W':' ',
         prot.hasPROT_EXEC()?'E':' '
        );
#endif
  if(!context->getAddressSpace()->isMapped(addr,len))
    return setSysErr(context,ErrNoMem);
  context->getAddressSpace()->protectSegment(addr,len,
    prot.hasPROT_READ(),prot.hasPROT_WRITE(),prot.hasPROT_EXEC());
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysOpen(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::OpenFlags OpenFlags;
  typedef typename defs::Tmode_t Tmode_t;
  FuncArgs args(context);
  Tstr path(context,args.get<Tintptr_t>());
  if(!path)
    return;
  OpenFlags flags = args.get<Tuint>();
  Tmode_t mode = args.get<Tuint>();
  // Do the actual call
  int newfd=context->getOpenFiles()->open(path,flags.toNat(),mode);
#ifdef DEBUG_FILES
  printf("(%d) open %s as %d\n",context->gettid(),(const char *)path,newfd);
#endif
  if(newfd==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,newfd);
}
template<class defs>
void LinuxSys::sysPipe(ThreadContext *context, InstDesc *inst){
  int fds[2];
  if(context->getOpenFiles()->pipe(fds)==-1)
    return setSysErr(context,getErrCode(errno));
#ifdef DEBUG_FILES
  printf("[%d] pipe rd %d wr %d\n",context->gettid(),fds[0],fds[1]);
#endif
  setSysRet(context,fds[0],fds[1]);
}
template<class defs>
void LinuxSys::sysDup(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint oldfd=args.get<Tint>();
  // Do the actual call
  Tint newfd=context->getOpenFiles()->dup(oldfd);
  if(newfd==-1)
    return setSysErr(context,getErrCode(errno));
#ifdef DEBUG_FILES
  printf("[%d] dup %d as %d\n",context->gettid(),oldfd,newfd);
#endif
  setSysRet(context,newfd);
}
template<class defs>
void LinuxSys::sysDup2(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint oldfd=args.get<Tint>();
  Tint newfd=args.get<Tint>();
  // Do the actual call
  if(context->getOpenFiles()->dup2(oldfd,newfd)==-1)
    return setSysErr(context,getErrCode(errno));
#ifdef DEBUG_FILES
  printf("[%d] dup2 %d as %d\n",context->gettid(),oldfd,newfd);
#endif
  setSysRet(context,newfd);
}

template<class defs>
void LinuxSys::sysFCntl(ThreadContext *context, InstDesc *inst){
  // We implement both fcntl and fcntl64 here. the only difference is that
  // fcntl64 also handles F_GETLK64, F_SETLK64, and F_SETLKW64 and fcntl does not
  typedef typename defs::Tint Tint;
  typedef typename defs::FcntlCmd FcntlCmd;
  typedef typename defs::FcntlFlags FcntlFlags;
  typedef typename defs::OpenFlags OpenFlags;
  FuncArgs args(context);
  Tint fd(args.get<Tint>());
  FcntlCmd cmd(args.get<typeof(cmd.val)>());
  if(cmd.isF_DUPFD()){
#ifdef DEBUG_FILES
    printf("[%d] dupfd %d called\n",context->gettid(),fd);
#endif
    Tint minfd(args.get<Tint>());
    int  newfd = context->getOpenFiles()->dupfd(fd,minfd);
    if(newfd==-1)
      return setSysErr(context,getErrCode(errno));
#ifdef DEBUG_FILES
    printf("[%d] dupfd %d as %d\n",context->gettid(),fd,newfd);
#endif
    I(newfd>=minfd);
    I(context->getOpenFiles()->isOpen(fd));
    I(context->getOpenFiles()->isOpen(newfd));
    I(context->getOpenFiles()->getDesc(fd)->getStatus()==context->getOpenFiles()->getDesc(newfd)->getStatus());
    return setSysRet(context,Tint(newfd));
  }
  if(cmd.isF_GETFD()){
#ifdef DEBUG_FILES
    printf("[%d] getfd %d called\n",context->gettid(),fd);
#endif
    int cloex=context->getOpenFiles()->getfd(fd);
    if(cloex==-1)
      return setSysErr(context,getErrCode(errno));
    return setSysRet(context,FcntlFlags::fromNat(cloex).val);
  }
  if(cmd.isF_SETFD()){
    FcntlFlags cloex(args.get<typeof(cloex.val)>());
#ifdef DEBUG_FILES
    printf("[%d] setfd %d to %d called\n",context->gettid(),fd,cloex.val);
#endif
    if(context->getOpenFiles()->setfd(fd,cloex.toNat())==-1)
      return setSysErr(context,getErrCode(errno));
#ifdef DEBUG_FILES
    printf("[%d] setfd %d to %d \n",context->gettid(),fd,cloex.val);
#endif
    return setSysRet(context,0);
  }
  if(cmd.isF_GETFL()){
#ifdef DEBUG_FILES
    printf("[%d] getfl %d called\n",context->gettid(),fd);
#endif
    int flags=context->getOpenFiles()->getfl(fd);
    if(flags==-1)
      return setSysErr(context,getErrCode(errno));
    return setSysRet(context,OpenFlags::fromNat(flags).val);
  }
  fail("sysCall32_fcntl64 unknown command %d on file %d\n",cmd.val,fd);
}
template<class defs>
void LinuxSys::sysRead(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint    Tint;
  typedef typename defs::Tintptr_t    Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tssize_t Tssize_t;
  FuncArgs args(context);
  Tint fd(args.get<Tint>());
  Tintptr_t buf(args.get<Tintptr_t>());
  Tsize_t count(args.get<Tsize_t>());
  if(!context->canWrite(buf,count))
    return setSysErr(context,ErrFault);
  ssize_t rv=context->writeMemFromFile(buf,count,fd,false);
  if((rv==-1)&&(errno==EAGAIN)&&!(context->getOpenFiles()->getfl(fd)&O_NONBLOCK)){
    // Enable SigIO
    SignalSet newMask=context->getSignalMask();
    if(newMask.test(SigIO))
      fail("LinuxSys::sysRead_(): SigIO masked out, not supported\n");
//       newMask.reset(SigIO);
//       sstate->pushMask(newMask);
    if(context->hasReadySignal()){
      setSysErr(context,ErrIntr);
      SigInfo *sigInfo=context->nextReadySignal();
//      sstate->popMask();
      handleSignal(context,sigInfo);
      delete sigInfo;
      return;
    }
    context->updIAddr(-inst->aupdate,-1);
    I(inst==context->getIDesc());
    I(inst==context->virt2inst(context->getIAddr()));
    context->getOpenFiles()->addReadBlock(fd,context->gettid());
//    sstate->setWakeup(new SigCallBack(context->gettid(),true));
#if (defined DEBUG_SIGNALS)
    printf("Suspend %d in sysCall32_read(fd=%d)\n",context->gettid(),fd);
    context->dumpCallStack();
    printf("Also suspended:");
    for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
      printf(" %d",*suspIt);
    printf("\n");
    suspSet.insert(context->gettid());
#endif
    context->suspend();
    return;
  }
#ifdef DEBUG_FILES
  printf("[%d] read %d wants %ld gets %ld bytes\n",context->gettid(),fd,(long)count,(long)rv);
#endif
  if(rv==-1)
    return setSysErr(context,getErrCode(errno));
  return setSysRet(context,Tssize_t(rv));
}
template<class defs>
void LinuxSys::sysWrite(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint    Tint;
  typedef typename defs::Tintptr_t    Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tssize_t Tssize_t;
  FuncArgs args(context);
  Tint fd(args.get<Tint>());
  Tintptr_t buf(args.get<Tintptr_t>());
  Tsize_t count(args.get<Tsize_t>());
  if(!context->canRead(buf,count))
    return setSysErr(context,ErrFault);
  ssize_t rv=context->readMemToFile(buf,count,fd,false);
  if((rv==-1)&&(errno==EAGAIN)&&!(context->getOpenFiles()->getfl(fd)&O_NONBLOCK)){
    // Enable SigIO
    SignalSet newMask=context->getSignalMask();
    if(newMask.test(SigIO))
      fail("LinuxSys::sysWrite_(): SigIO masked out, not supported\n");
    if(context->hasReadySignal()){
      setSysErr(context,ErrIntr);
      SigInfo *sigInfo=context->nextReadySignal();
      handleSignal(context,sigInfo);
      delete sigInfo;
      return;
    }
    context->updIAddr(-inst->aupdate,-1);
    I(inst==context->getIDesc());
    I(inst==context->virt2inst(context->getIAddr()));
    context->getOpenFiles()->addWriteBlock(fd,context->gettid());
#if (defined DEBUG_SIGNALS)
    printf("Suspend %d in sysCall32_write(fd=%d)\n",context->gettid(),fd);
    context->dumpCallStack();
    printf("Also suspended:");
    for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
      printf(" %d",*suspIt);
    printf("\n");
    suspSet.insert(context->gettid());
#endif
    context->suspend();
    return;
  }
#ifdef DEBUG_FILES
  printf("[%d] write %d wants %ld gets %ld bytes\n",context->gettid(),fd,(long)count,(long)rv);
#endif
  if(rv==-1)
    return setSysErr(context,getErrCode(errno));
  return setSysRet(context,Tssize_t(rv));
}
template<class defs>
void LinuxSys::sysWriteV(ThreadContext *context, InstDesc *inst){
  I(0);
  typedef typename defs::Tint Tint;
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tssize_t Tssize_t;
  typedef typename defs::Tiovec Tiovec;
  FuncArgs args(context);
  Tint fd(args.get<Tint>());
  Tintptr_t vector(args.get<Tintptr_t>());
  Tsize_t count(args.get<Tsize_t>());
  if(!context->canRead(vector,count*sizeof(Tiovec)))
    return setSysErr(context,ErrFault);
  Tssize_t rv=0;
  for(Tssize_t i=0;i<Tssize_t(count);i++){
    Tiovec iov(context,vector+i*sizeof(Tiovec));
    if(!context->canRead(iov.iov_base,iov.iov_len))
      return setSysErr(context,ErrFault);
    ssize_t nowBytes=context->readMemToFile(iov.iov_base,iov.iov_len,fd,false);
    if(nowBytes==-1)
      return setSysErr(context,getErrCode(errno));
    rv+=nowBytes;
    if((size_t)nowBytes<iov.iov_len)
      break;
  }
  setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysLSeek(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Toff_t Toff_t;
  typedef typename defs::Whence Whence;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  Toff_t offset=args.get<Toff_t>();
  Whence whence=args.get<Tuint>();
  off_t rv=context->getOpenFiles()->seek(fd,offset,whence.toNat());
  if(rv==(off_t)-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,Toff_t(rv));
}
template<class defs>
void LinuxSys::sysLLSeek(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tint Tint;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tulong Tulong;
  typedef typename defs::Tloff_t Tloff_t;
  typedef typename defs::Whence Whence;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  Tulong offset_high=args.get<Tulong>();
  Tulong offset_low=args.get<Tulong>();
  Tintptr_t result=args.get<Tintptr_t>();
  Whence whence=args.get<Tuint>();
  if(!context->canWrite(result,sizeof(Tloff_t)))
    return setSysErr(context,ErrFault);
  off_t offset=(off_t)((((uint64_t)offset_high)<<32)|((uint64_t)offset_low));
  off_t rv=context->getOpenFiles()->seek(fd,offset,whence.toNat());
  if(rv==(off_t)-1)
    return setSysErr(context,getErrCode(errno));
  context->writeMem<Tloff_t>(result,Tloff_t(rv));
  setSysRet(context,0);
}
template<class defs, class Tdirent>
void LinuxSys::sysGetDEnts(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  FuncArgs args(context);
  Tint fd(args.get<Tint>());
  Tintptr_t dirp(args.get<Tintptr_t>());
  Tsize_t count(args.get<Tsize_t>());
  if(!context->getOpenFiles()->isOpen(fd))
    return setSysErr(context,ErrBadf);
 #ifdef DEBUG_FILES
  printf("[%d] getdents from %d (%d entries to 0x%08x)\n",context->gettid(),fd,count,dirp);
#endif
 FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
  int realfd=desc->getStatus()->fd;
  // How many bytes have been read so far
  Tsize_t rv=0;
  // Get the current position in the directory file
  off_t dirPos=lseek(realfd,0,SEEK_CUR);
  I(dirPos!=(off_t)-1);
  while(true){
    struct dirent64 natDent;
    __off64_t dummyDirPos=dirPos;
    ssize_t rdBytes=getdirentries64(realfd,(char *)(&natDent),sizeof(natDent),&dummyDirPos);
    if(rdBytes==-1)
      return setSysErr(context,getErrCode(errno));
    if(rdBytes==0)
      break;
#ifdef DEBUG_FILES
    printf("  d_type %d d_name %s\n",natDent.d_type,natDent.d_name);
#endif
    Tdirent simDent(natDent);
    if(rv+simDent.d_reclen>count){
      lseek(realfd,dirPos,SEEK_SET);
      if(rv==0)
	return setSysErr(context,ErrInval);
      break;
    }
    if(!context->canWrite(dirp+rv,simDent.d_reclen))
      return setSysErr(context,ErrFault);
    simDent.write(context,dirp+rv);
    dirPos=lseek(realfd,natDent.d_off,SEEK_SET);
    rv+=simDent.d_reclen;
  }
  setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysIOCtl(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::IoctlReq IoctlReq;
  typedef typename defs::Twinsize Twinsize;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  IoctlReq cmd=args.get<typeof(cmd.val)>();
  if(!context->getOpenFiles()->isOpen(fd))
    return setSysErr(context,ErrBadf);
  FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
  int realfd=desc->getStatus()->fd;
  if(cmd.isTCGETS()){
    struct Mips32_kernel_termios{
      uint32_t c_iflag; // Input mode flags
      uint32_t c_oflag; // Output mode flags
      uint32_t c_cflag; // Control mode flags
      uint32_t c_lflag; // Local mode flags
      unsigned char c_line;      // Line discipline
      unsigned char c_cc[19]; // Control characters
    };
    // TODO: For now, it never succeeds (assume no file is a terminal)
    return setSysErr(context,ErrNoTTY);
  }else if(cmd.isTIOCGWINSZ()){
    Tintptr_t wsiz=args.get<Tintptr_t>();
    if(!context->canWrite(wsiz,sizeof(Twinsize)))
      return setSysErr(context,ErrFault);
    struct winsize natWinsize;
    int retVal=ioctl(realfd,TIOCGWINSZ,&natWinsize);
    if(retVal==-1)
      return setSysErr(context,getErrCode(errno));
    Twinsize(natWinsize).write(context,wsiz);
  }else if(cmd.isTCSETSW()){
    // TODO: For now, it never succeeds (assume no file is a terminal)
    return setSysErr(context,ErrNoTTY);
  }else
    fail("Mips::sysCall32_ioctl called with fd %d and req 0x%x at 0x%08x\n",
	 fd,cmd.val,context->getIAddr());
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysPoll(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tlong Tlong;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tpollfd Tpollfd;
  FuncArgs args(context);
  Tintptr_t fds=args.get<Tintptr_t>();
  Tuint nfds=args.get<Tuint>();
  Tlong timeout=args.get<Tlong>();
  if((!context->canRead(fds,nfds*sizeof(Tpollfd)))||
     (!context->canWrite(fds,nfds*sizeof(Tpollfd))))
    return setSysErr(context,ErrFault);
  int fdList[nfds];
  Tlong rv=0;
  for(size_t i=0;i<nfds;i++){
    Tpollfd myFd(context,fds+i*sizeof(Tpollfd));
    fdList[i]=myFd.fd;
    myFd.revents=0;
    if(!context->getOpenFiles()->isOpen(myFd.fd)){
      myFd.revents.setPOLLNVAL();
    }else if((myFd.events.hasPOLLIN())&&(!context->getOpenFiles()->willReadBlock(myFd.fd))){
      myFd.revents.setPOLLIN();
    }else
      continue;
    myFd.write(context,fds+i*sizeof(Tpollfd));
    rv++;
  }
  // If any of the files could be read withut blocking, we're done
  if(rv!=0)
    return setSysRet(context,rv);
  // We need to block and wait
  // Enable SigIO
  SignalSet newMask=context->getSignalMask();
  if(newMask.test(SigIO))
    fail("sysCall32_read: SigIO masked out, not supported\n");
  //     newMask.reset(SigIO);
  //     sstate->pushMask(newMask);
  if(context->hasReadySignal()){
    setSysErr(context,ErrIntr);
    SigInfo *sigInfo=context->nextReadySignal();
    //       sstate->popMask();
    handleSignal(context,sigInfo);
    delete sigInfo;
    return;
  }
  // Set up a callback, add a read block on each file, and suspend
  //     sstate->setWakeup(new SigCallBack(context->gettid(),true));
#if (defined DEBUG_SIGNALS)
  printf("Suspend %d in sysCall32_poll(fds=",context->gettid());
#endif
  for(size_t i=0;i<nfds;i++){
#if (defined DEBUG_SIGNALS)
    printf(" %d",fdList[i]);
#endif
    context->getOpenFiles()->addReadBlock(fdList[i],context->gettid());
  }
#if (defined DEBUG_SIGNALS)
  printf(")\n");
  context->dumpCallStack();
  printf("Also suspended:");
  for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
    printf(" %d",*suspIt);
  printf("\n");
  suspSet.insert(context->gettid());
#endif
  context->suspend();
}
template<class defs>
void LinuxSys::sysClose(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  // Do the actual call
  if(context->getOpenFiles()->close(fd)==-1)
    return setSysErr(context,getErrCode(errno));
#ifdef DEBUG_FILES
  printf("[%d] close %d\n",context->gettid(),fd);
#endif
  setSysRet(context,0);
}

template<class defs, class Toff_t>
void LinuxSys::sysTruncate(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr path(context,args.get<Tintptr_t>());
  if(!path)
    return;
  Toff_t length=args.get<Toff_t>();
#ifdef DEBUG_FILES
  printf("[%d] truncate %s to %ld\n",context->gettid(),(const char *)path,(long)length);
#endif
  if(truncate(path,(off_t)length)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs, class Toff_t>
void LinuxSys::sysFTruncate(ThreadContext *context, InstDesc *inst){
  I(0);
  typedef typename defs::Tint Tint;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  Toff_t length=args.get<Toff_t>();
#ifdef DEBUG_FILES
  printf("[%d] ftruncate %d to %ld\n",context->gettid(),fd,(long)length);
#endif
  if(!context->getOpenFiles()->isOpen(fd))
    return setSysErr(context,ErrBadf);
  FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
  int realfd=desc->getStatus()->fd;
  if(ftruncate(realfd,(off_t)length)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysChMod(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tmode_t Tmode_t;
  FuncArgs args(context);
  Tstr path(context,args.get<Tintptr_t>());
  if(!path)
    return;
  Tmode_t mode(args.get<Tuint>());
#ifdef DEBUG_FILES
  printf("[%d] chmod %s to %d\n",context->gettid(),(const char *)path,mode.val);
#endif
  if(chmod(path,mode)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs, bool link, class Tstat>
void LinuxSys::sysStat(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr path(context,args.get<Tintptr_t>());
  if(!path)
    return;
  Tintptr_t buf=args.get<Tintptr_t>();
#ifdef DEBUG_FILES
  printf("[%d] %cstat %s\n",context->gettid(),link?'l':' ',(const char *)path);
#endif
  if(!context->canWrite(buf,sizeof(Tstat)))
    return setSysErr(context,ErrFault);
  struct stat natStat;
  if(link?lstat(path,&natStat):stat(path,&natStat))
    return setSysErr(context,getErrCode(errno));
  Tstat(natStat).write(context,buf);
  setSysRet(context,0);
}
template<class defs, class Tstat>
void LinuxSys::sysFStat(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  Tintptr_t buf=args.get<Tintptr_t>();
  if(!context->canWrite(buf,sizeof(Tstat)))
    return setSysErr(context,ErrFault);
  if(!context->getOpenFiles()->isOpen(fd))
    return setSysErr(context,ErrBadf);
  FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
  int realfd=desc->getStatus()->fd;
  struct stat natStat;
  if(fstat(realfd,&natStat)==-1)
    return setSysErr(context,getErrCode(errno));
  // Make standard in, out, and err look like TTY devices
  if(fd<3){
    // Change format to character device
    natStat.st_mode&=(~S_IFMT);
    natStat.st_mode|=S_IFCHR;
    // Change device to a TTY device type
    // DEV_TTY_LOW_MAJOR is 136, DEV_TTY_HIGH_MAJOR is 143
    // We use a major number of 136 (0x88)
    natStat.st_rdev=0x8800;
  }
  Tstat(natStat).write(context,buf);
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysFStatFS(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tint Tint;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tint fd=args.get<Tint>();
  Tintptr_t buf=args.get<Tintptr_t>();
  printf("sysFStatFS(%d,%d) called (continuing with ENOSYS).\n",fd,buf);
  setSysErr(context,ErrNoSys); 
}
template<class defs>
void LinuxSys::sysUnlink(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr pathname(context,args.get<Tintptr_t>());
  if(!pathname)
    return;
#ifdef DEBUG_FILES
  printf("[%d] unlink %s\n",context->gettid(),(const char *)pathname);
#endif
  if(unlink(pathname)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysSymLink(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr path1(context,args.get<Tintptr_t>());
  Tstr path2(context,args.get<Tintptr_t>());
  if(!path2)
    return;
#ifdef DEBUG_FILES
  printf("[%d] symlink %s to %s\n",context->gettid(),(const char *)path2,(const char *)path1);
#endif
  if(symlink(path1,path2)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysRename(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr oldpath(context,args.get<Tintptr_t>());
  if(!oldpath)
    return;
  Tstr newpath(context,args.get<Tintptr_t>());
  if(!oldpath)
    return;
#ifdef DEBUG_FILES
  printf("[%d] rename %s to %s\n",context->gettid(),(const char *)oldpath,(const char *)newpath);
#endif
  if(rename(oldpath,newpath)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysChdir(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr path(context,args.get<Tintptr_t>());
  if(!path)
    return;
#ifdef DEBUG_FILES
  printf("[%d] chdir %s\n",context->gettid(),(const char *)path);
#endif
  if(chdir(path)==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysAccess(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::AMode AMode;
  FuncArgs args(context);
  Tstr fname(context,args.get<Tintptr_t>());
  if(!fname)
    return;
  // TODO: Translate file name using mount info
#ifdef DEBUG_FILES
  printf("[%d] access %s\n",context->gettid(),(const char *)fname);
#endif
  AMode mode(args.get<Tuint>());
  if(access(fname,mode.toNat())==-1)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysGetCWD(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  FuncArgs args(context);
  Tintptr_t buf(args.get<Tintptr_t>());
  Tsize_t size(args.get<Tsize_t>());
  char realBuf[size];
  if(!getcwd(realBuf,size))
    return setSysErr(context,getErrCode(errno));
  // TODO: Translate directory name using mount info
  int rv=strlen(realBuf)+1;
  if(!context->canWrite(buf,rv))
    return setSysErr(context,ErrFault);
  context->writeMemFromBuf(buf,rv,realBuf);
  return setSysRet(context,rv);
}
template<class defs>
void LinuxSys::sysMkdir(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tmode_t Tmode_t;
  FuncArgs args(context);
  Tstr pathname(context,args.get<Tintptr_t>());
  if(!pathname)
    return;
  Tmode_t mode(args.get<Tuint>());
  if(mkdir(pathname,mode)!=0)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysRmdir(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tstr pathname(context,args.get<Tintptr_t>());
  if(!pathname)
    return;
  if(rmdir(pathname)!=0)
    return setSysErr(context,getErrCode(errno));
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysUmask(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tuint Tuint;
  typedef typename defs::Tmode_t Tmode_t;
  FuncArgs args(context);
  Tmode_t mask(args.get<Tuint>());
  // TODO: Implement this call for non-zero mask
  if(mask.val)
    printf("sysUmask_(0x%08x) called\n",mask.val);
  setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysReadLink(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tssize_t Tssize_t;
  FuncArgs args(context);
  Tstr path(context,args.get<Tintptr_t>());
  if(!path)
    return;
#ifdef DEBUG_FILES
  printf("[%d] readlink %s\n",context->gettid(),(const char *)path);
#endif
  Tintptr_t buf=args.get<Tintptr_t>();
  Tsize_t bufsiz=args.get<Tsize_t>();
  char bufBuf[bufsiz];
  Tssize_t bufLen=readlink(path,bufBuf,bufsiz);
  if(bufLen==-1)
    return setSysErr(context,getErrCode(errno));
  if(!context->canWrite(buf,bufLen))
    return setSysErr(context,ErrFault);
  context->writeMemFromBuf(buf,bufLen,bufBuf);
  setSysRet(context,bufLen);         
}

template<class defs>
void LinuxSys::sysUname(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tutsname Tutsname;
  FuncArgs args(context);
  Tintptr_t buf(args.get<Tintptr_t>());
  Tutsname bufBuf;
  if(!context->canWrite(buf,sizeof(Tutsname)))
    return setSysErr(context,ErrFault);
  memset(&bufBuf,0,sizeof(Tutsname));
  strcpy(bufBuf.sysname,"GNU/Linux");
  strcpy(bufBuf.nodename,"sesc");
  strcpy(bufBuf.release,"2.6.22");
  strcpy(bufBuf.version,"#1 SMP Tue Jun 4 16:05:29 CDT 2002");
  strcpy(bufBuf.machine,"mips");
  strcpy(bufBuf.domainname,"Sesc");
  context->writeMemFromBuf(buf,sizeof(Tutsname),&bufBuf);
  return setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysSysCtl(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tintptr_t Tintptr_t;
  typedef typename defs::Tint Tint;
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::T__sysctl_args T__sysctl_args;
  typedef typename defs::SysCtl SysCtl;
  FuncArgs args(context);
  Tintptr_t argsptr=args.get<Tintptr_t>();
  if(!context->canRead(argsptr,sizeof(T__sysctl_args)))
    return setSysErr(context,ErrFault);
  T__sysctl_args argsbuf(context,argsptr);
  if(!context->canRead(argsbuf.name,argsbuf.nlen*sizeof(Tint)))
    return setSysErr(context,ErrFault);        
  SysCtl ctl(context->readMem<Tint>(argsbuf.name+0*sizeof(Tint)));
  if(ctl.isCTL_KERN()){
    typedef typename defs::SysCtlKern SysCtlKern;
    if(argsbuf.nlen<=1)
      return setSysErr(context,ErrNotDir);
    SysCtlKern kern(context->readMem<Tint>(argsbuf.name+1*sizeof(Tint)));
    if(kern.isKERN_VERSION()){
      if(argsbuf.newval!=0)
        return setSysErr(context,ErrPerm);
      if(!context->canRead(argsbuf.oldlenp,sizeof(Tsize_t)))
        return setSysErr(context,ErrFault);
      Tsize_t oldlen=context->readMem<Tsize_t>(argsbuf.oldlenp);
      char ver[]="#1 SMP Tue Jun 4 16:05:29 CDT 2002";
      Tsize_t verlen=strlen(ver)+1;
      if(oldlen<verlen)
        return setSysErr(context,ErrFault);
      if(!context->canWrite(argsbuf.oldlenp,sizeof(Tsize_t)))
        return setSysErr(context,ErrFault);
      if(!context->canWrite(argsbuf.oldval,verlen))
        return setSysErr(context,ErrFault);
      context->writeMemFromBuf(argsbuf.oldval,verlen,ver);
      context->writeMem(argsbuf.oldlenp,verlen);
    }else{
      fail("sysCall32__sysctl: KERN name %d not supported\n",kern.val);
      return setSysErr(context,ErrNotDir);
    }
  }else{
    fail("sysCall32__sysctl: top-level name %d not supported\n",ctl.val);
    return setSysErr(context,ErrNotDir);
  }
  return setSysRet(context,0);
}
template<class defs>
void LinuxSys::sysGetUid(ThreadContext *context, InstDesc *inst){
  // TODO: With different users, need to track simulated uid
  return setSysRet(context,getuid());
}
template<class defs>
void LinuxSys::sysGetEuid(ThreadContext *context, InstDesc *inst){
  // TODO: With different users, need to track simulated euid
  return setSysRet(context,geteuid());
}
template<class defs>
void LinuxSys::sysGetGid(ThreadContext *context, InstDesc *inst){
  // TODO: With different users, need to track simulated gid
  return setSysRet(context,getgid());
}
template<class defs>
void LinuxSys::sysGetEgid(ThreadContext *context, InstDesc *inst){
  // TODO: With different users, need to track simulated egid
  return setSysRet(context,getegid());
}
template<class defs>
void LinuxSys::sysGetGroups(ThreadContext *context, InstDesc *inst){
  typedef typename defs::Tsize_t Tsize_t;
  typedef typename defs::Tintptr_t Tintptr_t;
  FuncArgs args(context);
  Tsize_t size=args.get<Tsize_t>();
  Tintptr_t list=args.get<Tintptr_t>();
#if (defined DEBUG_BENCH)
  printf("sysCall32_getgroups(%ld,0x%08x)called\n",(long)size,list);
#endif
  setSysRet(context,0);
}

template<class defs>
void LinuxSys::sysSocket(ThreadContext *context, InstDesc *inst){
#if (defined DEBUG_BENCH) || (defined DEBUG_SOCKET)
  printf("sysCall32_socket: not implemented at 0x%08x\n",context->getIAddr());
#endif
  setSysErr(context,ErrAfNoSupport);
}
template<class defs>
void LinuxSys::sysConnect(ThreadContext *context, InstDesc *inst){
#if (defined DEBUG_BENCH) || (defined DEBUG_SOCKET)
  printf("sysConnect called (continuing with EAFNOSUPPORT)\n");
#endif
  setSysErr(context,ErrAfNoSupport);
}
template<class defs>
void LinuxSys::sysSend(ThreadContext *context, InstDesc *inst){
#if (defined DEBUG_BENCH) || (defined DEBUG_SOCKET)
  printf("sysSend called (continuing with EAFNOSUPPORT)\n");
#endif
  setSysErr(context,ErrAfNoSupport);
}
template<class defs>
void LinuxSys::sysBind(ThreadContext *context, InstDesc *inst){
#if (defined DEBUG_BENCH) || (defined DEBUG_SOCKET)
  printf("sysBind called (continuing with EAFNOSUPPORT)\n");
#endif
  setSysErr(context,ErrAfNoSupport);
}


uint32_t Mips32LinuxSys::getArgI32(ThreadContext *context, size_t &pos) const{
  I((pos%sizeof(uint32_t))==0);
  size_t argpos=pos;
  pos+=sizeof(uint32_t);
  if(argpos<16)
    return Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0+argpos/sizeof(uint32_t));
  return context->readMem<uint32_t>(Mips::getRegAny<Mips32,Mips32Defs::Tintptr_t,Mips::RegSP>(context,Mips::RegSP)+argpos);
}
uint64_t Mips32LinuxSys::getArgI64(ThreadContext *context, size_t &pos) const{
  I((pos%sizeof(uint32_t))==0);
  if(pos%sizeof(uint64_t))
    pos+=sizeof(uint32_t);
  I((pos%sizeof(uint64_t))==0);
  size_t argpos=pos;
  pos+=sizeof(uint64_t);
  if(argpos<16){
    uint64_t rv=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0+argpos/sizeof(uint32_t));
    rv<<=32;
    return rv|Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0+argpos/sizeof(uint32_t)+1);
  }
  return context->readMem<uint64_t>(Mips::getRegAny<Mips32,Mips32Defs::Tintptr_t,Mips::RegSP>(context,Mips::RegSP)+argpos);
}
void Mips32LinuxSys::setSysErr(ThreadContext *context, ErrCode err) const{
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegA3,(int32_t)1);
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegV0,Mips32Defs::ErrCode(err).val);
}

void Mips32LinuxSys::setSysRet(ThreadContext *context, int val) const{
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegA3,(int32_t)0);
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegV0,val);
}

void Mips32LinuxSys::setSysRet(ThreadContext *context, int val1, int val2) const{
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegA3,(int32_t)0);
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegV0,val1);
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegV1,val2);
}
void Mips32LinuxSys::setProgArgs(ThreadContext *context, int argc, char **argv, int envc, char **envp) const{
  setProgArgs_<Mips32Defs>(context,argc,argv,envc,envp);
}
void Mips32LinuxSys::exitRobustList(ThreadContext *context, VAddr robust_list){
  exitRobustList_<Mips32Defs>(context,robust_list);
}
void Mips32LinuxSys::setThreadArea(ThreadContext *context, VAddr addr) const{
  Mips::setRegAny<Mips32,Mips32Defs::Tintptr_t,Mips::RegTPtr>(context,Mips::RegTPtr,addr);
}
bool Mips32LinuxSys::stackGrowsDown(void) const{
  return true;
}
VAddr Mips32LinuxSys::getStackPointer(ThreadContext *context) const{
  return Mips::getRegAny<Mips32,Mips32Defs::Tintptr_t,Mips::RegSP>(context,Mips::RegSP);
}
void Mips32LinuxSys::setStackPointer(ThreadContext *context, VAddr addr) const{
  Mips::setRegAny<Mips32,Mips32Defs::Tintptr_t,Mips::RegSP>(context,Mips::RegSP,addr);
}
InstDesc *Mips32LinuxSys::sysCall(ThreadContext *context, InstDesc *inst){
  context->updIAddr(inst->aupdate,1);
  uint32_t sysCallNum=Mips::getRegAny<Mips32,uint32_t,Mips::RegV0>(context,Mips::RegV0);
  LinuxSys *system=context->getSystem();
  switch(sysCallNum){
  case 4001: case 4003: case 4004: case 4005: case 4006: case 4007: case 4010: 
  case 4011: case 4013: case 4019:
  case 4020: case 4024: case 4033: case 4038: case 4041: case 4042: case 4043: case 4045: case 4047: case 4049: 
  case 4050:
  case 4054: case 4060:
  case 4063: case 4064: case 4075: case 4076: case 4077: case 4080: case 4090: case 4091: case 4106: 
  case 4108: case 4114: case 4120: case 4122: 
  case 4125: case 4140: case 4146: case 4153: case 4166: case 4167: case 4169: case 4183: case 4194: case 4195:
  case 4203: case 4210:
  case 4212: case 4213: case 4214: case 4215: case 4219: case 4220: case 4238: case 4246: case 4252:
  case 4283: case 4309:
    break;
  default:
    printf("Using Mips32Linuxsys::sysCall %d\n",sysCallNum);
  }
  switch(sysCallNum){
//  case 4000: Mips::sysCall32_syscall(inst,context); break;
  case 4001: return sysExit<Mips32Defs>(context,inst); break;
  case 4002: /*Untested*/ sysFork<Mips32Defs>(context,inst); break;
  case 4003: sysRead<Mips32Defs>(context,inst); break;
  case 4004: sysWrite<Mips32Defs>(context,inst);  break;
  case 4005: sysOpen<Mips32Defs>(context,inst); break;
  case 4006: sysClose<Mips32Defs>(context,inst); break;
  case 4007: sysWaitpid<Mips32Defs>(context,inst); break;
//  case 4008: Mips::sysCall32_creat(inst,context); break;
//  case 4009: Mips::sysCall32_link(inst,context); break;
  case 4010: sysUnlink<Mips32Defs>(context,inst); break;
  case 4011: sysExecVe<Mips32Defs>(context,inst); break;
  case 4012: /*Untested*/ sysChdir<Mips32Defs>(context,inst); break;
  case 4013: sysTime<Mips32Defs>(context,inst); break;
//  case 4014: Mips::sysCall32_mknod(inst,context); break;
  case 4015: /*Untested*/sysChMod<Mips32Defs>(context,inst); break;
//  case 4016: Mips::sysCall32_lchown(inst,context); break;
//  case 4017: Mips::sysCall32_break(inst,context); break;
//  case 4018: Mips::sysCall32_oldstat(inst,context); break;
  case 4019: sysLSeek<Mips32Defs>(context,inst); break;
  case 4020: sysGetPid<Mips32Defs>(context,inst); break;
//  case 4021: Mips::sysCall32_mount(inst,context); break;
//  case 4022: Mips::sysCall32_umount(inst,context); break;
//  case 4023: Mips::sysCall32_setuid(inst,context); break;
  case 4024: sysGetUid<Mips32Defs>(context,inst); break;
//  case 4025: Mips::sysCall32_stime(inst,context); break;
//  case 4026: Mips::sysCall32_ptrace(inst,context); break;
  case 4027: /*Untested*/ sysAlarm<Mips32Defs>(context,inst); break;
//  case 4028: Mips::sysCall32_oldfstat(inst,context); break;
//  case 4029: Mips::sysCall32_pause(inst,context); break;
//  case 4030: Mips::sysCall32_utime(inst,context); break;
//  case 4031: Mips::sysCall32_stty(inst,context); break;
//  case 4032: Mips::sysCall32_gtty(inst,context); break;
  case 4033: sysAccess<Mips32Defs>(context,inst); break;
//  case 4034: Mips::sysCall32_nice(inst,context); break;
//  case 4035: Mips::sysCall32_ftime(inst,context); break;
//  case 4036: Mips::sysCall32_sync(inst,context); break;
  case 4037: /*Untested*/ sysKill<Mips32Defs>(context,inst); break;
  case 4038: sysRename<Mips32Defs>(context,inst); break;
  case 4039: /*Untested*/ sysMkdir<Mips32Defs>(context,inst); break;
  case 4040: /*Untested*/ sysRmdir<Mips32Defs>(context,inst); break;
  case 4041: sysDup<Mips32Defs>(context,inst); break;
  case 4042: sysPipe<Mips32Defs>(context,inst); break;
  case 4043: sysTimes<Mips32Defs>(context,inst); break;
//  case 4044: Mips::sysCall32_prof(inst,context); break;
  case 4045: sysBrk<Mips32Defs>(context,inst); break;
//  case 4046: Mips::sysCall32_setgid(inst,context); break;
  case 4047: sysGetGid<Mips32Defs>(context,inst); break;
//  case 4048: Mips::sysCall32_signal(inst,context); break;
  case 4049: sysGetEuid<Mips32Defs>(context,inst); break;
  case 4050: sysGetEgid<Mips32Defs>(context,inst); break;
//  case 4051: Mips::sysCall32_acct(inst,context); break;
//  case 4052: Mips::sysCall32_umount2(inst,context); break;
//  case 4053: Mips::sysCall32_lock(inst,context); break;
  case 4054: sysIOCtl<Mips32Defs>(context,inst); break;
  case 4055: /* Untested */ sysFCntl<Mips32Defs>(context,inst); break;
//  case 4056: Mips::sysCall32_mpx(inst,context); break;
//  case 4057: Mips::sysCall32_setpgid(inst,context); break;
//  case 4058: Mips::sysCall32_ulimit(inst,context); break;
//  case 4059: Mips::sysCall32_unused59(inst,context); break;
  case 4060: sysUmask<Mips32Defs>(context,inst); break;
//  case 4061: Mips::sysCall32_chroot(inst,context); break;
//  case 4062: Mips::sysCall32_ustat(inst,context); break;
  case 4063: sysDup2<Mips32Defs>(context,inst); break;
  case 4064: sysGetPPid<Mips32Defs>(context,inst); break;
//  case 4065: Mips::sysCall32_getpgrp(inst,context); break;
//  case 4066: Mips::sysCall32_setsid(inst,context); break;
//  case 4067: Mips::sysCall32_sigaction(inst,context); break;
//  case 4068: Mips::sysCall32_sgetmask(inst,context); break;
//  case 4069: Mips::sysCall32_ssetmask(inst,context); break;
//  case 4070: Mips::sysCall32_setreuid(inst,context); break;
//  case 4071: Mips::sysCall32_setregid(inst,context); break;
//  case 4072: Mips::sysCall32_sigsuspend(inst,context); break;
//  case 4073: Mips::sysCall32_sigpending(inst,context); break;
//  case 4074: Mips::sysCall32_sethostname(inst,context); break;
  case 4075: sysSetRLimit<Mips32Defs>(context,inst); break;
  case 4076: sysGetRLimit<Mips32Defs>(context,inst); break;
  case 4077: sysGetRUsage<Mips32Defs>(context,inst); break;
  case 4078: /*Untested*/ sysGetTimeOfDay<Mips32Defs>(context,inst); break;
//  case 4079: Mips::sysCall32_settimeofday(inst,context); break;
  case 4080: sysGetGroups<Mips32Defs>(context,inst); break;
//  case 4081: Mips::sysCall32_setgroups(inst,context); break;
//  case 4082: Mips::sysCall32_reserved82(inst,context); break;
  case 4083: /* Untested*/ sysSymLink<Mips32Defs>(context,inst); break;
//  case 4084: Mips::sysCall32_oldlstat(inst,context); break;
  case 4085: /*Untested*/ sysReadLink<Mips32Defs>(context,inst); break;
//  case 4086: Mips::sysCall32_uselib(inst,context); break;
//  case 4087: Mips::sysCall32_swapon(inst,context); break;
//  case 4088: Mips::sysCall32_reboot(inst,context); break;
//  case 4089: Mips::sysCall32_readdir(inst,context); break;
  case 4090: sysMMap<Mips32Defs,1>(context,inst); break;
  case 4091: sysMUnMap<Mips32Defs>(context,inst); break;
  case 4092: /*Untested*/ sysTruncate<Mips32Defs,Mips32Defs::Toff_t>(context,inst); break;
  case 4093: /*Untested*/ sysFTruncate<Mips32Defs,Mips32Defs::Toff_t>(context,inst); break;
//  case 4094: Mips::sysCall32_fchmod(inst,context); break;
//  case 4095: Mips::sysCall32_fchown(inst,context); break;
  case 4096: sysGetPriority<Mips32Defs>(context,inst); break;
//  case 4097: Mips::sysCall32_setpriority(inst,context); break;
//  case 4098: Mips::sysCall32_profil(inst,context); break;
//  case 4099: Mips::sysCall32_statfs(inst,context); break;
  case 4100: sysFStatFS<Mips32Defs>(context,inst); break;
//  case 4101: Mips::sysCall32_ioperm(inst,context); break;
//  case 4102: Mips::sysCall32_socketcall(inst,context); break;
//  case 4103: Mips::sysCall32_syslog(inst,context); break;
  case 4104: sysSetITimer<Mips32Defs>(context,inst); break;
//  case 4105: Mips::sysCall32_getitimer(inst,context); break;
  case 4106: sysStat<Mips32Defs,false,Mips32Defs::Tstat>(context,inst); break;
  case 4107: /*Untested*/ system->sysStat<Mips32Defs,true,Mips32Defs::Tstat>(context,inst); break;
  case 4108: sysFStat<Mips32Defs,Mips32Defs::Tstat>(context,inst); break;
//  case 4109: Mips::sysCall32_unused109(inst,context); break;
//  case 4110: Mips::sysCall32_iopl(inst,context); break;
//  case 4111: Mips::sysCall32_vhangup(inst,context); break;
//  case 4112: Mips::sysCall32_idle(inst,context); break;
//  case 4113: Mips::sysCall32_vm86(inst,context); break;
  case 4114: sysWait4<Mips32Defs>(context,inst); break;
//  case 4115: Mips::sysCall32_swapoff(inst,context); break;
//  case 4116: Mips::sysCall32_sysinfo(inst,context); break;
// If you implement ipc() you must handle CLONE_SYSVSEM in clone()
//  case 4117: Mips::sysCall32_ipc(inst,context); break;
//  case 4118: Mips::sysCall32_fsync(inst,context); break;
//  case 4119: Mips::sysCall32_sigreturn(inst,context); break;
  case 4120: sysClone<Mips32Defs>(context,inst); break;
//  case 4121: Mips::sysCall32_setdomainname(inst,context); break;
  case 4122: sysUname<Mips32Defs>(context,inst); break;
//  case 4123: Mips::sysCall32_modify_ldt(inst,context); break;
//  case 4124: Mips::sysCall32_adjtimex(inst,context); break;
  case 4125: sysMProtect<Mips32Defs>(context,inst); break;
//  case 4126: Mips::sysCall32_sigprocmask(inst,context); break;
//  case 4127: Mips::sysCall32_create_module(inst,context); break;
//  case 4128: Mips::sysCall32_init_module(inst,context); break;
//  case 4129: Mips::sysCall32_delete_module(inst,context); break;
//  case 4130: Mips::sysCall32_get_kernel_syms(inst,context); break;
//  case 4131: Mips::sysCall32_quotactl(inst,context); break;
//  case 4132: Mips::sysCall32_getpgid(inst,context); break;
//  case 4133: Mips::sysCall32_fchdir(inst,context); break;
//  case 4134: Mips::sysCall32_bdflush(inst,context); break;
//  case 4135: Mips::sysCall32_sysfs(inst,context); break;
//  case 4136: Mips::sysCall32_personality(inst,context); break;
//  case 4137: Mips::sysCall32_afs_syscall(inst,context); break;
//  case 4138: Mips::sysCall32_setfsuid(inst,context); break;
//  case 4139: Mips::sysCall32_setfsgid(inst,context); break;
  case 4140: sysLLSeek<Mips32Defs>(context,inst); break;
//  case 4141: Mips::sysCall32_getdents(inst,context); break;
//  case 4142: Mips::sysCall32__newselect(inst,context); break;
//  case 4143: Mips::sysCall32_flock(inst,context); break;
//  case 4144: Mips::sysCall32_msync(inst,context); break;
//  case 4145: Mips::sysCall32_readv(inst,context); break;
  case 4146: sysWriteV<Mips32Defs>(context,inst); break;
//  case 4147: Mips::sysCall32_cacheflush(inst,context); break;
//  case 4148: Mips::sysCall32_cachectl(inst,context); break;
//  case 4149: Mips::sysCall32_sysmips(inst,context); break;
//  case 4150: Mips::sysCall32_unused150(inst,context); break;
//  case 4151: Mips::sysCall32_getsid(inst,context); break;
//  case 4152: Mips::sysCall32_fdatasync(inst,context); break;
  case 4153: sysSysCtl<Mips32Defs>(context,inst); break;
//  case 4154: Mips::sysCall32_mlock(inst,context); break;
//  case 4155: Mips::sysCall32_munlock(inst,context); break;
//  case 4156: Mips::sysCall32_mlockall(inst,context); break;
//  case 4157: Mips::sysCall32_munlockall(inst,context); break;
//  case 4158: Mips::sysCall32_sched_setparam(inst,context); break;
  case 4159: sysSchedGetParam<Mips32Defs>(context,inst); break;
  case 4160: sysSchedSetScheduler<Mips32Defs>(context,inst); break;
  case 4161: sysSchedGetScheduler<Mips32Defs>(context,inst); break;
  case 4162: sysSchedYield<Mips32Defs>(context,inst); break;
  case 4163: sysSchedGetPriorityMax<Mips32Defs>(context,inst); break;
  case 4164: sysSchedGetPriorityMin<Mips32Defs>(context,inst); break;
//  case 4165: Mips::sysCall32_sched_rr_get_interval(inst,context); break;
  case 4166: sysNanoSleep<Mips32Defs>(context,inst); break;
  case 4167: sysMReMap<Mips32Defs>(context,inst); break;
//  case 4168: Mips::sysCall32_accept(inst,context); break;
  case 4169: sysBind<Mips32Defs>(context,inst); break;
  case 4170: sysConnect<Mips32Defs>(context,inst); break;
//  case 4171: Mips::sysCall32_getpeername(inst,context); break;
//  case 4172: Mips::sysCall32_getsockname(inst,context); break;
//  case 4173: Mips::sysCall32_getsockopt(inst,context); break;
//  case 4174: Mips::sysCall32_listen(inst,context); break;
//  case 4175: Mips::sysCall32_recv(inst,context); break;
//  case 4176: Mips::sysCall32_recvfrom(inst,context); break;
//  case 4177: Mips::sysCall32_recvmsg(inst,context); break;
  case 4178: sysSend<Mips32Defs>(context,inst); break;
//  case 4179: Mips::sysCall32_sendmsg(inst,context); break;
//  case 4180: Mips::sysCall32_sendto(inst,context); break;
//  case 4181: Mips::sysCall32_setsockopt(inst,context); break;
//  case 4182: Mips::sysCall32_shutdown(inst,context); break;
  case 4183: sysSocket<Mips32Defs>(context,inst); break;
//  case 4184: Mips::sysCall32_socketpair(inst,context); break;
//  case 4185: Mips::sysCall32_setresuid(inst,context); break;
//  case 4186: Mips::sysCall32_getresuid(inst,context); break;
//  case 4187: Mips::sysCall32_query_module(inst,context); break;
  case 4188: /*Untested*/ sysPoll<Mips32Defs>(context,inst); break;
//  case 4189: Mips::sysCall32_nfsservctl(inst,context); break;
//  case 4190: Mips::sysCall32_setresgid(inst,context); break;
//  case 4191: Mips::sysCall32_getresgid(inst,context); break;
//  case 4192: Mips::sysCall32_prctl(inst,context); break;
  case 4193: /*Untested*/ system->sysRtSigReturn(context,inst); break;
  case 4194: sysRtSigAction<Mips32Defs>(context,inst); break;
  case 4195: sysRtSigProcMask<Mips32Defs>(context,inst); break;
//  case 4196: Mips::sysCall32_rt_sigpending(inst,context); break;
//  case 4197: Mips::sysCall32_rt_sigtimedwait(inst,context); break;
//  case 4198: Mips::sysCall32_rt_sigqueueinfo(inst,context); break;
  case 4199: /*Untested*/ sysRtSigSuspend<Mips32Defs>(context,inst); break;
//  case 4200: Mips::sysCall32_pread64(inst,context); break;
//  case 4201: Mips::sysCall32_pwrite64(inst,context); break;
//  case 4202: Mips::sysCall32_chown(inst,context); break;
  case 4203: sysGetCWD<Mips32Defs>(context,inst); break;
//  case 4204: Mips::sysCall32_capget(inst,context); break;
//  case 4205: Mips::sysCall32_capset(inst,context); break;
//  case 4206: Mips::sysCall32_sigaltstack(inst,context); break;
//  case 4207: Mips::sysCall32_sendfile(inst,context); break;
//  case 4208: Mips::sysCall32_getpmsg(inst,context); break;
//  case 4209: Mips::sysCall32_putpmsg(inst,context); break;
  case 4210: sysMMap<Mips32Defs,4096>(context,inst); break; // This is mmap2 (offset multiplied by 4096)
  case 4211: /*Untested*/ sysTruncate<Mips32Defs,Mips32Defs::Tloff_t>(context,inst); break;
  case 4212: sysFTruncate<Mips32Defs,Mips32Defs::Tloff_t>(context,inst); break;
  case 4213: sysStat<Mips32Defs,false,Mips32Defs::Tstat64>(context,inst); break;
  case 4214: sysStat<Mips32Defs,true,Mips32Defs::Tstat64>(context,inst); break;
  case 4215: sysFStat<Mips32Defs,Mips32Defs::Tstat64>(context,inst); break;
//  case 4216: Mips::sysCall32_root_pivot(inst,context); break;
//  case 4217: Mips::sysCall32_mincore(inst,context); break;
//  case 4218: Mips::sysCall32_madvise(inst,context); break;
  case 4219: sysGetDEnts<Mips32Defs,Mips32Defs::Tdirent64>(context,inst); break;
  case 4220: sysFCntl<Mips32Defs>(context,inst); break; // This is actually fcntl64
//#define __NR_reserved221                (__NR_Linux + 221)
  case 4222: /*Untested*/ sysGetTid<Mips32Defs>(context,inst); break;
//#define __NR_readahead                  (__NR_Linux + 223)
//#define __NR_setxattr                   (__NR_Linux + 224)
//#define __NR_lsetxattr                  (__NR_Linux + 225)
//#define __NR_fsetxattr                  (__NR_Linux + 226)
//#define __NR_getxattr                   (__NR_Linux + 227)
//#define __NR_lgetxattr                  (__NR_Linux + 228)
//#define __NR_fgetxattr                  (__NR_Linux + 229)
//#define __NR_listxattr                  (__NR_Linux + 230)
//#define __NR_llistxattr                 (__NR_Linux + 231)
//#define __NR_flistxattr                 (__NR_Linux + 232)
//#define __NR_removexattr                (__NR_Linux + 233)
//#define __NR_lremovexattr               (__NR_Linux + 234)
//#define __NR_fremovexattr               (__NR_Linux + 235)
  case 4236: sysTKill<Mips32Defs>(context,inst); break;
//#define __NR_sendfile64                 (__NR_Linux + 237)
  case 4238: sysFutex<Mips32Defs>(context,inst); break;
  case 4239: sysSchedSetAffinity<Mips32Defs>(context,inst); break;
  case 4240: sysSchedGetAffinity<Mips32Defs>(context,inst); break;
//#define __NR_io_setup                   (__NR_Linux + 241)
//#define __NR_io_destroy                 (__NR_Linux + 242)
//#define __NR_io_getevents               (__NR_Linux + 243)
//#define __NR_io_submit                  (__NR_Linux + 244)
//#define __NR_io_cancel                  (__NR_Linux + 245)
  case 4246: return sysExitGroup<Mips32Defs>(context,inst); break;
//#define __NR_lookup_dcookie             (__NR_Linux + 247)
//#define __NR_epoll_create               (__NR_Linux + 248)
//#define __NR_epoll_ctl                  (__NR_Linux + 249)
//#define __NR_epoll_wait                 (__NR_Linux + 250)
//#define __NR_remap_file_pages           (__NR_Linux + 251)
  case 4252: sysSetTidAddress<Mips32Defs>(context,inst); break;
//#define __NR_restart_syscall            (__NR_Linux + 253)
//#define __NR_fadvise64                  (__NR_Linux + 254)
//#define __NR_statfs64                   (__NR_Linux + 255)
//#define __NR_fstatfs64                  (__NR_Linux + 256)
//#define __NR_timer_create               (__NR_Linux + 257)
//#define __NR_timer_settime              (__NR_Linux + 258)
//#define __NR_timer_gettime              (__NR_Linux + 259)
//#define __NR_timer_getoverrun           (__NR_Linux + 260)
//#define __NR_timer_delete               (__NR_Linux + 261)
//#define __NR_clock_settime              (__NR_Linux + 262)
  case 4263: sysClockGetTime<Mips32Defs>(context,inst); break;
  case 4264: sysClockGetRes<Mips32Defs>(context,inst); break;
  case 4265: /*Untested*/sysClockNanoSleep<Mips32Defs>(context,inst); break;
  case 4266: sysTgKill<Mips32Defs>(context,inst); break;
//#define __NR_utimes                     (__NR_Linux + 267)
//#define __NR_mbind                      (__NR_Linux + 268)
//#define __NR_get_mempolicy              (__NR_Linux + 269)
//#define __NR_set_mempolicy              (__NR_Linux + 270)
//#define __NR_mq_open                    (__NR_Linux + 271)
//#define __NR_mq_unlink                  (__NR_Linux + 272)
//#define __NR_mq_timedsend               (__NR_Linux + 273)
//#define __NR_mq_timedreceive            (__NR_Linux + 274)
//#define __NR_mq_notify                  (__NR_Linux + 275)
//#define __NR_mq_getsetattr              (__NR_Linux + 276)
//#define __NR_vserver                    (__NR_Linux + 277)
//#define __NR_waitid                     (__NR_Linux + 278)
//#define __NR_sys_setaltroot             (__NR_Linux + 279)
//#define __NR_add_key                    (__NR_Linux + 280)
//#define __NR_request_key                (__NR_Linux + 281)
//#define __NR_keyctl                     (__NR_Linux + 282)
  case 4283: sysSetThreadArea<Mips32Defs>(context,inst); break;
//#define __NR_inotify_init               (__NR_Linux + 284)
//#define __NR_inotify_add_watch          (__NR_Linux + 285)
//#define __NR_inotify_rm_watch           (__NR_Linux + 286)
//#define __NR_migrate_pages              (__NR_Linux + 287)
//#define __NR_openat                     (__NR_Linux + 288)
//#define __NR_mkdirat                    (__NR_Linux + 289)
//#define __NR_mknodat                    (__NR_Linux + 290)
//#define __NR_fchownat                   (__NR_Linux + 291)
//#define __NR_futimesat                  (__NR_Linux + 292)
//#define __NR_fstatat64                  (__NR_Linux + 293)
//#define __NR_unlinkat                   (__NR_Linux + 294)
//#define __NR_renameat                   (__NR_Linux + 295)
//#define __NR_linkat                     (__NR_Linux + 296)
//#define __NR_symlinkat                  (__NR_Linux + 297)
//#define __NR_readlinkat                 (__NR_Linux + 298)
//#define __NR_fchmodat                   (__NR_Linux + 299)
//#define __NR_faccessat                  (__NR_Linux + 300)
//#define __NR_pselect6                   (__NR_Linux + 301)
//#define __NR_ppoll                      (__NR_Linux + 302)
//#define __NR_unshare                    (__NR_Linux + 303)
//#define __NR_splice                     (__NR_Linux + 304)
//#define __NR_sync_file_range            (__NR_Linux + 305)
//#define __NR_tee                        (__NR_Linux + 306)
//#define __NR_vmsplice                   (__NR_Linux + 307)
//#define __NR_move_pages                 (__NR_Linux + 308)
  case 4309: sysSetRobustList<Mips32Defs>(context,inst); break;
//#define __NR_get_robust_list            (__NR_Linux + 310)
//#define __NR_kexec_load                 (__NR_Linux + 311)
//#define __NR_getcpu                     (__NR_Linux + 312)
//#define __NR_epoll_pwait                (__NR_Linux + 313)
//#define __NR_ioprio_set                 (__NR_Linux + 314)
//#define __NR_ioprio_get                 (__NR_Linux + 315)
//#define __NR_utimensat                  (__NR_Linux + 316)
//#define __NR_signalfd                   (__NR_Linux + 317)
//#define __NR_timerfd                    (__NR_Linux + 318)
//#define __NR_eventfd                    (__NR_Linux + 319)
  default:
    fail("Unknown Mips32 syscall %d at 0x%08x\n",sysCallNum,context->getIAddr());
  }
#if (defined DEBUG_SYSCALLS)
  if(Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA3)){
    switch(sysCallNum){
    case 4193: // rt_sigreturn (does not return from the call)
      break;
    default:
      printf("sysCall %d returns with error %d\n",sysCallNum,
	     Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA3)
	     );
      break;
    }
  }
#endif
  return inst;
}
