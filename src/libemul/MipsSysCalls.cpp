#include <sys/stat.h>
#include <fcntl.h>
#include "MipsSysCalls.h"
#include "ThreadContext.h"
#include "MipsRegs.h"
#include "ElfObject.h"
#include "FileSys.h"
// To get definition of fail()
#include "EmulInit.h"
// To get ChkWriter and ChkReader
#include "Checkpoint.h"

#include "Mips32Defs.h"
// For process control calls (spawn/wait/etc)
#include "OSSim.h"

//#define DEBUG_SYSCALLS
//#define DEBUG_FILES
//#define DEBUG_MEMORY

namespace Mips {

#if (defined DEBUG_SIGNALS)
  typedef std::set<Pid_t> PidSet;
  PidSet suspSet;
#endif

  void sysCall32_syscall(InstDesc *inst, ThreadContext *context);
  void sysCall32_exit(InstDesc *inst, ThreadContext *context);
  void sysCall32_fork(InstDesc *inst, ThreadContext *context);
  void sysCall32_open(InstDesc *inst, ThreadContext *context);
  void sysCall32_creat(InstDesc *inst, ThreadContext *context);
  void sysCall32_close(InstDesc *inst, ThreadContext *context);
  void sysCall32_read(InstDesc *inst, ThreadContext *context);
  void sysCall32_write(InstDesc *inst, ThreadContext *context);
  void sysCall32_readv(InstDesc *inst, ThreadContext *context);
  void sysCall32_writev(InstDesc *inst, ThreadContext *context);
  void sysCall32_pread(InstDesc *inst, ThreadContext *context);
  void sysCall32_pwrite(InstDesc *inst, ThreadContext *context);
  void sysCall32_lseek(InstDesc *inst, ThreadContext *context);
  void sysCall32__llseek(InstDesc *inst, ThreadContext *context);

  void sysCall32_stat(InstDesc *inst, ThreadContext *context);
  void sysCall32_lstat(InstDesc *inst, ThreadContext *context);
  void sysCall32_fstat(InstDesc *inst, ThreadContext *context);
  void sysCall32_stat64(InstDesc *inst, ThreadContext *context);
  void sysCall32_lstat64(InstDesc *inst, ThreadContext *context);
  void sysCall32_fstat64(InstDesc *inst, ThreadContext *context);

  // These take a file descriptor
  void sysCall32_ftruncate(InstDesc *inst, ThreadContext *context);
  void sysCall32_ftruncate64(InstDesc *inst, ThreadContext *context);
  // These take a file name
  void sysCall32_truncate(InstDesc *inst, ThreadContext *context);
  void sysCall32_truncate64(InstDesc *inst, ThreadContext *context);

