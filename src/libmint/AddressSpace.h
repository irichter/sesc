#if !(defined ADDRESS_SPACE_H)
#define ADDRESS_SPACE_H

#include <vector>
#include <map>
#include "Addressing.h"
#include "common.h"

class AddressSpace{
  static inline size_t getPageSize(void){ return 1<<16; }
  static inline size_t getVPage(VAddr vaddr){ return vaddr/getPageSize(); }
  // Mapping of virtual to real addresses
  typedef std::vector<RAddr> PageTable;
  PageTable pageTable;
  // Mapping of function names to code addresses
  typedef std::map<char *,VAddr> NameToAddrMap;
  typedef std::multimap<VAddr,char *> AddrToNameMap;
  NameToAddrMap funcNameToAddr;
  AddrToNameMap funcAddrToName;
 public:
  AddressSpace(void) :
    // Allocate the entire page table and zero it out
    pageTable(((1<<(8*sizeof(VAddr)-1))/getPageSize())*2,0){
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
    for(size_t pageNum=getVPage(begVAddr);pageNum!=getVPage(endVAddr);pageNum++){
      if(pageTable[pageNum])
	fatal("AddressSpace::newRMem region overlaps with existing memory");
      pageTable[pageNum]=(RAddr)realMem+(pageNum*getPageSize()-begVAddr);
    }
  }
  // Deletes real memory for a range of virtual memory addresses
  void delRMem(VAddr begVAddr, VAddr endVAddr){
    begVAddr=alignDown(begVAddr,getPageSize());
    endVAddr=alignUp(endVAddr,getPageSize());
    RAddr begRAddr=pageTable[begVAddr];
    free((void *)begRAddr);    
    for(VAddr pageNum=getVPage(begVAddr);pageNum!=getVPage(endVAddr);pageNum++){
      if(pageTable[pageNum]!=begRAddr+(pageNum*getPageSize()-begVAddr))
	fatal("AddressSpace::delRMem region not allocated contiguously");
      pageTable[pageNum]=0;
    }
  }
  // Finds a unmapped virtual memory range of a given size,
  // starting at the beginning of the address space
  VAddr findVMemLow(size_t memSize){
    size_t needPages=alignUp(memSize,getPageSize())/getPageSize();
    size_t foundPages=0;
    // Skip the first (zero) page, to avoid making null pointers valid
    size_t pageNum=1;
    while(foundPages<needPages){
      if(pageTable[pageNum])
	foundPages=0;
      else
	foundPages++;
      pageNum++;
      if(pageNum==pageTable.size())
	fatal("AddressSpace::findVMemLow not enough available virtual memory\n");
    }
    return (pageNum-needPages)*getPageSize();
  }
  // Finds a unmapped virtual memory range of a given size, 
  // starting at the end of the address space
  VAddr findVMemHigh(size_t memSize){
    size_t needPages=alignUp(memSize,getPageSize())/getPageSize();
    size_t foundPages=0;
    // Skip the last page, it creates addressing problems
    // becasue its upper-bound address is 0 due to wrap-around
    size_t pageNum=pageTable.size()-2;
    while(foundPages<needPages){
      if(pageTable[pageNum])
	foundPages=0;
      else
	foundPages++;
      pageNum--;
      // Can not use page zero because that would make the null pointer valid
      if(pageNum==0)
	fatal("AddressSpace::findVMemLow not enough available virtual memory\n");
    }
    return pageNum*getPageSize();
  }
  // Maps a virtual address to a real address
  RAddr virtToReal(VAddr vaddr) const{
    size_t vPage=getVPage(vaddr);
    RAddr  rPageBase=pageTable[vPage];
    if(!rPageBase)
      return 0;
    return rPageBase+(vaddr-vPage*getPageSize());
  }
  // Maps a real address to a virtual address (if any) in this address space
  VAddr realToVirt(RAddr raddr) const{
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
