#if !(defined MIPS_REGS_H)
#define MIPS_REGS_H

#include "Regs.h"

namespace Mips {
  
  enum MipsRegName{
    // General-purpose registers
    GprNameLb = RegTypeGpr,
    RegZero   = GprNameLb,
    RegAT,
    RegV0, // 2
    RegV1,
    RegA0, // 4
    RegA1,
    RegA2,
    RegA3,
    RegA4,
    RegA5,
    RegA6,
    RegA7,
    RegT4, // 12
    RegT5,
    RegT6,
    RegT7,
    RegS0, // 16
    RegS1,
    RegS2,
    RegS3,
    RegS4,
    RegS5,
    RegS6,
    RegS7,
    RegT8,
    RegT9, // 25
    RegKT0,
    RegKT1,
    RegGP,
    RegSP, // 29
    RegS8, // 30
    RegFP     = RegS8,
    RegRA, // 31
    RegTmp, // Temporary GPR for implementing micro-ops
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
    RegFpTmp, // Temporary FPR for implementing micro-ops
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
    RegCond, // Condition register for implementing micro-ops
    SpcNameUb,
  };

  template<typename T>
  inline T getReg(const ThreadContext *context, RegName name){
    I(!isFprName(name));
    I(static_cast<MipsRegName>(name)!=RegJunk);
    //    I(static_cast<MipsRegName>(name)!=RegZero);
   const T *ptr=static_cast<const T *>(context->getReg(name));
    switch(sizeof(T)){
    case 8:
      I(context->getMode()==Mips64);
      return *ptr;
      break;
    case 4: {
      I((context->getMode()==Mips64)||!(*(ptr+(__BYTE_ORDER!=__BIG_ENDIAN))));
      return *(ptr+(__BYTE_ORDER==__BIG_ENDIAN));
    } break;
    }
  }
  
  template<typename T>
  inline void setReg(ThreadContext *context, RegName name, T val){
    I(!isFprName(name));
    I(static_cast<MipsRegName>(name)!=RegJunk);
    I(static_cast<MipsRegName>(name)!=RegZero);
    T *ptr=static_cast<T *>(context->getReg(name));
    switch(sizeof(T)){
    case 8:
      I(context->getMode()==Mips64);
      *ptr=val;
      break;
    case 4: {
      *(ptr+(__BYTE_ORDER==__BIG_ENDIAN))=val;
      I(getReg<T>(context,name)==val);
      I(static_cast<uint32_t>(*static_cast<uint64_t *>(context->getReg(name)))==static_cast<uint32_t>(val));
    } break;
    }
  }
  
  template<typename T, CpuMode M>
  inline T getRegFp(const ThreadContext *context, RegName name){
    I(isFprName(name));
    switch(sizeof(T)){
    case 8: {
      const T *ptr=static_cast<const T *>(context->getReg(name));
      return *ptr;
    } break;
    case 4:
      I(__FLOAT_WORD_ORDER==__BYTE_ORDER);
      switch(M){
      case Mips32: {
        bool isOdd=static_cast<bool>(name&1);
	const T *ptr=static_cast<const T *>(context->getReg(static_cast<RegName>(name^isOdd)));
        return *(ptr+(isOdd^(__BYTE_ORDER==__BIG_ENDIAN)));
      } break;
      case Mips64: {
	const T *ptr=static_cast<const T *>(context->getReg(name));
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
      T *ptr=static_cast<T *>(context->getReg(name));
      *ptr=val;
    } break;
    case 4:
      I(__FLOAT_WORD_ORDER==__BYTE_ORDER);
      switch(M){
      case Mips32: {
        bool isOdd=static_cast<bool>(name&1);
	T *ptr=static_cast<T *>(context->getReg(static_cast<RegName>(name^isOdd)));
        *(ptr+(isOdd^(__BYTE_ORDER==__BIG_ENDIAN)))=val;
      } break;
      case Mips64: {
	T *ptr=static_cast<T *>(context->getReg(name));
        *(ptr+(__BYTE_ORDER==__BIG_ENDIAN))=val;
      } break;
      }
    }
  }
  
}

#endif // !(defined MIPS_REGS__H)
