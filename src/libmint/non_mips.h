#ifndef NON_MIPS_H
#define NON_MIPS_H 1

#ifndef IRIX64
#define PR_SADDR 0x40

#define MP_NPROCS	1
#define MP_NAPROCS	2
#define MP_KERNADDR	8
#define MP_SASZ		9
#define MP_SAGET	10
#define MP_SCHED	13
#define MP_PGSIZE	14
#define MP_SAGET1	15
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

#define SGI_SYSID	1
#define SGI_RDUBLK	2
#define SGI_TUNE	3
#define SGI_IDBG	4
#define SGI_INVENT	5
#define SGI_RDNAME	6
#define SGI_SETNVRAM	8
#define SGI_GETNVRAM	9
#define SGI_QUERY_CYCLECNTR	13
#define SGI_PROCSZ	14
#define SGI_SIGACTION	15
#define SGI_SIGPENDING	16
#define SGI_SIGPROCMASK	17
#define SGI_SIGSUSPEND	18
#define SGI_SETTIMETRIM	53
#define SGI_GETTIMETRIM	54
#define SGI_SPROFIL	55
#define SGI_RUSAGE	56
#define SGI_BDFLUSHCNT	61
#define SGI_SSYNC	62
#define SGI_USE_FP_BCOPY 129

#define SGI_INV_SIZEOF  1
#define SGI_INV_READ    2

typedef unsigned short NativeFPUControlType; 
#ifdef __i386__
#include <fpu_control.h>
/* Sets __i386__ FPU rounding flags according to the rouding bits in
   MIPS. MIPS uses the first two bits (bit1,bit0). I387 uses 11 and 10
   bits.
   
   MIPS I387         Meaning
   00     00 (0x000)  Nearest
   01     11 (0xC00)  Zero
   10     10 (0x800)  Plus Inf
   11     01 (0x400)  Minus Inf
*/
static inline NativeFPUControlType changeFPUControl(unsigned long mipsFlag) {
  NativeFPUControlType oldValue, newValue;
  _FPU_GETCW(oldValue);
  /* Change the lowest 2 bits to correspond to the i387 rounding */ 
  mipsFlag^=2*mipsFlag;
  /* Get only the lowest 2 bits of the MIPS rounding control */ 
  mipsFlag&=3;
  /* Now move the bits to where the i387 wants them */
  mipsFlag*=0x400;
  /* Change the rounding control bits */
  newValue=(oldValue&0xF3FF)|mipsFlag;
  _FPU_SETCW(oldValue);
  return oldValue;
}

static inline void restoreFPUControl(NativeFPUControlType i387Flag) {
  _FPU_SETCW(i387Flag);
}
#elif defined(DARWIN)
NativeFPUControlType changeFPUControl(unsigned long mipsFlag);
void restoreFPUControl(NativeFPUControlType i387Flag);
#else
static inline NativeFPUControlType changeFPUControl(unsigned long mipsFlag) {
  /* FIXME: Implement get/put */
  return 0;
}

static inline void restoreFPUControl(NativeFPUControlType i387Flag) {
  /* FIXME: Implement get/put */
}
#endif /* __i386__ */

#endif /* IRIX64 */

unsigned long mips_div(long a, long b, long *lo, long *hi);
unsigned long mips_divu(unsigned long a, unsigned long b, long *lo, long *hi);
unsigned long mips_lwlBE(long value, char *addr);
unsigned long mips_lwlLE(long value, char *addr);
long mips_lwrBE(long value, char *addr);
long mips_lwrLE(long value, char *addr);
long mips_mult(long a, long b, long *lo, long *hi);
long mips_multu(unsigned long a, unsigned long b, long *lo, long *hi);
void mips_swlBE(long value, char *addr);
void mips_swlLE(long value, char *addr);
void mips_swrBE(long value, char *addr);
void mips_swrLE(long value, char *addr);
int mips_cvt_w_s(float pf, long fsr);
int mips_cvt_w_d(double dval, long fsr);
void setFPUControl(unsigned long mipsFlag);


#endif /* NON_MIPS_H */
