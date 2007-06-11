#if !(defined MIPSSYSCALLS_H)
#define MIPSSYSCALLS_H

class ThreadContext;
class InstDesc;

namespace Mips{
  void initSystem(ThreadContext *context);
  void createStack(ThreadContext *context);
  void setProgArgs(ThreadContext *context, int argc, char **argv, int envc, char **envp);
  bool handleSignals(ThreadContext *context);
}

InstDesc *mipsSysCall(InstDesc *inst, ThreadContext *context);

#endif // !(defined SYSCALLS_H)
