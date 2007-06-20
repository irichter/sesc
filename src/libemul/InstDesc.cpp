// We need to emulate all sorts of floating point operations
#include <math.h>
// We need to control the rounding mode for floating point operations
#include <fenv.h>
// We use functionals in our implementation
#include <functional>

#include "InstDesc.h"
#include "ThreadContext.h"
#include "MipsSysCalls.h"
#include "MipsRegs.h"
// To get definition of fail()
#include "EmulInit.h"
// This is the SESC Instruction class to let us create a DInst
#include "Instruction.h"
// To get getContext(pid), needed to implement LL/SC
// (Remove this when ThreadContext stays in one place on thread switching)
#include "OSSim.h"

namespace fns{
  template <class _Tp> struct project1st_identity    : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return __x; } };
  template <class _Tp> struct project1st_bitwise_not : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return ~__x; } };
  template <class _Tp> struct project1st_neg         : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return -__x; } };
  template <class _Tp> struct project1st_abs         : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return (__x>=0)?__x:-__x; } };
  template <class _Tp> struct project1st_sqrt        : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return sqrt(__x); } };
  template <class _Tp> struct project1st_recip       : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return _Tp(1)/__x; } };
  template <class _Tp> struct project1st_sqrt_recip  : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return _Tp(1)/sqrt(__x); } };
  template <class _Tp> struct shift_left             : public std::binary_function<_Tp, uint32_t, _Tp>{ inline _Tp operator()(const _Tp& __x, const uint32_t& __y) const{ return (__x<<__y); } };
  template <class _Tp>
  struct shift_right : public std::binary_function<_Tp, uint32_t, _Tp>{
    inline _Tp operator()(const _Tp& __x, const uint32_t& __y) const{ return (__x>>__y); }
  };
  template <class _Tp> struct bitwise_and            : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return (__x&__y); } };
  template <class _Tp> struct bitwise_or             : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return (__x|__y); } };
  template <class _Tp> struct bitwise_xor            : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return (__x^__y); } };
  template <class _Tp> struct bitwise_nor            : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return ~(__x|__y); } };
  template <class _Tp> struct plus_neg               : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return -(__x + __y); } };
  template <class _Tp> struct minus_neg              : public std::binary_function<_Tp, _Tp, _Tp>{ inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ return -(__x - __y); } };
  template <class _Tp> struct multiplies_hi          : public std::binary_function<_Tp, _Tp, _Tp>{
    inline _Tp operator()(const _Tp& __x, const _Tp& __y) const{ fail("%s called, unsupported type\n",__PRETTY_FUNCTION__); return 0; }
  };
   template <> struct multiplies_hi<int32_t>          : public std::binary_function<int32_t,int32_t,int32_t>{
    inline int32_t operator()(const int32_t& __x, const int32_t& __y) const{
      int64_t res=int64_t(__x)*int64_t(__y);
      int32_t *ptr=(int32_t *)(&res);
      return *(ptr+(__BYTE_ORDER!=__BIG_ENDIAN));
    }
  };
  template <> struct multiplies_hi<uint32_t>          : public std::binary_function<uint32_t,uint32_t,uint32_t>{
    inline uint32_t operator()(const uint32_t& __x, const uint32_t& __y) const{
      uint64_t res=uint64_t(__x)*uint64_t(__y);
      uint32_t *ptr=(uint32_t *)(&res);
      return *(ptr+(__BYTE_ORDER!=__BIG_ENDIAN));
    }
  };
  template<class _Tp, bool ExcUn, bool UseLt, bool UseEq, bool UseUn>
  struct float_compare : public std::binary_function<_Tp,_Tp,bool>{
    inline bool operator()(const _Tp& __x, const _Tp& __y) const{
      return ((UseLt&&(__x<__y))||(UseEq&&(__x==__y))||(UseUn&&isunordered(__x,__y)));
    }
  };
  template<class _Ts, class _Td, int rm>
  struct project1st_round : public std::binary_function<_Ts,_Ts,_Td>{
    inline _Td operator()(const _Ts& __x, const _Ts& __y) const{
      int saverm=fegetround();
      fesetround(rm);
      _Td retVal=_Td(__x);
      fesetround(saverm);
      return retVal;
    }
  };
  template<class _Tp> struct tt : public std::binary_function<_Tp,_Tp,bool>{ inline bool operator()(const _Tp& __x, const _Tp& __y) const{ return true; } };
  template<class _Tp> struct eq : public std::equal_to<_Tp>{};
  template<class _Tp> struct ne : public std::not_equal_to<_Tp>{};
  template<class _Tp> struct lt : public std::less<_Tp>{};
  template<class _Tp> struct gt : public std::greater<_Tp>{};
  template<class _Tp> struct le : public std::less_equal<_Tp>{};
  template<class _Tp> struct ge : public std::greater_equal<_Tp>{};
}

InstDesc::~InstDesc(void){
  if(sescInst)
    delete sescInst;
  ID(sescInst=0);
}

namespace Mips {

  typedef uint32_t RawInst;

  typedef enum{
    //This is an invalid opcode
    OpInvalid,
    // Branches and jumps
    OpOB, OpOJ, OpOJal, OpOBal, OpOJr, OpORet, OpOJalr, OpOCallr,
    OpB,  OpJ,  OpJal,  OpBal,  OpJr,  OpRet,  OpJalr,  OpCallr,  OpCallrS,
    OpB_, OpJ_, OpJal_, OpBal_, OpJr_, OpRet_, OpJalr_, OpCallr_,
    OpOBeq,  OpOBne,  OpOBlez,  OpOBgtz,  OpOBltz,  OpOBgez,
    OpBeq,   OpBne,   OpBlez,   OpBgtz,   OpBltz,   OpBgez,
    OpBeq_,  OpBne_,  OpBlez_,  OpBgtz_,  OpBltz_,  OpBgez_,
    OpOBeql, OpOBnel, OpOBlezl, OpOBgtzl, OpOBltzl, OpOBgezl,
    OpBeql,  OpBnel,  OpBlezl,  OpBgtzl,  OpBltzl,  OpBgezl,
    OpBeql_, OpBnel_, OpBlezl_, OpBgtzl_, OpBltzl_, OpBgezl_,
    OpOBltzal, OpOBgezal, OpOBltzall, OpOBgezall,
    OpBltzal,  OpBgezal,  OpBltzall,  OpBgezall,
    OpBltzalS, OpBgezalS, OpBltzallS, OpBgezallS,
    OpBltzal_, OpBgezal_, OpBltzall_, OpBgezall_,
    OpOBc1f,  OpOBc1t,  OpOBc1fl,  OpOBc1tl,
    OpBc1f,  OpBc1t,  OpBc1fl,  OpBc1tl,
    OpBc1f_, OpBc1t_, OpBc1fl_, OpBc1tl_,
    // Syscalls and traps
    OpSyscall, OpBreak,
    // Conditional traps
    OpTge,  OpTgeu,  OpTlt,  OpTltu,  OpTeq,  OpTne,
    OpTgei, OpTgeiu, OpTlti, OpTltiu, OpTeqi, OpTnei,

    // GPR load/store
    OpLb, OpLh, OpLw, OpLd, OpLwl, OpLwr, OpLbu, OpLhu, OpLwu,
    OpSb, OpSh, OpSw, OpSd, OpSwl, OpSwr,
    // Coprocessor load/store
    OpLwc1,  OpLwc2, OpLdc1, OpLdc2,
    OpSwc1,  OpSwc2, OpSdc1, OpSdc2,
    OpLwxc1, OpLdxc1,
    // These are two-part ops: calc addr, then store
    OpSwxc1,  OpSdxc1,
    OpSwxc1_, OpSdxc1_,
    // Load-linked and store-conditional sync ops
    OpLl, OpSc,
    // These are not implemented yet
    OpLdl, OpLdr, OpLld, OpScd,
    OpSdl, OpSdr,
    OpPref, OpPrefx, OpSync,

    // Constant transfers
    OpLui, OpLi, OpLiu,
    // Shifts
    OpSll,    OpSrl,    OpSra,    OpSllv,  OpSrlv,  OpSrav,
    OpDsll,   OpDsrl,   OpDsra,   OpDsllv, OpDsrlv, OpDsrav,
    OpDsll32, OpDsrl32, OpDsra32,
    // Logical
    OpAnd, OpOr, OpXor, OpNor, OpAndi, OpOri, OpXori, OpNot,
    // Arithmetic
    OpAdd, OpAddu, OpSub, OpSubu, OpAddi, OpAddiu, OpNeg, OpNegu,
    OpMult, OpMultu, OpDiv, OpDivu, OpMult_, OpMultu_, OpDiv_, OpDivu_,
    OpDadd,OpDaddu,OpDsub,OpDsubu,OpDaddi,OpDaddiu,OpDmult,OpDmultu,OpDdiv,OpDdivu,
    // Moves
    OpMovw, OpFmovs, OpFmovd,
    OpMfhi, OpMflo, OpMfc1, OpDmfc1, OpCfc1,
    OpMthi, OpMtlo, OpMtc1, OpDmtc1, OpCtc1,
    // Integer comparisons
    OpSlt, OpSltu, OpSlti, OpSltiu,
    // Floating-point comparisons
    OpFcfs,    OpFcuns,   OpFceqs,   OpFcueqs,  OpFcolts,  OpFcults,  OpFcoles, OpFcules,
    OpFcsfs,   OpFcngles, OpFcseqs,  OpFcngls,  OpFclts,   OpFcnges,  OpFcles,  OpFcngts,
    OpFcfd,    OpFcund,   OpFceqd,   OpFcueqd,  OpFcoltd,  OpFcultd,  OpFcoled, OpFculed,
    OpFcsfd,   OpFcngled, OpFcseqd,  OpFcngld,  OpFcltd,   OpFcnged,  OpFcled,  OpFcngtd,
    // Conditional moves
    OpMovz,   OpMovn,   OpMovf,   OpMovt,
    OpFmovzs, OpFmovns, OpFmovfs, OpFmovts,
    OpFmovzd, OpFmovnd, OpFmovfd, OpFmovtd,
    // Floating-point arithmetic
    OpFadds, OpFsubs, OpFmuls, OpFdivs, OpFsqrts, OpFabss, OpFnegs, OpFrecips, OpFrsqrts,
    OpFaddd, OpFsubd, OpFmuld, OpFdivd, OpFsqrtd, OpFabsd, OpFnegd, OpFrecipd, OpFrsqrtd,
    // These are two-part ops: multiply, then add/subtract
    OpFmadds, OpFmsubs, OpFnmadds, OpFnmsubs, OpFmaddd, OpFmsubd, OpFnmaddd, OpFnmsubd,
    OpFmadds_,OpFmsubs_,OpFnmadds_,OpFnmsubs_,OpFmaddd_,OpFmsubd_,OpFnmaddd_,OpFnmsubd_,
    // Floating-point format conversion
    OpFroundls,OpFtruncls,OpFceills,OpFfloorls,OpFroundws,OpFtruncws,OpFceilws,OpFfloorws,
    OpFroundld,OpFtruncld,OpFceilld,OpFfloorld,OpFroundwd,OpFtruncwd,OpFceilwd,OpFfloorwd,
    OpFcvtls,OpFcvtws,OpFcvtld,OpFcvtwd,OpFcvtds,OpFcvtsd,OpFcvtsw,OpFcvtdw,OpFcvtsl,OpFcvtdl,

    OpNop, // Do nothing
    OpCut, // Special opcode for end-of-trace situations (acts as nop or completely omitted)

    NumOfOpcodes,
  } Opcode;
  
  typedef std::set<Pid_t> PidSet;
  PidSet linkset;

  static inline void preExec(InstDesc *inst, ThreadContext *context){
#if (defined DEBUG_BENCH)
    context->execInst(inst->addr,getRegAny<mode,uint32_t,RegTypeGpr>(context,static_cast<RegName>(RegSP)));
#endif
  }

  InstDesc *emulCut(InstDesc *inst, ThreadContext *context){
    context->setIAddr(context->getIAddr());
    I(context->getIDesc()!=inst);
    return (*(context->getIDesc()))(context);
  }

  template<Opcode op, CpuMode mode>
  InstDesc *emulNop(InstDesc *inst, ThreadContext *context){
    context->updIAddr(inst->aupdate,1);
    return (*(inst+1))(context);
  }

  typedef enum{ DestNone, DestTarg, DestRetA, DestCond } DestTyp;

  typedef enum{
    NextCont, // Continue to the next InstDesc (uop) within the same architectural instruction
    NextInst, // Continue to the next InstDesc (uop) and update the PC (move to the next architectural instruction)
    NextNext, // Continue to the next InstDesc (uop), check if PC update is needed
    NextBReg, // Branch to a register address (if cond) otherwise skip DS
    NextBImm, // Branch to a constant address (if cond) otherwise skip DS
    NextAnDS  // Skip DS if cond, otherwise continue to next InstDesc
  } NextTyp;

#define SrcZ RegName(RegTypeDyn+1)
#define SrcI RegName(RegTypeDyn+2)

  template<Opcode op, CpuMode mode, NextTyp NTyp, RegName Src1R, RegName Src2R, RegName DstR, typename Func>
  InstDesc *emulAlu(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    typedef typename Func::first_argument_type  Src1T;
    typedef typename Func::second_argument_type Src2T; 
    typedef typename Func::result_type          DstT;
    // Get source operands
    Src1T src1=(Src1R==SrcZ)?0:(Src1R==SrcI)?Src1T(inst->imm):getRegAny<mode,Src1T,Src1R>(context,inst->regSrc1);
    Src2T src2=(Src2R==SrcZ)?0:(Src2R==SrcI)?Src2T(inst->imm):getRegAny<mode,Src2T,Src2R>(context,inst->regSrc2);
    // Perform function to get result
    Func func;
    DstT dst=func(src1,src2);
    // Update destination register
    setRegAny<mode,DstT,DstR>(context,inst->regDst,dst);
    if((NTyp==NextCont)||((NTyp==NextNext)&&!inst->aupdate)){
      if(NTyp==NextNext)
	inst->emul=emulAlu<op,mode,NextCont,Src1R,Src2R,DstR,Func>;
      context->updIDesc(1);
    }else{
      I((NTyp==NextInst)||((NTyp==NextNext)&&inst->aupdate));
      if(NTyp==NextNext)
	inst->emul=emulAlu<op,mode,NextInst,Src1R,Src2R,DstR,Func>;
      context->updIAddr(inst->aupdate,1);
      if(inst->aupdate)
	handleSignals(context);
    }
    return inst;
  }