  void sysCall32_waitpid(InstDesc *inst, ThreadContext *context);
  void sysCall32_link(InstDesc *inst, ThreadContext *context);
  void sysCall32_unlink(InstDesc *inst, ThreadContext *context);
  InstDesc *sysCall32_execve(InstDesc *inst, ThreadContext *context);
  void sysCall32_chdir(InstDesc *inst, ThreadContext *context);
  void sysCall32_time(InstDesc *inst, ThreadContext *context);
  void sysCall32_mknod(InstDesc *inst, ThreadContext *context);
  void sysCall32_chmod(InstDesc *inst, ThreadContext *context);
  void sysCall32_lchown(InstDesc *inst, ThreadContext *context);
  void sysCall32_break(InstDesc *inst, ThreadContext *context);
  void sysCall32_oldstat(InstDesc *inst, ThreadContext *context);
  void sysCall32_getpid(InstDesc *inst, ThreadContext *context);
  void sysCall32_mount(InstDesc *inst, ThreadContext *context);
  void sysCall32_umount(InstDesc *inst, ThreadContext *context);
  void sysCall32_setuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_stime(InstDesc *inst, ThreadContext *context);
  void sysCall32_ptrace(InstDesc *inst, ThreadContext *context);
  void sysCall32_alarm(InstDesc *inst, ThreadContext *context);
  void sysCall32_oldfstat(InstDesc *inst, ThreadContext *context);
  void sysCall32_pause(InstDesc *inst, ThreadContext *context);
  void sysCall32_utime(InstDesc *inst, ThreadContext *context);
  void sysCall32_stty(InstDesc *inst, ThreadContext *context);
  void sysCall32_gtty(InstDesc *inst, ThreadContext *context);
  void sysCall32_access(InstDesc *inst, ThreadContext *context);
  void sysCall32_nice(InstDesc *inst, ThreadContext *context);
  void sysCall32_ftime(InstDesc *inst, ThreadContext *context);
  void sysCall32_sync(InstDesc *inst, ThreadContext *context);
  void sysCall32_kill(InstDesc *inst, ThreadContext *context);
  void sysCall32_rename(InstDesc *inst, ThreadContext *context);
  void sysCall32_mkdir(InstDesc *inst, ThreadContext *context);
  void sysCall32_rmdir(InstDesc *inst, ThreadContext *context);
  void sysCall32_dup(InstDesc *inst, ThreadContext *context);
  void sysCall32_pipe(InstDesc *inst, ThreadContext *context);
  void sysCall32_times(InstDesc *inst, ThreadContext *context);
  void sysCall32_prof(InstDesc *inst, ThreadContext *context);
  void sysCall32_brk(InstDesc *inst, ThreadContext *context);
  void sysCall32_setgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_signal(InstDesc *inst, ThreadContext *context);
  void sysCall32_geteuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getegid(InstDesc *inst, ThreadContext *context);
  void sysCall32_acct(InstDesc *inst, ThreadContext *context);
  void sysCall32_umount2(InstDesc *inst, ThreadContext *context);
  void sysCall32_lock(InstDesc *inst, ThreadContext *context);
  void sysCall32_ioctl(InstDesc *inst, ThreadContext *context);
  void sysCall32_fcntl(InstDesc *inst, ThreadContext *context);
  void sysCall32_mpx(InstDesc *inst, ThreadContext *context);
  void sysCall32_setpgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_ulimit(InstDesc *inst, ThreadContext *context);
  void sysCall32_unused59(InstDesc *inst, ThreadContext *context);
  void sysCall32_umask(InstDesc *inst, ThreadContext *context);
  void sysCall32_chroot(InstDesc *inst, ThreadContext *context);
  void sysCall32_ustat(InstDesc *inst, ThreadContext *context);
  void sysCall32_dup2(InstDesc *inst, ThreadContext *context);
  void sysCall32_getppid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getpgrp(InstDesc *inst, ThreadContext *context);
  void sysCall32_setsid(InstDesc *inst, ThreadContext *context);
  void sysCall32_sigaction(InstDesc *inst, ThreadContext *context);
  void sysCall32_sgetmask(InstDesc *inst, ThreadContext *context);
  void sysCall32_ssetmask(InstDesc *inst, ThreadContext *context);
  void sysCall32_setreuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_setregid(InstDesc *inst, ThreadContext *context);
  void sysCall32_sigsuspend(InstDesc *inst, ThreadContext *context);
  void sysCall32_sigpending(InstDesc *inst, ThreadContext *context);
  void sysCall32_sethostname(InstDesc *inst, ThreadContext *context);
  void sysCall32_setrlimit(InstDesc *inst, ThreadContext *context);
  void sysCall32_getrlimit(InstDesc *inst, ThreadContext *context);
  void sysCall32_getrusage(InstDesc *inst, ThreadContext *context);
  void sysCall32_gettimeofday(InstDesc *inst, ThreadContext *context);
  void sysCall32_settimeofday(InstDesc *inst, ThreadContext *context);
  void sysCall32_getgroups(InstDesc *inst, ThreadContext *context);
  void sysCall32_setgroups(InstDesc *inst, ThreadContext *context);
  void sysCall32_reserved82(InstDesc *inst, ThreadContext *context);
  void sysCall32_symlink(InstDesc *inst, ThreadContext *context);
  void sysCall32_oldlstat(InstDesc *inst, ThreadContext *context);
  void sysCall32_readlink(InstDesc *inst, ThreadContext *context);
  void sysCall32_uselib(InstDesc *inst, ThreadContext *context);
  void sysCall32_swapon(InstDesc *inst, ThreadContext *context);
  void sysCall32_reboot(InstDesc *inst, ThreadContext *context);
  void sysCall32_readdir(InstDesc *inst, ThreadContext *context);
  void sysCall32_mmap(InstDesc *inst, ThreadContext *context);
  void sysCall32_munmap(InstDesc *inst, ThreadContext *context);
  void sysCall32_fchmod(InstDesc *inst, ThreadContext *context);
  void sysCall32_fchown(InstDesc *inst, ThreadContext *context);
  void sysCall32_getpriority(InstDesc *inst, ThreadContext *context);
  void sysCall32_setpriority(InstDesc *inst, ThreadContext *context);
  void sysCall32_profil(InstDesc *inst, ThreadContext *context);
  void sysCall32_statfs(InstDesc *inst, ThreadContext *context);
  void sysCall32_fstatfs(InstDesc *inst, ThreadContext *context);
  void sysCall32_ioperm(InstDesc *inst, ThreadContext *context);
  void sysCall32_socketcall(InstDesc *inst, ThreadContext *context);
  void sysCall32_syslog(InstDesc *inst, ThreadContext *context);
  void sysCall32_setitimer(InstDesc *inst, ThreadContext *context);
  void sysCall32_getitimer(InstDesc *inst, ThreadContext *context);
  void sysCall32_unused109(InstDesc *inst, ThreadContext *context);
  void sysCall32_iopl(InstDesc *inst, ThreadContext *context);
  void sysCall32_vhangup(InstDesc *inst, ThreadContext *context);
  void sysCall32_idle(InstDesc *inst, ThreadContext *context);
  void sysCall32_vm86(InstDesc *inst, ThreadContext *context);
  void sysCall32_wait4(InstDesc *inst, ThreadContext *context);
  void sysCall32_swapoff(InstDesc *inst, ThreadContext *context);
  void sysCall32_sysinfo(InstDesc *inst, ThreadContext *context);
  void sysCall32_ipc(InstDesc *inst, ThreadContext *context);
  void sysCall32_fsync(InstDesc *inst, ThreadContext *context);
  void sysCall32_sigreturn(InstDesc *inst, ThreadContext *context);
  void sysCall32_clone(InstDesc *inst, ThreadContext *context);
  void sysCall32_setdomainname(InstDesc *inst, ThreadContext *context);
  void sysCall32_uname(InstDesc *inst, ThreadContext *context);
  void sysCall32_modify_ldt(InstDesc *inst, ThreadContext *context);
  void sysCall32_adjtimex(InstDesc *inst, ThreadContext *context);
  void sysCall32_mprotect(InstDesc *inst, ThreadContext *context);
  void sysCall32_sigprocmask(InstDesc *inst, ThreadContext *context);
  void sysCall32_create_module(InstDesc *inst, ThreadContext *context);
  void sysCall32_init_module(InstDesc *inst, ThreadContext *context);
  void sysCall32_delete_module(InstDesc *inst, ThreadContext *context);
  void sysCall32_get_kernel_syms(InstDesc *inst, ThreadContext *context);
  void sysCall32_quotactl(InstDesc *inst, ThreadContext *context);
  void sysCall32_getpgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_fchdir(InstDesc *inst, ThreadContext *context);
  void sysCall32_bdflush(InstDesc *inst, ThreadContext *context);
  void sysCall32_sysfs(InstDesc *inst, ThreadContext *context);
  void sysCall32_personality(InstDesc *inst, ThreadContext *context);
  void sysCall32_afs_syscall(InstDesc *inst, ThreadContext *context);
  void sysCall32_setfsuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_setfsgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getdents(InstDesc *inst, ThreadContext *context);
  void sysCall32__newselect(InstDesc *inst, ThreadContext *context);
  void sysCall32_flock(InstDesc *inst, ThreadContext *context);
  void sysCall32_msync(InstDesc *inst, ThreadContext *context);
  void sysCall32_cacheflush(InstDesc *inst, ThreadContext *context);
  void sysCall32_cachectl(InstDesc *inst, ThreadContext *context);
  void sysCall32_sysmips(InstDesc *inst, ThreadContext *context);
  void sysCall32_unused150(InstDesc *inst, ThreadContext *context);
  void sysCall32_getsid(InstDesc *inst, ThreadContext *context);
  void sysCall32_fdatasync(InstDesc *inst, ThreadContext *context);
  void sysCall32__sysctl(InstDesc *inst, ThreadContext *context);
  void sysCall32_mlock(InstDesc *inst, ThreadContext *context);
  void sysCall32_munlock(InstDesc *inst, ThreadContext *context);
  void sysCall32_mlockall(InstDesc *inst, ThreadContext *context);
  void sysCall32_munlockall(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_setparam(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_getparam(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_setscheduler(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_getscheduler(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_yield(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_get_priority_max(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_get_priority_min(InstDesc *inst, ThreadContext *context);
  void sysCall32_sched_rr_get_interval(InstDesc *inst, ThreadContext *context);
  void sysCall32_nanosleep(InstDesc *inst, ThreadContext *context);
  void sysCall32_mremap(InstDesc *inst, ThreadContext *context);
  void sysCall32_accept(InstDesc *inst, ThreadContext *context);
  void sysCall32_bind(InstDesc *inst, ThreadContext *context);
  void sysCall32_connect(InstDesc *inst, ThreadContext *context);
  void sysCall32_getpeername(InstDesc *inst, ThreadContext *context);
  void sysCall32_getsockname(InstDesc *inst, ThreadContext *context);
  void sysCall32_getsockopt(InstDesc *inst, ThreadContext *context);
  void sysCall32_listen(InstDesc *inst, ThreadContext *context);
  void sysCall32_recv(InstDesc *inst, ThreadContext *context);
  void sysCall32_recvfrom(InstDesc *inst, ThreadContext *context);
  void sysCall32_recvmsg(InstDesc *inst, ThreadContext *context);
  void sysCall32_send(InstDesc *inst, ThreadContext *context);
  void sysCall32_sendmsg(InstDesc *inst, ThreadContext *context);
  void sysCall32_sendto(InstDesc *inst, ThreadContext *context);
  void sysCall32_setsockopt(InstDesc *inst, ThreadContext *context);
  void sysCall32_shutdown(InstDesc *inst, ThreadContext *context);
  void sysCall32_socket(InstDesc *inst, ThreadContext *context);
  void sysCall32_socketpair(InstDesc *inst, ThreadContext *context);
  void sysCall32_setresuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getresuid(InstDesc *inst, ThreadContext *context);
  void sysCall32_query_module(InstDesc *inst, ThreadContext *context);
  void sysCall32_poll(InstDesc *inst, ThreadContext *context);
  void sysCall32_nfsservctl(InstDesc *inst, ThreadContext *context);
  void sysCall32_setresgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_getresgid(InstDesc *inst, ThreadContext *context);
  void sysCall32_prctl(InstDesc *inst, ThreadContext *context);

  void sysCall32_rt_sigreturn(InstDesc *inst, ThreadContext *context);
  void sysCall32_rt_sigaction(InstDesc *inst, ThreadContext *context);
  void sysCall32_rt_sigprocmask(InstDesc *inst, ThreadContext *context);
  void sysCall32_rt_sigpending(InstDesc *inst, ThreadContext *context);
  void sysCall32_rt_sigtimedwait(InstDesc *inst, ThreadContext *context);
  void sysCall32_rt_sigqueueinfo(InstDesc *inst, ThreadContext *context);
  void sysCall32_rt_sigsuspend(InstDesc *inst, ThreadContext *context);

  void sysCall32_chown(InstDesc *inst, ThreadContext *context);
  void sysCall32_getcwd(InstDesc *inst, ThreadContext *context);
  void sysCall32_capget(InstDesc *inst, ThreadContext *context);
  void sysCall32_capset(InstDesc *inst, ThreadContext *context);
  void sysCall32_sigaltstack(InstDesc *inst, ThreadContext *context);
  void sysCall32_sendfile(InstDesc *inst, ThreadContext *context);
  void sysCall32_getpmsg(InstDesc *inst, ThreadContext *context);
  void sysCall32_putpmsg(InstDesc *inst, ThreadContext *context);
  void sysCall32_mmap2(InstDesc *inst, ThreadContext *context);
  void sysCall32_root_pivot(InstDesc *inst, ThreadContext *context);
  void sysCall32_mincore(InstDesc *inst, ThreadContext *context);
  void sysCall32_madvise(InstDesc *inst, ThreadContext *context);
  void sysCall32_getdents64(InstDesc *inst, ThreadContext *context);
  void sysCall32_fcntl64(InstDesc *inst, ThreadContext *context);

} // namespace Mips

InstDesc *mipsSysCall(InstDesc *inst, ThreadContext *context){
  context->updIAddr(inst->aupdate,1);
  uint32_t sysCallNum=0;
  switch(context->getMode()){
  case NoCpuMode:
    fail("mipsSysCall: Should never execute in NoCpuMode\n");
    break;
  case Mips32:
    sysCallNum=Mips::getReg<Mips32,uint32_t>(context,static_cast<RegName>(Mips::RegV0));
    break;
  case Mips64:
    fail("mipsSysCall: Mips64 system calls not implemented yet\n");
    break;
  }
  switch(sysCallNum){
  case 4000: Mips::sysCall32_syscall(inst,context); break;
  case 4001: Mips::sysCall32_exit(inst,context); break;
  case 4002: Mips::sysCall32_fork(inst,context); break;
  case 4003: Mips::sysCall32_read(inst,context); break;
  case 4004: Mips::sysCall32_write(inst,context); break;
  case 4005: Mips::sysCall32_open(inst,context); break;
  case 4006: Mips::sysCall32_close(inst,context); break;
  case 4007: Mips::sysCall32_waitpid(inst,context); break;
  case 4008: Mips::sysCall32_creat(inst,context); break;
  case 4009: Mips::sysCall32_link(inst,context); break;
  case 4010: Mips::sysCall32_unlink(inst,context); break;
  case 4011: return Mips::sysCall32_execve(inst,context);
  case 4012: Mips::sysCall32_chdir(inst,context); break;
  case 4013: Mips::sysCall32_time(inst,context); break;
  case 4014: Mips::sysCall32_mknod(inst,context); break;
  case 4015: Mips::sysCall32_chmod(inst,context); break;
  case 4016: Mips::sysCall32_lchown(inst,context); break;
  case 4017: Mips::sysCall32_break(inst,context); break;
  case 4018: Mips::sysCall32_oldstat(inst,context); break;
  case 4019: Mips::sysCall32_lseek(inst,context); break;
  case 4020: Mips::sysCall32_getpid(inst,context); break;
  case 4021: Mips::sysCall32_mount(inst,context); break;
  case 4022: Mips::sysCall32_umount(inst,context); break;
  case 4023: Mips::sysCall32_setuid(inst,context); break;
  case 4024: Mips::sysCall32_getuid(inst,context); break;
  case 4025: Mips::sysCall32_stime(inst,context); break;
  case 4026: Mips::sysCall32_ptrace(inst,context); break;
  case 4027: Mips::sysCall32_alarm(inst,context); break;
  case 4028: Mips::sysCall32_oldfstat(inst,context); break;
  case 4029: Mips::sysCall32_pause(inst,context); break;
  case 4030: Mips::sysCall32_utime(inst,context); break;
  case 4031: Mips::sysCall32_stty(inst,context); break;
  case 4032: Mips::sysCall32_gtty(inst,context); break;
  case 4033: Mips::sysCall32_access(inst,context); break;
  case 4034: Mips::sysCall32_nice(inst,context); break;
  case 4035: Mips::sysCall32_ftime(inst,context); break;
  case 4036: Mips::sysCall32_sync(inst,context); break;
  case 4037: Mips::sysCall32_kill(inst,context); break;
  case 4038: Mips::sysCall32_rename(inst,context); break;
  case 4039: Mips::sysCall32_mkdir(inst,context); break;
  case 4040: Mips::sysCall32_rmdir(inst,context); break;
  case 4041: Mips::sysCall32_dup(inst,context); break;
  case 4042: Mips::sysCall32_pipe(inst,context); break;
  case 4043: Mips::sysCall32_times(inst,context); break;
  case 4044: Mips::sysCall32_prof(inst,context); break;
  case 4045: Mips::sysCall32_brk(inst,context); break;
  case 4046: Mips::sysCall32_setgid(inst,context); break;
  case 4047: Mips::sysCall32_getgid(inst,context); break;
  case 4048: Mips::sysCall32_signal(inst,context); break;
  case 4049: Mips::sysCall32_geteuid(inst,context); break;
  case 4050: Mips::sysCall32_getegid(inst,context); break;
  case 4051: Mips::sysCall32_acct(inst,context); break;
  case 4052: Mips::sysCall32_umount2(inst,context); break;
  case 4053: Mips::sysCall32_lock(inst,context); break;
  case 4054: Mips::sysCall32_ioctl(inst,context); break;
  case 4055: Mips::sysCall32_fcntl(inst,context); break;
  case 4056: Mips::sysCall32_mpx(inst,context); break;
  case 4057: Mips::sysCall32_setpgid(inst,context); break;
  case 4058: Mips::sysCall32_ulimit(inst,context); break;
  case 4059: Mips::sysCall32_unused59(inst,context); break;
  case 4060: Mips::sysCall32_umask(inst,context); break;
  case 4061: Mips::sysCall32_chroot(inst,context); break;
  case 4062: Mips::sysCall32_ustat(inst,context); break;
  case 4063: Mips::sysCall32_dup2(inst,context); break;
  case 4064: Mips::sysCall32_getppid(inst,context); break;
  case 4065: Mips::sysCall32_getpgrp(inst,context); break;
  case 4066: Mips::sysCall32_setsid(inst,context); break;
  case 4067: Mips::sysCall32_sigaction(inst,context); break;
  case 4068: Mips::sysCall32_sgetmask(inst,context); break;
  case 4069: Mips::sysCall32_ssetmask(inst,context); break;
  case 4070: Mips::sysCall32_setreuid(inst,context); break;
  case 4071: Mips::sysCall32_setregid(inst,context); break;
  case 4072: Mips::sysCall32_sigsuspend(inst,context); break;
  case 4073: Mips::sysCall32_sigpending(inst,context); break;
  case 4074: Mips::sysCall32_sethostname(inst,context); break;
  case 4075: Mips::sysCall32_setrlimit(inst,context); break;
  case 4076: Mips::sysCall32_getrlimit(inst,context); break;
  case 4077: Mips::sysCall32_getrusage(inst,context); break;
  case 4078: Mips::sysCall32_gettimeofday(inst,context); break;
  case 4079: Mips::sysCall32_settimeofday(inst,context); break;
  case 4080: Mips::sysCall32_getgroups(inst,context); break;
  case 4081: Mips::sysCall32_setgroups(inst,context); break;
  case 4082: Mips::sysCall32_reserved82(inst,context); break;
  case 4083: Mips::sysCall32_symlink(inst,context); break;
  case 4084: Mips::sysCall32_oldlstat(inst,context); break;
  case 4085: Mips::sysCall32_readlink(inst,context); break;
  case 4086: Mips::sysCall32_uselib(inst,context); break;
  case 4087: Mips::sysCall32_swapon(inst,context); break;
  case 4088: Mips::sysCall32_reboot(inst,context); break;
  case 4089: Mips::sysCall32_readdir(inst,context); break;
  case 4090: Mips::sysCall32_mmap(inst,context); break;
  case 4091: Mips::sysCall32_munmap(inst,context); break;
  case 4092: Mips::sysCall32_truncate(inst,context); break;
  case 4093: Mips::sysCall32_ftruncate(inst,context); break;
  case 4094: Mips::sysCall32_fchmod(inst,context); break;
  case 4095: Mips::sysCall32_fchown(inst,context); break;
  case 4096: Mips::sysCall32_getpriority(inst,context); break;
  case 4097: Mips::sysCall32_setpriority(inst,context); break;
  case 4098: Mips::sysCall32_profil(inst,context); break;
  case 4099: Mips::sysCall32_statfs(inst,context); break;
  case 4100: Mips::sysCall32_fstatfs(inst,context); break;
  case 4101: Mips::sysCall32_ioperm(inst,context); break;
  case 4102: Mips::sysCall32_socketcall(inst,context); break;
  case 4103: Mips::sysCall32_syslog(inst,context); break;
  case 4104: Mips::sysCall32_setitimer(inst,context); break;
  case 4105: Mips::sysCall32_getitimer(inst,context); break;
  case 4106: Mips::sysCall32_stat(inst,context); break;
  case 4107: Mips::sysCall32_lstat(inst,context); break;
  case 4108: Mips::sysCall32_fstat(inst,context); break;
  case 4109: Mips::sysCall32_unused109(inst,context); break;
  case 4110: Mips::sysCall32_iopl(inst,context); break;
  case 4111: Mips::sysCall32_vhangup(inst,context); break;
  case 4112: Mips::sysCall32_idle(inst,context); break;
  case 4113: Mips::sysCall32_vm86(inst,context); break;
  case 4114: Mips::sysCall32_wait4(inst,context); break;
  case 4115: Mips::sysCall32_swapoff(inst,context); break;
  case 4116: Mips::sysCall32_sysinfo(inst,context); break;
  case 4117: Mips::sysCall32_ipc(inst,context); break;
  case 4118: Mips::sysCall32_fsync(inst,context); break;
  case 4119: Mips::sysCall32_sigreturn(inst,context); break;
  case 4120: Mips::sysCall32_clone(inst,context); break;
  case 4121: Mips::sysCall32_setdomainname(inst,context); break;
  case 4122: Mips::sysCall32_uname(inst,context); break;
  case 4123: Mips::sysCall32_modify_ldt(inst,context); break;
  case 4124: Mips::sysCall32_adjtimex(inst,context); break;
  case 4125: Mips::sysCall32_mprotect(inst,context); break;
  case 4126: Mips::sysCall32_sigprocmask(inst,context); break;
  case 4127: Mips::sysCall32_create_module(inst,context); break;
  case 4128: Mips::sysCall32_init_module(inst,context); break;
  case 4129: Mips::sysCall32_delete_module(inst,context); break;
  case 4130: Mips::sysCall32_get_kernel_syms(inst,context); break;
  case 4131: Mips::sysCall32_quotactl(inst,context); break;
  case 4132: Mips::sysCall32_getpgid(inst,context); break;
  case 4133: Mips::sysCall32_fchdir(inst,context); break;
  case 4134: Mips::sysCall32_bdflush(inst,context); break;
  case 4135: Mips::sysCall32_sysfs(inst,context); break;
  case 4136: Mips::sysCall32_personality(inst,context); break;
  case 4137: Mips::sysCall32_afs_syscall(inst,context); break;
  case 4138: Mips::sysCall32_setfsuid(inst,context); break;
  case 4139: Mips::sysCall32_setfsgid(inst,context); break;
  case 4140: Mips::sysCall32__llseek(inst,context); break;
  case 4141: Mips::sysCall32_getdents(inst,context); break;
  case 4142: Mips::sysCall32__newselect(inst,context); break;
  case 4143: Mips::sysCall32_flock(inst,context); break;
  case 4144: Mips::sysCall32_msync(inst,context); break;
  case 4145: Mips::sysCall32_readv(inst,context); break;
  case 4146: Mips::sysCall32_writev(inst,context); break;
  case 4147: Mips::sysCall32_cacheflush(inst,context); break;
  case 4148: Mips::sysCall32_cachectl(inst,context); break;
  case 4149: Mips::sysCall32_sysmips(inst,context); break;
  case 4150: Mips::sysCall32_unused150(inst,context); break;
  case 4151: Mips::sysCall32_getsid(inst,context); break;
  case 4152: Mips::sysCall32_fdatasync(inst,context); break;
  case 4153: Mips::sysCall32__sysctl(inst,context); break;
  case 4154: Mips::sysCall32_mlock(inst,context); break;
  case 4155: Mips::sysCall32_munlock(inst,context); break;
  case 4156: Mips::sysCall32_mlockall(inst,context); break;
  case 4157: Mips::sysCall32_munlockall(inst,context); break;
  case 4158: Mips::sysCall32_sched_setparam(inst,context); break;
  case 4159: Mips::sysCall32_sched_getparam(inst,context); break;
  case 4160: Mips::sysCall32_sched_setscheduler(inst,context); break;
  case 4161: Mips::sysCall32_sched_getscheduler(inst,context); break;
  case 4162: Mips::sysCall32_sched_yield(inst,context); break;
  case 4163: Mips::sysCall32_sched_get_priority_max(inst,context); break;
  case 4164: Mips::sysCall32_sched_get_priority_min(inst,context); break;
  case 4165: Mips::sysCall32_sched_rr_get_interval(inst,context); break;
  case 4166: Mips::sysCall32_nanosleep(inst,context); break;
  case 4167: Mips::sysCall32_mremap(inst,context); break;
  case 4168: Mips::sysCall32_accept(inst,context); break;
  case 4169: Mips::sysCall32_bind(inst,context); break;
  case 4170: Mips::sysCall32_connect(inst,context); break;
  case 4171: Mips::sysCall32_getpeername(inst,context); break;
  case 4172: Mips::sysCall32_getsockname(inst,context); break;
  case 4173: Mips::sysCall32_getsockopt(inst,context); break;
  case 4174: Mips::sysCall32_listen(inst,context); break;
  case 4175: Mips::sysCall32_recv(inst,context); break;
  case 4176: Mips::sysCall32_recvfrom(inst,context); break;
  case 4177: Mips::sysCall32_recvmsg(inst,context); break;
  case 4178: Mips::sysCall32_send(inst,context); break;
  case 4179: Mips::sysCall32_sendmsg(inst,context); break;
  case 4180: Mips::sysCall32_sendto(inst,context); break;
  case 4181: Mips::sysCall32_setsockopt(inst,context); break;
  case 4182: Mips::sysCall32_shutdown(inst,context); break;
  case 4183: Mips::sysCall32_socket(inst,context); break;
  case 4184: Mips::sysCall32_socketpair(inst,context); break;
  case 4185: Mips::sysCall32_setresuid(inst,context); break;
  case 4186: Mips::sysCall32_getresuid(inst,context); break;
  case 4187: Mips::sysCall32_query_module(inst,context); break;
  case 4188: Mips::sysCall32_poll(inst,context); break;
  case 4189: Mips::sysCall32_nfsservctl(inst,context); break;
  case 4190: Mips::sysCall32_setresgid(inst,context); break;
  case 4191: Mips::sysCall32_getresgid(inst,context); break;
  case 4192: Mips::sysCall32_prctl(inst,context); break;
  case 4193: Mips::sysCall32_rt_sigreturn(inst,context); break;
  case 4194: Mips::sysCall32_rt_sigaction(inst,context); break;
  case 4195: Mips::sysCall32_rt_sigprocmask(inst,context); break;
  case 4196: Mips::sysCall32_rt_sigpending(inst,context); break;
  case 4197: Mips::sysCall32_rt_sigtimedwait(inst,context); break;
  case 4198: Mips::sysCall32_rt_sigqueueinfo(inst,context); break;
  case 4199: Mips::sysCall32_rt_sigsuspend(inst,context); break;
  case 4200: Mips::sysCall32_pread(inst,context); break;
  case 4201: Mips::sysCall32_pwrite(inst,context); break;
  case 4202: Mips::sysCall32_chown(inst,context); break;
  case 4203: Mips::sysCall32_getcwd(inst,context); break;
  case 4204: Mips::sysCall32_capget(inst,context); break;
  case 4205: Mips::sysCall32_capset(inst,context); break;
  case 4206: Mips::sysCall32_sigaltstack(inst,context); break;
  case 4207: Mips::sysCall32_sendfile(inst,context); break;
  case 4208: Mips::sysCall32_getpmsg(inst,context); break;
  case 4209: Mips::sysCall32_putpmsg(inst,context); break;
  case 4210: Mips::sysCall32_mmap2(inst,context); break;
  case 4211: Mips::sysCall32_truncate64(inst,context); break;
  case 4212: Mips::sysCall32_ftruncate64(inst,context); break;
  case 4213: Mips::sysCall32_stat64(inst,context); break;
  case 4214: Mips::sysCall32_lstat64(inst,context); break;
  case 4215: Mips::sysCall32_fstat64(inst,context); break;
  case 4216: Mips::sysCall32_root_pivot(inst,context); break;
  case 4217: Mips::sysCall32_mincore(inst,context); break;
  case 4218: Mips::sysCall32_madvise(inst,context); break;
  case 4219: Mips::sysCall32_getdents64(inst,context); break;
  case 4220: Mips::sysCall32_fcntl64(inst,context); break;
  default:
    fail("Unknown MIPS syscall %d at 0x%08x\n",sysCallNum,context->getIAddr());
  }
#if (defined DEBUG_SYSCALLS)
  if(Mips::getReg<uint32_t>(context,static_cast<RegName>(Mips::RegA3))){
    switch(sysCallNum){
    case 4193: // rt_sigreturn (does not return from the call)
      break;
    default:
      printf("sysCall %d returns with error %d\n",sysCallNum,
	     Mips::getReg<uint32_t>(context,static_cast<RegName>(Mips::RegA3))
	     );
      break;
    }
  }
#endif
  return inst;
}

// This class helps extract function parameters for substitutions (e.g. in subs.cpp)
// First, prepare for parameter extraction by constructing an instance of MintFuncArgs.
// The constructor takes as a parameter the ThreadContext in which a function has just
// been called (i.e. right after the jal and the delay slot have been emulated
// Then get the parameters in order from first to last, using getW, getL, or getA
// Mips32FuncArgs automatically takes care of getting the needed parameter from the register
// or from the stack, according to the MIPS III caling convention. In particular, it correctly
// implements extraction of 64-bit parameters, allowing lstat64 and similar functions to work
// The entire process of parameter extraction does not change the thread context in any way
class Mips32FuncArgs{
 private:
  ThreadContext *myContext;
  int   curPos;
 public:
  Mips32FuncArgs(ThreadContext *context)
    : myContext(context), curPos(0)
    {
      I(myContext->getMode()==Mips32);
    }
  int32_t getW(void);
  int64_t getL(void);
  VAddr   getA(void){ return (VAddr)getW(); }
};

int32_t Mips32FuncArgs::getW(void){
  int32_t retVal=0xDEADBEEF;
  I((curPos%sizeof(int32_t))==0);
  if(curPos<16){
    retVal=Mips::getReg<Mips32,int32_t>(myContext,static_cast<RegName>(Mips::RegA0+curPos/sizeof(retVal)));
  }else{
    myContext->readMem(Mips::getReg<Mips32,uint32_t>(myContext,static_cast<RegName>(Mips::RegSP))+curPos,retVal);
  }
  curPos+=sizeof(retVal);
  return retVal;
}
int64_t Mips32FuncArgs::getL(void){
  int64_t retVal=0xDEADBEEF;
  I((curPos%sizeof(int32_t))==0);
  // Align current position                  
  if(curPos%sizeof(retVal)!=0)
    curPos+=sizeof(int32_t);
  I(curPos%sizeof(retVal)==0);
  if(curPos<16){
    retVal=Mips::getReg<Mips32,int32_t>(myContext,static_cast<RegName>(Mips::RegA0+curPos/sizeof(int32_t)));
    retVal=(retVal<<32);
    I(!(retVal&0xffffffff));
    retVal|=Mips::getReg<Mips32,int32_t>(myContext,static_cast<RegName>(Mips::RegA0+curPos/sizeof(int32_t)+1));
  }else{
    myContext->readMem(Mips::getReg<Mips32,uint32_t>(myContext,static_cast<RegName>(Mips::RegSP))+curPos,retVal);
  }
  curPos+=sizeof(retVal);
  return retVal;
}

namespace Mips {

  static VAddr  sysCodeAddr=AddrSpacPageSize;
  static size_t sysCodeSize=6*sizeof(uint32_t);

  void initSystem(ThreadContext *context){
    AddressSpace *addrSpace=context->getAddressSpace();
    addrSpace->newSegment(sysCodeAddr,sysCodeSize,false,true,false,false,false);
    addrSpace->addFuncName("sysCode",sysCodeAddr);
    addrSpace->addFuncName("invalid",sysCodeAddr+sysCodeSize);
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
    setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSys),0);    
  }

  void createStack(ThreadContext *context){
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
    switch(context->getMode()){
    case Mips32:
      setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP),stackStart+stackSize);
      break;
    default:
      I(0);
    }
  }

  void setProgArgs(ThreadContext *context, int argc, char **argv, int envc, char **envp){
    I(context->getMode()==Mips32);
    uint32_t regSP=getReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP));
    // We will push arg and env string pointer arrays later, with nulls at end
    VAddr envVAddrs[envc+1];
    I(sizeof(envVAddrs)==(envc+1)*sizeof(VAddr));
    VAddr argVAddrs[argc+1];
    I(sizeof(argVAddrs)==(argc+1)*sizeof(VAddr));
    // Put the env strings on the stack and initialize the envVAddrs array
    for(int envIdx=envc-1;envIdx>=0;envIdx--){
      size_t strSize=strlen(envp[envIdx])+1;
      regSP-=alignUp(strSize,32);
      context->writeMemFromBuf(regSP,strSize,envp[envIdx]);
      envVAddrs[envIdx]=regSP;
      cvtEndianBig(envVAddrs[envIdx]);
    }
    envVAddrs[envc]=0;
    cvtEndianBig(envVAddrs[envc]);
    // Put the arg string on the stack
    for(int argIdx=argc-1;argIdx>=0;argIdx--){
      size_t strSize=strlen(argv[argIdx])+1;
      regSP-=alignUp(strSize,32);
      context->writeMemFromBuf(regSP,strSize,argv[argIdx]);
      argVAddrs[argIdx]=regSP;
      cvtEndianBig(argVAddrs[argIdx]);
    }
    argVAddrs[argc]=0;
    cvtEndianBig(argVAddrs[argc]);
    // Put the envp array (with NULL at the end) on the stack
    regSP-=sizeof(envVAddrs);
    context->writeMemFromBuf(regSP,sizeof(envVAddrs),envVAddrs);
    // Put the argv array (with NULL at the end) on the stack
    regSP-=sizeof(argVAddrs);
    context->writeMemFromBuf(regSP,sizeof(argVAddrs),argVAddrs);
    // Put the argc on the stack
    int32_t argcVal=argc;
    regSP-=sizeof(argcVal);
    context->writeMem(regSP,argcVal);
    setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP),regSP);
  }
  void sysCall32SetErr(ThreadContext *context, int32_t err){
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegA3),(int32_t)1);
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegV0),err);
  }
  void sysCall32SetRet(ThreadContext *context,int32_t ret){
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegA3),(int32_t)0);
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegV0),ret);
  }
  void sysCall32SetRet2(ThreadContext *context,int32_t ret1, int32_t ret2){
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegA3),(int32_t)0);
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegV0),ret1);
    setReg<Mips32,int32_t>(context,static_cast<RegName>(RegV1),ret2);
  }

  int sigFromLocal(SignalID sig){
    switch(sig){
    case SigChld: return Mips32_SIGCHLD;
    default:
      if(sig<=SigNMax)
	return static_cast<int>(sig);
      fail("Mips::sigFromLocal(%d) not supported\n",sig);
    }
    return 0;
  }

  SignalID sigToLocal(int sig){
    switch(sig){
    case Mips32_SIGCHLD: return SigChld;
    default:
      if(sig<=SigNMax)
	return static_cast<SignalID>(sig);
      fail("Mips::sigToLocal(%d) not supported\n",sig);
    }
    return SigNone;
  }

  void sigMaskToLocal(const Mips32_k_sigset_t &smask, SignalSet &lmask){
    lmask.reset();
    for(size_t w=0;w<Mips32__K_NSIG_WORDS;w++){
      uint32_t word=smask.sig[w];
      uint32_t mask=1;
      for(size_t b=0;b<32;b++){
	SignalID lsig=sigToLocal(w*32+b+1);
	if(lsig!=SigNone)
	  lmask.set(lsig,word&mask);
	mask<<=1;
      }
    }
  }

  void sigMaskFromLocal(Mips32_k_sigset_t &smask, const SignalSet &lmask){
    for(size_t w=0;w<Mips32__K_NSIG_WORDS;w++){
      uint32_t word=0;
      uint32_t mask=1;
      for(size_t b=0;b<32;b++){
	SignalID lsig=sigToLocal(w*32+b+1);
	if((lsig!=SigNone)&&(lmask.test(lsig)))
	  word|=mask;
	mask<<=1;
      }
      smask.sig[w]=word;
    }
  }

  void pushSignalMask(ThreadContext *context, SignalSet &mask){
    Mips32_k_sigset_t appmask;
    sigMaskFromLocal(appmask,mask);
    VAddr sp=Mips::getReg<Mips32,uint32_t>(context,static_cast<RegName>(Mips::RegSP))-sizeof(appmask);
    context->writeMem(sp,appmask);
    setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP),sp);
    setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSys),1);
  }

  void popSignalMask(ThreadContext *context){
    uint32_t pop=getReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSys));
    if(pop){
      setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSys),0);    
      Mips32_k_sigset_t appmask;
      VAddr sp=getReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP));
      context->readMem(sp,appmask);
      sp+=sizeof(appmask);
      setReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP),sp);    
      SignalSet simmask;
      sigMaskToLocal(appmask,simmask);
      context->setSignalMask(simmask);
    }
  }

  bool handleSignal(ThreadContext *context, SigInfo *sigInfo){
    // Pop signal mask if it's been saved
    popSignalMask(context);
    SignalDesc &sigDesc=(*(context->getSignalTable()))[sigInfo->signo];
#if (defined DEBUG_SIGNALS)
    printf("Mips::handleSignal for pid=%d, signal=%d, action=0x%08x\n",context->getPid(),sigInfo->signo,sigDesc.handler);
#endif
    switch((SignalAction)(sigDesc.handler)){
    case SigActCore:
    case SigActTerm:
      osSim->eventExit(context->getPid(),-1);
      return true;
    case SigActIgnore: 
      return false;
    case SigActStop:
#if (defined DEBUG_SIGNALS)
      suspSet.insert(context->getPid());
#endif
      context->suspend();
      return true;
    default:
      break;
    }
    // Save registers and PC
    I(context->getIDesc()==context->virt2inst(context->getIAddr()));
    VAddr pc=context->getIAddr();
    VAddr sp=Mips::getReg<Mips32,uint32_t>(context,static_cast<RegName>(Mips::RegSP));
    context->execCall(pc,Mips::sysCodeAddr,sp);
    for(size_t i=0;i<32;i++){
      uint32_t wrVal;
      switch(i){
      case 0:
	wrVal=pc;
	break;
      default:
	wrVal=Mips::getReg<Mips32,uint32_t>(context,static_cast<RegName>(i));
	break;
      }
      sp-=4;
      context->writeMem<uint32_t>(sp,wrVal);
    }
    // Save the current signal mask and use sigaction's signal mask
    Mips32_k_sigset_t oldMask;
    sigMaskFromLocal(oldMask,context->getSignalMask());
    sp-=sizeof(oldMask);
    context->writeMem(sp,oldMask);
    context->setSignalMask(sigDesc.mask);
    // Set registers and PC for execution of the handler
    Mips::setReg<Mips32,uint32_t>(context,static_cast<RegName>(Mips::RegSP),sp);
    Mips::setReg<Mips32,uint32_t>(context,static_cast<RegName>(Mips::RegA0),sigFromLocal(sigInfo->signo));
    Mips::setReg<Mips32,uint32_t>(context,static_cast<RegName>(Mips::RegT9),sigDesc.handler);
    context->setIAddr(Mips::sysCodeAddr);
    return true;
  }
  bool handleSignals(ThreadContext *context){
    while(context->hasReadySignal()){
      SigInfo *sigInfo=context->nextReadySignal();
      if(handleSignal(context,sigInfo))
	return true;
    }
    return false;
  }

  // Simulated wall clock time (for time() syscall)
  static time_t wallClock=(time_t)-1;

  // Process control system calls

  // TODO: This is a hack to get applications working.
  // We need a real user/system time estimator
  
  static uint64_t myUsrUsecs=0;
  static uint64_t mySysUsecs=0;
  bool operator==(const Mips32_rlimit &rlim1, const Mips32_rlimit &rlim2){
    return (rlim1.rlim_cur==rlim2.rlim_cur)&&(rlim1.rlim_max==rlim2.rlim_max);
  }
  void sysCall32_getrlimit(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int resource=myArgs.getW();
    VAddr rlim=myArgs.getA();
    Mips32_rlimit res;
    if(!context->canWrite(rlim,sizeof(res)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    switch(resource){
    case Mips32_RLIMIT_STACK:
      res.rlim_cur=res.rlim_max=context->getStackSize();
      break;
    default:
      fail("sysCall32_getrlimit called for resource %d at 0x%08x\n",resource,context->getIAddr());
    }
    context->writeMem(rlim,res);
    return sysCall32SetRet(context,0);
  }
  void sysCall32_setrlimit(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int resource=myArgs.getW();
    VAddr rlim=myArgs.getA();
    Mips32_rlimit res;
    if(!context->canRead(rlim,sizeof(res)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    context->readMem(rlim,res);
    switch(resource){
    case Mips32_RLIMIT_STACK:
      if((res.rlim_cur>context->getStackSize())||(res.rlim_max>context->getStackSize()))
	fail("sysCall32_setrlimit tries to change stack size (not supported yet)\n");
      break;
    default:
      fail("sysCall32_setrlimit called for resource %d at 0x%08x\n",resource,context->getIAddr());
    }
    return sysCall32SetRet(context,0);
  }

  void sysCall32_syscall(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_syscall: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_exit(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int status=myArgs.getW();
    if(context->exit(status))
      return;

//     static bool didThis=false;
//     if(!didThis){
//       std::ofstream os("dump.txt",std::ios::out);
//       ChkWriter chkWriter(os.rdbuf());
//       size_t pidCnt=0;
//       for(int i=0;i<context->getPidUb();i++)
// 	if(ThreadContext::getContext(i))
// 	  pidCnt++;
//       chkWriter << "Pids " << pidCnt << endl;
//       for(int i=0;i<context->getPidUb();i++){
// 	ThreadContext *ct=ThreadContext::getContext(i);
// 	if(ct)
//  	  ct->save(chkWriter);
//       }
//       os.close();
//       std::ifstream is("dump.txt",std::ios::in);
//       ChkReader chkReader(is.rdbuf());
//       size_t _pids;
//       chkReader >> "Pids " >> _pids >> endl;
//       while(_pids){
// 	new ThreadContext(chkReader);
// 	_pids--;
//       }
//       is.close();
//       didThis=true;
//     }

#if (defined DEBUG_SIGNALS)
    printf("Suspend %d in sysCall32_exit\n",context->getPid());
    context->dumpCallStack();
    printf("Also suspended:");
    for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
      printf(" %d",*suspIt);
    printf("\n");
    suspSet.insert(context->getPid());
#endif
    // Deliver the exit signal to the parent process if needed
    SignalID exitSig=context->getExitSig();
    // If no signal to be delivered, just wait to be reaped
    if(exitSig!=SigNone){
      SigInfo *sigInfo=new SigInfo(exitSig,SigCodeChldExit);
      sigInfo->pid=context->getPid();
      sigInfo->data=status;
      ThreadContext::getContext(context->getParentID())->signal(sigInfo);
    }
  }
  void sysCall32_fork(InstDesc *inst, ThreadContext *context){
    //    ThreadContext *newContext=context->createChild(false,false,false,SigChld);
    ThreadContext *newContext=new ThreadContext(*context,false,false,false,SigChld);
    I(newContext!=0);
    // Fork returns an error only if there is no memory, which should not happen here
    // Set return values for parent and child
    sysCall32SetRet(context,newContext->getPid());
    sysCall32SetRet(newContext,0);
    // Inform SESC that this process is created
    osSim->eventSpawn(-1,newContext->getPid(),0);
  }

  void sysCall32_wait4(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int      pid     = myArgs.getW();
    VAddr    status  = myArgs.getA();
    uint32_t options = myArgs.getW();
    VAddr    rusage  = myArgs.getA();
    I((pid==-1)||(pid>0));
    int cpid=pid;
    ThreadContext *ccontext;
    if(cpid>0){
      if(!context->isChildID(cpid))
	return sysCall32SetErr(context,Mips32_ECHILD);
      ThreadContext *ccontext=osSim->getContext(cpid);
      if((!ccontext->isExited())&&(!ccontext->isKilled()))
	cpid=0;
    }else if(cpid==-1){
      if(!context->hasChildren())
	return sysCall32SetErr(context,Mips32_ECHILD);
      cpid=context->findZombieChild();
    }else{
      fail("sysCall32_wait4 Only supported for pid -1 or >0\n");
    }
    if(cpid){
      ThreadContext *ccontext=osSim->getContext(cpid);
      if(status){
	uint32_t statVal=0xDEADBEEF;
	if(ccontext->isExited()){
	  statVal=(ccontext->getExitCode()<<8);
	}else{
	  I(ccontext->isKilled());
	  fail("sysCall32_wait4 for killed child not supported\n");
	}
	context->writeMem(status,statVal);
      }
      if(rusage)
	fail("sysCall32_wait4 with rusage parameter not supported\n");
#if (defined DEBUG_SIGNALS)
      suspSet.erase(pid);
#endif
      ccontext->reap();
      return sysCall32SetRet(context,cpid);
    }
    if(options&Mips32_WNOHANG)
      return sysCall32SetErr(context,Mips32_ECHILD);
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

  void sysCall32_waitpid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_waitpid: not implemented at 0x%08x\n",context->getIAddr());
  }
  InstDesc *sysCall32_execve(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr fname= myArgs.getA();
    VAddr argv = myArgs.getA();
    VAddr envp = myArgs.getA();
    // Get the filename and check file for errors
    ssize_t fnameLen=context->readMemString(fname,0,0);
    if(fnameLen<=1){
      sysCall32SetErr(context,Mips32_ENOENT);
      return inst;
    }
    char fnameBuf[fnameLen];
    context->readMemString(fname,fnameLen,fnameBuf);
    size_t realNameLen=FileSys::FileNames::getFileNames()->getReal(fnameBuf,0,0);
    char realName[realNameLen];
    FileSys::FileNames::getFileNames()->getReal(fnameBuf,realNameLen,realName);
    int elfErr=checkElfObject(realName);
    if(elfErr){
      sysCall32SetErr(context,fromNativeErrNums(elfErr));
      return inst;
    }
    // Pipeline flush to avoid mixing old and new InstDesc's in the pipeline
    if(context->getRefCount()>1){
      context->updIAddr(-inst->aupdate,-1);
      return 0;
    }
    // Find the length of the argv list
    size_t argvNum=0;
    size_t argvLen=0;
    while(true){
      VAddr argAddr=0;
      context->readMem(argv+sizeof(VAddr)*argvNum,argAddr);
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
      VAddr envAddr=0;
      context->readMem(envp+sizeof(VAddr)*envpNum,envAddr);
      if(!envAddr)
	break;
      ssize_t envLen=context->readMemString(envAddr,0,0);
      I(envLen>=1);
      envpLen+=envLen;
      envpNum++;
    }
    //    envpNum=0;
    char *argvArr[argvNum];
    char argvBuf[argvLen];
    char *argvPtr=argvBuf;
    for(size_t arg=0;arg<argvNum;arg++){
      VAddr argAddr=0;
      context->readMem(argv+sizeof(VAddr)*arg,argAddr);
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
      VAddr envAddr=0;
      context->readMem(envp+sizeof(VAddr)*env,envAddr);
      ssize_t envLen=context->readMemString(envAddr,0,0);
      I(envLen>=1);
      context->readMemString(envAddr,envLen,envpPtr);
      envpArr[env]=envpPtr;
      envpPtr+=envLen;
    }
    printf("execve fname is %s\n",realName);
    for(size_t arg=0;arg<argvNum;arg++){
      printf("execve argv[%ld] is %s\n",arg,argvArr[arg]);
    }
    for(size_t env=0;env<envpNum;env++){
      printf("execve envp[%ld] is %s\n",env,envpArr[env]);
    }
    // Close files that are still open and are cloexec
    context->getOpenFiles()->exec();
    // Clear up the address space and load the new object file
    context->getAddressSpace()->clear(true);
    loadElfObject(realName,context);
    I(context->getMode()==Mips32);
    Mips::initSystem(context);
    Mips::createStack(context);
    Mips::setProgArgs(context,argvNum,argvArr,envpNum,envpArr);
    // The InstDesc is gone now and we can't put it through the pipeline
    return 0;
  }

  void sysCall32_ptrace(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ptrace: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_alarm(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int seconds=myArgs.getW();
    // Clear existing alarms
    int oseconds=0;
    if(seconds){
      // Set new alarm
      fail("sysCall32_alarm(%d): not implemented at 0x%08x\n",seconds,context->getIAddr());
    }
    sysCall32SetRet(context,oseconds);
  }

  void sysCall32_pause(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_pause: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_nice(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_nice: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_signal(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_signal: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_acct(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_acct: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sgetmask(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sgetmask: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_ssetmask(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ssetmask: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sigsuspend(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sigsuspend: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sigpending(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sigpending: not implemented at 0x%08x\n",context->getIAddr());
  }
  
  bool operator==(const Mips32_timeval &var1, const Mips32_timeval &var2){
    return (var1.tv_sec==var2.tv_sec)&&(var1.tv_usec==var2.tv_usec);
  }
  bool operator==(const Mips32_rusage &rusg1, const Mips32_rusage &rusg2){
    return (rusg1.ru_utime==rusg2.ru_utime)&&(rusg1.ru_stime==rusg2.ru_utime);
  }
  void sysCall32_getrusage(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int     who=myArgs.getW();
    VAddr usage=myArgs.getA();

    // TODO: This is a hack. See definition of these vars
    myUsrUsecs+=100;
    mySysUsecs+=1;
    
    Mips32_rusage res;
    if(!context->canWrite(usage,sizeof(res)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    memset(&res,0,sizeof(res));
    res.ru_utime.tv_sec =myUsrUsecs/1000000;
    res.ru_utime.tv_usec=myUsrUsecs%1000000;
    res.ru_stime.tv_sec =mySysUsecs/1000000;
    res.ru_stime.tv_usec=mySysUsecs%1000000;
    // Other fields are zeros and need no endian-fixing
    //    // DEBUG begin: Remove after done
    //    Mips32_rusage res2;
    //    ID(context->readMem(usage,res2));
    //    I(memcmp(&res,&res2,sizeof(res))==0);
    //    // DEBUG end
    context->writeMem(usage,res);
    return sysCall32SetRet(context,0);
  }
  void sysCall32_getpriority(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getpriority: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setpriority(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setpriority: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setitimer(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setitimer: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getitimer(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getitimer: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sigreturn(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sigreturn: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_clone(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int32_t      flags=myArgs.getW();
    Mips32_VAddr child_stack=myArgs.getA();
    if(!(flags&Mips32_CLONE_FS))
      fail("sysCall32_clone without CLONE_FS not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags);
//     if(flags&Mips32_CLONE_PARENT)
//       fail("sysCall32_clone with CLONE_PARENT not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags);
//     if(flags&Mips32_CLONE_THREAD)
//       fail("sysCall32_clone with CLONE_PARENT not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags);
    if(flags&Mips32_CLONE_VFORK)
      fail("sysCall32_clone with CLONE_VFORK not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags);
//     if(flags&Mips32_CLONE_NEWNS)
//       fail("sysCall32_clone with CLONE_VFORK not supported yet at 0x%08x, flags=0x%08x\n",context->getIAddr(),flags);
    SignalID sig=sigToLocal(flags&Mips32_CSIGNAL);
    //    ThreadContext *newContext=context->createChild(flags&Mips32_CLONE_VM,flags&Mips32_CLONE_SIGHAND,flags&Mips32_CLONE_FILES,sig);
    ThreadContext *newContext=new ThreadContext(*context,flags&Mips32_CLONE_VM,flags&Mips32_CLONE_SIGHAND,flags&Mips32_CLONE_FILES,sig);
    I(newContext!=0);
    // Fork returns an error only if there is no memory, which should not happen here
    // Set return values for parent and child
    sysCall32SetRet(context,newContext->getPid());
    sysCall32SetRet(newContext,0);
    if(child_stack){
      newContext->setStack(newContext->getAddressSpace()->getSegmentAddr(child_stack),child_stack);
      switch(context->getMode()){
      case Mips32:
	setReg<Mips32,uint32_t>(newContext,static_cast<RegName>(RegSP),child_stack);
	break;
      default:
	I(0);
      }
#if (defined DEBUG_BENCH)
      newContext->clearCallStack();
#endif
    }else{
      I(!(flags&Mips32_CLONE_VM));
    }
    // Inform SESC that this process is created
    osSim->eventSpawn(-1,newContext->getPid(),0);
  }
  void sysCall32_sigprocmask(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sigprocmask: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getpid(InstDesc *inst, ThreadContext *context){
    return sysCall32SetRet(context,context->getPid());
  }
  void sysCall32_getpgid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getpgid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setpgid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setpgid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getppid(InstDesc *inst, ThreadContext *context){
    sysCall32SetRet(context,context->getParentID());
  }
  void sysCall32_getpgrp(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getpgrp: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getsid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getsid: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_setsid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setsid: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_kill(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int    pid = myArgs.getW();
    int    sig = myArgs.getW();
    ThreadContext *kcontext=osSim->getContext(pid);
    SigInfo *sigInfo=new SigInfo(sigToLocal(sig),SigCodeUser);
    sigInfo->pid=context->getPid();
    kcontext->signal(sigInfo);
#if (defined DEBUG_SIGNALS)
    printf("sysCall32_kill: signal %d sent from process %d to %d\n",sig,context->getPid(),pid);
#endif
    sysCall32SetRet(context,0);
  }
  void sysCall32_sigaction(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sigaction: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_rt_sigreturn(InstDesc *inst, ThreadContext *context){
#if (defined DEBUG_SIGNALS)
    printf("sysCall32_rt_sigreturn pid %d to ",context->getPid());
#endif    
    VAddr sp=getReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP));
    // Restore old signal mask
    Mips32_k_sigset_t oldMask;
    context->readMem(sp,oldMask);
    sp+=sizeof(oldMask);
    // Restore registers and PC
    for(size_t i=0;i<32;i++){
      uint32_t rdVal=0xDEADBEEF;
      context->readMem<uint32_t>(sp,rdVal);
      sp+=4;
      switch(31-i){
      case 0:
#if (defined DEBUG_SIGNALS)
	printf("0x%08x\n",rdVal);
#endif
	context->setIAddr(rdVal);
	break;
      default:
	setReg<Mips32,uint32_t>(context,static_cast<RegName>(31-i),rdVal);
	break;
      }
    }
#if (defined DEBUG)
    uint32_t chk=getReg<Mips32,uint32_t>(context,static_cast<RegName>(RegSP));
    I(sp==chk);
#endif
    context->execRet();
    SignalSet    oldSet;
    sigMaskToLocal(oldMask,oldSet);
    context->setSignalMask(oldSet);
  }
  void sysCall32_rt_sigaction(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int    sig =myArgs.getW();
    VAddr  act =myArgs.getA();
    VAddr  oact=myArgs.getA();
    size_t size=myArgs.getW();
    if((act)&&(!context->canRead(act,sizeof(Mips32_k_sigaction))))
      return sysCall32SetErr(context,Mips32_EFAULT);
    if((oact)&&!context->canWrite(oact,sizeof(Mips32_k_sigaction)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    Mips32_k_sigaction actBuf;
    if(size!=sizeof(actBuf.sa_mask))
      return sysCall32SetErr(context,Mips32_EINVAL);
    SignalID     localSig=sigToLocal(sig);
    SignalTable *sigTable=context->getSignalTable();
    SignalDesc  &sigDesc=(*sigTable)[localSig];
    // Get the existing signal handler into oactBuf
    if(oact){
      Mips32_k_sigaction oactBuf;
      VAddr oactHandler=sigDesc.handler;
      if(oactHandler==(VAddr)(getDflSigAction(localSig))){
	oactHandler=(VAddr)Mips32_SIG_DFL;
      }else if(oactHandler==(VAddr)SigActIgnore){
	oactHandler=(VAddr)Mips32_SIG_IGN;
      }
      oactBuf.k_sa_handler=oactHandler;
      oactBuf.sa_flags=0;//fromNativeSigactionFlags(context->getSignalFlags(sig));
      sigMaskFromLocal(oactBuf.sa_mask,sigDesc.mask);
      oactBuf.sa_restorer=0;
      context->writeMem(oact,oactBuf);
    }
    if(act){
      context->readMem(act,actBuf);
      if(actBuf.sa_restorer)
	printf("Mips::sysCall32_rt_sigaction specifies restorer, not supported\n");
      // Set the new signal handler
      VAddr actHandler;
      if(actBuf.k_sa_handler==(VAddr)Mips32_SIG_DFL){
	actHandler=getDflSigAction(localSig);
      }else if(actBuf.k_sa_handler==(VAddr)Mips32_SIG_IGN){
	actHandler=SigActIgnore;
      }else{
	actHandler=actBuf.k_sa_handler;
      }
      sigDesc.handler=actHandler;
      sigMaskToLocal(actBuf.sa_mask,sigDesc.mask);
      // Without SA_NODEFER, mask signal out in its own handler
      if(!(actBuf.sa_flags&Mips32_SA_NODEFER))
	sigDesc.mask.set(localSig);
      if(actBuf.sa_flags&Mips32_SA_SIGINFO)
	printf("Mips::sysCall32_rt_sigaction specifies SA_SIGINFO, not supported\n");
      if(actBuf.sa_flags&Mips32_SA_ONSTACK)
	printf("Mips::sysCall32_rt_sigaction specifies SA_ONSTACK, not supported\n");      
      if(actBuf.sa_flags&Mips32_SA_RESETHAND)
	printf("Mips::sysCall32_rt_sigaction specifies SA_RESETHAND, not supported\n");
      if((localSig==SigChld)&&!(actBuf.sa_flags&Mips32_SA_NOCLDSTOP))
	printf("Mips::sysCall32_rt_sigaction SIGCHLD without SA_NOCLDSTOP, not supported\n");
    }
    sysCall32SetRet(context,0);
  }
  void sysCall32_rt_sigprocmask(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int     how=myArgs.getW();
    VAddr  nset=myArgs.getA();
    VAddr  oset=myArgs.getA();
    size_t size=myArgs.getW();
    if((nset)&&(!context->canRead(nset,size)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    if((oset)&&(!context->canWrite(oset,size)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    SignalSet oldMask=context->getSignalMask();
    if(oset){
      Mips32_k_sigset_t omask;
      sigMaskFromLocal(omask,oldMask);
      context->writeMem(oset,omask);
    }
    if(nset){
      Mips32_k_sigset_t nmask;
      context->readMem(nset,nmask);
      SignalSet lset;
      sigMaskToLocal(nmask,lset);
      switch(how){
      case Mips32_SIG_BLOCK:
	context->setSignalMask(oldMask|lset);
	break;
      case Mips32_SIG_UNBLOCK:
	context->setSignalMask((oldMask|lset)^lset);
	break;
      case Mips32_SIG_SETMASK:
	context->setSignalMask(lset);
	break;
      default: fail("Unsupported value of how\n");
      }
    }
    sysCall32SetRet(context,0);
  }
  void sysCall32_rt_sigsuspend(InstDesc *inst, ThreadContext *context){
    // If this is a suspend following a wakeup, we need to pop the already-saved mask
    popSignalMask(context);
    Mips32FuncArgs myArgs(context);
    VAddr  nset=myArgs.getA();
    size_t size=myArgs.getW();
    if(!context->canRead(nset,size))
      return sysCall32SetErr(context,Mips32_EFAULT);
    // Change signal mask while suspended
    SignalSet oldMask=context->getSignalMask();
    Mips32_k_sigset_t appmask;
    context->readMem(nset,appmask);
    SignalSet newMask;
    sigMaskToLocal(appmask,newMask);
    context->setSignalMask(newMask);
    if(context->hasReadySignal()){
      sysCall32SetErr(context,Mips32_EINTR);
      SigInfo *sigInfo=context->nextReadySignal();
      context->setSignalMask(oldMask);
      handleSignal(context,sigInfo);
    }else{
      Pid_t pid=context->getPid();
#if (defined DEBUG_SIGNALS)
      printf("Suspend %d in sysCall32_rt_sigsuspend\n",pid);
      context->dumpCallStack();
      printf("Also suspended:");
      for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
	printf(" %d",*suspIt);
      printf("\n");
      suspSet.insert(context->getPid());
#endif
      // Save the old signal mask on stack so it can be restored
      pushSignalMask(context,oldMask);
      // Suspend and redo this system call when woken up
      context->updIAddr(-inst->aupdate,-1);
      I(inst==context->getIDesc());
      I(inst==context->virt2inst(context->getIAddr()));
      context->suspend();
    }
  }
  void sysCall32_rt_sigpending(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_rt_sigpending: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_rt_sigtimedwait(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_rt_sigtimedwait: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_rt_sigqueueinfo(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_rt_sigqueueinfo: not implemented at 0x%08x\n",context->getIAddr());
  }
  
  // Process user/group ID management
  
  void sysCall32_getuid(InstDesc *inst, ThreadContext *context){
    // TODO: With different users, need to track simulated uid
    return sysCall32SetRet(context,getuid());
  }
  void sysCall32_geteuid(InstDesc *inst, ThreadContext *context){
    // TODO: With different users, need to track simulated euid
    return sysCall32SetRet(context,geteuid());
  }
  void sysCall32_getresuid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getresuid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setuid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setuid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setreuid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setreuid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setresuid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setresuid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getgid(InstDesc *inst, ThreadContext *context){
    // TODO: With different users, need to track simulated gid
    return sysCall32SetRet(context,getgid());
  }
  void sysCall32_getegid(InstDesc *inst, ThreadContext *context){
    // TODO: With different users, need to track simulated egid
    return sysCall32SetRet(context,getegid());
  }
  void sysCall32_getresgid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getresgid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setgid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setgid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setregid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setregid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setresgid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setresgid: not implemented at 0x%08x\n",context->getIAddr());
  }
  
  void sysCall32_getgroups(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    size_t size=myArgs.getW();
    VAddr  list=myArgs.getA();
    printf("sysCall32_getgroups(%ld,0x%08x)called\n",size,list);
    sysCall32SetRet(context,0);
  }
  
  // Process memory management system calls
  
  void sysCall32_brk(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr brkPos=myArgs.getA();
    VAddr retVal=(VAddr)-1;
    VAddr  brkBase=context->getAddressSpace()->getBrkBase();
    size_t brkSize=context->getAddressSpace()->getSegmentSize(brkBase);
    if(brkPos==0)
      return sysCall32SetRet(context,brkBase+brkSize);
    if(brkPos<=brkBase+brkSize){
      if(brkPos<=brkBase)
	fail("sysCall32_brk: new break makes data segment size negative\n");
      context->getAddressSpace()->resizeSegment(brkBase,brkPos-brkBase);
      return sysCall32SetRet(context,brkPos);
    }
    if(context->getAddressSpace()->isNoSegment(brkBase+brkSize,brkPos-(brkBase+brkSize))){
      context->getAddressSpace()->resizeSegment(brkBase,brkPos-brkBase);
      context->writeMemWithByte(brkBase+brkSize,brkPos-(brkBase+brkSize),0);
      return sysCall32SetRet(context,brkPos);
    }
    return sysCall32SetErr(context,Mips32_ENOMEM);
  }
  
  void sysCall32_mmap(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  start=myArgs.getA();
    size_t length=myArgs.getW();
    int    prot=myArgs.getW();
    int    flags=myArgs.getW();
    int    fd=myArgs.getW();
    off_t  offset=myArgs.getW();
    if(flags&Mips32_MAP_FIXED)
      fail("sysCall32Mmap: MAP_FIXED not supported\n");
    I((flags&Mips32_MAP_SHARED)||(flags&Mips32_MAP_PRIVATE));
    I(!((flags&Mips32_MAP_SHARED)&&(flags&Mips32_MAP_PRIVATE)));
    if(!(flags&Mips32_MAP_ANONYMOUS))
      fail("sysCall32Mmap: Can't map real files yet\n");
    VAddr  retVal=0;
    if(start&&context->getAddressSpace()->isNoSegment(start,length)){
      retVal=start;
    }else{
      retVal=context->getAddressSpace()->newSegmentAddr(length);
    }
    if(!retVal)
      return sysCall32SetErr(context,Mips32_ENOMEM);
    // Create a write-only segment and zero it out
    context->getAddressSpace()->newSegment(retVal,length,false,true,false,flags&Mips32_MAP_SHARED);
    context->writeMemWithByte(retVal,length,0);
    context->getAddressSpace()->protectSegment(retVal,length,prot&Mips32_PROT_READ,prot&Mips32_PROT_WRITE,prot&Mips32_PROT_EXEC);
#if (defined DEBUG_MEMORY)
    printf("sysCall32_mmap addr 0x%08x len 0x%08lx R%d W%d X%d S%d\n",
	   retVal,(unsigned long)length,
	   (bool)(prot&Mips32_PROT_READ),
	   (bool)(prot&Mips32_PROT_WRITE),
	   (bool)(prot&Mips32_PROT_EXEC),
	   (bool)(flags&Mips32_MAP_SHARED));
#endif
    return sysCall32SetRet(context,retVal);
  }
  void sysCall32_mremap(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  oldaddr=myArgs.getA();
    size_t oldsize=myArgs.getW();
    size_t newsize=myArgs.getW();
    int    flags  =myArgs.getW();
    AddressSpace *addrSpace=context->getAddressSpace();
    if(!addrSpace->isSegment(oldaddr,oldsize))
      fail("sysCall32_mremap: old range not a segment\n");
    if(newsize<=oldsize){
      if(newsize<oldsize)
	addrSpace->resizeSegment(oldaddr,newsize);
      return sysCall32SetRet(context,oldaddr);
    }
    VAddr retVal=0;
    if(addrSpace->isNoSegment(oldaddr+oldsize,newsize-oldsize)){
      retVal=oldaddr;
    }else if(flags&Mips32_MREMAP_MAYMOVE){
      retVal=addrSpace->newSegmentAddr(newsize);
      if(retVal)
	addrSpace->moveSegment(oldaddr,retVal);
    }
    if(!retVal)
      return sysCall32SetErr(context,Mips32_ENOMEM);
    addrSpace->resizeSegment(retVal,newsize);
    context->writeMemWithByte(retVal+oldsize,newsize-oldsize,0);      
    return sysCall32SetRet(context,retVal);
  }
  void sysCall32_munmap(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  start=myArgs.getA();
    size_t length=myArgs.getW();
    //  printf("munmap from 0x%08x to 0x%08lx\n",start,start+length);
    context->getAddressSpace()->deleteSegment(start,length);
#if (defined DEBUG_MEMORY)
    printf("sysCall32_munmap addr 0x%08x len 0x%08lx\n",
	   start,(unsigned long)length);
#endif
    sysCall32SetRet(context,0);
  }
  void sysCall32_mprotect(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  addr=myArgs.getA();
    size_t len=myArgs.getW();
    int    prot=myArgs.getW();
#if (defined DEBUG_MEMORY)
    printf("sysCall32_mprotect addr 0x%08x len 0x%08lx R%d W%d X%d\n",
	   addr,(unsigned long)len,
	   (bool)(prot&Mips32_PROT_READ),
	   (bool)(prot&Mips32_PROT_WRITE),
	   (bool)(prot&Mips32_PROT_EXEC));
#endif
    if(!context->getAddressSpace()->isMapped(addr,len))
      printf("sysCall32_mprotect EINVAL at addr 0x%08x len 0x%08lx R%d W%d X%d\n",
	     addr,(unsigned long)len,
	     (bool)(prot&Mips32_PROT_READ),
	     (bool)(prot&Mips32_PROT_WRITE),
	     (bool)(prot&Mips32_PROT_EXEC));
    if(!context->getAddressSpace()->isMapped(addr,len))
      return sysCall32SetErr(context,Mips32_EINVAL);
    context->getAddressSpace()->protectSegment(addr,len,prot&Mips32_PROT_READ,prot&Mips32_PROT_WRITE,prot&Mips32_PROT_EXEC);
    sysCall32SetRet(context,0);
  }
  void sysCall32_msync(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_msync: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_cacheflush(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_cacheflush: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_mlock(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mlock: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_munlock(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_munlock: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_mlockall(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mlockall: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_munlockall(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_munlockall: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_mmap2(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mmap2: not implemented at 0x%08x\n",context->getIAddr());
  }

  // File system calls
  
  void sysCall32_open(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr path  = myArgs.getA();
    int   flags = myArgs.getW();
    int   mode  = myArgs.getW();  
    ssize_t pathLen=context->readMemString(path,0,0);
    if(pathLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char pathBuf[pathLen];
    if(context->readMemString(path,pathLen,pathBuf)!=pathLen)
      I(pathLen!=pathLen);
    I((size_t)pathLen==strlen(pathBuf)+1);
    // Convert flags
    int natFlags=toNativeOpenFlags(flags);
    // Do the actual call
    int newfd=context->getOpenFiles()->open(pathBuf,natFlags,mode);
    if(newfd==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
#ifdef DEBUG_FILES
    printf("[%d] open %s as %d\n",context->getPid(),pathBuf,newfd);
#endif
    sysCall32SetRet(context,newfd);
  }
  void sysCall32_creat(InstDesc *inst, ThreadContext *context){
    fail("Mips::sysCall32_creat: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_dup(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int oldfd=myArgs.getW();
    // Do the actual call
    int newfd=context->getOpenFiles()->dup(oldfd);
    if(newfd==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
#ifdef DEBUG_FILES
    printf("[%d] dup %d as %d\n",context->getPid(),oldfd,newfd);
#endif
    sysCall32SetRet(context,newfd);
  }
  void sysCall32_dup2(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int oldfd=myArgs.getW();
    int newfd=myArgs.getW();
#ifdef DEBUG_FILES
    printf("[%d] dup2 %d to %d called\n",context->getPid(),oldfd,newfd);
#endif
    // Do the actual call
    if(context->getOpenFiles()->dup2(oldfd,newfd)==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
#ifdef DEBUG_FILES
    printf("[%d] dup2 %d as %d\n",context->getPid(),oldfd,newfd);
#endif
    sysCall32SetRet(context,newfd);
  }
  void sysCall32_close(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int oldfd = myArgs.getW();
    // Do the actual call
    if(context->getOpenFiles()->close(oldfd)==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));    
#ifdef DEBUG_FILES
    printf("[%d] close %d\n",context->getPid(),oldfd);
#endif
    sysCall32SetRet(context,0);
  }

  void sysCall32_pipe(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    // Do the actual call
    int fds[2];
    if(context->getOpenFiles()->pipe(fds)==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
#ifdef DEBUG_FILES
    printf("[%d] pipe rd %d wr %d\n",context->getPid(),fds[0],fds[1]);
#endif
    sysCall32SetRet2(context,fds[0],fds[1]);
  }
  void sysCall32_fcntl64(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int fd = myArgs.getW();
    int cmd   = myArgs.getW();
    switch(cmd){
    case Mips32_F_DUPFD:
      {
#ifdef DEBUG_FILES
	printf("[%d] dupfd %d called\n",context->getPid(),fd);
#endif
	int minfd = myArgs.getW();
	int newfd = context->getOpenFiles()->dupfd(fd,minfd);
	if(newfd==-1)
	  return sysCall32SetErr(context,fromNativeErrNums(errno));
#ifdef DEBUG_FILES
	printf("[%d] dupfd %d as %d\n",context->getPid(),fd,newfd);
#endif
	I(newfd>=minfd);
	I(context->getOpenFiles()->isOpen(fd));
	I(context->getOpenFiles()->isOpen(newfd));
	I(context->getOpenFiles()->getDesc(fd)->getStatus()==context->getOpenFiles()->getDesc(newfd)->getStatus());
	return sysCall32SetRet(context,newfd);
      }
    case Mips32_F_GETFD: 
      {
#ifdef DEBUG_FILES
	printf("[%d] getfd %d called\n",context->getPid(),fd);
#endif
	int cloex=context->getOpenFiles()->getfd(fd);
	if(cloex==-1)
	  return sysCall32SetErr(context,fromNativeErrNums(errno));
	return sysCall32SetRet(context,fromNativeFcntlFlag(cloex));
      }
    case Mips32_F_SETFD:
      {
	int cloex = myArgs.getW();
#ifdef DEBUG_FILES
	printf("[%d] setfd %d to %d called\n",context->getPid(),fd,cloex);
#endif
	if(context->getOpenFiles()->setfd(fd,toNativeFcntlFlag(cloex))==-1)
	  return sysCall32SetErr(context,fromNativeErrNums(errno));
//	context->getAddressSpace()->setCloexec(fd,(arg&Mips32_FD_CLOEXEC));
#ifdef DEBUG_FILES
	printf("[%d] setfd %d to %d \n",context->getPid(),fd,cloex);
#endif
      } break;
    case Mips32_F_GETFL:
      {
#ifdef DEBUG_FILES
	printf("[%d] getfl %d called\n",context->getPid(),fd);
#endif
	int flags  = context->getOpenFiles()->getfl(fd);
	if(flags==-1)
	  return sysCall32SetErr(context,fromNativeErrNums(errno));
	return sysCall32SetRet(context,fromNativeOpenFlags(flags));
      }
    default: fail("sysCall32_fcntl64 unknown command %d on file %d\n",cmd,fd);
    }
    sysCall32SetRet(context,0);
  }
  void sysCall32_read(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int    fd    = myArgs.getW();
    VAddr  buf   = myArgs.getA();
    size_t count = myArgs.getW();
    if(!context->canWrite(buf,count))
      return sysCall32SetErr(context,Mips32_EFAULT);
    ssize_t retVal=context->writeMemFromFile(buf,count,fd,false);
    if((retVal==-1)&&(errno==EAGAIN)&&!(context->getOpenFiles()->getfl(fd)&O_NONBLOCK)){
      // Enable SigIO
      SignalSet newMask=context->getSignalMask();
      if(newMask.test(SigIO))
	fail("sysCall32_read: SigIO masked out, not supported\n");
//       newMask.reset(SigIO);
//       sstate->pushMask(newMask);
      if(context->hasReadySignal()){
	sysCall32SetErr(context,Mips32_EINTR);
	SigInfo *sigInfo=context->nextReadySignal();
// 	sstate->popMask();
	handleSignal(context,sigInfo);
	return;
      }
      context->updIAddr(-inst->aupdate,-1);
      I(inst==context->getIDesc());
      I(inst==context->virt2inst(context->getIAddr()));
      context->getOpenFiles()->addReadBlock(fd,context->getPid());
//      sstate->setWakeup(new SigCallBack(context->getPid(),true));
#if (defined DEBUG_SIGNALS)
      printf("Suspend %d in sysCall32_read(fd=%d)\n",context->getPid(),fd);
      context->dumpCallStack();
      printf("Also suspended:");
      for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
	printf(" %d",*suspIt);
      printf("\n");
      suspSet.insert(context->getPid());
#endif
      context->suspend();
      return;
    }
#ifdef DEBUG_FILES
    printf("[%d] read %d wants %ld gets %ld bytes\n",context->getPid(),fd,count,retVal);
#endif
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    return sysCall32SetRet(context,retVal);
  }
  void sysCall32_write(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int    fd = myArgs.getW();
    VAddr  buf   = myArgs.getA();
    size_t count = myArgs.getW();
    if(!context->canRead(buf,count))
      return sysCall32SetErr(context,Mips32_EFAULT);
    ssize_t retVal=context->readMemToFile(buf,count,fd,false);
#ifdef DEBUG_FILES
    printf("[%d] write %d wants %ld gets %ld bytes\n",context->getPid(),fd,count,retVal);
#endif
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,retVal);
  }

  void sysCall32_readv(InstDesc *inst, ThreadContext *context){
    fail("Mips::sysCall32_readv: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_writev(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int    fd  =myArgs.getW();
    VAddr  vector =myArgs.getA();
    int    count  =myArgs.getW();
    // Get the vector from the simulated address space
    Mips32_iovec myIovec[count];
    I(sizeof(myIovec)==sizeof(Mips32_iovec)*count);
    if(!context->canRead(vector,sizeof(myIovec)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    context->readMemToBuf(vector,sizeof(myIovec),myIovec);
    size_t totBytes=0;
    for(int i=0;i<count;i++){
      cvtEndianBig(myIovec[i]);
      VAddr  base = myIovec[i].iov_base;
      size_t len  = myIovec[i].iov_len;
      if(!context->canRead(base,len))
	return sysCall32SetErr(context,Mips32_EFAULT);
      ssize_t nowBytes=context->readMemToFile(base,len,fd,false);
      if(nowBytes==-1)
	return sysCall32SetErr(context,fromNativeErrNums(errno));
      totBytes+=nowBytes;
      if((size_t)nowBytes<len)
	break;
    }
    sysCall32SetRet(context,totBytes);
  }
  void sysCall32_pread(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_pread: not implemented at 0x%08x\n",context->getIAddr()); 
  }
  void sysCall32_pwrite(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_pwrite: not implemented at 0x%08x\n",context->getIAddr()); 
  }
  void sysCall32_lseek(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int   fd     = myArgs.getW();
    off_t offset = myArgs.getW();
    int   whence = myArgs.getW();
    off_t retVal=context->getOpenFiles()->seek(fd,offset,toNativeWhenceFlags(whence));
    if(retVal==(off_t)-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,retVal);
  }
  void sysCall32__llseek(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int      fd     = myArgs.getW();
    uint32_t hi     = myArgs.getW();
    uint32_t lo     = myArgs.getW();
    VAddr    res    = myArgs.getA();
    int      whence = myArgs.getW();
    if(!context->canWrite(res,sizeof(uint64_t)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    off64_t offs = (int64_t)((((uint64_t)hi)<<32)|((uint64_t)lo));
    off_t resVal=context->getOpenFiles()->seek(fd,offs,toNativeWhenceFlags(whence));
    if(resVal==(off_t)-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    context->writeMem<uint64_t>(res,resVal);
    sysCall32SetRet(context,0);
  }
  void sysCall32_ioctl(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int fd      = myArgs.getW();
    int request = myArgs.getW();
    if(!context->getOpenFiles()->isOpen(fd))
      return sysCall32SetErr(context,Mips32_EBADF);
    FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
    int realfd=desc->getStatus()->fd;
    switch(request){
    case Mips32_TCGETS: {
      struct Mips32_kernel_termios{
        uint32_t c_iflag; // Input mode flags
        uint32_t c_oflag; // Output mode flags
        uint32_t c_cflag; // Control mode flags
        uint32_t c_lflag; // Local mode flags
        unsigned char c_line;      // Line discipline
        unsigned char c_cc[19]; // Control characters
      };
      // TODO: For now, it never succeeds (assume no file is a terminal)
      return sysCall32SetErr(context,Mips32_ENOTTY);
    } break;
    case Mips32_TIOCGWINSZ: {
      VAddr wsiz= myArgs.getA();
      Mips32_winsize simWinsize;
      if(!context->canWrite(wsiz,sizeof(simWinsize)))
	return sysCall32SetErr(context,Mips32_EFAULT);
      struct winsize natWinsize;
      int retVal=ioctl(realfd,TIOCGWINSZ,&natWinsize);
      if(retVal==-1)
	return sysCall32SetErr(context,fromNativeErrNums(errno));
      simWinsize.ws_row=natWinsize.ws_row;
      simWinsize.ws_col=natWinsize.ws_col;
      simWinsize.ws_xpixel=natWinsize.ws_xpixel;
      simWinsize.ws_ypixel=natWinsize.ws_ypixel;
      context->writeMem(wsiz,simWinsize);
    } break;
    case Mips32_TCSETSW:
      // TODO: For now, it never succeeds (assume no file is a terminal)
      return sysCall32SetErr(context,Mips32_ENOTTY);      
    default:
      fail("Mips::sysCall32_ioctl called with fd %d and req 0x%x at 0x%08x\n",
	   fd,request,context->getIAddr());
    }
    sysCall32SetRet(context,0);
  }
  void sysCall32_getdents64(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int   fd    = myArgs.getW();
    VAddr dirp  = myArgs.getA();
    int   count = myArgs.getW();
    if(!context->getOpenFiles()->isOpen(fd))
      return sysCall32SetErr(context,Mips32_EBADF);
    FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
    int realfd=desc->getStatus()->fd;
    // How many bytes have been read so far
    int retVal=0;
    // Get the current position in the direcotry file
    off_t dirPos=lseek(realfd,0,SEEK_CUR);
    I(dirPos!=(off_t)-1);
    while(true){
      struct dirent64 natDent;
      Mips32_dirent64 simDent;
      __off64_t dummyDirPos=dirPos;
      ssize_t rdBytes=getdirentries64(realfd,(char *)(&natDent),sizeof(natDent),&dummyDirPos);
      if(rdBytes==-1)
	return sysCall32SetErr(context,fromNativeErrNums(errno));
      if(rdBytes==0)
	break;
      simDent.d_ino   =natDent.d_ino;
      simDent.d_type  =natDent.d_type;
      simDent.d_off   =natDent.d_off;
      size_t nmLen=strlen(natDent.d_name)+1;
      I(nmLen<=sizeof(simDent.d_name));
      // The actual length of this directory record (double-word aligned)
      size_t simRecLen=(simDent.d_name+nmLen)-((char *)(&simDent));
      size_t simRecPad=(simRecLen+7)-((simRecLen+7)%8)-simRecLen;
      strncpy(simDent.d_name,natDent.d_name,nmLen+simRecPad);
      simRecLen+=simRecPad;
      simDent.d_reclen=simRecLen;
      if(retVal+(int)simRecLen>count){
	lseek(realfd,dirPos,SEEK_SET);
	if(retVal==0)
	  return sysCall32SetErr(context,Mips32_EINVAL);
	break;
      }
      if(!context->canWrite(dirp+retVal,simRecLen))
	return sysCall32SetErr(context,Mips32_EFAULT);
      cvtEndianBig(simDent);
      context->writeMemFromBuf(dirp+retVal,simRecLen,&simDent);
      dirPos=lseek(realfd,natDent.d_off,SEEK_SET);
      retVal+=simRecLen;
      //      printf("Entry: %s type %d\n",natDent.d_name,natDent.d_type);
    }
    sysCall32SetRet(context,retVal);
  }
  template<class StatStruct>
  void fromNativeStat(StatStruct &mipsStat, const struct stat &natStat){
    mipsStat.st_dev     = natStat.st_dev;
    mipsStat.st_ino     = natStat.st_ino;
    mipsStat.st_mode    = natStat.st_mode;
    mipsStat.st_nlink   = natStat.st_nlink;
    mipsStat.st_uid     = natStat.st_uid;
    mipsStat.st_gid     = natStat.st_gid;
    mipsStat.st_rdev    = natStat.st_rdev;
    mipsStat.st_size    = natStat.st_size;
    mipsStat.st_atim    = natStat.st_atime;
    mipsStat.st_mtim    = natStat.st_mtime;
    mipsStat.st_ctim    = natStat.st_ctime;
    mipsStat.st_blksize = natStat.st_blksize;
    mipsStat.st_blocks  = natStat.st_blocks;
  };
  
  template<class StatStruct>
  bool equalsStat(const StatStruct &st1, const StatStruct &st2){
    return
      (st1.st_dev==st2.st_dev)&&(st1.st_ino==st2.st_ino)&&
      (st1.st_mode==st2.st_mode)&&(st1.st_nlink==st2.st_nlink)&&
      (st1.st_uid==st2.st_uid)&&(st1.st_gid==st2.st_gid)&&
      (st1.st_rdev==st2.st_rdev)&&(st1.st_size==st2.st_size)&&
      (st1.st_atim==st2.st_atim)&&(st1.st_mtim==st2.st_mtim)&&
      (st1.st_ctim==st2.st_ctim)&&(st1.st_blksize==st2.st_blksize)&&
      (st1.st_blocks==st2.st_blocks);
  }
  bool operator==(const Mips32_stat &st1, const Mips32_stat &st2){
    return equalsStat<Mips32_stat>(st1,st2);
  }
  bool operator==(const Mips32_stat64 &st1, const Mips32_stat64 &st2){
    return equalsStat<Mips32_stat64>(st1,st2);
  }

  enum StatVariant{ Stat, FStat, LStat, Stat64, FStat64, LStat64 };

  template<StatVariant StVar>
  void sysCall32AnyStat(ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    struct stat  nativeStat;
    int retVal;
    switch(StVar) {
    case Stat: case Stat64: case LStat: case LStat64: {
      // Get the file name
      VAddr fname=myArgs.getA();
      ssize_t fnameLen=context->readMemString(fname,0,0);
      if(fnameLen==-1)
	return sysCall32SetErr(context,Mips32_EFAULT);
      char fnameBuf[fnameLen];
      size_t fnameLenChk=context->readMemString(fname,fnameLen,fnameBuf);
      I(fnameLenChk==(size_t)fnameLen);
      I(fnameLenChk==strlen(fnameBuf)+1);
      // Do the actual system call
      switch(StVar){
      case Stat: case Stat64:
	retVal=stat(fnameBuf,&nativeStat); break;
      case LStat: case LStat64:
	retVal=lstat(fnameBuf,&nativeStat); break;
      }
    } break;
    case FStat: case FStat64: {
      int fd=myArgs.getW();
      if(!context->getOpenFiles()->isOpen(fd))
	return sysCall32SetErr(context,Mips32_EBADF);
      FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
      int realfd=desc->getStatus()->fd;
      retVal=fstat(realfd,&nativeStat);
      // Make standard in, out, and err look like TTY devices
      if(fd<3){
	// Change format to character device
	nativeStat.st_mode&=(~S_IFMT);
	nativeStat.st_mode|=S_IFCHR;
	// Change device to a TTY device type
	// DEV_TTY_LOW_MAJOR is 136, DEV_TTY_HIGH_MAJOR is 143
	// We use a major number of 136 (0x88)
	nativeStat.st_rdev=0x8800;
      }
    } break;
    }
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    VAddr  buf    = myArgs.getA();
    switch(StVar){
    case Stat: case FStat: case LStat: {
      Mips32_stat mipsStat;
      if(!context->canWrite(buf,sizeof(mipsStat)))
	return sysCall32SetErr(context,Mips32_EFAULT);
      fromNativeStat(mipsStat,nativeStat);
      context->writeMem(buf,mipsStat);
    } break;
    case Stat64: case FStat64: case LStat64: {
      Mips32_stat64 mipsStat;
      if(!context->canWrite(buf,sizeof(mipsStat)))
	return sysCall32SetErr(context,Mips32_EFAULT);
      fromNativeStat(mipsStat,nativeStat);
      context->writeMem(buf,mipsStat);
    } break;
    }
    sysCall32SetRet(context,0);
  }
  void sysCall32_stat64(InstDesc *inst, ThreadContext *context){ sysCall32AnyStat<Stat64>(context);}
  void sysCall32_lstat64(InstDesc *inst, ThreadContext *context){sysCall32AnyStat<LStat64>(context);}
  void sysCall32_fstat64(InstDesc *inst, ThreadContext *context){sysCall32AnyStat<FStat64>(context);}
  void sysCall32_stat(InstDesc *inst, ThreadContext *context){   sysCall32AnyStat<Stat>(context);}
  void sysCall32_lstat(InstDesc *inst, ThreadContext *context){  sysCall32AnyStat<LStat>(context);}
  void sysCall32_fstat(InstDesc *inst, ThreadContext *context){  sysCall32AnyStat<FStat>(context);}

  enum TruncateVariant { Trunc, FTrunc, Trunc64, FTrunc64 };

  template<TruncateVariant TrVar>
  void sysCall32AnyTrunc(ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    int retVal;
    switch(TrVar){
    case Trunc: case Trunc64: {
      // Get the file name
      VAddr path=myArgs.getA();
      ssize_t pathLen=context->readMemString(path,0,0);
      if(pathLen==-1)
	return sysCall32SetErr(context,Mips32_EFAULT);
      char pathBuf[pathLen];
      if(context->readMemString(path,pathLen,pathBuf)!=pathLen)
	I(pathLen!=pathLen);
      I((size_t)pathLen==strlen(pathBuf)+1);
      // Do the actual system call
      switch(TrVar){
      case Trunc:
	retVal=truncate(pathBuf,(off_t)(myArgs.getW())); break;
      case Trunc64:
	retVal=truncate(pathBuf,(off_t)(myArgs.getL())); break;
      }
    } break;
    case FTrunc: case FTrunc64: {
      int fd=myArgs.getW();
      if(!context->getOpenFiles()->isOpen(fd))
	return sysCall32SetErr(context,Mips32_EBADF);
      FileSys::FileDesc *desc=context->getOpenFiles()->getDesc(fd);
      int realfd=desc->getStatus()->fd;
      switch(TrVar){
      case FTrunc:
	retVal=ftruncate(realfd,(off_t)(myArgs.getW())); break;
      case FTrunc64:
	retVal=ftruncate(realfd,(off_t)(myArgs.getL())); break;
      }
    } break;
    }
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,0);
  }
  
  void sysCall32_truncate(InstDesc *inst, ThreadContext *context){ sysCall32AnyTrunc<Trunc>(context); }
  void sysCall32_ftruncate(InstDesc *inst, ThreadContext *context){ sysCall32AnyTrunc<FTrunc>(context); }
  void sysCall32_truncate64(InstDesc *inst, ThreadContext *context){ sysCall32AnyTrunc<Trunc64>(context); }
  void sysCall32_ftruncate64(InstDesc *inst, ThreadContext *context){ sysCall32AnyTrunc<FTrunc64>(context); }
  
  void sysCall32_link(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_link: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_unlink(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr pathname  = myArgs.getA();
    ssize_t pathnameLen=context->readMemString(pathname,0,0);
    if(pathnameLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char pathnameBuf[pathnameLen];
    size_t pathnameLenChk=context->readMemString(pathname,pathnameLen,pathnameBuf);
    I(pathnameLenChk==(size_t)pathnameLen);
    I(pathnameLenChk==strlen(pathnameBuf)+1);
    int retVal=unlink(pathnameBuf);
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,retVal);
  }
  void sysCall32_rename(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr oldpath  = myArgs.getA();
    VAddr newpath  = myArgs.getA();
    ssize_t oldpathLen=context->readMemString(oldpath,0,0);
    ssize_t newpathLen=context->readMemString(newpath,0,0);
    if((oldpathLen==-1)||(newpathLen==-1))
      return sysCall32SetErr(context,Mips32_EFAULT);
    char oldpathBuf[oldpathLen];
    char newpathBuf[newpathLen];
    size_t oldpathLenChk=context->readMemString(oldpath,oldpathLen,oldpathBuf);
    size_t newpathLenChk=context->readMemString(newpath,newpathLen,newpathBuf);
    I(oldpathLenChk==(size_t)oldpathLen);
    I(oldpathLenChk==strlen(oldpathBuf)+1);
    I(newpathLenChk==(size_t)newpathLen);
    I(newpathLenChk==strlen(newpathBuf)+1);
    int retVal=rename(oldpathBuf,newpathBuf);
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,retVal);
  }
  void sysCall32_chdir(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr path  = myArgs.getA();
    int   retVal = -1;
    int   errNum = 0;
    ssize_t pathLen=context->readMemString(path,0,0);
    if(pathLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char pathBuf[pathLen];
    size_t pathLenChk=context->readMemString(path,pathLen,pathBuf);
    I(pathLenChk==(size_t)pathLen);
    I(pathLenChk==strlen(pathBuf)+1);
    retVal=chdir(pathBuf);
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,retVal);
  }
  void sysCall32_mknod(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mknod: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_chmod(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_chmod: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_lchown(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_lchown: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_utime(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_utime: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_access(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  fname = myArgs.getA();
    int    mode  = myArgs.getW();
    ssize_t fnameLen=context->readMemString(fname,0,0);
    if(fnameLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char fnameBuf[fnameLen];
    size_t fnameLenChk=context->readMemString(fname,fnameLen,fnameBuf);
    I(fnameLenChk==(size_t)fnameLen);
    I(fnameLenChk==strlen(fnameBuf)+1);
    int retVal=access(fnameBuf,mode);
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,retVal);
  }

  void sysCall32_sync(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sync: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_mkdir(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr pathname  = myArgs.getA();
    int   mode      = myArgs.getW();
    ssize_t pathnameLen=context->readMemString(pathname,0,0);
    if(pathnameLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char pathnameBuf[pathnameLen];
    size_t pathnameLenChk=context->readMemString(pathname,pathnameLen,pathnameBuf);
    I(pathnameLenChk==(size_t)pathnameLen);
    I(pathnameLenChk==strlen(pathnameBuf)+1);
    if(mkdir(pathnameBuf,mode)!=0)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,0);
  }
  void sysCall32_rmdir(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr pathname  = myArgs.getA();
    ssize_t pathnameLen=context->readMemString(pathname,0,0);
    if(pathnameLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char pathnameBuf[pathnameLen];
    size_t pathnameLenChk=context->readMemString(pathname,pathnameLen,pathnameBuf);
    I(pathnameLenChk==(size_t)pathnameLen);
    I(pathnameLenChk==strlen(pathnameBuf)+1);
    if(rmdir(pathnameBuf)!=0)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    sysCall32SetRet(context,0);
  }

  void sysCall32_fcntl(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fcntl: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_umask(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    Mips32_mode_t mask = myArgs.getW();
    printf("sysCall32_umask(0x%08x) called\n",mask);
    sysCall32SetRet(context,0);
  }
  
  void sysCall32_ustat(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ustat: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_symlink(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_symlink: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32_readlink(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  path   = myArgs.getA();
    VAddr  buf    = myArgs.getA();
    size_t bufsiz = myArgs.getW();
    ssize_t pathLen=context->readMemString(path,0,0);
    if(pathLen==-1)
      return sysCall32SetErr(context,Mips32_EFAULT);
    char pathBuf[pathLen];
    if(context->readMemString(path,pathLen,pathBuf)!=pathLen)
      I(pathLen!=pathLen);
    I((size_t)pathLen==strlen(pathBuf)+1);
    char bufBuf[bufsiz];
    int retVal=readlink(pathBuf,bufBuf,bufsiz);
    if(retVal==-1)
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    size_t bufLen=strlen(bufBuf)+1;
    if(!context->canWrite(buf,bufLen))
      return sysCall32SetErr(context,Mips32_EFAULT);
    context->writeMemFromBuf(buf,bufLen,bufBuf);
    sysCall32SetRet(context,retVal);
  }

  void sysCall32_readdir(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_readdir: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_fchmod(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fchmod: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_fchown(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fchown: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_statfs(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_statfs: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_fstatfs(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fstatfs: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_ioperm(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ioperm: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_socketcall(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_socketcall: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_syslog(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_syslog: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_fsync(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fsync: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_fchdir(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fchdir: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setfsuid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setfsuid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setfsgid(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setfsgid: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getdents(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getdents: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32__newselect(InstDesc *inst, ThreadContext *context){
    fail("sysCall32__newselect: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_flock(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_flock: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_fdatasync(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_fdatasync: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_chown(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_chown: not implemented at 0x%08x\n",context->getIAddr()); }
  
  // Message-passing (IPC) system calls
  
  void sysCall32_ipc(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ipc: not implemented at 0x%08x\n",context->getIAddr());
  }

  // Obsolete system calls, should not be used
  
  void sysCall32_oldstat(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_oldstat: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_break(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_break: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_oldfstat(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_oldfstat: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_stty(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_stty: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_gtty(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_gtty: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_prof(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_prof: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_lock(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_lock: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_afs_syscall(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_afs_syscall: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_ftime(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ftime: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_getpmsg(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getpmsg: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_putpmsg(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_putpmsg: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_mpx(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mpx: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_profil(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_profil: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_ulimit(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_ulimit: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_oldlstat(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_oldlstat: not implemented at 0x%08x\n",context->getIAddr());
  }
  
  // Privileged (superuser) system calls, should not be used
  
  void sysCall32_mount(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mount: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_umount(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_umount: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_stime(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_stime: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_umount2(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_umount2: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_chroot(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_chroot: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sethostname(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sethostname: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setgroups(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setgroups: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_swapon(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_swapon: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_reboot(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_reboot: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_iopl(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_iopl: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_vhangup(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_vhangup: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_idle(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_idle: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_vm86(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_vm86: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_swapoff(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_swapoff: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_setdomainname(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setdomainname: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_modify_ldt(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_modify_ldt: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_adjtimex(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_adjtimex: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_create_module(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_create_module: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_init_module(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_init_module: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_delete_module(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_delete_module: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_get_kernel_syms(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_get_kernel_syms: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_quotactl(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_quotactl: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_bdflush(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_bdflush: not implemented at 0x%08x\n",context->getIAddr());
  }
  
  void sysCall32_time(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr t=myArgs.getA();
    // TODO: This should actually take into account simulated time
    if(wallClock==(time_t)-1)
      wallClock=time(0);
    I(wallClock>0);
    int32_t retVal=wallClock;
    if(t!=(VAddr)0){
      if(!context->canWrite(t,sizeof(int32_t)))
	return sysCall32SetErr(context,Mips32_EFAULT);
      context->writeMem(t,retVal);
    }
    return sysCall32SetRet(context,retVal);
  }
  
  bool operator==(const Mips32_tms &var1, const Mips32_tms &var2){
    return
      (var1.tms_utime==var2.tms_utime)&&(var1.tms_stime==var2.tms_stime)&&
      (var1.tms_cutime==var2.tms_cutime)&&(var1.tms_cstime==var2.tms_cstime);
  }
  void sysCall32_times(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr buf=myArgs.getA();
    Mips32_tms myTms;
    if(!context->canWrite(buf,sizeof(myTms)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    // TODO: This is a hack. See above.
    myUsrUsecs+=100;
    mySysUsecs+=1;
    
    myTms.tms_utime=myTms.tms_cutime=myUsrUsecs/1000;
    myTms.tms_stime=myTms.tms_cstime=mySysUsecs/1000;
    Mips32_clock_t retVal=(myUsrUsecs+mySysUsecs)/1000;
    context->writeMem(buf,myTms);
    return sysCall32SetRet(context,retVal);
  }

  void sysCall32_unused59(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_unused59: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_reserved82(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_reserved82: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_unused109(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_unused109: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_unused150(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_unused150: not implemented at 0x%08x\n",context->getIAddr());
  }

  // Miscaleneous and unsorted
  
  void sysCall32_gettimeofday(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr tv=myArgs.getA();
    VAddr tz=myArgs.getA();
    if(tz)
      fail("sysCall32_gettimeofday tz param is not null\n");
    if(!context->canWrite(tv,sizeof(Mips32_timeval)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    Mips32_timeval tvBuf;
    tvBuf.tv_sec=15;
    tvBuf.tv_usec=0;
    context->writeMem(tv,tvBuf);
    sysCall32SetRet(context,0);
  }
  void sysCall32_settimeofday(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_settimeofday: not implemented at 0x%08x\n",context->getIAddr());
  }
  
  void sysCall32_uselib(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_uselib: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sysinfo(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sysinfo: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_uname(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr buf = myArgs.getA();
    int retVal=-1;
    int errNum=0;
    Mips32_utsname myUtsname;
    if(!context->canWrite(buf,sizeof(myUtsname)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    memset(&myUtsname,0,sizeof(myUtsname));
    strcpy(myUtsname.sysname,"SescLinux");
    strcpy(myUtsname.nodename,"sesc");
    strcpy(myUtsname.release,"2.4.18");
    strcpy(myUtsname.version,"#1 SMP Tue Jun 4 16:05:29 CDT 2002");
    strcpy(myUtsname.machine,"mips");
    strcpy(myUtsname.domainname,"Sesc");
    context->writeMemFromBuf(buf,sizeof(myUtsname),&myUtsname);
    return sysCall32SetRet(context,0);
  }
  void sysCall32_sysfs(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sysfs: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_personality(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_personality: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_cachectl(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_cachectl: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sysmips(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sysmips: not implemented at 0x%08x\n",context->getIAddr());
  }

  void sysCall32__sysctl(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr args = myArgs.getA();
    Mips32___sysctl_args mySysctlArgs;
    if(!context->canRead(args,sizeof(mySysctlArgs)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    context->readMem(args,mySysctlArgs);
    int32_t name[mySysctlArgs.nlen];
    I(sizeof(name)==mySysctlArgs.nlen*sizeof(int32_t));
    if(!context->canRead(mySysctlArgs.name,sizeof(name)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    for(int i=0;i<mySysctlArgs.nlen;i++)
      context->readMem(mySysctlArgs.name+i*sizeof(int32_t),name[i]);
    switch(name[0]){
    case Mips32_CTL_KERN:
      if(mySysctlArgs.nlen<=1)
	return sysCall32SetErr(context,Mips32_ENOTDIR);
      switch(name[1]){
      case Mips32_KERN_VERSION:
	{
	  if(mySysctlArgs.newval!=0)
	    return sysCall32SetErr(context,Mips32_EPERM);
	  Mips32_VSize oldlen=0xDEADBEEF;
	  if(!context->canRead(mySysctlArgs.oldlenp,sizeof(oldlen)))
	    return sysCall32SetErr(context,Mips32_EFAULT);
	  context->readMem(mySysctlArgs.oldlenp,oldlen);
	  char ver[]="#1 SMP Tue Jun 4 16:05:29 CDT 2002";
	  size_t verlen=strlen(ver)+1;
	  if(oldlen<verlen)
	    return sysCall32SetErr(context,Mips32_EFAULT);
	  if(!context->canWrite(mySysctlArgs.oldlenp,sizeof(oldlen)))
	    return sysCall32SetErr(context,Mips32_EFAULT);
	  if(!context->canWrite(mySysctlArgs.oldval,verlen))
	    return sysCall32SetErr(context,Mips32_EFAULT);
	  context->writeMemFromBuf(mySysctlArgs.oldval,verlen,ver);
	  context->writeMem(mySysctlArgs.oldlenp,verlen);
	}
	break;
      default:
	fail("sysCall32__sysctl: KERN name %d not supported\n",name[0]);
	return sysCall32SetErr(context,Mips32_ENOTDIR);
      }
      break;
    defualt:
      fail("sysCall32__sysctl: top-level name %d not supported\n",name[0]);
      return sysCall32SetErr(context,Mips32_ENOTDIR);
    }
    return sysCall32SetRet(context,0);
  }

  void sysCall32_sched_setparam(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_setparam: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_getparam(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_getparam: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_setscheduler(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_setscheduler: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_getscheduler(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_getscheduler: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_yield(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_yield: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_get_priority_max(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_get_priority_max: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_get_priority_min(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_get_priority_min: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sched_rr_get_interval(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sched_rr_get_interval: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_nanosleep(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr req=myArgs.getA();
    VAddr rem=myArgs.getA();
    if(!context->canWrite(req,sizeof(Mips32_timespec)))
      return sysCall32SetErr(context,Mips32_EFAULT);
    if(rem&&(!context->canWrite(rem,sizeof(Mips32_timespec))))
      return sysCall32SetErr(context,Mips32_EFAULT);
    Mips32_timespec tsBuf;
    context->readMem(req,tsBuf);
    // TODO: We need to actually suspend this thread for the specified time
    wallClock+=tsBuf.tv_sec;
    if(rem){
      tsBuf.tv_sec=0;
      tsBuf.tv_nsec=0;
      context->writeMem(req,tsBuf);
    }
    sysCall32SetRet(context,0);
  }
  void sysCall32_accept(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_accept: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_bind(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_bind: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_connect(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_connect: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_getpeername(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getpeername: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_getsockname(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getsockname: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_getsockopt(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_getsockopt: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_listen(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_listen: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_recv(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_recv: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_recvfrom(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_recvfrom: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_recvmsg(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_recvmsg: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_send(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_send: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_sendmsg(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sendmsg: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_sendto(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sendto: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_setsockopt(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_setsockopt: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_shutdown(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_shutdown: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_socket(InstDesc *inst, ThreadContext *context){
    printf("sysCall32_socket: not implemented at 0x%08x\n",context->getIAddr());
    Mips::sysCall32SetErr(context,Mips32_EACCES);
  }
  void sysCall32_socketpair(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_socketpair: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_query_module(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_query_module: not implemented at 0x%08x\n",context->getIAddr()); }

  void sysCall32_poll(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    Mips32_VAddr ufds=myArgs.getA();
    uint32_t     nfds=myArgs.getW();
    int32_t      timeout=myArgs.getW();
    Mips32_pollfd myUfds[nfds];
    if((!context->canRead(ufds,sizeof(myUfds)))||(!context->canWrite(ufds,sizeof(myUfds))))
      return sysCall32SetErr(context,Mips32_EFAULT);
    size_t retCnt=0;
    for(size_t i=0;i<nfds;i++){
      context->readMem(ufds+i*sizeof(Mips32_pollfd),myUfds[i]);
      if(!context->getOpenFiles()->isOpen(myUfds[i].fd)){
        myUfds[i].revents=Mips32_POLLNVAL;
      }else if((myUfds[i].events&Mips32_POLLIN)&&(!context->getOpenFiles()->willReadBlock(myUfds[i].fd))){
        myUfds[i].revents=Mips32_POLLIN;
      }else
        continue;
      context->writeMem(ufds+i*sizeof(Mips32_pollfd),myUfds[i]);
      retCnt++;
    }
    // If any of the files could be read withut blocking, we're done
    if(retCnt!=0)
      return sysCall32SetRet(context,retCnt);
    // We need to block and wait
    // Enable SigIO
    SignalSet newMask=context->getSignalMask();
    if(newMask.test(SigIO))
      fail("sysCall32_read: SigIO masked out, not supported\n");
//     newMask.reset(SigIO);
//     sstate->pushMask(newMask);
    if(context->hasReadySignal()){
      sysCall32SetErr(context,Mips32_EINTR);
      SigInfo *sigInfo=context->nextReadySignal();
//       sstate->popMask();
      handleSignal(context,sigInfo);
      return;
    }
    // Set up a callback, add a read block on each file, and suspend
//     sstate->setWakeup(new SigCallBack(context->getPid(),true));
    I(context->canRead(ufds,sizeof(myUfds)));
#if (defined DEBUG_SIGNALS)
    printf("Suspend %d in sysCall32_poll(fds=",context->getPid());
#endif
    for(uint32_t i=0;i<nfds;i++){
      context->readMem(ufds+i*sizeof(Mips32_pollfd),myUfds[i]);
#if (defined DEBUG_SIGNALS)
      printf(" %d",myUfds[i].fd);
#endif
      context->getOpenFiles()->addReadBlock(myUfds[i].fd,context->getPid());
    }
#if (defined DEBUG_SIGNALS)
    printf(")\n");
    context->dumpCallStack();
    printf("Also suspended:");
    for(PidSet::iterator suspIt=suspSet.begin();suspIt!=suspSet.end();suspIt++)
      printf(" %d",*suspIt);
    printf("\n");
    suspSet.insert(context->getPid());
#endif
    context->suspend();
  }

  void sysCall32_nfsservctl(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_nfsservctl: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_prctl(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_prctl: not implemented at 0x%08x\n",context->getIAddr()); }
  void sysCall32_getcwd(InstDesc *inst, ThreadContext *context){
    Mips32FuncArgs myArgs(context);
    VAddr  buf  = myArgs.getA();
    size_t size = myArgs.getW();
    char realBuf[size];
    if(!getcwd(realBuf,size))
      return sysCall32SetErr(context,fromNativeErrNums(errno));
    int retVal=strlen(realBuf)+1;
    if(!context->canWrite(buf,retVal))
      return sysCall32SetErr(context,Mips32_EFAULT);
    context->writeMemFromBuf(buf,retVal,realBuf);
    return sysCall32SetRet(context,retVal);
  }
  void sysCall32_capget(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_capget: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_capset(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_capset: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sigaltstack(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sigaltstack: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_sendfile(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_sendfile: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_root_pivot(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_root_pivot: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_mincore(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_mincore: not implemented at 0x%08x\n",context->getIAddr());
  }
  void sysCall32_madvise(InstDesc *inst, ThreadContext *context){
    fail("sysCall32_madvise: not implemented at 0x%08x\n",context->getIAddr());
  }

} // Namespace Mips
