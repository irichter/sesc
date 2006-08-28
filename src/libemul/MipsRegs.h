#if !(defined MIPS_REGS_H)
#define MIPS_REGS_H

#include "Regs.h"

namespace Mips {

enum MipsRegName{
  // General-purpose registers
  GprNameLb = RegTypeGpr,
  RegZero   = GprNameLb,
  RegAT,
  RegV0,
  RegV1,
  RegA0,
  RegA1,
  RegA2,
  RegA3,
  RegA4,
  RegA5,
  RegA6,
  RegA7,
  RegT4,
  RegT5,
  RegT6,
  RegT7,
  RegS0,
  RegS1,
  RegS2,
  RegS3,
  RegS4,
  RegS5,
  RegS6,
  RegS7,
  RegT8,
  RegT9,
  RegKT0,
  RegKT1,
  RegGP,
  RegSP,
  RegS8,
  RegFP     = RegS8,
  RegRA,
  GprNameUb,
  // Floating-point registers
  FprNameLb = RegTypeFpr,
  RegF0     = FprNameLb,
  RegF1,
  RegF2,
  RegF3,
  RegF4,
  RegF5,
  RegF6,
  RegF7,
  RegF8,
  RegF9,
  RegF10,
  RegF11,
  RegF12,
  RegF13,
  RegF14,
  RegF15,
  RegF16,
  RegF17,
  RegF18,
  RegF19,
  RegF20,
  RegF21,
  RegF22,
  RegF23,
  RegF24,
  RegF25,
  RegF26,
  RegF27,
  RegF28,
  RegF29,
  RegF30,
  RegF31,
  FprNameUb,
  // Control registers
  FcrNameLb = RegTypeCtl,
  RegFC0    = FcrNameLb,
  RegFC31   = FcrNameLb+31,
  RegFCSR   = RegFC31, // FPU (COP1) control register 31 is the "FPU Control and Status Register" (FCSR)
  FcrNameUb,
  // Special registers
  SpcNameLb = RegTypeSpc,
  RegHi     = SpcNameLb,
  RegLo,
  // Some instructions modify both Reghi and RegLo
  // We use RegLo as a target to create a dependence
  RegHL=RegLo,
  RegJunk, // Junk register, stores to RegZero go here
  SpcNameUb,
};

/* struct MipsRegVal{ */
/*   union{ */
/*     RegVal      whole; */
/*     VAddr       a; // Virtual address */
/*     int64_t     l; // 64-bit integer */
/*     struct{ */
/* #if (__BYTE_ORDER == __BIG_ENDIAN) */
/*       int32_t   whi; // Upper 32 bits of a 64 bit register */
/*       int32_t   w;   // 32-bit integer */
/* #else */
/*       int32_t   w;   // 32-bit integer */
/*       int32_t   whi; // Upper 32 bits of a 64 bit register */
/* #endif */
/*     }; */
/*     float64_t   d; // 64-bit floating point */
/*     struct{ */
/* #if (__FLOAT_WORD_ORDER == __BIG_ENDIAN) */
/*       float32_t   shi; // Upper 32 bits of a 64 bit register */
/*       float32_t   s;   // 32-bit floating point */
/* #else */
/*       float32_t   s;   // 32-bit floating point */
/*       float32_t   shi; // Upper 32 bits of a 64 bit register */
/* #endif */
/*     }; */
/*   }; */
/* }; */

/* inline const MipsRegVal &getMipsReg(const ThreadContext *context, MipsRegName name){ */
/*   const MipsRegVal &reg=(MipsRegVal &)(context->getReg((RegName)name)); */
/*   return reg; */
/* } */

/* inline MipsRegVal &getMipsReg(ThreadContext *context, MipsRegName name){ */
/*   MipsRegVal &reg=(MipsRegVal &)(context->getReg((RegName)name)); */
/*   return reg; */
/* } */

/* inline bool isMipsGprName(RegName reg){ */
/*   I((!isGprName(reg))||((reg&RegNumMask)<32)); */
/*   return isGprName(reg); */
/* } */
/* inline bool isMipsFprName(RegName reg){ */
/*   I((!isFprName(reg))||((reg&RegNumMask)<32)); */
/*   return isFprName(reg); */
/* } */

/* inline int64_t &getMipsRegL(ThreadContext *context, RegName name){ */
/*   MipsRegVal &reg=(MipsRegVal &)(context->getReg(name)); */
/*   return reg.l; */
/* } */
/* inline const int64_t &getMipsRegL(const ThreadContext *context, const RegName name){ */
/*   const MipsRegVal &reg=(const MipsRegVal &)(context->getReg(name)); */
/*   return reg.l; */
/* } */
/* inline int32_t &getMipsRegW(ThreadContext *context, RegName name){ */
/*   if((context->getMode()==Mips32)&&(getRegType(name)==RegTypeFpr)&&(name&1)){ */
/*     MipsRegVal &reg=(MipsRegVal &)(context->getReg((RegName)(name-1))); */
/*     return reg.whi; */
/*   } */
/*   MipsRegVal &reg=(MipsRegVal &)(context->getReg(name)); */
/*   return reg.w; */
/* } */
/* inline const int32_t &getMipsRegW(const ThreadContext *context, const RegName name){ */
/*   if((context->getMode()==Mips32)&&(getRegType(name)==RegTypeFpr)&&(name&1)){ */
/*     const MipsRegVal &reg=(const MipsRegVal &)(context->getReg((RegName)(name-1))); */
/*     return reg.whi; */
/*   }else{ */
/*     const MipsRegVal &reg=(const MipsRegVal &)(context->getReg(name)); */
/*     return reg.w; */
/*   } */
/* } */
/* inline float64_t &getMipsRegD(ThreadContext *context, RegName name){ */
/*   MipsRegVal &reg=(MipsRegVal &)(context->getReg(name)); */
/*   return reg.d; */
/* } */
/* inline const float64_t &getMipsRegD(const ThreadContext *context, const RegName name){ */
/*   const MipsRegVal &reg=(const MipsRegVal &)(context->getReg(name)); */
/*   return reg.d; */
/* } */
/* inline float32_t &getMipsRegS(ThreadContext *context, RegName name){ */
/*   if((context->getMode()==Mips32)&&(getRegType(name)==RegTypeFpr)&&(name&1)){ */
/*     MipsRegVal &reg=(MipsRegVal &)(context->getReg((RegName)(name-1))); */
/*     return reg.shi; */
/*   } */
/*   MipsRegVal &reg=(MipsRegVal &)(context->getReg(name)); */
/*   return reg.s; */
/* } */
/* inline const float32_t &getMipsRegS(const ThreadContext *context, const RegName name){ */
/*   if((context->getMode()==Mips32)&&(getRegType(name)==RegTypeFpr)&&(name&1)){ */
/*     const MipsRegVal &reg=(const MipsRegVal &)(context->getReg((RegName)(name-1))); */
/*     return reg.shi; */
/*   }else{ */
/*     const MipsRegVal &reg=(const MipsRegVal &)(context->getReg(name)); */
/*     return reg.s; */
/*   } */
/* } */

template<typename T>
inline T getReg(const ThreadContext *context, RegName name){
  I(!isFprName(name));
  switch(sizeof(T)){
    case 8:
      I(context->getMode()==Mips64);
      return context->getReg(name); 
      break;
    case 4: {
      const T *ptr=(T *)(&(context->getReg(name)));
      I((context->getMode()==Mips64)||!(*(ptr+(__BYTE_ORDER!=__BIG_ENDIAN))));
      return *(ptr+(__BYTE_ORDER==__BIG_ENDIAN));
    } break;
  }
}

template<typename T>
inline void setReg(ThreadContext *context, RegName name, T val){
  I(!isFprName(name));
  switch(sizeof(T)){
    case 8:
      I(context->getMode()==Mips64);
      context->getReg(name)=val; 
      break;
    case 4: {
      //      context->getReg(name)=static_cast<int64_t>(static_cast<int32_t>(val));
      T *ptr=(T *)(&(context->getReg(name)));
      *(ptr+(__BYTE_ORDER==__BIG_ENDIAN))=val;
      I(getReg<T>(context,name)==val);
      I(((T)(context->getReg(name)))==val);
    } break;
  }
}

template<typename T, CpuMode M>
inline T getRegFp(const ThreadContext *context, RegName name){
  I(isFprName(name));
  switch(sizeof(T)){
    case 8: {
      const T *ptr=(T *)(&(context->getReg(name)));
      return *ptr;
    } break;
    case 4:
      switch(M){
      case Mips32: {
        bool isOdd=static_cast<bool>(name&1);
        const T *ptr=(T *)(&(context->getReg(static_cast<RegName>(name^isOdd))));
        return *(ptr+(isOdd^(__BYTE_ORDER==__BIG_ENDIAN)));
      } break;
      case Mips64: {
        const T *ptr=(T *)(&(context->getReg(name)));
        return *(ptr+(__BYTE_ORDER==__BIG_ENDIAN));
      } break;
      }
  }
}
template<typename T, CpuMode M>
inline void setRegFp(ThreadContext *context, RegName name, T val){
  I(isFprName(name));
  switch(sizeof(T)){
    case 8: {
      T *ptr=(T *)(&(context->getReg(name)));
      *ptr=val;
    } break;
    case 4:
      switch(M){
      case Mips32: {
        bool isOdd=static_cast<bool>(name&1);
        T *ptr=(T *)(&(context->getReg(static_cast<RegName>(name^isOdd))));
        *(ptr+(isOdd^(__BYTE_ORDER==__BIG_ENDIAN)))=val;
      } break;
      case Mips64: {
        T *ptr=(T *)(&(context->getReg(name)));
        *(ptr+(__BYTE_ORDER==__BIG_ENDIAN))=val;
      } break;
      }
  }
}

}

#endif // !(defined MIPS_REGS__H)
