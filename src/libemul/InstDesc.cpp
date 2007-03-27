// We need to emulate all sorts of floating point operations
#include <math.h>
// We need to control the rounding mode for floating point operations
#include <fenv.h>

#include "InstDesc.h"
#include "ThreadContext.h"
#include "MipsSysCalls.h"
#include "MipsRegs.h"
// To get definition of fail()
#include "EmulInit.h"
// This is the SESC Instruction class to let us create a DInst
#include "Instruction.h"

namespace Mips {

  // This structure defines the various bit-fields in a MIPS instruction
  typedef struct{
    // We use a union to allow fields to be read for different instruction types
    union{
      
      // This is the entire 32-bit MIPS instruction
      uint32_t whole;
      
      // Now we define individual bit-fields for op, sa, rd, rs, rt, func, immed, and targ
      
      // The R-type funct field is in the lowermost 6 bits
      // The R-type shift amount (SA) field is in the next five lowermost bits
      // The R-type RD field is in the next five lowermost bits
      // The R-type and I-type RT field is in the next five lowermost bits
      // The opcode filed (all types) is in the uppermost six bits
      struct{
	unsigned int func : 6;
	unsigned int sa : 5;
	unsigned int rd : 5;
	unsigned int rt : 5;
	unsigned int rs : 5;
	unsigned int op : 6;
      };
      // The I-type unsigned immediate field is in the lowermost 16 bits
      struct{
	unsigned int uimmed : 16;
	unsigned int _uimmed_rest : 16;
      };
      // The I-type signed immediate field is in the lowermost 16 bits
      struct{
	signed int   simmed : 16;
	unsigned int _simmed_rest : 16;
      };
      // The J-type jump target field is in the lowermost 26 bits
      struct{
	unsigned int targ : 26;
	unsigned int _targ_rest : 6;
      };
      // The "code" field for the breakpoint and syscall instruction
      // is in 20 bits between the func and op fields
      struct{
	unsigned int _excode_func: 6;
	unsigned int excode : 20;
	unsigned int _excode_op: 6;
      };
      // The 10-bit "code" field for conditional trap instructions is in bits 6..15
      struct{
	unsigned int _trcode_func: 6;
	unsigned int trcode : 10;
	unsigned int _trcode_op: 16;
      };    
      // Floating point (Cop1 and Cop1x) instructions have a different encoding
      struct{
	unsigned int _fd_func : 6;
	unsigned int fd : 5;
	unsigned int fs : 5;
	unsigned int ft : 5;
	unsigned int fmt : 5;
	unsigned int _fmt_op : 6;
      };
      struct{
	unsigned int _fccc_rest : 8;
	unsigned int fccc : 3;
      unsigned int _ndtf_rest : 5;
	unsigned int ndtf  : 2;
      unsigned int fbcc : 3;
	unsigned int fr   : 5;
	unsigned int _fr_rest2 : 6;
      };
    };
  } RawInst;

  typedef enum{
    //This is an invalid opcode
    OpInvalid,
    // Opcodes decoded using the "op" field begin here
    MOpSpecial, MOpRegimm, OpJ,       OpJal,     OpBeq,     OpBne,    OpBlez,   OpBgtz,
    OpAddi,     OpAddiu,   OpSlti,    OpSltiu,   OpAndi,    OpOri,    OpXori,   OpLui,
    MOpCop0,    MOpCop1,   MOpCop2,   MOpCop1x,  OpBeql,    OpBnel,   OpBlezl,  OpBgtzl,
    OpDaddi,    OpDaddiu,  OpLdl,     OpLdr,     MOpInv1C,  MOpInv1D, MOpInv1E, MOpInv1F,
    OpLb,       OpLh,      OpLwl,     OpLw,      OpLbu,     OpLhu,    OpLwr,    OpLwu,
    OpSb,       OpSh,      OpSwl,     OpSw,      OpSdl,     OpSdr,    OpSwr,    MOpInv2F,
    OpLl,       OpLwc1,    OpLwc2,    OpPref,    OpLld,     OpLdc1,   OpLdc2,   OpLd,
    OpSc,       OpSwc1,    OpSwc2,    MOpInv3B,  OpScd,     OpSdc1,   OpSdc2,   OpSd,
    // Opcodes decoded using the "function" field for MOpSpecial begin here
    OpSll,      SOpMovci,  OpSrl,     OpSra,     OpSllv,    SOpInv05, OpSrlv,   OpSrav,
    OpJr,       OpJalr,    OpMovz,    OpMovn,    OpSyscall, OpBreak,  SOpInv0E, OpSync,
    OpMfhi,     OpMthi,    OpMflo,    OpMtlo,    OpDsllv,   SOpInv15, OpDsrlv,  OpDsrav,
    OpMult,     OpMultu,   OpDiv,     OpDivu,    OpDmult,   OpDmultu, OpDdiv,   OpDdivu,
    OpAdd,      OpAddu,    OpSub,     OpSubu,    OpAnd,     OpOr,     OpXor,    OpNor,
    SOpInv28,   SOpInv29,  OpSlt,     OpSltu,    OpDadd,    OpDaddu,  OpDsub,   OpDsubu,
    OpTge,      OpTgeu,    OpTlt,     OpTltu,    OpTeq,     SOpInv35, OpTne,    SOpInv37,
    OpDsll,     SOpInv39,  OpDsrl,    OpDsra,    OpDsll32,  SOpInv3D, OpDsrl32, OpDsra32,
    // Opcodes decoded using the "rt" field for MOpRegimm begin here
    OpBltz,     OpBgez,    OpBltzl,   OpBgezl,   ROpInv04,  ROpInv05, ROpInv06, ROpInv07,
    OpTgei,     OpTgeiu,   OpTlti,    OpTltiu,   OpTeqi,    ROpInv0D, OpTnei,   ROpInv0F,
    OpBltzal,   OpBgezal,  OpBltzall, OpBgezall, ROpInv14,  ROpInv15, ROpInv16, ROpInv17,
    ROpInv18,   ROpInv19,  ROpInv1A,  ROpInv1B,  ROpInv1C,  ROpInv1D, ROpInv1E, ROpInv1F,
    // Opcodes decoded using the "fmt" field for MOpCop1
    OpMfc1,    OpDmfc1,    OpCfc1,    C1OpInv03, OpMtc1,    OpDmtc1,  OpCtc1,   C1OpInv07,
    C1OpBc,    C1OpInv09,  C1OpInv0A, C1OpInv0B, C1OpInv0C, C1OpInv0D,C1OpInv0E,C1OpInv0F, 
    C1OpFmts,  C1OpFmtd,   C1OpInv12, C1OpInv13, C1OpFmtw,  C1OpFmtl, C1OpInv16,C1OpInv17,
    C1OpInv18, C1OpInv19,  C1OpInv1A, C1OpInv1B, C1OpInv1C, C1OpInv1D,C1OpInv1E,C1OpInv1F,
    // Opcodes decoded using the "func" field for MOpCop1x
    OpLwxc1,   OpLdxc1,   CXOpInv02, CXOpInv03, CXOpInv04, CXOpInv05, CXOpInv06, CXOpInv07,
    OpSwxc1,   OpSdxc1,   CXOpInv0A, CXOpInv0B, CXOpInv0C, CXOpInv0D, CXOpInv0E, OpPrefx,
    CXOpInv10, CXOpInv11, CXOpInv12, CXOpInv13, CXOpInv14, CXOpInv15, CXOpInv16, CXOpInv17,
    CXOpInv18, CXOpInv19, CXOpInv1A, CXOpInv1B, CXOpInv1C, CXOpInv1D, CXOpInv1E, CXOpInv1F,
    OpFmadds,  OpFmaddd,  CXOpInv22, CXOpInv23, CXOpInv24, CXOpInv25, CXOpInv26, CXOpInv27,
    OpFmsubs,  OpFmsubd,  CXOpInv2A, CXOpInv2B, CXOpInv2C, CXOpInv2D, CXOpInv2E, CXOpInv2F,
    OpFnmadds, OpFnmaddd, CXOpInv32, CXOpInv33, CXOpInv34, CXOpInv35, CXOpInv36, CXOpInv37,
    OpFnmsubs, OpFnmsubd, CXOpInv3A, CXOpInv3B, CXOpInv3C, CXOpInv3D, CXOpInv3E, CXOpInv3F,
    // Opcodes decoded using the 2-bit "nd/tf" field for MOpSpecial and SOpMovci
    OpMovf,    OpMovt,    MVOpInv2,  MVOpInv3,
    // Opcodes decoded using the 2-bit "nd/tf" field for MOpCop1 and C1OpBc
    OpFbc1f,   OpFbc1t,   OpFbc1fl,  OpFbc1tl,
    // Opcodes decoded using the "func" field for MOpCop1 and C1OpFmts
    OpFadds,   OpFsubs,   OpFmuls,   OpFdivs,   OpFsqrts,  OpFabss,   OpFmovs,  OpFnegs,
    OpFroundls,OpFtruncls,OpFceills, OpFfloorls,OpFroundws,OpFtruncws,OpFceilws,OpFfloorws,
    FSOpInv10, FSOpMovcf, OpFmovzs,  OpFmovns,  FSOpInv14, OpFrecips, OpFrsqrts,FSOpInv17,
    FSOpInv18, FSOpInv19, FSOpInv1A, FSOpInv1B, FSOpInv1C, FSOpInv1D, FSOpInv1E,FSOpInv1F,
    FSOpInv20, OpFcvtds,  FSOpInv22, FSOpInv23, OpFcvtws,  OpFcvtls,  FSOpInv26,FSOpInv27,
    FSOpInv28, FSOpInv29, FSOpInv2A, FSOpInv2B, FSOpInv2C, FSOpInv2D, FSOpInv2E,FSOpInv2F,
    OpFcfs,    OpFcuns,   OpFceqs,   OpFcueqs,  OpFcolts,  OpFcults,  OpFcoles, OpFcules,
    OpFcsfs,   OpFcngles, OpFcseqs,  OpFcngls,  OpFclts,   OpFcnges,  OpFcles,  OpFcngts,
    // Opcodes decoded using the "nd/tf" field for MOpCop1, C1OpFmts, and FSOpMovcf
    OpFmovfs,  OpFmovts,  FSMOpInv2, FSMOpInv3,
    // Opcodes decoded using the "func" field for MOpCop1 and C1OpFmtd
    OpFaddd,   OpFsubd,   OpFmuld,   OpFdivd,   OpFsqrtd,  OpFabsd,   OpFmovd,  OpFnegd,
    OpFroundld,OpFtruncld,OpFceilld, OpFfloorld,OpFroundwd,OpFtruncwd,OpFceilwd,OpFfloorwd,
    FDOpInv10, FDOpMovcf, OpFmovzd,  OpFmovnd,  FDOpInv14, OpFrecipd, OpFrsqrtd,FDOpInv17,
    FDOpInv18, FDOpInv19, FDOpInv1A, FDOpInv1B, FDOpInv1C, FDOpInv1D, FDOpInv1E,FDOpInv1F,
    OpFcvtsd,  FDOpInv21, FDOpInv22, FDOpInv23, OpFcvtwd,  OpFcvtld,  FDOpInv26,FDOpInv27,
    FDOpInv28, FDOpInv29, FDOpInv2A, FDOpInv2B, FDOpInv2C, FDOpInv2D, FDOpInv2E,FDOpInv2F,
    OpFcfd,    OpFcund,   OpFceqd,   OpFcueqd,  OpFcoltd,  OpFcultd,  OpFcoled, OpFculed,
    OpFcsfd,   OpFcngled, OpFcseqd,  OpFcngld,  OpFcltd,   OpFcnged,  OpFcled,  OpFcngtd,
    // Opcodes decoded using the "nd/tf" field for MOpCop1, C1OpFmts, and FDOpMovcf
    OpFmovfd,  OpFmovtd,  FDMOpInv2, FDMOpInv3,
    // Opcodes decoded using the "func" field for MOpCop1 and C1OpFmtw
    FWOpInv00, FWOpInv01, FWOpInv02, FWOpInv03, FWOpInv04, FWOpInv05, FWOpInv06,FWOpInv07,
    FWOpInv08, FWOpInv09, FWOpInv0A, FWOpInv0B, FWOpInv0C, FWOpInv0D, FWOpInv0E,FWOpInv0F,
    FWOpInv10, FWOpInv11, FWOpInv12, FWOpInv13, FWOpInv14, FWOpInv15, FWOpInv16,FWOpInv17,
    FWOpInv18, FWOpInv19, FWOpInv1A, FWOpInv1B, FWOpInv1C, FWOpInv1D, FWOpInv1E,FWOpInv1F,
    OpFcvtsw,  OpFcvtdw,  FWOpInv22, FWOpInv23, FWOpInv24, FWOpInv25, FWOpInv26,FWOpInv27,
    FWOpInv28, FWOpInv29, FWOpInv2A, FWOpInv2B, FWOpInv2C, FWOpInv2D, FWOpInv2E,FWOpInv2F,
    FWOpInv30, FWOpInv31, FWOpInv32, FWOpInv33, FWOpInv34, FWOpInv35, FWOpInv36,FWOpInv37,
    FWOpInv38, FWOpInv39, FWOpInv3A, FWOpInv3B, FWOpInv3C, FWOpInv3D, FWOpInv3E,FWOpInv3F,
    // Opcodes decoded using the "func" field for MOpCop1 and C1OpFmtl
    FLOpInv00, FLOpInv01, FLOpInv02, FLOpInv03, FLOpInv04, FLOpInv05, FLOpInv06,FLOpInv07,
    FLOpInv08, FLOpInv09, FLOpInv0A, FLOpInv0B, FLOpInv0C, FLOpInv0D, FLOpInv0E,FLOpInv0F,
    FLOpInv10, FLOpInv11, FLOpInv12, FLOpInv13, FLOpInv14, FLOpInv15, FLOpInv16,FLOpInv17,
    FLOpInv18, FLOpInv19, FLOpInv1A, FLOpInv1B, FLOpInv1C, FLOpInv1D, FLOpInv1E,FLOpInv1F,
    OpFcvtsl,  OpFcvtdl,  FLOpInv22, FLOpInv23, FLOpInv24, FLOpInv25, FLOpInv26,FLOpInv27,
    FLOpInv28, FLOpInv29, FLOpInv2A, FLOpInv2B, FLOpInv2C, FLOpInv2D, FLOpInv2E,FLOpInv2F,
    FLOpInv30, FLOpInv31, FLOpInv32, FLOpInv33, FLOpInv34, FLOpInv35, FLOpInv36,FLOpInv37,
    FLOpInv38, FLOpInv39, FLOpInv3A, FLOpInv3B, FLOpInv3C, FLOpInv3D, FLOpInv3E,FLOpInv3F,
    // Derived opcodes (special cases of real opcodes)
    OpNop,  // Do nothing
    OpB,    // Unconditional branch (IP-relative)
    OpBal,  // Unconditional branch and link (IP-relative)
    OpBeqz, // Branch if equal to zero
    OpBeqzl,// Branch if equal to zero likely
    OpBnez, // Branch if not zero
    OpBnezl,// Branch if not zero likely
    OpMov,  // GPR-to-GPR move
    OpLi,   // Put immediate in GPR
    OpClr,  // Put zero in GPR
    OpNot,  // Bit-wise not
    OpNeg,  // Arithmetic negate (unary minus)
    OpNegu, // Arithmetic negate (unary minus) without overflow detection
    NumOfOpcodes,

    // These have to be after "NumOfOpcodes", they are not real opcodes
    
    // Where the opcodes begin (decodes the op field)
    OpBase=MOpSpecial,
    // Where the opcodes for op==MOpSpecial begin
    SOpBase=OpSll,
  // Where the opcodes for op==MOpRegimm begin
    ROpBase=OpBltz,
    // Where the opcodes for op==MOpCop1 begin
    C1OpBase=OpMfc1,
    // where the opcodes for op==MOpCop1x begin
    CXOpBase=OpLwxc1,
    // Where the opcodes for op=MOpSpecial and func=SOpMovci begin
    MVOpBase=OpMovf,
    // Where the opcodes for op==MOpCop1 and fmt==C1OpBC begin
    FBOpBase=OpFbc1f,
    // Where the opcodes for op==MOpCop1 and fmt==C1OpFmts begin
    FSOpBase=OpFadds,
    // Where the opcodes for op==MOpCop1, fmt==C1OpFmts, and func==FSOpMovcf begin
    FSMOpBase=OpFmovfs,
    // Where the opcodes for op==MOpCop1 and fmt==C1OpFmtd begin
    FDOpBase=OpFaddd,
    // Where the opcodes for op==MOpCop1, fmt==C1OpFmtd, and func==FDOpMovcf begin
    FDMOpBase=OpFmovfd,
    // Where the opcodes for op==MOpCop1 and fmt==C1OpFmtw begin
    FWOpBase=FWOpInv00,
    // Where the opcodes for op==MOpCop1 and fmt==C1OpFmtl begin
    FLOpBase=FLOpInv00,
  } Opcode;
  
