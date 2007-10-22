#include "LinuxSys.h"
#include "ThreadContext.h"
// Get declaration of fail()
#include "EmulInit.h"

#include <map>

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

typedef std::multimap<VAddr,ThreadContext *> ContextMultiMap;
ContextMultiMap futexContexts;

template<typename val_t>
bool LinuxSys::futexCheck(ThreadContext *context, VAddr futex, val_t val){
  bool rv=(context->readMem<val_t>(futex)==val);
  if(!rv)
    context->getSystem()->setSysErr(context,EAGAIN);
  return rv;
}

void LinuxSys::futexWait(ThreadContext *context, VAddr futex){
  futexContexts.insert(ContextMultiMap::value_type(futex,context));                
#if (defined DEBUG_SIGNALS)     
  suspSet.insert(context->getPid());
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

#include "MipsRegs.h"
#include "Mips32Defs.h"

uint32_t Mips32LinuxSys::getArgI32(ThreadContext *context, size_t &pos) const{
  I((pos%sizeof(uint32_t))==0);
  size_t argpos=pos;
  pos+=sizeof(uint32_t);
  if(argpos<16)
    return Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0+argpos/sizeof(uint32_t));
  return context->readMem<uint32_t>(Mips::getRegAny<Mips32,Mips32_VAddr,Mips::RegSP>(context,Mips::RegSP)+argpos);
}
uint64_t Mips32LinuxSys::getArgI64(ThreadContext *context, size_t &pos) const{
  I((pos%sizeof(uint32_t))==0);
  if(pos%sizeof(uint64_t))
    pos+=sizeof(uint32_t);
  I((pos%sizeof(uint64_t))==0);
  size_t argpos=pos;
  pos+=sizeof(uint64_t);
  if(argpos<16){
    uint64_t rv=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0+pos/sizeof(uint32_t));
    rv<<=32;
    return rv|Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,Mips::RegA0+pos/sizeof(uint32_t)+1);
  }
  return context->readMem<uint64_t>(Mips::getRegAny<Mips32,Mips32_VAddr,Mips::RegSP>(context,Mips::RegSP)+argpos);
}
void Mips32LinuxSys::setSysErr(ThreadContext *context, int err) const{
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegA3,(int32_t)1);
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegV0,fromNativeErrNums(err));
}

void Mips32LinuxSys::setSysRet(ThreadContext *context, int val) const{
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegA3,(int32_t)0);
  Mips::setRegAny<Mips32,int32_t,RegTypeGpr>(context,Mips::RegV0,val);    
}

#include "OSSim.h"

#define FUTEX_WAIT              0
#define FUTEX_WAKE              1
#define FUTEX_FD                2
#define FUTEX_REQUEUE           3
#define FUTEX_CMP_REQUEUE       4
#define FUTEX_WAKE_OP           5

#define FUTEX_OP_SET            0       /* *(int *)UADDR2 = OPARG; */
#define FUTEX_OP_ADD            1       /* *(int *)UADDR2 += OPARG; */
#define FUTEX_OP_OR             2       /* *(int *)UADDR2 |= OPARG; */
#define FUTEX_OP_ANDN           3       /* *(int *)UADDR2 &= ~OPARG; */
#define FUTEX_OP_XOR            4       /* *(int *)UADDR2 ^= OPARG; */

#define FUTEX_OP_OPARG_SHIFT    8       /* Use (1 << OPARG) instead of OPARG.  */

#define FUTEX_OP_CMP_EQ         0       /* if (oldval == CMPARG) wake */
#define FUTEX_OP_CMP_NE         1       /* if (oldval != CMPARG) wake */
#define FUTEX_OP_CMP_LT         2       /* if (oldval < CMPARG) wake */
#define FUTEX_OP_CMP_LE         3       /* if (oldval <= CMPARG) wake */
#define FUTEX_OP_CMP_GT         4       /* if (oldval > CMPARG) wake */
#define FUTEX_OP_CMP_GE         5       /* if (oldval >= CMPARG) wake */


