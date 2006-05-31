#if !(defined ADDRESSING_H)
#define ADDRESSING_H

// There are three types of addresses used in the simulator
typedef intptr_t RAddr;        // Type for Real addresses (actual address in the simulator)
typedef uint32_t PAddr;        // Type for Physical addresses (in simulated machine)
typedef uint32_t VAddr;        // Type for Virtual addresses (in simulated application)

#endif
