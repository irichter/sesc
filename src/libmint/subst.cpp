/*
 * Includes all the functions that are substituted for their
 * counterparts in the traced object file.
 *
 * Copyright (C) 1993 by Jack E. Veenstra (veenstra@cs.rochester.edu)
 * 
 * This file is part of MINT, a MIPS code interpreter and event generator
 * for parallel programs.
 * 
 * MINT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 * 
 * MINT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MINT; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <string.h>
#include <ulimit.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <errno.h>

/* #define DEBUG_VERBOSE 1 */

#ifdef DARWIN
typedef off_t off64_t;
#else
#include <stropts.h>
#endif

#if (defined LINUX) && (defined __i386)
#include <sys/syscall.h>
#include <linux/types.h>
#include <linux/unistd.h>
typedef __off64_t off64_t;

static _syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count);
int getdents(unsigned int fd, struct dirent *dirp, unsigned int count);
#endif

#include "alloca.h"
#include "icode.h"
#include "ThreadContext.h"
#include "globals.h"
#include "opcodes.h"
#include "symtab.h"
#include "mendian.h"
#include "prctl.h"
#include "mintapi.h"
#include "OSSim.h"

#if (defined TLS)
#include "Epoch.h"
#endif

struct glibc_stat64 {
  unsigned long	st_dev;
  unsigned long	st_pad0[3];	/* Reserved for st_dev expansion  */

  unsigned long long	st_ino;

  unsigned int  st_mode;
  int           st_nlink;

  int           st_uid;
  int           st_gid;

  unsigned long	st_rdev;
  unsigned long	st_pad1[3];	/* Reserved for st_rdev expansion  */

  long long	st_size;

  /*
   * Actually this should be timestruc_t st_atime, st_mtime and st_ctime
   * but we don't have it under Linux.
   */
  long          st_atim;
  unsigned long	reserved0;	/* Reserved for st_atime expansion  */

  long          st_mtim;
  unsigned long	reserved1;	/* Reserved for st_mtime expansion  */

  long          st_ctim;
  unsigned long	reserved2;	/* Reserved for st_ctime expansion  */

  unsigned long	st_blksize;
  unsigned long	st_pad2;

  long long	st_blocks;
};

/* defines to survive the 64 bits mix */
struct glibc_stat32 {
  unsigned int    st_dev;
  long		st_pad1[3];		/* Reserved for network id */
  unsigned long   st_ino;
  unsigned int    st_mode;
  int             st_nlink;
  int             st_uid;
  int             st_gid;
  unsigned int    st_rdev;
  long		st_pad2[2];
  long            st_size;
  long		st_pad3;
  /*
   * Actually this should be timestruc_t st_atime, st_mtime and st_ctime
   * but we don't have it under Linux.
   */
  long            st_atim;
  long		reserved0;
  long            st_mtim;
  long		reserved1;
  long            st_ctim;
  long		reserved2;
  long		st_blksize;
  long		st_blocks;
  long		st_pad4[14];
};

void conv_stat32_native2glibc(const struct stat *statp, struct glibc_stat32 *stat32p)
{
  stat32p->st_dev   = SWAP_WORD(statp->st_dev);
  stat32p->st_ino   = SWAP_WORD(statp->st_ino); 
  stat32p->st_mode  = SWAP_WORD(statp->st_mode);
  stat32p->st_nlink = SWAP_WORD(statp->st_nlink);
  stat32p->st_uid   = SWAP_WORD(statp->st_uid);
  stat32p->st_gid   = SWAP_WORD(statp->st_gid);
  stat32p->st_rdev  = SWAP_WORD(statp->st_rdev); 
  stat32p->st_size  = SWAP_WORD(statp->st_size); 

  stat32p->st_atim = SWAP_WORD(statp->st_atime);
  stat32p->st_mtim = SWAP_WORD(statp->st_mtime);
  stat32p->st_ctim = SWAP_WORD(statp->st_ctime);

  stat32p->st_blksize = SWAP_WORD(statp->st_blksize);
  stat32p->st_blocks  = SWAP_WORD(statp->st_blocks);
}

void conv_stat64_native2glibc(const struct stat *statp, struct glibc_stat64 *stat64p)
{
  stat64p->st_dev   = SWAP_WORD(statp->st_dev);
  stat64p->st_ino   = SWAP_LONG((unsigned long long)statp->st_ino); 
  stat64p->st_mode  = SWAP_WORD(statp->st_mode);
  stat64p->st_nlink = SWAP_WORD(statp->st_nlink);
  stat64p->st_uid   = SWAP_WORD(statp->st_uid);
  stat64p->st_gid   = SWAP_WORD(statp->st_gid);
  stat64p->st_rdev  = SWAP_WORD(statp->st_rdev); 
  stat64p->st_size  = SWAP_LONG((unsigned long long)statp->st_size); 

  stat64p->st_atim = SWAP_WORD(statp->st_atime);
  stat64p->st_mtim = SWAP_WORD(statp->st_mtime);
  stat64p->st_ctim = SWAP_WORD(statp->st_ctime);

  stat64p->st_blksize = SWAP_WORD(statp->st_blksize);
  stat64p->st_blocks  = SWAP_LONG((unsigned long long)statp->st_blocks);
}

#if (defined TASKSCALAR) || (defined TLS)
/* Exit icode, used from SESC with Taskscalar to force a thread to terminate */
icode_t TerminatePicode;
#endif
/* Invalid icode, used as the "next icode" for contexts without any thread to execute */ 
icode_t invalidIcode;
icode_ptr Idone1;		/* calls terminator1() */
static icode_t Iterminator1;

extern int errno;

/* substitute functions */
OP(mint_ulimit);
OP(mint_sysmp);
OP(mint_getdents);
OP(mint_readv);OP(mint_writev);
OP(mint_execl); OP(mint_execle); OP(mint_execlp);
OP(mint_execv); OP(mint_execve); OP(mint_execvp);
OP(mint_mmap); OP(mint_munmap);
OP(mint_open); OP(mint_close); OP(mint_read); OP(mint_write);
OP(mint_creat); OP(mint_link); OP(mint_unlink); OP(mint_chdir);
OP(mint_rename);
OP(mint_chmod); OP(mint_fchmod); OP(mint_chown); OP(mint_fchown);
OP(mint_lseek); OP(mint_lseek64); OP(mint_access);
OP(mint_stat); OP(mint_lstat); OP(mint_fstat); 
OP(mint_stat64); OP(mint_lstat64); OP(mint_fstat64); 
OP(mint_fxstat64); 
OP(mint_dup); OP(mint_pipe); OP(mint_xstat); OP(mint_xstat64);
OP(mint_symlink); OP(mint_readlink); OP(mint_umask);
OP(mint_getuid); OP(mint_geteuid); OP(mint_getgid); OP(mint_getegid);
OP(mint_getdomainname); OP(mint_setdomainname);
OP(mint_gethostname); OP(mint_socket); OP(mint_connect);
OP(mint_send); OP(mint_sendto); OP(mint_sendmsg);
OP(mint_recv); OP(mint_recvfrom); OP(mint_recvmsg);
OP(mint_getsockopt); OP(mint_setsockopt);
OP(mint_select);
OP(mint_oserror); OP(mint_setoserror); OP(mint_perror);
OP(mint_times); OP(mint_getdtablesize);
OP(mint_sigprocmask);
OP(mint_syssgi); OP(mint_time);
OP(mint_prctl);

OP(mint_malloc); OP(mint_calloc); OP(mint_free); OP(mint_realloc);
OP(mint_usinit); OP(mint_usconfig); OP(mint_usdetach);

OP(mint_getpid); OP(mint_getppid); 
OP(mint_clock);
OP(mint_gettimeofday);
OP(mint_isatty); OP(mint_ioctl); OP(mint_fcntl);
OP(mint_cerror); OP(mint_null_func);
OP(return1); OP(mint_exit); 

OP(mint_finish); 
OP(mint_printf);	/* not used */
OP(mint_sesc_get_num_cpus);
#ifdef TLS
//OP(mint_sesc_begin_epochs);
OP(mint_sesc_future_epoch);
OP(mint_sesc_future_epoch_jump);
OP(mint_sesc_acquire_begin);
OP(mint_sesc_acquire_retry);
OP(mint_sesc_acquire_end);
OP(mint_sesc_release_begin);
OP(mint_sesc_release_end);
OP(mint_sesc_commit_epoch);
OP(mint_sesc_change_epoch);
//OP(mint_sesc_end_epochs);
/* OP(mint_sesc_barrier_init); */
/* OP(mint_sesc_barrier); */
/* OP(mint_sesc_lock_init); */
/* OP(mint_sesc_lock); */
/* OP(mint_sesc_unlock); */
/* OP(mint_sesc_flag_init); */
/* OP(mint_sesc_flag_set); */
/* OP(mint_sesc_flag_trywait); */
/* OP(mint_sesc_flag_wait); */
/* OP(mint_sesc_flag_clear); */
#endif
OP(mint_do_nothing);
#ifdef VALUEPRED
OP(mint_sesc_get_last_value);
OP(mint_sesc_put_last_value);
OP(mint_sesc_get_stride_value);
OP(mint_sesc_put_stride_value);
OP(mint_sesc_get_incr_value);
OP(mint_sesc_put_incr_value);
OP(mint_sesc_verify_value);
#endif
#ifdef TASKSCALAR
OP(mint_rexit);
OP(mint_sesc_become_safe);
OP(mint_sesc_is_safe);
OP(mint_sesc_prof_commit);
OP(mint_sesc_prof_fork_successor);
#ifdef ATOMIC
OP(mint_sesc_start_transaction);
OP(mint_sesc_commit_transaction);
#endif
#endif


#ifdef SESC_LOCKPROFILE
OP(mint_sesc_startlock);
OP(mint_sesc_endlock);
OP(mint_sesc_startlock2);
OP(mint_sesc_endlock2);
#endif

OP(mint_uname);
OP(mint_getrlimit);
OP(mint_getrusage);
OP(mint_getcwd);
OP(mint_notimplemented);
OP(mint_assert_fail);

/* Calls to these functions execute a Mint function instead of the
 * native function.
 */

/* NOTE:
 *
 * It is NOT necessary to add underscores (_+) or libc_ at the
 * beginning of the function name. To avoid replication, noun_strcmp
 * eats all the underscores and libc_ that it finds.
 *
 * Ex: mmap would match for mmap, _mmap, __mmap, __libc_mmap ....
 *
 */