  static inline void preExec(InstDesc *inst, ThreadContext *context){
#if (defined DEBUG_BENCH)
    context->execInst(inst->addr,getReg<uint32_t>(context,static_cast<RegName>(RegSP)));
#endif
  }

  // Called by non-jump emulX functions to handle transition to the next instruction
  template<bool delaySlot>
  inline void emulNext(InstDesc *inst, ThreadContext *context){
    if(delaySlot&&(context->getJumpInstDesc()!=NoJumpInstDesc)){
      context->setNextInstDesc(context->getJumpInstDesc());
      context->setJumpInstDesc(NoJumpInstDesc);
    }else{
      I(inst->nextInst->addr==inst->addr+(inst->ctlInfo&CtlISize));
      context->setNextInstDesc(inst->nextInst);
    }
    handleSignals(context);
    I(context->getNextInstDesc()!=InvalidInstDesc);
  }

  template<Opcode op, bool delaySlot, CpuMode mode>
  void emulNop(InstDesc *inst, ThreadContext *context){
    preExec(inst,context);
    return emulNext<delaySlot>(inst,context);
  }

  template<Opcode op, typename RegSigT, typename RegUnsT>
  void calcJump(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    // Evaluate the branch/jump condition
    bool cond;
    switch(op){
    case OpJ:  case OpB: case OpJal: case OpJr: case OpJalr: case OpBal:
      cond=true;
      break;
    case OpBeq:  case OpBeql:
      cond=(getReg<RegSigT>(context,inst->regSrc1)==getReg<RegSigT>(context,inst->regSrc2));
      break;
    case OpBeqz:  case OpBeqzl:
      cond=(getReg<RegSigT>(context,inst->regSrc1)==0);
      break;
    case OpBne:  case OpBnel:
      cond=(getReg<RegSigT>(context,inst->regSrc1)!=getReg<RegSigT>(context,inst->regSrc2));
      break;
    case OpBnez:  case OpBnezl:
      cond=(getReg<RegSigT>(context,inst->regSrc1)!=0);
      break;
    case OpBlez: case OpBlezl:
      cond=(getReg<RegSigT>(context,inst->regSrc1)<=0);
      break;
    case OpBgez: case OpBgezl: case OpBgezal: case OpBgezall:
      cond=(getReg<RegSigT>(context,inst->regSrc1)>=0);
      break;
    case OpBgtz: case OpBgtzl:
      cond=(getReg<RegSigT>(context,inst->regSrc1)>0);
      break;
    case OpBltz: case OpBltzl: case OpBltzal: case OpBltzall:
      cond=(getReg<RegSigT>(context,inst->regSrc1)<0);
      break;
    case OpFbc1f: case OpFbc1fl: case OpFbc1t: case OpFbc1tl:
      {
	uint32_t ccMask=inst->imm2.u; I(ccMask&0xFE800000);
	I(static_cast<MipsRegName>(inst->regSrc1)==RegFCSR);
	uint32_t ccReg=getReg<uint32_t>(context,static_cast<RegName>(RegFCSR));
	switch(op){
        case OpFbc1f: case OpFbc1fl:
	  cond=!(ccReg&ccMask); break;
        case OpFbc1t: case OpFbc1tl:
          cond=(ccReg&ccMask); break;
	}
      } break;
    default:
      fail("emulJump: unsupported instruction at 0x%08x\n",inst->addr); break;
    }
    // Link if needed
    switch(op){
    case OpJal: case OpJalr: case OpBgezal: case OpBltzal: case OpBgezall: case OpBltzall: case OpBal:
      setReg<RegUnsT>(context,inst->regDst,inst->addr+2*sizeof(RawInst));
      break;
    default:
      break;
    }
    if(cond){
      // Determine and set the branch target
      // Only jr and jalr jump to a register address, everything else jumps to pre-computed addresses
      switch(op){
      case OpJr: case OpJalr:
	// Only jr and jalr jump to a register address
	context->setJumpInstDesc(context->virt2inst(getReg<RegUnsT>(context,inst->regSrc1)));
        break;
      default:
	context->setJumpInstDesc(inst->imm1.i); break;
      }
#if (defined DEBUG_BENCH)
      context->execJump(inst->addr,context->getJumpInstDesc()->addr);
#endif
    }else{
      switch(op){
      case OpBeql:   case OpBeqzl: case OpBnel:  case OpBnezl: case OpBlezl:   case OpBgezl: 
      case OpBgtzl:  case OpBltzl: case OpBgezall: case OpBltzall:
      case OpFbc1fl: case OpFbc1tl:
	// Instruction after the delay slot executes next
	I(context->virt2inst(inst->addr+2*sizeof(RawInst))==inst->nextInst->nextInst);
	context->setNextInstDesc(inst->nextInst->nextInst);
	return;
      default:
	// All othes simply continue to the delay slot
	break;
      }
    }
    I(context->virt2inst(inst->nextInst->addr)==inst->nextInst);
    context->setNextInstDesc(inst->nextInst);
  }

  template<Opcode op, bool delaySlot, CpuMode mode>
  void emulJump(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    I(!delaySlot);
    preExec(inst,context);
    switch(mode){
    case Mips32: calcJump<op,int32_t,uint32_t>(inst,context); break;
    case Mips64: calcJump<op,int64_t,uint64_t>(inst,context); break;
    }
  }

  template<Opcode op, typename RegSigT, typename RegUnsT>
  inline bool evalTcnd(InstDesc *inst, ThreadContext *context){
    I(inst->op==op);
    switch(op){
    case OpTge:
      return (getReg<RegSigT>(context,inst->regSrc1)>=getReg<RegSigT>(context,inst->regSrc2));
    case OpTgei:
      return (getReg<RegSigT>(context,inst->regSrc1)>=static_cast<RegSigT>(inst->imm1.s));
    case OpTgeu:
      return (getReg<RegUnsT>(context,inst->regSrc1)>=getReg<RegUnsT>(context,inst->regSrc2));
    case OpTgeiu:
      return (getReg<RegUnsT>(context,inst->regSrc1)>=static_cast<RegUnsT>(static_cast<RegSigT>(inst->imm1.s)));
    case OpTlt:
      return (getReg<RegSigT>(context,inst->regSrc1)<getReg<RegSigT>(context,inst->regSrc2));
    case OpTlti:
      return (getReg<RegSigT>(context,inst->regSrc1)<static_cast<RegSigT>(inst->imm1.s));
    case OpTltu:
      return (getReg<RegUnsT>(context,inst->regSrc1)<getReg<RegUnsT>(context,inst->regSrc2));
    case OpTltiu:
      return (getReg<RegUnsT>(context,inst->regSrc1)<static_cast<RegUnsT>(static_cast<RegSigT>(inst->imm1.s)));
    case OpTeq:
      return (getReg<RegSigT>(context,inst->regSrc1)==getReg<RegSigT>(context,inst->regSrc2));
    case OpTeqi:
      return (getReg<RegSigT>(context,inst->regSrc1)==static_cast<RegSigT>(inst->imm1.s));
    case OpTne:
      return (getReg<RegSigT>(context,inst->regSrc1)!=getReg<RegSigT>(context,inst->regSrc2));
    case OpTnei:
      return (getReg<RegSigT>(context,inst->regSrc1)!=static_cast<RegSigT>(inst->imm1.s));
    }
  }
  template<Opcode op, bool delaySlot, CpuMode mode>
  void emulTcnd(InstDesc *inst, ThreadContext *context){
    I(inst->op==op);
    preExec(inst,context);
    bool cond;
    switch(mode){
    case Mips32: cond=evalTcnd<op,int32_t,uint32_t>(inst,context); break;
    case Mips64: cond=evalTcnd<op,int64_t,uint64_t>(inst,context); break;
    }
    if(cond)
      fail("emulTcnd: trap caused at 0x%08x, not supported yet\n",inst->addr);
    return emulNext<delaySlot>(inst,context);
  }
  
