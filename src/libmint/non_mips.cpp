
/* C functions to emulate MIPS instructions on machines without MIPS
 * processors.
 */


#include <mendian.h>
#include <stdio.h>
#include "icode.h"
#include "globals.h"

#if defined(__i386__)
void setFPUControl(unsigned long mipsFlag)
{
  changeFPUControl(mipsFlag & 0x3);
}

#elif defined(DARWIN)
#include <fenv.h>

static unsigned short FloatValue [] ={FE_TONEAREST,  /* 00 nearest   */
				      FE_TOWARDZERO, /* 01 zero      */
				      FE_UPWARD,     /* 10 plus inf  */
				      FE_DOWNWARD    /* 11 minus inf */
};
void setFPUControl(unsigned long mipsFlag)
{
  fesetround(FloatValue[mipsFlag & 0x3]);
}

NativeFPUControlType changeFPUControl(unsigned long mipsFlag)
{
  NativeFPUControlType oldValue;

  oldValue = fegetround();

  setFPUControl(mipsFlag);

  return oldValue;
}

void restoreFPUControl(NativeFPUControlType ppcFlag)
{
  fesetround(ppcFlag);
}

#elif defined(AIX)
#include <float.h>

#if 0
static unsigned short FloatValue [] ={FP_RND_RN, /* 1 */
				      FP_RND_RZ, /* 0 */
				      FP_RND_RP, /* 2 */
				      FP_RND_RM  /* 3 */
};
#endif

void setFPUControl(unsigned long mipsFlag)
{
#if 0
  fprnd_t val= FloatValue[mipsFlag & 0x3];
  
  fp_swap_rnd(val);
#endif
}
#elif __i386__
void setFPUControl(unsigned long mipsFlag)
{
  fpsetround(mipsFlag & 0x3);
}
#else
void setFPUControl(unsigned long mipsFlag)
{
#warning "setFPUControl not implemented for this architecture"
  // FIXME: not implemented fpsetround(mipsFlag & 0x3);
}
#endif /* __i386__ */



unsigned long mips_div(long a, long b, long *lo, long *hi)
{
    *lo = a / b;
    *hi = a % b;
    return 0;
}

unsigned long mips_divu(unsigned long a, unsigned long b, long *lo, long *hi)
{
    *lo = a / b;
    *hi = a % b;
    return 0;
}

unsigned long mips_lwlBE(long value, char *addr)
{
    unsigned long rval;
    char *pval;
	
    rval = value;
    pval = (char *) &rval;
    *pval++ = *addr++;
    while ((long) addr & 0x3)
        *pval++ = *addr++;
    return rval;
}

unsigned long mips_lwlLE(long value, char *addr)
{
    unsigned long rval;
    unsigned long offset;
    char *pval;

    offset = ((unsigned long)addr) & 0x3;
    rval = value;
    pval = (char *) &rval+3;
	
    do{
      *pval-- = *addr++;
    }while(offset++<3);
	
    return rval;
}

long mips_lwrBE(long value, char *addr)
{
    long rval;
    char *pval;

    rval = value;
    pval = (char *) &rval + 3;
    *pval-- = *addr--;
    while (((long) addr & 0x3) != 0x3)
        *pval-- = *addr--;
    return rval;
}

long mips_lwrLE(long value, char *addr)
{
    long rval;
	unsigned long offset;
    char *pval;

    offset = ((unsigned long)addr) & 0x3;
    addr = (char *)(((unsigned long )addr) & 0xFFFFFFFC);
	
    rval = value;
    pval = (char *) &rval + offset;
	
	do{
		*pval-- = *addr++;
	}while(offset-->0);
	
    return rval;
}

/* This routine computes the signed 64-bit product of the signed 32-bit
 * parameters "a" and "b" and stores the low 32 bits of the result in the
 * location pointed to by "lo" and stores the high 32 bits of the result
 * in the location pointed to by "hi".
 * 
 * There must be a more efficient way to do signed 32-bit multiplication.
 */
long mips_mult(long a, long b, long *lo, long *hi)
{
    unsigned long ahi, alo, bhi, blo;
    unsigned long part1, part2, part3, part4;
    unsigned long p2lo, p2hi, p3lo, p3hi;
    unsigned long ms1, ms2, ms3, sum, carry;
    unsigned long extra;

    ahi = (unsigned long) a >> 16;
    alo = a & 0xffff;
    bhi = (unsigned long) b >> 16;
    blo = b & 0xffff;

    /* compute the partial products */
    part4 = ahi * bhi;
    part3 = ahi * blo;
    part2 = alo * bhi;
    part1 = alo * blo;

    p2lo = part2 << 16;
    p2hi = part2 >> 16;
    p3lo = part3 << 16;
    p3hi = part3 >> 16;

    /* compute the carry bit by checking the high-order bits of the addends */
    carry = 0;
    sum = part1 + p2lo;
    ms1 = part1 & 0x80000000;
    ms2 = p2lo & 0x80000000;
    ms3 = sum & 0x80000000;
    if ((ms1 & ms2) || ((ms1 | ms2) & ~ms3))
        carry++;
    ms1 = p3lo & 0x80000000;
    sum += p3lo;
    ms2 = sum & 0x80000000;
    if ((ms1 & ms3) || ((ms1 | ms3) & ~ms2))
        carry++;

    *lo = sum;

    extra = 0;
    if (a < 0)
        extra = b;
    if (b < 0)
        extra += a;
    
    *hi = part4 + p2hi + p3hi + carry - extra;
    return 0;
}