func_desc_t Func_subst[] = {
  // char *name               PFPI func;
  {"ulimit",		      mint_ulimit,	               1, OpExposed},
  {"execl",		      mint_execl,	               1, OpExposed},
  {"execle",		      mint_execle,	               1, OpExposed},
  {"execlp",		      mint_execlp,	               1, OpExposed},
  {"execv",		      mint_execv,	               1, OpExposed},
  {"execve",		      mint_execve,	               1, OpExposed},
  {"execvp",		      mint_execvp,	               1, OpExposed},
  {"mmap",		      mint_mmap,	               1, OpExposed},
  {"open64",                  mint_open,                       1, OpExposed}, 
  {"open",                    mint_open,                       1, OpExposed},
  {"close",                   mint_close,                      1, OpExposed},
  {"read",                    mint_read,                       1, OpExposed},
  {"write",                   mint_write,                      1, OpExposed},
  {"readv",                   mint_readv,                      1, OpExposed},
  {"writev",                  mint_writev,                     1, OpExposed},
  {"creat",                   mint_creat,                      1, OpExposed},
  {"link",                    mint_link,                       1, OpExposed},
  {"unlink",                  mint_unlink,                     1, OpExposed},
  {"rename",                  mint_rename,                     1, OpExposed},
  {"chdir",                   mint_chdir,                      1, OpExposed},
  {"chmod",                   mint_chmod,                      1, OpExposed},
  {"fchmod",                  mint_fchmod,                     1, OpExposed},
  {"chown",                   mint_chown,                      1, OpExposed},
  {"fchown",                  mint_fchown,                     1, OpExposed},
  {"lseek64",                 mint_lseek64,                    1, OpExposed},
  {"lseek",                   mint_lseek,                      1, OpExposed},
  {"access",                  mint_access,                     1, OpExposed},
  {"stat64",                  mint_stat64,                     1, OpExposed},
  {"lstat64",                 mint_lstat64,                    1, OpExposed},
  {"fstat64",                 mint_fstat64,                    1, OpExposed},
  {"xstat64",                 mint_xstat64,                    1, OpExposed},
  {"fxstat64",                mint_fxstat64,                   1, OpExposed},
  {"stat",                    mint_stat,                       1, OpExposed},
  {"lstat",                   mint_lstat,                      1, OpExposed},
  {"fstat",                   mint_fstat,                      1, OpExposed},
  {"xstat",                   mint_xstat,                      1, OpExposed},
  {"dup",                     mint_dup,                        1, OpExposed},
  {"pipe",                    mint_pipe,                       1, OpExposed},
  {"symlink",                 mint_symlink,                    1, OpExposed},
  {"readlink",                mint_readlink,                   1, OpExposed},
  {"umask",                   mint_umask,                      1, OpExposed},
  {"getuid",                  mint_getuid,                     0, OpExposed},
  {"geteuid",                 mint_geteuid,                    0, OpExposed},
  {"getgid",                  mint_getgid,                     0, OpExposed},
  {"getegid",                 mint_getegid,                    0, OpExposed},
  {"gethostname",             mint_gethostname,                1, OpExposed},
  {"getdomainname",	      mint_getdomainname,	       1, OpExposed},
  {"setdomainname",	      mint_setdomainname,	       1, OpExposed},
  {"socket",		      mint_socket,	               1, OpExposed},
  {"connect",		      mint_connect,	               1, OpExposed},
  {"send",		      mint_send,		       1, OpExposed},
  {"sendto",		      mint_sendto,	               1, OpExposed},
  {"sendmsg",		      mint_sendmsg,	               1, OpExposed},
  {"recv",		      mint_recv,		       1, OpExposed},
  {"recvfrom",		      mint_recvfrom,	               1, OpExposed},
  {"recvmsg",		      mint_recvmsg,	               1, OpExposed},
  {"getsockopt",	      mint_getsockopt,	               1, OpExposed},
  {"setsockopt",	      mint_setsockopt,	               1, OpExposed},
  {"select",		      mint_select,	               1, OpExposed},
  {"cachectl",                mint_null_func,                  1, OpExposed},
  {"oserror",                 mint_oserror,                    0, OpExposed},
  {"setoserror",              mint_setoserror,                 0, OpExposed},
  {"perror",                  mint_perror,                     1, OpExposed},
  {"times",                   mint_times,                      1, OpExposed},
  {"getdtablesize",	      mint_getdtablesize,	       1, OpExposed},
  {"syssgi",		      mint_syssgi,	               1, OpExposed},
  {"time",		      mint_time,		       1, OpExposed},
  {"munmap",                  mint_munmap,                     1, OpExposed},
  {"malloc",                  mint_malloc,                     1, OpUndoable},
  {"calloc",                  mint_calloc,                     1, OpUndoable},
  {"free",                    mint_free,                       1, OpUndoable},
  {"cfree",                   mint_free,                       1, OpExposed},
  {"realloc",                 mint_realloc,                    1, OpExposed},
  {"getpid",                  mint_getpid,                     1, OpExposed},
  {"getppid",                 mint_getppid,                    1, OpExposed},
  {"clock",                   mint_clock,                      1, OpExposed},
  {"gettimeofday",            mint_gettimeofday,               1, OpExposed},
  {"sysmp",                   mint_sysmp,                      1, OpExposed},
  {"getdents",		      mint_getdents,	               1, OpExposed},
  {"_getdents",	              mint_getdents,	               1, OpExposed},
  {"sproc",                   mint_notimplemented,             1, OpExposed},
  {"sprocsp",		      mint_notimplemented,             1, OpExposed},
  {"m_fork",                  mint_notimplemented,             1, OpExposed},
  {"m_next",		      mint_notimplemented,             1, OpExposed},
  {"m_set_procs",             mint_notimplemented,             1, OpExposed},
  {"m_get_numprocs",          mint_notimplemented,             1, OpExposed},
  {"m_get_myid",              mint_notimplemented,             1, OpExposed},
  {"m_kill_procs",	      mint_notimplemented,	       1, OpExposed},
  {"m_parc_procs",	      mint_notimplemented,	       1, OpExposed},
  {"m_rele_procs",	      mint_notimplemented,	       1, OpExposed},
  {"m_sync",		      mint_notimplemented,	       1, OpExposed},
  {"m_lock",		      mint_notimplemented,	       1, OpExposed},
  {"m_unlock",		      mint_notimplemented,	       1, OpExposed},
  {"fork",                    mint_notimplemented,             1, OpExposed},
  {"wait",                    mint_notimplemented,             1, OpExposed},
  {"wait3",                   mint_notimplemented,             1, OpExposed},
  {"waitpid",		      mint_notimplemented,             1, OpExposed},
  {"pthread_create",          mint_notimplemented,             1, OpExposed},
  {"pthread_lock",            mint_notimplemented,             1, OpExposed},
  /* wait must generate a yield because it might block */
  {"isatty",		      mint_isatty,                     1, OpExposed},
  {"ioctl",                   mint_ioctl,                      1, OpExposed},
  {"prctl",                   mint_prctl,                      1, OpExposed},
  {"fcntl64",                 mint_fcntl,                      1, OpExposed},
  {"fcntl",                   mint_fcntl,                      1, OpExposed},
  {"cerror64",                mint_cerror,                     1, OpExposed},
  {"cerror",                  mint_cerror,                     1, OpExposed},
#if (defined TASKSCALAR) & (! defined ATOMIC)
  {"abort",                   mint_rexit,                      1, OpExposed},
  {"exit",                    mint_rexit,                      1, OpExposed},
#else
  {"abort",                   mint_exit,                       1, OpExposed},
  {"exit",                    mint_exit,                       1, OpNoReplay},
#endif
  {"sesc_fetch_op",           mint_sesc_fetch_op,              1, OpExposed},
  {"sesc_unlock_op",          mint_sesc_unlock_op,             1, OpExposed},
  {"sesc_spawn",              mint_sesc_spawn,                 1, OpExposed},
  {"sesc_spawn_",             mint_sesc_spawn,                 1, OpExposed},
  {"sesc_sysconf",            mint_sesc_sysconf,               1, OpExposed},
  {"sesc_sysconf_",           mint_sesc_sysconf,               1, OpExposed},
  {"sesc_self",               mint_getpid,                     1, OpExposed},
  {"sesc_self_",              mint_getpid,                     1, OpExposed},
  {"sesc_exit",               mint_exit,                       1, OpExposed},
  {"sesc_exit_",              mint_exit,                       1, OpExposed},
  {"sesc_finish",             mint_finish,                     1, OpExposed},
  {"sesc_yield",              mint_sesc_yield,                 1, OpExposed},
  {"sesc_yield_",             mint_sesc_yield,                 1, OpExposed},
  {"sesc_suspend",            mint_sesc_suspend,               1, OpExposed},
  {"sesc_suspend_",           mint_sesc_suspend,               1, OpExposed},
  {"sesc_resume",             mint_sesc_resume,                1, OpExposed},
  {"sesc_resume_",            mint_sesc_resume,                1, OpExposed},
  {"sesc_simulation_mark",    mint_sesc_simulation_mark,       1, OpExposed},
  {"sesc_simulation_mark_id", mint_sesc_simulation_mark_id,    1, OpExposed},
  {"sesc_fast_sim_begin",     mint_sesc_fast_sim_begin,        1, OpExposed},
  {"sesc_fast_sim_begin_",    mint_sesc_fast_sim_begin,        1, OpExposed},
  {"sesc_fast_sim_end",       mint_sesc_fast_sim_end,          1, OpExposed},
  {"sesc_fast_sim_end_",      mint_sesc_fast_sim_end,          1, OpExposed},
  {"sesc_preevent",           mint_sesc_preevent,              1, OpExposed},
  {"sesc_preevent_",          mint_sesc_preevent,              1, OpExposed},
  {"sesc_postevent",          mint_sesc_postevent,             1, OpExposed},
  {"sesc_postevent_",         mint_sesc_postevent,             1, OpExposed},
  {"sesc_memfence",           mint_sesc_memfence,              1, OpExposed},
  {"sesc_memfence_",          mint_sesc_memfence,              1, OpExposed},
  {"sesc_acquire",            mint_sesc_acquire,               1, OpExposed},
  {"sesc_acquire_",           mint_sesc_acquire,               1, OpExposed},
  {"sesc_release",            mint_sesc_release,               1, OpExposed},
  {"sesc_release_",           mint_sesc_release,               1, OpExposed},
  {"sesc_wait",               mint_sesc_wait,                  1, OpExposed},
  //  {"printf",		      mint_printf,	               1, OpExposed},
  //  {"IO_printf",	              mint_printf,	               1, OpExposed},
  {"sesc_get_num_cpus",       mint_sesc_get_num_cpus,          0, OpExposed},
#if (defined TLS)
  //  {"sesc_begin_epochs",       mint_sesc_begin_epochs,          0, OpExposed},
  {"sesc_future_epoch",       mint_sesc_future_epoch,          0, OpExposed},
  {"sesc_future_epoch_jump",  mint_sesc_future_epoch_jump,     0, OpExposed},
  {"sesc_acquire_begin",      mint_sesc_acquire_begin,         0, OpInternal},
  {"sesc_acquire_retry",      mint_sesc_acquire_retry,         0, OpInternal},
  {"sesc_acquire_end",        mint_sesc_acquire_end,           0, OpInternal},
  {"sesc_release_begin",      mint_sesc_release_begin,         0, OpInternal},
  {"sesc_release_end",        mint_sesc_release_end,           0, OpInternal},
  {"sesc_commit_epoch",       mint_sesc_commit_epoch,          0, OpExposed},
  {"sesc_change_epoch",       mint_sesc_change_epoch,          0, OpInternal},
  //  {"sesc_end_epochs",         mint_sesc_end_epochs,            0, OpExposed},
#endif
#ifdef TASKSCALAR
  {"sesc_fork_successor",     mint_sesc_fork_successor,        0, OpExposed},
  {"sesc_prof_fork_successor",mint_sesc_prof_fork_successor,   0, OpExposed},
  {"sesc_commit",             mint_sesc_commit,                0, OpExposed},
  {"sesc_prof_commit",        mint_sesc_prof_commit,           0, OpExposed},
  {"sesc_become_safe",        mint_sesc_become_safe,           0, OpExposed},
  {"sesc_is_safe",            mint_sesc_is_safe,               0, OpExposed},
  {"sesc_is_versioned",       mint_do_nothing,                 0, OpExposed},
  {"sesc_begin_versioning",   mint_do_nothing,                 0, OpExposed},
#ifdef ATOMIC
  {"sesc_start_transaction",  mint_sesc_start_transaction,     0, OpExposed},
  {"sesc_commit_transaction", mint_sesc_commit_transaction,    0, OpExposed},
#endif
#endif
#ifdef SESC_LOCKPROFILE
  {"sesc_startlock",          mint_sesc_startlock,             0, OpExposed},
  {"sesc_endlock",            mint_sesc_endlock,               0, OpExposed},
  {"sesc_startlock2",         mint_sesc_startlock2,            0, OpExposed},
  {"sesc_endlock2",           mint_sesc_endlock2,              0, OpExposed},
#endif
#ifdef VALUEPRED
  {"sesc_get_last_value",     mint_sesc_get_last_value,        0, OpExposed}, 
  {"sesc_put_last_value",     mint_sesc_put_last_value,        0, OpExposed}, 
  {"sesc_get_stride_value",   mint_sesc_get_stride_value,      0, OpExposed}, 
  {"sesc_put_stride_value",   mint_sesc_put_stride_value,      0, OpExposed}, 
  {"sesc_get_incr_value",     mint_sesc_get_incr_value,        0, OpExposed}, 
  {"sesc_put_incr_value",     mint_sesc_put_incr_value,        0, OpExposed}, 
  {"sesc_verify_value",       mint_sesc_verify_value,          0, OpExposed}, 
#endif
  {"uname",                   mint_uname,                      1, OpExposed},
  {"getrlimit",               mint_getrlimit,                  1, OpExposed},
  {"setrlimit",               mint_do_nothing,                 1, OpExposed},
  {"getrusage",               mint_getrusage,                  1, OpExposed},
  {"syscall_fstat64",         mint_fstat64,                    1, OpExposed},
  {"syscall_fstat",           mint_fstat,                      1, OpExposed},
  {"syscall_stat64",          mint_stat64,                     1, OpExposed},
  {"syscall_stat",            mint_stat,                       1, OpExposed},
  {"syscall_lstat64",         mint_lstat64,                    1, OpExposed},
  {"syscall_lstat",           mint_lstat,                      1, OpExposed},
  {"syscall_getcwd",          mint_getcwd,                     1, OpExposed},
  {"assert_fail",             mint_assert_fail,                1, OpExposed},
  {"sigaction",               mint_do_nothing,                 1, OpExposed},
  {"ftruncate64",             mint_do_nothing,                 1, OpExposed},
  { NULL,                     NULL,                            1, OpExposed}
};

