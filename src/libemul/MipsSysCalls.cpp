#include <sys/stat.h>
#include <fcntl.h>
#include "MipsSysCalls.h"
#include "ThreadContext.h"
#include "MipsRegs.h"
// To get definition of fail()
#include "EmulInit.h"

#include "Mips32Defs.h"

void sysCallMips32_syscall(InstDesc *inst, ThreadContext *context);
void sysCallMips32_exit(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fork(InstDesc *inst, ThreadContext *context);
void sysCallMips32_read(InstDesc *inst, ThreadContext *context);
void sysCallMips32_write(InstDesc *inst, ThreadContext *context);
void sysCallMips32_open(InstDesc *inst, ThreadContext *context);
void sysCallMips32_close(InstDesc *inst, ThreadContext *context);
void sysCallMips32_waitpid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_creat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_link(InstDesc *inst, ThreadContext *context);
void sysCallMips32_unlink(InstDesc *inst, ThreadContext *context);
void sysCallMips32_execve(InstDesc *inst, ThreadContext *context);
void sysCallMips32_chdir(InstDesc *inst, ThreadContext *context);
void sysCallMips32_time(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mknod(InstDesc *inst, ThreadContext *context);
void sysCallMips32_chmod(InstDesc *inst, ThreadContext *context);
void sysCallMips32_lchown(InstDesc *inst, ThreadContext *context);
void sysCallMips32_break(InstDesc *inst, ThreadContext *context);
void sysCallMips32_oldstat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_lseek(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getpid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mount(InstDesc *inst, ThreadContext *context);
void sysCallMips32_umount(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_stime(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ptrace(InstDesc *inst, ThreadContext *context);
void sysCallMips32_alarm(InstDesc *inst, ThreadContext *context);
void sysCallMips32_oldfstat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_pause(InstDesc *inst, ThreadContext *context);
void sysCallMips32_utime(InstDesc *inst, ThreadContext *context);
void sysCallMips32_stty(InstDesc *inst, ThreadContext *context);
void sysCallMips32_gtty(InstDesc *inst, ThreadContext *context);
void sysCallMips32_access(InstDesc *inst, ThreadContext *context);
void sysCallMips32_nice(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ftime(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sync(InstDesc *inst, ThreadContext *context);
void sysCallMips32_kill(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rename(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mkdir(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rmdir(InstDesc *inst, ThreadContext *context);
void sysCallMips32_dup(InstDesc *inst, ThreadContext *context);
void sysCallMips32_pipe(InstDesc *inst, ThreadContext *context);
void sysCallMips32_times(InstDesc *inst, ThreadContext *context);
void sysCallMips32_prof(InstDesc *inst, ThreadContext *context);
void sysCallMips32_brk(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_signal(InstDesc *inst, ThreadContext *context);
void sysCallMips32_geteuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getegid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_acct(InstDesc *inst, ThreadContext *context);
void sysCallMips32_umount2(InstDesc *inst, ThreadContext *context);
void sysCallMips32_lock(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ioctl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fcntl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mpx(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setpgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ulimit(InstDesc *inst, ThreadContext *context);
void sysCallMips32_unused59(InstDesc *inst, ThreadContext *context);
void sysCallMips32_umask(InstDesc *inst, ThreadContext *context);
void sysCallMips32_chroot(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ustat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_dup2(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getppid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getpgrp(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setsid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sigaction(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sgetmask(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ssetmask(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setreuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setregid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sigsuspend(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sigpending(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sethostname(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setrlimit(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getrlimit(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getrusage(InstDesc *inst, ThreadContext *context);
void sysCallMips32_gettimeofday(InstDesc *inst, ThreadContext *context);
void sysCallMips32_settimeofday(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getgroups(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setgroups(InstDesc *inst, ThreadContext *context);
void sysCallMips32_reserved82(InstDesc *inst, ThreadContext *context);
void sysCallMips32_symlink(InstDesc *inst, ThreadContext *context);
void sysCallMips32_oldlstat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_readlink(InstDesc *inst, ThreadContext *context);
void sysCallMips32_uselib(InstDesc *inst, ThreadContext *context);
void sysCallMips32_swapon(InstDesc *inst, ThreadContext *context);
void sysCallMips32_reboot(InstDesc *inst, ThreadContext *context);
void sysCallMips32_readdir(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mmap(InstDesc *inst, ThreadContext *context);
void sysCallMips32_munmap(InstDesc *inst, ThreadContext *context);
void sysCallMips32_truncate(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ftruncate(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fchmod(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fchown(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getpriority(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setpriority(InstDesc *inst, ThreadContext *context);
void sysCallMips32_profil(InstDesc *inst, ThreadContext *context);
void sysCallMips32_statfs(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fstatfs(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ioperm(InstDesc *inst, ThreadContext *context);
void sysCallMips32_socketcall(InstDesc *inst, ThreadContext *context);
void sysCallMips32_syslog(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setitimer(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getitimer(InstDesc *inst, ThreadContext *context);
void sysCallMips32_stat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_lstat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fstat(InstDesc *inst, ThreadContext *context);
void sysCallMips32_unused109(InstDesc *inst, ThreadContext *context);
void sysCallMips32_iopl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_vhangup(InstDesc *inst, ThreadContext *context);
void sysCallMips32_idle(InstDesc *inst, ThreadContext *context);
void sysCallMips32_vm86(InstDesc *inst, ThreadContext *context);
void sysCallMips32_wait4(InstDesc *inst, ThreadContext *context);
void sysCallMips32_swapoff(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sysinfo(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ipc(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fsync(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sigreturn(InstDesc *inst, ThreadContext *context);
void sysCallMips32_clone(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setdomainname(InstDesc *inst, ThreadContext *context);
void sysCallMips32_uname(InstDesc *inst, ThreadContext *context);
void sysCallMips32_modify_ldt(InstDesc *inst, ThreadContext *context);
void sysCallMips32_adjtimex(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mprotect(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sigprocmask(InstDesc *inst, ThreadContext *context);
void sysCallMips32_create_module(InstDesc *inst, ThreadContext *context);
void sysCallMips32_init_module(InstDesc *inst, ThreadContext *context);
void sysCallMips32_delete_module(InstDesc *inst, ThreadContext *context);
void sysCallMips32_get_kernel_syms(InstDesc *inst, ThreadContext *context);
void sysCallMips32_quotactl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getpgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fchdir(InstDesc *inst, ThreadContext *context);
void sysCallMips32_bdflush(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sysfs(InstDesc *inst, ThreadContext *context);
void sysCallMips32_personality(InstDesc *inst, ThreadContext *context);
void sysCallMips32_afs_syscall(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setfsuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setfsgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32__llseek(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getdents(InstDesc *inst, ThreadContext *context);
void sysCallMips32__newselect(InstDesc *inst, ThreadContext *context);
void sysCallMips32_flock(InstDesc *inst, ThreadContext *context);
void sysCallMips32_msync(InstDesc *inst, ThreadContext *context);
void sysCallMips32_readv(InstDesc *inst, ThreadContext *context);
void sysCallMips32_writev(InstDesc *inst, ThreadContext *context);
void sysCallMips32_cacheflush(InstDesc *inst, ThreadContext *context);
void sysCallMips32_cachectl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sysmips(InstDesc *inst, ThreadContext *context);
void sysCallMips32_unused150(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getsid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fdatasync(InstDesc *inst, ThreadContext *context);
void sysCallMips32__sysctl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mlock(InstDesc *inst, ThreadContext *context);
void sysCallMips32_munlock(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mlockall(InstDesc *inst, ThreadContext *context);
void sysCallMips32_munlockall(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_setparam(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_getparam(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_setscheduler(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_getscheduler(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_yield(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_get_priority_max(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_get_priority_min(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sched_rr_get_interval(InstDesc *inst, ThreadContext *context);
void sysCallMips32_nanosleep(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mremap(InstDesc *inst, ThreadContext *context);
void sysCallMips32_accept(InstDesc *inst, ThreadContext *context);
void sysCallMips32_bind(InstDesc *inst, ThreadContext *context);
void sysCallMips32_connect(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getpeername(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getsockname(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getsockopt(InstDesc *inst, ThreadContext *context);
void sysCallMips32_listen(InstDesc *inst, ThreadContext *context);
void sysCallMips32_recv(InstDesc *inst, ThreadContext *context);
void sysCallMips32_recvfrom(InstDesc *inst, ThreadContext *context);
void sysCallMips32_recvmsg(InstDesc *inst, ThreadContext *context);
void sysCallMips32_send(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sendmsg(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sendto(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setsockopt(InstDesc *inst, ThreadContext *context);
void sysCallMips32_shutdown(InstDesc *inst, ThreadContext *context);
void sysCallMips32_socket(InstDesc *inst, ThreadContext *context);
void sysCallMips32_socketpair(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setresuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getresuid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_query_module(InstDesc *inst, ThreadContext *context);
void sysCallMips32_poll(InstDesc *inst, ThreadContext *context);
void sysCallMips32_nfsservctl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_setresgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getresgid(InstDesc *inst, ThreadContext *context);
void sysCallMips32_prctl(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigreturn(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigaction(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigprocmask(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigpending(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigtimedwait(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigqueueinfo(InstDesc *inst, ThreadContext *context);
void sysCallMips32_rt_sigsuspend(InstDesc *inst, ThreadContext *context);
void sysCallMips32_pread(InstDesc *inst, ThreadContext *context);
void sysCallMips32_pwrite(InstDesc *inst, ThreadContext *context);
void sysCallMips32_chown(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getcwd(InstDesc *inst, ThreadContext *context);
void sysCallMips32_capget(InstDesc *inst, ThreadContext *context);
void sysCallMips32_capset(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sigaltstack(InstDesc *inst, ThreadContext *context);
void sysCallMips32_sendfile(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getpmsg(InstDesc *inst, ThreadContext *context);
void sysCallMips32_putpmsg(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mmap2(InstDesc *inst, ThreadContext *context);
void sysCallMips32_truncate64(InstDesc *inst, ThreadContext *context);
void sysCallMips32_ftruncate64(InstDesc *inst, ThreadContext *context);
void sysCallMips32_stat64(InstDesc *inst, ThreadContext *context);
void sysCallMips32_lstat64(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fstat64(InstDesc *inst, ThreadContext *context);
void sysCallMips32_root_pivot(InstDesc *inst, ThreadContext *context);
void sysCallMips32_mincore(InstDesc *inst, ThreadContext *context);
void sysCallMips32_madvise(InstDesc *inst, ThreadContext *context);
void sysCallMips32_getdents64(InstDesc *inst, ThreadContext *context);
void sysCallMips32_fcntl64(InstDesc *inst, ThreadContext *context);

void mipsSysCall(InstDesc *inst, ThreadContext *context){
  uint32_t sysCallNum;
  switch(context->getMode()){
  case Mips32:
    sysCallNum=Mips::getReg<uint32_t>(context,static_cast<RegName>(Mips::RegV0));
    break;
  case Mips64:
    fail("mipsSysCall: Mips64 system calls not implemented yet\n");
    break;
  }
  switch(sysCallNum){
  case 4000: sysCallMips32_syscall(inst,context); break;
  case 4001: sysCallMips32_exit(inst,context); break;
  case 4002: sysCallMips32_fork(inst,context); break;
  case 4003: sysCallMips32_read(inst,context); break;
  case 4004: sysCallMips32_write(inst,context); break;
  case 4005: sysCallMips32_open(inst,context); break;
  case 4006: sysCallMips32_close(inst,context); break;
  case 4007: sysCallMips32_waitpid(inst,context); break;
  case 4008: sysCallMips32_creat(inst,context); break;
  case 4009: sysCallMips32_link(inst,context); break;
  case 4010: sysCallMips32_unlink(inst,context); break;
  case 4011: sysCallMips32_execve(inst,context); break;
  case 4012: sysCallMips32_chdir(inst,context); break;
  case 4013: sysCallMips32_time(inst,context); break;
  case 4014: sysCallMips32_mknod(inst,context); break;
  case 4015: sysCallMips32_chmod(inst,context); break;
  case 4016: sysCallMips32_lchown(inst,context); break;
  case 4017: sysCallMips32_break(inst,context); break;
  case 4018: sysCallMips32_oldstat(inst,context); break;
  case 4019: sysCallMips32_lseek(inst,context); break;
  case 4020: sysCallMips32_getpid(inst,context); break;
  case 4021: sysCallMips32_mount(inst,context); break;
  case 4022: sysCallMips32_umount(inst,context); break;
  case 4023: sysCallMips32_setuid(inst,context); break;
  case 4024: sysCallMips32_getuid(inst,context); break;
  case 4025: sysCallMips32_stime(inst,context); break;
  case 4026: sysCallMips32_ptrace(inst,context); break;
  case 4027: sysCallMips32_alarm(inst,context); break;
  case 4028: sysCallMips32_oldfstat(inst,context); break;
  case 4029: sysCallMips32_pause(inst,context); break;
  case 4030: sysCallMips32_utime(inst,context); break;
  case 4031: sysCallMips32_stty(inst,context); break;
  case 4032: sysCallMips32_gtty(inst,context); break;
  case 4033: sysCallMips32_access(inst,context); break;
  case 4034: sysCallMips32_nice(inst,context); break;
  case 4035: sysCallMips32_ftime(inst,context); break;
  case 4036: sysCallMips32_sync(inst,context); break;
  case 4037: sysCallMips32_kill(inst,context); break;
  case 4038: sysCallMips32_rename(inst,context); break;
  case 4039: sysCallMips32_mkdir(inst,context); break;
  case 4040: sysCallMips32_rmdir(inst,context); break;
  case 4041: sysCallMips32_dup(inst,context); break;
  case 4042: sysCallMips32_pipe(inst,context); break;
  case 4043: sysCallMips32_times(inst,context); break;
  case 4044: sysCallMips32_prof(inst,context); break;
  case 4045: sysCallMips32_brk(inst,context); break;
  case 4046: sysCallMips32_setgid(inst,context); break;
  case 4047: sysCallMips32_getgid(inst,context); break;
  case 4048: sysCallMips32_signal(inst,context); break;
  case 4049: sysCallMips32_geteuid(inst,context); break;
  case 4050: sysCallMips32_getegid(inst,context); break;
  case 4051: sysCallMips32_acct(inst,context); break;
  case 4052: sysCallMips32_umount2(inst,context); break;
  case 4053: sysCallMips32_lock(inst,context); break;
  case 4054: sysCallMips32_ioctl(inst,context); break;
  case 4055: sysCallMips32_fcntl(inst,context); break;
  case 4056: sysCallMips32_mpx(inst,context); break;
  case 4057: sysCallMips32_setpgid(inst,context); break;
  case 4058: sysCallMips32_ulimit(inst,context); break;
  case 4059: sysCallMips32_unused59(inst,context); break;
  case 4060: sysCallMips32_umask(inst,context); break;
  case 4061: sysCallMips32_chroot(inst,context); break;
  case 4062: sysCallMips32_ustat(inst,context); break;
  case 4063: sysCallMips32_dup2(inst,context); break;
  case 4064: sysCallMips32_getppid(inst,context); break;
  case 4065: sysCallMips32_getpgrp(inst,context); break;
  case 4066: sysCallMips32_setsid(inst,context); break;
  case 4067: sysCallMips32_sigaction(inst,context); break;
  case 4068: sysCallMips32_sgetmask(inst,context); break;
  case 4069: sysCallMips32_ssetmask(inst,context); break;
  case 4070: sysCallMips32_setreuid(inst,context); break;
  case 4071: sysCallMips32_setregid(inst,context); break;
  case 4072: sysCallMips32_sigsuspend(inst,context); break;
  case 4073: sysCallMips32_sigpending(inst,context); break;
  case 4074: sysCallMips32_sethostname(inst,context); break;
  case 4075: sysCallMips32_setrlimit(inst,context); break;
  case 4076: sysCallMips32_getrlimit(inst,context); break;
  case 4077: sysCallMips32_getrusage(inst,context); break;
  case 4078: sysCallMips32_gettimeofday(inst,context); break;
  case 4079: sysCallMips32_settimeofday(inst,context); break;
  case 4080: sysCallMips32_getgroups(inst,context); break;
  case 4081: sysCallMips32_setgroups(inst,context); break;
  case 4082: sysCallMips32_reserved82(inst,context); break;
  case 4083: sysCallMips32_symlink(inst,context); break;
  case 4084: sysCallMips32_oldlstat(inst,context); break;
  case 4085: sysCallMips32_readlink(inst,context); break;
  case 4086: sysCallMips32_uselib(inst,context); break;
  case 4087: sysCallMips32_swapon(inst,context); break;
  case 4088: sysCallMips32_reboot(inst,context); break;
  case 4089: sysCallMips32_readdir(inst,context); break;
  case 4090: sysCallMips32_mmap(inst,context); break;
  case 4091: sysCallMips32_munmap(inst,context); break;
  case 4092: sysCallMips32_truncate(inst,context); break;
  case 4093: sysCallMips32_ftruncate(inst,context); break;
  case 4094: sysCallMips32_fchmod(inst,context); break;
  case 4095: sysCallMips32_fchown(inst,context); break;
  case 4096: sysCallMips32_getpriority(inst,context); break;
  case 4097: sysCallMips32_setpriority(inst,context); break;
  case 4098: sysCallMips32_profil(inst,context); break;
  case 4099: sysCallMips32_statfs(inst,context); break;
  case 4100: sysCallMips32_fstatfs(inst,context); break;
  case 4101: sysCallMips32_ioperm(inst,context); break;
  case 4102: sysCallMips32_socketcall(inst,context); break;
  case 4103: sysCallMips32_syslog(inst,context); break;
  case 4104: sysCallMips32_setitimer(inst,context); break;
  case 4105: sysCallMips32_getitimer(inst,context); break;
  case 4106: sysCallMips32_stat(inst,context); break;
  case 4107: sysCallMips32_lstat(inst,context); break;
  case 4108: sysCallMips32_fstat(inst,context); break;
  case 4109: sysCallMips32_unused109(inst,context); break;
  case 4110: sysCallMips32_iopl(inst,context); break;
  case 4111: sysCallMips32_vhangup(inst,context); break;
  case 4112: sysCallMips32_idle(inst,context); break;
  case 4113: sysCallMips32_vm86(inst,context); break;
  case 4114: sysCallMips32_wait4(inst,context); break;
  case 4115: sysCallMips32_swapoff(inst,context); break;
  case 4116: sysCallMips32_sysinfo(inst,context); break;
  case 4117: sysCallMips32_ipc(inst,context); break;
  case 4118: sysCallMips32_fsync(inst,context); break;
  case 4119: sysCallMips32_sigreturn(inst,context); break;
  case 4120: sysCallMips32_clone(inst,context); break;
  case 4121: sysCallMips32_setdomainname(inst,context); break;
  case 4122: sysCallMips32_uname(inst,context); break;
  case 4123: sysCallMips32_modify_ldt(inst,context); break;
  case 4124: sysCallMips32_adjtimex(inst,context); break;
  case 4125: sysCallMips32_mprotect(inst,context); break;
  case 4126: sysCallMips32_sigprocmask(inst,context); break;
  case 4127: sysCallMips32_create_module(inst,context); break;
  case 4128: sysCallMips32_init_module(inst,context); break;
  case 4129: sysCallMips32_delete_module(inst,context); break;
  case 4130: sysCallMips32_get_kernel_syms(inst,context); break;
  case 4131: sysCallMips32_quotactl(inst,context); break;
  case 4132: sysCallMips32_getpgid(inst,context); break;
  case 4133: sysCallMips32_fchdir(inst,context); break;
  case 4134: sysCallMips32_bdflush(inst,context); break;
  case 4135: sysCallMips32_sysfs(inst,context); break;
  case 4136: sysCallMips32_personality(inst,context); break;
  case 4137: sysCallMips32_afs_syscall(inst,context); break;
  case 4138: sysCallMips32_setfsuid(inst,context); break;
  case 4139: sysCallMips32_setfsgid(inst,context); break;
  case 4140: sysCallMips32__llseek(inst,context); break;
  case 4141: sysCallMips32_getdents(inst,context); break;
  case 4142: sysCallMips32__newselect(inst,context); break;
  case 4143: sysCallMips32_flock(inst,context); break;
  case 4144: sysCallMips32_msync(inst,context); break;
  case 4145: sysCallMips32_readv(inst,context); break;
  case 4146: sysCallMips32_writev(inst,context); break;
  case 4147: sysCallMips32_cacheflush(inst,context); break;
  case 4148: sysCallMips32_cachectl(inst,context); break;
  case 4149: sysCallMips32_sysmips(inst,context); break;
  case 4150: sysCallMips32_unused150(inst,context); break;
  case 4151: sysCallMips32_getsid(inst,context); break;
  case 4152: sysCallMips32_fdatasync(inst,context); break;
  case 4153: sysCallMips32__sysctl(inst,context); break;
  case 4154: sysCallMips32_mlock(inst,context); break;
  case 4155: sysCallMips32_munlock(inst,context); break;
  case 4156: sysCallMips32_mlockall(inst,context); break;
  case 4157: sysCallMips32_munlockall(inst,context); break;
  case 4158: sysCallMips32_sched_setparam(inst,context); break;
  case 4159: sysCallMips32_sched_getparam(inst,context); break;
  case 4160: sysCallMips32_sched_setscheduler(inst,context); break;
  case 4161: sysCallMips32_sched_getscheduler(inst,context); break;
  case 4162: sysCallMips32_sched_yield(inst,context); break;
  case 4163: sysCallMips32_sched_get_priority_max(inst,context); break;
  case 4164: sysCallMips32_sched_get_priority_min(inst,context); break;
  case 4165: sysCallMips32_sched_rr_get_interval(inst,context); break;
  case 4166: sysCallMips32_nanosleep(inst,context); break;
  case 4167: sysCallMips32_mremap(inst,context); break;
  case 4168: sysCallMips32_accept(inst,context); break;
  case 4169: sysCallMips32_bind(inst,context); break;
  case 4170: sysCallMips32_connect(inst,context); break;
  case 4171: sysCallMips32_getpeername(inst,context); break;
  case 4172: sysCallMips32_getsockname(inst,context); break;
  case 4173: sysCallMips32_getsockopt(inst,context); break;
  case 4174: sysCallMips32_listen(inst,context); break;
  case 4175: sysCallMips32_recv(inst,context); break;
  case 4176: sysCallMips32_recvfrom(inst,context); break;
  case 4177: sysCallMips32_recvmsg(inst,context); break;
  case 4178: sysCallMips32_send(inst,context); break;
  case 4179: sysCallMips32_sendmsg(inst,context); break;
  case 4180: sysCallMips32_sendto(inst,context); break;
  case 4181: sysCallMips32_setsockopt(inst,context); break;
  case 4182: sysCallMips32_shutdown(inst,context); break;
  case 4183: sysCallMips32_socket(inst,context); break;
  case 4184: sysCallMips32_socketpair(inst,context); break;
  case 4185: sysCallMips32_setresuid(inst,context); break;
  case 4186: sysCallMips32_getresuid(inst,context); break;
  case 4187: sysCallMips32_query_module(inst,context); break;
  case 4188: sysCallMips32_poll(inst,context); break;
  case 4189: sysCallMips32_nfsservctl(inst,context); break;
  case 4190: sysCallMips32_setresgid(inst,context); break;
  case 4191: sysCallMips32_getresgid(inst,context); break;
  case 4192: sysCallMips32_prctl(inst,context); break;
  case 4193: sysCallMips32_rt_sigreturn(inst,context); break;
  case 4194: sysCallMips32_rt_sigaction(inst,context); break;
  case 4195: sysCallMips32_rt_sigprocmask(inst,context); break;
  case 4196: sysCallMips32_rt_sigpending(inst,context); break;
  case 4197: sysCallMips32_rt_sigtimedwait(inst,context); break;
  case 4198: sysCallMips32_rt_sigqueueinfo(inst,context); break;
  case 4199: sysCallMips32_rt_sigsuspend(inst,context); break;
  case 4200: sysCallMips32_pread(inst,context); break;
  case 4201: sysCallMips32_pwrite(inst,context); break;
  case 4202: sysCallMips32_chown(inst,context); break;
  case 4203: sysCallMips32_getcwd(inst,context); break;
  case 4204: sysCallMips32_capget(inst,context); break;
  case 4205: sysCallMips32_capset(inst,context); break;
  case 4206: sysCallMips32_sigaltstack(inst,context); break;
  case 4207: sysCallMips32_sendfile(inst,context); break;
  case 4208: sysCallMips32_getpmsg(inst,context); break;
  case 4209: sysCallMips32_putpmsg(inst,context); break;
  case 4210: sysCallMips32_mmap2(inst,context); break;
  case 4211: sysCallMips32_truncate64(inst,context); break;
  case 4212: sysCallMips32_ftruncate64(inst,context); break;
  case 4213: sysCallMips32_stat64(inst,context); break;
  case 4214: sysCallMips32_lstat64(inst,context); break;
  case 4215: sysCallMips32_fstat64(inst,context); break;
  case 4216: sysCallMips32_root_pivot(inst,context); break;
  case 4217: sysCallMips32_mincore(inst,context); break;
  case 4218: sysCallMips32_madvise(inst,context); break;
  case 4219: sysCallMips32_getdents64(inst,context); break;
  case 4220: sysCallMips32_fcntl64(inst,context); break;
  default:
    fail("Unknown MIPS syscall %d at 0x%08x\n",sysCallNum,inst->addr);
  }
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
  int32_t retVal; 
  I((curPos%sizeof(retVal))==0);
  if(curPos<16){
    retVal=Mips::getReg<int32_t>(myContext,static_cast<RegName>(Mips::RegA0+curPos/sizeof(retVal)));
  }else{
    myContext->readMem(Mips::getReg<uint32_t>(myContext,static_cast<RegName>(Mips::RegSP))+curPos,retVal);
  }
  curPos+=sizeof(retVal);
  return retVal;
}
int64_t Mips32FuncArgs::getL(void){
  int64_t retVal;
  I((curPos%sizeof(int32_t))==0);
  // Align current position                  
  if(curPos%sizeof(retVal)!=0)
    curPos+=sizeof(int32_t);
  I(curPos%sizeof(retVal)==0);
  if(curPos<16){
    retVal=Mips::getReg<int32_t>(myContext,static_cast<RegName>(Mips::RegA0+curPos/sizeof(int32_t)));
    retVal=(retVal<<32);
    I(!(retVal&0xffffffff));
    retVal|=Mips::getReg<int32_t>(myContext,static_cast<RegName>(Mips::RegA0+curPos/sizeof(int32_t)+1));
  }else{
    myContext->readMem(Mips::getReg<uint32_t>(myContext,static_cast<RegName>(Mips::RegSP))+curPos,retVal);
  }
  curPos+=sizeof(retVal);
  return retVal;
}

void mipsProgArgs(ThreadContext *context, int argc, char **argv, char **envp){
  I(context->getMode()==Mips32);
  // Count the environment variables
  int envc=0;
  while(envp[envc])
    envc++;
  uint32_t regSP=Mips::getReg<uint32_t>(context,static_cast<RegName>(Mips::RegSP));
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
  Mips::setReg<uint32_t>(context,static_cast<RegName>(Mips::RegSP),regSP);
}

// Process control system calls

// TODO: This is a hack to get applications working.
// We need a real user/system time estimator

static uint64_t myUsrUsecs=0;
static uint64_t mySysUsecs=0;
bool operator==(const Mips32_rlimit &rlim1, const Mips32_rlimit &rlim2){
  return (rlim1.rlim_cur==rlim2.rlim_cur)&&(rlim1.rlim_max==rlim2.rlim_max);
}
void sysCallMips32_getrlimit(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int resource=myArgs.getW();
  VAddr rlim=myArgs.getA();
  int   retVal = -1;
  int   errNum = 0;
  Mips32_rlimit res;
  if(!context->canWrite(rlim,sizeof(res))){
    errNum=Mips32_EFAULT;
  }else{
    switch(resource){
    case Mips32_RLIMIT_STACK:
      res.rlim_cur=res.rlim_max=context->getStackSize();
      break;
    default:
      fail("sysCallMips32_getrlimit called for resource %d at 0x%08x\n",resource,context->getNextInstDesc()->addr);
    }
    // DEBUG begin: Remove after done
    Mips32_rlimit res2;
    ID(context->readMem(rlim,res2));
    I(memcmp(&res,&res2,sizeof(res))==0);
    // DEBUG end
    context->writeMem(rlim,res);
    retVal=0;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_setrlimit(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int resource=myArgs.getW();
  VAddr rlim=myArgs.getA();
  int   retVal = -1;
  int   errNum = 0;
  Mips32_rlimit res;
  if(!context->canRead(rlim,sizeof(res))){
    errNum=Mips32_EFAULT;
  }else{
    context->readMem(rlim,res);
    switch(resource){
    case Mips32_RLIMIT_STACK:
      if((res.rlim_cur!=context->getStackSize())||(res.rlim_max!=context->getStackSize()))
	fail("sysCallMips32_setrlimit tries to change stack size (not supported yet)\n");
      break;
    default:
      fail("sysCallMips32_setrlimit called for resource %d at 0x%08x\n",resource,context->getNextInstDesc()->addr);
    }
    retVal=0;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_syscall(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_syscall: not implemented at 0x%08x\n",inst->addr);
}
#include "OSSim.h"
void sysCallMips32_exit(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int status=myArgs.getW();
  osSim->eventExit(context->getPid(),status);
}
void sysCallMips32_fork(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fork: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_waitpid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_waitpid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_execve(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_execve: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ptrace(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ptrace: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_alarm(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_alarm: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_pause(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_pause: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_nice(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_nice: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_signal(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_signal: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_acct(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_acct: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_sgetmask(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sgetmask: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ssetmask(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ssetmask: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_sigsuspend(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sigsuspend: not implemented at 0x%08x\n",inst->addr);
 }
void sysCallMips32_sigpending(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sigpending: not implemented at 0x%08x\n",inst->addr);
}

bool operator==(const Mips32_timeval &var1, const Mips32_timeval &var2){
  return (var1.tv_sec==var2.tv_sec)&&(var1.tv_usec==var2.tv_usec);
}
bool operator==(const Mips32_rusage &rusg1, const Mips32_rusage &rusg2){
  return (rusg1.ru_utime==rusg2.ru_utime)&&(rusg1.ru_stime==rusg2.ru_utime);
}
void sysCallMips32_getrusage(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int     who=myArgs.getW();
  VAddr usage=myArgs.getA();
  int   retVal = -1;
  int   errNum = 0;

  // TODO: This is a hack. See definition of these vars
  myUsrUsecs+=100;
  mySysUsecs+=1;

  Mips32_rusage res;
  if(!context->canWrite(usage,sizeof(res))){
    errNum=Mips32_EFAULT;
  }else{
    memset(&res,0,sizeof(res));
    res.ru_utime.tv_sec =myUsrUsecs/1000000;
    res.ru_utime.tv_usec=myUsrUsecs%1000000;
    res.ru_stime.tv_sec =mySysUsecs/1000000;
    res.ru_stime.tv_usec=mySysUsecs%1000000;
     // Other fields are zeros and need no endian-fixing
    // DEBUG begin: Remove after done
    Mips32_rusage res2;
    ID(context->readMem(usage,res2));
    I(memcmp(&res,&res2,sizeof(res))==0);
    // DEBUG end
    context->writeMem(usage,res);
    retVal=0;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_getpriority(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getpriority: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setpriority(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setpriority: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setitimer(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setitimer: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getitimer(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getitimer: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_wait4(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_wait4: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_sigreturn(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sigreturn: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_clone(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_clone: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_sigprocmask(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sigprocmask: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getpid(InstDesc *inst, ThreadContext *context){
  int errNum=0;
  // TODO: should return the actual process ID
  int retVal=2;
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_getpgid(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_getpgid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setpgid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setpgid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getppid(InstDesc *inst, ThreadContext *context){
  int errNum=0;
  // TODO: should return the actual parent process ID
  int retVal=1;
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_getpgrp(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getpgrp: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getsid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getsid: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_setsid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setsid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_kill(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int    pid = myArgs.getW();
  int    sig = myArgs.getW();
  printf("sysCallMips32Kill: signal %d sent to process %d, not supported\n",sig,pid);
}
void sysCallMips32_rt_sigreturn(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_rt_sigreturn: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sigaction(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sigaction: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_rt_sigaction(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int sig=myArgs.getW();
  printf("Signal number is %d\n",sig);
  // TODO: Signal handling not supported yet, do nothing
  int errNum=0;
  int retVal=0;
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_rt_sigprocmask(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int how=myArgs.getW();
  printf("How is %d\n",how);
  // TODO: Signal handling not supported yet, do nothing
  int errNum=0;
  int retVal=0;
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_rt_sigpending(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_rt_sigpending: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_rt_sigtimedwait(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_rt_sigtimedwait: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_rt_sigqueueinfo(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_rt_sigqueueinfo: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_rt_sigsuspend(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_rt_sigsuspend: not implemented at 0x%08x\n",inst->addr); }

// Process user/group ID management

void sysCallMips32_getuid(InstDesc *inst, ThreadContext *context){
  // TODO: With different users, need to track simulated uid
  int errNum=0;
  int retVal=getuid();
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_geteuid(InstDesc *inst, ThreadContext *context){
  // TODO: With different users, need to track simulated euid
  int errNum=0;
  int retVal=geteuid();
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_getresuid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getresuid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setuid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setuid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setreuid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setreuid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setresuid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setresuid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getgid(InstDesc *inst, ThreadContext *context){
  // TODO: With different users, need to track simulated gid
  int errNum=0;
  int retVal=getgid();
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_getegid(InstDesc *inst, ThreadContext *context){
  // TODO: With different users, need to track simulated egid
  int errNum=0;
  int retVal=getegid();
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_getresgid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getresgid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setgid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setgid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setregid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setregid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setresgid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setresgid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getgroups(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getgroups: not implemented at 0x%08x\n",inst->addr);
}

// Process memory management system calls

void sysCallMips32_brk(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr brkPos=myArgs.getA();
  VAddr retVal=(VAddr)-1;
  int   errNum=0;
  VAddr  brkBase=context->getAddressSpace()->getBrkBase();
  size_t brkSize=context->getAddressSpace()->getSegmentSize(brkBase);
  if(brkPos==0){
    retVal=brkBase+brkSize;
  }else if(brkPos<=brkBase+brkSize){
    if(brkPos<=brkBase)
      fail("sysCallMips32_brk: new break makes data segment size negative\n");
    context->getAddressSpace()->resizeSegment(brkBase,brkPos-brkBase);
    retVal=brkPos;
  }else if(context->getAddressSpace()->isNoSegment(brkBase+brkSize,brkPos-(brkBase+brkSize))){
    context->getAddressSpace()->resizeSegment(brkBase,brkPos-brkBase);
    context->writeMemWithByte(brkBase+brkSize,brkPos-(brkBase+brkSize),0);
    retVal=brkPos;
  }else{
    errNum=Mips32_ENOMEM;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}

void sysCallMips32_mmap(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr  start=myArgs.getA();
  size_t length=myArgs.getW();
  int    prot=myArgs.getW();
  int    flags=myArgs.getW();
  int    fd=myArgs.getW();
  off_t  offset=myArgs.getW();
  VAddr  retVal=0;
  int    errNum=0;
  if(!(prot&Mips32_PROT_READ))
    fail("sysCallMips32_mmap: mapping without PROT_READ not supported\n");
  if(flags&Mips32_MAP_FIXED)
    fail("sysCallMips32Mmap: MAP_FIXED not supported\n");
  I((flags&Mips32_MAP_SHARED)||(flags&Mips32_MAP_PRIVATE));
  I(!((flags&Mips32_MAP_SHARED)&&(flags&Mips32_MAP_PRIVATE)));
  if(flags&Mips32_MAP_ANONYMOUS){
    if(start&&context->getAddressSpace()->isNoSegment(start,length)){
      retVal=start;
    }else{
      retVal=context->getAddressSpace()->newSegmentAddr(length);
    }
    if(retVal){
      context->getAddressSpace()->newSegment(retVal,length,true,prot&Mips32_PROT_WRITE,prot&Mips32_PROT_EXEC,flags&Mips32_MAP_SHARED);
      context->writeMemWithByte(retVal,length,0);
    }else{
      retVal=(VAddr)-1;
      errNum=Mips32_ENOMEM;
    }
  }else{
    fail("sysCallMips32Mmap: Can't map real files yet\n");
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_mremap(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr  oldaddr=myArgs.getA();
  size_t oldsize=myArgs.getW();
  size_t newsize=myArgs.getW();
  int    flags  =myArgs.getW();
  VAddr retVal=0;
  int   errNum=0;
  AddressSpace *addrSpace=context->getAddressSpace();
  if(!addrSpace->isSegment(oldaddr,oldsize))
    fail("sysCallMips32_mremap: old range not a segment\n");
  if(newsize==oldsize){
    retVal=oldaddr;
  }else if(newsize>oldsize){
    if(addrSpace->isNoSegment(oldaddr+oldsize,newsize-oldsize)){
      retVal=oldaddr;
    }else if(flags&Mips32_MREMAP_MAYMOVE){
      retVal=addrSpace->newSegmentAddr(newsize);
      if(retVal)
	addrSpace->moveSegment(oldaddr,retVal);
    }
    if(retVal){
      addrSpace->resizeSegment(retVal,newsize);
      context->writeMemWithByte(retVal+oldsize,newsize-oldsize,0);      
    }else{
      errNum=Mips32_ENOMEM;
      retVal=(VAddr)-1;
    }
  }else{
    I(newsize<oldsize);
    addrSpace->resizeSegment(oldaddr,newsize);
    retVal=oldaddr;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_munmap(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr  start=myArgs.getA();
  size_t length=myArgs.getW();
  //  printf("munmap from 0x%08x to 0x%08lx\n",start,start+length);
  context->getAddressSpace()->deleteSegment(start,length);
  int errNum=0;
  int retVal=0;
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_mprotect(InstDesc *inst, ThreadContext *context){
  // TODO: It does nothing now, implement actual protections
  int errNum=0;
  int retVal=0;
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_msync(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_msync: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_cacheflush(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_cacheflush: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_mlock(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_mlock: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_munlock(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_munlock: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_mlockall(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_mlockall: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_munlockall(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_munlockall: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_mmap2(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_mmap2: not implemented at 0x%08x\n",inst->addr); }

// File system calls

void sysCallMips32_open(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr pathname  = myArgs.getA();
  int   flags     = myArgs.getW();
  int   mode      = myArgs.getW();  
  int   retVal = -1;
  int   errNum = 0;
  ssize_t pathnameLen=context->readMemString(pathname,0,0);
  if(pathnameLen==-1){
    errNum=Mips32_EFAULT;
  }else{
    char pathnameBuf[pathnameLen];
    size_t pathnameLenChk=context->readMemString(pathname,pathnameLen,pathnameBuf);
    I(pathnameLenChk==(size_t)pathnameLen);
    I(pathnameLenChk==strlen(pathnameBuf)+1);
    // Convert flags
    int natFlags=toNativeOpenFlags(flags);
    // Do the actual call
    retVal=open(pathnameBuf,natFlags,mode);
    if(retVal==-1){
      errNum=fromNativeErrNums(errno);
    }else{
      retVal=context->getAddressSpace()->newSimFd(retVal);
      I(retVal!=-1);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_creat(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_creat: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_dup(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int simfd=myArgs.getW();
  int  retVal=-1;
  int  errNum=0;
  int fd=context->getAddressSpace()->getRealFd(simfd);
  if(fd==-1){
    errNum=Mips32_EBADF;
  }else{
    retVal=dup(fd);
    if(retVal==-1){
      switch(errno){
      default: fail("sysCallMips32_dup: Unknown error %d\n",errNum);  
      }
    }else{
      retVal=context->getAddressSpace()->newSimFd(retVal);
      I(retVal!=-1);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_close(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int  simfd = myArgs.getW();
  int  retVal=-1;
  int  errNum=0;
  int fd=context->getAddressSpace()->getRealFd(simfd);
  if(fd==-1){
    errNum=Mips32_EBADF;
  }else{
    retVal=close(fd);
    if(retVal==-1){
      switch(errno){
      default: fail("sysCallMips32_close: Unknown error %d\n",errNum);  
      }
    }else{
      context->getAddressSpace()->delSimFd(simfd);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_read(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int    fd    = myArgs.getW();
  VAddr  buf   = myArgs.getA();
  int    count = myArgs.getW();
  ssize_t retVal=-1;
  int     errNum=0;
  fd=context->getAddressSpace()->getRealFd(fd);
  if(fd==-1){      
    errNum=Mips32_EBADF;              
  }else if(!context->canWrite(buf,count)){
    errNum=Mips32_EFAULT;
  }else{
    retVal=context->writeMemFromFile(buf,count,fd);
    if(retVal==-1){
      switch(errno){
      default: fail("sysCallMips32_read: Unknown error %d\n",errNum);
      }
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_write(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int    simfd = myArgs.getW();
  VAddr  buf   = myArgs.getA();
  int    count = myArgs.getW();
  ssize_t retVal=-1;
  int     errNum=0;
  int fd=context->getAddressSpace()->getRealFd(simfd);
  if(fd==-1){      
    errNum=Mips32_EBADF;              
  }else if(!context->canRead(buf,count)){
    errNum=Mips32_EFAULT;
  }else{
    retVal=context->readMemToFile(buf,count,fd);
    if(retVal==-1){
      switch(errno){
      default: fail("sysCallMips32_read: Unknown error %d\n",errNum);
      }
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}

void sysCallMips32_readv(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_readv: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_writev(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int    simfd  =myArgs.getW();
  VAddr  vector =myArgs.getA();
  int    count  =myArgs.getW();
  ssize_t retVal=-1;
  int     errNum=0;
  // Get the vector from the simulated address space
  Mips32_iovec myIovec[count];
  I(sizeof(myIovec)==sizeof(Mips32_iovec)*count);
  int fd=context->getAddressSpace()->getRealFd(simfd);
  if(fd==-1){      
    errNum=Mips32_EBADF;              
  }else if(!context->canRead(vector,sizeof(myIovec))){
    errNum=Mips32_EFAULT;
  }else{
    context->readMemToBuf(vector,sizeof(myIovec),myIovec);
    size_t totBytes=0;
    for(int i=0;i<count;i++){
      cvtEndianBig(myIovec[i]);
      VAddr  base = myIovec[i].iov_base;
      size_t len  = myIovec[i].iov_len;
      if(!context->canRead(base,len)){
	errNum=Mips32_EFAULT; break;
      }else{
	ssize_t nowBytes=context->readMemToFile(base,len,fd);
	if(nowBytes==-1){
	  switch(errno){
	  default: fail("sysCallMips32_writev: Unknown error %d\n",errNum);
	  }
	  break;
	}else{
	  totBytes+=nowBytes;
	}
      }
    }
    if(!errNum)
      retVal=(ssize_t)totBytes;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_pread(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_pread: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_pwrite(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_pwrite: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_lseek(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int   fd     = myArgs.getW();
  off_t offset = myArgs.getW();
  int   whence = myArgs.getW();
  off_t retVal=(off_t)-1;
  int   errNum=0;
  fd=context->getAddressSpace()->getRealFd(fd);
  if(fd==-1){      
    errNum=Mips32_EBADF;
  }else{
    retVal=lseek(fd,offset,whence);
    if(retVal==(off_t)-1){
      errNum=fromNativeErrNums(errno);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32__llseek(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int      fd     = myArgs.getW();
  uint32_t hi     = myArgs.getW();
  uint32_t lo     = myArgs.getW();
  VAddr    res    = myArgs.getA();
  int      whence = myArgs.getW();
  int retVal=-1;
  int errNum=0;
  fd=context->getAddressSpace()->getRealFd(fd);
  if(fd==-1){      
    errNum=Mips32_EBADF;              
  }else{
    int64_t offs = (int64_t)((((uint64_t)hi)<<32)|((uint64_t)lo));
    off_t resVal=lseek(fd,offs,whence);
    if(resVal==(off_t)-1){
      errNum=fromNativeErrNums(errno);
    }else{
      retVal=0;
      context->writeMem<uint64_t>(res,resVal);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_link(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_link: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_unlink(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr pathname  = myArgs.getA();
  int   retVal = -1;
  int   errNum = 0;
  ssize_t pathnameLen=context->readMemString(pathname,0,0);
  if(pathnameLen==-1){
    errNum=Mips32_EFAULT;
  }else{
    char pathnameBuf[pathnameLen];
    size_t pathnameLenChk=context->readMemString(pathname,pathnameLen,pathnameBuf);
    I(pathnameLenChk==(size_t)pathnameLen);
    I(pathnameLenChk==strlen(pathnameBuf)+1);
    retVal=unlink(pathnameBuf);
    if(retVal==-1){
      errNum=fromNativeErrNums(errno);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_rename(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr oldpath  = myArgs.getA();
  VAddr newpath  = myArgs.getA();
  int   retVal = -1;
  int   errNum = 0;
  ssize_t oldpathLen=context->readMemString(oldpath,0,0);
  ssize_t newpathLen=context->readMemString(newpath,0,0);
  if((oldpathLen==-1)||(newpathLen==-1)){
    errNum=Mips32_EFAULT;
  }else{
    char oldpathBuf[oldpathLen];
    char newpathBuf[newpathLen];
    size_t oldpathLenChk=context->readMemString(oldpath,oldpathLen,oldpathBuf);
    size_t newpathLenChk=context->readMemString(newpath,newpathLen,newpathBuf);
    I(oldpathLenChk==(size_t)oldpathLen);
    I(oldpathLenChk==strlen(oldpathBuf)+1);
    I(newpathLenChk==(size_t)newpathLen);
    I(newpathLenChk==strlen(newpathBuf)+1);
    retVal=rename(oldpathBuf,newpathBuf);
    if(retVal==-1){
      errNum=fromNativeErrNums(errno);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_chdir(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_chdir: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_mknod(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_mknod: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_chmod(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_chmod: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_lchown(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_lchown: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_utime(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_utime: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_access(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr  fname = myArgs.getA();
  int    mode  = myArgs.getW();
  int   retVal = -1;
  int   errNum = 0;
  ssize_t fnameLen=context->readMemString(fname,0,0);
  if(fnameLen==-1){
    errNum=Mips32_EFAULT;
  }else{
    char fnameBuf[fnameLen];
    size_t fnameLenChk=context->readMemString(fname,fnameLen,fnameBuf);
    I(fnameLenChk==(size_t)fnameLen);
    I(fnameLenChk==strlen(fnameBuf)+1);
    retVal=access(fnameBuf,mode);
    if(retVal==-1){
      switch(errno){
      default: fail("sysCallMips32_access: Unknown error %d\n",errno);
      }
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_sync(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sync: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_mkdir(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_mkdir: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_rmdir(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_rmdir: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_pipe(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_pipe: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ioctl(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int fd      = myArgs.getW();
  int request = myArgs.getW();
  int retVal = -1;
  int errNum = 0;
  fd=context->getAddressSpace()->getRealFd(fd);
  if(fd==-1){      
    errNum=Mips32_EBADF;
  }else{
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
      errNum=Mips32_ENOTTY;
    } break;
    default:
      fail("sysCallMips32Ioctl called with fd %d and req 0x%x at 0x%08x\n",
            fd,request,context->getNextInstDesc()->addr);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_fcntl(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fcntl: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fcntl64(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int simfd = myArgs.getW();
  int cmd   = toNativeFcntlCmd(myArgs.getW());
  int retVal = -1;
  int errNum = 0;
  int fd=context->getAddressSpace()->getRealFd(simfd);
  if(fd==-1){
    errNum=Mips32_EBADF;
  }else{
    switch(cmd){
    case F_GETFD:
      retVal=context->getAddressSpace()->getCloexec(simfd)?Mips32_FD_CLOEXEC:0;
      break;
    case F_SETFD: {
      int arg = myArgs.getW();
      context->getAddressSpace()->setCloexec(simfd,(arg&Mips32_FD_CLOEXEC));
      retVal=0;
    } break;
    default: fail("sysCallMips32_fcntl64 unknown command %d on file %d\n",cmd,fd);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}

template<class Mips32Stat>
void fromNativeStat(Mips32Stat &mipsStat, const struct stat &natStat){
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

template<class Mips32Stat>
bool equalsStat(const Mips32Stat &st1, const Mips32Stat &st2){
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
void sysCallMips32AnyStat(ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  struct stat  nativeStat;
  int retVal=-1;
  int errNum=0;
  switch(StVar) {
  case Stat: case Stat64: case LStat: case LStat64: {
    // Get the file name
    VAddr fname=myArgs.getA();
    ssize_t fnameLen=context->readMemString(fname,0,0);
    if(fnameLen==-1){
      errNum=Mips32_EFAULT;
    }else{
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
    }
  } break;
  case FStat: case FStat64: {
    int simfd=myArgs.getW();
    int fd=context->getAddressSpace()->getRealFd(simfd);
    if(fd==-1){      
      errNum=Mips32_EBADF;              
    }else{
      retVal=fstat(simfd,&nativeStat);
      off_t st_size=nativeStat.st_size;
      dev_t st_rdev=nativeStat.st_rdev;
      blksize_t st_blksize=nativeStat.st_blksize;
      blkcnt_t  st_blocks=nativeStat.st_blocks;
      retVal=fstat(fd,&nativeStat);
      // Make standard in, out, and err look like TTY devices
      if(simfd<3){
	// Change format to character device
	nativeStat.st_mode&=(~S_IFMT);
	nativeStat.st_mode|=S_IFCHR;
	// Change device to a TTY device type
	// DEV_TTY_LOW_MAJOR is 136, DEV_TTY_HIGH_MAJOR is 143
	// We use a major number of 136 (0x88)
	nativeStat.st_rdev=0x8800;
      }
    }
  } break;
  }
  if(!errNum&&retVal){
    errNum=fromNativeErrNums(errno);
  }
  if(!retVal){
    VAddr  buf    = myArgs.getA();
    switch(StVar){
    case Stat: case FStat: case LStat: {
      Mips32_stat mipsStat;
      if(!context->canWrite(buf,sizeof(mipsStat))){
	retVal=-1; errNum=Mips32_EFAULT;
      }else{
	fromNativeStat(mipsStat,nativeStat);
	context->writeMem(buf,mipsStat);
      }
    } break;
    case Stat64: case FStat64: case LStat64: {
      Mips32_stat64 mipsStat;
      if(!context->canWrite(buf,sizeof(mipsStat))){
	retVal=-1; errNum=Mips32_EFAULT;
      }else{
	fromNativeStat(mipsStat,nativeStat);
	context->writeMem(buf,mipsStat);
      }
    } break;
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_stat64(InstDesc *inst, ThreadContext *context){ sysCallMips32AnyStat<Stat64>(context);}
void sysCallMips32_lstat64(InstDesc *inst, ThreadContext *context){sysCallMips32AnyStat<LStat64>(context);}
void sysCallMips32_fstat64(InstDesc *inst, ThreadContext *context){sysCallMips32AnyStat<FStat64>(context);}
void sysCallMips32_stat(InstDesc *inst, ThreadContext *context){   sysCallMips32AnyStat<Stat>(context);}
void sysCallMips32_lstat(InstDesc *inst, ThreadContext *context){  sysCallMips32AnyStat<LStat>(context);}
void sysCallMips32_fstat(InstDesc *inst, ThreadContext *context){  sysCallMips32AnyStat<FStat>(context);}

void sysCallMips32_umask(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_umask: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ustat(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ustat: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_dup2(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_dup2: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_symlink(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_symlink: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_readlink(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_readlink: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_readdir(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_readdir: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fchmod(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fchmod: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fchown(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fchown: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_statfs(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_statfs: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fstatfs(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fstatfs: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ioperm(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ioperm: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_socketcall(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_socketcall: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_syslog(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_syslog: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fsync(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fsync: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fchdir(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_fchdir: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setfsuid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setfsuid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setfsgid(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setfsgid: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getdents(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getdents: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32__newselect(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32__newselect: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_flock(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_flock: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_fdatasync(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_fdatasync: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_chown(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_chown: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_getdents64(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_getdents64: not implemented at 0x%08x\n",inst->addr); }

enum TruncateVariant { Trunc, FTrunc, Trunc64, FTrunc64 };

template<TruncateVariant TrVar>
void sysCallMips32AnyTrunc(ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  int retVal=-1;
  int errNum=0;
  switch(TrVar){
  case Trunc: case Trunc64: {
    // Get the file name
    VAddr path=myArgs.getA();
    ssize_t pathLen=context->readMemString(path,0,0);
    if(pathLen==-1){
      errNum=Mips32_EFAULT;
    }else{
      char pathBuf[pathLen];
      size_t pathLenChk=context->readMemString(path,pathLen,pathBuf);
      I(pathLenChk==(size_t)pathLen);
      I(pathLenChk==strlen(pathBuf)+1);
      // Do the actual system call
      switch(TrVar){
      case Trunc:
	retVal=truncate(pathBuf,(off_t)(myArgs.getW())); break;
      case Trunc64:
	retVal=truncate(pathBuf,(off_t)(myArgs.getL())); break;
      }
    }
  } break;
  case FTrunc: case FTrunc64: {
    int fd=myArgs.getW();
    fd=context->getAddressSpace()->getRealFd(fd);
    if(fd==-1){      
      errNum=Mips32_EBADF;
    }else{
      switch(TrVar){
      case FTrunc:
        retVal=ftruncate(fd,(off_t)(myArgs.getW())); break;
      case FTrunc64:
        retVal=ftruncate(fd,(off_t)(myArgs.getL())); break;
      }
    }
  } break;
  }
  if(!errNum&&retVal){
    switch(errno){
    default:
      fail("sysCallMips32AnyTrunc: unknown error %d at 0x%08x\n",errno,context->getNextInstDesc()->addr);
    }
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}

void sysCallMips32_truncate(InstDesc *inst, ThreadContext *context){ sysCallMips32AnyTrunc<Trunc>(context); }
void sysCallMips32_ftruncate(InstDesc *inst, ThreadContext *context){ sysCallMips32AnyTrunc<FTrunc>(context); }
void sysCallMips32_truncate64(InstDesc *inst, ThreadContext *context){ sysCallMips32AnyTrunc<Trunc64>(context); }
void sysCallMips32_ftruncate64(InstDesc *inst, ThreadContext *context){ sysCallMips32AnyTrunc<FTrunc64>(context); }

// Message-passing (IPC) system calls

void sysCallMips32_ipc(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ipc: not implemented at 0x%08x\n",inst->addr);
}


// Obsolete system calls, should not be used

void sysCallMips32_oldstat(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_oldstat: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_break(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_break: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_oldfstat(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_oldfstat: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_stty(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_stty: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_gtty(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_gtty: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_prof(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_prof: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_lock(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_lock: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_afs_syscall(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_afs_syscall: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ftime(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ftime: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_getpmsg(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_getpmsg: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_putpmsg(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_putpmsg: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_mpx(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_mpx: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_profil(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_profil: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_ulimit(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_ulimit: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_oldlstat(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_oldlstat: not implemented at 0x%08x\n",inst->addr);
}

// Privileged (superuser) system calls, should not be used

void sysCallMips32_mount(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_mount: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_umount(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_umount: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_stime(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_stime: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_umount2(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_umount2: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_chroot(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_chroot: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_sethostname(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sethostname: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setgroups(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setgroups: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_swapon(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_swapon: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_reboot(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_reboot: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_iopl(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_iopl: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_vhangup(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_vhangup: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_idle(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_idle: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_vm86(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_vm86: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_swapoff(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_swapoff: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_setdomainname(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_setdomainname: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_modify_ldt(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_modify_ldt: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_adjtimex(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_adjtimex: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_create_module(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_create_module: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_init_module(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_init_module: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_delete_module(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_delete_module: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_get_kernel_syms(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_get_kernel_syms: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_quotactl(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_quotactl: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_bdflush(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_bdflush: not implemented at 0x%08x\n",inst->addr);
}

void sysCallMips32_time(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr t=myArgs.getA();
  int errNum=0;
  // TODO: This should actually take into account simulated time
  int32_t retVal=(int32_t)(time(0));
  if(retVal==-1){
    errNum=fromNativeErrNums(errno);
  }else{
    if(t!=(VAddr)0)
      context->writeMem<int32_t>(t,retVal);
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}

bool operator==(const Mips32_tms &var1, const Mips32_tms &var2){
  return
    (var1.tms_utime==var2.tms_utime)&&(var1.tms_stime==var2.tms_stime)&&
    (var1.tms_cutime==var2.tms_cutime)&&(var1.tms_cstime==var2.tms_cstime);
}
void sysCallMips32_times(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr buf=myArgs.getA();
  Mips32_clock_t retVal=-1;
  int errNum=0;
  Mips32_tms myTms;
  if(!context->canWrite(buf,sizeof(myTms))){
    errNum=Mips32_EFAULT;
  }else{
    // TODO: This is a hack. See above.
    myUsrUsecs+=100;
    mySysUsecs+=1;
  
    myTms.tms_utime=myTms.tms_cutime=myUsrUsecs/1000;
    myTms.tms_stime=myTms.tms_cstime=mySysUsecs/1000;
    retVal=(myUsrUsecs+mySysUsecs)/1000;
    context->writeMem(buf,myTms);
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}

void sysCallMips32_unused59(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_unused59: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_reserved82(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_reserved82: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_unused109(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_unused109: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_unused150(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_unused150: not implemented at 0x%08x\n",inst->addr);
}

// Miscaleneous and unsorted

void sysCallMips32_gettimeofday(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_gettimeofday: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_settimeofday(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_settimeofday: not implemented at 0x%08x\n",inst->addr);
}

void sysCallMips32_uselib(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_uselib: not implemented at 0x%08x\n",inst->addr); }

void sysCallMips32_sysinfo(InstDesc *inst, ThreadContext *context){
  fail("sysCallMips32_sysinfo: not implemented at 0x%08x\n",inst->addr);
}
void sysCallMips32_uname(InstDesc *inst, ThreadContext *context){
  Mips32FuncArgs myArgs(context);
  VAddr buf = myArgs.getA();
  int retVal=-1;
  int errNum=0;
  Mips32_utsname myUtsname;
  if(!context->canWrite(buf,sizeof(myUtsname))){
    errNum=Mips32_EFAULT;
  }else{
    memset(&myUtsname,0,sizeof(myUtsname));
    strcpy(myUtsname.sysname,"SescLinux");
    strcpy(myUtsname.nodename,"sesc");
    strcpy(myUtsname.release,"2.4.18");
    strcpy(myUtsname.version,"#1 SMP Tue Jun 4 16:05:29 CDT 2002");
    strcpy(myUtsname.machine,"mips");
    strcpy(myUtsname.domainname,"Sesc");
    context->writeMemFromBuf(buf,sizeof(myUtsname),&myUtsname);
    retVal=0;
  }
  I(context->getMode()==Mips32);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegA3),errNum);
  Mips::setReg<int32_t>(context,static_cast<RegName>(Mips::RegV0),retVal);
}
void sysCallMips32_sysfs(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sysfs: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_personality(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_personality: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_cachectl(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_cachectl: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sysmips(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sysmips: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32__sysctl(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32__sysctl: not implemented at 0x%08x\n",inst->addr); }

void sysCallMips32_sched_setparam(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_setparam: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_getparam(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_getparam: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_setscheduler(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_setscheduler: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_getscheduler(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_getscheduler: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_yield(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_yield: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_get_priority_max(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_get_priority_max: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_get_priority_min(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_get_priority_min: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sched_rr_get_interval(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sched_rr_get_interval: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_nanosleep(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_nanosleep: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_accept(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_accept: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_bind(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_bind: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_connect(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_connect: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_getpeername(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_getpeername: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_getsockname(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_getsockname: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_getsockopt(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_getsockopt: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_listen(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_listen: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_recv(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_recv: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_recvfrom(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_recvfrom: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_recvmsg(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_recvmsg: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_send(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_send: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sendmsg(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sendmsg: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sendto(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sendto: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_setsockopt(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_setsockopt: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_shutdown(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_shutdown: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_socket(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_socket: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_socketpair(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_socketpair: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_query_module(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_query_module: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_poll(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_poll: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_nfsservctl(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_nfsservctl: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_prctl(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_prctl: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_getcwd(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_getcwd: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_capget(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_capget: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_capset(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_capset: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sigaltstack(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sigaltstack: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_sendfile(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_sendfile: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_root_pivot(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_root_pivot: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_mincore(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_mincore: not implemented at 0x%08x\n",inst->addr); }
void sysCallMips32_madvise(InstDesc *inst, ThreadContext *context){
 fail("sysCallMips32_madvise: not implemented at 0x%08x\n",inst->addr); }