  template<Opcode op, CpuMode mode, typename AddrT, RegName Cnd1R, RegName Cnd2R, typename CFunc, DestTyp DTyp, NextTyp NTyp>
  InstDesc *emulJump(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    I(inst->addr==context->getIAddr());
    preExec(inst,context);
    typedef typename CFunc::first_argument_type  Cnd1T;
    typedef typename CFunc::second_argument_type Cnd2T; 
    // Evaluate the branch/jump condition
    Cnd1T cnd1=(Cnd1R==SrcZ)?0:(Cnd1R==SrcI)?Cnd1T(inst->imm):getRegAny<mode,Cnd1T,Cnd1R>(context,inst->regSrc1);
    Cnd2T cnd2=(Cnd2R==SrcZ)?0:(Cnd2R==SrcI)?Cnd2T(inst->imm):getRegAny<mode,Cnd2T,Cnd2R>(context,inst->regSrc2);
    CFunc func;
    bool cond=func(cnd1,cnd2);
    // Set the destination register (if any)
    if(DTyp==DestTarg)
      setRegAny<mode,AddrT,RegTypeGpr>(context,inst->regDst,getRegAny<mode,AddrT,RegTypeGpr>(context,inst->regSrc1));
    else if(DTyp==DestRetA)
      setRegAny<mode,AddrT,RegTypeGpr>(context,inst->regDst,context->getIAddr()+2*sizeof(RawInst));
    else if(DTyp==DestCond)
      setRegAny<mode,bool,RegTypeSpc>(context,inst->regDst,cond?1:0);
    // Update the PC and next instruction
    if(NTyp==NextBReg){
      context->setIAddr(getRegAny<mode,AddrT,RegTypeGpr>(context,inst->regSrc1));
    }else if(NTyp==NextCont){
      context->updIDesc(1);
    }else if(NTyp==NextBImm){
      if(cond){
	context->setIAddr(AddrT(inst->imm));
      }else{
        context->updIAddr(inst->aupdate,inst->iupdate);
      }
    }else if(NTyp==NextAnDS){
      context->updIDesc(cond?1:inst->iupdate);
    }
    if(inst->aupdate)
      handleSignals(context);
    return inst;
  }

  template<Opcode op, CpuMode mode, typename SrcT, typename Cmp, bool src2reg>
  InstDesc *emulTcnd(InstDesc *inst, ThreadContext *context){
    I(inst->op==op);
    preExec(inst,context);
    SrcT src1=getRegAny<mode,SrcT,RegTypeGpr>(context,inst->regSrc1);
    SrcT src2=src2reg?getRegAny<mode,SrcT,RegTypeGpr>(context,inst->regSrc2):SrcT(inst->imm);
    Cmp cmp;
    bool cond=cmp(src1,src2);
    if(cond)
      fail("emulTcnd: trap caused at 0x%08x, not supported yet\n",context->getIAddr());
    context->updIAddr(inst->aupdate,1);
    if(inst->aupdate)
      handleSignals(context);
    return inst;
  }
  