static struct namelist *Func_nlist;

int Nfuncs = sizeof(Func_subst) / sizeof (struct func_desc_t);

void subst_init()
{
  int i;
  int notfound;
  struct namelist *pnlist, *psym;
  struct namelist errno_nlist[7];
  FILE *fdummy;

  Iterminator1.func = terminator1;
  Iterminator1.opnum = 0;
  Iterminator1.next = NULL;
  Iterminator1.instID = 0;

  invalidIcode.addr = 0;
  invalidIcode.func = NULL;
  invalidIcode.opnum = 0;
  invalidIcode.next = NULL;
  invalidIcode.instID = 8;

#ifdef TASKSCALAR
  TerminatePicode.func   = mint_exit;
  TerminatePicode.addr   = 8; /* Invalid addr */
  TerminatePicode.opnum  = 0;
  TerminatePicode.next   = NULL;
  TerminatePicode.instID = 0;
#endif

  bzero(errno_nlist,sizeof(struct namelist)*7);

  errno_nlist[0].n_name = "__errnoaddr";
  /* Original: errno_nlist[0].n_name = "errno"; */
  errno_nlist[1].n_name = "sesc_spawn";
  errno_nlist[2].n_name = "exit";
  errno_nlist[3].n_name = "environ";
  errno_nlist[4].n_name = "m_fork";
  errno_nlist[5].n_name = "sprocsp";
  errno_nlist[6].n_name = NULL;

  /* check that object exists and is readable */
  fdummy = fopen(Objname, "r");
  if (fdummy == NULL) {
    perror(Objname);
    exit(1);
  }
  fclose(fdummy);
  notfound = namelist(Objname, errno_nlist);
  if (notfound == -1)
    fatal("nlist() failed on %s\n", Objname);
  Errno_addr = errno_nlist[0].n_value;
  if( Errno_addr == 4 ) {
#ifdef DEBUG
    printf("Errno_addr is pointing to who knows where\n");
#endif
    Errno_addr = 0;
  }
    
  Environ_addr = errno_nlist[3].n_value;
  Exit_addr = errno_nlist[2].n_value;
  if (errno_nlist[2].n_type == 0 || errno_nlist[2].n_value == 0)
    fatal("subst_init: cannot find address of exit()\n");

  pnlist = (struct namelist *) malloc(sizeof(struct namelist) * Nfuncs);
  if (pnlist == NULL)
    fatal("subst_init: cannot allocate 0x%x bytes for function nlist.\n",
	  sizeof(struct namelist) * Nfuncs);
  for (i = 0; i < Nfuncs; i++)
    pnlist[i].n_name = Func_subst[i].name;

  notfound = namelist(Objname, pnlist);
  if (notfound == -1)
    fatal("nlist() failed on %s\n", Objname);
  Func_nlist = pnlist;

  /* Find minimum address in name list. Anything past this address
   * can be assumed to be in library code.
   */
  Min_lib_value = 0xffffffff;
  for (psym = pnlist; psym->n_name; psym++) {
    if (psym->n_type == 0)
      continue;
    if (strcmp(psym->n_name, "sesc_preevent") == 0)
      continue;
    if (strcmp(psym->n_name, "sesc_preevent_") == 0)
      continue;

    if (strcmp(psym->n_name, "sesc_postevent") == 0)
      continue;
    if (strcmp(psym->n_name, "sesc_postevent_") == 0)
      continue;

    if (strcmp(psym->n_name, "sesc_memfence") == 0)
      continue;
    if (strcmp(psym->n_name, "sesc_memfence_") == 0)
      continue;
		
    if (strcmp(psym->n_name, "sesc_acquire") == 0)
      continue;
    if (strcmp(psym->n_name, "sesc_acquire_") == 0)
      continue;

    if (strcmp(psym->n_name, "sesc_release") == 0)
      continue;
    if (strcmp(psym->n_name, "sesc_relase_") == 0)
      continue;
    if (Min_lib_value > psym->n_value)
      Min_lib_value = psym->n_value;
  }
}

void subst_functions()
{
  int i;
  int notfound;
  icode_ptr picode, pcopy;
  func_desc_ptr pfname;
  struct namelist *pnlist, *psym;
  long base;
  
  pnlist = Func_nlist;
  pfname = Func_subst;
  for(psym=pnlist;psym->n_name;psym++,pfname++){
    if(psym->n_type==0)
      continue;
    if(psym->n_value<(unsigned)Text_start||psym->n_value>=(unsigned)Text_end)
      continue;
    
#ifdef DEBUG_VERBOSE
    printf("func name = %s, value = 0x%x\n", psym->n_name, psym->n_value);
#endif
    picode = addr2icode(psym->n_value);
    /* replace the first instruction in this routine with my function */
    picode->func = pfname->func;
#if (defined TASKSCALAR)
    if (pfname->no_spec == 1)
      picode->opflags = E_NO_SPEC;
    if (pfname->no_spec == 2)
      picode->opflags = E_LIMIT_SPEC;
#endif
#if (defined TLS)
    picode->setReplayClass(pfname->replayClass);
#endif
    picode->opnum = 0;
    
    /* Set the next field to return to the same routine. If we are
     * generating events, then this field will be changed to point to
     * the event function.
     */
    picode->next = picode;
  }
  free(pnlist);
  
  base = Text_size;
  Idone1 = Itext[base + DONE_ICODE];
}

OP(mint_assert_fail)
{
  printf("assertion failed in simulated program: (%s) %s:%ld\n",
         (const char *)pthread->virt2real(REGNUM(4)), (const char *)pthread->virt2real(REGNUM(5)), REGNUM(6)); 

  mint_exit(0,pthread);

  return 0;
}

OP(mint_notimplemented)
{
  fatal("System call @0x%lx not implemented. Use the appropiate API\n",
        picode->addr);

  return 0;
}

void rsesc_sysconf(int pid, int id, long flags);
icode_ptr rsesc_get_instruction_pointer(int pid);
void rsesc_set_instruction_pointer(int pid, icode_ptr picode);
void rsesc_spawn_stopped(int pid, int id, long flags);

OP(mint_sesc_sysconf)
{
  int flags;
  int pid;
  pid   = REGNUM(4);
  flags = REGNUM(5);

  rsesc_sysconf(pthread->getPid(),pid,flags);

  return addr2icode(REGNUM(31));
}

OP(mint_sesc_spawn){
  // Arguments of the sesc_spawn call
  RAddr entry = pthread->getGPR(Arg1stGPR);
  RAddr arg   = pthread->getGPR(Arg2ndGPR);
  long    flags = pthread->getGPR(Arg3rdGPR);
  // Set things up for the return from this call
  osSim->eventSetInstructionPointer(pthread->getPid(),addr2icode(pthread->getGPR(RetAddrGPR)));
  // Process ID of the parent thread
  Pid_t ppid=pthread->getPid();
  
#ifdef DEBUG_VERBOSE
  printf("sesc_spawn( 0x%lx, 0x%lx, 0x%lx ) pid = %d\n",
         entry,arg,flags,pthread->getPid());
#endif
  
  // Allocate a new thread for the child
  ThreadContext *child=ThreadContext::newActual();
  if(!child)
    fatal("sesc_spawn: exceeded process limit (%d)\n", Max_nprocs);

  // Process ID of the child thread
  Pid_t cpid=child->getPid();
  if (Maxpid < cpid)
    Maxpid = cpid;
  
  /* entry == 0 is more like a fork (with shared address space of
     course). In share_addr_space, the last parameter set to true
     means that the stack is copied
  */
  child->shareAddrSpace(pthread, PR_SADDR, !entry);
  
  child->init();

  pthread->newChild(child);
  
  if( entry ) {
    // The first instruction for the child is the entry point passed in
    child->setIP(addr2icode(entry));
    child->reg[4] = arg;
    child->reg[5] = Stack_size;		/* for sprocsp() */
    // In position-independent code every function expects to find
    // its own entry address in register jp (also known as t9 and R25)
    child->reg[25]= entry;
    // When the child returns from the 'entry' function,
    // it will go directly to the exit() function
    child->setGPR(RetAddrGPR,Exit_addr);
  }else{
    // If no entry point is supplied, we have fork-like behavior
    // The child will just return from sesc_spawn the same as parent
    child->setIP(pthread->getIP());
  }
  // The return value for the parent is the child's pid
  pthread->setGPR(RetValGPR,cpid);
  // The return value for the child is 0
  child->setGPR(RetValGPR,0);
  // Inform SESC of what we have done here
  rsesc_spawn(pthread->getPid(),cpid,flags);
#if (defined TLS)
  I(!pthread->getEpoch());
  tls::Epoch *iniEpoch=tls::Epoch::initialEpoch(static_cast<tls::ThreadID>(cpid));
  iniEpoch->run();
#endif
  // Note: not neccessarily running the same thread as before
  return osSim->eventGetInstructionPointer(pthread->getPid());
}

int mint_sesc_create_clone(ThreadContext *pthread){
  int ppid=pthread->getPid();
  int cpid;
  ThreadContext *cthread;
  int i;
#if (defined TLS)
  cthread=ThreadContext::newCloned();
  assert(cthread!=0);
#else
#ifdef DEBUG_VERBOSE
  printf("mint_sesc_create_clone: pid=%d at iAddr %08x\n",
	 ppid,icode2addr(rsesc_get_instruction_pointer(ppid)));
#endif
  cthread=ThreadContext::newActual();
#endif /* (defined TLS) */

  if(cthread==NULL) {
    fatal("mint_sesc_create_clone: exceeded process limit (%d)\n",Max_nprocs);
  }
  cpid=cthread->getPid();
  if(Maxpid<cpid)
    Maxpid=cpid;
  /* The child and the parent have the same address space (even the stack) */
  cthread->useSameAddrSpace(pthread);

  /* init_thread() must be called *after* use_same_addr_space() since
   * init_thread() uses the mapping fields set up by use_same_addr_space().
   */
  cthread->init();
  pthread->newChild(cthread);

  // The first instruction for the child is to return
  cthread->setPicode(rsesc_get_instruction_pointer(ppid));
  cthread->setTarget(pthread->getTarget());

  rsesc_spawn_stopped(ppid,cpid,0);
  return cpid;
}

void mint_sesc_die(thread_ptr pthread)
{
  REGNUM(4)=0;
  mint_exit(0,pthread);
}

#if (defined TLS)

OP(mint_sesc_acquire_begin){
  // Set things up for the return from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Do the actual call
  rsesc_acquire_begin(pthread->getPid());
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_acquire_retry){
  // This function never returns, so we don't care about the return address 
  rsesc_acquire_retry(pthread->getPid());
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_acquire_end){
  // Set things up for the return from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Do the actual call
  rsesc_acquire_end(pthread->getPid());
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_release_begin){
  // Set things up for the return from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Do the actual call
  rsesc_release_begin(pthread->getPid());
  // Note: not neccessarily running the same thread as before
  return pthread->getIP();
}

OP(mint_sesc_release_end){
  int pid=pthread->getPid();
  /* The address to return to from this call */
  long returnAddr=REGNUM(31);
  /* Next instruction is after this call */
  rsesc_set_instruction_pointer(pid,addr2icode(returnAddr));
  /* Do the actual call */
  rsesc_release_end(pid);
  /* Note that pthread->getPid() is not neccessarily the same as pid */
  return rsesc_get_instruction_pointer(pthread->getPid());
}

//OP(mint_sesc_begin_epochs){
  // Set things up for the return from this call
//  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Do the actual call
//   if(!pthread->getEpoch()){
//     tls::Epoch *iniEpoch=tls::Epoch::initialEpoch(static_cast<tls::ThreadID>(pthread->getPid()));
//     iniEpoch->run();
//   }
  // Note: not neccessarily running the same thread as before
//  return pthread->getIP();
//}

OP(mint_sesc_future_epoch){
  int pid=pthread->getPid();
  /* The address to return to from this call */
  long returnAddr=REGNUM(31);
  /* Next instruction is after this call */
  rsesc_set_instruction_pointer(pid,addr2icode(returnAddr));
  /* Do the actual call to create the successor */
  rsesc_future_epoch(pid);
  /* Note that pthread->getPid() is not neccessarily the same as pid */
  return rsesc_get_instruction_pointer(pthread->getPid()); 
}

OP(mint_sesc_future_epoch_jump){
  int pid=pthread->getPid();
  /* The address to return to from this call */
  long returnAddr=REGNUM(31);
  /* The address where the successor is to start */
  long successorAddr=REGNUM(4);
  /* Next instruction is after this call */
  rsesc_set_instruction_pointer(pid,addr2icode(returnAddr));
  /* Do the actual call to create the successor */
  rsesc_future_epoch_jump(pid,addr2icode(successorAddr));
  /* Note that pthread->getPid() is not neccessarily the same as pid */
  return rsesc_get_instruction_pointer(pthread->getPid()); 
}

