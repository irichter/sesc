#if !(defined ADDRESS_SPACE_H)
#define ADDRESS_SPACE_H

#include <map>
#include "Addressing.h"
#include "common.h"

class AddressSpace{
  // Mapping of virtual to real addresses
  static size_t getPageSize(void){ return 1<<16; }
  typedef std::map<VAddr,RAddr> PageTable;
  PageTable pageTable;

  // Mapping of function names to code addresses
  typedef std::map<char *,VAddr> NameToAddrMap;
  typedef std::multimap<VAddr,char *> AddrToNameMap;
  NameToAddrMap funcNameToAddr;
  AddrToNameMap funcAddrToName;
 public:
  AddressSpace(void){
  }
  ~AddressSpace(void){
    // Free function name strings
    for(AddrToNameMap::iterator funcIt=funcAddrToName.begin();funcIt!=funcAddrToName.end();funcIt++)
      free(funcIt->second);
  }
  void addReference(void){
  }
  void delReference(void){
  }
  // Creates real memory for a range of virtual memory addresses
  void newRMem(VAddr begVAddr, VAddr endVAddr){
    begVAddr=alignDown(begVAddr,getPageSize());
    endVAddr=alignUp(endVAddr,getPageSize());
    void *realMem;
    if(posix_memalign(&realMem,getPageSize(),endVAddr-begVAddr))
      fatal("AddressSpace::newRMem could not allocate memory\n");
    for(VAddr pagePtr=begVAddr;pagePtr!=endVAddr;pagePtr+=getPageSize()){
      if((pageTable.find(pagePtr)!=pageTable.end())&&(pageTable[pagePtr]))
	fatal("AddressSpace::newRMem region overlaps with existing memory");
      pageTable[pagePtr]=(RAddr)realMem+(pagePtr-begVAddr);
    }
  }
  // Deletes real memory for a range of virtual memory addresses
  void delRMem(VAddr begVAddr, VAddr endVAddr){
    begVAddr=alignDown(begVAddr,getPageSize());
    endVAddr=alignUp(endVAddr,getPageSize());
    RAddr begRAddr=pageTable[begVAddr];
    free((void *)begRAddr);    
    for(VAddr pagePtr=begVAddr;pagePtr!=endVAddr;pagePtr+=getPageSize()){
      if(pageTable[pagePtr]!=begRAddr+(pagePtr-begVAddr))
	fatal("AddressSpace::delRMem region not allocated contiguously");
      pageTable[pagePtr]=0;
    }
  }
  // Finds a unmapped virtual memory range of a given size, starting at the beginning of the address space
  VAddr findVMemLow(size_t memSize){
    memSize=alignUp(memSize,getPageSize());
    VAddr foundAddr=0;
    for(PageTable::iterator ptIt=pageTable.begin();ptIt!=pageTable.end();ptIt++){
      if(!ptIt->second)
	continue;
      if(ptIt->first>=foundAddr+memSize)
	break;
      foundAddr=ptIt->first+getPageSize();
    }
    return foundAddr;
  }
  // Finds a unmapped virtual memory range of a given size, starting at the beginning of the address space
  VAddr findVMemHigh(size_t memSize){
    memSize=alignUp(memSize,getPageSize());
    // Note that we skip the last page of the address space
    // It creates problems with range checking because its
    // upper-bound address is 0 due to wrap-around
    VAddr foundAddr=(VAddr)(0-getPageSize()-memSize);
    for(PageTable::reverse_iterator ptIt=pageTable.rbegin();ptIt!=pageTable.rend();ptIt++){
      if(!ptIt->second)
	continue;
      if(ptIt->first+getPageSize()<=foundAddr)
	break;
      foundAddr=ptIt->first-memSize;
    }
    return foundAddr;
  }
  // Maps a virtual address to a real address
  RAddr virtToReal(VAddr vaddr) const{
    VAddr page=alignDown(vaddr,getPageSize());
    PageTable::const_iterator pageIt=pageTable.find(page);
    if((pageIt==pageTable.end())||(!pageIt->second))
      return 0;
    return pageIt->second+(vaddr-page);
  }
  // Maps a real address to a virtual address (if any) in this address space
  VAddr realToVirt(RAddr raddr) const{
    RAddr page=alignDown(raddr,getPageSize());
    return 0;
  }
  // Add a new function name-address mapping
  void addFuncName(const char *name, VAddr addr){
    char *myName=strdup(name);
    if(!myName)
      fatal("AddressSpace::addFuncName couldn't copy function name %s\n",name);
    if(funcNameToAddr.find(myName)!=funcNameToAddr.end())
      fatal("AddressSpace::addFuncName called with duplicate function name %s\n",name);
    funcNameToAddr.insert(NameToAddrMap::value_type(myName,addr));
    funcAddrToName.insert(AddrToNameMap::value_type(addr,myName));
  }
};

#endif
