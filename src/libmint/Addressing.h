#if !(defined ADDRESSING_H)
#define ADDRESSING_H

#include <stdlib.h>
#include <errno.h>

// There are three types of addresses used in the simulator

// Type for Real addresses (actual address in the simulator)
typedef uintptr_t RAddr;
// Type for Physical addresses (in simulated machine)
typedef uint32_t PAddr;
// Type for Virtual addresses (in simulated application)
typedef uint32_t VAddr;

// These are the native types for the target (simulated) machine
typedef uint16_t targUShort;
typedef int16_t  targShort;
typedef uint32_t targUInt;
typedef int32_t  targInt;
typedef uint32_t targULong;
typedef int32_t  targLong;
typedef uint64_t targULongLong;
typedef int64_t  targLongLong;

// Return value v aligned (up or down) to a multiple of a
template<class V, class A>
inline V alignDown(V v, A a){
  return v-(v&(a-1));
}
template<class V, class A>
inline V alignUp(V v, A a){
  return alignDown(v+a-1,a);
}

// Converts numeric values between host machine's endianness
// and the simulated big endian format
template<class T>
inline T bigEndian(T val){
#if (defined LENDIAN)
  switch(sizeof(val)){
  case 1:
    return val;
  case 2:
    return
      ((val&0xff)<<8) |
      ((val>>8)&0xff);
  case 4:
    return 
      ((val&0xff)<<24)|
      (((val>>8)&0xff)<<16)|
      (((val>>16)&0xff)<<8)|
      ((val>>24)&0xff);
  default:
    errno=EINVAL;
    perror("bigEndian called with wrong-size argument");
    exit(1);
  }
#else // !(defined LENDIAN)
  return val;
#endif // !(defined LENDIAN)
}

#endif