OP(mint_sesc_commit_epoch){
  // Set things up for the return from this call
  osSim->eventSetInstructionPointer(pthread->getPid(),addr2icode(pthread->getGPR(RetAddrGPR)));
  /* Do the actual call to fork the successor */
  tls::Epoch *epoch=pthread->getEpoch();
  I(epoch==tls::Epoch::getEpoch(pthread->getPid()));
  if(epoch)
    epoch->complete();
  // Note: not neccessarily running the same thread as before
  return osSim->eventGetInstructionPointer(pthread->getPid());
}

OP(mint_sesc_change_epoch){
  // Set things up for the return from this call
  osSim->eventSetInstructionPointer(pthread->getPid(),addr2icode(pthread->getGPR(RetAddrGPR)));
  // Do the actual call
  tls::Epoch *oldEpoch=pthread->getEpoch();
  I(oldEpoch==tls::Epoch::getEpoch(pthread->getPid()));
  if(oldEpoch)
    oldEpoch->changeEpoch();
  // Note: not neccessarily running the same thread as before
  return osSim->eventGetInstructionPointer(pthread->getPid());
}

//OP(mint_sesc_end_epochs){
  // Set things up for the return from this call
//  osSim->eventSetInstructionPointer(pthread->getPid(),addr2icode(pthread->getGPR(RetAddrGPR)));
//   // Do the actual call
//   tls::Epoch *epoch=pthread->getEpoch();
//   I(epoch==tls::Epoch::getEpoch(pthread->getPid()));
//   if(epoch)
//     epoch->complete();
  // Note: not neccessarily running the same thread as before
//  return osSim->eventGetInstructionPointer(pthread->getPid());
//}

#endif

#ifdef TASKSCALAR
void rsesc_fork_successor(int ppid, long where, int tid);
OP(mint_sesc_fork_successor)
{
  int ppid=pthread->getPid();
  long returnAddr=REGNUM(31);

  /* Next instruction in the parent is after this call */
  rsesc_set_instruction_pointer(ppid,addr2icode(returnAddr));

  rsesc_fork_successor(ppid, returnAddr, 0);

  /* Note that pthread->getPid() is not neccessarily the same as pid */
  return rsesc_get_instruction_pointer(pthread->getPid());
}

OP(mint_sesc_prof_fork_successor)
{
  int tid = REGNUM(4);
  int ppid=pthread->getPid();
  long returnAddr=REGNUM(31);

  /* Next instruction in the parent is after this call */
  rsesc_set_instruction_pointer(ppid,addr2icode(returnAddr));

  rsesc_fork_successor(ppid, returnAddr, tid);

  /* Note that pthread->getPid() is not neccessarily the same as pid */
  return rsesc_get_instruction_pointer(pthread->getPid());
}

int rsesc_become_safe(int pid);
OP(mint_sesc_become_safe)
{
  int pid=pthread->getPid();

  rsesc_set_instruction_pointer(pid,addr2icode(REGNUM(31)));

  rsesc_become_safe(pid);

  return rsesc_get_instruction_pointer(pthread->getPid());
}

int rsesc_is_safe(int pid);
OP(mint_sesc_is_safe)
{
  int pid=REGNUM(4);
  REGNUM(2)=(long)(rsesc_is_safe(pid));
  return addr2icode(REGNUM(31));  
}

#endif


#ifdef VALUEPRED
int rsesc_get_last_value(int pid, int index);
OP(mint_sesc_get_last_value)
{
  int  index = REGNUM(4);
  REGNUM(2)=rsesc_get_last_value(pthread->getPid(), index);
  return addr2icode(REGNUM(31));  
}

void rsesc_put_last_value(int pid, int index, int val);
OP(mint_sesc_put_last_value)
{
  int  index = REGNUM(4);
  int  val   = REGNUM(5);
  rsesc_put_last_value(pthread->getPid(), index, val);
  return addr2icode(REGNUM(31));  
}

int rsesc_get_stride_value(int pid, int index);
OP(mint_sesc_get_stride_value)
{
  int  index = REGNUM(4);
  REGNUM(2)=rsesc_get_stride_value(pthread->getPid(), index);
  return addr2icode(REGNUM(31));  
}

void rsesc_put_stride_value(int pid, int index, int val);
OP(mint_sesc_put_stride_value)
{
  int  index = REGNUM(4);
  int  val   = REGNUM(5);
  rsesc_put_stride_value(pthread->getPid(), index, val);
  return addr2icode(REGNUM(31));  
}

int rsesc_get_incr_value(int pid, int index, int lval);
OP(mint_sesc_get_incr_value)
{
  int  index = REGNUM(4);
  int  lval  = REGNUM(5); 
  REGNUM(2)=rsesc_get_incr_value(pthread->getPid(), index, lval);
  return addr2icode(REGNUM(31));
}

void rsesc_put_incr_value(int pid, int index, int incr);
OP(mint_sesc_put_incr_value)
{
  int  index = REGNUM(4);
  int  incr  = REGNUM(5);
  rsesc_put_incr_value(pthread->getPid(), index, incr);
  return addr2icode(REGNUM(31));
}

void rsesc_verify_value(int pid, int rval, int pval);
OP(mint_sesc_verify_value)
{
  int  rval = REGNUM(4);
  int  pval = REGNUM(5);
  rsesc_verify_value(pthread->getPid(), rval, pval);
  return addr2icode(REGNUM(31));  
}
#endif

/* ARGSUSED */
OP(mint_sysmp)
{
    long command;
#if 0
    long arg1, arg2, arg3, arg4;
#endif

    command = REGNUM(4);
#if 0
    arg1 = REGNUM(5);
    arg2 = REGNUM(6);
    arg3 = REGNUM(7);
#endif
    switch (command) {
      case MP_MUSTRUN:
      case MP_RUNANYWHERE:
      case MP_CLOCK:
      case MP_EMPOWER:
      case MP_RESTRICT:
      case MP_UNISOLATE:
      case MP_SCHED:
        REGNUM(2) = 0;
        break;
      case MP_NPROCS:
      case MP_NAPROCS:
        REGNUM(2) = Max_nprocs;
        break;
      case MP_PGSIZE:
        REGNUM(2) = getpagesize();
        break;
      default:
        fatal("mint_sysmp( 0x%x ) not supported yet.\n", command);
        break;
    }
    return addr2icode(REGNUM(31));
}

/* Please, someone should port mint to execute mint3 and glibc from
 * gcc. glibc is a fully reentrant library, not like this crap!
 */
OP(mint_printf)
{
    long addr;
    long *sp;
    int index;
    char *cp;
    long args[100];
#if (defined TASKSCALAR) || (defined TLS)
    long *wdata;
    unsigned char *tempbuff2 = (unsigned char *) alloca (8000);
#endif
 
#ifdef DEBUG_VERBOSE
    printf("mint_printf()\n");
#endif

#if (defined TASKSCALAR) || (defined TLS)
    sp = (long *) REGNUM(29);
    rsesc_OS_read_string(pthread->getPid(), picode->addr, tempbuff2, (const void *)REGNUM(4) , 100);
    addr = (long)tempbuff2;
    tempbuff2 += 100;
#else
    addr = pthread->virt2real(REGNUM(4));
    sp = (long *) pthread->virt2real(REGNUM(29));
#endif
    args[0] = addr;
    args[1] = REGNUM(5);
    args[2] = REGNUM(6);
    args[3] = REGNUM(7);

    index = 0;
    for(cp=(char *)addr;*cp;cp++){
      if(*cp!='%')
	continue;
      if(*++cp=='%')
	continue;
      index++;

      if(index>3){
#if (defined TASKSCALAR) || (defined TLS)
	wdata = (long *)rsesc_OS_read(pthread->getPid(), (int)(sp + index), picode->addr, E_WORD);
	args[index]=SWAP_WORD(*wdata);
#else
	args[index]=SWAP_WORD(*(sp+index));
#endif
      }
      while((*cp>='0')&&(*cp<='9')) {
	cp++;
      }
      if(*cp=='s'){
	if(args[index]) {
#if (defined TASKSCALAR) || (defined TLS)
	  rsesc_OS_read_string(pthread->getPid(), picode->addr, tempbuff2, (const void *) args[index], 100);
	  args[index]=(long)tempbuff2;
	  tempbuff2 += 100;
#else
	  args[index]=pthread->virt2real(args[index]);
#endif
	}else
	  args[index]=(long)"(nil)";
      }else if(*cp=='f'||*cp=='g') {
	/* found a double */
	/* need to fix this sometime; for now don't use both %f and %s */
      }
    }

#ifdef TASKSCALAR
    if(index>10){
      printf("mint_printf: too many args\n");
      return addr2icode(REGNUM(31));
    }
    if (!rsesc_is_safe(pthread->getPid()))
      printf("SPEC[");
#else
    if(index>10)
      fatal("mint_printf: too many args\n");
#endif

    printf((char *)addr, args[1], args[2], args[3], args[4], args[5], args[6],
           args[7], args[8], args[9], args[10]);

#ifdef TASKSCALAR
    if (!rsesc_is_safe(pthread->getPid())) {
      printf("]");
      fflush(stdout);
    }
    
#endif

    fflush(stdout);

    return addr2icode(REGNUM(31));
}

// enum FetchOpType {
//   FetchIncOp  =0,
//   FetchDecOp,
//   FetchSwapOp
// };

#include "sescapi.h"

long rsesc_fetch_op(int pid, enum FetchOpType op, long addr, long *data, long val);
OP(mint_sesc_fetch_op)
{
  int  op   = REGNUM(4);
  long addr = REGNUM(5);
  long val  = REGNUM(6);
  long *data = (long *)pthread->virt2real(addr);

  REGNUM(2) = rsesc_fetch_op(pthread->getPid(),(enum FetchOpType)op,addr,data,val);
  
  return addr2icode(REGNUM(31));
}

void rsesc_unlock_op(int pid, long addr, long *data, int val);
OP(mint_sesc_unlock_op)
{
  long addr = REGNUM(4);
  long val  = REGNUM(5);
  long *data = (long *)pthread->virt2real(addr);
  rsesc_unlock_op(pthread->getPid(), addr, data, val);

  return addr2icode(REGNUM(31));
}

int rsesc_exit(int pid, int err);

void rsesc_finish(int pid);

OP(mint_finish)
{
  rsesc_finish(pthread->getPid());
  return &Iterminator1;
}

/* ARGSUSED */
OP(mint_exit){
#if (defined TASKSCALAR)
  if(rsesc_version_can_exit(pthread->getPid())==1)
    return addr2icode(pthread->getGPR(RetAddrGPR));
#endif
#if (defined TLS)
  tls::Epoch *epoch=pthread->getEpoch();
  I(epoch==tls::Epoch::getEpoch(pthread->getPid()));
  if(epoch){
    if(epoch->waitFullyMerged())
      epoch->endThread(pthread->getGPR(Arg1stGPR));
    else
      epoch->cancelInstr();
  }else{
    rsesc_exit(pthread->getPid(),pthread->getGPR(Arg1stGPR));
  }
#else
  rsesc_exit(pthread->getPid(),pthread->getGPR(Arg1stGPR));
#endif // (defined TLS)
  return rsesc_get_instruction_pointer(pthread->getPid());
}

#ifdef TASKSCALAR
OP(mint_sesc_commit)
{
#ifdef DEBUG_VERBOSE
  printf("mint_sesc_commit\n");
#endif  
  REGNUM(2) = 1;
  return mint_exit(picode, pthread);
}

void rsesc_prof_commit(int pid, int tid);
OP(mint_sesc_prof_commit)
{
  int tid = REGNUM(4);
  int pid = pthread->getPid();

#ifdef DEBUG_VERBOSE
  printf("mint_sesc_prof_commit\n");
#endif  
  rsesc_prof_commit(pid, tid);

  REGNUM(2) = 1;
  return mint_exit(picode, pthread);
}
#endif


#ifdef TASKSCALAR
void mint_termination(int pid);
OP(mint_rexit)
{
  fprintf(stderr,"mint_rexit called pid(%d) RA=0x%08x\n", pthread->getPid(), (unsigned int) REGNUM(31));
  mint_termination(pthread->getPid());

  return rsesc_get_instruction_pointer(pthread->getPid());
}
#endif

