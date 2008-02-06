#if !(defined LINUXSYS_H)
#define LINUXSYS_H

#include <stddef.h>
#include "Addressing.h"
#include "SignalHandling.h"

enum CpuMode{
  NoCpuMode,
  Mips32,
  Mips64,
  x86_32
};

class ThreadContext;
class InstDesc;

class LinuxSys{
 public:
  virtual InstDesc *sysCall(ThreadContext *context, InstDesc *inst) = 0;
 protected:
  class FuncArgs{
  private:
    ThreadContext *context;
    size_t pos;
  public:
    FuncArgs(ThreadContext *context);
    template<typename T>
    T get(void);
  };
  virtual void setThreadArea(ThreadContext *context, VAddr addr) const = 0;
  virtual bool stackGrowsDown(void) const = 0;
  virtual VAddr getStackPointer(ThreadContext *context) const = 0;
  virtual void setStackPointer(ThreadContext *context, VAddr addr) const = 0;
  template<typename T>
  void pushScalar(ThreadContext *context, const T &val) const;
  template<typename T>
  T popScalar(ThreadContext *context) const;
  template<typename T>
  void pushStruct(ThreadContext *context, const T &val) const;
  template<typename T>
  T popStruct(ThreadContext *context) const;
 public:
  // Creates an object of an architecture-specific LinuxSys subclass
  static LinuxSys *create(CpuMode cpuMode);
  // Destroys a Linuxsys object
  virtual ~LinuxSys(void){
  }
  virtual SignalAction handleSignal(ThreadContext *context, SigInfo *sigInfo) const = 0;
  bool handleSignals(ThreadContext *context) const;
  // Returns a 32-bit integer argument at position pos, moves pos to next arg
  virtual uint32_t getArgI32(ThreadContext *context, size_t &pos) const = 0;
  // Returns a 64-bit integer argument at position pos, moves pos to next arg
  virtual uint64_t getArgI64(ThreadContext *context, size_t &pos) const = 0;
  typedef enum{
    ErrAgain,
    ErrNoSys,
    ErrFault,
    ErrChild,
    ErrNoMem,
    ErrInval,
    ErrNoEnt,
    ErrSrch,
    ErrIntr,
    ErrBadf,
    ErrSpipe,
    ErrNoTTY,
    ErrAfNoSupport,
    ErrNotDir,
    ErrPerm,
  } ErrCode;
  static ErrCode getErrCode(int err);
  // System call return with error code
  virtual void setSysErr(ThreadContext *context, ErrCode err) const = 0;
  // System call successful return
  virtual void setSysRet(ThreadContext *context, int val) const = 0;
  // System call successful return with two values
  virtual void setSysRet(ThreadContext *context, int val1, int val2) const = 0;
  // Initialize the exception handler trampoline code
  virtual void initSystem(ThreadContext *context) const = 0;
  virtual void createStack(ThreadContext *context) const;
  template<class defs>
  void setProgArgs_(ThreadContext *context, int argc, char **argv, int envc, char **envp) const;
  virtual void setProgArgs(ThreadContext *context, int argc, char **argv, int envc, char **envp) const = 0;
  template<class defs>
  void exitRobustList_(ThreadContext *context, VAddr robust_list);
  virtual void exitRobustList(ThreadContext *context, VAddr robust_list) = 0;
  // Linux system calls. For each system call, we have "worker" functions,
  // an internal templated function, and a pure virtual external function
  // For each architecture, we need to overload the external function and make it
  // call the internal one with the correct template parameters