/* This routine computes the unsigned 64-bit product of the unsigned 32-bit
 * parameters "a" and "b" and stores the low 32 bits of the result in the
 * location pointed to by "lo" and stores the high 32 bits of the result
 * in the location pointed to by "hi".
 */
long mips_multu(unsigned long a, unsigned long b, long *lo, long *hi)
{
    unsigned long ahi, alo, bhi, blo;
    unsigned long part1, part2, part3, part4;
    unsigned long p2lo, p2hi, p3lo, p3hi;
    unsigned long ms1, ms2, ms3, sum, carry;

    ahi = a >> 16;
    alo = a & 0xffff;
    bhi = b >> 16;
    blo = b & 0xffff;

    /* compute the partial products */
    part4 = ahi * bhi;
    part3 = ahi * blo;
    part2 = alo * bhi;
    part1 = alo * blo;

    p2lo = part2 << 16;
    p2hi = part2 >> 16;
    p3lo = part3 << 16;
    p3hi = part3 >> 16;

    /* compute the carry bit by checking the high-order bits of the addends */
    carry = 0;
    sum = part1 + p2lo;
    ms1 = part1 & 0x80000000;
    ms2 = p2lo & 0x80000000;
    ms3 = sum & 0x80000000;
    if ((ms1 & ms2) || ((ms1 | ms2) & ~ms3))
        carry++;
    ms1 = p3lo & 0x80000000;
    sum += p3lo;
    ms2 = sum & 0x80000000;
    if ((ms1 & ms3) || ((ms1 | ms3) & ~ms2))
        carry++;

    *lo = sum;
    *hi = part4 + p2hi + p3hi + carry;
    return 0;
}

void mips_swlBE(long value, char *addr)
{
    char *pval;

    pval = (char *) &value;
    *addr++ = *pval++;
    while ((long) addr & 0x3)
        *addr++ = *pval++;
}

void mips_swlLE(long value, char *addr)
{
    char *pval;
	unsigned long offset;

	offset = ((unsigned long)addr) & 0x3;
    pval = (char *) &value+3 ;
	
	do{
		*addr++ = *pval--;
	}while(offset++<3);
}


void mips_swrBE(long value, char *addr)
{
    char *pval;

    pval = (char *) &value + 3;
    *addr-- = *pval--;
    while (((long) addr & 0x3) != 0x3){
        *addr-- = *pval--;
	}
}

void mips_swrLE(long value, char *addr)
{
    char *pval;
	unsigned long offset;

	offset = ((unsigned long)addr) & 0x3;
	offset = 3 - offset;
/*	addr = (char *)(((unsigned long )addr) & 0xFFFFFFFC); */
	
    pval = (char *) &value;
	
	do{
		*addr-- = *pval++;
	}while(offset++<3);
}


/* The conversion from float to int on the SPARC ignores the rounding mode
 * bits, but the MIPS follows them faithfully. For example, when round-to-
 * nearest or round-to-plus-infinity is used, 1234.6 should be converted
 * to 1235, but the SPARC always truncates. So we have to do the conversion
 * explicitly. (Ugh!)
 */
int mips_cvt_w_s(float pf, long fsr)
{
    int ival, rmode;

    ival = (int) pf;
    rmode = fsr & 3;
    if (ival < 0) {
        if (rmode == 0) {
            ival = (int) (pf - 0.5);
            /* -1.5 rounds down to -2, but -2.5 rounds up to -2 */
            if ((ival == pf - 0.5) && (ival & 1))
                ival++;
        } else {
            if (rmode == 3 && ival != pf)
                ival--;
        }
    } else {
        if (rmode == 0) {
            ival = (int) (pf + 0.5);
            /* 1.5 rounds up to 2, but 2.5 rounds down to 2 */
            if ((ival == pf + 0.5) && (ival & 1))
                ival--;
        } else {
            if (rmode == 2 && ival != pf)
                ival++;
        }
    }
    return ival;
}

int mips_cvt_w_d(double dval, long fsr)
{
    int ival, rmode;

    rmode = fsr & 3;
    if (dval < 0) {
        if (rmode == 0) {
            ival = (int) (dval - 0.5);
            /* -1.5 rounds down to -2, but -2.5 rounds up to -2 */
            if ((ival == dval - 0.5) && (ival & 1))
                ival++;
        } else {
            ival = (int) dval;
            if (rmode == 3 && ival != dval)
                ival--;
        }
    } else {
        if (rmode == 0) {
            ival = (int) (dval + 0.5);
            /* 1.5 rounds up to 2, but 2.5 rounds down to 2 */
            if ((ival == dval + 0.5) && (ival & 1))
                ival--;
        } else {
            ival = (int) dval;
            if (rmode == 2 && ival != dval)
                ival++;
        }
    }
    return ival;
}