/* ARGSUSED */
OP(mint_isatty)
{
#ifdef DEBUG_VERBOSE
    printf("mint_isatty()\n");
#endif

  if (REGNUM(4)==0) REGNUM(2) = isatty(REGNUM(4)); 
  else REGNUM(2) = 1;
/*  REGNUM(2) = isatty(REGNUM(4)); 
 *  REGNUM(2) = 1;
 *
 * This should be the real, but makes simulation harder because the
 * program behaves different if the output is redirected to the
 * standard output
 * MCH: we only fake out to be tty. Input is real checking because of crafty
 */

#ifdef DEBUG_VERBOSE
  printf("isatty(%d) returned %d\n", (int) REGNUM(4), (int) REGNUM(2));
#endif
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_ioctl)
{
  long fd, cmd, arg;
  int err;

#ifdef DEBUG_VERBOSE
  printf("mint_ioctl()\n");
#endif
  fd = REGNUM(4);
  cmd = REGNUM(5);
  arg = REGNUM(6);
  switch (cmd) {
#if 0
  case FIOCLEX:
  case FIONCLEX:
    break;
#endif
#ifndef DARWIN
  case I_FLUSH:
  case I_SETSIG:
  case I_SRDOPT:
  case I_SENDFD:
    break;
#endif
#ifndef SUNOS
  case FIONREAD:
  case FIONBIO:
  case FIOASYNC:
  case FIOSETOWN:
  case FIOGETOWN:
#endif
#ifndef DARWIN
  case I_PUSH:
  case I_POP:
  case I_LOOK:
  case I_GETSIG:
  case I_FIND:
  case I_GRDOPT:
  case I_NREAD:
#endif
    if (arg)
      arg = pthread->virt2real(arg);
    break;
  default:
    fatal("ioctl command %d (0x%x) not supported.\n", cmd, cmd);
    break;
  }
  err = ioctl(fd, cmd, (char *) arg);
  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_prctl)
{
    long option;
    int err = 0;

#ifdef DEBUG_VERBOSE
    printf("mint_prctl()\n");
#endif
    option = REGNUM(4);
    switch (option) {
      case PR_MAXPROCS:
      case PR_MAXPPROCS:
        err = Max_nprocs;
        break;
      case PR_GETNSHARE:
        err = 0;
        break;
      case PR_SETEXITSIG:
	/* not really supported, but just fake it for now */
        err = 0;
        break;
      default:
        fatal("prctl option %d not supported yet.\n", option);
        break;
    }
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_fcntl)
{
  long fd, cmd, arg;
  int err;

#ifdef DEBUG_VERBOSE
  printf("mint_fcntl()\n");
#endif
  fd = REGNUM(4);
  cmd = REGNUM(5);
  arg = REGNUM(6);
  switch (cmd) {
  case F_DUPFD:
  case F_GETFD:
  case F_SETFD:
  case F_GETFL:
  case F_SETFL:
    break;
  case F_GETLK:
  case F_SETLK:
    if (arg)
      arg = pthread->virt2real(arg);
    break;
  case F_SETLKW:
    fatal("mint_fcntl: system call fcntl cmd `F_SETLKW' not supported.\n");
    break;
  case F_GETOWN:
    break;
  case F_SETOWN:
    break;
  default:
    fatal("mint_fcntl: system call fcntl cmd (%d) not expected.\n",
	  cmd);
  }
  err = fcntl(fd, cmd, arg);
  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);
  else {
    if (cmd == F_DUPFD && err < MAX_FDNUM)
      pthread->setFD(err, 1);
  }
  return addr2icode(REGNUM(31));
}

struct  mint_utsname {
  char    sysname[65];  /* Name of OS */
  char    nodename[65]; /* Name of this network node */
  char    release[65];  /* Release level */
  char    version[65];  /* Version level */
  char    machine[65];  /* Hardware type */
};

/* ARGSUSED */
OP(mint_uname)
{
  long r4 = REGNUM(4);
  struct mint_utsname *tp;

#ifdef DEBUG_VERBOSE
  printf("mint_uname()\n");
#endif
  if(r4 == 0) {
    fatal("uname called with a null pointer\n");
  }
  tp = (struct mint_utsname *)pthread->virt2real(r4);

  strcpy(tp->sysname, "SescLinux");
  strcpy(tp->nodename, "sesc");
  strcpy(tp->release, "2.4.18");
  strcpy(tp->version, "#1 SMP Tue Jun 4 16:05:29 CDT 2002"); /* fake */
  strcpy(tp->machine, "mips");

  REGNUM(2) = 0;
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getrlimit)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  ret;
  
#ifdef DEBUG_VERBOSE
  printf("mint_getrlimit()\n");
#endif
  if(r5 == 0) {
    fatal("uname called with a null pointer\n");
  }

  ret = getrlimit(r4,(struct rlimit *)pthread->virt2real(r5));

  if(ret == -1)
    pthread->setperrno(errno);
    
  REGNUM(2) = ret;
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getrusage)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  ret;
  
#ifdef DEBUG_VERBOSE
  printf("mint_getrusage()\n");
#endif
  if(r5 == 0) {
    fatal("uname called with a null pointer\n");
  }

  ret = getrusage(r4,(struct rusage *)pthread->virt2real(r5));

  if(ret == -1)
    pthread->setperrno(errno);
    
  REGNUM(2) = ret;
  return addr2icode(REGNUM(31));
}


/* ARGSUSED */
OP(mint_getpid)
{
#ifdef DEBUG_VERBOSE
  printf("mint_getpid()\n");
#endif
  REGNUM(2) = pthread->getPid();
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getppid)
{
#ifdef DEBUG_VERBOSE
  printf("mint_getppid()\n");
#endif

  if (pthread->getParent())
    REGNUM(2) = pthread->getParent()->getPid();
  else
    REGNUM(2) = 0;

  return addr2icode(REGNUM(31));
}

long rsesc_usecs();

/* ARGSUSED */
OP(mint_clock)
{
  static clock_t t=0;

  if (t==0) {
    t = clock();
  }
  /* 100 ticks per second is the standard */

  REGNUM(2) = t+rsesc_usecs()/10000;

#ifdef DEBUG_VERBOSE
  printf("mint_clock() %d\n", REGNUM(2));
#endif

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_null_func)
{
#ifdef DEBUG_VERBOSE
  printf("null func\n");
#endif

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(return1)
{
#ifdef DEBUG_VERBOSE
  printf("return 1\n");
#endif

  REGNUM(2) = 1;
  return addr2icode(REGNUM(31));
}

OP(mint_cerror)
{
  printf("mint_cerror\n");
#ifdef DEBUG_VERBOSE
  printf("mint_cerror\n");
#endif
  pthread->dump();
  picode->dump();
  REGNUM(2) = -1;
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_gettimeofday)
{
  long tp, tzp;
  long r4, r5;

  r4 = REGNUM(4);
  r5 = REGNUM(5);

  if (r4 == 0) {
    REGNUM(2) = -1;
    return addr2icode(REGNUM(31));
  }
  tp = pthread->virt2real(r4);

  if (r5)
    tzp = pthread->virt2real(r5);
  else
    tzp = 0;

  static long native_usecs=0;
  if (native_usecs == 0) {
    struct timeval tv;
    gettimeofday(&tv,0);
    native_usecs = tv.tv_sec * 1000000 + tv.tv_usec;
  }

  long usecs = native_usecs + rsesc_usecs();
  struct timeval tv;

  tv.tv_sec  = SWAP_WORD(usecs / 1000000);
  tv.tv_usec = SWAP_WORD(usecs % 1000000);

#ifdef DEBUG_VERBOSE
  printf("mint_gettimeofday() %d secs %d usesc\n", usecs / 1000000, usecs % 1000000);
#endif

#ifdef TASKSCALAR
  rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) r4, &tv, sizeof(struct timeval));
#else
  struct timeval *r4map = (struct timeval *) pthread->virt2real(r4);
  memcpy(r4map,&tv,sizeof(struct timeval));
#endif

  REGNUM(2) = 0;
  return addr2icode(REGNUM(31));
}

void rsesc_wait(int pid);

OP(mint_sesc_wait)
{
  int pid = pthread->getPid();

  rsesc_set_instruction_pointer(pid,addr2icode(REGNUM(31)));
  rsesc_wait(pid);
  return rsesc_get_instruction_pointer(pthread->getPid());
}

/* It's "system calls" all the way down. */

/* ARGSUSED */
OP(mint_ulimit)
{
    long cmd, newlimit;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_ulimit()\n");
#endif
    cmd = REGNUM(4);
    newlimit = REGNUM(5);

    err = ulimit(cmd, newlimit);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_execl)
{
#ifdef DEBUG_VERBOSE
    printf("mint_execl()\n");
#endif
    fatal("execl() system call not yet implemented\n");
    return NULL;
}

/* ARGSUSED */
OP(mint_execle)
{
#ifdef DEBUG_VERBOSE
    printf("mint_execle()\n");
#endif
    fatal("execle() system call not yet implemented\n");
    return NULL;
}

/* ARGSUSED */
OP(mint_execlp)
{
#ifdef DEBUG_VERBOSE
    printf("mint_execlp()\n");
#endif
    fatal("execlp() system call not yet implemented\n");
    return NULL;
}

/* ARGSUSED */
OP(mint_execv)
{
#ifdef DEBUG_VERBOSE
    printf("mint_execv()\n");
#endif
    fatal("execv() system call not yet implemented\n");
    return NULL;
}

/* ARGSUSED */
OP(mint_execve)
{
#ifdef DEBUG_VERBOSE
    printf("mint_execve()\n");
#endif
    fatal("execve() system call not yet implemented\n");
    return NULL;
}

/* ARGSUSED */
OP(mint_execvp)
{
#ifdef DEBUG_VERBOSE
    printf("mint_execvp()\n");
#endif
    fatal("execvp() system call not yet implemented\n");
    return NULL;
}

OP(mint_munmap)
{
  long r4   = REGNUM(4);

#ifdef DEBUG_VERBOSE
  printf("mint_unmmap(%d)\n", (int) r4);
#endif

  if (r4)
    mint_free(picode,pthread);

  return addr2icode(REGNUM(31));
}


/* ARGSUSED */
OP(mint_mmap)
{
  long r4   = REGNUM(4);

#ifdef DEBUG_VERBOSE
  printf("mint_mmap(%d)\n", (int) REGNUM(5));
#endif
  
  if( r4 ) {
    /* We can not force to map a specific address */
    fatal("mmap called with start different than zero\n");
  }

  REGNUM(4) = 1;
  mint_calloc(picode,pthread);
  REGNUM(4) = r4;
  
  if(REGNUM(2)==0) {
    REGNUM(2)=-1;
    pthread->setperrno(errno);
  }
  
  return addr2icode(REGNUM(31));
}

int conv_flags_to_native(int flags)
{
  int ret = flags & 07;

  if (flags & 0x0008) ret |= O_APPEND;
  /* Ignore O_SYNC Flag because it would be sync (atomic inside mint)
   * if (flags & 0x0010) ret |= O_SYNC;
   * if (flags & 0x1000) ret |= O_ASYNC;
  */
  if (flags & 0x0080) ret |= O_NONBLOCK;
  if (flags & 0x0100) ret |= O_CREAT;
  if (flags & 0x0200) ret |= O_TRUNC;
  if (flags & 0x0400) ret |= O_EXCL;
  if (flags & 0x0800) ret |= O_NOCTTY;

  /* get those from host's bits/fcntl.h */
  if (flags & 0x2000) ret |= 0100000;
  if (flags & 0x8000) ret |= 040000;
  if (flags & 0x10000) ret |= 0200000;
  if (flags & 0x20000) ret |= 0400000;

  if (flags == 0) ret |= O_CREAT;  /* for some reason, sometimes the flag is zero */
  
  return ret;
}