template<typename addr_t, typename op_t, typename val_t, typename count_t, typename wakeop_t>
void LinuxSys::sysFutex_(ThreadContext *context, InstDesc *inst){
  FuncArgs args(context);
  addr_t futex=args.get<addr_t>();
  op_t   op=args.get<op_t>();
  switch(op){
  case FUTEX_WAIT: {
    val_t  val=args.get<val_t>();
    addr_t timeout=args.get<addr_t>();
    if(timeout)
      fail("LinuxSys::sysFutex non-zero timeout unsupported for FUTEX_WAIT");
    if(futexCheck(context,futex,val))
      futexWait(context,futex);
  } break;
  case FUTEX_WAKE: {
    count_t nr_wake=args.get<count_t>();
    addr_t timeout=args.get<addr_t>();
    if(timeout)
      fail("LinuxSys::sysFutex non-zero timeout unsupported for FUTEX_WAKE");
    setSysRet(context,futexWake(context,futex,nr_wake));
  } break;
  case FUTEX_REQUEUE: {
    count_t nr_wake=args.get<count_t>();
    count_t nr_move=args.get<count_t>();
    addr_t  futex2 =args.get<addr_t>();
    setSysRet(context,futexWake(context,futex,nr_wake)+futexMove(context,futex,futex2,nr_move));
  } break;
  case FUTEX_CMP_REQUEUE: { return setSysErr(context,ENOSYS);
    count_t nr_wake=args.get<count_t>();
    count_t nr_move=args.get<count_t>();
    addr_t  futex2 =args.get<addr_t>();
    val_t   val=args.get<val_t>();
    if(futexCheck<uint32_t>(context,futex,val))
      setSysRet(context,futexWake(context,futex,nr_wake)+futexMove(context,futex,futex2,nr_move));
  } break;
  case FUTEX_WAKE_OP: {return setSysErr(context,ENOSYS);
    count_t  nr_wake =args.get<count_t>();
    count_t  nr_wake2=args.get<count_t>();
    addr_t   futex2  =args.get<addr_t>();
    wakeop_t wakeop  =args.get<wakeop_t>();
    if(!context->canRead(futex2,sizeof(val_t))||!context->canWrite(futex2,sizeof(val_t)))
      return setSysErr(context,EFAULT);
    int atmop    = (wakeop >> 28) & 0x00f;
    int cmpop    = (wakeop >> 24) & 0x00f;
    val_t atmarg = ((wakeop <<  8) >> 20);
    val_t cmparg = ((wakeop << 20) >> 20);
    if(atmop&FUTEX_OP_OPARG_SHIFT){
      atmop^=FUTEX_OP_OPARG_SHIFT;
      atmarg=1<<atmarg;
    }
    val_t val=context->readMem<val_t>(futex2);
    bool cond;
    switch(cmpop){
    case FUTEX_OP_CMP_EQ: cond=(val==cmparg); break;
    case FUTEX_OP_CMP_NE: cond=(val!=cmparg); break;
    case FUTEX_OP_CMP_LT: cond=(val< cmparg); break;
    case FUTEX_OP_CMP_LE: cond=(val<=cmparg); break;
    case FUTEX_OP_CMP_GT: cond=(val> cmparg); break;
    case FUTEX_OP_CMP_GE: cond=(val>=cmparg); break;
    default: return setSysErr(context,ENOSYS);
    }
    switch(atmop){
    case FUTEX_OP_SET:  val = atmarg; break;
    case FUTEX_OP_ADD:  val+= atmarg; break;
    case FUTEX_OP_OR:   val|= atmarg; break;
    case FUTEX_OP_ANDN: val&=~atmarg; break;
    case FUTEX_OP_XOR:  val^= atmarg; break;
    default: return setSysErr(context,ENOSYS);
    }
    context->writeMem<val_t>(futex2,val);
    setSysRet(context,futexWake(context,futex,nr_wake)+cond?futexWake(context,futex2,nr_wake2):0);
  } break;
  default:
    fail("LinuxSys::sysFutex with unsupported op=%d\n",op);
  }
}

