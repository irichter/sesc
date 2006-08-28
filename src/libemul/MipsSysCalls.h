#if !(defined MIPSSYSCALLS_H)
#define MIPSSYSCALLS_H

class ThreadContext;
class InstDesc;

void mipsProgArgs(ThreadContext *context, int argc, char **argv, char **envp);

void mipsSysCall(InstDesc *inst, ThreadContext *context);

#endif // !(defined SYSCALLS_H)