/* ARGSUSED */
OP(mint_open)
{
  long r4, r5, r6;
  int err;

  r4 = REGNUM(4);
  r5 = REGNUM(5);
  r6 = REGNUM(6);
    
  r5 = conv_flags_to_native(r5);

#ifdef TASKSCALAR
  {
    char cadena[100];

    rsesc_OS_read_string(pthread->getPid(), picode->addr, cadena, (const void *) REGNUM(4), 100);
      
    err = open((const char *) cadena, r5, r6);
    if( err == -1 ) {
      fprintf(stderr,"original flag = %ld, and conv_flag = %ld\n", REGNUM(5), r5);
      fprintf(stderr,"mint_open(%s,%ld,%ld) (failed) ret=%d\n",(const char *) cadena, r5, r6, err);
    }  


#ifdef DEBUG_VERBOSE
    fprintf(stderr,"mint_open(%s,%ld,%ld) ret=%d\n",(const char *) cadena, r5, r6, err);
#endif
  }
#else
  err = open((const char *)pthread->virt2real(r4) , r5, r6);

#ifdef DEBUG_VERBOSE
  fprintf(stderr,"mint_open(%s,%ld,%ld) ret=%d\n",(const char *) pthread->virt2real(r4), r5, r6, err);
#endif
#endif

  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);
  else {
    if (err < MAX_FDNUM)
      pthread->setFD(err, 1);
  }
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_close)
{
    long r4;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_close()\n");
#endif
    r4 = REGNUM(4);
    REGNUM(2) = 0;
    /* don't close our file descriptors */
    if (r4 < MAX_FDNUM && pthread->getFD(r4)) {
      pthread->setFD(r4, 0);
      err = close(r4);
      REGNUM(2) = err;
      if (err == -1)
	pthread->setperrno(errno);
    }
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_read)
{
    long r4, r5, r6;
    int err;

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

#ifdef DEBUG_VERBOSE
    printf("mint_read(%d, 0x%08x, %d), RA=0x%08x\n", (int) r4, (unsigned) r5, 
	   (int) r6, (unsigned) REGNUM(31));
#endif

#ifdef TASKSCALAR
    {
      void *tempbuff;
      tempbuff = (void *)malloc (r6);

      err = read(r4, tempbuff, r6);
      if (err > 0)
	rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) r5, (const void *)tempbuff, err);
      free(tempbuff);
    }
#else
    err = read(r4, (void *) pthread->virt2real(r5), r6);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_write){
  // Set things up for the return from this call
  pthread->setIP(addr2icode(pthread->getGPR(RetAddrGPR)));
  // Arguments of the write call
  int        fd=pthread->getGPR(Arg1stGPR);
  RAddr   buf=pthread->getGPR(Arg2ndGPR);
  size_t  count=pthread->getGPR(Arg3rdGPR);

  int err;
  int pid=pthread->getPid();

#ifdef DEBUG_VERBOSE
  printf("mint_write(%d, 0x%08x, %d)\n",(int)fd,(unsigned)buf ,(int)count);
#endif
  // Map buffer pointer to real address space
  buf=pthread->virt2real(buf);

#if (defined TASKSCALAR) || (defined TLS)
  {
    void *tempbuff=alloca(count);
    rsesc_OS_read_block(pthread->getPid(),picode->addr,tempbuff,(const void *)buf,count);
    err=write(fd,tempbuff,count);
  }
#else
  err=write(fd,(const void *)buf,count);
#endif
  pthread->setGPR(RetValGPR,err);
  if(err==-1)
    pthread->setErrno(errno);
  return pthread->getIP();
}

/* ARGSUSED */
OP(mint_readv)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_readv()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    r5 = pthread->virt2real(r5);

#ifdef TASKSCALAR
    {
      void *tempbuff = alloca (r6);
      
      err = readv(r4, (const iovec *)tempbuff, r6);
      if (err > 0)
	rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(5), tempbuff, err);
    }
#else
    err = readv(r4, (const iovec *) r5, r6);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_writev)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_writev()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    r5 = pthread->virt2real(r5);

#ifdef TASKSCALAR
    {
      void *tempbuff =  alloca (r6);
      rsesc_OS_read_block(pthread->getPid(), picode->addr, tempbuff, (const void *)REGNUM(5), r6);

      err = writev(r4, (const iovec *)tempbuff, r6);
    }
#else
    err = writev(r4, (const iovec *) r5, r6);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_creat)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_creat()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cadena[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cadena, (const void *) REGNUM(4), 100);
      
      err = open((const char *) cadena, r5);
    }
#else
    err = creat((const char *) r4, r5);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_link)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_link()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

#ifdef TASKSCALAR
    {
      char cad1[100];
      char cad2[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad2, (const void *) REGNUM(5), 100);
      
      err = link((const char *) cad1, cad2);
    }
#else
    r4 = pthread->virt2real(r4);
    r5 = pthread->virt2real(r5);

    err = link((const char *) r4, (const char *) r5);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_unlink)
{
    long r4;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_unlink()\n");
#endif
    r4 = REGNUM(4);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cad1[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      
      err = unlink((const char *) cad1);
    }
#else
    err = unlink((const char *) r4);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_rename)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_rename()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

#ifdef TASKSCALAR
    {
      char cad1[100];
      char cad2[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad2, (const void *) REGNUM(5), 100);
      
      err = rename((const char *) cad1, (const char *)cad2);
    }
#else
    r4 = pthread->virt2real(r4);
    r5 = pthread->virt2real(r5);

    err = rename((const char *) r4, (const char *) r5);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_chdir)
{
    long r4;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_chdir()\n");
#endif
    r4 = REGNUM(4);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cad1[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      
      err = chdir((const char *) cad1);
    }
#else
    err = chdir((const char *) r4);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_chmod)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_chmod()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cad1[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      
      err = chmod(cad1, r5);
    }
#else
    err = chmod((char *) r4, r5);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_fchmod)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_fchmod()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

    err = fchmod(r4, r5);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_chown)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_chown()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cad1[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      
      err = chown((const char *) cad1, r5, r6);
    }
#else
    err = chown((const char *) r4, r5, r6);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_fchown)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_chown()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    err = fchown(r4, r5, r6);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_lseek64)
{
    int r4, r6;
    off64_t r5;
    off64_t err;

#ifdef DEBUG_VERBOSE
    printf("mint_lseek64()\n");
#endif
    r4 = (int)(REGNUM(4));
    r5 = (off64_t)(REGNUM(5));
    r6 = (int)(REGNUM(6));

    /* r6 value is changed to -1 by the instruction after jal */
    if( r6 < 0 ) r6 = 0;
    
    err = (int) lseek(r4,r5,r6);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_lseek)
{
    long r4 = REGNUM(4);
    long r5 = REGNUM(5);
    long r6 = REGNUM(6);
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_lseek()\n");
#endif

    err = (int) lseek((int) r4, (off64_t) r5, (int) r6);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}


/* ARGSUSED */
OP(mint_access)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_access()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cad1[100];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);
      
      err = access((const char *) cad1, r5);
    }
#else
    err = access((const char *) r4, r5);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_stat)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  err;

#ifdef DEBUG_VERBOSE
  printf("mint_stat()\n");
#endif
  if(r4 == 0 || r5 == 0) {
    fatal("stat called with a null pointer\n");
  }

#if (defined TLS) || (defined TASKSCALAR)
  {
    struct glibc_stat32 stat32p;
    struct stat stat_native;
    char tmpbuff[2048];
      
    rsesc_OS_read_string(pthread->getPid(), picode->addr, tmpbuff, (const void *)r4, 2048);

    err = stat(tmpbuff, &stat_native);
    conv_stat32_native2glibc(&stat_native, &stat32p);

    rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(5), &stat32p, sizeof(struct glibc_stat32));
  }
#else
  {
    struct glibc_stat32 *stat32p = (struct glibc_stat32 *)pthread->virt2real(r5);
    struct stat stat_native;
      
    err = stat((const char *)pthread->virt2real(r4), &stat_native);
      
    conv_stat32_native2glibc(&stat_native, stat32p);
  }
#endif

  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_lstat)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  err;

#ifdef DEBUG_VERBOSE
  printf("mint_stat()\n");
#endif
  if(r4 == 0 || r5 == 0) {
    fatal("stat called with a null pointer\n");
  }

#if (defined TLS) || (defined TASKSCALAR)
  {
    struct glibc_stat32 stat32p;
    struct stat stat_native;
    char tmpbuff[2048];
      
    rsesc_OS_read_string(pthread->getPid(), picode->addr, tmpbuff, (const void *)r4, 2048);

    err = lstat(tmpbuff, &stat_native);
    conv_stat32_native2glibc(&stat_native, &stat32p);

    rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(5), &stat32p, sizeof(struct glibc_stat32));
  }
#else
  {
    struct glibc_stat32 *stat32p = (struct glibc_stat32 *)pthread->virt2real(r5);
    struct stat stat_native;
      
    err = lstat((const char *)pthread->virt2real(r4), &stat_native);
      
    conv_stat32_native2glibc(&stat_native, stat32p);
  }
#endif

  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_fstat)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  err;

#ifdef DEBUG_VERBOSE
  printf("mint_fstat(%ld, 0x%lx)\n", r4, r5);
#endif
  if(r5 == 0) {
    fatal("fstat called with a null pointer\n");
  }

#if (defined TLS) || (defined TASKSCALAR)
  {
    struct glibc_stat64 stat64p;
    struct stat stat_native;
    char tmpbuff[2048];
      
    err = fstat(r4, &stat_native);
    conv_stat64_native2glibc(&stat_native, &stat64p);

    rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) r5, &stat64p, sizeof(struct glibc_stat64));
  }
#else
  {
    struct glibc_stat64 *stat64p = (struct glibc_stat64 *)pthread->virt2real(r5);
    struct stat stat_native;
      
    err = fstat(r4, &stat_native);
      
    conv_stat64_native2glibc(&stat_native, stat64p);
  }
#endif

  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_fstat64)
{
  return mint_fstat(picode, pthread);
}

/* ARGSUSED */
OP(mint_xstat)
{
  long r4, r5, r6;

  r4 = REGNUM(4);
  r5 = REGNUM(5);
  r6 = REGNUM(6);

#ifdef DEBUG_VERBOSE
  printf("mint_xstat(0x%08lx,0x%08lx,0x%08lx)\n", r4, r5, r6);
#endif

  /* calling fstat ignoring the _stat_ver in glibc */
  REGNUM(4) = REGNUM(5);
  REGNUM(5) = REGNUM(6);
  return mint_stat(picode,pthread);
}

OP(mint_xstat64)
{
  long r4, r5, r6;

  r4 = REGNUM(4);
  r5 = REGNUM(5);
  r6 = REGNUM(6);

#ifdef DEBUG_VERBOSE
  printf("mint_xstat64(0x%08lx,0x%08lx,0x%08lx)\n", r4, r5, r6);
#endif

  /* calling fstat ignoring the _stat_ver in glibc */
  REGNUM(4) = REGNUM(5);
  REGNUM(5) = REGNUM(6);
  return mint_stat(picode, pthread);
}

OP(mint_fxstat64)
{
  long r4, r5, r6;

  r4 = REGNUM(4);
  r5 = REGNUM(5);
  r6 = REGNUM(6);

#ifdef DEBUG_VERBOSE
  printf("mint_fxstat64(0x%08lx,0x%08lx,0x%08lx)\n", r4, r5, r6);
#endif

  /* calling fstat ignoring the _stat_ver in glibc */
  REGNUM(4) = REGNUM(5);
  REGNUM(5) = REGNUM(6);
  return mint_fstat(picode,pthread);
}

/* ARGSUSED */
OP(mint_lstat64)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  err;

#ifdef DEBUG_VERBOSE
  printf("mint_lstat64()\n");
#endif
  if(r4 == 0 || r5 == 0) {
    fatal("lstat64 called with a null pointer\n");
  }

#if (defined TLS) || (defined TASKSCALAR)
  {
    struct glibc_stat64 stat64p;
    struct stat stat_native;
    char tmpbuff[2048];
      
    rsesc_OS_read_string(pthread->getPid(), picode->addr, tmpbuff, (const void *)r4, 2048);

    err = lstat(tmpbuff, &stat_native);
    conv_stat64_native2glibc(&stat_native, &stat64p);

    rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) r5, &stat64p, sizeof(struct glibc_stat64));
  }
#else
  {
    struct glibc_stat64 *stat64p = (struct glibc_stat64 *)pthread->virt2real(r5);
    struct stat stat_native;
      
    err = lstat((const char *)pthread->virt2real(r4), &stat_native);
      
    conv_stat64_native2glibc(&stat_native, stat64p);
  }
#endif

  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_stat64)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int  err;
  
#ifdef DEBUG_VERBOSE
  printf("mint_stat64()\n");
#endif
  if(r4 == 0 || r5 == 0) {
    fatal("stat64 called with a null pointer\n");
  }

#if (defined TLS) || (defined TASKSCALAR)
  {
    struct glibc_stat64 stat64p;
    struct stat stat_native;
    char tmpbuff[2048];
      
    rsesc_OS_read_string(pthread->getPid(), picode->addr, tmpbuff, (const void *)r4, 2048);

    err = stat(tmpbuff, &stat_native);
    conv_stat64_native2glibc(&stat_native, &stat64p);

    rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) r5, &stat64p, sizeof(struct glibc_stat64));
  }
#else
  {
    struct glibc_stat64 *stat64p = (struct glibc_stat64 *)pthread->virt2real(r5);
    struct stat stat_native;
      
    err = stat((const char *)pthread->virt2real(r4), &stat_native);
      
    conv_stat64_native2glibc(&stat_native, stat64p);
  }