  // Futex functions
  template<typename val_t>
  bool futexCheck(ThreadContext *context, VAddr futex, val_t val);
  void futexWait(ThreadContext *context, VAddr futex);
  int futexWake(ThreadContext *context, VAddr futex, int nr_wake);
  int futexMove(ThreadContext *context, VAddr srcFutex, VAddr dstFutex, int nr_move);
  template<class defs>
  void sysFutex(ThreadContext *context, InstDesc *inst);
  //  template<typename addr_t, typename siz_t>
  template<class defs>
  void sysSetRobustList(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSetTidAddress(ThreadContext *context, InstDesc *inst);

  // User info functions
  template<class defs>
  void sysGetUid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetEuid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetGid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetEgid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetGroups(ThreadContext *context, InstDesc *inst);
  // Process functions
  template<class defs>
  void doWait4(ThreadContext *context, InstDesc *inst,
	       typename defs::Tpid_t cpid, typename defs::Tintptr_t status,
	       typename defs::WaitOpts options, typename defs::Tintptr_t rusage);
  template<class defs>
  void sysWait4(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysWaitpid(ThreadContext *context, InstDesc *inst);
  virtual void sysRtSigReturn(ThreadContext *context, InstDesc *inst) = 0;
  template<class defs>
  void sysRtSigAction(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysRtSigProcMask(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysRtSigSuspend(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysKill(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysTKill(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysTgKill(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetPriority(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSchedGetParam(ThreadContext *context, InstDesc *inst);  
  template<class defs>
  void sysSchedSetScheduler(ThreadContext *context, InstDesc *inst);  
  template<class defs>
  void sysSchedGetScheduler(ThreadContext *context, InstDesc *inst);  
  template<class defs>
  void sysSchedYield(ThreadContext *context, InstDesc *inst);  
  template<class defs>
  void sysSchedGetPriorityMax(ThreadContext *context, InstDesc *inst);  
  template<class defs>
  void sysSchedGetPriorityMin(ThreadContext *context, InstDesc *inst);  
  template<class defs>
  void sysSchedSetAffinity(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSchedGetAffinity(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysAlarm(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSetITimer(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysClockGetRes(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysNanoSleep(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysTime(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysTimes(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysFork(ThreadContext *context, InstDesc *inst);
  template<class defs>
  InstDesc *sysExit(ThreadContext *context, InstDesc *inst);
  template<class defs>
  InstDesc *sysExitGroup(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysClone(ThreadContext *context, InstDesc *inst);
  template<class defs>
  InstDesc *sysExecVe(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSetRLimit(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetRLimit(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetRUsage(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetTimeOfDay(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSetThreadArea(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetPid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetTid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetPPid(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysBrk(ThreadContext *context, InstDesc *inst);
  template<class defs, off_t offsmul>
  void sysMMap(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysMReMap(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysMUnMap(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysMProtect(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysOpen(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysPipe(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysDup(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysDup2(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysFCntl(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysRead(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysWrite(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysWriteV(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysLSeek(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysLLSeek(ThreadContext *context, InstDesc *inst);
  template<class defs, class Tdirent>
  void sysGetDEnts(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysIOCtl(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysPoll(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysClose(ThreadContext *context, InstDesc *inst);
  template<class defs, typename Toff_t>
  void sysTruncate(ThreadContext *context, InstDesc *inst);  
  template<class defs, typename Toff_t>
  void sysFTruncate(ThreadContext *context, InstDesc *inst);  
  template<class defs, bool link, class Tstat>
  void sysStat(ThreadContext *context, InstDesc *inst);
  template<class defs, class Tstat>
  void sysFStat(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysFStatFS(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysUnlink(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysRename(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysChdir(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysAccess(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysGetCWD(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysMkdir(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysRmdir(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysUmask(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysReadlink(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysUname(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSysCtl(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSocket(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysConnect(ThreadContext *context, InstDesc *inst);
  template<class defs>
  void sysSend(ThreadContext *context, InstDesc *inst);
};

class Mips32LinuxSys : public LinuxSys{
 public:
  virtual InstDesc *sysCall(ThreadContext *context, InstDesc *inst);
 protected:
  virtual void setThreadArea(ThreadContext *context, VAddr addr) const;
  virtual bool stackGrowsDown(void) const;
  virtual VAddr getStackPointer(ThreadContext *context) const;
  virtual void setStackPointer(ThreadContext *context, VAddr addr) const;
 public:
  virtual SignalAction handleSignal(ThreadContext *context, SigInfo *sigInfo) const;
  virtual uint32_t getArgI32(ThreadContext *context, size_t &pos) const;
  virtual uint64_t getArgI64(ThreadContext *context, size_t &pos) const;
  virtual void setSysErr(ThreadContext *context, ErrCode err) const;
  virtual void setSysRet(ThreadContext *context, int val) const;
  virtual void setSysRet(ThreadContext *context, int val1, int val2) const;
  virtual void initSystem(ThreadContext *context) const;
  virtual void setProgArgs(ThreadContext *context, int argc, char **argv, int envc, char **envp) const;
  virtual void exitRobustList(ThreadContext *context, VAddr robust_list);
  virtual void sysRtSigReturn(ThreadContext *context, InstDesc *inst);
};

#endif // !(defined LINUXSYS_H)
