#if !(defined INST_DESC_H)
#define INST_DESC_H

#include <stdlib.h>
#include "common.h"
#include "nanassert.h"
#include "Addressing.h"
#include "Regs.h"
// To get definition of fail()
#include "EmulInit.h"

enum{
  OpInvalid = 0,
};
typedef uint16_t Opcode;

class ThreadContext;
class InstDesc;
typedef InstDesc *EmulFunc(InstDesc *inst, ThreadContext *context);

enum InstTypInfoEnum{
  // Main instruction opcode
  TypOpMask   = 0xF00, // Mask for the main instruction opcode
  TypNop      = 0x000,
  TypIntOp    = 0x100,
  TypFpOp     = 0x200,
  TypBrOp     = 0x300,
  TypMemOp    = 0x400,

  TypSubMask  = 0xFF0, // Mask for the subtype
  // Subtypes for integer opcodes
  IntOpALU    = 0x00 + TypIntOp, // ALU operation
  IntOpMul    = 0x10 + TypIntOp, // Multiply
  IntOpDiv    = 0x20 + TypIntOp, // Divide
  // Subtypes for FP opcodes
  FpOpALU     = 0x00 + TypFpOp,  // "Easy" FP op
  FpOpMul     = 0x10 + TypFpOp,  // Multiply-like FP op
  FpOpDiv     = 0x20 + TypFpOp,  // Divide-like FP op (also sqrt, recip, etc)
  // Subtypes for Branch opcodes
  BrOpJump    = 0x00 + TypBrOp,  // Unconditional branch
  BrOpCond    = 0x10 + TypBrOp,  // Conditional branch
  BrOpCall    = 0x20 + TypBrOp,  // Function call
  BrOpCCall   = 0x30 + TypBrOp,  // Conditional function call
  BrOpRet     = 0x40 + TypBrOp,  // Function return
  BrOpCRet    = 0x50 + TypBrOp,  // Conditional function return
  BrOpTrap    = 0x80 + TypBrOp,  // Trap/syscall
  BrOpCTrap   = 0x90 + TypBrOp,  // Conditional trap/syscall
  // Subtypes for memory opcodes
  TypMemLd    = 0x00 + TypMemOp, // Load
  TypMemSt    = 0x10 + TypMemOp, // Store
  TypSynLd    = 0x20 + TypMemOp, // Sync Load
  TypSynSt    = 0x30 + TypMemOp, // Sync Store
  MemSizeMask = 0xF, // Mask for load/store access size
  MemOpLd1    = TypMemLd+1,
  MemOpLd2    = TypMemLd+2,
  MemOpLd4    = TypMemLd+4,
  MemOpLd8    = TypMemLd+8,
  MemOpSt1    = TypMemSt+1,
  MemOpSt2    = TypMemSt+2,
  MemOpSt4    = TypMemSt+4,
  MemOpSt8    = TypMemSt+8,
  MemOpLl     = TypSynLd+4,
  MemOpSc     = TypSynSt+4
};
typedef InstTypInfoEnum InstTypInfo;

inline bool isSync(InstTypInfo typ){
  return ((typ&0xF20)==(0x20+TypMemOp));
}

class InstImm{
 public:
  union{
    VAddr     a;
    int32_t   s;
    uint32_t  u;
    InstDesc *i;
  };
  InstImm(void){}
  InstImm(int32_t   s) : s(s){}
  InstImm(uint32_t  u) : u(u){}
  InstImm(InstDesc *i) : i(i){}
  inline operator int32_t(){
    return s;
  }
  inline operator int64_t(){
    return int64_t(s);
  }
  inline operator uint32_t(){
    return u;
  }
  inline operator uint64_t(){
    return uint64_t(u);
  }
  inline operator float32_t(){
    fail("InstImm float32_t conversion\n");
    return float32_t(s);
  }
  inline operator float64_t(){
    fail("InstImm float64_t conversion\n");
    return float64_t(s);
  }
  inline operator bool(){
    return bool(u);
  }
};

class ThreadContext;

void decodeTrace(ThreadContext *context, VAddr iaddr);

class Instruction;

class InstDesc{
 public:
  EmulFunc    *emul;
  Instruction *sescInst;
  InstImm      imm;
  RegName      regDst;
  RegName      regSrc1;
  RegName      regSrc2;
  uint8_t      iupdate;
  uint8_t      aupdate;
#if (defined DEBUG)
  InstTypInfo  typ;
  VAddr        addr;
  Opcode       op;
#endif
 public:
  InstDesc(void) : sescInst(0){
#if (defined DEBUG)
//    emul=0;
//    regDst=RegNone;
//    regSrc1=RegNone;
//    regSrc2=RegNone;
//    imm.i=0;
//    iupdate=0;
//    aupdate=0;
//    typ=TypNop;
//    addr=0;
//    op=OpInvalid;
#endif
  }
  ~InstDesc(void);
  InstDesc *operator()(ThreadContext *context){
    return emul(this,context);
  }
  Instruction *getSescInst(void) const{
    I(sescInst);
    return sescInst;
  }
};

#define InvalidInstDesc ((InstDesc *)0)

#endif // !(defined INST_DESC_H)