  template<Opcode op, CpuMode mode, typename RegSigT, typename RegUnsT>
  inline RegUnsT calcAddr(const InstDesc *inst, const ThreadContext *context){
    switch(op){
    case OpLb:    case OpLbu: case OpSb:
    case OpLh:    case OpLhu: case OpSh:
    case OpLw:    case OpLwu: case OpSw:
    case OpLwl:   case OpLwr: case OpSwl: case OpSwr:
    case OpLl:    case OpSc:
    case OpLd:    case OpSd:
    case OpLdl:   case OpLdr: case OpSdl: case OpSdr:
    case OpLwc1:  case OpSwc1:
    case OpLdc1:  case OpSdc1:
      I(((MipsRegName)(inst->regSrc1)>=GprNameLb)&&((MipsRegName)(inst->regSrc1)<GprNameUb));
      return static_cast<RegUnsT>(getRegAny<mode,RegSigT,RegTypeGpr>(context,inst->regSrc1)+static_cast<RegSigT>(inst->imm.s));
    case OpLwxc1: case OpLdxc1: 
    case OpSwxc1: case OpSdxc1:
      I(((MipsRegName)(inst->regSrc1)>=GprNameLb)&&((MipsRegName)(inst->regSrc1)<GprNameUb));
      I(((MipsRegName)(inst->regSrc2)>=GprNameLb)&&((MipsRegName)(inst->regSrc2)<GprNameUb));
      return static_cast<RegUnsT>(getRegAny<mode,RegSigT,RegTypeGpr>(context,inst->regSrc1)+getRegAny<mode,RegSigT,RegTypeGpr>(context,inst->regSrc2));
    case OpSwxc1_: case OpSdxc1_:
      I(inst->regSrc1==static_cast<RegName>(RegTmp));
      return getRegAny<mode,RegUnsT,RegTypeGpr>(context,inst->regSrc1);
//    default:
//      fail("calcAddr: unsupported opcode\n");
    }
//    return 0;
  }
  template<CpuMode mode, typename RegUnsT>
  void growStack(ThreadContext *context){
    RegUnsT sp=getRegAny<mode,RegUnsT,RegTypeGpr>(context,static_cast<RegName>(RegSP));
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
  template<CpuMode mode, typename T, typename AddrT>
  inline T readMem(ThreadContext *context, AddrT addr){
    if(!context->canRead(addr,sizeof(T))){
      growStack<mode,AddrT>(context);
      if(!context->canRead(addr,sizeof(T)))
	fail("readMem: segmentation fault\n");
    }
    return context->readMem<T>(addr);
  }
  template<CpuMode mode, typename T, typename RegUnsT>
  inline bool readMem(ThreadContext *context, RegUnsT addr, T &val){
    if(!context->readMem(addr,val)){
      growStack<mode,RegUnsT>(context);
      if(!context->readMem(addr,val))
	fail("readMem: segmentation fault\n");
    }
    return true;
  }
  template<CpuMode mode, typename T, typename RegUnsT>
  inline bool writeMem(ThreadContext *context, RegUnsT addr, const T &val){
    if(!context->writeMem(addr,val)){
      growStack<mode,RegUnsT>(context);
      if(!context->writeMem(addr,val))
	fail("writeMem: segmentation fault\n");
    }
    if(!linkset.empty()){
      PidSet::iterator pidIt=linkset.begin();
      while(pidIt!=linkset.end()){
	if((*pidIt==context->getPid())||
	   (getRegAny<mode,RegUnsT,RegTypeSpc>(osSim->getContext(*pidIt),static_cast<RegName>(RegLink))==(addr-(addr&0x7)))){
	  setRegAny<mode,RegUnsT,RegTypeSpc>(osSim->getContext(*pidIt),static_cast<RegName>(RegLink),0);
	  linkset.erase(pidIt);
	  pidIt=linkset.begin();
	}else{
	  pidIt++;
	}
      }
    }
    return true;
  }

  template<Opcode op, CpuMode mode, typename OffsT, typename AddrT, typename RegT, typename MemT, RegName RTyp>
  inline InstDesc *emulLd(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    AddrT addr=calcAddr<op,mode,OffsT,AddrT>(inst,context);
    context->setDAddr(addr);
    if(op==OpLl){
      setRegAny<mode,AddrT,RegTypeSpc>(context,static_cast<RegName>(RegLink),addr-(addr&0x7));
      linkset.insert(context->getPid());
    }
    MemT  val=readMem<mode,MemT,AddrT>(context,addr);
    setRegAny<mode,RegT,RTyp>(context,inst->regDst,static_cast<RegT>(val));
    I((RTyp!=RegTypeFpr)||(static_cast<RegT>(val)==getRegFp<mode,RegT>(context,inst->regDst)));
    context->updIAddr(inst->aupdate,1);
    if(inst->aupdate)
      handleSignals(context);
    return inst;
  }
  template<Opcode op, CpuMode mode, typename OffsT, typename AddrT, typename RegT, typename MemT, RegName RTyp>
  inline InstDesc *emulSt(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    AddrT addr=calcAddr<op,mode,OffsT,AddrT>(inst,context);
    context->setDAddr(addr);
    RegT val=getRegAny<mode,RegT,RTyp>(context,inst->regSrc2);
    I((RTyp!=RegTypeFpr)||(val==getRegFp<mode,RegT>(context,inst->regSrc2)));
    if(op==OpSwl){
      cvtEndianBig(val);
      context->writeMemFromBuf(addr,4-(addr&0x3),&val);
    }else if(op==OpSwr){
      cvtEndianBig(val);
      context->writeMemFromBuf(addr-(addr&0x3),(addr&0x3)+1,((uint8_t *)(&val))+3-(addr&0x3));
    }else{
      writeMem<mode,MemT,AddrT>(context,addr,val);
    }
    context->updIAddr(inst->aupdate,1);
    if(inst->aupdate)
      handleSignals(context);
    return inst;
  }
  template<Opcode op, CpuMode mode, typename RegSigT, typename RegUnsT>
  InstDesc *emulLdSt(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    // Compute the effective (virtual) address
    RegUnsT vaddr=calcAddr<op,mode,RegSigT,RegUnsT>(inst,context);
    //    I(vaddr!=0x10008edc);
    context->setDAddr(vaddr);
    switch(op){
    case OpLwl: case OpLwr: {
      size_t   offs=(vaddr%sizeof(int32_t));
      int32_t mval=readMem<mode,int32_t,VAddr>(context,vaddr-offs);
      int32_t rval=getRegAny<mode,int32_t,RegTypeGpr>(context,inst->regDst);
      if(op==OpLwl){
        rval&=~(-1<<(8*offs)); rval|=(mval<<(8*offs));
      }else{
        rval&=(-256<<(8*offs)); rval|=((mval>>(8*(3-offs)))&(~(-256<<(8*offs))));
      }

      //#if (defined DEBUG)
      int32_t val=getRegAny<mode,int32_t,RegTypeGpr>(context,inst->regDst);
      cvtEndianBig(val);
      switch(op){
      case OpLwl: context->readMemToBuf(vaddr,4-(vaddr&0x3),&val); break;
      case OpLwr: context->readMemToBuf(vaddr-(vaddr&0x3),(vaddr&0x3)+1,((uint8_t *)(&val))+3-(vaddr&0x3)); break;
      }
      cvtEndianBig(val);
      if(static_cast<int32_t>(rval)!=val)
        fail("OpLwl(0)/OpLwr(1) %d old-new mismatch at 0x%08x for addr 0x%08x rval 0x%08x mval 0x%08x ores 0x%08x nres 0x%08x\n",
             (op==OpLwl)?0:1,context->getIAddr(),vaddr,getRegAny<mode,uint32_t,RegTypeGpr>(context,inst->regDst),mval,val,rval);
      //#endif

      setRegAny<mode,RegSigT,RegTypeGpr>(context,inst->regDst,static_cast<RegSigT>(rval));
    } break;
    case OpSc:
       // If link does not match, regDst becomes zero and there is no write 
       if(getRegAny<mode,RegUnsT,RegTypeSpc>(context,static_cast<RegName>(RegLink))!=(vaddr-(vaddr&0x7))){
         setRegAny<mode,RegSigT,RegTypeGpr>(context,inst->regDst,0);
#if (defined DEBUG_BENCH)
	printf("OpSc fails inst 0x%08x addr 0x%08lx in %d\n",inst->addr,(long unsigned int)vaddr,context->getPid());
#endif
      }else{
	writeMem<mode,uint32_t,VAddr>(context,vaddr,getRegAny<mode,uint32_t,RegTypeGpr>(context,inst->regSrc2));
	setRegAny<mode,RegSigT,RegTypeGpr>(context,inst->regDst,1);
      }
      break;
//    case OpSwl: {
//      int32_t val=getReg<mode,int32_t>(context,inst->regSrc2);
//      cvtEndianBig(val);
//      context->writeMemFromBuf(vaddr,4-(vaddr&0x3),&val);
//    } break;
//    case OpSwr: {
//      int32_t val=getReg<mode,int32_t>(context,inst->regSrc2);
//      cvtEndianBig(val);
//      context->writeMemFromBuf(vaddr-(vaddr&0x3),(vaddr&0x3)+1,((uint8_t *)(&val))+3-(vaddr&0x3));
//    } break;
    }
    context->updIAddr(inst->aupdate,1);
    if(inst->aupdate)
      handleSignals(context);
    return inst;
  }

  template<class _Ts, class _Tm, class _Td>
  struct mips_float_convert : public std::binary_function<_Ts,_Tm,_Td>{
    inline _Td operator()(const _Ts& __x, const _Tm& __y) const{
      static int Mips32_RoundMode[] = {
        FE_TONEAREST,  /* 00 nearest   */
        FE_TOWARDZERO, /* 01 zero      */
        FE_UPWARD,     /* 10 plus inf  */
        FE_DOWNWARD    /* 11 minus inf */
      };
      int rm=Mips32_RoundMode[__y&3];
      int saverm=fegetround();
      fesetround(rm);
      _Td retVal=_Td(__x);
      fesetround(saverm);
      return retVal;
    }
  };

  template<Opcode op, CpuMode mode, typename CndT, typename RegT, RegName RTyp>
  InstDesc *emulMovc(InstDesc *inst, ThreadContext *context){
    I(op==inst->op);
    preExec(inst,context);
    bool cond;
    switch(op){
    case OpMovf: case OpFmovfs: case OpFmovfd:
      I(inst->imm.u&0xFE800000); I(static_cast<MipsRegName>(inst->regSrc2)==RegFCSR);
      cond=((getRegAny<mode,CndT,RegTypeCtl>(context,static_cast<RegName>(RegFCSR))&CndT(inst->imm))==0);
      break;
    case OpMovt: case OpFmovts: case OpFmovtd:
      I(inst->imm.u&0xFE800000); I(static_cast<MipsRegName>(inst->regSrc2)==RegFCSR);
      cond=((getRegAny<mode,CndT,RegTypeCtl>(context,static_cast<RegName>(RegFCSR))&CndT(inst->imm))!=0);
      break;
    case OpMovz: case OpFmovzs: case OpFmovzd:
      I(isGprName(inst->regSrc2));
      cond=(getRegAny<mode,CndT,RegTypeGpr>(context,inst->regSrc2)==0);
      break;
    case OpMovn: case OpFmovns: case OpFmovnd:
      I(isGprName(inst->regSrc2));
      cond=(getRegAny<mode,CndT,RegTypeGpr>(context,inst->regSrc2)!=0);
      break;
    }
    if(cond)
      setRegAny<mode,RegT,RTyp>(context,inst->regDst,getRegAny<mode,RegT,RTyp>(context,inst->regSrc1));
    context->updIAddr(inst->aupdate,1);
    if(inst->aupdate)
      handleSignals(context);
    return inst;
  }

  template<Opcode op, CpuMode mode>
  InstDesc *emulBreak(InstDesc *inst, ThreadContext *context){
    fail("emulBreak: BREAK instruction not supported yet\n");
    return inst;
  }

  template<Opcode op, CpuMode mode>
  InstDesc *emulSyscl(InstDesc *inst, ThreadContext *context){
    // Note the ordering of IP update and mipsSysCall, which allows
    // mipsSysCall to change control flow or simply let it continue
    return mipsSysCall(inst,context);
  }

enum InstCtlInfoEnum{
  CtlInv  = 0x0000,   // No instruction should have CtlInfo of zero (should at least have length)

  CtlNorm = 0x0001, // Regular (non-branch) instruction
  CtlBran = 0x0002, // Branch instruction
  CtlLkly = 0x0004, // ISA indicates that branch/jump is likely
  CtlTarg = 0x0008, // Branch/jump has a fixed target
  CtlAdDS = 0x0010, // Decode the delay slot after this but don't map it
  CtlMpDS = 0x0020,   // Can map the delay slot after this (decode only if mapping needed)

  CtlBr   = CtlBran + CtlMpDS,
  CtlBrLk = CtlBran + CtlMpDS + CtlLkly,
  CtlBrTg = CtlBran + CtlMpDS + CtlTarg,
  CtlBrTL = CtlBran + CtlMpDS + CtlLkly + CtlTarg,
  CtlPrBr = CtlNorm + CtlAdDS
};               
typedef InstCtlInfoEnum InstCtlInfo;

static inline RegType getSescRegType(RegName reg, bool src){
  if(src){
    if((reg==RegNone)||(static_cast<Mips::MipsRegName>(reg)==Mips::RegZero))
      return NoDependence;
  }else{
    if((reg==RegNone)||(static_cast<Mips::MipsRegName>(reg)==Mips::RegJunk))
      return InvalidOutput;
  }
  if(isGprName(reg))
    return static_cast<RegType>(getRegNum(reg));
  if(isFprName(reg))
    return static_cast<RegType>(IntFPBoundary+getRegNum(reg));
  if(getRegType(reg)==RegTypeCtl)
    return CoprocStatReg;
  if((static_cast<Mips::MipsRegName>(reg)==Mips::RegHi)||(static_cast<Mips::MipsRegName>(reg)==Mips::RegLo))
    return HiReg;
  if(static_cast<Mips::MipsRegName>(reg)==Mips::RegCond)
    return CondReg;
  I(0);
  return NoDependence;
}

// Create a SESC Instruction for this static instruction
Instruction *createSescInst(const InstDesc *inst, VAddr iaddr, size_t deltaAddr, InstTypInfo typ, Mips::InstCtlInfo ctl){
  Instruction *sescInst=new Instruction();
  sescInst->addr=iaddr;
  InstType    iType    =iOpInvalid;
  InstSubType iSubType =iSubInvalid;
  MemDataSize iDataSize=0;
  switch(typ&TypOpMask){
  case TypNop:
    sescInst->opcode=iALU;
    sescInst->subCode=iNop;
    break;
  case TypIntOp:
    switch(typ&TypSubMask){
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
    switch(typ&TypSubMask){
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
    switch(typ&TypSubMask){
    case BrOpJump:
      sescInst->subCode=BJUncond;
      break;
    case BrOpCond:
      sescInst->subCode=BJCond;
      break;
    case BrOpCall: case BrOpCCall:
      sescInst->subCode=BJCall;
      break;
    case BrOpRet: case BrOpCRet:
      sescInst->subCode=BJRet;
      break;
    case BrOpTrap: case BrOpCTrap:
      sescInst->opcode=iALU;
      sescInst->subCode=iNop;
      break;
    default:
      fail("createSescInst: Unknown subtype for TypBrOp\n");
    }
    break;
  case TypMemOp:
    switch(typ&TypSubMask){
    case TypMemLd: case TypSynLd:
      sescInst->opcode=iLoad;
      break;
    case TypMemSt: case TypSynSt:
      sescInst->opcode=iStore;
      break;
    default:
      fail("createSescInst: Unknown subtype for TypMemOp\n");
    }
    sescInst->dataSize=(typ&MemSizeMask);
    sescInst->subCode=iMemory;
    break;
  default:
    fail("createSescInst: Unknown instruction type\n");
  }
  sescInst->skipDelay=deltaAddr;
  sescInst->uEvent=NoEvent;
  sescInst->guessTaken=false;
  sescInst->condLikely=false;
  sescInst->jumpLabel =false;
  if(sescInst->opcode==iBJ){
    if(ctl&Mips::CtlTarg){
      sescInst->jumpLabel=true;
      if(inst->imm.a<iaddr)
	sescInst->guessTaken=true;
    }
    if(ctl&Mips::CtlLkly){
      sescInst->condLikely=true;
      sescInst->guessTaken=true;
    }
    if(sescInst->subCode!=BJCond)
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

  // What is the encoding for the instruction's register argument
  enum InstArgInfo  { ArgNo, ArgRd, ArgRt, ArgRs, ArgFd, ArgFt, ArgFs, ArgFr,
                      ArgFCs, ArgFCSR, ArgFccc, ArgFbcc,
                      ArgTmp, ArgBTmp, ArgFTmp, ArgCond, ArgRa, ArgHi, ArgLo, ArgHL, ArgZero };
  // What is the format of the immediate (if any)
  enum InstImmInfo  { ImmNo, ImmJpTg, ImmBrOf, ImmSExt, ImmZExt, ImmLui, ImmSh, ImmSh32, ImmExCd, ImmTrCd, ImmFccc, ImmFbcc };

  class DecodeInst{
    class OpKey{
      RawInst mask;
      RawInst val;
    public:
      OpKey(RawInst mask, RawInst val)
	: mask(mask), val(val){
      }
      bool operator<(const OpKey &other) const{
	if(mask==other.mask)
	  return val<other.val;
	return mask<other.mask;
      }
    };
    class OpEntry{
    public:
      RawInst mask;
      Opcode     op;
      OpEntry(void){
      }
      OpEntry(RawInst mask, Opcode op=OpInvalid)
	: mask(mask), op(op){
      }
      OpEntry(Opcode op)
	: mask(0), op(op){
      }
    };
    typedef std::map<OpKey,OpEntry> OpMap;
    OpMap opMap;

    class OpData{
    public:
      EmulFunc   *emul[2];
      InstCtlInfo ctl;
      InstTypInfo typ;
      InstArgInfo dst;
      InstArgInfo src1;
      InstArgInfo src2;
      InstImmInfo imm;
      Opcode      next;
      OpData(void){
      }
      OpData(EmulFunc emul32, EmulFunc emul64,
	     InstCtlInfo ctl, InstTypInfo typ,
	     InstArgInfo dst, InstArgInfo src1, InstArgInfo src2,
	     InstImmInfo imm, Opcode next=OpInvalid)
	: ctl(ctl), typ(typ), dst(dst), src1(src1), src2(src2), imm(imm), next(next)
      {
	emul[0]=emul32;
	emul[1]=emul64;
      }
      bool addDSlot(void) const{
	return (ctl&CtlAdDS);
      }
      bool mapDSlot(void) const{
	return (ctl&CtlMpDS);
      }
    };
    typedef std::map<Opcode,OpData> Op2Data;
    Op2Data op2Data;

    typedef std::map<Opcode,Opcode> Op2Op;
    Op2Op brNoDSlot;

  public:
    DecodeInst(void);
    Opcode decodeOp(RawInst raw){
      OpKey   curKey(0,0);
      OpEntry curEntry(0);
      OpMap::const_iterator it;
      while((it=opMap.find(curKey))!=opMap.end()){
	curEntry=it->second;
	if(!curEntry.mask)
	  break;
	curKey=OpKey(curEntry.mask,raw&curEntry.mask);
      }
      return curEntry.op;
    }
    static RegName decodeArg(InstArgInfo arg, RawInst inst){
      switch(arg){
      case ArgNo:   return RegNone;
      case ArgRd:   return static_cast<RegName>(((inst>>11)&0x1F)+GprNameLb);
      case ArgRt:   return static_cast<RegName>(((inst>>16)&0x1F)+GprNameLb);
      case ArgRs:   return static_cast<RegName>(((inst>>21)&0x1F)+GprNameLb);
      case ArgTmp:  return static_cast<RegName>(RegTmp);
      case ArgBTmp: return static_cast<RegName>(RegBTmp);
      case ArgFd:   return static_cast<RegName>(((inst>> 6)&0x1F)+FprNameLb);
      case ArgFt:   return static_cast<RegName>(((inst>>16)&0x1F)+FprNameLb);
      case ArgFs:   return static_cast<RegName>(((inst>>11)&0x1F)+FprNameLb);
      case ArgFr:   return static_cast<RegName>(((inst>>21)&0x1F)+FprNameLb);
      case ArgFCs:  return static_cast<RegName>(((inst>>11)&0x1F)+FcrNameLb);
      case ArgFTmp: return static_cast<RegName>(RegFTmp);
      case ArgFCSR: return static_cast<RegName>(RegFCSR);
      case ArgFccc: return static_cast<RegName>(FccNameLb+((inst>>8)&0x7));
      case ArgFbcc: return static_cast<RegName>(FccNameLb+((inst>>18)&0x7));
      case ArgRa:   return static_cast<RegName>(RegRA);
      case ArgHi:   return static_cast<RegName>(RegHi);
      case ArgLo:   return static_cast<RegName>(RegLo);
      case ArgHL:   return static_cast<RegName>(RegHL);
      case ArgCond: return static_cast<RegName>(RegCond);
      default:
        fail("decodeArg called for invalid register arg %d in raw inst 0x%08x\n",arg,inst);
      }
      return RegNone;
    }
    static InstImm decodeImm(InstImmInfo imm, RawInst inst, VAddr addr){
      switch(imm){
      case ImmNo:   return 0;
      case ImmJpTg: return static_cast<VAddr>(((addr+sizeof(inst))&(~0x0fffffff))|((inst&0x0fffffff)*sizeof(inst)));
      case ImmBrOf: return static_cast<VAddr>((addr+sizeof(inst))+((static_cast<int16_t>(inst&0xffff))*sizeof(inst)));
      case ImmSExt: return static_cast<int32_t>(static_cast<int16_t>(inst&0xffff));
      case ImmZExt: return static_cast<uint32_t>(static_cast<uint16_t>(inst&0xffff));
      case ImmLui:  return static_cast<int32_t>((inst<<16)&0xffff0000);
      case ImmSh:   return static_cast<uint32_t>((inst>>6)&0x1F);
      case ImmSh32: return static_cast<uint32_t>(32+((inst>>6)&0x1F));
      case ImmExCd: return static_cast<uint32_t>((inst>>6)&0xFFFFF);
      case ImmTrCd: return static_cast<uint32_t>((inst>>6)&0x3FF);
      case ImmFbcc: { unsigned int cc=((inst>>18)&0x7); return static_cast<uint32_t>(1<<(23+cc+(cc>0))); }
      case ImmFccc: { unsigned int cc=((inst>>8)&0x7); return static_cast<uint32_t>(1<<(23+cc+(cc>0))); }
      default:
        fail("decodeImm called for invalid imm %d in raw inst 0x%08x\n",imm,inst);
      }
      return 0;
    }
    bool isNop(ThreadContext *context, VAddr addr){
      RawInst raw=context->readMem<RawInst>(addr);
      Opcode op=decodeOp(raw);
      if((op==OpInvalid)||(op>=NumOfOpcodes))
	fail("Invalid op while decoding raw inst 0x%08x at 0x%08x\n",raw,addr);
      const OpData &data=op2Data[op];
      return (data.typ==TypNop)&&(data.next==OpInvalid);
    }
    void decodeInstSize(ThreadContext *context, VAddr funcAddr, VAddr &curAddr, VAddr endAddr, size_t &tsize, bool domap){
      // Function entry point may need to call a handler
      if((curAddr==funcAddr)&&context->getAddressSpace()->hasCallHandler(funcAddr)){
        I(domap);
        AddressSpace::HandlerSet hset;
        context->getAddressSpace()->getCallHandlers(funcAddr,hset);
        tsize+=hset.size();
      }
      I(!context->getAddressSpace()->isMappedInst(curAddr));
      RawInst raw=context->readMem<RawInst>(curAddr);
      curAddr+=sizeof(RawInst);
      Opcode op=decodeOp(raw);
      if((op==OpInvalid)||(op>=NumOfOpcodes))
	fail("Invalid op while decoding raw inst 0x%08x at 0x%08x\n",raw,curAddr-sizeof(RawInst));
      // If nop in delay slot and optimized no-delay-slot decoding exists, use that decoding
      if((brNoDSlot.find(op)!=brNoDSlot.end())&&isNop(context,curAddr))
	op=brNoDSlot[op];
      while(true){
	if(!op2Data.count(op))
	  fail("No decoding available for raw inst 0x%08x at 0x%08x\n",raw,curAddr-sizeof(RawInst));
	const OpData &data=op2Data[op];
	InstTypInfo typ=static_cast<InstTypInfo>(data.typ&TypSubMask);
        // Function return may need to call a handler
        if((typ==BrOpRet)&&context->getAddressSpace()->hasRetHandler(funcAddr)){
          I(domap);
          AddressSpace::HandlerSet hset;
          context->getAddressSpace()->getRetHandlers(funcAddr,hset);
          tsize+=hset.size();
        }
	tsize++;
	// Decode delay slots
	if(data.addDSlot()){
	  I(domap);
	  VAddr dsaddr=curAddr;
	  decodeInstSize(context,funcAddr,dsaddr,endAddr,tsize,false);
	}else if(data.mapDSlot()){
	  I(domap);
	  decodeInstSize(context,funcAddr,curAddr,endAddr,tsize,true);
	}
	op=data.next;
	if(op==OpInvalid){
	  // Is this the end of this trace?
	  if(domap&&(curAddr==endAddr)){
	    // Unconditional jumps, calls, and returns don't continue directly to next instruction
	    // Everything else needs an OpCut to link to the continuation in another trace
	    if((typ!=BrOpJump)&&(typ!=BrOpCall)&&(typ!=BrOpRet))
	      tsize++;
	  }
	  // Exit the decoding loop
	  break;
	}
      }
    }
    void decodeInst(ThreadContext *context, VAddr funcAddr, VAddr &curAddr, VAddr endAddr, InstDesc *&trace, bool domap){
      I(!context->getAddressSpace()->isMappedInst(curAddr));
      if(domap)
	context->getAddressSpace()->mapInst(curAddr,trace);
      // Add function entry handlers if this is a function entry point
      if((curAddr==funcAddr)&&context->getAddressSpace()->hasCallHandler(funcAddr)){
        I(domap);
        AddressSpace::HandlerSet hset;
        context->getAddressSpace()->getCallHandlers(funcAddr,hset);
        while(!hset.empty()){
          AddressSpace::HandlerSet::iterator it=hset.begin();
          trace->emul=*it;
          trace++;
          hset.erase(it);
        }
      }
      VAddr origiaddr=curAddr;
      RawInst raw=context->readMem<RawInst>(curAddr);
      curAddr+=sizeof(RawInst);
      Opcode op=decodeOp(raw);
      I(op!=OpInvalid);
      // If nop in delay slot and optimized no-delay-slot decoding exists, use that decoding
      if((brNoDSlot.find(op)!=brNoDSlot.end())&&isNop(context,curAddr))
	op=brNoDSlot[op];
      while(true){
	I(op2Data.count(op));
	const OpData &data=op2Data[op];
	InstTypInfo typ=static_cast<InstTypInfo>(data.typ&TypSubMask);
        // Function return may need to call a handler
        if((typ==BrOpRet)&&context->getAddressSpace()->hasRetHandler(funcAddr)){
          I(domap);
          AddressSpace::HandlerSet hset;
          context->getAddressSpace()->getRetHandlers(funcAddr,hset);
          while(!hset.empty()){
            AddressSpace::HandlerSet::iterator it=hset.begin();
            trace->emul=*it;
            trace++;
            hset.erase(it);
          }
        }
	InstDesc *myinst=trace++;
	switch(context->getMode()){
	case Mips32: myinst->emul=data.emul[0]; break;
	case Mips64: myinst->emul=data.emul[1]; break;
	default: fail("Mips::decodeInst used in non-MIPS CPU mode\n");
	}
#if (defined DEBUG)
	myinst->op=op;
	if(domap)
	  myinst->addr=origiaddr;
	else
	  myinst->addr=origiaddr-sizeof(RawInst);
	myinst->typ=data.typ;
#endif
	myinst->regDst=decodeArg(data.dst,raw);
	if(myinst->regDst==static_cast<RegName>(RegZero))
	  myinst->regDst=static_cast<RegName>(RegJunk);
	myinst->regSrc1=decodeArg(data.src1,raw);
	myinst->regSrc2=decodeArg(data.src2,raw);
	myinst->imm=decodeImm(data.imm,raw,origiaddr);
	// Decode delay slots
	if(data.addDSlot()){
	  I(domap);
	  VAddr dsaddr=curAddr;
	  decodeInst(context,funcAddr,dsaddr,endAddr,trace,false);
	  myinst->iupdate=trace-myinst;
	}else if(data.mapDSlot()){
 	  I(domap);
	  decodeInst(context,funcAddr,curAddr,endAddr,trace,true);
	  myinst->iupdate=trace-myinst;
	}else{
          myinst->iupdate=1;
        }
	myinst->sescInst=createSescInst(myinst,origiaddr,curAddr-origiaddr,data.typ,data.ctl);
	op=data.next;
	if(op==OpInvalid){
          myinst->aupdate=domap?(curAddr-origiaddr):0;
	  // Is this the end of this trace?
          if(domap&&(curAddr==endAddr)){
	    InstTypInfo typ=static_cast<InstTypInfo>(data.typ&TypSubMask);
	    // Unconditional jumps, calls, and returns don't continue directly to next instruction
	    // Everything else needs an OpCut to link to the continuation in another trace
	    if((typ!=BrOpJump)&&(typ!=BrOpCall)&&(typ!=BrOpRet)){
              InstDesc *ctinst=trace++;
              ctinst->emul=emulCut;
              ctinst->regDst=ctinst->regSrc1=ctinst->regSrc2=RegNone;
              ctinst->imm.a=0;
              ctinst->iupdate=0;
              ctinst->aupdate=0;
#if (defined DEBUG)
              ctinst->op=OpCut;
              ctinst->addr=curAddr;
              ctinst->typ=TypNop;
#endif
              ctinst->sescInst=0;
            }
	  }
          break;
	}
	myinst->aupdate=0;
      }
    }
    void decodeTrace(ThreadContext *context, VAddr addr,size_t len){
      VAddr funcAddr=context->getAddressSpace()->getFuncAddr(addr);
      I(addr+len<=funcAddr+context->getAddressSpace()->getFuncSize(funcAddr));
      VAddr endAddr=addr+len;
      size_t tsize=0;
      VAddr sizaddr=addr;
      while(sizaddr!=endAddr)
        decodeInstSize(context,funcAddr,sizaddr,endAddr,tsize,true);
      InstDesc *trace=new InstDesc[tsize];
      InstDesc *curtrace=trace;
      VAddr trcaddr=addr;
      while(trcaddr!=endAddr)
        decodeInst(context,funcAddr,trcaddr,endAddr,curtrace,true);
      I((ssize_t)tsize==(curtrace-trace));
      I(trcaddr==sizaddr);
      context->getAddressSpace()->mapTrace(trace,curtrace,addr,trcaddr);
    }
  };

  DecodeInst dcdInst;
  
  DecodeInst::DecodeInst(void){
    // First look at the op field (uppermost six bits)
    opMap[OpKey(0x00000000,0x00000000)]=OpEntry(0xFC000000);

    // "Main" opcodes begin here (decoded using the op field)
    opMap[OpKey(0xFC000000,0x00000000)]=OpEntry(0xFC00003F); // SPECIAL (look at func)
    opMap[OpKey(0xFC000000,0x04000000)]=OpEntry(0xFC1F0000); // REGIMM (look at rt)
    opMap[OpKey(0xFC000000,0x08000000)]=OpEntry(OpJ);
    opMap[OpKey(0xFC000000,0x0C000000)]=OpEntry(OpJal);
    opMap[OpKey(0xFC000000,0x10000000)]=OpEntry(0xFFFF0000,OpBeq);   // OpB? (look at rs & rt)
    opMap[OpKey(0xFC000000,0x14000000)]=OpEntry(OpBne);
    opMap[OpKey(0xFC000000,0x18000000)]=OpEntry(OpBlez);
    opMap[OpKey(0xFC000000,0x1C000000)]=OpEntry(OpBgtz);
    opMap[OpKey(0xFC000000,0x20000000)]=OpEntry(OpAddi);
    opMap[OpKey(0xFC000000,0x24000000)]=OpEntry(0xFFE00000,OpAddiu); // OpLi? (look at rs)
    opMap[OpKey(0xFC000000,0x28000000)]=OpEntry(OpSlti);
    opMap[OpKey(0xFC000000,0x2C000000)]=OpEntry(OpSltiu);
    opMap[OpKey(0xFC000000,0x30000000)]=OpEntry(OpAndi);
    opMap[OpKey(0xFC000000,0x34000000)]=OpEntry(0xFFE00000,OpOri);   // OpLiu? (look at rs)
    opMap[OpKey(0xFC000000,0x38000000)]=OpEntry(OpXori);
    opMap[OpKey(0xFC000000,0x3C000000)]=OpEntry(OpLui);
    opMap[OpKey(0xFC000000,0x40000000)]=OpEntry(0x00000000); // COP0
    opMap[OpKey(0xFC000000,0x44000000)]=OpEntry(0xFFE00000); // COP1 (look at fmt)
    opMap[OpKey(0xFC000000,0x48000000)]=OpEntry(0x00000000); // COP2
    opMap[OpKey(0xFC000000,0x4C000000)]=OpEntry(0xFC00003F); // COP1X (look at func)
    opMap[OpKey(0xFC000000,0x50000000)]=OpEntry(OpBeql);
    opMap[OpKey(0xFC000000,0x54000000)]=OpEntry(OpBnel);
    opMap[OpKey(0xFC000000,0x58000000)]=OpEntry(OpBlezl);
    opMap[OpKey(0xFC000000,0x5C000000)]=OpEntry(OpBgtzl);
    opMap[OpKey(0xFC000000,0x60000000)]=OpEntry(OpDaddi);
    opMap[OpKey(0xFC000000,0x64000000)]=OpEntry(OpDaddiu);
    opMap[OpKey(0xFC000000,0x68000000)]=OpEntry(OpLdl);
    opMap[OpKey(0xFC000000,0x6C000000)]=OpEntry(OpLdr);
    opMap[OpKey(0xFC000000,0x80000000)]=OpEntry(OpLb);
    opMap[OpKey(0xFC000000,0x84000000)]=OpEntry(OpLh);
    opMap[OpKey(0xFC000000,0x88000000)]=OpEntry(OpLwl);
    opMap[OpKey(0xFC000000,0x8C000000)]=OpEntry(OpLw);
    opMap[OpKey(0xFC000000,0x90000000)]=OpEntry(OpLbu);
    opMap[OpKey(0xFC000000,0x94000000)]=OpEntry(OpLhu);
    opMap[OpKey(0xFC000000,0x98000000)]=OpEntry(OpLwr);
    opMap[OpKey(0xFC000000,0x9C000000)]=OpEntry(OpLwu);
    opMap[OpKey(0xFC000000,0xA0000000)]=OpEntry(OpSb);
    opMap[OpKey(0xFC000000,0xA4000000)]=OpEntry(OpSh);
    opMap[OpKey(0xFC000000,0xA8000000)]=OpEntry(OpSwl);
    opMap[OpKey(0xFC000000,0xAC000000)]=OpEntry(OpSw);
    opMap[OpKey(0xFC000000,0xB0000000)]=OpEntry(OpSdl);
    opMap[OpKey(0xFC000000,0xB4000000)]=OpEntry(OpSdr);
    opMap[OpKey(0xFC000000,0xB8000000)]=OpEntry(OpSwr);
    opMap[OpKey(0xFC000000,0xC0000000)]=OpEntry(OpLl);
    opMap[OpKey(0xFC000000,0xC4000000)]=OpEntry(OpLwc1);
    opMap[OpKey(0xFC000000,0xC8000000)]=OpEntry(OpLwc2);
    opMap[OpKey(0xFC000000,0xCC000000)]=OpEntry(OpPref);
    opMap[OpKey(0xFC000000,0xD0000000)]=OpEntry(OpLld);
    opMap[OpKey(0xFC000000,0xD4000000)]=OpEntry(OpLdc1);
    opMap[OpKey(0xFC000000,0xD8000000)]=OpEntry(OpLdc2);
    opMap[OpKey(0xFC000000,0xDC000000)]=OpEntry(OpLd);
    opMap[OpKey(0xFC000000,0xE0000000)]=OpEntry(OpSc);
    opMap[OpKey(0xFC000000,0xE4000000)]=OpEntry(OpSwc1);
    opMap[OpKey(0xFC000000,0xE8000000)]=OpEntry(OpSwc2);
    opMap[OpKey(0xFC000000,0xF0000000)]=OpEntry(OpScd);
    opMap[OpKey(0xFC000000,0xF4000000)]=OpEntry(OpSdc1);
    opMap[OpKey(0xFC000000,0xF8000000)]=OpEntry(OpSdc2);
    opMap[OpKey(0xFC000000,0xFC000000)]=OpEntry(OpSd);

    // "Special" opcodes begin here
    opMap[OpKey(0xFC00003F,0x00000000)]=OpEntry(0xFC00F83F,OpSll); // OpNop? (look at rd)
    opMap[OpKey(0xFC00003F,0x00000001)]=OpEntry(0xFC01003F);  // MOVCI (look at tf)
    opMap[OpKey(0xFC00003F,0x00000002)]=OpEntry(OpSrl);
    opMap[OpKey(0xFC00003F,0x00000003)]=OpEntry(OpSra);
    opMap[OpKey(0xFC00003F,0x00000004)]=OpEntry(OpSllv);
    opMap[OpKey(0xFC00003F,0x00000006)]=OpEntry(OpSrlv);
    opMap[OpKey(0xFC00003F,0x00000007)]=OpEntry(OpSrav);
    opMap[OpKey(0xFC00003F,0x00000008)]=OpEntry(0xFFE0003F,OpJr); // OpRet? (look at rs)
    opMap[OpKey(0xFC00003F,0x00000009)]=OpEntry(0xFC00F83F,OpJalr); // OpCallr (look at rd)
    opMap[OpKey(0xFC00003F,0x0000000A)]=OpEntry(OpMovz);
    opMap[OpKey(0xFC00003F,0x0000000B)]=OpEntry(OpMovn);
    opMap[OpKey(0xFC00003F,0x0000000C)]=OpEntry(OpSyscall);
    opMap[OpKey(0xFC00003F,0x0000000D)]=OpEntry(OpBreak);
    opMap[OpKey(0xFC00003F,0x0000000F)]=OpEntry(OpSync);
    opMap[OpKey(0xFC00003F,0x00000010)]=OpEntry(OpMfhi);
    opMap[OpKey(0xFC00003F,0x00000011)]=OpEntry(OpMthi);
    opMap[OpKey(0xFC00003F,0x00000012)]=OpEntry(OpMflo);
    opMap[OpKey(0xFC00003F,0x00000013)]=OpEntry(OpMtlo);
    opMap[OpKey(0xFC00003F,0x00000014)]=OpEntry(OpDsllv);
    opMap[OpKey(0xFC00003F,0x00000016)]=OpEntry(OpDsrlv);
    opMap[OpKey(0xFC00003F,0x00000017)]=OpEntry(OpDsrav);
    opMap[OpKey(0xFC00003F,0x00000018)]=OpEntry(OpMult);
    opMap[OpKey(0xFC00003F,0x00000019)]=OpEntry(OpMultu);
    opMap[OpKey(0xFC00003F,0x0000001A)]=OpEntry(OpDiv);
    opMap[OpKey(0xFC00003F,0x0000001B)]=OpEntry(OpDivu);
    opMap[OpKey(0xFC00003F,0x0000001C)]=OpEntry(OpDmult);
    opMap[OpKey(0xFC00003F,0x0000001D)]=OpEntry(OpDmultu);
    opMap[OpKey(0xFC00003F,0x0000001E)]=OpEntry(OpDdiv);
    opMap[OpKey(0xFC00003F,0x0000001F)]=OpEntry(OpDdivu);
    opMap[OpKey(0xFC00003F,0x00000020)]=OpEntry(OpAdd);
    // OpAddu must look at the rt field (bits 20:16) to check for OpMov
    opMap[OpKey(0xFC00003F,0x00000021)]=OpEntry(0xFC1F003F,OpAddu);
    opMap[OpKey(0xFC00003F,0x00000022)]=OpEntry(OpSub);
    // OpSubu must look at the rs field (bits 25:21) to check for OpNegu
    opMap[OpKey(0xFC00003F,0x00000023)]=OpEntry(0xFFE0003F,OpSubu);
    opMap[OpKey(0xFC00003F,0x00000024)]=OpEntry(OpAnd);
    opMap[OpKey(0xFC00003F,0x00000025)]=OpEntry(OpOr);
    opMap[OpKey(0xFC00003F,0x00000026)]=OpEntry(OpXor);
    // OpNor must look at the rs field (bits 25:21) to check for OpNot
    opMap[OpKey(0xFC00003F,0x00000027)]=OpEntry(0xFFE0003F,OpNor);
    opMap[OpKey(0xFC00003F,0x0000002A)]=OpEntry(OpSlt);
    opMap[OpKey(0xFC00003F,0x0000002B)]=OpEntry(OpSltu);
    opMap[OpKey(0xFC00003F,0x0000002C)]=OpEntry(OpDadd);
    opMap[OpKey(0xFC00003F,0x0000002D)]=OpEntry(OpDaddu);
    opMap[OpKey(0xFC00003F,0x0000002E)]=OpEntry(OpDsub);
    opMap[OpKey(0xFC00003F,0x0000002F)]=OpEntry(OpDsubu);
    opMap[OpKey(0xFC00003F,0x00000030)]=OpEntry(OpTge);
    opMap[OpKey(0xFC00003F,0x00000031)]=OpEntry(OpTgeu);
    opMap[OpKey(0xFC00003F,0x00000032)]=OpEntry(OpTlt);
    opMap[OpKey(0xFC00003F,0x00000033)]=OpEntry(OpTltu);
    opMap[OpKey(0xFC00003F,0x00000034)]=OpEntry(OpTeq);
    opMap[OpKey(0xFC00003F,0x00000036)]=OpEntry(OpTne);
    opMap[OpKey(0xFC00003F,0x00000038)]=OpEntry(OpDsll);
    opMap[OpKey(0xFC00003F,0x0000003A)]=OpEntry(OpDsrl);
    opMap[OpKey(0xFC00003F,0x0000003B)]=OpEntry(OpDsra);
    opMap[OpKey(0xFC00003F,0x0000003C)]=OpEntry(OpDsll32);
    opMap[OpKey(0xFC00003F,0x0000003E)]=OpEntry(OpDsrl32);
    opMap[OpKey(0xFC00003F,0x0000003F)]=OpEntry(OpDsra32);

    // "Regimm" opcodes begin here
    opMap[OpKey(0xFC1F0000,0x04000000)]=OpEntry(OpBltz);
    opMap[OpKey(0xFC1F0000,0x04010000)]=OpEntry(OpBgez);
    opMap[OpKey(0xFC1F0000,0x04020000)]=OpEntry(OpBltzl);
    opMap[OpKey(0xFC1F0000,0x04030000)]=OpEntry(OpBgezl);
    opMap[OpKey(0xFC1F0000,0x04080000)]=OpEntry(OpTgei);
    opMap[OpKey(0xFC1F0000,0x04090000)]=OpEntry(OpTgeiu);
    opMap[OpKey(0xFC1F0000,0x040A0000)]=OpEntry(OpTlti);
    opMap[OpKey(0xFC1F0000,0x040B0000)]=OpEntry(OpTltiu);
    opMap[OpKey(0xFC1F0000,0x040C0000)]=OpEntry(OpTeqi);
    opMap[OpKey(0xFC1F0000,0x040E0000)]=OpEntry(OpTnei);
    opMap[OpKey(0xFC1F0000,0x04100000)]=OpEntry(OpBltzal);
    // OpBgezal must look at rs (bits 25:21) to check for OpBal
    opMap[OpKey(0xFC1F0000,0x04110000)]=OpEntry(0xFFFF0000,OpBgezal);
    opMap[OpKey(0xFC1F0000,0x04120000)]=OpEntry(OpBltzall);
    opMap[OpKey(0xFC1F0000,0x04130000)]=OpEntry(OpBgezall);


    // "COP1" opcodes begin here
    opMap[OpKey(0xFFE00000,0x44000000)]=OpEntry(OpMfc1);
    opMap[OpKey(0xFFE00000,0x44200000)]=OpEntry(OpDmfc1);
    opMap[OpKey(0xFFE00000,0x44400000)]=OpEntry(OpCfc1);
    opMap[OpKey(0xFFE00000,0x44800000)]=OpEntry(OpMtc1);
    opMap[OpKey(0xFFE00000,0x44A00000)]=OpEntry(OpDmtc1);
    opMap[OpKey(0xFFE00000,0x44C00000)]=OpEntry(OpCtc1);
    // "COP1 fmt=BC" must be decoded using nd/tf (bits 17 and 16)
    opMap[OpKey(0xFFE00000,0x45000000)]=OpEntry(0xFFE30000);
    // "COP1 fmt=S" must be decoded using the func field (lowermost six bits)
    opMap[OpKey(0xFFE00000,0x46000000)]=OpEntry(0xFFE0003F);
    // "COP1 fmt=D" must be decoded using the func field (lowermost six bits)
    opMap[OpKey(0xFFE00000,0x46200000)]=OpEntry(0xFFE0003F);
    // "COP1 fmt=W" must be decoded using the func field (lowermost six bits)
    opMap[OpKey(0xFFE00000,0x46800000)]=OpEntry(0xFFE0003F);
    // "COP1 fmt=L" must be decoded using the func field (lowermost six bits)
    opMap[OpKey(0xFFE00000,0x46A00000)]=OpEntry(0xFFE0003F);

    // "COP1X" Opcodes begin here
    opMap[OpKey(0xFC00003F,0x4C000000)]=OpEntry(0x0000000,OpLwxc1);
    opMap[OpKey(0xFC00003F,0x4C000001)]=OpEntry(0x0000000,OpLdxc1);
    opMap[OpKey(0xFC00003F,0x4C000008)]=OpEntry(0x0000000,OpSwxc1);
    opMap[OpKey(0xFC00003F,0x4C000009)]=OpEntry(0x0000000,OpSdxc1);
    opMap[OpKey(0xFC00003F,0x4C00000F)]=OpEntry(0x0000000,OpPrefx);
    opMap[OpKey(0xFC00003F,0x4C000020)]=OpEntry(0x0000000,OpFmadds);
    opMap[OpKey(0xFC00003F,0x4C000021)]=OpEntry(0x0000000,OpFmaddd);
    opMap[OpKey(0xFC00003F,0x4C000028)]=OpEntry(0x0000000,OpFmsubs);
    opMap[OpKey(0xFC00003F,0x4C000029)]=OpEntry(0x0000000,OpFmsubd);
    opMap[OpKey(0xFC00003F,0x4C000030)]=OpEntry(0x0000000,OpFnmadds);
    opMap[OpKey(0xFC00003F,0x4C000031)]=OpEntry(0x0000000,OpFnmaddd);
    opMap[OpKey(0xFC00003F,0x4C000038)]=OpEntry(0x0000000,OpFnmsubs);
    opMap[OpKey(0xFC00003F,0x4C000039)]=OpEntry(0x0000000,OpFnmsubd);

    // "Movci" opcodes begin here
    opMap[OpKey(0xFC01003F,0x00000001)]=OpEntry(OpMovf);
    opMap[OpKey(0xFC01003F,0x00010001)]=OpEntry(OpMovt);

    // "COP1 fmt=BC" opcodes begin here
    opMap[OpKey(0xFFE30000,0x45000000)]=OpEntry(OpBc1f);
    opMap[OpKey(0xFFE30000,0x45010000)]=OpEntry(OpBc1t);
    opMap[OpKey(0xFFE30000,0x45020000)]=OpEntry(OpBc1fl);
    opMap[OpKey(0xFFE30000,0x45030000)]=OpEntry(OpBc1tl);

    // "COP1 fmt=S" opcodes begin here
    opMap[OpKey(0xFFE0003F,0x46000000)]=OpEntry(OpFadds);
    opMap[OpKey(0xFFE0003F,0x46000001)]=OpEntry(OpFsubs);
    opMap[OpKey(0xFFE0003F,0x46000002)]=OpEntry(OpFmuls);
    opMap[OpKey(0xFFE0003F,0x46000003)]=OpEntry(OpFdivs);
    opMap[OpKey(0xFFE0003F,0x46000004)]=OpEntry(OpFsqrts);
    opMap[OpKey(0xFFE0003F,0x46000005)]=OpEntry(OpFabss);
    opMap[OpKey(0xFFE0003F,0x46000006)]=OpEntry(OpFmovs);
    opMap[OpKey(0xFFE0003F,0x46000007)]=OpEntry(OpFnegs);
    opMap[OpKey(0xFFE0003F,0x46000008)]=OpEntry(OpFroundls);
    opMap[OpKey(0xFFE0003F,0x46000009)]=OpEntry(OpFtruncls);
    opMap[OpKey(0xFFE0003F,0x4600000A)]=OpEntry(OpFceills);
    opMap[OpKey(0xFFE0003F,0x4600000B)]=OpEntry(OpFfloorls);
    opMap[OpKey(0xFFE0003F,0x4600000C)]=OpEntry(OpFroundws);
    opMap[OpKey(0xFFE0003F,0x4600000D)]=OpEntry(OpFtruncws);
    opMap[OpKey(0xFFE0003F,0x4600000E)]=OpEntry(OpFceilws);
    opMap[OpKey(0xFFE0003F,0x4600000F)]=OpEntry(OpFfloorws);
    // "MOVCF.S" must be decoded using the tf bit (bit 16)
    opMap[OpKey(0xFFE0003F,0x46000011)]=OpEntry(0xFFE1003F);
    opMap[OpKey(0xFFE0003F,0x46000012)]=OpEntry(OpFmovzs);
    opMap[OpKey(0xFFE0003F,0x46000013)]=OpEntry(OpFmovns);
    opMap[OpKey(0xFFE0003F,0x46000015)]=OpEntry(OpFrecips);
    opMap[OpKey(0xFFE0003F,0x46000016)]=OpEntry(OpFrsqrts);
    opMap[OpKey(0xFFE0003F,0x46000021)]=OpEntry(OpFcvtds);
    opMap[OpKey(0xFFE0003F,0x46000024)]=OpEntry(OpFcvtws);
    opMap[OpKey(0xFFE0003F,0x46000025)]=OpEntry(OpFcvtls);
    opMap[OpKey(0xFFE0003F,0x46000030)]=OpEntry(OpFcfs);
    opMap[OpKey(0xFFE0003F,0x46000031)]=OpEntry(OpFcuns);
    opMap[OpKey(0xFFE0003F,0x46000032)]=OpEntry(OpFceqs);
    opMap[OpKey(0xFFE0003F,0x46000033)]=OpEntry(OpFcueqs);
    opMap[OpKey(0xFFE0003F,0x46000034)]=OpEntry(OpFcolts);
    opMap[OpKey(0xFFE0003F,0x46000035)]=OpEntry(OpFcults);
    opMap[OpKey(0xFFE0003F,0x46000036)]=OpEntry(OpFcoles);
    opMap[OpKey(0xFFE0003F,0x46000037)]=OpEntry(OpFcules);
    opMap[OpKey(0xFFE0003F,0x46000038)]=OpEntry(OpFcsfs);
    opMap[OpKey(0xFFE0003F,0x46000039)]=OpEntry(OpFcngles);
    opMap[OpKey(0xFFE0003F,0x4600003A)]=OpEntry(OpFcseqs);
    opMap[OpKey(0xFFE0003F,0x4600003B)]=OpEntry(OpFcngls);
    opMap[OpKey(0xFFE0003F,0x4600003C)]=OpEntry(OpFclts);
    opMap[OpKey(0xFFE0003F,0x4600003D)]=OpEntry(OpFcnges);
    opMap[OpKey(0xFFE0003F,0x4600003E)]=OpEntry(OpFcles);
    opMap[OpKey(0xFFE0003F,0x4600003F)]=OpEntry(OpFcngts);

    // "COP1 fmt=D" opcodes begin here
    opMap[OpKey(0xFFE0003F,0x46200000)]=OpEntry(OpFaddd);
    opMap[OpKey(0xFFE0003F,0x46200001)]=OpEntry(OpFsubd);
    opMap[OpKey(0xFFE0003F,0x46200002)]=OpEntry(OpFmuld);
    opMap[OpKey(0xFFE0003F,0x46200003)]=OpEntry(OpFdivd);
    opMap[OpKey(0xFFE0003F,0x46200004)]=OpEntry(OpFsqrtd);
    opMap[OpKey(0xFFE0003F,0x46200005)]=OpEntry(OpFabsd);
    opMap[OpKey(0xFFE0003F,0x46200006)]=OpEntry(OpFmovd);
    opMap[OpKey(0xFFE0003F,0x46200007)]=OpEntry(OpFnegd);
    opMap[OpKey(0xFFE0003F,0x46200008)]=OpEntry(OpFroundld);
    opMap[OpKey(0xFFE0003F,0x46200009)]=OpEntry(OpFtruncld);
    opMap[OpKey(0xFFE0003F,0x4620000A)]=OpEntry(OpFceilld);
    opMap[OpKey(0xFFE0003F,0x4620000B)]=OpEntry(OpFfloorld);
    opMap[OpKey(0xFFE0003F,0x4620000C)]=OpEntry(OpFroundwd);
    opMap[OpKey(0xFFE0003F,0x4620000D)]=OpEntry(OpFtruncwd);
    opMap[OpKey(0xFFE0003F,0x4620000E)]=OpEntry(OpFceilwd);
    opMap[OpKey(0xFFE0003F,0x4620000F)]=OpEntry(OpFfloorwd);
    // "MOVCF.D" must be decoded using the tf bit (bit 16)
    opMap[OpKey(0xFFE0003F,0x46200011)]=OpEntry(0xFFE1003F);
    opMap[OpKey(0xFFE0003F,0x46200012)]=OpEntry(OpFmovzd);
    opMap[OpKey(0xFFE0003F,0x46200013)]=OpEntry(OpFmovnd);
    opMap[OpKey(0xFFE0003F,0x46200015)]=OpEntry(OpFrecipd);
    opMap[OpKey(0xFFE0003F,0x46200016)]=OpEntry(OpFrsqrtd);
    opMap[OpKey(0xFFE0003F,0x46200020)]=OpEntry(OpFcvtsd);
    opMap[OpKey(0xFFE0003F,0x46200024)]=OpEntry(OpFcvtwd);
    opMap[OpKey(0xFFE0003F,0x46200025)]=OpEntry(OpFcvtld);
    opMap[OpKey(0xFFE0003F,0x46200030)]=OpEntry(OpFcfd);
    opMap[OpKey(0xFFE0003F,0x46200031)]=OpEntry(OpFcund);
    opMap[OpKey(0xFFE0003F,0x46200032)]=OpEntry(OpFceqd);
    opMap[OpKey(0xFFE0003F,0x46200033)]=OpEntry(OpFcueqd);
    opMap[OpKey(0xFFE0003F,0x46200034)]=OpEntry(OpFcoltd);
    opMap[OpKey(0xFFE0003F,0x46200035)]=OpEntry(OpFcultd);
    opMap[OpKey(0xFFE0003F,0x46200036)]=OpEntry(OpFcoled);
    opMap[OpKey(0xFFE0003F,0x46200037)]=OpEntry(OpFculed);
    opMap[OpKey(0xFFE0003F,0x46200038)]=OpEntry(OpFcsfd);
    opMap[OpKey(0xFFE0003F,0x46200039)]=OpEntry(OpFcngled);
    opMap[OpKey(0xFFE0003F,0x4620003A)]=OpEntry(OpFcseqd);
    opMap[OpKey(0xFFE0003F,0x4620003B)]=OpEntry(OpFcngld);
    opMap[OpKey(0xFFE0003F,0x4620003C)]=OpEntry(OpFcltd);
    opMap[OpKey(0xFFE0003F,0x4620003D)]=OpEntry(OpFcnged);
    opMap[OpKey(0xFFE0003F,0x4620003E)]=OpEntry(OpFcled);
    opMap[OpKey(0xFFE0003F,0x4620003F)]=OpEntry(OpFcngtd);

    // "COP1 fmt=W" opcodes begin here
    opMap[OpKey(0xFFE0003F,0x46800020)]=OpEntry(OpFcvtsw);
    opMap[OpKey(0xFFE0003F,0x46800021)]=OpEntry(OpFcvtdw);

    // "COP1 fmt=L" opcodes begin here
    opMap[OpKey(0xFFE0003F,0x46A00020)]=OpEntry(OpFcvtsl);
    opMap[OpKey(0xFFE0003F,0x46A00020)]=OpEntry(OpFcvtdl);

    // "MOVCF.S" opcodes begin here
    opMap[OpKey(0xFFE1003F,0x46000011)]=OpEntry(OpFmovfs);
    opMap[OpKey(0xFFE1003F,0x46010011)]=OpEntry(OpFmovts);

    // "MOVCF.D" opcodes begin here
    opMap[OpKey(0xFFE1003F,0x46200011)]=OpEntry(OpFmovfd);
    opMap[OpKey(0xFFE1003F,0x46210011)]=OpEntry(OpFmovtd);

    // OpSll with rd=zero is really an OpNop
    opMap[OpKey(0xFC00F83F,0x00000000)]=OpEntry(OpNop);
    // OpBeq with rs=rt=zero is actually OpB
    opMap[OpKey(0xFFFF0000,0x10000000)]=OpEntry(OpB);
    // OpBgezal with rs=zero is actually OpBal
    opMap[OpKey(0xFFFF0000,0x04110000)]=OpEntry(OpBal);
    // OpJr with rs=RegRA is actually an OpRet
    opMap[OpKey(0xFFE0003F,0x03e00008)]=OpEntry(OpRet);
    // OpJalr with rd=RegRA is actually OpCallr
    opMap[OpKey(0xFC00F83F,0x0000f809)]=OpEntry(OpCallr);
    // OpAddiu with rs=zero is actually OpLi
    opMap[OpKey(0xFFE00000,0x24000000)]=OpEntry(OpLi);
    // OpOri with rs=zero is actually OpLiu
    opMap[OpKey(0xFFE00000,0x34000000)]=OpEntry(OpLiu);
    // OpAddu with rt=zero is actually an OpMovw
    opMap[OpKey(0xFC1F003F,0x00000021)]=OpEntry(OpMovw);
    // OpSubu with rs=zero is actually an OpNegu
    opMap[OpKey(0xFFE0003F,0x00000023)]=OpEntry(OpNegu);
    // OpNor with rs=zero is actually an OpNot
    opMap[OpKey(0xFFE0003F,0x00000027)]=OpEntry(OpNot);

#define RegIntS int32_t
#define RegIntU uint32_t
    
#define OpDataC0(op,ctl,typ,dst,src1,src2,imm,next,emul) op2Data[op]=OpData(emul<op,Mips32 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC1(op,ctl,typ,dst,src1,src2,imm,next,emul,T1) op2Data[op]=OpData(emul<op,Mips32,T1 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC2(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2) op2Data[op]=OpData(emul<op,Mips32,T1,T2 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC3(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2,T3) op2Data[op]=OpData(emul<op,Mips32,T1,T2,T3 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC4(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2,T3,T4) op2Data[op]=OpData(emul<op,Mips32,T1,T2,T3,T4 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC5(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2,T3,T4,T5) op2Data[op]=OpData(emul<op,Mips32,T1,T2,T3,T4,T5 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC6(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2,T3,T4,T5,T6) op2Data[op]=OpData(emul<op,Mips32,T1,T2,T3,T4,T5,T6 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC7(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2,T3,T4,T5,T6,T7) op2Data[op]=OpData(emul<op,Mips32,T1,T2,T3,T4,T5,T6,T7 >,0,ctl,typ,dst,src1,src2,imm,next)
#define OpDataC8(op,ctl,typ,dst,src1,src2,imm,next,emul,T1,T2,T3,T4,T5,T6,T7,T8) op2Data[op]=OpData(emul<op,Mips32,T1,T2,T3,T4,T5,T6,T7,T8 >,0,ctl,typ,dst,src1,src2,imm,next)

#define OpDataI4(op,ctl,typ,dst,src1,src2,imm,next,emul,...) op2Data[op]=OpData(emul<op,Mips32,NextNext,__VA_ARGS__ >,0,ctl,typ,dst,src1,src2,imm,next)

//     OpDataX(OpJal     ,emulJump , CtlBpr, TypNop  , ArgNo  , ArgNo  , ArgNo  , ImmNo  , OpJal_);
//     OpDataE(OpJal_    ,emulJump , CtlJal, BrOpCall, ArgRa  , ArgNo  , ArgNo  , ImmJpTg);

//     OpDataX(OpJalr    ,emulJump , CtlBpr, TypNop  , ArgNo  , ArgNo  , ArgNo  , ImmNo  , OpJalr_);
//     OpDataE(OpJalr_   ,emulJump , CtlJlr, BrOpJump, ArgRd  , ArgRs  , ArgNo  , ImmNo);

    // Branches and jumps
    brNoDSlot[OpB]=OpOB;
    OpDataC6(OpOB      , CtlBrTg, BrOpJump , ArgNo  , ArgNo  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpB       , CtlPrBr, IntOpALU , ArgNo  , ArgNo  , ArgNo  , ImmNo  , OpB_      , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextCont);
    OpDataC6(OpB_      , CtlBrTg, BrOpJump , ArgNo  , ArgNo  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpJ]=OpOJ;
    OpDataC6(OpOJ      , CtlBrTg, BrOpJump , ArgNo  , ArgNo  , ArgNo  , ImmJpTg, OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpJ       , CtlPrBr, IntOpALU , ArgNo  , ArgNo  , ArgNo  , ImmNo  , OpJ_      , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextCont);
    OpDataC6(OpJ_      , CtlBrTg, BrOpJump , ArgNo  , ArgNo  , ArgNo  , ImmJpTg, OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpJr]=OpOJr;
    OpDataC6(OpOJr     , CtlBr  , BrOpJump , ArgNo  , ArgRs  , ArgNo  , ImmNo  , OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBReg);
    OpDataC6(OpJr      , CtlPrBr, IntOpALU , ArgBTmp, ArgRs  , ArgNo  , ImmNo  , OpJr_     , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestTarg, NextCont);
    OpDataC6(OpJr_     , CtlBr  , BrOpJump , ArgNo  , ArgBTmp, ArgNo  , ImmNo  , OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBReg);
    brNoDSlot[OpCallr]=OpOCallr;
    OpDataC6(OpOCallr  , CtlBr  , BrOpCall , ArgRa  , ArgRs  , ArgNo  , ImmNo  , OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestRetA, NextBReg);
    OpDataC6(OpCallr   , CtlNorm, IntOpALU , ArgBTmp, ArgRs  , ArgNo  , ImmNo  , OpCallrS  , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestTarg, NextCont);
    OpDataC6(OpCallrS  , CtlPrBr, IntOpALU , ArgRa  , ArgNo  , ArgNo  , ImmNo  , OpCallr_  , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestRetA, NextCont);
    OpDataC6(OpCallr_  , CtlBr  , BrOpCall , ArgNo  , ArgBTmp, ArgNo  , ImmNo  , OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBReg);
    brNoDSlot[OpBal]=OpOBal;
    OpDataC6(OpOBal    , CtlBrTg, BrOpCall , ArgRa  , ArgNo  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestRetA, NextBImm);
    OpDataC6(OpBal     , CtlPrBr, IntOpALU , ArgRa  , ArgNo  , ArgNo  , ImmNo  , OpBal_    , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestRetA, NextCont);
    OpDataC6(OpBal_    , CtlBrTg, BrOpCall , ArgNo  , ArgNo  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpRet]=OpORet;
    OpDataC6(OpORet    , CtlBr  , BrOpRet  , ArgNo  , ArgRa  , ArgNo  , ImmNo  , OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBReg);
    OpDataC6(OpRet     , CtlPrBr, IntOpALU , ArgBTmp, ArgRa  , ArgNo  , ImmNo  , OpRet_    , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestTarg, NextCont);
    OpDataC6(OpRet_    , CtlBr  , BrOpRet  , ArgNo  , ArgBTmp, ArgNo  , ImmNo  , OpInvalid , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestNone, NextBReg);
    brNoDSlot[OpBeq]=OpOBeq;
    OpDataC6(OpOBeq    , CtlBrTg, BrOpCond , ArgNo  , ArgRs  , ArgRt  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::eq<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBeq     , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgRt  , ImmNo  , OpBeq_    , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::eq<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBeq_    , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBeql]=OpOBeql;
    OpDataC6(OpOBeql   , CtlBrTL, BrOpCond , ArgNo  , ArgRs  , ArgRt  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::eq<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBeql    , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgRt  , ImmNo  , OpBeql_   , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::eq<RegIntS> , DestCond, NextAnDS);
    OpDataC6(OpBeql_   , CtlBrTL, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBne]=OpOBne;
    OpDataC6(OpOBne    , CtlBrTg, BrOpCond , ArgNo  , ArgRs  , ArgRt  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::ne<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBne     , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgRt  , ImmNo  , OpBne_    , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::ne<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBne_    , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBnel]=OpOBnel;
    OpDataC6(OpOBnel   , CtlBrTL, BrOpCond , ArgNo  , ArgRs  , ArgRt  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::ne<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBnel    , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgRt  , ImmNo  , OpBnel_   , emulJump, RegIntU, RegTypeGpr, RegTypeGpr, fns::ne<RegIntS> , DestCond, NextAnDS);
    OpDataC6(OpBnel_   , CtlBrTL, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBlez]=OpOBlez;
    OpDataC6(OpOBlez   , CtlBrTg, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::le<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBlez    , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBlez_   , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::le<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBlez_   , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBlezl]=OpOBlezl;
    OpDataC6(OpOBlezl  , CtlBrTL, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::le<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBlezl   , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBlezl_  , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::le<RegIntS> , DestCond, NextAnDS);
    OpDataC6(OpBlezl_  , CtlBrTL, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBgtz]=OpOBgtz;
    OpDataC6(OpOBgtz   , CtlBrTg, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::gt<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBgtz    , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBgtz_   , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::gt<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBgtz_   , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBgtzl]=OpOBgtzl;
    OpDataC6(OpOBgtzl  , CtlBrTL, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::gt<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBgtzl   , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBgtzl_  , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::gt<RegIntS> , DestCond, NextAnDS);
    OpDataC6(OpBgtzl_  , CtlBrTL, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBltz]=OpOBltz;
    OpDataC6(OpOBltz   , CtlBrTg, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBltz    , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBltz_   , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBltz_   , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBltzl]=OpOBltzl;
    OpDataC6(OpOBltzl  , CtlBrTL, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBltzl   , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBltzl_  , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestCond, NextAnDS);
    OpDataC6(OpBltzl_  , CtlBrTL, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBgez]=OpOBgez;
    OpDataC6(OpOBgez   , CtlBrTg, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBgez    , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBgez_   , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBgez_   , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBgezl]=OpOBgezl;
    OpDataC6(OpOBgezl  , CtlBrTL, BrOpCond , ArgNo  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestNone, NextBImm);
    OpDataC6(OpBgezl   , CtlPrBr, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBgezl_  , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestCond, NextAnDS);
    OpDataC6(OpBgezl_  , CtlBrTL, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBc1f]=OpOBc1f;
    OpDataC6(OpOBc1f   , CtlBrTg, IntOpALU , ArgNo  , ArgFbcc, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::eq<uint32_t>, DestNone, NextBImm);
    OpDataC6(OpBc1f    , CtlPrBr, IntOpALU , ArgCond, ArgFbcc, ArgNo  , ImmNo  , OpBc1f_   , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::eq<uint32_t>, DestCond, NextCont);
    OpDataC6(OpBc1f_   , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<uint32_t>, DestNone, NextBImm);
    brNoDSlot[OpBc1fl]=OpOBc1fl;
    OpDataC6(OpOBc1fl  , CtlBrTg, IntOpALU , ArgNo  , ArgFbcc, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::eq<uint32_t>, DestNone, NextBImm);
    OpDataC6(OpBc1fl   , CtlPrBr, IntOpALU , ArgCond, ArgFbcc, ArgNo  , ImmNo  , OpBc1fl_  , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::eq<uint32_t>, DestCond, NextAnDS);
    OpDataC6(OpBc1fl_  , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<uint32_t>, DestNone, NextBImm);
    brNoDSlot[OpBc1t]=OpOBc1t;
    OpDataC6(OpOBc1t   , CtlBrTg, IntOpALU , ArgNo  , ArgFbcc, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::ne<uint32_t>, DestNone, NextBImm);
    OpDataC6(OpBc1t    , CtlPrBr, IntOpALU , ArgCond, ArgFbcc, ArgNo  , ImmNo  , OpBc1t_   , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::ne<uint32_t>, DestCond, NextCont);
    OpDataC6(OpBc1t_   , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<uint32_t>, DestNone, NextBImm);
    brNoDSlot[OpBc1tl]=OpOBc1tl;
    OpDataC6(OpOBc1tl  , CtlBrTg, IntOpALU , ArgNo  , ArgFbcc, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::ne<uint32_t>, DestNone, NextBImm);
    OpDataC6(OpBc1tl   , CtlPrBr, IntOpALU , ArgCond, ArgFbcc, ArgNo  , ImmNo  , OpBc1tl_  , emulJump, RegIntU, RegTypeCtl, SrcZ      , fns::ne<uint32_t>, DestCond, NextAnDS);
    OpDataC6(OpBc1tl_  , CtlBrTg, BrOpCond , ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<uint32_t>, DestNone, NextBImm);
    brNoDSlot[OpBltzal]=OpOBltzal;
    OpDataC6(OpOBltzal , CtlBrTg, BrOpCCall, ArgRa  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestRetA, NextBImm);
    OpDataC6(OpBltzal  , CtlNorm, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBltzalS , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBltzalS , CtlPrBr, IntOpALU , ArgRa  , ArgNo  , ArgNo  , ImmNo  , OpBltzal_ , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestRetA, NextCont);
    OpDataC6(OpBltzal_ , CtlBrTg, BrOpCCall, ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBltzall]=OpOBltzall;
    OpDataC6(OpOBltzall, CtlBrTL, BrOpCCall, ArgRa  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestRetA, NextBImm);
    OpDataC6(OpBltzall , CtlNorm, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBltzallS, emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::lt<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBltzallS, CtlPrBr, IntOpALU , ArgRa  , ArgCond, ArgNo  , ImmNo  , OpBltzall_, emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestRetA, NextAnDS);
    OpDataC6(OpBltzall_, CtlBrTL, BrOpCCall, ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBgezal]=OpOBgezal;
    OpDataC6(OpOBgezal , CtlBrTg, BrOpCCall, ArgRa  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestRetA, NextBImm);
    OpDataC6(OpBgezal  , CtlNorm, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBgezalS , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBgezalS , CtlPrBr, IntOpALU , ArgRa  , ArgNo  , ArgNo  , ImmNo  , OpBgezal_ , emulJump, RegIntU, SrcZ      , SrcZ      , fns::tt<RegIntS> , DestRetA, NextCont);
    OpDataC6(OpBgezal_ , CtlBrTg, BrOpCCall, ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    brNoDSlot[OpBgezall]=OpOBgezall;
    OpDataC6(OpOBgezall, CtlBrTL, BrOpCCall, ArgRa  , ArgRs  , ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestRetA, NextBImm);
    OpDataC6(OpBgezall , CtlNorm, IntOpALU , ArgCond, ArgRs  , ArgNo  , ImmNo  , OpBgezallS, emulJump, RegIntU, RegTypeGpr, SrcZ      , fns::ge<RegIntS> , DestCond, NextCont);
    OpDataC6(OpBgezallS, CtlPrBr, IntOpALU , ArgRa  , ArgCond, ArgNo  , ImmNo  , OpBgezall_, emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestRetA, NextAnDS);
    OpDataC6(OpBgezall_, CtlBrTL, BrOpCCall, ArgNo  , ArgCond, ArgNo  , ImmBrOf, OpInvalid , emulJump, RegIntU, RegTypeSpc, SrcZ      , fns::ne<RegIntS> , DestNone, NextBImm);
    // Syscalls and traps
    OpDataC0(OpSyscall , CtlNorm, BrOpTrap , ArgNo  , ArgNo  , ArgNo  , ImmExCd, OpInvalid , emulSyscl);
    OpDataC0(OpBreak   , CtlNorm, BrOpTrap , ArgNo  , ArgNo  , ArgNo  , ImmExCd, OpInvalid , emulBreak);
    // Conditional traps
    OpDataC3(OpTge     , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgRt  , ImmTrCd, OpInvalid , emulTcnd , RegIntS, fns::ge<RegIntS> , true);
    OpDataC3(OpTgeu    , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgRt  , ImmTrCd, OpInvalid , emulTcnd , RegIntU, fns::ge<RegIntU> , true);
    OpDataC3(OpTlt     , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgRt  , ImmTrCd, OpInvalid , emulTcnd , RegIntS, fns::lt<RegIntS> , true);
    OpDataC3(OpTltu    , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgRt  , ImmTrCd, OpInvalid , emulTcnd , RegIntU, fns::lt<RegIntU> , true);
    OpDataC3(OpTeq     , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgRt  , ImmTrCd, OpInvalid , emulTcnd , RegIntS, fns::eq<RegIntS> , true);
    OpDataC3(OpTne     , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgRt  , ImmTrCd, OpInvalid , emulTcnd , RegIntS, fns::ne<RegIntS> , true);
    OpDataC3(OpTgei    , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulTcnd , RegIntS, fns::ge<RegIntS> , false);
    OpDataC3(OpTgeiu   , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulTcnd , RegIntU, fns::ge<RegIntU> , false);
    OpDataC3(OpTlti    , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulTcnd , RegIntS, fns::lt<RegIntS> , false);
    OpDataC3(OpTltiu   , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulTcnd , RegIntU, fns::lt<RegIntU> , false);
    OpDataC3(OpTeqi    , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulTcnd , RegIntS, fns::eq<RegIntS> , false);
    OpDataC3(OpTnei    , CtlNorm, BrOpCTrap, ArgNo  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulTcnd , RegIntS, fns::ne<RegIntS> , false);
    // Conditional moves
    OpDataC3(OpMovz    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulMovc , RegIntS  , RegIntS  ,RegTypeGpr);
    OpDataC3(OpMovn    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulMovc , RegIntS  , RegIntS  ,RegTypeGpr);
    OpDataC3(OpMovf    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgFCSR, ImmFbcc, OpInvalid , emulMovc , uint32_t , RegIntS  ,RegTypeGpr);
    OpDataC3(OpMovt    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgFCSR, ImmFbcc, OpInvalid , emulMovc , uint32_t , RegIntS  ,RegTypeGpr);
    OpDataC3(OpFmovzs  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgRt  , ImmNo  , OpInvalid , emulMovc , RegIntS  , float32_t,RegTypeFpr);
    OpDataC3(OpFmovns  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgRt  , ImmNo  , OpInvalid , emulMovc , RegIntS  , float32_t,RegTypeFpr);
    OpDataC3(OpFmovfs  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmFbcc, OpInvalid , emulMovc , uint32_t , float32_t,RegTypeFpr);
    OpDataC3(OpFmovts  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmFbcc, OpInvalid , emulMovc , uint32_t , float32_t,RegTypeFpr);
    OpDataC3(OpFmovzd  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgRt  , ImmNo  , OpInvalid , emulMovc , RegIntS  , float64_t,RegTypeFpr);
    OpDataC3(OpFmovnd  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgRt  , ImmNo  , OpInvalid , emulMovc , RegIntS  , float64_t,RegTypeFpr);
    OpDataC3(OpFmovfd  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmFbcc, OpInvalid , emulMovc , uint32_t , float64_t,RegTypeFpr);
    OpDataC3(OpFmovtd  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmFbcc, OpInvalid , emulMovc , uint32_t , float64_t,RegTypeFpr);
    // Load/Store
    OpDataC5(OpLb      , CtlNorm, MemOpLd1 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntS, int8_t  , RegTypeGpr);
    OpDataC5(OpLbu     , CtlNorm, MemOpLd1 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntU, uint8_t , RegTypeGpr);
    OpDataC5(OpLh      , CtlNorm, MemOpLd2 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntS, int16_t , RegTypeGpr);
    OpDataC5(OpLhu     , CtlNorm, MemOpLd2 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntU, uint16_t, RegTypeGpr);
    OpDataC5(OpLw      , CtlNorm, MemOpLd4 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntS, int32_t , RegTypeGpr);
    OpDataC5(OpLwu     , CtlNorm, MemOpLd4 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntU, uint32_t, RegTypeGpr);
    OpDataC5(OpLd      , CtlNorm, MemOpLd8 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntS, int64_t , RegTypeGpr);
    OpDataC5(OpSb      , CtlNorm, MemOpSt1 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, RegIntU, uint8_t , RegTypeGpr);
    OpDataC5(OpSh      , CtlNorm, MemOpSt2 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, RegIntU, uint16_t, RegTypeGpr);
    OpDataC5(OpSw      , CtlNorm, MemOpSt4 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, RegIntU, uint32_t, RegTypeGpr);
    OpDataC5(OpSd      , CtlNorm, MemOpSt8 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, RegIntU, uint64_t, RegTypeGpr);

    OpDataC2(OpLwl     , CtlNorm, MemOpLd4 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLdSt , RegIntS, RegIntU);
    OpDataC2(OpLwr     , CtlNorm, MemOpLd4 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLdSt , RegIntS, RegIntU);
    OpDataC2(OpLdl     , CtlNorm, MemOpLd8 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLdSt , RegIntS, RegIntU);
    OpDataC2(OpLdr     , CtlNorm, MemOpLd8 , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLdSt , RegIntS, RegIntU);
    OpDataC5(OpSwl     , CtlNorm, MemOpSt4 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, int32_t, uint32_t, RegTypeGpr);
    OpDataC5(OpSwr     , CtlNorm, MemOpSt4 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, int32_t, uint32_t, RegTypeGpr);
    OpDataC5(OpSdl     , CtlNorm, MemOpSt8 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, RegIntU, uint64_t, RegTypeGpr);
    OpDataC5(OpSdr     , CtlNorm, MemOpSt8 , ArgNo  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, RegIntU, uint64_t, RegTypeGpr);

    OpDataC5(OpLwc1    , CtlNorm, MemOpLd4 , ArgFt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, uint32_t, uint32_t, RegTypeFpr);
    OpDataC5(OpLdc1    , CtlNorm, MemOpLd8 , ArgFt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, uint64_t, uint64_t, RegTypeFpr);
    OpDataC5(OpLwxc1   , CtlNorm, MemOpLd4 , ArgFd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulLd, RegIntS, RegIntU, uint32_t, uint32_t, RegTypeFpr);
    OpDataC5(OpLdxc1   , CtlNorm, MemOpLd8 , ArgFd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulLd, RegIntS, RegIntU, uint64_t, uint64_t, RegTypeFpr);
    OpDataC5(OpSwc1    , CtlNorm, MemOpSt4 , ArgNo  , ArgRs  , ArgFt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, uint32_t, uint32_t, RegTypeFpr);
    OpDataC5(OpSdc1    , CtlNorm, MemOpSt8 , ArgNo  , ArgRs  , ArgFt  , ImmSExt, OpInvalid , emulSt, RegIntS, RegIntU, uint64_t, uint64_t, RegTypeFpr);
    // These are two-part ops: calc addr, then store
    OpDataI4(OpSwxc1   , CtlNorm, IntOpALU , ArgTmp , ArgRs  , ArgRt  , ImmNo  , OpSwxc1_  , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::plus<RegIntU>);
    OpDataC5(OpSwxc1_  , CtlNorm, MemOpSt4 , ArgNo  , ArgTmp , ArgFs  , ImmNo  , OpInvalid , emulSt   , RegIntS, RegIntU, uint32_t, uint32_t, RegTypeFpr);
    OpDataI4(OpSdxc1   , CtlNorm, IntOpALU , ArgTmp , ArgRs  , ArgRt  , ImmNo  , OpSdxc1_  , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::plus<RegIntU>);
    OpDataC5(OpSdxc1_  , CtlNorm, MemOpSt8 , ArgNo  , ArgTmp , ArgFs  , ImmNo  , OpInvalid , emulSt   , RegIntS, RegIntU, uint64_t, uint64_t, RegTypeFpr);
    // Load-linked and store-conditional sync ops
    OpDataC5(OpLl      , CtlNorm, MemOpLl  , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulLd, RegIntS, RegIntU, RegIntS , int32_t , RegTypeGpr);
    OpDataC2(OpSc      , CtlNorm, MemOpSc  , ArgRt  , ArgRs  , ArgRt  , ImmSExt, OpInvalid , emulLdSt , RegIntS, RegIntU);

    // Constant transfers
    OpDataI4(OpLui     , CtlNorm, IntOpALU , ArgRt  , ArgNo  , ArgNo  , ImmLui , OpInvalid , emulAlu  , SrcI      , SrcZ      , RegTypeGpr, fns::project1st_identity<RegIntS>);
    OpDataI4(OpLi      , CtlNorm, IntOpALU , ArgRt  , ArgNo  , ArgNo  , ImmSExt, OpInvalid , emulAlu  , SrcI      , SrcZ      , RegTypeGpr, fns::project1st_identity<RegIntS>);
    OpDataI4(OpLiu     , CtlNorm, IntOpALU , ArgRt  , ArgNo  , ArgNo  , ImmZExt, OpInvalid , emulAlu  , SrcI      , SrcZ      , RegTypeGpr, fns::project1st_identity<RegIntS>);
    // Shifts
    OpDataI4(OpSll     , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh  , OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_left<uint32_t>);
    OpDataI4(OpSrl     , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh  , OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_right<uint32_t>);
    OpDataI4(OpSra     , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh  , OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_right<int32_t>);
    OpDataI4(OpSllv    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgRs  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::shift_left<uint32_t>);
    OpDataI4(OpSrlv    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgRs  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::shift_right<uint32_t>);
    OpDataI4(OpSrav    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgRs  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::shift_right<int32_t>);
    OpDataI4(OpDsll    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh  , OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_left<uint64_t>);
    OpDataI4(OpDsrl    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh  , OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_right<uint64_t>);
    OpDataI4(OpDsra    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh  , OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_right<int64_t>);
    OpDataI4(OpDsll32  , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh32, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_left<uint64_t>);
    OpDataI4(OpDsrl32  , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh32, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_right<uint64_t>);
    OpDataI4(OpDsra32  , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmSh32, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::shift_right<int64_t>);
    OpDataI4(OpDsllv   , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgRs  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::shift_left<uint64_t>);
    OpDataI4(OpDsrlv   , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgRs  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::shift_right<uint64_t>);
    OpDataI4(OpDsrav   , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgRs  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::shift_right<int64_t>);
    // Logical
    OpDataI4(OpAnd     , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::bitwise_and<RegIntU>);
    OpDataI4(OpOr      , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::bitwise_or<RegIntU>);
    OpDataI4(OpXor     , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::bitwise_xor<RegIntU>);
    OpDataI4(OpNor     , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, fns::bitwise_nor<RegIntU>);
    OpDataI4(OpAndi    , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmZExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::bitwise_and<RegIntU>);
    OpDataI4(OpOri     , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmZExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::bitwise_or<RegIntU>);
    OpDataI4(OpXori    , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmZExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, fns::bitwise_xor<RegIntU>);
    OpDataI4(OpNot     , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeGpr, fns::project1st_bitwise_not<RegIntU>);
   // Arithmetic
    OpDataI4(OpAdd     , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::plus<int32_t>);
    OpDataI4(OpAddu    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::plus<int32_t>);
    OpDataI4(OpSub     , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::minus<int32_t>);
    OpDataI4(OpSubu    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::minus<int32_t>);
    OpDataI4(OpAddi    , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, std::plus<int32_t>);
    OpDataI4(OpAddiu   , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, std::plus<int32_t>);
    OpDataI4(OpNegu    , CtlNorm, IntOpALU , ArgRd  , ArgRt  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeGpr, fns::project1st_neg<int32_t>);
    OpDataI4(OpMult    , CtlNorm, IntOpMul , ArgLo  , ArgRs  , ArgRt  , ImmNo  , OpMult_   , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, std::multiplies<int32_t>);
    OpDataI4(OpMult_   , CtlNorm, IntOpALU , ArgHi  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, fns::multiplies_hi<int32_t>);
    OpDataI4(OpMultu   , CtlNorm, IntOpMul , ArgLo  , ArgRs  , ArgRt  , ImmNo  , OpMultu_  , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, std::multiplies<uint32_t>);
    OpDataI4(OpMultu_  , CtlNorm, IntOpALU , ArgHi  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, fns::multiplies_hi<uint32_t>);
    OpDataI4(OpDiv     , CtlNorm, IntOpDiv , ArgLo  , ArgRs  , ArgRt  , ImmNo  , OpDiv_    , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, std::divides<int32_t>);
    OpDataI4(OpDiv_    , CtlNorm, IntOpALU , ArgHi  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, std::modulus<int32_t>);
    OpDataI4(OpDivu    , CtlNorm, IntOpDiv , ArgLo  , ArgRs  , ArgRt  , ImmNo  , OpDivu_   , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, std::divides<uint32_t>);
    OpDataI4(OpDivu_   , CtlNorm, IntOpALU , ArgHi  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeSpc, std::modulus<uint32_t>);
    // Moves
    OpDataI4(OpMovw    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeGpr, fns::project1st_identity<int32_t>);
    OpDataI4(OpMfhi    , CtlNorm, IntOpALU , ArgRd  , ArgHi  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeSpc, SrcZ      , RegTypeGpr, fns::project1st_identity<RegIntS>);
    OpDataI4(OpMflo    , CtlNorm, IntOpALU , ArgRd  , ArgLo  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeSpc, SrcZ      , RegTypeGpr, fns::project1st_identity<RegIntS>);
    OpDataI4(OpMthi    , CtlNorm, IntOpALU , ArgHi  , ArgRs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeSpc, fns::project1st_identity<RegIntS>);
    OpDataI4(OpMtlo    , CtlNorm, IntOpALU , ArgLo  , ArgRs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeSpc, fns::project1st_identity<RegIntS>);
    OpDataI4(OpMfc1    , CtlNorm, IntOpALU , ArgRt  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeGpr, fns::project1st_identity<int32_t>);
    OpDataI4(OpCfc1    , CtlNorm, IntOpALU , ArgRt  , ArgFCs , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeCtl, SrcZ      , RegTypeGpr, fns::project1st_identity<int32_t>);
    OpDataI4(OpMtc1    , CtlNorm, IntOpALU , ArgFs  , ArgRt  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeFpr, fns::project1st_identity<int32_t>);
    OpDataI4(OpCtc1    , CtlNorm, IntOpALU , ArgFCs , ArgRt  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, SrcZ      , RegTypeCtl, fns::project1st_identity<int32_t>);
    // Integer comparisons
    OpDataI4(OpSlti    , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, std::less<RegIntS>);
    OpDataI4(OpSltiu   , CtlNorm, IntOpALU , ArgRt  , ArgRs  , ArgNo  , ImmSExt, OpInvalid , emulAlu  , RegTypeGpr, SrcI      , RegTypeGpr, std::less<RegIntU>);
    OpDataI4(OpSlt     , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::less<RegIntS>);
    OpDataI4(OpSltu    , CtlNorm, IntOpALU , ArgRd  , ArgRs  , ArgRt  , ImmNo  , OpInvalid , emulAlu  , RegTypeGpr, RegTypeGpr, RegTypeGpr, std::less<RegIntU>);
    // Floating-point comparisons
    OpDataI4(OpFcfs    , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, false, false, false>));
    OpDataI4(OpFcuns   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, false, false, true >));
    OpDataI4(OpFceqs   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, false, true , false>));
    OpDataI4(OpFcueqs  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, false, true , true >));
    OpDataI4(OpFcolts  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, true , false, false>));
    OpDataI4(OpFcults  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, true , false, true >));
    OpDataI4(OpFcoles  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, true , true , false>));
    OpDataI4(OpFcules  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, false, true , true , true >));
    OpDataI4(OpFcsfs   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , false, false, false>));
    OpDataI4(OpFcngles , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , false, false, true >));
    OpDataI4(OpFcseqs  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , false, true , false>));
    OpDataI4(OpFcngls  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , false, true , true >));
    OpDataI4(OpFclts   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , true , false, false>));
    OpDataI4(OpFcnges  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , true , false, true >));
    OpDataI4(OpFcles   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , true , true , false>));
    OpDataI4(OpFcngts  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float32_t, true , true , true , true >));
    OpDataI4(OpFcfd    , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, false, false, false>));
    OpDataI4(OpFcund   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, false, false, true >));
    OpDataI4(OpFceqd   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, false, true , false>));
    OpDataI4(OpFcueqd  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, false, true , true >));
    OpDataI4(OpFcoltd  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, true , false, false>));
    OpDataI4(OpFcultd  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, true , false, true >));
    OpDataI4(OpFcoled  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, true , true , false>));
    OpDataI4(OpFculed  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, false, true , true , true >));
    OpDataI4(OpFcsfd   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , false, false, false>));
    OpDataI4(OpFcngled , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , false, false, true >));
    OpDataI4(OpFcseqd  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , false, true , false>));
    OpDataI4(OpFcngld  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , false, true , true >));
    OpDataI4(OpFcltd   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , true , false, false>));
    OpDataI4(OpFcnged  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , true , false, true >));
    OpDataI4(OpFcled   , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , true , true , false>));
    OpDataI4(OpFcngtd  , CtlNorm, FpOpALU  , ArgFccc, ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeCtl, typeof(fns::float_compare<float64_t, true , true , true , true >));
    // Floating-point arithmetic
    OpDataI4(OpFmovs   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_identity<float32_t>);
    OpDataI4(OpFsqrts  , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_sqrt<float32_t>);
    OpDataI4(OpFabss   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_abs<float32_t>);
    OpDataI4(OpFnegs   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_neg<float32_t>);
    OpDataI4(OpFrecips , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_recip<float32_t>);
    OpDataI4(OpFrsqrts , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_sqrt_recip<float32_t>);
    OpDataI4(OpFmovd   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_identity<float64_t>);
    OpDataI4(OpFsqrtd  , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_sqrt<float64_t>);
    OpDataI4(OpFabsd   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_abs<float64_t>);
    OpDataI4(OpFnegd   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_neg<float64_t>);
    OpDataI4(OpFrecipd , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_recip<float64_t>);
    OpDataI4(OpFrsqrtd , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, fns::project1st_sqrt_recip<float64_t>);
    OpDataI4(OpFadds   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::plus<float32_t>);
    OpDataI4(OpFsubs   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::minus<float32_t>);
    OpDataI4(OpFmuls   , CtlNorm, FpOpMul  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float32_t>);
    OpDataI4(OpFdivs   , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::divides<float32_t>);
    OpDataI4(OpFaddd   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::plus<float64_t>);
    OpDataI4(OpFsubd   , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::minus<float64_t>);
    OpDataI4(OpFmuld   , CtlNorm, FpOpMul  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float64_t>);
    OpDataI4(OpFdivd   , CtlNorm, FpOpDiv  , ArgFd  , ArgFs  , ArgFt  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::divides<float64_t>);
    // These are two-part FP arithmetic ops: multiply, then add/subtract
    OpDataI4(OpFmadds  , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFmadds_ , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float32_t>);
    OpDataI4(OpFmadds_ , CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::plus<float32_t>);
    OpDataI4(OpFmaddd  , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFmaddd_ , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float64_t>);
    OpDataI4(OpFmaddd_ , CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::plus<float64_t>);
    OpDataI4(OpFmsubs  , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFmsubs_ , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float32_t>);
    OpDataI4(OpFmsubs_ , CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::minus<float32_t>);
    OpDataI4(OpFmsubd  , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFmsubd_ , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float64_t>);
    OpDataI4(OpFmsubd_ , CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::minus<float64_t>);
    OpDataI4(OpFnmadds , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFnmadds_, emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float32_t>);
    OpDataI4(OpFnmadds_, CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, fns::plus_neg<float32_t>);
    OpDataI4(OpFnmaddd , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFnmaddd_, emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float64_t>);
    OpDataI4(OpFnmaddd_, CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, fns::plus_neg<float64_t>);
    OpDataI4(OpFnmsubs , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFnmsubs_, emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float32_t>);
    OpDataI4(OpFnmsubs_, CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, fns::minus_neg<float32_t>);
    OpDataI4(OpFnmsubd , CtlNorm, FpOpMul  , ArgFTmp, ArgFs  , ArgFt  , ImmNo  , OpFnmsubd_, emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, std::multiplies<float64_t>);
    OpDataI4(OpFnmsubd_, CtlNorm, FpOpALU  , ArgFd  , ArgFTmp, ArgFr  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeFpr, RegTypeFpr, fns::minus_neg<float64_t>);
    // Floating-point format conversion
    OpDataI4(OpFroundls, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int64_t,FE_TONEAREST>));
    OpDataI4(OpFtruncls, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int64_t,FE_TOWARDZERO>));
    OpDataI4(OpFceills , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int64_t,FE_UPWARD>));
    OpDataI4(OpFfloorls, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int64_t,FE_DOWNWARD>));
    OpDataI4(OpFroundws, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int32_t,FE_TONEAREST>));
    OpDataI4(OpFtruncws, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int32_t,FE_TOWARDZERO>));
    OpDataI4(OpFceilws , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int32_t,FE_UPWARD>));
    OpDataI4(OpFfloorws, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float32_t,int32_t,FE_DOWNWARD>));
    OpDataI4(OpFroundld, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int64_t,FE_TONEAREST>));
    OpDataI4(OpFtruncld, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int64_t,FE_TOWARDZERO>));
    OpDataI4(OpFceilld , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int64_t,FE_UPWARD>));
    OpDataI4(OpFfloorld, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int64_t,FE_DOWNWARD>));
    OpDataI4(OpFroundwd, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int32_t,FE_TONEAREST>));
    OpDataI4(OpFtruncwd, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int32_t,FE_TOWARDZERO>));
    OpDataI4(OpFceilwd , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int32_t,FE_UPWARD>));
    OpDataI4(OpFfloorwd, CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgNo  , ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, SrcZ      , RegTypeFpr, typeof(fns::project1st_round<float64_t,int32_t,FE_DOWNWARD>));
    OpDataI4(OpFcvtds  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<float32_t,uint32_t,float64_t>));
    OpDataI4(OpFcvtws  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<float32_t,uint32_t,int32_t>));
    OpDataI4(OpFcvtls  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<float32_t,uint32_t,int64_t>));
    OpDataI4(OpFcvtsd  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<float64_t,uint32_t,float32_t>));
    OpDataI4(OpFcvtwd  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<float64_t,uint32_t,int32_t>));
    OpDataI4(OpFcvtld  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<float64_t,uint32_t,int64_t>));
    OpDataI4(OpFcvtsw  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<int32_t,uint32_t,float32_t>));
    OpDataI4(OpFcvtdw  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<int32_t,uint32_t,float64_t>));
    OpDataI4(OpFcvtsl  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<int64_t,uint32_t,float32_t>));
    OpDataI4(OpFcvtdl  , CtlNorm, FpOpALU  , ArgFd  , ArgFs  , ArgFCSR, ImmNo  , OpInvalid , emulAlu  , RegTypeFpr, RegTypeCtl, RegTypeFpr, typeof(mips_float_convert<int64_t,uint32_t,float64_t>));

    OpDataC0(OpNop     , CtlNorm, TypNop   , ArgNo  , ArgNo  , ArgNo  , ImmNo  , OpInvalid , emulNop  );

