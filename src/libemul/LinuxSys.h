#if !(defined LINUXSYS_H)
#define LINUXSYS_H

#include <stddef.h>
#include "Addressing.h"

enum CpuMode{
  NoCpuMode,
  Mips32,
  Mips64,
  x86_32
};

class ThreadContext;
class InstDesc;

class LinuxSys{
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
 public:
  // Creates an object of an architecture-specific LinuxSys subclass
  static LinuxSys *create(CpuMode cpuMode);
  // Destroys a Linuxsys object
  virtual ~LinuxSys(void){
  }
  // Returns a 32-bit integer argument at position pos, moves pos to next arg
  virtual uint32_t getArgI32(ThreadContext *context, size_t &pos) const = 0;
  // Returns a 64-bit integer argument at position pos, moves pos to next arg
  virtual uint64_t getArgI64(ThreadContext *context, size_t &pos) const = 0;
  // Set system call error return value
  virtual void setSysErr(ThreadContext *context, int err) const = 0;
  // Set system call successful return value 
  virtual void setSysRet(ThreadContext *context, int val) const = 0;

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
  template<typename addr_t, typename op_t, typename val_t, typename count_t>
  void sysFutex_(ThreadContext *context, InstDesc *inst);
  virtual void sysFutex(ThreadContext *context, InstDesc *inst) = 0;
  // Process functions
  template<typename status_t, typename rusage_t>
  void doWait4(ThreadContext *context, InstDesc *inst, int pid, VAddr status, int options, VAddr rusage);
  template<typename addr_t, typename wpid_t, typename status_t, typename opts_t, typename rusage_t>
  void sysWait4_(ThreadContext *context, InstDesc *inst);
  template<typename addr_t, typename wpid_t, typename status_t, typename opts_t>
  void sysWaitpid_(ThreadContext *context, InstDesc *inst);
  
  virtual void sysWait4(ThreadContext *context, InstDesc *inst) = 0;
  virtual void sysWaitpid(ThreadContext *context, InstDesc *inst) = 0;
};

class Mips32LinuxSys : public LinuxSys{
 public:
  virtual uint32_t getArgI32(ThreadContext *context, size_t &pos) const;
  virtual uint64_t getArgI64(ThreadContext *context, size_t &pos) const;
  virtual void setSysErr(ThreadContext *context, int err) const;
  virtual void setSysRet(ThreadContext *context, int val) const;
  virtual void sysFutex(ThreadContext *context, InstDesc *inst);
  virtual void sysWait4(ThreadContext *context, InstDesc *inst);
  virtual void sysWaitpid(ThreadContext *context, InstDesc *inst);
};

#endif // !(defined LINUXSYS_H)
