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

#include <linux/futex.h>
#include "OSSim.h"
template<typename addr_t, typename op_t, typename val_t, typename count_t>
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
  case FUTEX_CMP_REQUEUE: {
    count_t nr_wake=args.get<count_t>();
    count_t nr_move=args.get<count_t>();
    addr_t  mutex=args.get<addr_t>();
    val_t   val=args.get<val_t>();
    if(futexCheck<uint32_t>(context,futex,val))
      setSysRet(context,futexWake(context,futex,nr_wake)+futexMove(context,futex,mutex,nr_move));
  } break;
  default:
    fail("LinuxSys::sysFutex with unsupported op=%d\n",op);
  }
}

void Mips32LinuxSys::sysFutex(ThreadContext *context, InstDesc *inst){
  LinuxSys::sysFutex_<Mips32_VAddr,uint32_t,uint32_t,uint32_t>(context,inst);
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
