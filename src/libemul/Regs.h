#if !(defined REGS_H)
#define REGS_H

#include <stdint.h>

enum RegNameEnum{
  RegTypeMask = 0x300, // Mask for determining the type of a register
  RegTypeGpr  = 0x000, // General-purpose (integer) registers
  RegTypeFpr  = 0x100, // Floating point registers
  RegTypeCtl  = 0x200, // Control registers
  RegTypeSpc  = 0x300, // Special registers

  RegNumMask  = 0x0FF, // Mask for register number

  // Not a register name, it is the maximum number of all registers
  NumOfRegs   = RegTypeMask+RegNumMask+1,
  // Placeholder for when the instruction has no register operand
  RegNone=NumOfRegs,
};
typedef RegNameEnum RegName;

inline bool isGprName(RegName reg){ return (reg&RegTypeMask)==RegTypeGpr; }
inline bool isFprName(RegName reg){ return (reg&RegTypeMask)==RegTypeFpr; }

inline size_t  getRegNum(RegName reg){ return (reg&RegNumMask); }
inline RegName getRegType(RegName reg){ return (RegName)(reg&RegTypeMask); }

typedef int64_t RegVal;

#endif // !(defined REGS_H)