void Mips32LinuxSys::sysFutex(ThreadContext *context, InstDesc *inst){
  LinuxSys::sysFutex_<Mips32_VAddr,uint32_t,int32_t,uint32_t,int32_t>(context,inst);
}

template<typename status_t, typename rusage_t>
void LinuxSys::doWait4(ThreadContext *context, InstDesc *inst, int pid, VAddr status, int options, VAddr rusage){
  int cpid=pid;
  ThreadContext *ccontext;
  if(cpid>0){
    if(!context->isChildID(cpid))
      return setSysErr(context,ECHILD);
    ThreadContext *ccontext=osSim->getContext(cpid);
    if((!ccontext->isExited())&&(!ccontext->isKilled()))
      cpid=0;
  }else if(cpid==-1){
    if(!context->hasChildren())
      return setSysErr(context,ECHILD);
    cpid=context->findZombieChild();
  }else{
    fail("LinuxSys::doWait4 Only supported for pid -1 or >0\n");
  }
  if(cpid){
    ThreadContext *ccontext=osSim->getContext(cpid);
    if(status){
      status_t statVal=0xDEADBEEF;
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
    return setSysRet(context,cpid);
  }
  if(options&WNOHANG)
    return setSysRet(context,0);
  context->updIAddr(-inst->aupdate,-1);
  I(inst==context->getIDesc());
  I(inst==context->virt2inst(context->getIAddr()));
#if (defined DEBUG_SIGNALS)
  printf("Suspend %d in sysCall32_wait4(pid=%d,status=%x,options=%x)\n",context->getPid(),pid,status,options);
  printf("Also suspended:");
  for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
    printf(" %d",*suspIt);
  printf("\n");
  suspSet.insert(context->getPid());
#endif
  context->suspend(); 
}

template<typename addr_t, typename wpid_t, typename status_t, typename opts_t, typename rusage_t>
void LinuxSys::sysWait4_(ThreadContext *context, InstDesc *inst){
  FuncArgs args(context);
  wpid_t pid=args.get<wpid_t>();
  addr_t status=args.get<addr_t>();
  opts_t options=args.get<opts_t>();
  addr_t rusage= args.get<addr_t>();
  doWait4<status_t,rusage_t>(context,inst,pid,status,toNativeWaitOptions(options),rusage);
}

template<typename addr_t, typename wpid_t, typename status_t, typename opts_t>
void LinuxSys::sysWaitpid_(ThreadContext *context, InstDesc *inst){
  FuncArgs args(context);
  wpid_t pid=args.get<wpid_t>();
  addr_t status=args.get<addr_t>();
  opts_t options=args.get<opts_t>();
  doWait4<status_t,rusage>(context,inst,pid,status,toNativeWaitOptions(options),0);
}

void Mips32LinuxSys::sysWait4(ThreadContext *context, InstDesc *inst){
  LinuxSys::sysWait4_<Mips32_VAddr,int32_t,uint32_t,uint32_t,Mips32_rusage>(context,inst);
}
void Mips32LinuxSys::sysWaitpid(ThreadContext *context, InstDesc *inst){
  LinuxSys::sysWaitpid_<Mips32_VAddr,int32_t,uint32_t,uint32_t>(context,inst);
}
void LinuxSys::sysGetpid(ThreadContext *context, InstDesc *inst){
  setSysRet(context,context->gettgid());
}
void LinuxSys::sysGettid(ThreadContext *context, InstDesc *inst){
  setSysRet(context,context->gettid());
}
void LinuxSys::sysFork(ThreadContext *context, InstDesc *inst){
  ThreadContext *newContext=new ThreadContext(*context,false,false,false,false,false,SigChld,0);
  I(newContext!=0);
  // Fork returns an error only if there is no memory, which should not happen here
  // Set return values for parent and child
  setSysRet(context,newContext->gettid());
  setSysRet(newContext,0);
  // Inform SESC that this process is created
  osSim->eventSpawn(-1,newContext->gettid(),0);
}
template<typename addr_t, typename flag_t, typename tid_t>
void LinuxSys::sysClone_(ThreadContext *context, InstDesc *inst){
  FuncArgs args(context);
  flag_t flags=args.get<typename flag_t::val_t>();
  addr_t child_stack=args.get<addr_t>();
  addr_t ptid=args.get<addr_t>();
  addr_t tarea=args.get<addr_t>();
  addr_t ctid=args.get<addr_t>();
  if(flags.hasCLONE_VFORK())
    fail("sysCall32_clone with CLONE_VFORK not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags.val);
  if(flags.hasCLONE_NEWNS())
    fail("sysCall32_clone with CLONE_VFORK not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags.val);
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
  setSysRet(context,newContext->gettid());
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
    context->writeMem<tid_t>(ptid,newContext->getPid());
  if(flags.hasCLONE_CHILD_SETTID())
    newContext->writeMem<tid_t>(ctid,newContext->getPid());
  if(flags.hasCLONE_SETTLS())
    setThreadArea(newContext,tarea);
  // Inform SESC that this process is created
  osSim->eventSpawn(-1,newContext->getPid(),0);
}
class Mips32SigNum{
public:
  typedef uint32_t val_t;
  val_t val;
  Mips32SigNum(val_t val) : val(val){}
  bool isSIGCHLD(void) const { return val==0x12; }
  operator SignalID(void){
    if(isSIGCHLD())
      return SigChld;
    if(val<=SigNMax)
      return static_cast<SignalID>(val);
    fail("Mips::sigToLocal(%d) not supported\n",val);
    return SigNone;
  }
};
class Mips32CloneFlags{
public:
  typedef uint32_t val_t;
  val_t val;
  Mips32CloneFlags(val_t val) : val(val){}
  typedef Mips32SigNum signum_t;
  signum_t getCSIGNAL(void) const{          return val&0xff; }
  bool hasCLONE_VM(void) const{             return val&0x100; }
  bool hasCLONE_FS(void) const{             return val&0x200; }
  bool hasCLONE_FILES(void) const{          return val&0x400; }
  bool hasCLONE_SIGHAND(void) const{        return val&0x800; }
  bool hasCLONE_VFORK(void) const{          return val&0x4000; }
  bool hasCLONE_PARENT(void) const{         return val&0x8000; }
  bool hasCLONE_THREAD(void) const{         return val&0x10000; }
  bool hasCLONE_NEWNS(void) const{          return val&0x20000; }
  bool hasCLONE_SYSVSEM(void) const{        return val&0x40000; }
  bool hasCLONE_SETTLS(void) const{         return val&0x80000; }
  bool hasCLONE_PARENT_SETTID(void) const{  return val&0x100000; }
  bool hasCLONE_CHILD_CLEARTID(void) const{ return val&0x200000; }
  bool hasCLONE_DETACHED(void) const{       return val&0x400000; }
  bool hasCLONE_UNTRACED(void) const{       return val&0x800000; }
  bool hasCLONE_CHILD_SETTID(void) const{   return val&0x1000000; }
  bool hasCLONE_STOPPED(void) const{        return val&0x2000000; }
};
void Mips32LinuxSys::sysClone(ThreadContext *context, InstDesc *inst){
  LinuxSys::sysClone_<Mips32_VAddr,Mips32CloneFlags,uint32_t>(context,inst);
}
template<typename addr_t>
void LinuxSys::sysSetThreadArea_(ThreadContext *context, InstDesc *inst){
  FuncArgs args(context);
  addr_t addr=args.get<addr_t>();
  setThreadArea(context,addr);
  setSysRet(context,0);
}
void Mips32LinuxSys::sysSetThreadArea(ThreadContext *context, InstDesc *inst){
  LinuxSys::sysSetThreadArea_<Mips32_VAddr>(context,inst);
}
void Mips32LinuxSys::setThreadArea(ThreadContext *context, VAddr addr){
  Mips::setRegAny<Mips32,uint32_t,Mips::RegTPtr>(context,Mips::RegTPtr,addr);
}
void Mips32LinuxSys::setStackPointer(ThreadContext *context, VAddr addr){
  Mips::setRegAny<Mips32,uint32_t,Mips::RegSP>(context,Mips::RegSP,addr);
}
