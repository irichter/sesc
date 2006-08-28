#if !(defined INST_DESC_H)
#define INST_DESC_H

#include <stdlib.h>
#include "common.h"
#include "Addressing.h"
#include "Regs.h"

enum{
  OpInvalid = 0,
};
typedef uint16_t Opcode;

class ThreadContext;
class InstDesc;
typedef void EmulFunc(InstDesc *inst, ThreadContext *context);

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
  BrOpJump    = 0x00 + TypBrOp,  // Ordinary unconditional branch
  BrOpCond    = 0x10 + TypBrOp,  // Ordinary conditional branch
  BrOpCall    = 0x20 + TypBrOp,  // Function call
  BrOpRet     = 0x30 + TypBrOp,  // Function return
  BrOpTrap    = 0x40 + TypBrOp,  // Trap/Syscall
  // Subtypes for memory opcodes
  TypMemLd    = 0x00 + TypMemOp, // Load
  TypMemSt    = 0x10 + TypMemOp, // Store
  MemSizeMask = 0xF, // Mask for load/store access size
  MemOpLd1    = TypMemLd+1,
  MemOpLd2    = TypMemLd+2,
  MemOpLd4    = TypMemLd+4,
  MemOpLd8    = TypMemLd+8,
  MemOpSt1    = TypMemSt+1,
  MemOpSt2    = TypMemSt+2,
  MemOpSt4    = TypMemSt+4,
  MemOpSt8    = TypMemSt+8,
};
typedef InstTypInfoEnum InstTypInfo;

enum InstCtlInfoEnum{
  CtlInvalid = 0x0000,   // No instruction should have CtlInfo of zero (should at least nave length)

  CtlISize   = 0x003F,   // Mask for instruction size
  CtlISize4  = 0x0004,   // Instruction is 4 bytes long

  CtlIMask   = 0x0FC0,   // Mask for bits that depend on what the instruction is doing, not its size or location
  CtlExcept  = 0x0040,   // Instruction may generate exceptions
  CtlBr      = 0x0080,   // Instruction may jump/branch
  CtlFix     = 0x0100,   // Branch/jump target is fixed (not register-based)
  CtlCall    = 0x0200,   // Branch/jump is a function call
  CtlAnull   = 0x0400,   // Likely (anull) conditional branch/jump

  CtlLMask   = 0xF000,  // Mask for all the bits that are a property of the location where the instruction is, not if instruction itself
  CtlInDSlot = 0x1000,  // This instruction is in a delay slot of another instruction
};
typedef InstCtlInfoEnum InstCtlInfo;

void instDecode(InstDesc *inst, ThreadContext *context);

class Instruction;

class InstDesc{
 public:
  EmulFunc *emulFunc;
  RegName  regDst;
  RegName  regSrc1;
  RegName  regSrc2;
  RegName  regSrc3;
  union{
    int32_t   s;
    uint32_t  u;
    InstDesc *i;
  } imm1;
  union{
    uint32_t  u;
  } imm2;
  VAddr    addr;
  InstCtlInfo ctlInfo;
#if (defined DEBUG)
  Opcode   op;
  InstTypInfo typInfo;
#endif
  InstDesc *nextInst;
  Instruction *inst;
 public:
  InstDesc(void) : emulFunc(instDecode), ctlInfo(CtlInvalid), nextInst(0){
#if (defined DEBUG)
    op=OpInvalid;
    typInfo=TypNop;
#endif
    //    I(!(ctlInfo&CtlInDSlot));
    regDst=RegNone;
    regSrc1=regSrc2=regSrc3=RegNone;
    imm1.i=0;
    imm2.u=0;
    inst=0;
  }
  size_t getInstSize(void){
    return ctlInfo&CtlISize;
  }
  void beInDSlot(void){
    ctlInfo=(InstCtlInfo)(ctlInfo|CtlInDSlot);
  }
  void beNoDSlot(void){
    ctlInfo=(InstCtlInfo)(ctlInfo&(~CtlInDSlot));
  }
  bool isInDSlot(void){
    return ctlInfo&CtlInDSlot;
  }
};

#endif // !(defined INST_DESC_H)