#endif

  if(err == -1)
    pthread->setperrno(errno);
    
  REGNUM(2) = err;

  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_dup)
{
    long r4;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_dup()\n");
#endif
    r4 = REGNUM(4);

    err = dup(r4);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    else {
        if (err < MAX_FDNUM)
	  pthread->setFD(err, 1);
    }
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getcwd)
{
  long r4 = REGNUM(4);
  long r5 = REGNUM(5);
  int err;
  
#ifdef DEBUG_VERBOSE
  printf("mint_getcwd()");
#endif

  if(r4 == 0) {
    fatal("getcwd called with a null pointer\n");
  }

#if (defined TLS) || (defined TASKSCALAR)
  {
    char tmpbuf[4096];
    if (r5 > 4096)
      fatal("getcwd(): please increase tmpbuf to %ld\n", r5);
    err = (int)getcwd(tmpbuf, (size_t)r5);
    rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *)REGNUM(4), (const void *)tmpbuf, r5);
  }
#else
  {
    err = (int)getcwd((char *)pthread->virt2real(r4), (size_t)r5);
  }
#endif  

  if(err == 0)
    pthread->setperrno(errno);
  
  REGNUM(2) = err;

#ifdef DEBUG_VERBOSE
  printf("=%s\n", (char *)REGNUM(2));
#endif

  return addr2icode(REGNUM(31));
}


/* ARGSUSED */
OP(mint_pipe)
{
    long r4;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_pipe()\n");
#endif
    r4 = REGNUM(4);

    r4 = pthread->virt2real(r4);

#ifdef TASKSCALAR
    {
      char cad1[100];

      rsesc_OS_read_block(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 2*4);

      err = pipe((int *) cad1);
    }
#else
    err = pipe((int *) r4);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_symlink)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_symlink()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);

    r4 = pthread->virt2real(r4);
    r5 = pthread->virt2real(r5);
#ifdef TASKSCALAR
    {
      char cad1[1024];
      char cad2[1024];

      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 1024);
      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad2, (const void *) REGNUM(5), 1024);

      err = symlink((const char *) cad1, (const char *) cad2);
    }
#else
    err = symlink((const char *) r4, (const char *) r5);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_readlink)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_readlink()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    r4 = pthread->virt2real(r4);
#ifdef TASKSCALAR
    {
      char cad1[1024];
      char *cad2 = (char *)alloca(r6);
      
      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 1024);

      err = readlink(cad1, cad2, r6);

      if (r5)
	rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(5), cad2, r6);
    }
#else
    if (r5)
        r5 = pthread->virt2real(r5);

    err = readlink((const char *) r4, (char *)r5, r6);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_umask)
{
    long r4;

#ifdef DEBUG_VERBOSE
    printf("mint_umask()\n");
#endif
    r4 = REGNUM(4);

    REGNUM(2) = (long) umask((mode_t)r4);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getuid)
{
#ifdef DEBUG_VERBOSE
    printf("mint_getuid()\n");
#endif

    REGNUM(2) = (long) getuid();
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_geteuid)
{
#ifdef DEBUG_VERBOSE
    printf("mint_geteuid()\n");
#endif

    REGNUM(2) = (long) geteuid();
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getgid)
{
#ifdef DEBUG_VERBOSE
    printf("mint_getgid()\n");
#endif

    REGNUM(2) = (long) getgid();
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getegid)
{
#ifdef DEBUG_VERBOSE
    printf("mint_getegid()\n");
#endif

    REGNUM(2) = (long) getegid();
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getdomainname)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_getdomainname()\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);

#if (defined SUNOS) || (defined AIX)
    fatal("getdomainname not supported\n");
#else
#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(r5);
      
      err = getdomainname(cad1, r5);

      if (r4)
	rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(4), cad1, r5);
    }
#else
    if (r4)
        r4 = pthread->virt2real(r4);

    err = getdomainname((char *) r4, r5);
#endif
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_setdomainname)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_setdomainname()\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);

#if (defined SUNOS) || (defined AIX)
    fatal("setdomainname not supported\n");
#else
#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(r5);
      
      if (r4)
	rsesc_OS_read_block(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), r5);
      else
	cad1 = 0;

      err = setdomainname(cad1, r5);
    }
#else
    if (r4)
        r4 = pthread->virt2real(r4);

    err = setdomainname((char *) r4, r5);
#endif
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_gethostname)
{
    long r4, r5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_gethostname()\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);

#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(r5);
      
      err = gethostname(cad1, r5);

      if (r4)
	rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(4), cad1, r5);
    }
#else
    if (r4)
        r4 = pthread->virt2real(r4);

    err = gethostname((char *) r4, r5);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_socket)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_socket()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    err = socket(r4, r5, r6);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    else {
        if (err < MAX_FDNUM)
	  pthread->setFD(err, 1);
    }
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_connect)
{
  long r4, r5, r6;
  int err;

#ifdef DEBUG_VERBOSE
  printf("mint_connect()\n");
#endif
  r4 = REGNUM(4);
  r5 = REGNUM(5);
  r6 = REGNUM(6);

#ifdef TASKSCALAR
  fatal("connect not implemented with TLS\n");
#endif
  if (r5)
    r5 = pthread->virt2real(r5);

#ifdef sparc
  err = connect(r4, (struct sockaddr *) r5, r6);
#else
  err = connect(r4, (const struct sockaddr *) r5, r6);
#endif
  REGNUM(2) = err;
  if (err == -1)
    pthread->setperrno(errno);
  else {
    if (err < MAX_FDNUM)
      pthread->setFD(err, 1);
  }
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_send)
{
    long r4, r5, r6, r7;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_send()\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);

#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(r6);
      
      if (r5)
	rsesc_OS_read_block(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(5), r6);
      else
	cad1 = 0;

      err = send(r4, (const void *) cad1, r6, r7);
    }