  template<Opcode op, typename RegSigT, typename RegUnsT>
  inline void calcAlu(InstDesc *inst, ThreadContext *context){
    I(inst->op==op);
    switch(op){
    case OpLui: case OpLi: case OpClr: {
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(inst->imm1.s));
    }  break;
    case OpAddi: case OpAddiu: {
      int32_t res=getReg<int32_t>(context,inst->regSrc1)+static_cast<int32_t>(inst->imm1.s);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(res));
    } break;
    case OpAdd: case OpAddu: {
      int32_t res=getReg<int32_t>(context,inst->regSrc1)+getReg<int32_t>(context,inst->regSrc2);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(res));
    } break;
    case OpSub: case OpSubu: {
      int32_t res=getReg<int32_t>(context,inst->regSrc1)-getReg<int32_t>(context,inst->regSrc2);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(res));
    } break;
    case OpNeg: case OpNegu: {
      int32_t res=-getReg<int32_t>(context,inst->regSrc1);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(res));
    } break;
    case OpSll: {
      uint32_t res=(getReg<uint32_t>(context,inst->regSrc1)<<inst->imm1.u);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(static_cast<int32_t>(res)));
    }  break;
    case OpSllv: {
      uint32_t res=(getReg<uint32_t>(context,inst->regSrc1)<<(getReg<uint32_t>(context,inst->regSrc2)&0x1F));
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(static_cast<int32_t>(res)));
    }  break;
    case OpSrl: {
      uint32_t res=(getReg<uint32_t>(context,inst->regSrc1)>>inst->imm1.u);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(static_cast<int32_t>(res)));
    }  break;
    case OpSrlv: {
      uint32_t res=(getReg<uint32_t>(context,inst->regSrc1)>>(getReg<uint32_t>(context,inst->regSrc2)&0x1F));
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(static_cast<int32_t>(res)));
    }  break;
    case OpSra: {
      int32_t res=(getReg<int32_t>(context,inst->regSrc1)>>inst->imm1.s);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(res));
    }  break;
    case OpSrav: {
      int32_t res=(getReg<int32_t>(context,inst->regSrc1)>>(getReg<int32_t>(context,inst->regSrc2)&0x1F));
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(res));
    }  break;
    case OpAndi:
      setReg<RegUnsT>(context,inst->regDst,getReg<RegUnsT>(context,inst->regSrc1)&static_cast<RegUnsT>(inst->imm1.u));
      break;
    case OpAnd: {
      RegUnsT res=(getReg<RegUnsT>(context,inst->regSrc1)&getReg<RegUnsT>(context,inst->regSrc2));
      setReg<RegUnsT>(context,inst->regDst,res);
    } break;
    case OpOri:
      setReg<RegUnsT>(context,inst->regDst,getReg<RegUnsT>(context,inst->regSrc1)|static_cast<RegUnsT>(inst->imm1.u));
      break;
    case OpOr: {
      RegUnsT res=(getReg<RegUnsT>(context,inst->regSrc1)|getReg<RegUnsT>(context,inst->regSrc2));
      setReg<RegUnsT>(context,inst->regDst,res);
    } break;
    case OpXori:
      setReg<RegUnsT>(context,inst->regDst,getReg<RegUnsT>(context,inst->regSrc1)^static_cast<RegUnsT>(inst->imm1.u));
      break;
    case OpXor: {
      RegUnsT res=(getReg<RegUnsT>(context,inst->regSrc1)^getReg<RegUnsT>(context,inst->regSrc2));
      setReg<RegUnsT>(context,inst->regDst,res);
    } break;
    case OpNor: {
      RegUnsT res=~(getReg<RegUnsT>(context,inst->regSrc1)|getReg<RegUnsT>(context,inst->regSrc2));
      setReg<RegUnsT>(context,inst->regDst,res);
    } break;
    case OpNot: {
      RegUnsT res=~getReg<RegUnsT>(context,inst->regSrc1);
      setReg<RegUnsT>(context,inst->regDst,res);
    } break;
    case OpSlti: {
      bool cond=(getReg<RegSigT>(context,inst->regSrc1)<static_cast<RegSigT>(inst->imm1.s));
      setReg<RegSigT>(context,inst->regDst,cond?1:0);
    }  break;
    case OpSlt:  {
#if (defined DEBUG)
      if(static_cast<MipsRegName>(inst->regSrc1)==RegZero){
	setReg<RegSigT>(context,inst->regDst,(0<getReg<RegSigT>(context,inst->regSrc2))?1:0);
	break;
      }
#endif
      bool cond=(getReg<RegSigT>(context,inst->regSrc1)<getReg<RegSigT>(context,inst->regSrc2));
      setReg<RegSigT>(context,inst->regDst,cond?1:0);
    }  break;
    case OpSltiu: {
      bool cond=(getReg<RegUnsT>(context,inst->regSrc1)<static_cast<RegUnsT>(static_cast<RegSigT>(inst->imm1.s)));
      setReg<RegSigT>(context,inst->regDst,cond?1:0);
    }  break;
    case OpSltu:  {
#if (defined DEBUG)
      if(static_cast<MipsRegName>(inst->regSrc1)==RegZero){
	setReg<RegSigT>(context,inst->regDst,(0<getReg<RegUnsT>(context,inst->regSrc2))?1:0);
	break;
      }
#endif
      bool cond=(getReg<RegUnsT>(context,inst->regSrc1)<getReg<RegUnsT>(context,inst->regSrc2));
      setReg<RegSigT>(context,inst->regDst,cond?1:0);
    }  break;
    case OpMfhi: case OpMflo: case OpMthi: case OpMtlo: case OpMov:
      setReg<RegSigT>(context,inst->regDst,getReg<RegSigT>(context,inst->regSrc1));
      break;
    case OpMovf:
      I(inst->imm1.u&0xFE800000); I(static_cast<MipsRegName>(inst->regSrc2)==RegFCSR);
      if((getReg<uint32_t>(context,static_cast<RegName>(RegFCSR))&static_cast<uint32_t>(inst->imm1.u))==0)
        setReg<RegSigT>(context,inst->regDst,getReg<RegSigT>(context,inst->regSrc1));
      break;
    case OpMovt:
      I(inst->imm1.u&0xFE800000); I(static_cast<MipsRegName>(inst->regSrc2)==RegFCSR);
      if((getReg<uint32_t>(context,static_cast<RegName>(RegFCSR))&static_cast<uint32_t>(inst->imm1.u))!=0)
        setReg<RegSigT>(context,inst->regDst,getReg<RegSigT>(context,inst->regSrc1));
      break;
    case OpMovz:
      if(getReg<RegSigT>(context,inst->regSrc2)==0)
        setReg<RegSigT>(context,inst->regDst,getReg<RegSigT>(context,inst->regSrc1));
      break;
    case OpMovn:
      if(getReg<RegSigT>(context,inst->regSrc2)!=0)
        setReg<RegSigT>(context,inst->regDst,getReg<RegSigT>(context,inst->regSrc1));
      break;
    case OpMult: {
      int64_t  res=
        static_cast<int64_t>(getReg<int32_t>(context,inst->regSrc1))*
        static_cast<int64_t>(getReg<int32_t>(context,inst->regSrc2));
      int32_t *ptr=(int32_t *)(&res);
      setReg<RegSigT>(context,static_cast<RegName>(RegLo),static_cast<RegSigT>(*(ptr+(__BYTE_ORDER==__BIG_ENDIAN))));
      setReg<RegSigT>(context,static_cast<RegName>(RegHi),static_cast<RegSigT>(*(ptr+(__BYTE_ORDER!=__BIG_ENDIAN))));
    } break;
    case OpMultu: {
      uint64_t  res=
        static_cast<uint64_t>(getReg<uint32_t>(context,inst->regSrc1))*
        static_cast<uint64_t>(getReg<uint32_t>(context,inst->regSrc2));
      int32_t *ptr=(int32_t *)(&res);
      setReg<RegSigT>(context,static_cast<RegName>(RegLo),static_cast<RegSigT>(*(ptr+(__BYTE_ORDER==__BIG_ENDIAN))));
      setReg<RegSigT>(context,static_cast<RegName>(RegHi),static_cast<RegSigT>(*(ptr+(__BYTE_ORDER!=__BIG_ENDIAN))));
    } break;
    case OpDiv: {
      int32_t par1=getReg<int32_t>(context,inst->regSrc1);
      int32_t par2=getReg<int32_t>(context,inst->regSrc2);
      setReg<RegSigT>(context,static_cast<RegName>(RegLo),static_cast<RegSigT>(par1/par2));
      setReg<RegSigT>(context,static_cast<RegName>(RegHi),static_cast<RegSigT>(par1%par2));
    } break;
    case OpDivu: {
      uint32_t par1=getReg<uint32_t>(context,inst->regSrc1);
      uint32_t par2=getReg<uint32_t>(context,inst->regSrc2);
      setReg<RegSigT>(context,static_cast<RegName>(RegLo),static_cast<RegSigT>(static_cast<RegUnsT>(par1/par2)));
      setReg<RegSigT>(context,static_cast<RegName>(RegHi),static_cast<RegSigT>(static_cast<RegUnsT>(par1%par2)));
    } break;
    default:
      fail("emulAlu: unsupported instruction at 0x%08x\n",inst->addr); break;
    }
  }

  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulAlu(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    switch(mode){
    case Mips32: calcAlu<op,int32_t,uint32_t>(inst,context); break;
    case Mips64: calcAlu<op,int64_t,uint64_t>(inst,context); break;
    }
    return emulNext<delaySlot>(inst,context);
  }

  static int Mips32_RoundMode[] = {
    FE_TONEAREST,  /* 00 nearest   */
    FE_TOWARDZERO, /* 01 zero      */
    FE_UPWARD,     /* 10 plus inf  */
    FE_DOWNWARD    /* 11 minus inf */
  };

  template<Opcode op, typename RegSigT, CpuMode mode>
  void calcMvcop(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    switch(op){
    case OpMfc1:
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(getRegFp<int32_t,mode>(context,inst->regSrc1)));
      break;
    case OpMtc1:
      setRegFp<int32_t,mode>(context,inst->regDst,getReg<int32_t>(context,inst->regSrc1));
      break;
    case OpCfc1:
      I(static_cast<MipsRegName>(inst->regSrc1)==RegFCSR);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(getReg<int32_t>(context,inst->regSrc1)));
      break;
    case OpCtc1:
      I(static_cast<MipsRegName>(inst->regDst)==RegFCSR);
      setReg<uint32_t>(context,inst->regDst,getReg<uint32_t>(context,inst->regSrc1));
      break;
    }
  }
  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulMvcop(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    switch(mode){
    case Mips32: calcMvcop<op,int32_t,mode>(inst,context); break;
    case Mips64: calcMvcop<op,int64_t,mode>(inst,context); break;
    }
    return emulNext<delaySlot>(inst,context);
  }

  template<Opcode op, typename RegSigT, typename RegUnsT>
  inline RegUnsT calcAddr(const InstDesc *inst, const ThreadContext *context){
    switch(op){
    case OpLb:    case OpLbu: case OpSb:
    case OpLh:    case OpLhu: case OpSh:
    case OpLw:    case OpLwu: case OpSw:
    case OpLwl:   case OpLwr: case OpSwl: case OpSwr:
    case OpLl:    case OpSc:
    case OpLd:    case OpSd:
    case OpLwc1:  case OpSwc1:
    case OpLdc1:  case OpSdc1:
      I(((MipsRegName)(inst->regSrc1)>=GprNameLb)&&((MipsRegName)(inst->regSrc1)<GprNameUb));
      return static_cast<RegUnsT>(getReg<RegSigT>(context,inst->regSrc1)+static_cast<RegSigT>(inst->imm1.s));
    case OpLwxc1: case OpSwxc1:
    case OpLdxc1: case OpSdxc1:
      I(((MipsRegName)(inst->regSrc1)>=GprNameLb)&&((MipsRegName)(inst->regSrc1)<GprNameUb));
      I(((MipsRegName)(inst->regSrc2)>=GprNameLb)&&((MipsRegName)(inst->regSrc2)<GprNameUb));
      return static_cast<RegUnsT>(getReg<RegSigT>(context,inst->regSrc1)+getReg<RegSigT>(context,inst->regSrc2));
    default:
      fail("calcAddr: unsupported opcode\n");
    }
    return 0;
  }
  template<typename RegUnsT>
  void growStack(ThreadContext *context){
    RegUnsT sp=getReg<RegUnsT>(context,static_cast<RegName>(RegSP));
    RegUnsT sa=context->getStackAddr();
    size_t  sl=context->getStackSize();
    I(context->getAddressSpace()->isSegment(sa,sl));
    I(sp<sa);
    I(context->getAddressSpace()->isNoSegment(sp,sa-sp));
    // If there is room to grow stack, do it
    if((sp<sa)&&(context->getAddressSpace()->isNoSegment(sp,sa-sp))){
      // Try to grow it to a 16KB boundary, but if we can't then grow only to cover sp
      RegUnsT newSa=sp-(sp&0x3FFF);
      if(!context->getAddressSpace()->isNoSegment(newSa,sa-newSa))
	newSa=sp;
      context->getAddressSpace()->growSegmentDown(sa,newSa);
      context->setStack(newSa,sa+sl);
    }
  }
  template<typename T, typename RegUnsT>
  inline bool readMem(ThreadContext *context, RegUnsT addr, T &val){
    if(!context->readMem(addr,val)){
      growStack<RegUnsT>(context);
      if(!context->readMem(addr,val))
	fail("readMem: segmentation fault\n");
    }
    return true;
  }
  template<typename T, typename RegUnsT>
  inline bool writeMem(ThreadContext *context, RegUnsT addr, const T &val){
    if(!context->writeMem(addr,val)){
      growStack<RegUnsT>(context);
      if(!context->writeMem(addr,val))
	fail("writeMem: segmentation fault\n");
    }
    return true;
  }
  template<Opcode op, typename RegSigT, typename RegUnsT, CpuMode mode>
  void calcLdSt(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    // Compute the effective (virtual) address
    RegUnsT vaddr=calcAddr<op,RegSigT,RegUnsT>(inst,context);
    //    I(vaddr!=0x10008edc);
    context->setLastDataVAddr(vaddr);
    switch(op){
    case OpLb:  {
      int8_t val;
      readMem(context,vaddr,val);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(val));
    } break;
    case OpLbu: {
      uint8_t  val;
      readMem(context,vaddr,val);
      setReg<RegUnsT>(context,inst->regDst,static_cast<RegUnsT>(val));
    } break;
    case OpLh:  {
      int16_t  val;
      readMem(context,vaddr,val);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(val));
    } break;
    case OpLhu: {
      uint16_t val;
      readMem(context,vaddr,val);
      setReg<RegUnsT>(context,inst->regDst,static_cast<RegUnsT>(val));
    } break;
    case OpLl:  // TODO: Do the actual link, then same as OpLw
    case OpLw:  {
      int32_t  val;
      readMem(context,vaddr,val);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(val));
    } break;
    case OpLwu: {
      uint32_t val;
      readMem(context,vaddr,val);
      setReg<RegUnsT>(context,inst->regDst,static_cast<RegUnsT>(val));
    } break;
    case OpLd:  {
      I(sizeof(RegSigT)>=8);
      uint64_t val;
      readMem(context,vaddr,val);
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(val));
    } break;
    case OpLwl: case OpLwr: {
      int32_t val=getReg<int32_t>(context,inst->regDst);
      cvtEndianBig(val);
      switch(op){
      case OpLwl: context->readMemToBuf(vaddr,4-(vaddr&0x3),&val); break;
      case OpLwr: context->readMemToBuf(vaddr-(vaddr&0x3),(vaddr&0x3)+1,((uint8_t *)(&val))+3-(vaddr&0x3)); break;
      }
      cvtEndianBig(val);

      uint32_t rval=getReg<uint32_t>(context,inst->regDst);
      size_t   offs=(vaddr%4);
      VAddr    addr=vaddr-offs;
      uint32_t mval;
      readMem(context,addr,mval);
      switch(op){
      case OpLwl: 
	{
	  uint32_t LoMask32[]={ 0x00000000, 0x000000FF, 0x0000FFFF, 0x00FFFFFF, 0xFFFFFFFF };
	  size_t  rbits=8*offs;
	  size_t  mbits=32-rbits;
	  rval=((rval&LoMask32[offs])|(mval<<rbits));
	  if(static_cast<int32_t>(rval)!=val) fail("OpLwl old-new mismatch\n");
	} break;
      case OpLwr:
	{
	  uint32_t HiMask32[]={ 0xFFFFFFFF, 0xFFFFFF00, 0xFFFF0000, 0xFF000000, 0x00000000 };
	  size_t  mbits=8*(offs+1);
	  size_t  rbits=32-mbits;
	  rval=((rval&(0xFFFFFFFFu<<mbits))|(mval>>rbits));
	  if(static_cast<int32_t>(rval)!=val) fail("OpLwr old-new mismatch\n");
	} break;
      }
     
      setReg<RegSigT>(context,inst->regDst,static_cast<RegSigT>(val));
    } break;
    case OpLwc1: case OpLwxc1: {
      float32_t val;
      readMem(context,vaddr,val);
      I(!isunordered(val,val));
      setRegFp<float32_t,mode>(context,inst->regDst,val);
    } break;
    case OpLdc1: case OpLdxc1: {
      float64_t val;
      readMem(context,vaddr,val);
      I(!isunordered(val,val));
      setRegFp<float64_t,mode>(context,inst->regDst,val);
    } break;
    case OpSb:
