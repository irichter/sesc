#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/resource.h>
// Define Rlimit resource numbers
enum Mips32_RlimitNums {
  Mips32_RLIMIT_AS      = 0x6,
  Mips32_RLIMIT_CORE    = 0x4,
  Mips32_RLIMIT_CPU     = 0x0,
  Mips32_RLIMIT_DATA    = 0x2,
  Mips32_RLIMIT_FSIZE   = 0x1,
  Mips32_RLIMIT_LOCKS   = 0xa,
  Mips32_RLIMIT_MEMLOCK = 0x9,
  Mips32_RLIMIT_NOFILE  = 0x5,
  Mips32_RLIMIT_NPROC   = 0x8,
  Mips32_RLIMIT_RSS     = 0x7,
  Mips32_RLIMIT_STACK   = 0x3,
};
#include <sys/mman.h>
// Define Protection flags (mmap)
enum Mips32_MmapProts {
  Mips32_PROT_NONE  = 0x0,
  Mips32_PROT_READ  = 0x1,
  Mips32_PROT_WRITE = 0x2,
  Mips32_PROT_EXEC  = 0x4,
};
// Define Mmap flags
enum Mips32_MmapFlags {
  Mips32_MAP_FIXED     = 0x10, // Flag
  Mips32_MAP_SHARED    = 0x1, // Flag
  Mips32_MAP_PRIVATE   = 0x2, // Flag
  Mips32_MAP_ANONYMOUS = 0x800, // Flag
};
// Define Mremap flags
enum Mips32_MremapFlags {
  Mips32_MREMAP_MAYMOVE = 0x1, // Flag
};
#include <fcntl.h>
// Define File open flags
enum Mips32_OpenFlags {
  Mips32_O_ACCMODE = 0x3, // Mask
  // These choices are masked with O_ACCMODE
  Mips32_O_RDONLY  = 0x0,
  Mips32_O_WRONLY  = 0x1,
  Mips32_O_RDWR    = 0x2,
  // End of choices masked with O_ACCMODE  
  Mips32_O_CREAT     = 0x100, // Flag
  Mips32_O_EXCL      = 0x400, // Flag
  Mips32_O_NOCTTY    = 0x800, // Flag
  Mips32_O_TRUNC     = 0x200, // Flag
  Mips32_O_APPEND    = 0x8, // Flag
  Mips32_O_NONBLOCK  = 0x80, // Flag
  Mips32_O_SYNC      = 0x10, // Flag
  Mips32_O_NOFOLLOW  = 0x20000, // Flag
  Mips32_O_DIRECTORY = 0x10000, // Flag
  Mips32_O_DIRECT    = 0x8000, // Flag
  Mips32_O_ASYNC     = 0x1000, // Flag
  Mips32_O_LARGEFILE = 0x2000, // Flag
};
#include <sys/ioctl.h>
// Define ioctl requests
enum Mips32_IoctlReq {
  Mips32_TCGETS = 0x540d,
};
// Define fcntl commands
enum Mips32_FcntlCmd {
  Mips32_F_DUPFD = 0x0,
  Mips32_F_GETFD = 0x1,
  Mips32_F_SETFD = 0x2,
  Mips32_F_GETFL = 0x3,
  Mips32_F_SETFL = 0x4,
};
// Define fcntl flags
enum Mips32_FcntlFlag {
  Mips32_FD_CLOEXEC = 0x1, // Flag
};
#include <errno.h>
// Define Error numbers
enum Mips32_ErrNums {
  Mips32_ENOMEM    = 0xc,
  Mips32_EPERM     = 0x1,
  Mips32_EBADF     = 0x9,
  Mips32_EFAULT    = 0xe,
  Mips32_ENOENT    = 0x2,
  Mips32_EEXIST    = 0x11,
  Mips32_EISDIR    = 0x15,
  Mips32_ESPIPE    = 0x1d,
  Mips32_EACCES    = 0xd,
  Mips32_EINVAL    = 0x16,
  Mips32_EOVERFLOW = 0x4f,
  Mips32_ENOTTY    = 0x19,
};
#include <signal.h>
// Define Signal numbers
enum Mips32_SigNums {
  Mips32_SIGHUP = 0x1,
  Mips32_SIGINT = 0x2,
  Mips32_SIGQUIT = 0x3,
  Mips32_SIGILL = 0x4,
  Mips32_SIGTRAP = 0x5,
  Mips32_SIGABRT = 0x6,
  Mips32_SIGBUS = 0xa,
  Mips32_SIGFPE = 0x8,
  Mips32_SIGKILL = 0x9,
  Mips32_SIGUSR1 = 0x10,
  Mips32_SIGUSR2 = 0x11,
  Mips32_SIGSEGV = 0xb,
  Mips32_SIGPIPE = 0xd,
  Mips32_SIGALRM = 0xe,
  Mips32_SIGTERM = 0xf,
  Mips32_SIGCHLD = 0x12,
  Mips32_SIGCONT = 0x19,
  Mips32_SIGSTOP = 0x17,
  Mips32_SIGTSTP = 0x18,
};
// Convert Rlimit resource numbers to native
int toNativeRlimitNums(int val){
  int retVal=0;
  switch(val){
    case Mips32_RLIMIT_AS     : retVal+=RLIMIT_AS     ; break;
    case Mips32_RLIMIT_CORE   : retVal+=RLIMIT_CORE   ; break;
    case Mips32_RLIMIT_CPU    : retVal+=RLIMIT_CPU    ; break;
    case Mips32_RLIMIT_DATA   : retVal+=RLIMIT_DATA   ; break;
    case Mips32_RLIMIT_FSIZE  : retVal+=RLIMIT_FSIZE  ; break;
    case Mips32_RLIMIT_LOCKS  : retVal+=RLIMIT_LOCKS  ; break;
    case Mips32_RLIMIT_MEMLOCK: retVal+=RLIMIT_MEMLOCK; break;
    case Mips32_RLIMIT_NOFILE : retVal+=RLIMIT_NOFILE ; break;
    case Mips32_RLIMIT_NPROC  : retVal+=RLIMIT_NPROC  ; break;
    case Mips32_RLIMIT_RSS    : retVal+=RLIMIT_RSS    ; break;
    case Mips32_RLIMIT_STACK  : retVal+=RLIMIT_STACK  ; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Protection flags (mmap) to native
int toNativeMmapProts(int val){
  int retVal=0;
  switch(val){
    case Mips32_PROT_NONE : retVal+=PROT_NONE ; break;
    case Mips32_PROT_READ : retVal+=PROT_READ ; break;
    case Mips32_PROT_WRITE: retVal+=PROT_WRITE; break;
    case Mips32_PROT_EXEC : retVal+=PROT_EXEC ; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Mmap flags to native
int toNativeMmapFlags(int val){
  int retVal=0;
  if(val&Mips32_MAP_FIXED    ){
    retVal|=MAP_FIXED    ;
    val^=Mips32_MAP_FIXED    ;
  }
  if(val&Mips32_MAP_SHARED   ){
    retVal|=MAP_SHARED   ;
    val^=Mips32_MAP_SHARED   ;
  }
  if(val&Mips32_MAP_PRIVATE  ){
    retVal|=MAP_PRIVATE  ;
    val^=Mips32_MAP_PRIVATE  ;
  }
  if(val&Mips32_MAP_ANONYMOUS){
    retVal|=MAP_ANONYMOUS;
    val^=Mips32_MAP_ANONYMOUS;
  }
  I(val==0);
  return retVal;
}
// Convert Mremap flags to native
int toNativeMremapFlags(int val){
  int retVal=0;
  if(val&Mips32_MREMAP_MAYMOVE){
    retVal|=MREMAP_MAYMOVE;
    val^=Mips32_MREMAP_MAYMOVE;
  }
  I(val==0);
  return retVal;
}
// Convert File open flags to native
int toNativeOpenFlags(int val){
  int retVal=0;
  switch(val&Mips32_O_ACCMODE){
    case Mips32_O_RDONLY : retVal|=O_RDONLY ; break;
    case Mips32_O_WRONLY : retVal|=O_WRONLY ; break;
    case Mips32_O_RDWR   : retVal|=O_RDWR   ; break;
    default: fail("Unknown value %d (0x%x) for mask Mips32_O_ACCMODE  \n",val,val);
  }
  val^=(val&Mips32_O_ACCMODE  );
  if(val&Mips32_O_CREAT    ){
    retVal|=O_CREAT    ;
    val^=Mips32_O_CREAT    ;
  }
  if(val&Mips32_O_EXCL     ){
    retVal|=O_EXCL     ;
    val^=Mips32_O_EXCL     ;
  }
  if(val&Mips32_O_NOCTTY   ){
    retVal|=O_NOCTTY   ;
    val^=Mips32_O_NOCTTY   ;
  }
  if(val&Mips32_O_TRUNC    ){
    retVal|=O_TRUNC    ;
    val^=Mips32_O_TRUNC    ;
  }
  if(val&Mips32_O_APPEND   ){
    retVal|=O_APPEND   ;
    val^=Mips32_O_APPEND   ;
  }
  if(val&Mips32_O_NONBLOCK ){
    retVal|=O_NONBLOCK ;
    val^=Mips32_O_NONBLOCK ;
  }
  if(val&Mips32_O_SYNC     ){
    retVal|=O_SYNC     ;
    val^=Mips32_O_SYNC     ;
  }
  if(val&Mips32_O_NOFOLLOW ){
    retVal|=O_NOFOLLOW ;
    val^=Mips32_O_NOFOLLOW ;
  }
  if(val&Mips32_O_DIRECTORY){
    retVal|=O_DIRECTORY;
    val^=Mips32_O_DIRECTORY;
  }
  if(val&Mips32_O_DIRECT   ){
    retVal|=O_DIRECT   ;
    val^=Mips32_O_DIRECT   ;
  }
  if(val&Mips32_O_ASYNC    ){
    retVal|=O_ASYNC    ;
    val^=Mips32_O_ASYNC    ;
  }
  if(val&Mips32_O_LARGEFILE){
    retVal|=O_LARGEFILE;
    val^=Mips32_O_LARGEFILE;
  }
  I(val==0);
  return retVal;
}
// Convert ioctl requests to native
int toNativeIoctlReq(int val){
  int retVal=0;
  switch(val){
    case Mips32_TCGETS: retVal+=TCGETS; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert fcntl commands to native
int toNativeFcntlCmd(int val){
  int retVal=0;
  switch(val){
    case Mips32_F_DUPFD: retVal+=F_DUPFD; break;
    case Mips32_F_GETFD: retVal+=F_GETFD; break;
    case Mips32_F_SETFD: retVal+=F_SETFD; break;
    case Mips32_F_GETFL: retVal+=F_GETFL; break;
    case Mips32_F_SETFL: retVal+=F_SETFL; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert fcntl flags to native
int toNativeFcntlFlag(int val){
  int retVal=0;
  if(val&Mips32_FD_CLOEXEC){
    retVal|=FD_CLOEXEC;
    val^=Mips32_FD_CLOEXEC;
  }
  I(val==0);
  return retVal;
}
// Convert Error numbers to native
int toNativeErrNums(int val){
  int retVal=0;
  switch(val){
    case Mips32_ENOMEM   : retVal+=ENOMEM   ; break;
    case Mips32_EPERM    : retVal+=EPERM    ; break;
    case Mips32_EBADF    : retVal+=EBADF    ; break;
    case Mips32_EFAULT   : retVal+=EFAULT   ; break;
    case Mips32_ENOENT   : retVal+=ENOENT   ; break;
    case Mips32_EEXIST   : retVal+=EEXIST   ; break;
    case Mips32_EISDIR   : retVal+=EISDIR   ; break;
    case Mips32_ESPIPE   : retVal+=ESPIPE   ; break;
    case Mips32_EACCES   : retVal+=EACCES   ; break;
    case Mips32_EINVAL   : retVal+=EINVAL   ; break;
    case Mips32_EOVERFLOW: retVal+=EOVERFLOW; break;
    case Mips32_ENOTTY   : retVal+=ENOTTY   ; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Signal numbers to native
int toNativeSigNums(int val){
  int retVal=0;
  switch(val){
    case Mips32_SIGHUP: retVal+=SIGHUP; break;
    case Mips32_SIGINT: retVal+=SIGINT; break;
    case Mips32_SIGQUIT: retVal+=SIGQUIT; break;
    case Mips32_SIGILL: retVal+=SIGILL; break;
    case Mips32_SIGTRAP: retVal+=SIGTRAP; break;
    case Mips32_SIGABRT: retVal+=SIGABRT; break;
    case Mips32_SIGBUS: retVal+=SIGBUS; break;
    case Mips32_SIGFPE: retVal+=SIGFPE; break;
    case Mips32_SIGKILL: retVal+=SIGKILL; break;
    case Mips32_SIGUSR1: retVal+=SIGUSR1; break;
    case Mips32_SIGUSR2: retVal+=SIGUSR2; break;
    case Mips32_SIGSEGV: retVal+=SIGSEGV; break;
    case Mips32_SIGPIPE: retVal+=SIGPIPE; break;
    case Mips32_SIGALRM: retVal+=SIGALRM; break;
    case Mips32_SIGTERM: retVal+=SIGTERM; break;
    case Mips32_SIGCHLD: retVal+=SIGCHLD; break;
    case Mips32_SIGCONT: retVal+=SIGCONT; break;
    case Mips32_SIGSTOP: retVal+=SIGSTOP; break;
    case Mips32_SIGTSTP: retVal+=SIGTSTP; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Rlimit resource numbers from native
int fromNativeRlimitNums(int val){
  int retVal=0;
  switch(val){
    case RLIMIT_AS     : retVal+=Mips32_RLIMIT_AS     ; break;
    case RLIMIT_CORE   : retVal+=Mips32_RLIMIT_CORE   ; break;
    case RLIMIT_CPU    : retVal+=Mips32_RLIMIT_CPU    ; break;
    case RLIMIT_DATA   : retVal+=Mips32_RLIMIT_DATA   ; break;
    case RLIMIT_FSIZE  : retVal+=Mips32_RLIMIT_FSIZE  ; break;
    case RLIMIT_LOCKS  : retVal+=Mips32_RLIMIT_LOCKS  ; break;
    case RLIMIT_MEMLOCK: retVal+=Mips32_RLIMIT_MEMLOCK; break;
    case RLIMIT_NOFILE : retVal+=Mips32_RLIMIT_NOFILE ; break;
    case RLIMIT_NPROC  : retVal+=Mips32_RLIMIT_NPROC  ; break;
    case RLIMIT_RSS    : retVal+=Mips32_RLIMIT_RSS    ; break;
    case RLIMIT_STACK  : retVal+=Mips32_RLIMIT_STACK  ; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Protection flags (mmap) from native
int fromNativeMmapProts(int val){
  int retVal=0;
  switch(val){
    case PROT_NONE : retVal+=Mips32_PROT_NONE ; break;
    case PROT_READ : retVal+=Mips32_PROT_READ ; break;
    case PROT_WRITE: retVal+=Mips32_PROT_WRITE; break;
    case PROT_EXEC : retVal+=Mips32_PROT_EXEC ; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Mmap flags from native
int fromNativeMmapFlags(int val){
  int retVal=0;
  if(val&MAP_FIXED    ){ retVal|=Mips32_MAP_FIXED    ; val^=MAP_FIXED    ; }
  if(val&MAP_SHARED   ){ retVal|=Mips32_MAP_SHARED   ; val^=MAP_SHARED   ; }
  if(val&MAP_PRIVATE  ){ retVal|=Mips32_MAP_PRIVATE  ; val^=MAP_PRIVATE  ; }
  if(val&MAP_ANONYMOUS){ retVal|=Mips32_MAP_ANONYMOUS; val^=MAP_ANONYMOUS; }
  I(val==0);
  return retVal;
}
// Convert Mremap flags from native
int fromNativeMremapFlags(int val){
  int retVal=0;
  if(val&MREMAP_MAYMOVE){ retVal|=Mips32_MREMAP_MAYMOVE; val^=MREMAP_MAYMOVE; }
  I(val==0);
  return retVal;
}
// Convert File open flags from native
int fromNativeOpenFlags(int val){
  int retVal=0;
  switch(val&O_ACCMODE){
    case O_RDONLY : retVal|=Mips32_O_RDONLY ; break;
    case O_WRONLY : retVal|=Mips32_O_WRONLY ; break;
    case O_RDWR   : retVal|=Mips32_O_RDWR   ; break;
    default: fail("Unknown value %d (0x%x) for mask O_ACCMODE  \n",val,val);
  }
  val^=(val&O_ACCMODE  );
  if(val&O_CREAT    ){ retVal|=Mips32_O_CREAT    ; val^=O_CREAT    ; }
  if(val&O_EXCL     ){ retVal|=Mips32_O_EXCL     ; val^=O_EXCL     ; }
  if(val&O_NOCTTY   ){ retVal|=Mips32_O_NOCTTY   ; val^=O_NOCTTY   ; }
  if(val&O_TRUNC    ){ retVal|=Mips32_O_TRUNC    ; val^=O_TRUNC    ; }
  if(val&O_APPEND   ){ retVal|=Mips32_O_APPEND   ; val^=O_APPEND   ; }
  if(val&O_NONBLOCK ){ retVal|=Mips32_O_NONBLOCK ; val^=O_NONBLOCK ; }
  if(val&O_SYNC     ){ retVal|=Mips32_O_SYNC     ; val^=O_SYNC     ; }
  if(val&O_NOFOLLOW ){ retVal|=Mips32_O_NOFOLLOW ; val^=O_NOFOLLOW ; }
  if(val&O_DIRECTORY){ retVal|=Mips32_O_DIRECTORY; val^=O_DIRECTORY; }
  if(val&O_DIRECT   ){ retVal|=Mips32_O_DIRECT   ; val^=O_DIRECT   ; }
  if(val&O_ASYNC    ){ retVal|=Mips32_O_ASYNC    ; val^=O_ASYNC    ; }
  if(val&O_LARGEFILE){ retVal|=Mips32_O_LARGEFILE; val^=O_LARGEFILE; }
  I(val==0);
  return retVal;
}
// Convert ioctl requests from native
int fromNativeIoctlReq(int val){
  int retVal=0;
  switch(val){
    case TCGETS: retVal+=Mips32_TCGETS; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert fcntl commands from native
int fromNativeFcntlCmd(int val){
  int retVal=0;
  switch(val){
    case F_DUPFD: retVal+=Mips32_F_DUPFD; break;
    case F_GETFD: retVal+=Mips32_F_GETFD; break;
    case F_SETFD: retVal+=Mips32_F_SETFD; break;
    case F_GETFL: retVal+=Mips32_F_GETFL; break;
    case F_SETFL: retVal+=Mips32_F_SETFL; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert fcntl flags from native
int fromNativeFcntlFlag(int val){
  int retVal=0;
  if(val&FD_CLOEXEC){ retVal|=Mips32_FD_CLOEXEC; val^=FD_CLOEXEC; }
  I(val==0);
  return retVal;
}
// Convert Error numbers from native
int fromNativeErrNums(int val){
  int retVal=0;
  switch(val){
    case ENOMEM   : retVal+=Mips32_ENOMEM   ; break;
    case EPERM    : retVal+=Mips32_EPERM    ; break;
    case EBADF    : retVal+=Mips32_EBADF    ; break;
    case EFAULT   : retVal+=Mips32_EFAULT   ; break;
    case ENOENT   : retVal+=Mips32_ENOENT   ; break;
    case EEXIST   : retVal+=Mips32_EEXIST   ; break;
    case EISDIR   : retVal+=Mips32_EISDIR   ; break;
    case ESPIPE   : retVal+=Mips32_ESPIPE   ; break;
    case EACCES   : retVal+=Mips32_EACCES   ; break;
    case EINVAL   : retVal+=Mips32_EINVAL   ; break;
    case EOVERFLOW: retVal+=Mips32_EOVERFLOW; break;
    case ENOTTY   : retVal+=Mips32_ENOTTY   ; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Convert Signal numbers from native
int fromNativeSigNums(int val){
  int retVal=0;
  switch(val){
    case SIGHUP: retVal+=Mips32_SIGHUP; break;
    case SIGINT: retVal+=Mips32_SIGINT; break;
    case SIGQUIT: retVal+=Mips32_SIGQUIT; break;
    case SIGILL: retVal+=Mips32_SIGILL; break;
    case SIGTRAP: retVal+=Mips32_SIGTRAP; break;
    case SIGABRT: retVal+=Mips32_SIGABRT; break;
    case SIGBUS: retVal+=Mips32_SIGBUS; break;
    case SIGFPE: retVal+=Mips32_SIGFPE; break;
    case SIGKILL: retVal+=Mips32_SIGKILL; break;
    case SIGUSR1: retVal+=Mips32_SIGUSR1; break;
    case SIGUSR2: retVal+=Mips32_SIGUSR2; break;
    case SIGSEGV: retVal+=Mips32_SIGSEGV; break;
    case SIGPIPE: retVal+=Mips32_SIGPIPE; break;
    case SIGALRM: retVal+=Mips32_SIGALRM; break;
    case SIGTERM: retVal+=Mips32_SIGTERM; break;
    case SIGCHLD: retVal+=Mips32_SIGCHLD; break;
    case SIGCONT: retVal+=Mips32_SIGCONT; break;
    case SIGSTOP: retVal+=Mips32_SIGSTOP; break;
    case SIGTSTP: retVal+=Mips32_SIGTSTP; break;
    default: fail("Unknown value %d (0x%x)\n",val,val);
  }
  val=0;
  I(val==0);
  return retVal;
}
// Define Virtual address
typedef uint32_t Mips32_VAddr;
// Note: VAddr plays the role of native type void *
// Define Memory size
typedef uint32_t Mips32_VSize;
// Note: VSize plays the role of native type size_t
#include <sys/uio.h>

// Define iovec for readv/writev
typedef struct Mips32_iovec {
  Mips32_VAddr iov_base;
  Mips32_VSize iov_len ;
} Mips32_iovec;
#include <sys/resource.h>
// Define Resource limit size
typedef uint32_t Mips32_rlim_t;
// Note: rlim_t plays the role of native type rlim_t

// Define rlimit for getrlimit/setrlimit
typedef struct Mips32_rlimit {
  Mips32_rlim_t rlim_cur;
  Mips32_rlim_t rlim_max;
} Mips32_rlimit;

// Define timeval for rusage
typedef struct Mips32_timeval {
  int32_t tv_sec ;
  int32_t tv_usec;
} Mips32_timeval;

// Define rusage for getrusage/setrusage
typedef struct Mips32_rusage {
  Mips32_timeval ru_utime;
  Mips32_timeval ru_stime;
  int32_t ru_maxrss;
  int32_t ru_ixrss;
  int32_t ru_idrss;
  int32_t ru_isrss;
  int32_t ru_minflt;
  int32_t ru_majflt;
  int32_t ru_nswap;
  int32_t ru_inblock;
  int32_t ru_oublock;
  int32_t ru_msgsnd;
  int32_t ru_msgrcv;
  int32_t ru_nsignals;
  int32_t ru_nvcsw;
  int32_t ru_nivcsw;
} Mips32_rusage;
// Define Clock type
typedef int32_t Mips32_clock_t;
// Note: clock_t plays the role of native type clock_t
#include <sys/times.h>

// Define tms structure for times
typedef struct Mips32_tms {
  Mips32_clock_t tms_utime;
  Mips32_clock_t tms_stime;
  Mips32_clock_t tms_cutime;
  Mips32_clock_t tms_cstime;
} Mips32_tms;
#include <sys/utsname.h>

// Define utsname for uname
typedef struct Mips32_utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
} Mips32_utsname;
typedef int32_t Mips32_mode_t;
// Note: mode_t plays the role of native type mode_t
typedef int32_t Mips32_uid_t;
// Note: uid_t plays the role of native type uid_t
typedef int32_t Mips32_gid_t;
// Note: gid_t plays the role of native type gid_t
typedef int32_t Mips32_off_t;
// Note: off_t plays the role of native type off_t
typedef int64_t Mips32_off64_t;
// Note: off64_t plays the role of native type off64_t
typedef int32_t Mips32_time_t;
// Note: time_t plays the role of native type time_t
typedef int32_t Mips32_blksize_t;
// Note: blksize_t plays the role of native type blksize_t
typedef int32_t Mips32_blkcnt_t;
// Note: blkcnt_t plays the role of native type blkcnt_t
typedef int64_t Mips32_blkcnt64_t;
// Note: blkcnt64_t plays the role of native type blkcnt64_t
typedef int32_t Mips32_ino_t;
// Note: ino_t plays the role of native type ino_t
typedef int64_t Mips32_ino64_t;
// Note: ino64_t plays the role of native type ino64_t
typedef uint32_t Mips32_nlink_t;
// Note: nlink_t plays the role of native type nlink_t

// Define stat64 for [fl]stat64
typedef struct Mips32_stat64 {
  uint32_t st_dev;
  uint32_t st_pad1[3];
  Mips32_ino64_t st_ino;
  Mips32_mode_t st_mode;
  Mips32_nlink_t st_nlink;
  Mips32_uid_t st_uid;
  Mips32_gid_t st_gid;
  uint32_t st_rdev;
  uint32_t st_pad2[3];
  Mips32_off64_t st_size;
  Mips32_time_t st_atim;
  uint32_t reserved0;
  Mips32_time_t st_mtim;
  uint32_t reserved1;
  Mips32_time_t st_ctim;
  uint32_t reserved2;
  Mips32_blksize_t st_blksize;
  uint32_t st_pad3;
  Mips32_blkcnt64_t st_blocks;
  uint32_t st_pad4[14];
} Mips32_stat64;

// Define stat for [fl]stat
typedef struct Mips32_stat {
  uint32_t  st_dev;
  uint32_t st_pad1[3];
  Mips32_ino_t     st_ino;
  Mips32_mode_t    st_mode;
  Mips32_nlink_t   st_nlink;
  Mips32_uid_t     st_uid;
  Mips32_gid_t     st_gid;
  uint32_t  st_rdev;
  uint32_t st_pad2[2];
  Mips32_off_t     st_size;
  uint32_t  st_pad3;
  Mips32_time_t    st_atim;
  int32_t   reserved0;
  Mips32_time_t    st_mtim;
  int32_t   reserved1;
  Mips32_time_t    st_ctim;
  int32_t   reserved2;
  Mips32_blksize_t st_blksize;
  Mips32_blkcnt_t  st_blocks;
  uint32_t  st_pad5[14];
} Mips32_stat;
// Convert endianness of iovec for readv/writev
void cvtEndianBig(Mips32_iovec &val){
  cvtEndianBig(val.iov_base);
  cvtEndianBig(val.iov_len );
}
// Convert endianness of rlimit for getrlimit/setrlimit
void cvtEndianBig(Mips32_rlimit &val){
  cvtEndianBig(val.rlim_cur);
  cvtEndianBig(val.rlim_max);
}
// Convert endianness of timeval for rusage
void cvtEndianBig(Mips32_timeval &val){
  cvtEndianBig(val.tv_sec );
  cvtEndianBig(val.tv_usec);
}
// Convert endianness of rusage for getrusage/setrusage
void cvtEndianBig(Mips32_rusage &val){
  cvtEndianBig(val.ru_utime);
  cvtEndianBig(val.ru_stime);
  cvtEndianBig(val.ru_maxrss);
  cvtEndianBig(val.ru_ixrss);
  cvtEndianBig(val.ru_idrss);
  cvtEndianBig(val.ru_isrss);
  cvtEndianBig(val.ru_minflt);
  cvtEndianBig(val.ru_majflt);
  cvtEndianBig(val.ru_nswap);
  cvtEndianBig(val.ru_inblock);
  cvtEndianBig(val.ru_oublock);
  cvtEndianBig(val.ru_msgsnd);
  cvtEndianBig(val.ru_msgrcv);
  cvtEndianBig(val.ru_nsignals);
  cvtEndianBig(val.ru_nvcsw);
  cvtEndianBig(val.ru_nivcsw);
}
// Convert endianness of tms structure for times
void cvtEndianBig(Mips32_tms &val){
  cvtEndianBig(val.tms_utime);
  cvtEndianBig(val.tms_stime);
  cvtEndianBig(val.tms_cutime);
  cvtEndianBig(val.tms_cstime);
}
// Convert endianness of utsname for uname
void cvtEndianBig(Mips32_utsname &val){
  { for(int i=0;i<65;i++) cvtEndianBig(val.sysname[i]); }
  { for(int i=0;i<65;i++) cvtEndianBig(val.nodename[i]); }
  { for(int i=0;i<65;i++) cvtEndianBig(val.release[i]); }
  { for(int i=0;i<65;i++) cvtEndianBig(val.version[i]); }
  { for(int i=0;i<65;i++) cvtEndianBig(val.machine[i]); }
  { for(int i=0;i<65;i++) cvtEndianBig(val.domainname[i]); }
}
// Convert endianness of stat64 for [fl]stat64
void cvtEndianBig(Mips32_stat64 &val){
  cvtEndianBig(val.st_dev);
  cvtEndianBig(val.st_ino);
  cvtEndianBig(val.st_mode);
  cvtEndianBig(val.st_nlink);
  cvtEndianBig(val.st_uid);
  cvtEndianBig(val.st_gid);
  cvtEndianBig(val.st_rdev);
  cvtEndianBig(val.st_size);
  cvtEndianBig(val.st_atim);
  cvtEndianBig(val.st_mtim);
  cvtEndianBig(val.st_ctim);
  cvtEndianBig(val.st_blksize);
  cvtEndianBig(val.st_blocks);
}
// Convert endianness of stat for [fl]stat
void cvtEndianBig(Mips32_stat &val){
  cvtEndianBig(val.st_dev);
  cvtEndianBig(val.st_ino);
  cvtEndianBig(val.st_mode);
  cvtEndianBig(val.st_nlink);
  cvtEndianBig(val.st_uid);
  cvtEndianBig(val.st_gid);
  cvtEndianBig(val.st_rdev);
  cvtEndianBig(val.st_size);
  cvtEndianBig(val.st_atim);
  cvtEndianBig(val.st_mtim);
  cvtEndianBig(val.st_ctim);
  cvtEndianBig(val.st_blksize);
  cvtEndianBig(val.st_blocks);
}