#undef OpDataC0
#undef OpDataC1
#undef OpDataC2
#undef OpDataC2
#undef OpDataC3
#undef OpDataC4
#undef OpDataC5
#undef RegIntS
#undef RegIntU

  }

} // End of namespace Mips

#if (defined INTERCEPT_HEAP_CALLS)
InstDesc *handleMallocCall(InstDesc *inst, ThreadContext *context){
  uint32_t siz=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegA0));
  printf("Malloc call with size=0x%08x\n",siz);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
InstDesc *handleMallocRet(InstDesc *inst, ThreadContext *context){
  uint32_t addr=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegV0));
  printf("Malloc ret  with addr=0x%08x\n",addr);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
InstDesc *handleCallocCall(InstDesc *inst, ThreadContext *context){
  uint32_t nmemb=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegA0));
  uint32_t siz  =Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegA1));
  printf("Calloc call with nmemb=0x%08x size=0x%08x\n",nmemb,siz);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
InstDesc *handleCallocRet(InstDesc *inst, ThreadContext *context){
  uint32_t addr=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegV0));
  printf("Calloc ret  with addr=0x%08x\n",addr);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
InstDesc *handleReallocCall(InstDesc *inst, ThreadContext *context){
  uint32_t addr=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegA0));
  uint32_t siz =Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegA1));
  printf("Realloc call with addr=0x%08x size=0x%08x\n",addr,siz);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
