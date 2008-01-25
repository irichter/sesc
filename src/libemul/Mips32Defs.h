#ifndef _Mips32defs_h_
#define _Mips32defs_h_

// Get definitions of standard types
#include <stdint.h>
// Get definitions of O_* in OpenFlags
#include <fcntl.h>
// Get definitions of getdirentries() and struct dirent
#include <dirent.h>
// Get definitions of ioctl() and struct winsize
#include <sys/ioctl.h>

class Mips32Defs{
public:
  typedef uint32_t      /*intptr_t*/Tintptr_t;
  typedef int32_t            /*int*/Tint;
  typedef uint32_t  /*unsigned int*/Tuint;
  typedef Tint              /*long*/Tlong;
  typedef Tuint    /*unsigned long*/Tulong;
  typedef int64_t      /*long long*/Tllong;
  typedef Tulong           /*ino_t*/Tino_t;
  typedef Tulong         /*nlink_t*/Tnlink_t;
  typedef Tlong            /*off_t*/Toff_t;
  typedef Tint             /*pid_t*/Tpid_t;
  typedef Tuint            /*uid_t*/Tuid_t;
  typedef Tuint            /*gid_t*/Tgid_t;
  typedef Tuint           /*size_t*/Tsize_t;
  typedef Tint           /*ssize_t*/Tssize_t;
  typedef Tint         /*ptrdiff_t*/Tptrdiff_t;
  typedef Tlong           /*time_t*/Ttime_t;
  typedef Tlong          /*clock_t*/Tclock_t;
  typedef Tint           /*timer_t*/Ttimer_t;
  typedef Tint         /*clockid_t*/Tclockid_t;
  typedef Tllong          /*loff_t*/Tloff_t;
  class SigNum{
    public:
    Tuint val;
    SigNum(Tuint val) : val(val){}
    SigNum(SignalID sig){
      fail("Mips32Defs::SigNum::SigNum(SignalID) from SignalID\n");
    }
    bool isSIGCHLD(void) const { return val==/*SIGCHLD*/0x12; }
    operator SignalID(void){
      if(isSIGCHLD()) return SigChld;
      if(val<=SigNMax)
        return static_cast<SignalID>(val);
      fail("Mips32Defs::SigNum::operator SignalID() for %d not supported\n",val);
      return SigNone;
    }
  };
  enum{
    T_K_NSIG_WORDS=/*_K_NSIG_WORDS*/4
  };
  class Tk_sigset_t{
    public:
    uint32_t sig[T_K_NSIG_WORDS];
    Tk_sigset_t(ThreadContext *context, VAddr addr){
      for(size_t w=0;w<T_K_NSIG_WORDS;w++)
        sig[w]=context->readMem<uint32_t>(addr+w*sizeof(uint32_t));
    }
    void write(ThreadContext *context, VAddr addr) const{
      for(size_t w=0;w<T_K_NSIG_WORDS;w++)
        context->writeMem<uint32_t>(addr+w*sizeof(uint32_t),sig[w]);
    }
    Tk_sigset_t(const SignalSet &sset){
      for(size_t w=0;w<T_K_NSIG_WORDS;w++){
        sig[w]=0;
        for(size_t b=0;b<32;b++){
          SignalID lsig=SigNum(Tint(w*32+b+1));
          if((lsig!=SigNone)&&(sset.test(lsig)))
            sig[w]|=(1<<b);
        }
      }
    }
    operator SignalSet(){
      SignalSet rv;
      for(size_t w=0;w<T_K_NSIG_WORDS;w++){
        for(size_t b=0;b<32;b++){  
          if(sig[w]&(1<<b)){
            SignalID lsig=SigNum(Tint(w*32+b+1));
	    if(lsig!=SigNone)
              rv.set(lsig);
	  }
        }
      }
      return rv;
    }
  };
  class ErrCode{
    public:
    Tuint val;
    ErrCode(LinuxSys::ErrCode err){
      switch(err){
      case LinuxSys::ErrAgain:       val=/*EAGAIN*/0xb; break;
      case LinuxSys::ErrNoSys:       val=/*ENOSYS*/89; break;
      case LinuxSys::ErrFault:       val=/*EFAULT*/0xe; break;
      case LinuxSys::ErrChild:       val=/*ECHILD*/0xa; break;
      case LinuxSys::ErrNoMem:       val=/*ENOMEM*/0xc; break;
      case LinuxSys::ErrInval:       val=/*EINVAL*/0x16; break;
      case LinuxSys::ErrNoEnt:       val=/*ENOENT*/0x2; break;
      case LinuxSys::ErrIntr:        val=/*EINTR*/0x4; break;
      case LinuxSys::ErrBadf:        val=/*EINTR*/0x9; break;
      case LinuxSys::ErrSpipe:       val=/*ESPIPE*/0x1d; break;
      case LinuxSys::ErrNoTTY:       val=/*ENOTTY*/0x19; break;
      case LinuxSys::ErrAfNoSupport: val=/*EAFNOSUPPORT*/0x7c; break;
      case LinuxSys::ErrNotDir:      val=/*ENOTDIR*/0x14; break;
      case LinuxSys::ErrPerm:        val=/*EPERM*/0x1; break;
      default: fail("Mips32Def::ErrCode::ErrCode(LinuxSys::ErrCode err) with unknown ErrCode %d\n",err);
      }
    }
  };
  enum { StrArgAlign=32 };
  class Tauxv_t{
   public:
    uint32_t a_type; // Entry type
    uint32_t a_val;  // Integer value
    Tauxv_t(uint32_t a_type, uint32_t a_val)
      : a_type(a_type),
        a_val(a_val)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&a_type)-((char *)this)),a_type);
      context->writeMem(addr+(((char *)&a_val)-((char *)this)),a_val);
    }
  };
  class FutexCmd{
   public:
    Tuint val;
    FutexCmd(Tuint val) : val(val){}
    bool isFUTEX_WAIT(void) const {       return val==/*FUTEX_WAIT*/0; }
    bool isFUTEX_WAKE(void) const {       return val==/*FUTEX_WAKE*/1; }
    bool isFUTEX_FD(void) const {         return val==/*FUTEX_FD*/2;   }
    bool isFUTEX_REQUEUE(void) const{     return val==/*FUTEX_REQUEUE*/3; }
    bool isFUTEX_CMP_REQUEUE(void) const{ return val==/*FUTEX_CMP_REQUEUE*/4; }
    bool isFUTEX_WAKE_OP(void) const {    return val==/*FUTEX_WAKE_OP*/5; }
  };
  class FutexOp{
   public:
    Tuint val;
    FutexOp(Tuint val) : val(val){}
    FutexCmd getFutexCmd(void) const { return FutexCmd(val&/*FUTEX_CMD_MASK*/0xffffff7f); }
    bool hasFUTEX_PRIVATE_FLAG(void) const { return val&/*FUTEX_PRIVATE_FLAG*/0x80; }
  };
  class FutexAtmOp{
   public:
    Tuint val;
    FutexAtmOp(Tuint val) : val(val){}
    bool isFUTEX_OP_SET(void) const {  return val==/*FUTEX_OP_SET*/0; }
    bool isFUTEX_OP_ADD(void) const {  return val==/*FUTEX_OP_ADD*/1; }
    bool isFUTEX_OP_OR(void) const {   return val==/*FUTEX_OP_OR*/2; }
    bool isFUTEX_OP_ANDN(void) const { return val==/*FUTEX_OP_ANDN*/3; }
    bool isFUTEX_OP_XOR(void) const {  return val==/*FUTEX_OP_XOR*/4; }
  };
  class FutexCmpOp{
   public:
    Tuint val;
    FutexCmpOp(Tuint val) : val(val){}
    bool isFUTEX_OP_CMP_EQ(void) const { return val==/*FUTEX_OP_CMP_EQ*/0; }
    bool isFUTEX_OP_CMP_NE(void) const { return val==/*FUTEX_OP_CMP_NE*/1; }
    bool isFUTEX_OP_CMP_LT(void) const { return val==/*FUTEX_OP_CMP_LT*/2; }
    bool isFUTEX_OP_CMP_LE(void) const { return val==/*FUTEX_OP_CMP_LE*/3; }
    bool isFUTEX_OP_CMP_GT(void) const { return val==/*FUTEX_OP_CMP_GT*/4; }
    bool isFUTEX_OP_CMP_GE(void) const { return val==/*FUTEX_OP_CMP_GE*/5; }
  };
  class FutexWakeOp{
   public:
    Tuint val;
    FutexWakeOp(Tuint val) : val(val){}
    FutexAtmOp getAtmOp(void) const{ return (val>>28)&0x7; }
    FutexCmpOp getCmpOp(void) const{ return (val>>24)&0xf; }
    Tint getAtmArg(void) const{ return ((Tint(val)<<8)>>20); }
    Tint getCmpArg(void) const{ return ((Tint(val)<<20)>>20); }
    bool hasFUTEX_OP_OPARG_SHIFT(void) const { return (val>>28)&/*FUTEX_OP_OPARG_SHIFT*/8; }
  };
  class Trobust_list{
  public:
    Tintptr_t next; // struct robust_list __user *next;
    Trobust_list(ThreadContext *context, VAddr addr)
      :
      next(context->readMem<typeof(next)>(addr+(((char *)&next)-((char *)this))))
    {
    }
  };
  class Trobust_list_head{
  public:
    /*
     * The head of the list. Points back to itself if empty:
     */
    Trobust_list list;
    /*
     * This relative offset is set by user-space, it gives the kernel
     * the relative position of the futex field to examine. This way
     * we keep userspace flexible, to freely shape its data-structure,
     * without hardcoding any particular offset into the kernel:
     */
    Tlong futex_offset;
    /*
     * The death of the thread may race with userspace setting
     * up a lock's links. So to handle this race, userspace first
     * sets this field to the address of the to-be-taken lock,
     * then does the lock acquire, and then adds itself to the
     * list, and then clears this field. Hence the kernel will
     * always have full knowledge of all locks that the thread
     * _might_ have taken. We check the owner TID in any case,
     * so only truly owned locks will be handled.
     */
    Tintptr_t list_op_pending; // robust_list __user *list_op_pending;
    Trobust_list_head(ThreadContext *context, VAddr addr)
      :
      list(context,addr+(((char *)&list-((char *)this)))),
      futex_offset(context->readMem<typeof(futex_offset)>(addr+(((char *)&futex_offset)-((char *)this)))),
      list_op_pending(context->readMem<typeof(list_op_pending)>(addr+(((char *)&list_op_pending)-((char *)this))))
    {
    }
  };
  class SaFlags{
   public:
    Tuint val;
    SaFlags(Tuint val) : val(val){}
    bool hasSA_NOCLDSTOP() const{ return val&/*SA_NOCLDSTOP*/0x1; }
    bool hasSA_NOCLDWAIT() const{ return val&/*SA_NOCLDWAIT*/0x10000; }
    bool hasSA_RESETHAND() const{ return val&/*SA_RESETHAND*/0x80000000; }
    bool hasSA_ONSTACK() const{   return val&/*SA_ONSTACK*/0x8000000; }
    bool hasSA_SIGINFO() const{   return val&/*SA_SIGINFO*/0x8; }
    bool hasSA_RESTART() const{   return val&/*SA_RESTART*/0x10000000; }
    bool hasSA_NODEFER() const{   return val&/*SA_NODEFER*/0x40000000; }
    bool hasSA_INTERRUPT() const{ return val&/*SA_INTERRUPT*/0x20000000; }
    SaFlags(SaSigFlags flags){
      val=0;
      if(flags&SaNoDefer) val|=/*SA_NODEFER*/0x40000000;
      if(flags&SaSigInfo) val|=/*SA_SIGINFO*/0x8;
      if(flags&SaRestart) val|=/*SA_RESTART*/0x10000000;
    }
    operator SaSigFlags() const{
      if(hasSA_NODEFER()) return SaNoDefer;
      if(hasSA_SIGINFO()) return SaSigInfo;
      if(hasSA_RESTART()) return SaRestart;
      return SaSigFlags(0);
    }
  };
  class SigHand{
   public:
    Tintptr_t val;
    SigHand(Tintptr_t val) : val(val){}
    bool isSIG_DFL(void) const{ return val==/*SIG_DFL*/0; }
    bool isSIG_IGN(void) const{ return val==/*SIG_IGN*/1; }
    static SigHand fromVAddr(VAddr hnd){
      if(hnd==(VAddr)SigActDefault) return SigHand(Tintptr_t(/*SIG_DFL*/0));
      if(hnd==(VAddr)SigActIgnore)  return SigHand(Tintptr_t(/*SIG_IGN*/1));
      return SigHand(Tintptr_t(hnd));
    }
    VAddr toVAddr() const{
      if(isSIG_DFL()) return (VAddr)SigActDefault;
      if(isSIG_IGN()) return (VAddr)SigActIgnore;
      return (VAddr)val;
    }
  };
  class Tk_sigaction{
   public:
    SaFlags     sa_flags;
    SigHand     k_sa_handler;
    Tk_sigset_t sa_mask;
    Tintptr_t   sa_restorer;
    Tuint       sa_pad;
    Tk_sigaction(SignalDesc &sdesc)
      :
      sa_flags(sdesc.flags),
      k_sa_handler(SigHand::fromVAddr(sdesc.handler)),
      sa_mask(sdesc.mask)
    {
    }
    operator SignalDesc(){
      SignalDesc rv;
      rv.flags=sa_flags;
      rv.handler=k_sa_handler.toVAddr();
      rv.mask=sa_mask;
      return rv;
    }
    Tk_sigaction(ThreadContext *context, VAddr addr)
      :
      sa_flags(context->readMem<typeof(sa_flags.val)>(addr+(((char *)&sa_flags)-((char *)this)))),
      k_sa_handler(context->readMem<typeof(k_sa_handler.val)>(addr+(((char *)&k_sa_handler)-((char *)this)))),
      sa_mask(context,addr+(((char *)&sa_mask)-((char *)this)))
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&sa_flags)-((char *)this)),sa_flags.val);
      context->writeMem(addr+(((char *)&k_sa_handler)-((char *)this)),k_sa_handler.val);
      sa_mask.write(context,addr+(((char *)&sa_mask)-((char *)this)));
    }
  };
  class MaskHow{
  public:
    Tuint val;
    MaskHow(Tuint val) : val(val){}
    bool isSIG_BLOCK(void) const{   return val==/*SIG_BLOCK*/0x1; }
    bool isSIG_UNBLOCK(void) const{ return val==/*SIG_UNBLOCK*/0x2; }
    bool isSIG_SETMASK(void) const{ return val==/*SIG_SETMASK*/0x3; }
  };
  class WaitOpts{
  public:
    Tuint val;
    WaitOpts(Tuint val) : val(val){}
    bool hasWNOHANG(void) const{   return val&0x1; }
    bool hasWUNTRACED(void) const{ return val&0x2; }
  };
  class Ttimespec{
    /*struct timespec*/
  public:
    Ttime_t tv_sec;
    Tint    tv_nsec;
    Ttimespec(ThreadContext *context, VAddr addr)
      : tv_sec(context->readMem<Ttime_t>(addr+(((char *)&tv_sec)-((char *)this)))),
	tv_nsec(context->readMem<Tint>(addr+(((char *)&tv_nsec)-((char *)this))))
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&tv_sec)-((char *)this)),tv_sec);
      context->writeMem(addr+(((char *)&tv_nsec)-((char *)this)),tv_nsec);
    }
  };
  class Ttms{
    /* struct tms*/
  public:
    Tclock_t tms_utime;
    Tclock_t tms_stime;
    Tclock_t tms_cutime;
    Tclock_t tms_cstime;
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&tms_utime)-((char *)this)),tms_utime);
      context->writeMem(addr+(((char *)&tms_stime)-((char *)this)),tms_stime);
      context->writeMem(addr+(((char *)&tms_cutime)-((char *)this)),tms_cutime);
      context->writeMem(addr+(((char *)&tms_cstime)-((char *)this)),tms_cstime);
    }
  };
  class Ttimeval{
  public:
    Tint tv_sec;
    Tint tv_usec;
    Ttimeval(Tint sec, Tint usec)
      : tv_sec(sec),
	tv_usec(usec)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&tv_sec)-((char *)this)),tv_sec);
      context->writeMem(addr+(((char *)&tv_usec)-((char *)this)),tv_usec);
    }
  };
  class Trusage{
  public:
    Ttimeval ru_utime;
    Ttimeval ru_stime;
    Tint ru_maxrss;
    Tint ru_ixrss;
    Tint ru_idrss;
    Tint ru_isrss;
    Tint ru_minflt;
    Tint ru_majflt;
    Tint ru_nswap;
    Tint ru_inblock;
    Tint ru_oublock;
    Tint ru_msgsnd;
    Tint ru_msgrcv;
    Tint ru_nsignals;
    Tint ru_nvcsw;
    Tint ru_nivcsw;
    Trusage(Ttimeval utime, Ttimeval stime)
      : ru_utime(utime),
	ru_stime(stime),
	ru_maxrss(0), ru_ixrss(0), ru_idrss(0), ru_isrss(0),
	ru_minflt(0), ru_majflt(0), ru_nswap(0),
	ru_inblock(0), ru_oublock(0), ru_msgsnd(0), ru_msgrcv(0),
	ru_nsignals(0), ru_nvcsw(0), ru_nivcsw(0)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      ru_utime.write(context,addr+(((char *)&ru_utime)-((char *)this)));
      ru_stime.write(context,addr+(((char *)&ru_stime)-((char *)this)));
      context->writeMem(addr+(((char *)&ru_maxrss)-((char *)this)),ru_maxrss);
      context->writeMem(addr+(((char *)&ru_ixrss)-((char *)this)),ru_ixrss);
      context->writeMem(addr+(((char *)&ru_idrss)-((char *)this)),ru_idrss);
      context->writeMem(addr+(((char *)&ru_isrss)-((char *)this)),ru_isrss);
      context->writeMem(addr+(((char *)&ru_minflt)-((char *)this)),ru_minflt);
      context->writeMem(addr+(((char *)&ru_majflt)-((char *)this)),ru_majflt);
      context->writeMem(addr+(((char *)&ru_nswap)-((char *)this)),ru_nswap);
      context->writeMem(addr+(((char *)&ru_inblock)-((char *)this)),ru_inblock);
      context->writeMem(addr+(((char *)&ru_oublock)-((char *)this)),ru_oublock);
      context->writeMem(addr+(((char *)&ru_msgsnd)-((char *)this)),ru_msgsnd);
      context->writeMem(addr+(((char *)&ru_msgrcv)-((char *)this)),ru_msgrcv);
      context->writeMem(addr+(((char *)&ru_nsignals)-((char *)this)),ru_nsignals);
      context->writeMem(addr+(((char *)&ru_nvcsw)-((char *)this)),ru_nvcsw);
      context->writeMem(addr+(((char *)&ru_nivcsw)-((char *)this)),ru_nivcsw);
    }
  };
  class RUsageWho{
  public:
    Tuint val;
    RUsageWho(Tuint val) : val(val){}
    bool isRUSAGE_SELF(void) const { return val==/*RUSAGE_SELF*/0; }
    bool isRUSAGE_CHILDREN(void) const { return val==Tuint(Tint(/*RUSAGE_CHILDREN*/-1)); }
  };
  typedef Tuint Trlim_t;
  class Trlimit{
  public:
    static Trlim_t getRLIM_INFINITY(void){ return Trlim_t(/*RLIM_INFINITY*/0x7fffffff); }
    Trlim_t rlim_cur;
    Trlim_t rlim_max;
    Trlimit(void)
      : rlim_cur(getRLIM_INFINITY()), rlim_max(getRLIM_INFINITY())
    {
    }
    Trlimit(ThreadContext *context, VAddr addr)
      : rlim_cur(context->readMem<Trlim_t>(addr+(((char *)&rlim_cur)-((char *)this)))),
	rlim_max(context->readMem<Trlim_t>(addr+(((char *)&rlim_max)-((char *)this))))
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&rlim_cur)-((char *)this)),rlim_cur);
      context->writeMem(addr+(((char *)&rlim_max)-((char *)this)),rlim_max);
    }
  };
  class T__rlimit_resource{
  public:
    Tuint val;
    T__rlimit_resource(Tuint val) : val(val){}
    bool isRLIMIT_AS(void) const {      return val==/*RLIMIT_AS*/0x6; }
    bool isRLIMIT_CORE(void) const {    return val==/*RLIMIT_CORE*/0x4; }
    bool isRLIMIT_CPU(void) const {     return val==/*RLIMIT_CPU*/0x0; }
    bool isRLIMIT_DATA(void) const {    return val==/*RLIMIT_DATA*/0x2; }
    bool isRLIMIT_FSIZE(void) const {   return val==/*RLIMIT_FSIZE*/0x1; }
    bool isRLIMIT_LOCKS(void) const {   return val==/*RLIMIT_LOCKS*/0xa; }
    bool isRLIMIT_MEMLOCK(void) const { return val==/*RLIMIT_MEMLOCK*/0x9; }
    bool isRLIMIT_NOFILE(void) const {  return val==/*RLIMIT_NOFILE*/0x5; }
    bool isRLIMIT_NPROC(void) const {   return val==/*RLIMIT_NPROC*/0x8; }
    bool isRLIMIT_RSS(void) const {     return val==/*RLIMIT_RSS*/0x7; }
    bool isRLIMIT_STACK(void) const {   return val==/*RLIMIT_STACK*/0x3; }
  };
  class Ttimezone{
  public:
    Tint tz_minuteswest;
    Tint tz_dsttime;
    Ttimezone(int minwest, int dsttime)
      : tz_minuteswest(minwest),
	tz_dsttime(dsttime)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&tz_minuteswest)-((char *)this)),tz_minuteswest);
      context->writeMem(addr+(((char *)&tz_dsttime)-((char *)this)),tz_dsttime);
    }
  };
  class CloneFlags{
  public:
    Tuint val;
    CloneFlags(Tuint val) : val(val){}
    SigNum getCSIGNAL(void) const{            return val&0xff; }
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
  class MemProt{
  public:
    Tuint val;
    MemProt(Tuint val) : val(val){}
    bool hasPROT_READ(void) const{  return val&0x1; }
    bool hasPROT_WRITE(void) const{ return val&0x2; }
    bool hasPROT_EXEC(void) const{  return val&0x4; }
  };
  class MMapFlags{
  public:
    Tuint val;
    MMapFlags(Tuint val) : val(val){}
    bool hasMAP_SHARED(void) const{    return val&0x1; }
    bool hasMAP_PRIVATE(void) const{   return val&0x2; }
    bool hasMAP_FIXED(void) const{     return val&0x10; }
    bool hasMAP_ANONYMOUS(void) const{ return val&0x800; }
  };
  class MReMapFlags{
  public:
    Tuint val;
    MReMapFlags(Tuint val) : val(val){}
    bool hasMREMAP_MAYMOVE(void) const{ return val&0x1; }
  };
  class Tmode_t{
   public:
    Tuint val;
    Tmode_t(Tuint val) : val(val){}
    operator mode_t(){
      // TODO: Implement this conversion
      return val;
    }
  };
  class OpenFlags{
  public:
    Tuint val;
    OpenFlags(Tuint val) : val(val){}
    static OpenFlags fromNat(int flags){
      Tuint rv;
      switch(flags&O_ACCMODE){
      case O_RDONLY: rv=/*O_RDONLY*/0x0; break;
      case O_WRONLY: rv=/*O_WRONLY*/0x1; break;
      case O_RDWR: rv=/*O_RDWR*/0x2; break;
      default: fail("Mips32::OpenFlags::fromInt() unknown O_ACCMODE in %0x08\n",flags);
      }
      if(flags&O_APPEND) rv|=/*O_APPEND*/0x8;
      if(flags&O_SYNC) rv|=/*O_SYNC*/0x10;
      if(flags&O_NONBLOCK) rv|=/*O_NONBLOCK*/0x80;
      if(flags&O_CREAT) rv|=/*O_CREAT*/0x100;
      if(flags&O_TRUNC) rv |=/*O_TRUNC*/0x200;
      if(flags&O_EXCL) rv |=/*O_EXCL*/0x400;
      if(flags&O_NOCTTY) rv |=/*O_NOCTTY*/0x800;
      if(flags&O_ASYNC) rv |=/*O_ASYNC*/0x1000;
      if(flags&O_LARGEFILE) rv |=/*O_LARGEFILE*/0x2000;
      if(flags&O_DIRECT) rv |=/*O_DIRECT*/0x8000;
      if(flags&O_DIRECTORY) rv |=/*O_DIRECTORY*/0x10000;
      if(flags&O_NOFOLLOW) rv |=/*O_NOFOLLOW*/0x20000;
      return OpenFlags(rv);
    }
    int toNat(void) const{
      int rv;
      switch(val&/*O_ACCMODE*/0x3){
      case /*O_RDONLY*/0x0: rv=O_RDONLY; break;
      case /*O_WRONLY*/0x1: rv=O_WRONLY; break;
      case /*O_RDWR*/0x2: rv=O_RDWR; break;
      default: fail("Mips32::OpenFlags::toNat(): unknown O_ACCMODE in %0x08\n",val);
      }
      if(val&/*O_APPEND*/0x8) rv|=O_APPEND;
      if(val&/*O_SYNC*/0x10) rv|=O_SYNC;
      if(val&/*O_NONBLOCK*/0x80) rv|=O_NONBLOCK;
      if(val&/*O_CREAT*/0x100) rv|=O_CREAT;
      if(val&/*O_TRUNC*/0x200) rv|=O_TRUNC;
      if(val&/*O_EXCL*/0x400) rv|=O_EXCL;
      if(val&/*O_NOCTTY*/0x800) rv|=O_NOCTTY;
      if(val&/*O_ASYNC*/0x1000) rv|=O_ASYNC;
      if(val&/*O_LARGEFILE*/0x2000) rv|=O_LARGEFILE;
      if(val&/*O_DIRECT*/0x8000) rv|=O_DIRECT;
      if(val&/*O_DIRECTORY*/0x10000) rv|=O_DIRECTORY;
      if(val&/*O_NOFOLLOW*/0x20000) rv|=O_NOFOLLOW;
      return rv;
    }
  };
  class FcntlCmd{
  public:
    Tuint val;
    FcntlCmd(Tuint val) : val(val){}
    bool isF_DUPFD(void) const{ return val==/*F_DUPFD*/0x0; }
    bool isF_GETFD(void) const{ return val==/*F_GETFD*/0x1; }
    bool isF_SETFD(void) const{ return val==/*F_SETFD*/0x2; }
    bool isF_GETFL(void) const{ return val==/*F_GETFL*/0x3; }
    bool isF_SETFL(void) const{ return val==/*F_SETFL*/0x4; }
  };
  class FcntlFlags{
  public:
    Tuint val;
    FcntlFlags(Tuint val) : val(val){}
    static FcntlFlags fromNat(int flags){
      Tuint rv=0;
      if(flags&FD_CLOEXEC) rv|=/*FD_CLOEXEC;*/0x1;
      return FcntlFlags(rv);
    }
    int toNat(void) const{
      int rv=0;
      if(val&/*FD_CLOEXEC;*/0x1) rv|=FD_CLOEXEC;
      return rv;
    }
  };
  class Tiovec{
    /*struct iovec*/
  public:
    Tintptr_t    iov_base;
    Tsize_t iov_len;
    Tiovec(ThreadContext *context, VAddr addr)
      : iov_base(context->readMem<typeof(iov_base)>(addr+(((char *)&iov_base)-((char *)this)))),
	iov_len(context->readMem<typeof(iov_len)>(addr+(((char *)&iov_len)-((char *)this))))
    {
    }
  };
  class Whence{
  public:
    Tuint val;
    Whence(Tuint val) : val(val){}
    bool isSEEK_SET(void) const{ return val==/*SEEK_SET*/0x0; }
    bool isSEEK_CUR(void) const{ return val==/*SEEK_CUR*/0x1; }
    bool isSEEK_END(void) const{ return val==/*SEEK_END*/0x2; }
    int toNat(void) const{
      if(isSEEK_SET()) return SEEK_SET;
      if(isSEEK_CUR()) return SEEK_CUR;
      if(isSEEK_END()) return SEEK_END;
      fail("Mips32::Whence::toNat(): conversion to native with unknown value %d\n",val);
    }
  };
  typedef int64_t Tino64_t;
  typedef int64_t Toff64_t;
  class Tdirent64{
    /*struct dirent64*/
  public:
    Tino64_t d_ino;
    Toff64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[256];
    Tdirent64(struct dirent64 &src)
      : d_ino(src.d_ino),
	d_off(src.d_off),
	d_type(src.d_type)
    {
      size_t nmLen=strlen(src.d_name)+1;
      I(nmLen<=sizeof(d_name));
      size_t recLen=(d_name+nmLen)-((char *)this);
      // Record length is double-word aligned
      size_t recPad=(recLen+7)-((recLen+7)%8)-recLen;
      strncpy(d_name,src.d_name,nmLen+recPad);
      d_reclen=recLen+recPad;
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&d_ino)-((char *)this)),d_ino);
      context->writeMem(addr+(((char *)&d_off)-((char *)this)),d_off);
      context->writeMem(addr+(((char *)&d_reclen)-((char *)this)),d_reclen);
      context->writeMem(addr+(((char *)&d_type)-((char *)this)),d_type);
      context->writeMemFromBuf(addr+(((char *)&d_name)-((char *)this)),
			       d_reclen-(((char *)&d_name)-((char *)this)),d_name);
    }
  };
  class IoctlReq{
  public:
    Tuint val;
    IoctlReq(Tuint val) : val(val){}
    bool isTCGETS(void) const {     return val==/*TCGETS*/0x540d; }
    bool isTCSETS(void) const {     return val==/*TCSETS*/0x540e; }
    bool isTCSETSW(void) const {    return val==/*TCSETSW*/0x540f; }
    bool isTCSETSF(void) const {    return val==/*TCSETSF*/0x5410; }
    bool isTCGETA(void) const {     return val==/*TCGETA*/0x5401; }
    bool isTCSETA(void) const {     return val==/*TCSETA*/0x5402; }
    bool isTCSETAW(void) const {    return val==/*TCSETAW*/0x5403; }
    bool isTCSETAF(void) const {    return val==/*TCSETAF*/0x5404; }
    bool isTIOCGWINSZ(void) const { return val==/*TIOCGWINSZ*/0x40087468; }
  };
  class Twinsize{
    /* struct winsize*/
  public:
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
    Twinsize(const struct winsize &src)
      : ws_row(src.ws_row),
	ws_col(src.ws_col),
	ws_xpixel(src.ws_xpixel),
	ws_ypixel(src.ws_ypixel)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&ws_row)-((char *)this)),ws_row);
      context->writeMem(addr+(((char *)&ws_col)-((char *)this)),ws_col);
      context->writeMem(addr+(((char *)&ws_xpixel)-((char *)this)),ws_xpixel);
      context->writeMem(addr+(((char *)&ws_ypixel)-((char *)this)),ws_ypixel);
    }
  };
  class PollFlags{
  public:
    Tuint val;
    PollFlags(Tuint val) : val(val){}
    bool hasPOLLIN(void) const{   return val&/*POLLIN*/0x1; }
    bool hasPOLLPRI(void) const{  return val&/*POLLPRI*/0x2; }
    bool hasPOLLOUT(void) const{  return val&/*POLLOUT*/0x4; }
    bool hasPOLLERR(void) const{  return val&/*POLLERR*/0x8; }
    bool hasPOLLHUP(void) const{  return val&/*POLLHUP*/0x10; }
    bool hasPOLLNVAL(void) const{ return val&/*POLLNVAL*/0x20; }
    void setPOLLIN(void){ val|=/*POLLIN*/0x1; }
    void setPOLLNVAL(void){ val|=/*POLLNVAL*/0x20; }
  };
  class Tpollfd{
    /*struct pollfd*/
  public:
    Tint fd;
    PollFlags events;
    PollFlags revents;
    Tpollfd(ThreadContext *context, VAddr addr)
      : fd(context->readMem<Tint>(addr+(((char *)&fd)-((char *)this)))),
	events(context->readMem<Tuint>(addr+(((char *)&events)-((char *)this)))),
	revents(context->readMem<Tuint>(addr+(((char *)&revents)-((char *)this))))
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&fd)-((char *)this)),fd);
      context->writeMem(addr+(((char *)&events)-((char *)this)),events.val);
      context->writeMem(addr+(((char *)&revents)-((char *)this)),revents.val);
    }
  };
  class Tstat{
    /*struct stat*/
  public:
    uint32_t st_dev;
    uint32_t st_pad1[3];
    int32_t  st_ino;
    int32_t  st_mode;
    uint32_t st_nlink;
    int32_t  st_uid;
    int32_t  st_gid;
    uint32_t st_rdev;
    uint32_t st_pad2[2];
    int32_t  st_size;
    uint32_t st_pad3;
    int32_t  st_atim;
    int32_t  reserved0;
    int32_t  st_mtim;
    int32_t  reserved1;
    int32_t  st_ctim;
    int32_t  reserved2;
    int32_t  st_blksize;
    int32_t  st_blocks;
    uint32_t st_pad5[14];
    Tstat(struct stat &st)
      : 
      st_dev(st.st_dev),
      st_ino(st.st_ino),
      st_mode(st.st_mode),
      st_nlink(st.st_nlink),
      st_uid(st.st_uid),
      st_gid(st.st_gid),
      st_rdev(st.st_rdev),
      st_size(st.st_size),
      st_atim(st.st_atime),
      st_mtim(st.st_mtime),
      st_ctim(st.st_ctime),
      st_blksize(st.st_blksize),
      st_blocks(st.st_blocks)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&st_dev)-((char *)this)),st_dev);
      context->writeMem(addr+(((char *)&st_ino)-((char *)this)),st_ino);
      context->writeMem(addr+(((char *)&st_mode)-((char *)this)),st_mode);
      context->writeMem(addr+(((char *)&st_nlink)-((char *)this)),st_nlink);
      context->writeMem(addr+(((char *)&st_uid)-((char *)this)),st_uid);
      context->writeMem(addr+(((char *)&st_gid)-((char *)this)),st_gid);
      context->writeMem(addr+(((char *)&st_rdev)-((char *)this)),st_rdev);
      context->writeMem(addr+(((char *)&st_size)-((char *)this)),st_size);
      context->writeMem(addr+(((char *)&st_atim)-((char *)this)),st_atim);
      context->writeMem(addr+(((char *)&st_mtim)-((char *)this)),st_mtim);
      context->writeMem(addr+(((char *)&st_ctim)-((char *)this)),st_ctim);
      context->writeMem(addr+(((char *)&st_blksize)-((char *)this)),st_blksize);
      context->writeMem(addr+(((char *)&st_blocks)-((char *)this)),st_blocks);
    }
  };
  class Tstat64{
    /*struct stat64*/
  public:
    uint32_t st_dev;
    uint32_t st_pad1[3];
    int64_t  st_ino;
    int32_t  st_mode;
    uint32_t st_nlink;
    int32_t  st_uid;
    int32_t  st_gid;
    uint32_t st_rdev;
    uint32_t st_pad2[3];
    int64_t  st_size;
    int32_t  st_atim;
    uint32_t reserved0;
    int32_t  st_mtim;
    uint32_t reserved1;
    int32_t  st_ctim;
    uint32_t reserved2;
    int32_t  st_blksize;
    uint32_t st_pad3;
    int64_t  st_blocks;
    uint32_t st_pad4[14];
    Tstat64(struct stat &st)
      : 
      st_dev(st.st_dev),
      st_ino(st.st_ino),
      st_mode(st.st_mode),
      st_nlink(st.st_nlink),
      st_uid(st.st_uid),
      st_gid(st.st_gid),
      st_rdev(st.st_rdev),
      st_size(st.st_size),
      st_atim(st.st_atime),
      st_mtim(st.st_mtime),
      st_ctim(st.st_ctime),
      st_blksize(st.st_blksize),
      st_blocks(st.st_blocks)
    {
    }
    void write(ThreadContext *context, VAddr addr) const{
      context->writeMem(addr+(((char *)&st_dev)-((char *)this)),st_dev);
      context->writeMem(addr+(((char *)&st_ino)-((char *)this)),st_ino);
      context->writeMem(addr+(((char *)&st_mode)-((char *)this)),st_mode);
      context->writeMem(addr+(((char *)&st_nlink)-((char *)this)),st_nlink);
      context->writeMem(addr+(((char *)&st_uid)-((char *)this)),st_uid);
      context->writeMem(addr+(((char *)&st_gid)-((char *)this)),st_gid);
      context->writeMem(addr+(((char *)&st_rdev)-((char *)this)),st_rdev);
      context->writeMem(addr+(((char *)&st_size)-((char *)this)),st_size);
      context->writeMem(addr+(((char *)&st_atim)-((char *)this)),st_atim);
      context->writeMem(addr+(((char *)&st_mtim)-((char *)this)),st_mtim);
      context->writeMem(addr+(((char *)&st_ctim)-((char *)this)),st_ctim);
      context->writeMem(addr+(((char *)&st_blksize)-((char *)this)),st_blksize);
      context->writeMem(addr+(((char *)&st_blocks)-((char *)this)),st_blocks);
    }
  };
  class AMode{
  public:
    Tuint val;
    AMode(Tuint val) : val(val){}
    int toNat(void) const{
      // TODO: Implement this conversion
      return val;
    }
  };
  class Tutsname{
    /*struct utsname*/
  public:
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
  };
  class SysCtl{
  public:
    Tuint val;
    SysCtl(Tuint val) : val(val){}
    bool isCTL_KERN(void) const { return val==0x1; }
  };
  class SysCtlKern{
  public:
    Tuint val;
    SysCtlKern(Tuint val) : val(val){}
    bool isKERN_VERSION(void) const { return val==0x4; }
  };
  class T__sysctl_args{
    /*struct __sysctl_args*/
  public:
    Tintptr_t name;
    Tint nlen;
    Tintptr_t oldval;
    Tintptr_t oldlenp;
    Tintptr_t newval;
    Tsize_t newlen;
    Tuint __unused[4];
    T__sysctl_args(ThreadContext *context, VAddr addr)
      : name(context->readMem<typeof(name)>(addr+(((char *)&name)-((char *)this)))),
	nlen(context->readMem<typeof(nlen)>(addr+(((char *)&nlen)-((char *)this)))),
	oldval(context->readMem<typeof(oldval)>(addr+(((char *)&oldval)-((char *)this)))),
	oldlenp(context->readMem<typeof(oldlenp)>(addr+(((char *)&oldlenp)-((char *)this)))),
	newval(context->readMem<typeof(newval)>(addr+(((char *)&newval)-((char *)this)))),
	newlen(context->readMem<typeof(newlen)>(addr+(((char *)&newlen)-((char *)this))))
    {
    }
  };
};

#endif // _Mips32defs_h_