#if (defined DEBUG)
      if(static_cast<MipsRegName>(inst->regSrc2)==RegZero){
	writeMem(context,vaddr,static_cast<uint8_t>(0));
	break;
      }
#endif
      writeMem(context,vaddr,(uint8_t )(getReg<uint32_t>(context,inst->regSrc2)));
      break;
    case OpSh:
#if (defined DEBUG)
      if(static_cast<MipsRegName>(inst->regSrc2)==RegZero){
	writeMem(context,vaddr,static_cast<uint16_t>(0));
	break;
      }
#endif
      writeMem(context,vaddr,(uint16_t)(getReg<uint32_t>(context,inst->regSrc2)));
      break;
    case OpSc:  // TODO: Do the actual link check, then proceed to OpSw
#if (defined DEBUG)
      if(static_cast<MipsRegName>(inst->regSrc2)==RegZero){
	writeMem(context,vaddr,static_cast<uint32_t>(0));
	break;
      }
#endif
      writeMem(context,vaddr,getReg<uint32_t>(context,inst->regSrc2));
      setReg<RegSigT>(context,inst->regDst,1);
      break;
    case OpSw:
#if (defined DEBUG)
      if(static_cast<MipsRegName>(inst->regSrc2)==RegZero){
	writeMem(context,vaddr,static_cast<uint32_t>(0));
	break;
      }
#endif
      writeMem(context,vaddr,getReg<uint32_t>(context,inst->regSrc2));
      break;
    case OpSd:
      I(sizeof(RegUnsT)>=8);
      writeMem(context,vaddr,getReg<uint64_t>(context,inst->regSrc2));
      break;
    case OpSwl: {
      int32_t val=getReg<int32_t>(context,inst->regSrc2);
      cvtEndianBig(val);
      context->writeMemFromBuf(vaddr,4-(vaddr&0x3),&val);
    } break;
    case OpSwr: {
      int32_t val=getReg<int32_t>(context,inst->regSrc2);
      cvtEndianBig(val);
      context->writeMemFromBuf(vaddr-(vaddr&0x3),(vaddr&0x3)+1,((uint8_t *)(&val))+3-(vaddr&0x3));
    } break;
    case OpSwc1:
      writeMem(context,vaddr,getRegFp<uint32_t,mode>(context,inst->regSrc2));
      break;
    case OpSwxc1:
      writeMem(context,vaddr,getRegFp<uint32_t,mode>(context,inst->regSrc3));
      break;
    case OpSdc1:
      writeMem(context,vaddr,getRegFp<uint64_t,mode>(context,inst->regSrc2));
      break;
    case OpSdxc1:
      writeMem(context,vaddr,getRegFp<uint64_t,mode>(context,inst->regSrc3));
      break;
    }
  }

  template<Opcode op, bool delaySlot, CpuMode mode>
  void emulLdSt(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    switch(mode){
    case Mips32: calcLdSt<op,int32_t,uint32_t,mode>(inst,context); break;
    case Mips64: calcLdSt<op,int64_t,uint64_t,mode>(inst,context); break;
    }
    return emulNext<delaySlot>(inst,context);
  }

  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulFcvt(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    int savedRoundMode=fegetround();
    switch(op){
    case OpFroundws: case OpFroundwd: case OpFroundls: case OpFroundld:
      I(inst->regSrc2==RegNone);
      fesetround(FE_TONEAREST); break;
    case OpFtruncws: case OpFtruncwd: case OpFtruncls: case OpFtruncld:
      I(inst->regSrc2==RegNone);
      fesetround(FE_TOWARDZERO); break;
    case OpFceilws: case OpFceilwd: case OpFceills: case OpFceilld:
      I(inst->regSrc2==RegNone);
      fesetround(FE_UPWARD); break;
    case OpFfloorws: case OpFfloorwd: case OpFfloorls: case OpFfloorld:
      I(inst->regSrc2==RegNone);
      fesetround(FE_DOWNWARD); break;
    case OpFcvtsd: case OpFcvtsw: case OpFcvtsl:
    case OpFcvtds: case OpFcvtdw: case OpFcvtdl:
    case OpFcvtws: case OpFcvtwd:
    case OpFcvtls: case OpFcvtld:
      I((MipsRegName)(inst->regSrc2)==RegFCSR);
      fesetround(Mips32_RoundMode[getReg<uint32_t>(context,static_cast<RegName>(RegFCSR))&0x3]);
      break;
    default:
      fail("emulFcvt: unsupported opcode\n");
    }
    switch(op){
    case OpFroundws: case OpFtruncws: case OpFceilws: case OpFfloorws: case OpFcvtws:
      setRegFp<int32_t,mode>(context,inst->regDst,(int32_t)getRegFp<float32_t,mode>(context,inst->regSrc1));
      break;
    case OpFroundwd: case OpFtruncwd: case OpFceilwd: case OpFfloorwd: case OpFcvtwd:
      setRegFp<int32_t,mode>(context,inst->regDst,(int32_t)getRegFp<float64_t,mode>(context,inst->regSrc1));
      break;
    case OpFroundls: case OpFtruncls: case OpFceills: case OpFfloorls: case OpFcvtls:
      setRegFp<int64_t,mode>(context,inst->regDst,(int64_t)getRegFp<float32_t,mode>(context,inst->regSrc1));
      break;
    case OpFroundld: case OpFtruncld: case OpFceilld: case OpFfloorld: case OpFcvtld:
      setRegFp<int64_t,mode>(context,inst->regDst,(int64_t)getRegFp<float64_t,mode>(context,inst->regSrc1));
      break;
    case OpFcvtsd:
      setRegFp<float32_t,mode>(context,inst->regDst,(float32_t)getRegFp<float64_t,mode>(context,inst->regSrc1));
      break;
    case OpFcvtsw:
      setRegFp<float32_t,mode>(context,inst->regDst,(float32_t)getRegFp<int32_t,mode>(context,inst->regSrc1));
      break;
    case OpFcvtsl:
      setRegFp<float32_t,mode>(context,inst->regDst,(float32_t)getRegFp<int64_t,mode>(context,inst->regSrc1));
      break;
    case OpFcvtds:
      setRegFp<float64_t,mode>(context,inst->regDst,(float64_t)getRegFp<float32_t,mode>(context,inst->regSrc1));
      break;
    case OpFcvtdw:
      setRegFp<float64_t,mode>(context,inst->regDst,(float64_t)getRegFp<int32_t,mode>(context,inst->regSrc1));
      break;
    case OpFcvtdl:
      setRegFp<float64_t,mode>(context,inst->regDst,(float64_t)getRegFp<int64_t,mode>(context,inst->regSrc1));
      break;
    }
    fesetround(savedRoundMode);
    return emulNext<delaySlot>(inst,context);
  }

  template<Opcode op, typename RegT, CpuMode mode>
  void calcFcond(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    I(isFprName(inst->regSrc1));
    I(isFprName(inst->regSrc2));
    I(inst->regSrc3==RegNone);
    RegT val1=getRegFp<RegT,mode>(context,inst->regSrc1);
    RegT val2=getRegFp<RegT,mode>(context,inst->regSrc2);
    bool cond;
    switch(op){
    case OpFcsfs:   case OpFcsfd:   case OpFcfs:   case OpFcfd:
      cond=false;
      break;
    case OpFcngles: case OpFcngled: case OpFcuns:  case OpFcund:
      cond=isunordered(val1,val2);
      break;
    case OpFceqs:   case OpFceqd:   case OpFcseqs: case OpFcseqd:
      cond=(val1==val2);
      break;
    case OpFcueqs:  case OpFcueqd:  case OpFcngls: case OpFcngld:
      cond=((val1==val2)||isunordered(val1,val2));
      break;
    case OpFcolts:  case OpFcoltd:  case OpFclts:  case OpFcltd:
      cond=(val1<val2);
      break;
    case OpFcults:  case OpFcultd:  case OpFcnges: case OpFcnged:
      cond=((val1<val2)||isunordered(val1,val2));
      break;
    case OpFcoles:  case OpFcoled:  case OpFcles:  case OpFcled:
      cond=(val1<=val2);
      break;
    case OpFcules:  case OpFculed:  case OpFcngts: case OpFcngtd:
      cond=((val1<=val2)||isunordered(val1,val2));
      break;
    }
    switch(op){
    case OpFcsfs: case OpFcngles: case OpFcseqs: case OpFcngls:
    case OpFclts: case OpFcnges:  case OpFcles:  case OpFcngts:
    case OpFcsfd: case OpFcngled: case OpFcseqd: case OpFcngld:
    case OpFcltd: case OpFcnged:  case OpFcled:  case OpFcngtd:
      // We need to signal an exception for NaNs here
      if(isunordered(val1,val2))
        ; // Do nothing (TODO)
      break;
    default:
      // Other conditionals do not signal exceptions
      break;
    }
    uint32_t ccMask=inst->imm1.u;
    I(ccMask&0xFE800000);
    I(static_cast<MipsRegName>(inst->regDst)==RegFCSR);
    uint32_t fcsrVal=getReg<uint32_t>(context,static_cast<RegName>(RegFCSR));
    if(cond){
      fcsrVal|=ccMask;
    }else{
      fcsrVal&=(~ccMask);
    }
    setReg<uint32_t>(context,static_cast<RegName>(RegFCSR),fcsrVal);
    I(fcsrVal==getReg<uint32_t>(context,static_cast<RegName>(RegFCSR)));
  }
    
  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulFcond(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    switch(op){
    case OpFcfs:   case OpFcuns:   case OpFceqs:  case OpFcueqs:
    case OpFcolts: case OpFcults:  case OpFcoles: case OpFcules:
    case OpFcsfs:  case OpFcngles: case OpFcseqs: case OpFcngls:
    case OpFclts:  case OpFcnges:  case OpFcles:  case OpFcngts:
      // Single-precision operation
      calcFcond<op,float32_t,mode>(inst,context);
      break;
    case OpFcfd:   case OpFcund:   case OpFceqd:  case OpFcueqd:
    case OpFcoltd: case OpFcultd:  case OpFcoled: case OpFculed:
    case OpFcsfd:  case OpFcngled: case OpFcseqd: case OpFcngld:
    case OpFcltd:  case OpFcnged:  case OpFcled:  case OpFcngtd:
      // Double-precision operation
      calcFcond<op,float64_t,mode>(inst,context);
      break;
    }
    return emulNext<delaySlot>(inst,context);
  }

  template<Opcode op, typename RegT, CpuMode mode>
  inline void calcFpu1(InstDesc *inst, ThreadContext *context){
    RegT arg=getRegFp<RegT,mode>(context,inst->regSrc1);
    RegT res;
    switch(op){
    case OpFsqrts:
      res=sqrtf(arg); break;
    case OpFsqrtd:
      res=sqrt(arg); break;
    case OpFabss:
      res=fabsf(arg); break;
    case OpFabsd:
      res=fabs(arg); break;
    case OpFmovs: case OpFmovd:
      res=arg; break;
    case OpFnegs:   case OpFnegd:
      res=-arg; break;
    case OpFrecips: case OpFrecipd:
      res=1.0/arg; break;
    case OpFrsqrts:
      res=1.0/sqrtf(arg); break;
    case OpFrsqrtd:
      res=1.0/sqrt(arg); break;
    }
    setRegFp<RegT,mode>(context,inst->regDst,res);
  }
  template<Opcode op, typename RegT, CpuMode mode>
  inline void calcFpu2(InstDesc *inst, ThreadContext *context){
    RegT arg1=getRegFp<RegT,mode>(context,inst->regSrc1);
    RegT arg2=getRegFp<RegT,mode>(context,inst->regSrc2);
    RegT res;
    switch(op){
    case OpFadds: case OpFaddd:
      res=arg1+arg2; break;
    case OpFsubs: case OpFsubd:
      res=arg1-arg2; break;
    case OpFmuls: case OpFmuld:
      res=arg1*arg2; break;
    case OpFdivs: case OpFdivd:
      res=arg1/arg2; break;
    }
    setRegFp<RegT,mode>(context,inst->regDst,res);
  }
  template<Opcode op, typename RegT, CpuMode mode>
  inline void calcFpu3(InstDesc *inst, ThreadContext *context){
    RegT arg1=getRegFp<RegT,mode>(context,inst->regSrc1);
    RegT arg2=getRegFp<RegT,mode>(context,inst->regSrc2);
    RegT arg3=getRegFp<RegT,mode>(context,inst->regSrc3);
    RegT res;
    switch(op){
    case OpFmadds:  case OpFmaddd:
      res=  arg1*arg2+arg3;  break;
    case OpFnmadds: case OpFnmaddd:
      res=-(arg1*arg2+arg3); break;
    case OpFmsubs:  case OpFmsubd:
      res=  arg1*arg2-arg3;  break;
    case OpFnmsubs: case OpFnmsubd:
      res=  -(arg1*arg2-arg3); break;
    }
    setRegFp<RegT,mode>(context,inst->regDst,res);
  }
  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulFpu(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    switch(op){
    case OpFsqrts: case OpFabss: case OpFmovs:  case OpFnegs:  case OpFrecips: case OpFrsqrts:
      // Single-precision single-operand ops
      calcFpu1<op,float32_t,mode>(inst,context);
      break;
    case OpFsqrtd: case OpFabsd: case OpFmovd:  case OpFnegd:  case OpFrecipd: case OpFrsqrtd:
      // Double precision single-operand ops
      calcFpu1<op,float64_t,mode>(inst,context);
      break;
    case OpFadds: case OpFsubs: case OpFmuls: case OpFdivs:
      // Single-precision two-operand ops
      calcFpu2<op,float32_t,mode>(inst,context);
      break;
    case OpFaddd: case OpFsubd: case OpFmuld: case OpFdivd:
      // Double precision two-operand ops
      calcFpu2<op,float64_t,mode>(inst,context);
      break;
    case OpFmadds: case OpFnmadds: case OpFmsubs: case OpFnmsubs:
      // Single-precision three-operand ops
      calcFpu3<op,float32_t,mode>(inst,context);
      break;
    case OpFmaddd: case OpFnmaddd: case OpFmsubd: case OpFnmsubd:
      // Double precision three-operand ops
      calcFpu3<op,float64_t,mode>(inst,context);
      break;
    case OpFmovfs: case OpFmovfd: OpFmovts: case OpFmovtd:
      bool zero;
      switch(op){
      case OpFmovfs: case OpFmovfd: case OpFmovts: case OpFmovtd:
	I(static_cast<MipsRegName>(inst->regSrc2)==RegFCSR);
	I(inst->imm1.u&0xFE800000);
	zero=((getReg<uint32_t>(context,static_cast<RegName>(RegFCSR))&inst->imm1.u)==0);
	break;
      case OpFmovzs: case OpFmovzd: case OpFmovns: case OpFmovnd:
	I(isGprName(inst->regSrc2));
	switch(mode){
	case Mips32: zero=(getReg<int32_t>(context,inst->regSrc2)==0); break;
	case Mips64: zero=(getReg<int64_t>(context,inst->regSrc2)==0); break;
	}
	break;
      }
      bool cond;
      switch(op){
      case OpFmovzs: case OpFmovzd: case OpFmovfs: case OpFmovfd:
	cond=zero;  break;
      case OpFmovns: case OpFmovnd: case OpFmovts: case OpFmovtd:
	cond=!zero; break;
      }
      if(cond){
	switch(op){
	case OpFmovzs: case OpFmovns: case OpFmovfs: case OpFmovts:
	  setRegFp<float32_t,mode>(context,inst->regDst,getRegFp<float32_t,mode>(context,inst->regSrc1));
	  break;
	case OpFmovzd: case OpFmovnd: case OpFmovfd: case OpFmovtd:
	  setRegFp<float64_t,mode>(context,inst->regDst,getRegFp<float64_t,mode>(context,inst->regSrc1));
	  break;
	}
      }
      break;
    }
    return emulNext<delaySlot>(inst,context);
  }

  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulBreak(InstDesc *inst, ThreadContext *context){
    fail("emulBreak: BREAK instruction not supported yet\n");
  }

  template<Opcode op,bool delaySlot, CpuMode mode>
  void emulSyscl(InstDesc *inst, ThreadContext *context){
    I(!delaySlot);
    preExec(inst,context);
    // Note the ordering of emulNext and mipsSysCall, which allows
    // mipsSysCall to change control flow or simply let it continue
    emulNext<delaySlot>(inst,context);
    mipsSysCall(inst,context);
  }
  
  // Where does a destination operand of the instruction come from
  enum InstDstInfo  {DstNo, DstRd, DstFd, DstRt, DstFt, DstFs, DstFCs, DstFCSR, DstRa, DstHi, DstLo, DstHiLo};
  // Where does a source operand of the instruction come from
  enum InstSrcInfo  {SrcNo, SrcRt, SrcRs, SrcFt, SrcFs, SrcFr, SrcFCs, SrcFCSR, SrcHi, SrcLo};
  // What is the format of the immediate (if any)
  enum InstImmInfo  {
    ImmNo, ImmJpTg, ImmBrOf,
    ImmSExt, ImmZExt, ImmLui, ImmZero,
    ImmSh, ImmExCd, ImmTrCd, ImmFccc, ImmFbcc
  };

  struct OpcodeInfo{
    // The opcode, should be either OpInvalid or equal to array index
    Opcode op;
    // How does this instruction affect control flow
    InstCtlInfo ctl;
    // What kind of operation is it (ALU, Br, ...)
    InstTypInfo typ;
    // Destination, source, and immediate operands
    InstDstInfo dst;
    InstSrcInfo src1;
    InstSrcInfo src2;
    InstSrcInfo src3;
    InstImmInfo imm1;
    InstImmInfo imm2;
    // Functions that emulate this opcode, in a delay slot or not
    EmulFunc *emulFunc[2][2];

    OpcodeInfo(Opcode op=OpInvalid,
	       EmulFunc emulFunc32InDSlot=0, EmulFunc emulFunc32NotDSlot=0,
	       EmulFunc emulFunc64InDSlot=0, EmulFunc emulFunc64NotDSlot=0,
	       InstCtlInfo ctl=CtlInvalid, InstTypInfo typ=TypNop,
	       InstDstInfo dst=DstNo, InstSrcInfo src1=SrcNo, InstSrcInfo src2=SrcNo, InstSrcInfo src3=SrcNo,
	       InstImmInfo imm1=ImmNo, InstImmInfo imm2=ImmNo)
      : op(op), ctl(ctl), typ(typ), dst(dst), src1(src1), src2(src2), src3(src3), imm1(imm1), imm2(imm2)
    {
      emulFunc[true ][Mips32]=emulFunc32InDSlot;
      emulFunc[false][Mips32]=emulFunc32NotDSlot;
      emulFunc[true ][Mips64]=emulFunc64InDSlot;
      emulFunc[false][Mips64]=emulFunc64NotDSlot;
    }
  };
  