#else
    if (r5)
        r5 = pthread->virt2real(r5);

    err = send(r4, (const void *) r5, r6, r7);
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_sendto)
{
    long r4, r5, r6, r7;
    long *sp, arg5, arg6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_sendto()\n");
#endif

#ifdef TASKSCALAR
    fatal("sendto not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);
    sp = (long *) pthread->virt2real(REGNUM(29));
    arg5=SWAP_WORD(sp[4]);
    arg6=SWAP_WORD(sp[5]);

    if (r5)
        r5 = pthread->virt2real(r5);
    if (arg5)
        arg5 = pthread->virt2real(arg5);

    err = sendto(r4, (const void *) r5, r6, r7, (struct sockaddr *) arg5, arg6);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_sendmsg)
{
    long r4, r5, r6;
    int err;
    unsigned i;
    struct msghdr *msg;
    struct iovec *iovp;

#ifdef DEBUG_VERBOSE
    printf("mint_sendmsg()\n");
#endif

#ifdef TASKSCALAR
    fatal("sendmsg not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    if (r5) {
        r5 = pthread->virt2real(r5);
        msg = (struct msghdr *) r5;
        if (msg->msg_name)
	  msg->msg_name = (char *) pthread->virt2real((unsigned long) msg->msg_name);
        iovp = msg->msg_iov;
        if (iovp) {
            iovp = (struct iovec *) pthread->virt2real((unsigned long) iovp);
            msg->msg_iov = iovp;
            for (i = 0; i < msg->msg_iovlen; i++, iovp++)
                if (iovp->iov_base)
                    iovp->iov_base = (char *) pthread->virt2real((unsigned long) iovp->iov_base);
        }
#ifdef IRIX64
	/* This is BSB4.3 neither implemented in Linux nor AIX */
        if (msg->msg_accrights)
            msg->msg_accrights = (char *) pthread->virt2real(msg->msg_accrights);
#endif
    }

#ifdef sparc
    err = sendmsg(r4, (struct msghdr *) r5, r6);
#else
    err = sendmsg(r4, (const struct msghdr *) r5, r6);
#endif

    if (r5) {
        msg = (struct msghdr *) r5;
        if (msg->msg_name)
	  msg->msg_name = (char *) pthread->real2virt((unsigned long) msg->msg_name);
        iovp = msg->msg_iov;
        if (iovp) {
            for (i = 0; i < msg->msg_iovlen; i++, iovp++)
                if (iovp->iov_base)
		  iovp->iov_base = (char *) pthread->real2virt((unsigned long) iovp->iov_base);
            msg->msg_iov = (struct iovec *) pthread->real2virt((unsigned long) msg->msg_iov);
        }
#ifdef IRIX64
	/* This is BSB4.3 neither implemented in Linux nor AIX */
        if (msg->msg_accrights)
            msg->msg_accrights = (char *) pthread->real2virt(msg->msg_accrights);
#endif
    }

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_recv)
{
    long r4, r5, r6, r7;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_recv()\n");
#endif

#ifdef TASKSCALAR
    fatal("recv not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);

    if (r5)
        r5 = pthread->virt2real(r5);

    err = recv(r4, (void *) r5, r6, r7);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_recvfrom)
{
    long r4, r5, r6, r7;
    long *sp, arg5, arg6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_recvfrom()\n");
#endif

#ifdef TASKSCALAR
    fatal("recvfrom not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);
    sp = (long *) pthread->virt2real(REGNUM(29));
    arg5 = SWAP_WORD(sp[4]);
    arg6 = SWAP_WORD(sp[5]);

    if (r5)
        r5 = pthread->virt2real(r5);
    if (arg5)
        arg5 = pthread->virt2real(arg5);
    if (arg6)
        arg6 = pthread->virt2real(arg6);

    err = recvfrom(r4, (void *) r5, r6, r7, (struct sockaddr *) arg5, (socklen_t *) arg6);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_recvmsg)
{
    long r4, r5, r6;
    int err;
    unsigned i;
    struct msghdr *msg;
    struct iovec *iovp;

#ifdef DEBUG_VERBOSE
    printf("mint_recvmsg()\n");
#endif

#ifdef TASKSCALAR
    fatal("recvmsg not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

    if (r5) {
        r5 = pthread->virt2real(r5);
        msg = (struct msghdr *) r5;
        if (msg->msg_name)
            msg->msg_name = (char *) pthread->virt2real((unsigned long) msg->msg_name);
        iovp = msg->msg_iov;
        if (iovp) {
            iovp = (struct iovec *) pthread->virt2real((unsigned long) iovp);
            msg->msg_iov = iovp;
            for (i = 0; i < msg->msg_iovlen; i++, iovp++)
                if (iovp->iov_base)
                    iovp->iov_base = (char *) pthread->virt2real((unsigned long) iovp->iov_base);
        }
#ifdef IRIX64
	/* This is BSB4.3 neither implemented in Linux nor AIX */
        if (msg->msg_accrights)
            msg->msg_accrights = (char *) pthread->virt2real(msg->msg_accrights);
#endif
    }

    err = recvmsg(r4, (struct msghdr *) r5, r6);
    
    if (r5) {
        msg = (struct msghdr *) r5;
        if (msg->msg_name)
            msg->msg_name = (char *) pthread->real2virt((unsigned long) msg->msg_name);
        iovp = msg->msg_iov;
        if (iovp) {
            for (i = 0; i < msg->msg_iovlen; i++, iovp++)
                if (iovp->iov_base)
                    iovp->iov_base = (char *) pthread->real2virt((unsigned long) iovp->iov_base);
            msg->msg_iov = (struct iovec *) pthread->real2virt((unsigned long) msg->msg_iov);
        }
#ifdef IRIX64
	/* This is BSB4.3 neither implemented in Linux nor AIX */
        if (msg->msg_accrights)
            msg->msg_accrights = (char *) pthread->real2virt(msg->msg_accrights);
#endif
    }

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_select)
{
    long r4, r5, r6, r7;
    long *sp, arg5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_select()\n");
#endif

#ifdef TASKSCALAR
    fatal("select not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);
    sp = (long *) pthread->virt2real(REGNUM(29));
    arg5 = SWAP_WORD(sp[4]);

    if (r5)
        r5 = pthread->virt2real(r5);
    if (r6)
        r6 = pthread->virt2real(r6);
    if (r7)
        r7 = pthread->virt2real(r7);
    if (arg5)
        arg5 = pthread->virt2real(arg5);

    err = select(r4, (fd_set *) r5, (fd_set *) r6, (fd_set *) r7, (struct timeval *) arg5);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getsockopt)
{
    long r4, r5, r6, r7;
    long *sp, arg5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_getsockopt()\n");
#endif

#ifdef TASKSCALAR
    fatal("getsockopt not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);
    sp = (long *) pthread->virt2real(REGNUM(29));
    arg5 = SWAP_WORD(sp[4]);

    if (r7)
        r7 = pthread->virt2real(r7);
    if (arg5)
        arg5 = pthread->virt2real(arg5);

    err = getsockopt(r4, r5, r6, (void *) r7, (socklen_t *) arg5);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_setsockopt)
{
    long r4, r5, r6, r7;
    long *sp, arg5;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_setsockopt()\n");
#endif

#ifdef TASKSCALAR
    fatal("setsockopt not implemented with TLS\n");
#endif

    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);
    r7 = REGNUM(7);
    sp = (long *) pthread->virt2real(REGNUM(29));
    arg5 = SWAP_WORD(sp[4]);

    if (r7)
        r7 = pthread->virt2real(r7);

    err = setsockopt(r4, r5, r6, (const void *) r7, arg5);
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_oserror)
{
#ifdef DEBUG_VERBOSE
  printf("mint_oserror()\n");
#endif

  REGNUM(2) = pthread->getperrno();
  return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_setoserror)
{
    long r4;

#ifdef DEBUG_VERBOSE
    printf("mint_oserror()\n");
#endif
    r4 = REGNUM(4);

    REGNUM(2) = pthread->getperrno();
    pthread->setperrno(r4);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_perror)
{
    long r4;

#ifdef DEBUG_VERBOSE
    printf("mint_oserror()\n");
#endif

    r4 = REGNUM(4);
    r4 = pthread->virt2real(r4);

    errno = pthread->getperrno();
#ifdef TASKSCALAR
    {
      char cad1[100];
      
      rsesc_OS_read_string(pthread->getPid(), picode->addr, cad1, (const void *) REGNUM(4), 100);

      perror((char *) cad1);
    }
#else
    perror((char *) r4);
#endif

    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_times)
{
    int err;

#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(sizeof(struct tms));
      
      err = (long) times((struct tms *)cad1);

      rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(4), cad1 , sizeof(struct tms));
    }
#else
    long r4 = REGNUM(4);

    r4 = pthread->virt2real(r4);
    err = (long) times((struct tms *)r4);
#endif

#ifdef DEBUG_VERBOSE
    printf("mint_times()\n");
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_time)
{
    long r4;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_time()\n");
#endif
    r4 = REGNUM(4);

#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(sizeof(time_t));
      
      err = (long) time((time_t *)cad1);

      if (r4)
	rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(4), cad1, sizeof(time_t));
    }
#else
    if (r4)
        r4 = pthread->virt2real(r4);

    err = (long) time((time_t *) r4);
#endif
    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getdents)
{
    long r4, r5, r6;
    int err;

#ifdef DEBUG_VERBOSE
    printf("mint_getdents()\n");
#endif
    r4 = REGNUM(4);
    r5 = REGNUM(5);
    r6 = REGNUM(6);

#if (defined LINUX) && (defined __i386__)
#ifdef TASKSCALAR
    {
      char *cad1 = (char *)alloca(r6);
      
      err = (long) getdents(r4, (struct dirent *)cad1, r6);

      rsesc_OS_write_block(pthread->getPid(), picode->addr, (void *) REGNUM(5), cad1, r6);
    }
#else
    r5 = pthread->virt2real(r5);

    err = (long) getdents(r4, (struct dirent *)r5, r6);
#endif
#else
    err = -1;
    fatal("getdents is only implemented in Linux i386.");
#endif

    REGNUM(2) = err;
    if (err == -1)
        pthread->setperrno(errno);
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_getdtablesize)
{
#ifdef DEBUG_VERBOSE
    printf("mint_getdtablesize()\n");
#endif
    REGNUM(2) = MAX_FDNUM; 
    return addr2icode(REGNUM(31));
}

/* ARGSUSED */
OP(mint_syssgi)
{
  long r4, r5, r6;

#ifdef DEBUG_VERBOSE
  printf("mint_syssgi()\n");
#endif
  r4 = REGNUM(4);
  switch(r4) {
  case SGI_SYSID:
  case SGI_RDNAME:
    /* case SGI_PROCSZ: */
  case SGI_TUNE:
  case SGI_IDBG:
  case SGI_SETNVRAM:
  case SGI_GETNVRAM:
    fatal("mint_syssgi: command 0x%x not supported yet.\n", r4);
    break;
  case SGI_RUSAGE:
  case SGI_SSYNC:
  case SGI_BDFLUSHCNT:
  case SGI_QUERY_CYCLECNTR:
  case SGI_SPROFIL:
  case SGI_SETTIMETRIM:
  case SGI_GETTIMETRIM:
    fatal("mint_syssgi: command 0x%x not supported yet.\n", r4);
    break;
  case 103:
    /* What does arg 103 do? It occurs in startup code on some SGI
     * machines. For now, just do nothing.
     */
    REGNUM(2) = 0;
    break;
  case SGI_INVENT:
    r5 = REGNUM(5);
    if (r5 == SGI_INV_SIZEOF)
      /* return a non-zero size */
      REGNUM(2) = 20;
    else if (r5 == SGI_INV_READ) {
      /* for now, just do nothing */
      r6 = REGNUM(6);
      if (r6)
	r6 = pthread->virt2real(r6);
      REGNUM(2) = 0;
    } else
      fatal("mint_syssgi: SGI_INVENT: arg 0x%x not expected.\n", r5);
    break;
  case SGI_USE_FP_BCOPY:
    REGNUM(2) = 0;
    break;
  default:
    fatal("mint_syssgi: command 0x%x not expected.\n", r4);
    break;
  }
  return addr2icode(REGNUM(31));
}

OP(mint_sesc_get_num_cpus)
{
  // Do the actual call (should not context-switch)
  ID(Pid_t thePid=pthread->getPid());
  ValueGPR retVal=rsesc_get_num_cpus();
  I(pthread->getPid()==thePid);
  // Set the return value
  pthread->setGPR(RetValGPR,retVal);
  // Return from the call
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_do_nothing)
{
  /* Next instruction is after this call */
  return addr2icode(REGNUM(31));
}

// Heap related functions

OP(mint_malloc)
{
  // This call should not context-switch
  ID(Pid_t thePid=pthread->getPid());
  // Get size parameter
  size_t size=pthread->getGPR(Arg1stGPR);
  if(!size) size++;
#if (defined TLS)
  tls::Epoch *epoch=pthread->getEpoch();
  I(epoch==tls::Epoch::getEpoch(pthread->getPid()));
  if(epoch){
    SysCall *sysCall=epoch->nextSysCall();
    if(sysCall){
      I(dynamic_cast<SysCallMalloc *>(sysCall)!=0);
      sysCall->redo(pthread);
    }else{
      epoch->makeSysCall(new SysCallMalloc(pthread));
    }
  }else{
#endif
    RAddr addr=pthread->getHeapManager()->allocate(size);
    if(addr){
      // Map to logical address space
      addr=pthread->real2virt(addr);
    }else{
      // Set errno to be POSIX compliant
      pthread->setErrno(ENOMEM);
    }
    pthread->setGPR(RetValGPR,addr);
#if (defined TLS)
  }
#endif
  // Return from the call (and check for context switch)
  I(pthread->getPid()==thePid);
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_calloc)
{
  // This call should not context-switch
  ID(Pid_t thePid=pthread->getPid());
  // Get parameters
  size_t nmemb=pthread->getGPR(Arg1stGPR);
  size_t size =pthread->getGPR(Arg2ndGPR);
  // The total size of the allocation
  size_t totalSize=nmemb*size;
  if(!totalSize) totalSize++;
  pthread->setGPR(Arg1stGPR,totalSize);
#if (defined TLS)
  tls::Epoch *epoch=tls::Epoch::getEpoch(pthread->getPid());
  if(epoch){
    SysCall *sysCall=epoch->nextSysCall();
    if(sysCall){
      I(dynamic_cast<SysCallMalloc *>(sysCall)!=0);
      sysCall->redo(pthread);
    }else{
      epoch->makeSysCall(new SysCallMalloc(pthread));
    }
  }else{
#endif          
    RAddr addr=pthread->getHeapManager()->allocate(totalSize);
    if(addr){
      // Map to logical address space
      addr=pthread->real2virt(addr);
    }else{
      // Set errno to be POSIX compliant
      pthread->setErrno(ENOMEM);
    }
    pthread->setGPR(RetValGPR,addr);
#if (defined TLS)
  }
#endif
  // Clear the allocated region
  Address blockPtr=pthread->getGPR(RetValGPR);
  if(blockPtr){
    size_t blockSize=nmemb*size;
#if (defined TLS)
    unsigned long long zero=0ull;
    I((blockPtr&(sizeof(zero)-1))==0);
    while(blockSize>=sizeof(zero)){
      rsesc_OS_write_block(pthread->getPid(),picode->addr,(void *)blockPtr,&zero,sizeof(zero));
      blockPtr+=sizeof(zero);
      blockSize-=sizeof(zero);
    }
    if(blockSize){
      rsesc_OS_write_block(pthread->getPid(),picode->addr,(void *)blockPtr,&zero,blockSize);
    }
#else
    memset((void *)pthread->virt2real(blockPtr),0,blockSize);
#endif
  }
  // Return from the call (and check for context switch)
  I(pthread->getPid()==thePid);
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_free)
{
  // This call should not context-switch
  ID(Pid_t thePid=pthread->getPid());
  // Get address parameter
  RAddr addr=pthread->getGPR(Arg1stGPR);
  if(addr){
#if (defined TLS)
    tls::Epoch *epoch=tls::Epoch::getEpoch(pthread->getPid());
    if(epoch){
      SysCall *sysCall=epoch->nextSysCall();
      if(sysCall){
	I(dynamic_cast<SysCallFree *>(sysCall)!=0);
        sysCall->redo(pthread);
      }else{
        epoch->makeSysCall(new SysCallFree(pthread));
      }
    }else{
#endif
      // Map to real address space and deallocate
      addr=pthread->virt2real(addr);
      pthread->getHeapManager()->deallocate(addr);
#if (defined TLS)
    }
#endif
  }
  // Return from the call (and check for context switch)
  I(pthread->getPid()==thePid);
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

OP(mint_realloc)
{
  // This call should not context-switch
  ID(Pid_t thePid=pthread->getPid());
  // Get parameters
  RAddr oldAddr=pthread->getGPR(Arg1stGPR);
  size_t  newSize=pthread->getGPR(Arg2ndGPR);
  I(oldAddr||newSize);
  if(oldAddr==0){
    // Equivalent to malloc
    pthread->setGPR(Arg1stGPR,newSize);
    mint_malloc(picode,pthread);
    pthread->setGPR(Arg1stGPR,0);
  }else if(newSize==0){
    // Equivalent to free, but returns NULL
    mint_free(picode,pthread);
    pthread->setGPR(RetValGPR,0);
  }else{
    RAddr  oldAddrR=pthread->virt2real(oldAddr);
    size_t oldSize =pthread->getHeapManager()->deallocate(oldAddrR);
    RAddr  newAddrR=pthread->getHeapManager()->allocate(oldAddrR,newSize);
    RAddr  newAddr =pthread->real2virt(newAddrR);
    if(newAddr!=oldAddr){
      // Block could not grow in place, must copy old data
      I(newSize>oldSize);
      memmove((void *)newAddrR,(void *)oldAddrR,oldSize);
    }
    pthread->setGPR(RetValGPR,newAddr);    
  }
  // Return from the call (and check for context switch)
  I(pthread->getPid()==thePid);
  return addr2icode(pthread->getGPR(RetAddrGPR));
}

#ifdef ATOMIC
void rsesc_start_transaction(int pid);
int rsesc_commit_transaction(int pid);
void rsesc_thread_exit(int pid);

OP(mint_sesc_start_transaction)
{
  int pid=pthread->getPid();

  rsesc_start_transaction(pid);

  return addr2icode(REGNUM(31));
}

OP(mint_sesc_commit_transaction)
{
  int pid=pthread->getPid();

  rsesc_commit_transaction(pid);

  return addr2icode(REGNUM(31));
}
#endif

#ifdef SESC_LOCKPROFILE
void rsesc_startlock(int pid);
void rsesc_endlock(int pid);
void rsesc_startlock2(int pid);
void rsesc_endlock2(int pid);

OP(mint_sesc_startlock)
{
  rsesc_startlock(pthread->getPid());
  return addr2icode(REGNUM(31));
}

OP(mint_sesc_endlock)
{
  rsesc_endlock(pthread->getPid());
  return addr2icode(REGNUM(31));
}

OP(mint_sesc_startlock2)
{
  rsesc_startlock2(pthread->getPid());
  return addr2icode(REGNUM(31));
}

OP(mint_sesc_endlock2)
{
  rsesc_endlock2(pthread->getPid());
  return addr2icode(REGNUM(31));
}
#endif
