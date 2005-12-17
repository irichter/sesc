#ifndef NON_MIPS_H
#define NON_MIPS_H 1

#ifndef IRIX64
#define PR_SADDR 0x40

#define MP_NPROCS       1
#define MP_NAPROCS      2
#define MP_KERNADDR     8
#define MP_SASZ         9
#define MP_SAGET        10
#define MP_SCHED        13
#define MP_PGSIZE       14
#define MP_SAGET1       15
#define MP_EMPOWER      16
#define MP_RESTRICT     17
#define MP_CLOCK        18
#define MP_MUSTRUN      19
#define MP_RUNANYWHERE  20
#define MP_STAT         21
#define MP_ISOLATE      22
#define MP_UNISOLATE    23
#define MP_PREEMPTIVE   24
#define MP_NONPREEMPTIVE 25

#define SGI_SYSID       1
#define SGI_RDUBLK      2
#define SGI_TUNE        3
#define SGI_IDBG        4
#define SGI_INVENT      5
#define SGI_RDNAME      6
#define SGI_SETNVRAM    8
#define SGI_GETNVRAM    9
#define SGI_QUERY_CYCLECNTR     13
#define SGI_PROCSZ      14
#define SGI_SIGACTION   15
#define SGI_SIGPENDING  16
#define SGI_SIGPROCMASK 17
#define SGI_SIGSUSPEND  18
#define SGI_SETTIMETRIM 53
#define SGI_GETTIMETRIM 54
#define SGI_SPROFIL     55
#define SGI_RUSAGE      56
#define SGI_BDFLUSHCNT  61
#define SGI_SSYNC       62
#define SGI_USE_FP_BCOPY 129

#define SGI_INV_SIZEOF  1
#define SGI_INV_READ    2

// FIXME: to make it portable use #include <ieeefp.h> instead of fpu_control.h

typedef unsigned int NativeFPUControlType; 
NativeFPUControlType changeFPUControl(unsigned int mipsFlag);
void restoreFPUControl(NativeFPUControlType nativeFlag);
void setFPUControl(unsigned int mipsFlag);

#endif /* IRIX64 */

unsigned int mips_div(int a, int b, int *lo, int *hi);
unsigned int mips_divu(unsigned int a, unsigned int b, int *lo, int *hi);
unsigned int mips_lwlBE(int value, char *addr);
unsigned int mips_lwlLE(int value, char *addr);
int mips_lwrBE(int value, char *addr);
int mips_lwrLE(int value, char *addr);
int mips_mult(int a, int b, int *lo, int *hi);
int mips_multu(unsigned int a, unsigned int b, int *lo, int *hi);
void mips_swlBE(int value, char *addr);
void mips_swlLE(int value, char *addr);
void mips_swrBE(int value, char *addr);
void mips_swrLE(int value, char *addr);
int mips_cvt_w_s(float pf, int fsr);
int mips_cvt_w_d(double dval, int fsr);


#endif /* NON_MIPS_H */