#define OpInfo(op,emul) op,emul<op,true,Mips32>,emul<op,false,Mips32>,emul<op,true,Mips64>,emul<op,false,Mips64>
#define CtlNon InstCtlInfo(CtlISize4)
#define CtlExc InstCtlInfo(CtlISize4+CtlExcept)
#define CtlJr  InstCtlInfo(CtlISize4+CtlBr)
#define CtlJmp InstCtlInfo(CtlISize4+CtlBr+CtlFix)
#define CtlJal InstCtlInfo(CtlISize4+CtlBr+CtlFix+CtlCall)
#define CtlJpL InstCtlInfo(CtlISize4+CtlBr+CtlFix+CtlAnull)
#define CtlJLL InstCtlInfo(CtlISize4+CtlBr+CtlFix+CtlCall+CtlAnull)
#define CtlJlr InstCtlInfo(CtlISize4+CtlBr+CtlCall)

  const OpcodeInfo opcodeInfo[NumOfOpcodes]={
    OpcodeInfo(), // OpInvalid
    OpcodeInfo(), // MOpSpecial
    OpcodeInfo(), // MOpRegimm
    OpcodeInfo(OpInfo(OpJ       ,emulJump ), CtlJmp, BrOpJump, DstNo  , SrcNo  , SrcNo, SrcNo, ImmJpTg),
    OpcodeInfo(OpInfo(OpJal     ,emulJump ), CtlJal, BrOpCall, DstRa  , SrcNo  , SrcNo, SrcNo, ImmJpTg),
    OpcodeInfo(OpInfo(OpBeq     ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcRt, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBne     ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcRt, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBlez    ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBgtz    ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpAddi    ,emulAlu  ), CtlExc, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpAddiu   ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSlti    ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSltiu   ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpAndi    ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmZExt),
    OpcodeInfo(OpInfo(OpOri     ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmZExt),
    OpcodeInfo(OpInfo(OpXori    ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcRs  , SrcNo, SrcNo, ImmZExt),
    OpcodeInfo(OpInfo(OpLui     ,emulAlu  ), CtlNon, IntOpALU, DstRt  , SrcNo  , SrcNo, SrcNo, ImmLui ),
    OpcodeInfo(), // MOpCop0
    OpcodeInfo(), // MOpCop1
    OpcodeInfo(), // MOpCop2
    OpcodeInfo(), // MOpCop1x
    OpcodeInfo(OpInfo(OpBeql    ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcRt, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBnel    ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcRt, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBlezl   ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBgtzl   ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(), // OpDaddi
    OpcodeInfo(), // OpDaddiu
    OpcodeInfo(OpInfo(OpLdl     ,emulLdSt ), CtlExc, MemOpLd8, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLdr     ,emulLdSt ), CtlExc, MemOpLd8, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(), // MOpInv1C
    OpcodeInfo(), // MOpInv1D
    OpcodeInfo(), // MOpInv1E
    OpcodeInfo(), // MOpInv1F
    OpcodeInfo(OpInfo(OpLb      ,emulLdSt ), CtlExc, MemOpLd1, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLh      ,emulLdSt ), CtlExc, MemOpLd2, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLwl     ,emulLdSt ), CtlExc, MemOpLd4, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLw      ,emulLdSt ), CtlExc, MemOpLd4, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLbu     ,emulLdSt ), CtlExc, MemOpLd1, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLhu     ,emulLdSt ), CtlExc, MemOpLd2, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLwr     ,emulLdSt ), CtlExc, MemOpLd4, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLwu     ,emulLdSt ), CtlExc, MemOpLd4, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSb      ,emulLdSt ), CtlExc, MemOpSt1, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSh      ,emulLdSt ), CtlExc, MemOpSt2, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSwl     ,emulLdSt ), CtlExc, MemOpSt4, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSw      ,emulLdSt ), CtlExc, MemOpSt4, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSdl     ,emulLdSt ), CtlExc, MemOpSt8, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSdr     ,emulLdSt ), CtlExc, MemOpSt8, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSwr     ,emulLdSt ), CtlExc, MemOpSt4, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(), // MOpInv2F
    OpcodeInfo(OpInfo(OpLl      ,emulLdSt ), CtlExc, MemOpLd4, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpLwc1    ,emulLdSt ), CtlExc, MemOpLd4, DstFt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(), // OpLwc2
    OpcodeInfo(), // OpPref
    OpcodeInfo(), // OpLld
    OpcodeInfo(OpInfo(OpLdc1    ,emulLdSt ), CtlExc, MemOpLd8, DstFt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(), // OpLdc2
    OpcodeInfo(OpInfo(OpLd      ,emulLdSt ), CtlExc, MemOpLd8, DstRt  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSc      ,emulLdSt ), CtlExc, MemOpSt4, DstRt  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpSwc1    ,emulLdSt ), CtlExc, MemOpSt4, DstNo  , SrcRs  , SrcFt, SrcNo, ImmSExt),
    OpcodeInfo(), // OpSwc2
    OpcodeInfo(), // MOpInv3B
    OpcodeInfo(), // OpScd
    OpcodeInfo(OpInfo(OpSdc1    ,emulLdSt ), CtlExc, MemOpSt8, DstNo  , SrcRs  , SrcFt, SrcNo, ImmSExt),
    OpcodeInfo(), // OpSdc2
    OpcodeInfo(OpInfo(OpSd      ,emulLdSt ), CtlExc, MemOpSt8, DstNo  , SrcRs  , SrcRt, SrcNo, ImmSExt),
    // These are the opcodes decoded through the "function" field for MOpSpecial
    OpcodeInfo(OpInfo(OpSll     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRt  , SrcNo, SrcNo, ImmSh),
    OpcodeInfo(), // SOpMovci
    OpcodeInfo(OpInfo(OpSrl     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRt  , SrcNo, SrcNo, ImmSh),
    OpcodeInfo(OpInfo(OpSra     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRt  , SrcNo, SrcNo, ImmSh),
    OpcodeInfo(OpInfo(OpSllv    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRt  , SrcRs, SrcNo, ImmNo),
    OpcodeInfo(), // SOpInv05
    OpcodeInfo(OpInfo(OpSrlv    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRt  , SrcRs, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpSrav    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRt  , SrcRs, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpJr      ,emulJump ), CtlJr , BrOpJump, DstNo  , SrcRs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpJalr    ,emulJump ), CtlJlr, BrOpJump, DstRd  , SrcRs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpMovz    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpMovn    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpSyscall ,emulSyscl), CtlExc, BrOpTrap, DstNo  , SrcNo  , SrcNo, SrcNo, ImmExCd),
    OpcodeInfo(OpInfo(OpBreak   ,emulBreak), CtlExc, BrOpTrap, DstNo  , SrcNo  , SrcNo, SrcNo, ImmExCd),
    OpcodeInfo(), // SOpInv0E
    OpcodeInfo(), // OpSync
    OpcodeInfo(OpInfo(OpMfhi    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcHi  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpMthi    ,emulAlu  ), CtlNon, IntOpALU, DstHi  , SrcRs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpMflo    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcLo  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpMtlo    ,emulAlu  ), CtlNon, IntOpALU, DstLo  , SrcRs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // OpDsllv
    OpcodeInfo(), // SOpInv15
    OpcodeInfo(), // OpDsrlv
    OpcodeInfo(), // OpDsrav
    OpcodeInfo(OpInfo(OpMult    ,emulAlu  ), CtlNon, IntOpMul, DstHiLo, SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpMultu   ,emulAlu  ), CtlNon, IntOpMul, DstHiLo, SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpDiv     ,emulAlu  ), CtlNon, IntOpDiv, DstHiLo, SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpDivu    ,emulAlu  ), CtlNon, IntOpDiv, DstHiLo, SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(), // OpDmult
    OpcodeInfo(), // OpDmultu
    OpcodeInfo(), // OpDdiv
    OpcodeInfo(), // OpDdivu
    OpcodeInfo(OpInfo(OpAdd     ,emulAlu  ), CtlExc, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpAddu    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpSub     ,emulAlu  ), CtlExc, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpSubu    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpAnd     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpOr      ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpXor     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpNor     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(), // SOpInv28
    OpcodeInfo(), // SOpInv29
    OpcodeInfo(OpInfo(OpSlt     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpSltu    ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(), // OpDadd
    OpcodeInfo(), // OpDaddu
    OpcodeInfo(), // OpDsub
    OpcodeInfo(), // OpDsubu
    OpcodeInfo(OpInfo(OpTge     ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcRt, SrcNo, ImmTrCd),
    OpcodeInfo(OpInfo(OpTgeu    ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcRt, SrcNo, ImmTrCd),
    OpcodeInfo(OpInfo(OpTlt     ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcRt, SrcNo, ImmTrCd),
    OpcodeInfo(OpInfo(OpTltu    ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcRt, SrcNo, ImmTrCd),
    OpcodeInfo(OpInfo(OpTeq     ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcRt, SrcNo, ImmTrCd),
    OpcodeInfo(), // SOpInv35
    OpcodeInfo(OpInfo(OpTne     ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcRt, SrcNo, ImmTrCd),
    OpcodeInfo(), // SOpInv37
    OpcodeInfo(), // OpDsll
    OpcodeInfo(), // SOpInv39
    OpcodeInfo(), // OpDsrl
    OpcodeInfo(), // OpDsra
    OpcodeInfo(), // OpDsll32
    OpcodeInfo(), // SOpInv3D
    OpcodeInfo(), // OpDsrl32
    OpcodeInfo(), // OpDsra32
    // These are the opcodes decoded using the "rt" field for MOpRegimm
    OpcodeInfo(OpInfo(OpBltz    ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBgez    ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBltzl   ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBgezl   ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(), // ROpInv04
    OpcodeInfo(), // ROpInv05
    OpcodeInfo(), // ROpInv06
    OpcodeInfo(), // ROpInv07
    OpcodeInfo(OpInfo(OpTgei    ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpTgeiu   ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpTlti    ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpTltiu   ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(OpInfo(OpTeqi    ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(), // ROpInv0D
    OpcodeInfo(OpInfo(OpTnei    ,emulTcnd ), CtlExc, BrOpTrap, DstNo  , SrcRs  , SrcNo, SrcNo, ImmSExt),
    OpcodeInfo(), // ROpInv0F
    OpcodeInfo(OpInfo(OpBltzal  ,emulJump ), CtlJal, BrOpCall, DstRa  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBgezal  ,emulJump ), CtlJal, BrOpCall, DstRa  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBltzall ,emulJump ), CtlJLL, BrOpCall, DstRa  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBgezall ,emulJump ), CtlJLL, BrOpCall, DstRa  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(), // ROpInv14
    OpcodeInfo(), // ROpInv15
    OpcodeInfo(), // ROpInv16
    OpcodeInfo(), // ROpInv17
    OpcodeInfo(), // ROpInv18
    OpcodeInfo(), // ROpInv19
    OpcodeInfo(), // ROpInv1A
    OpcodeInfo(), // ROpInv1B
    OpcodeInfo(), // ROpInv1C
    OpcodeInfo(), // ROpInv1D
    OpcodeInfo(), // ROpInv1E
    OpcodeInfo(), // ROpInv1F
    // These are the opcodes decoded using the "fmt" field for MOpCop1
    OpcodeInfo(OpInfo(OpMfc1    ,emulMvcop), CtlExc, IntOpALU, DstRt  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // OpDmfc1
    OpcodeInfo(OpInfo(OpCfc1    ,emulMvcop), CtlNon, IntOpALU, DstRt  , SrcFCs , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // C1OpInv03
    OpcodeInfo(OpInfo(OpMtc1    ,emulMvcop), CtlExc, IntOpALU, DstFs  , SrcRt  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // OpDmtc1
    OpcodeInfo(OpInfo(OpCtc1    ,emulMvcop), CtlNon, IntOpALU, DstFCs , SrcRt  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // C1OpInv07
    OpcodeInfo(), // C1OpBc
    OpcodeInfo(), // C1OpInv09
    OpcodeInfo(), // C1OpInv0A
    OpcodeInfo(), // C1OpInv0B
    OpcodeInfo(), // C1OpInv0C
    OpcodeInfo(), // C1OpInv0D
    OpcodeInfo(), // C1OpInv0E
    OpcodeInfo(), // C1OpInv0F
    OpcodeInfo(), // C1OpFmts
    OpcodeInfo(), // C1OpFmtd
    OpcodeInfo(), // C1OpInv12
    OpcodeInfo(), // C1OpInv13
    OpcodeInfo(), // C1OpFmtw
    OpcodeInfo(), // C1OpFmtl
    OpcodeInfo(), // C1OpInv16
    OpcodeInfo(), // C1OpInv17
    OpcodeInfo(), // C1OpInv18
    OpcodeInfo(), // C1OpInv19
    OpcodeInfo(), // C1OpInv1A
    OpcodeInfo(), // C1OpInv1B
    OpcodeInfo(), // C1OpInv1C
    OpcodeInfo(), // C1OpInv1D
    OpcodeInfo(), // C1OpInv1E
    OpcodeInfo(), // C1OpInv1F
    // These are the opcodes decoded using the "func" field for MOpCop1x
    OpcodeInfo(OpInfo(OpLwxc1   ,emulLdSt ), CtlExc, MemOpLd4, DstFd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpLdxc1   ,emulLdSt ), CtlExc, MemOpLd8, DstFd  , SrcRs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(),  // CXOpInv02
    OpcodeInfo(),  // CXOpInv03
    OpcodeInfo(),  // CXOpInv04
    OpcodeInfo(),  // CXOpInv05
    OpcodeInfo(),  // CXOpInv06
    OpcodeInfo(),  // CXOpInv07
    OpcodeInfo(OpInfo(OpSwxc1   ,emulLdSt ), CtlExc, MemOpSt4, DstNo  , SrcRs  , SrcRt, SrcFs, ImmNo),
    OpcodeInfo(OpInfo(OpSdxc1   ,emulLdSt ), CtlExc, MemOpSt8, DstNo  , SrcRs  , SrcRt, SrcFs, ImmNo),
    OpcodeInfo(),  // CXOpInv0A
    OpcodeInfo(),  // CXOpInv0B
    OpcodeInfo(),  // CXOpInv0C
    OpcodeInfo(),  // CXOpInv0D
    OpcodeInfo(),  // CXOpInv0E
    OpcodeInfo(),  // OpPrefx
    OpcodeInfo(),  // CXOpInv10
    OpcodeInfo(),  // CXOpInv11
    OpcodeInfo(),  // CXOpInv12
    OpcodeInfo(),  // CXOpInv13
    OpcodeInfo(),  // CXOpInv14
    OpcodeInfo(),  // CXOpInv15
    OpcodeInfo(),  // CXOpInv16
    OpcodeInfo(),  // CXOpInv17
    OpcodeInfo(),  // CXOpInv18
    OpcodeInfo(),  // CXOpInv19
    OpcodeInfo(),  // CXOpInv1A
    OpcodeInfo(),  // CXOpInv1B
    OpcodeInfo(),  // CXOpInv1C
    OpcodeInfo(),  // CXOpInv1D
    OpcodeInfo(),  // CXOpInv1E
    OpcodeInfo(),  // CXOpInv1F
    OpcodeInfo(OpInfo(OpFmadds  ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(OpInfo(OpFmaddd  ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(),  // CXOpInv22
    OpcodeInfo(),  // CXOpInv23
    OpcodeInfo(),  // CXOpInv24
    OpcodeInfo(),  // CXOpInv25
    OpcodeInfo(),  // CXOpInv26
    OpcodeInfo(),  // CXOpInv27
    OpcodeInfo(OpInfo(OpFmsubs  ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(OpInfo(OpFmsubd  ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(),  // CXOpInv2A
    OpcodeInfo(),  // CXOpInv2B
    OpcodeInfo(),  // CXOpInv2C
    OpcodeInfo(),  // CXOpInv2D
    OpcodeInfo(),  // CXOpInv2E
    OpcodeInfo(),  // CXOpInv2F
    OpcodeInfo(OpInfo(OpFnmadds ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(OpInfo(OpFnmaddd ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(),  // CXOpInv32
    OpcodeInfo(),  // CXOpInv33
    OpcodeInfo(),  // CXOpInv34
    OpcodeInfo(),  // CXOpInv35
    OpcodeInfo(),  // CXOpInv36
    OpcodeInfo(),  // CXOpInv37
    OpcodeInfo(OpInfo(OpFnmsubs ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(OpInfo(OpFnmsubd ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcFr, ImmNo),
    OpcodeInfo(),  // CXOpInv3A
    OpcodeInfo(),  // CXOpInv3B
    OpcodeInfo(),  // CXOpInv3C
    OpcodeInfo(),  // CXOpInv3D
    OpcodeInfo(),  // CXOpInv3E
    OpcodeInfo(),  // CXOpInv3F
    // These are the opcodes decoded using the 2-bit "nd/tf" field for MOpSpecial and SOpMovci
    OpcodeInfo(OpInfo(OpMovf    ,emulAlu ), CtlNon, IntOpALU , DstRd  , SrcRs  , SrcFCSR, SrcNo, ImmFbcc),
    OpcodeInfo(OpInfo(OpMovt    ,emulAlu ), CtlNon, IntOpALU , DstRd  , SrcRs  , SrcFCSR, SrcNo, ImmFbcc),
    OpcodeInfo(),  // MVOpInv2
    OpcodeInfo(),  // MVOpInv3
    // These are the opcodes decoded using the 2-bit "nd/tf" field for MOpCop1 and C1OpBC
    OpcodeInfo(OpInfo(OpFbc1f   ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcFCSR, SrcNo, SrcNo, ImmBrOf, ImmFbcc),
    OpcodeInfo(OpInfo(OpFbc1t   ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcFCSR, SrcNo, SrcNo, ImmBrOf, ImmFbcc),
    OpcodeInfo(OpInfo(OpFbc1fl  ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcFCSR, SrcNo, SrcNo, ImmBrOf, ImmFbcc),
    OpcodeInfo(OpInfo(OpFbc1tl  ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcFCSR, SrcNo, SrcNo, ImmBrOf, ImmFbcc),
    // These are the opcodes decoded using the "func" field for MOpCop1 and C1OpFmts
    OpcodeInfo(OpInfo(OpFadds   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFsubs   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFmuls   ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFdivs   ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFsqrts  ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFabss   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFmovs   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFnegs   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFroundls,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFtruncls,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFceills ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFfloorls,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFroundws,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFtruncws,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFceilws ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFfloorws,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // FSOpInv10
    OpcodeInfo(), // FSOpMovcf
    OpcodeInfo(OpInfo(OpFmovzs  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFmovns  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(), // FSOpInv14
    OpcodeInfo(OpInfo(OpFrecips ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFrsqrts ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // FSOpInv17
    OpcodeInfo(), // FSOpInv18
    OpcodeInfo(), // FSOpInv19
    OpcodeInfo(), // FSOpInv1A
    OpcodeInfo(), // FSOpInv1B
    OpcodeInfo(), // FSOpInv1C
    OpcodeInfo(), // FSOpInv1D
    OpcodeInfo(), // FSOpInv1E
    OpcodeInfo(), // FSOpInv1F
    OpcodeInfo(), // FSOpInv20
    OpcodeInfo(OpInfo(OpFcvtds  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(), // FSOpInv22
    OpcodeInfo(), // FSOpInv23
    OpcodeInfo(OpInfo(OpFcvtws  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFcvtls  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(), // FSOpInv26
    OpcodeInfo(), // FSOpInv27
    OpcodeInfo(), // FSOpInv28
    OpcodeInfo(), // FSOpInv29
    OpcodeInfo(), // FSOpInv2A
    OpcodeInfo(), // FSOpInv2B
    OpcodeInfo(), // FSOpInv2C
    OpcodeInfo(), // FSOpInv2D
    OpcodeInfo(), // FSOpInv2E
    OpcodeInfo(), // FSOpInv2F
    OpcodeInfo(OpInfo(OpFcfs    ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcuns   ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFceqs   ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcueqs  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcolts  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcults  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcoles  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcules  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcsfs   ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcngles ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcseqs  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcngls  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFclts   ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcnges  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcles   ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcngts  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    // These are the opcodes decoded using the "nd/tf" field for MOpCop1, C1OpFmts, and FSOpMovcf 
    OpcodeInfo(OpInfo(OpFmovfs  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmFbcc),
    OpcodeInfo(OpInfo(OpFmovts  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmFbcc),
    OpcodeInfo(), // FSMOpInv2,
    OpcodeInfo(), // FSMOpInv3,
    // These are the opcodes decoded using the "func" field for MOpCop1 and C1OpFmtd
    OpcodeInfo(OpInfo(OpFaddd   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFsubd   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFmuld   ,emulFpu  ), CtlExc, FpOpMul , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFdivd   ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcFt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFsqrtd  ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFabsd   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFmovd   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFnegd   ,emulFpu  ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFroundld,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFtruncld,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFceilld ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFfloorld,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFroundwd,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFtruncwd,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFceilwd ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFfloorwd,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // FDOpInv10
    OpcodeInfo(), // FDOpMovcf
    OpcodeInfo(OpInfo(OpFmovzd  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFmovnd  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcRt, SrcNo, ImmNo),
    OpcodeInfo(), // FDOpInv14
    OpcodeInfo(OpInfo(OpFrecipd ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFrsqrtd ,emulFpu  ), CtlExc, FpOpDiv , DstFd  , SrcFs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(), // FDOpInv17
    OpcodeInfo(), // FDOpInv18
    OpcodeInfo(), // FDOpInv19
    OpcodeInfo(), // FDOpInv1A
    OpcodeInfo(), // FDOpInv1B
    OpcodeInfo(), // FDOpInv1C
    OpcodeInfo(), // FDOpInv1D
    OpcodeInfo(), // FDOpInv1E
    OpcodeInfo(), // FDOpInv1F
    OpcodeInfo(OpInfo(OpFcvtsd  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(), // FDOpInv21
    OpcodeInfo(), // FDOpInv22
    OpcodeInfo(), // FDOpInv23
    OpcodeInfo(OpInfo(OpFcvtwd  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFcvtld  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(), // FDOpInv26
    OpcodeInfo(), // FDOpInv27
    OpcodeInfo(), // FDOpInv28
    OpcodeInfo(), // FDOpInv29
    OpcodeInfo(), // FDOpInv2A
    OpcodeInfo(), // FDOpInv2B
    OpcodeInfo(), // FDOpInv2C
    OpcodeInfo(), // FDOpInv2D
    OpcodeInfo(), // FDOpInv2E
    OpcodeInfo(), // FDOpInv2F
    OpcodeInfo(OpInfo(OpFcfd    ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcund   ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFceqd   ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcueqd  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcoltd  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcultd  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcoled  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFculed  ,emulFcond), CtlNon, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcsfd   ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcngled ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcseqd  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcngld  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcltd   ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcnged  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcled   ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    OpcodeInfo(OpInfo(OpFcngtd  ,emulFcond), CtlExc, FpOpALU , DstFCSR, SrcFs  , SrcFt, SrcNo, ImmFccc),
    // These are the opcodes decoded using the "nd/tf" field for MOpCop1, C1OpFmts, and FDOpMovcf 
    OpcodeInfo(OpInfo(OpFmovfd  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmFbcc),
    OpcodeInfo(OpInfo(OpFmovtd  ,emulFpu  ), CtlNon, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmFbcc),
    OpcodeInfo(), // FDMOpInv2
    OpcodeInfo(), // FDMOpInv3
    // These are the opcodes decoded using the "func" field for MOpCop1 and C1OpFmtw
    OpcodeInfo(), // FWOpInv00
    OpcodeInfo(), // FWOpInv01
    OpcodeInfo(), // FWOpInv02
    OpcodeInfo(), // FWOpInv03
    OpcodeInfo(), // FWOpInv04
    OpcodeInfo(), // FWOpInv05
    OpcodeInfo(), // FWOpInv06
    OpcodeInfo(), // FWOpInv07
    OpcodeInfo(), // FWOpInv08
    OpcodeInfo(), // FWOpInv09
    OpcodeInfo(), // FWOpInv0A
    OpcodeInfo(), // FWOpInv0B
    OpcodeInfo(), // FWOpInv0C
    OpcodeInfo(), // FWOpInv0D
    OpcodeInfo(), // FWOpInv0E
    OpcodeInfo(), // FWOpInv0F
    OpcodeInfo(), // FWOpInv10
    OpcodeInfo(), // FWOpInv11
    OpcodeInfo(), // FWOpInv12
    OpcodeInfo(), // FWOpInv13
    OpcodeInfo(), // FWOpInv14
    OpcodeInfo(), // FWOpInv15
    OpcodeInfo(), // FWOpInv16
    OpcodeInfo(), // FWOpInv17
    OpcodeInfo(), // FWOpInv18
    OpcodeInfo(), // FWOpInv19
    OpcodeInfo(), // FWOpInv1A
    OpcodeInfo(), // FWOpInv1B
    OpcodeInfo(), // FWOpInv1C
    OpcodeInfo(), // FWOpInv1D
    OpcodeInfo(), // FWOpInv1E
    OpcodeInfo(), // FWOpInv1F
    OpcodeInfo(OpInfo(OpFcvtsw  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFcvtdw  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(), // FWOpInv22
    OpcodeInfo(), // FWOpInv23
    OpcodeInfo(), // FWOpInv24
    OpcodeInfo(), // FWOpInv25
    OpcodeInfo(), // FWOpInv26
    OpcodeInfo(), // FWOpInv27
    OpcodeInfo(), // FWOpInv28
    OpcodeInfo(), // FWOpInv29
    OpcodeInfo(), // FWOpInv2A
    OpcodeInfo(), // FWOpInv2B
    OpcodeInfo(), // FWOpInv2C
    OpcodeInfo(), // FWOpInv2D
    OpcodeInfo(), // FWOpInv2E
    OpcodeInfo(), // FWOpInv2F
    OpcodeInfo(), // FWOpInv30
    OpcodeInfo(), // FWOpInv31
    OpcodeInfo(), // FWOpInv32
    OpcodeInfo(), // FWOpInv33
    OpcodeInfo(), // FWOpInv34
    OpcodeInfo(), // FWOpInv35
    OpcodeInfo(), // FWOpInv36
    OpcodeInfo(), // FWOpInv37
    OpcodeInfo(), // FWOpInv38
    OpcodeInfo(), // FWOpInv39
    OpcodeInfo(), // FWOpInv3A
    OpcodeInfo(), // FWOpInv3B
    OpcodeInfo(), // FWOpInv3C
    OpcodeInfo(), // FWOpInv3D
    OpcodeInfo(), // FWOpInv3E
    OpcodeInfo(), // FWOpInv3F
    // These are the opcodes decoded using the "func" field for MOpCop1 and C1OpFmtl
    OpcodeInfo(), // FLOpInv00
    OpcodeInfo(), // FLOpInv01
    OpcodeInfo(), // FLOpInv02
    OpcodeInfo(), // FLOpInv03
    OpcodeInfo(), // FLOpInv04
    OpcodeInfo(), // FLOpInv05
    OpcodeInfo(), // FLOpInv06
    OpcodeInfo(), // FLOpInv07
    OpcodeInfo(), // FLOpInv08
    OpcodeInfo(), // FLOpInv09
    OpcodeInfo(), // FLOpInv0A
    OpcodeInfo(), // FLOpInv0B
    OpcodeInfo(), // FLOpInv0C
    OpcodeInfo(), // FLOpInv0D
    OpcodeInfo(), // FLOpInv0E
    OpcodeInfo(), // FLOpInv0F
    OpcodeInfo(), // FLOpInv10
    OpcodeInfo(), // FLOpInv11
    OpcodeInfo(), // FLOpInv12
    OpcodeInfo(), // FLOpInv13
    OpcodeInfo(), // FLOpInv14
    OpcodeInfo(), // FLOpInv15
    OpcodeInfo(), // FLOpInv16
    OpcodeInfo(), // FLOpInv17
    OpcodeInfo(), // FLOpInv18
    OpcodeInfo(), // FLOpInv19
    OpcodeInfo(), // FLOpInv1A
    OpcodeInfo(), // FLOpInv1B
    OpcodeInfo(), // FLOpInv1C
    OpcodeInfo(), // FLOpInv1D
    OpcodeInfo(), // FLOpInv1E
    OpcodeInfo(), // FLOpInv1F
    OpcodeInfo(OpInfo(OpFcvtsl  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpFcvtdl  ,emulFcvt ), CtlExc, FpOpALU , DstFd  , SrcFs  , SrcFCSR, SrcNo, ImmNo),
    OpcodeInfo(), // FLOpInv22
    OpcodeInfo(), // FLOpInv23
    OpcodeInfo(), // FLOpInv24
    OpcodeInfo(), // FLOpInv25
    OpcodeInfo(), // FLOpInv26
    OpcodeInfo(), // FLOpInv27
    OpcodeInfo(), // FLOpInv28
    OpcodeInfo(), // FLOpInv29
    OpcodeInfo(), // FLOpInv2A
    OpcodeInfo(), // FLOpInv2B
    OpcodeInfo(), // FLOpInv2C
    OpcodeInfo(), // FLOpInv2D
    OpcodeInfo(), // FLOpInv2E
    OpcodeInfo(), // FLOpInv2F
    OpcodeInfo(), // FLOpInv30
    OpcodeInfo(), // FLOpInv31
    OpcodeInfo(), // FLOpInv32
    OpcodeInfo(), // FLOpInv33
    OpcodeInfo(), // FLOpInv34
    OpcodeInfo(), // FLOpInv35
    OpcodeInfo(), // FLOpInv36
    OpcodeInfo(), // FLOpInv37
    OpcodeInfo(), // FLOpInv38
    OpcodeInfo(), // FLOpInv39
    OpcodeInfo(), // FLOpInv3A
    OpcodeInfo(), // FLOpInv3B
    OpcodeInfo(), // FLOpInv3C
    OpcodeInfo(), // FLOpInv3D
    OpcodeInfo(), // FLOpInv3E
    OpcodeInfo(), // FLOpInv3F
    // Derived opcodes (special cases of real opcodes)
    OpcodeInfo(OpInfo(OpNop     ,emulNop  ), CtlNon, TypNop  , DstNo  , SrcNo  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpB       ,emulJump ), CtlJmp, BrOpJump, DstNo  , SrcNo  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBal     ,emulJump ), CtlJal, BrOpCall, DstRa  , SrcNo  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBeqz    ,emulJump ), CtlJmp, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBeqzl   ,emulJump ), CtlJpL, BrOpCond, DstNo  , SrcRs  , SrcNo, SrcNo, ImmBrOf),
    OpcodeInfo(OpInfo(OpBnez    ,emulJump )), // Decoded via OpBne, not using this table
    OpcodeInfo(OpInfo(OpBnezl   ,emulJump )), // Decoded via OpBnel, not using this table
    OpcodeInfo(OpInfo(OpMov     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcRs  , SrcNo, SrcNo, ImmNo),
    OpcodeInfo(OpInfo(OpLi      ,emulAlu  )), // Decoded via OpAddiu or OpOri, not using this table
    OpcodeInfo(OpInfo(OpClr     ,emulAlu  ), CtlNon, IntOpALU, DstRd  , SrcNo  , SrcNo, SrcNo, ImmZero),
    OpcodeInfo(OpInfo(OpNot     ,emulAlu  )), // Decoded via OpNor, not using this table
    OpcodeInfo(OpInfo(OpNeg     ,emulAlu  )), // Decoded via OpSub, not using this table
    OpcodeInfo(OpInfo(OpNegu    ,emulAlu  )), // Decoded via OpSubu, not using this table
  };
  
  Opcode decodeOp(RawInst rawInst){
    Opcode op=(Opcode)(OpBase+rawInst.op);
    switch(op){
    case MOpSpecial:
      op=(Opcode)(SOpBase+rawInst.func);
      switch(op){
      case SOpMovci:
	op=(Opcode)(MVOpBase+rawInst.ndtf); break;
      default:
	break;
      }
      break;
    case MOpRegimm:
      op=(Opcode)(ROpBase+rawInst.rt);   break;
    case MOpCop1:
      op=(Opcode)(C1OpBase+rawInst.fmt);
      switch(op){
      case C1OpBc:
	op=(Opcode)(FBOpBase+rawInst.ndtf); break;
      case C1OpFmts:
	op=(Opcode)(FSOpBase+rawInst.func);
	switch(op){
	case FSOpMovcf:
	  op=(Opcode)(FSMOpBase+rawInst.ndtf); break;
	default:
	  break;
	}
	break;
      case C1OpFmtd:
	op=(Opcode)(FDOpBase+rawInst.func);
	switch(op){
	case FDOpMovcf:
	  op=(Opcode)(FDMOpBase+rawInst.ndtf); break;
	default:
	  break;
	}
	break;
      case C1OpFmtw:
	op=(Opcode)(FWOpBase+rawInst.func); break;
      case C1OpFmtl:
	op=(Opcode)(FLOpBase+rawInst.func); break;
      default:
	break;
      }
      break;
    case MOpCop1x:
      op=(Opcode)(CXOpBase+rawInst.func); break;
    default: 
      break;
    }
    return op;
  }

  void decodeInst(InstDesc *inst, ThreadContext *context){
    // Read the raw MIPS instruction
    I(context->virt2inst(inst->addr)==inst);
    RawInst rawInst;
    context->readMem(inst->addr,rawInst.whole);
    // Remeber if this instruction was classified as a branch before this decoding
    bool wasBranch=(inst->ctlInfo&CtlBr);
    // Determine the Opcode for this instruction
    Opcode op=decodeOp(rawInst);
    while(true){
      if(op==OpInvalid){
	fail("Mips::decodeInst: unsupported instruction at 0x%08x\n",inst->addr);
      }else if(opcodeInfo[op].op!=op){
	fail("Mips::decodeInst: opcodeInfo table error for instruction at 0x%08x\n",inst->addr);
      }
      // Load the instruction-related part of ctlInfo from the table
      inst->ctlInfo=(InstCtlInfo)(inst->ctlInfo&CtlLMask);
      I(!(opcodeInfo[op].ctl&CtlLMask));
      inst->ctlInfo=(InstCtlInfo)(inst->ctlInfo|opcodeInfo[op].ctl);
      inst->typInfo=opcodeInfo[op].typ;
      switch(opcodeInfo[op].dst){
      case DstNo:   inst->regDst=RegNone; break;
      case DstRd:   inst->regDst=(RegName)(rawInst.rd); break;
      case DstFd:   inst->regDst=(RegName)(rawInst.fd+FprNameLb); break;
      case DstRt:   inst->regDst=(RegName)(rawInst.rt); break;
      case DstFt:   inst->regDst=(RegName)(rawInst.ft+FprNameLb); break;
      case DstFs:   inst->regDst=(RegName)(rawInst.fs+FprNameLb); break;
      case DstFCs:  inst->regDst=(RegName)(rawInst.fs+FcrNameLb); break;
      case DstFCSR: inst->regDst=static_cast<RegName>(RegFCSR); break;
      case DstRa:   inst->regDst=static_cast<RegName>(RegRA); break;
      case DstHi:   inst->regDst=static_cast<RegName>(RegHi); break;
      case DstLo:   inst->regDst=static_cast<RegName>(RegLo); break;
      case DstHiLo: inst->regDst=static_cast<RegName>(RegHL); break;
      default: fail("mipsDecode: invalid regDst for instruction at 0x%08x\n",inst->addr);
      }
      if(static_cast<MipsRegName>(inst->regDst)==RegZero){
	// Change destination register to RegJunk to avoid polluting RegZero
	inst->regDst=static_cast<RegName>(RegJunk);
      }
      switch(opcodeInfo[op].src1){
      case SrcNo:  inst->regSrc1=RegNone; break;
      case SrcRt:  inst->regSrc1=(RegName)(rawInst.rt); break;
      case SrcRs:  inst->regSrc1=(RegName)(rawInst.rs); break;
      case SrcFs:  inst->regSrc1=(RegName)(rawInst.fs+FprNameLb); break;
      case SrcFCs: inst->regSrc1=(RegName)(rawInst.fs+FcrNameLb); break;
      case SrcFCSR:inst->regSrc1=static_cast<RegName>(RegFCSR); break;
      case SrcHi:  inst->regSrc1=static_cast<RegName>(RegHi); break;
      case SrcLo:  inst->regSrc1=static_cast<RegName>(RegLo); break;
      default: fail("mipsDecode: invalid regSrc1 for instruction at 0x%08x\n",inst->addr);
      }
      switch(opcodeInfo[op].src2){
      case SrcNo:   inst->regSrc2=RegNone; break;
      case SrcRs:   inst->regSrc2=(RegName)(rawInst.rs); break;
      case SrcRt:   inst->regSrc2=(RegName)(rawInst.rt); break;
      case SrcFt:   inst->regSrc2=(RegName)(rawInst.ft+FprNameLb); break;
      case SrcFs:   inst->regSrc2=(RegName)(rawInst.fs+FprNameLb); break;
      case SrcFCSR: inst->regSrc2=static_cast<RegName>(RegFCSR); break;
      default: fail("mipsDecode: invalid regSrc2 for instruction at 0x%08x\n",inst->addr);
      }
      switch(opcodeInfo[op].src3){
      case SrcNo: inst->regSrc3=RegNone; break;
      case SrcFr: inst->regSrc3=(RegName)(rawInst.fr+FprNameLb); break;
      case SrcFs: inst->regSrc3=(RegName)(rawInst.fs+FprNameLb); break;
      default: fail("mipsDecode: invalid regSrc3 for instruction at 0x%08x\n",inst->addr);
      }
      switch(opcodeInfo[op].imm1){
      case ImmNo:   inst->imm1.i==0; break;
      case ImmJpTg:
      case ImmBrOf:
	{
	  VAddr nextIp=inst->addr+sizeof(rawInst);
	  VAddr targ;
	  if(opcodeInfo[op].imm1==ImmJpTg){
	    // The target address takes lowemost 28 (actually 26, shifted by 2) bits from the instruction 
	    // and takes the remaining (high) bits from the instruction pointer (which is pre-incremented)
	    targ=(nextIp&(~0x0fffffff))|(rawInst.targ*sizeof(rawInst));
	  }else{
	    I(opcodeInfo[op].imm1==ImmBrOf);
	    // The target address is the 18-bit sign-extended offset (actually 16 bits shifted by 2) 
	    // added to the (pre-incremented) instruction pointer
	    targ=nextIp+(rawInst.simmed*sizeof(rawInst));
	  }
	  // TODO: This instruction must be re-decoded if the target InstDesc moves after this
	  inst->imm1.i=context->virt2inst(targ);
	  I(inst->imm1.i->addr==targ);
	} break;
      case ImmSExt: inst->imm1.s=rawInst.simmed; break;
      case ImmZExt: inst->imm1.u=rawInst.uimmed; break;
      case ImmLui:  inst->imm1.s=(rawInst.simmed<<16); break;
      case ImmZero: inst->imm1.s=0; break;
      case ImmSh:   inst->imm1.u=rawInst.sa; break;
      case ImmExCd: inst->imm1.u=rawInst.excode; break;
      case ImmTrCd: inst->imm1.u=rawInst.trcode; break;
      case ImmFbcc: inst->imm1.u=(1<<(23+rawInst.fbcc+(rawInst.fbcc>0))); break;
      case ImmFccc: inst->imm1.u=(1<<(23+rawInst.fccc+(rawInst.fccc>0))); break;
      default: fail("Mips::DecodeInst: invalid imm1 for instruction at 0x%08x\n",inst->addr);
      }
      switch(opcodeInfo[op].imm2){
      case ImmNo:   inst->imm2.u=0; break;
      case ImmFbcc: inst->imm2.u=(1<<(23+rawInst.fbcc+(rawInst.fbcc>0))); break;
      default: fail("Mips::DecodeInst: invalid imm2 for instruction at 0x%08x\n",inst->addr);
      }
      // Now handle special cases
      // Only OpNop should have no control flow or memory and no destination register
      I((op==OpNop)||(inst->ctlInfo&CtlIMask)||((inst->typInfo&TypOpMask)==TypMemOp)||(inst->regDst!=RegNone));
      // Now see if there is *actually* no effect (write to RegJunk with no side effects)
      if(((inst->ctlInfo&CtlIMask)==0)&&((inst->typInfo&TypOpMask)!=TypMemOp)&&
	 (static_cast<MipsRegName>(inst->regDst)==RegJunk)){
	op=OpNop;
      }else if((op==OpBgezal)&&(static_cast<MipsRegName>(inst->regSrc1)==RegZero)){
	op=OpBal;
      }else if((op==OpBeq)&&(static_cast<MipsRegName>(inst->regSrc2)==RegZero)){
	if(static_cast<MipsRegName>(inst->regSrc1)==RegZero){
	  op=OpB;
	}else{
	  op=OpBeqz;
	}
      }else if((op==OpBeql)&&(static_cast<MipsRegName>(inst->regSrc2)==RegZero)){
	op=OpBeqzl;
      }else if((op==OpBne)&&
	       ((static_cast<MipsRegName>(inst->regSrc1)==RegZero)||
		(static_cast<MipsRegName>(inst->regSrc2)==RegZero))){
	op=OpBnez;
	if(static_cast<MipsRegName>(inst->regSrc1)==RegZero)
	  inst->regSrc1=inst->regSrc2;
	inst->regSrc2=RegNone;
	break;
      }else if((op==OpBnel)&&
	       ((static_cast<MipsRegName>(inst->regSrc1)==RegZero)||
		(static_cast<MipsRegName>(inst->regSrc2)==RegZero))){
	op=OpBnezl;
	if(static_cast<MipsRegName>(inst->regSrc1)==RegZero)
	  inst->regSrc1=inst->regSrc2;
	inst->regSrc2=RegNone;
	break;
      }else if((op==OpAddu)&&(static_cast<MipsRegName>(inst->regSrc2)==RegZero)){
	if(static_cast<MipsRegName>(inst->regSrc1)==RegZero){
	  op=OpClr;
	}else{
 	  op=OpMov;
	}
      }else if(((op==OpAddiu)||(op==OpOri))&&(static_cast<MipsRegName>(inst->regSrc1)==RegZero)){
	op=OpLi;
	inst->regSrc1=RegNone;
	break;
      }else if((op==OpNor)&&
	       ((static_cast<MipsRegName>(inst->regSrc1)==RegZero)||
		(static_cast<MipsRegName>(inst->regSrc2)==RegZero))){
	op=OpNot;
	if(static_cast<MipsRegName>(inst->regSrc1)==RegZero)
	  inst->regSrc1=inst->regSrc2;
	inst->regSrc2=RegNone;
	break;
      }else if((op==OpSub)&&(static_cast<MipsRegName>(inst->regSrc1)==RegZero)){
	op=OpNeg;
	inst->regSrc1=inst->regSrc2;
	inst->regSrc2=RegNone;
	break;
      }else if((op==OpSubu)&&(static_cast<MipsRegName>(inst->regSrc1)==RegZero)){
	op=OpNegu;
	inst->regSrc1=inst->regSrc2;
	inst->regSrc2=RegNone;
	break;
      }else if((op==OpJr)&&(static_cast<MipsRegName>(inst->regSrc1)==RegRA)){
	inst->typInfo=BrOpRet;
	break;
      }else if((op==OpJalr)&&(static_cast<MipsRegName>(inst->regDst)==RegRA)){
	inst->typInfo=BrOpCall;
	break;
      }else{
	// The opcode is fully decoded now (no more special cases)
	break;
      }
    }
    // Function to emulate this opcode
    inst->emulFunc=opcodeInfo[op].emulFunc[(bool)(inst->ctlInfo&CtlInDSlot)][context->getMode()];
    // Info for debugging
    I((rawInst.whole!=0)||(op==OpNop));
#if (defined DEBUG)
    inst->op=(Opcode)op;
#endif
    VAddr nextAddr=inst->addr+(inst->ctlInfo&CtlISize);
    I(nextAddr==inst->addr+4);
    inst->nextInst=context->virt2inst(nextAddr);
    I(inst->nextInst->addr==inst->addr+4);
    bool isBranch=(inst->ctlInfo&CtlBr);
    if(isBranch!=wasBranch){
      // This instruction changed between non-branch and branch
      // The next instruction (delay slot) must be re-decoded
      I(wasBranch==(bool)(inst->nextInst->ctlInfo&CtlInDSlot));
      inst->nextInst->ctlInfo=(InstCtlInfo)(inst->nextInst->ctlInfo^CtlInDSlot);
      inst->nextInst->emulFunc=instDecode;
    }
    // Decode the delay slot for a branch
    if(isBranch){
      decodeInst(inst->nextInst,context);
    }
  }
}; // namespace Mips

void instDecode(InstDesc *inst, ThreadContext *context){
  //  I(inst->addr!=0x4603d4);
  switch(context->getMode()){
  case Mips32: case Mips64:
    Mips::decodeInst(inst,context);
    break;
  default:
    fail("instDecode: Unknown CPU mode\n");
    break;
  }
  inst->emulFunc(inst,context);
}

static inline RegType getSescRegType(RegName reg, bool src){
  if(src){
    if((reg==RegNone)||(static_cast<Mips::MipsRegName>(reg)==Mips::RegZero))
      return NoDependence;
  }else{
    if((reg==RegNone)||(static_cast<Mips::MipsRegName>(reg)==Mips::RegJunk))
      return InvalidOutput;
  }
  if(isGprName(reg))
    return static_cast<RegType>(reg&RegNumMask);
  if(isFprName(reg))
    return static_cast<RegType>(IntFPBoundary+(reg&RegNumMask));
  if(getRegType(reg)==RegTypeCtl)
    return CoprocStatReg;
  if((static_cast<Mips::MipsRegName>(reg)==Mips::RegHi)||(static_cast<Mips::MipsRegName>(reg)==Mips::RegLo))
    return HiReg;
  I(0);
  return NoDependence;
}

// Create a SESC Instruction for this static instruction
Instruction *createSescInst(const InstDesc *inst){
  Instruction *sescInst=new Instruction();
  sescInst->addr=inst->addr;
  InstType    iType    =iOpInvalid;
  InstSubType iSubType =iSubInvalid;
  MemDataSize iDataSize=0;
  switch(inst->typInfo&TypOpMask){
  case TypNop:
    sescInst->opcode=iALU;
    sescInst->subCode=iNop;
    break;
  case TypIntOp:
    switch(inst->typInfo&TypSubMask){
    case IntOpALU:
      sescInst->opcode=iALU;
      break;
    case IntOpMul:
      sescInst->opcode=iMult;
      break;
    case IntOpDiv:
      sescInst->opcode=iDiv;
      break;
    default:
      fail("createSescInst: Unknown subtype for TypIntOp\n");
    }
    sescInst->subCode=iSubInvalid;
    break;
  case TypFpOp:
    switch(inst->typInfo&TypSubMask){
    case FpOpALU:
      sescInst->opcode=fpALU;
      break;
    case FpOpMul:
      sescInst->opcode=fpMult;
      break;
    case FpOpDiv:
      sescInst->opcode=fpDiv;
      break;
    default:
      fail("createSescInst: Unknown subtype for TypFpOp\n");
    }
    sescInst->subCode=iSubInvalid;    
    break;
  case TypBrOp:
    sescInst->opcode=iBJ;
    switch(inst->typInfo&TypSubMask){
    case BrOpJump:
      sescInst->subCode=BJUncond;
      break;
    case BrOpCond:
      sescInst->subCode=BJCond;
      break;
    case BrOpCall:
      sescInst->subCode=BJCall;
      break;
    case BrOpRet:
      sescInst->subCode=BJRet;
      break;
    case BrOpTrap:
      sescInst->opcode=iALU;
      sescInst->subCode=iNop;
      break;
    default:
      fail("createSescInst: Unknown subtype for TypBrOp\n");
    }
    break;
  case TypMemOp:
    switch(inst->typInfo&TypSubMask){
    case TypMemLd:
      sescInst->opcode=iLoad;
      break;
    case TypMemSt:
      sescInst->opcode=iStore;
      break;
    default:
      fail("createSescInst: Unknown subtype for TypMemOp\n");
    }
    sescInst->dataSize=(inst->typInfo&MemSizeMask);
    sescInst->subCode=iMemory;
    break;
  default:
    fail("createSescInst: Unknown instruction type\n");
  }
  sescInst->skipDelay=inst->nextInst->addr-inst->addr;
  sescInst->uEvent=NoEvent;
  sescInst->guessTaken=false;
  sescInst->condLikely=false;
  sescInst->jumpLabel =false;
  if(inst->ctlInfo&CtlBr){
    if(inst->ctlInfo&CtlFix){
      sescInst->jumpLabel=true;
      if(inst->imm1.i->addr<inst->addr)
	sescInst->guessTaken=true;
    }
    if(inst->ctlInfo&CtlAnull){
      sescInst->condLikely=true;
      sescInst->guessTaken=true;
    }
    if((inst->typInfo&TypSubMask)!=BrOpCond)
      sescInst->guessTaken=true;
  }
  sescInst->src1=getSescRegType(inst->regSrc1,true);
  sescInst->src2=getSescRegType(inst->regSrc2,true);
  sescInst->dest=getSescRegType(inst->regDst,false);
  sescInst->src1Pool=Instruction::whichPool(sescInst->src1);
  sescInst->src2Pool=Instruction::whichPool(sescInst->src2);
  sescInst->dstPool =Instruction::whichPool(sescInst->dest);
  return sescInst;
}