InstDesc *handleReallocRet(InstDesc *inst, ThreadContext *context){
  uint32_t addr=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegV0));
  printf("Realloc ret  with addr=0x%08x\n",addr);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
InstDesc *handleFreeCall(InstDesc *inst, ThreadContext *context){
  uint32_t addr=Mips::getRegAny<Mips32,uint32_t,RegTypeGpr>(context,static_cast<RegName>(Mips::RegA0));
  printf("Free call with addr=0x%08x\n",addr);
  context->updIDesc(1);
  return (*(inst+1))(context);
}
#endif

void decodeTrace(ThreadContext *context, VAddr addr, size_t len){
#if (defined INTERCEPT_HEAP_CALLS)
  static bool didThis=false;
  if(!didThis){
    // This should not be here, but it's added for testing
    AddressSpace::addCallHandler("malloc",handleMallocCall);
    AddressSpace::addRetHandler("malloc",handleMallocRet);
    AddressSpace::addCallHandler("calloc",handleCallocCall);
    AddressSpace::addRetHandler("calloc",handleCallocRet);
    AddressSpace::addCallHandler("realloc",handleReallocCall);
    AddressSpace::addRetHandler("realloc",handleReallocRet);
    AddressSpace::addCallHandler("free",handleFreeCall);
    didThis=true;
  }
#endif
  Mips::dcdInst.decodeTrace(context,addr,len);
}
