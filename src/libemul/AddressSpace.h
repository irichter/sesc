#if !(defined ADDRESS_SPACE_H)
#define ADDRESS_SPACE_H

#include <unistd.h>
#include <vector>
#include <set>
#include <map>
#include <list>
#include "Addressing.h"
//#include "CvtEndian.h"
#include "InstDesc.h"
#include "common.h"
#include "nanassert.h"
#include "GCObject.h"
#include "DbgObject.h"
#include "Checkpoint.h"
#include "FileSys.h"
#include "MemState.h"

#define MAP_PAGE_TABLE
//#define NEW_PAGE_TABLE

typedef size_t PageNum;

#define AddrSpacPageOffsBits (12)
#define AddrSpacPageSize     (1<<AddrSpacPageOffsBits)
#define AddrSpacPageOffsMask (AddrSpacPageSize-1)

typedef uint64_t MemAlignType;

namespace MemSys{

 // Information about pages of physical memory
  class FrameDesc : public GCObject{
  public:
    typedef SmartPtr<FrameDesc> pointer;
  private:
    typedef std::set<PAddr> PAddrSet;
    static PAddr    nextPAddr;
    static PAddrSet freePAddrs;
    static inline PAddr newPAddr(void){
      PAddr retVal;
      if(nextPAddr){
	retVal=nextPAddr;
	nextPAddr+=AddrSpacPageSize;
      }else{
	PAddrSet::iterator it=freePAddrs.begin();
	if(it==freePAddrs.end())
	  fail("FrameDesc::newPAddr ran out of physical address space\n");
	retVal=*it;
	freePAddrs.erase(it);
      }
      return retVal;
    }
    PAddr    basePAddr;
    bool     shared;
    bool     dirty;
    FileSys::FileStatus::pointer filePtr;
    off_t               fileOff;
    MemAlignType data[AddrSpacPageSize/sizeof(MemAlignType)];
#if (defined HAS_MEM_STATE)
    MemState state[AddrSpacPageSize/MemState::Granularity];
#endif
  public:
    FrameDesc();
    FrameDesc(FileSys::FileStatus *fs, off_t ofs);
    FrameDesc(FrameDesc &src);
    ~FrameDesc();
    int8_t *getData(VAddr addr){
      size_t offs=(addr&AddrSpacPageOffsMask);
      int8_t *retVal=&(reinterpret_cast<int8_t *>(data)[offs]);
      I(reinterpret_cast<unsigned long int>(retVal)>=reinterpret_cast<unsigned long int>(data));
      I(AddrSpacPageSize==sizeof(data));
      I(reinterpret_cast<unsigned long int>(retVal)<reinterpret_cast<unsigned long int>(data)+AddrSpacPageSize);
      return retVal;
    }
    const int8_t *getData(VAddr addr) const{
      size_t offs=(addr&AddrSpacPageOffsMask);
      const int8_t *retVal=&(reinterpret_cast<const int8_t *>(data)[offs]);
      I(reinterpret_cast<unsigned long int>(retVal)>=reinterpret_cast<unsigned long int>(data));
      I(AddrSpacPageSize==sizeof(data));
      I(reinterpret_cast<unsigned long int>(retVal)<reinterpret_cast<unsigned long int>(data)+AddrSpacPageSize);
      return retVal;
    }
    PAddr getPAddr(VAddr addr) const{
      return basePAddr+(addr&AddrSpacPageOffsMask);
    }
#if (defined HAS_MEM_STATE)
    MemState &getState(VAddr addr){
      size_t offs=(addr&AddrSpacPageOffsMask)/MemState::Granularity;
      I(offs*sizeof(MemState)<sizeof(state));
      return state[offs];
    }
    const MemState &getState(VAddr addr) const{
      size_t offs=(addr&AddrSpacPageOffsMask)/MemState::Granularity;
      I(offs*sizeof(MemState)<sizeof(state));
      return state[offs];
    }
#endif
    void save(ChkWriter &out) const;
    FrameDesc(ChkReader &in);
  };

}

class AddressSpace : public GCObject{
 public:
  typedef SmartPtr<AddressSpace> pointer;
 private:
  class SegmentDesc{
  public:
    // Starting adress and length of the segment
    VAddr  addr;
    size_t len;
    // Protections for this segment
    bool   canRead;
    bool   canWrite;
    bool   canExec;
    // Does the segment autogrow and in which direction
    bool autoGrow;
    bool growDown;
    // Does this segment correspond to a shared mapping
    bool shared;
    // Points to the FileStatus from which this segment is mapped
    FileSys::FileStatus::pointer fileStatus;
    // If mapped from a file, this is the offset in the file from which the mapping came
    off_t fileOffset;
    SegmentDesc &operator=(const SegmentDesc &src){
      addr=src.addr;
      len=src.len;
      canRead =src.canRead;
      canWrite=src.canWrite;
      canExec =src.canExec;
      autoGrow=src.autoGrow;
      growDown=src.growDown;
      shared=src.shared;
      fileStatus=src.fileStatus;
      fileOffset=src.fileOffset;
      return *this;
    }
    // Page number (not address) of the first page that overlaps with this segment
    size_t pageNumLb(void){
      return (addr>>AddrSpacPageOffsBits);
    }
    // Page number (not address) of the first page after this segment with no overlap with it
    size_t pageNumUb(void){
      I(len);
      return ((addr+len+AddrSpacPageSize-1)>>AddrSpacPageOffsBits);
    }
    void save(ChkWriter &out) const;
    ChkReader &operator=(ChkReader &in);
  };
  typedef std::map<VAddr, SegmentDesc, std::greater<VAddr> > SegmentMap;
  SegmentMap segmentMap;
  // Information about decoded instructions for a page of virtual memory
  class TraceDesc{
  public:
    InstDesc *binst;
    InstDesc *einst;
    VAddr     baddr;
    VAddr     eaddr;
  };
  typedef std::map<VAddr, TraceDesc, std::greater<VAddr> > TraceMap;
  TraceMap traceMap;
  // The InstMap is sorted by virtual address from lowest to highest
  typedef std::map<VAddr, InstDesc *> InstMap;
  InstMap  instMap;
  // Information about pages of virtual memory (in each AddressSpace)
  class PageDesc
#if (defined DEBUG_PageDesc)
 : public DbgObject<PageDesc>
#endif
  {
  public:
    MemSys::FrameDesc::pointer frame;
//    TraceDesc *insts;
    // Number of mapped segments that overlap with this page
    size_t     mapCount;
    // Number of can-read segments that overlap with this page
    size_t     canRead;
    // Number of can-write segments that overlap with this page
    size_t     canWrite;
    // Number of can-exec segments that overlap with this page
    size_t     canExec;
    // Is this page shared
    bool shared;
    // This page needs a copy-on-write
    bool copyOnWrite;
    PageDesc(void);
    PageDesc(PageDesc &src);
    PageDesc(const PageDesc &src);
    PageDesc &operator=(PageDesc &src);
    void map(bool r, bool w, bool x){
      if(!mapCount){
	frame=new MemSys::FrameDesc();
      }
      mapCount++;
      canRead+=r;
      canWrite+=w;
      canExec+=x;
    }
    void unmap(bool r, bool w, bool x){
      canRead-=r;
      canWrite-=w;
      canExec-=x;
      mapCount--;
      if(mapCount)
	return;
      // Last mapping removed, free memory
      I((!canRead)&&(!canWrite)&&(!canExec));
      if(frame)
	frame=0;
    }
    void allow(bool r, bool w, bool x){
      canRead+=r;
      canWrite+=w;
      canExec+=x;
    }
    void deny(bool r, bool w, bool x){
      canRead-=r;
      canWrite-=w;
      canExec-=x;
    }      
    MemSys::FrameDesc *getFrame(void) const{
      return frame;
    }
    void save(ChkWriter &out) const;
    ChkReader &operator=(ChkReader &in);
  };
  class PageTable{
#if (defined MAP_PAGE_TABLE)
    typedef std::map<size_t,PageDesc> PageMap;
    PageMap map;
#endif
#if (defined NEW_PAGE_TABLE)
    static const PageNum PageBlockBits=16;
    static const PageNum PageBlockSize=1<<PageBlockBits;
    static const PageNum PageBlockMask=PageBlockSize-1;
    typedef PageDesc *PageBlock;
    typedef std::vector<PageBlock> PageBlocks;
    PageBlocks pageBlocks;
#endif
    class Cache{
    public:
      struct Entry{
	PageNum   pageNum;
	PageDesc *pageDesc;
	Entry(void) : pageNum(0), pageDesc(0){
	}
      };
    private:
      static const PageNum AddrSpaceCacheSize=(1<<16);
      Entry cache[AddrSpaceCacheSize];
    public:
      inline Entry &operator[](PageNum pageNum){
	return cache[pageNum%AddrSpaceCacheSize];
      }
      inline void clear(void){
	for(size_t i=0;i<AddrSpaceCacheSize;i++)
	  cache[i].pageNum=0;
      }
    };
    Cache cache;
  public:
    PageTable(void){
    }
    PageTable(PageTable &src){
#if (defined MAP_PAGE_TABLE)
      for(PageMap::iterator it=src.map.begin();it!=src.map.end();it++)
	map[it->first]=it->second;
#endif
#if (defined NEW_PAGE_TABLE)
      for(PageNum b=0;b<src.pageBlocks.size();b++){
	if(src.pageBlocks[b]){
	  pageBlocks.push_back(new PageDesc[PageBlockSize]);
	  for(PageNum p=0;p<PageBlockSize;p++)
	    pageBlocks[b][p]=src.pageBlocks[b][p];
	}else{
	  pageBlocks.push_back(0);
	}
      }
#endif
    }
    inline PageDesc &operator[](PageNum pageNum){
      Cache::Entry &centry=cache[pageNum];
      if(centry.pageNum==pageNum)
	return *(centry.pageDesc);
#if (defined MAP_PAGE_TABLE)
      PageDesc &entry=map[pageNum];
#endif
#if (defined NEW_PAGE_TABLE)
      PageNum block=(pageNum&PageBlockMask);
      PageNum bitem=(pageNum>>PageBlockBits);
      while(pageBlocks.size()<=block)
	pageBlocks.push_back(0);
      if(!pageBlocks[block])
	pageBlocks[block]=new PageDesc[PageBlockSize];
      PageDesc &entry=pageBlock[block][bitem];
#endif
      centry.pageNum=pageNum;
      centry.pageDesc=&entry;
      return entry;
    }
    inline MemSys::FrameDesc *getFrameDesc(PageNum pageNum) const{
#if (defined MAP_PAGE_TABLE)
      PageMap::const_iterator it=map.find(pageNum);
      I(it!=map.begin());
      return it->second.frame;
#endif
#if (defined NEW_PAGE_TABLE)
      PageNum block=(pageNum&PageBlockMask);
      PageNum bitem=(pageNum>>PageBlockBits);
      if(pageBlocks.size()<=block)
	return 0;
      if(!pageBlocks[block])
	return 0;
      return pageBlocks[block][bitem].frame;
#endif
    }
    inline void clear(void){
#if (defined MAP_PAGE_TABLE)
      map.clear();
#endif
#if (defined NEW_PAGE_TABLE)
      for(PageNum b=0;b<pageBlocks.size();b++)
	if(pageBlocks[b])
	  delete [] pageBlocks[b];
#endif
      cache.clear();
    }
  };
  PageTable pageTable;
  static inline size_t getPageNum(VAddr addr){
    return (addr>>AddrSpacPageOffsBits);
  }
  static inline size_t getPageOff(VAddr addr){
    return (addr&AddrSpacPageOffsMask);
  }
  VAddr brkBase;
 public:
  static inline size_t getPageSize(void){
    return AddrSpacPageSize;
  }
  static inline VAddr pageAlignDown(VAddr addr){
    return alignDown(addr,AddrSpacPageSize);    
  }
  static inline VAddr pageAlignUp(VAddr addr){
    return alignUp(addr,AddrSpacPageSize);    
  }
  // Returns true iff the specified block does not ovelap with any allocated segment
  // and can be used to allocate a new segment or extend an existing one
  bool isNoSegment(VAddr addr, size_t len) const{
    I(addr>0);
    if(addr+len<addr)
      return false;
    SegmentMap::const_iterator segIt=segmentMap.upper_bound(addr+len);
    if(segIt==segmentMap.end())
      return true;
    const SegmentDesc &segDesc=segIt->second;
    return (segDesc.addr+segDesc.len<=addr);
  }
  // Returns true iff the specified block is entirely within the same allocated segment
  bool isInSegment(VAddr addr, size_t len) const{
    SegmentMap::const_iterator segIt=segmentMap.lower_bound(addr);
    return (segIt!=segmentMap.end())&&(segIt->second.addr+segIt->second.len>=addr+len);
  }
  // Returns true iff the specified block exactly matches an allocated segment
  bool isSegment(VAddr addr, size_t len) const{
    SegmentMap::const_iterator segIt=segmentMap.find(addr);
    return (segIt!=segmentMap.end())&&(segIt->second.len==len);
  }
  // Returns true iff the entire specified block is mapped
  bool isMapped(VAddr addr, size_t len) const{
    if(addr+len<addr)
      return false;
    while(true){
      SegmentMap::const_iterator segIt=segmentMap.lower_bound(addr);
      if(segIt==segmentMap.end())
	return false;
      VAddr endAddr=segIt->second.addr+segIt->second.len;
      if(endAddr<=addr)
        return false;
      if(endAddr>=addr+len)
	return true;
      len-=(endAddr-addr);
      addr=endAddr;
    }
  }
  void setBrkBase(VAddr addr){
    brkBase=addr;
  }
  VAddr getBrkBase(void) const{
    I(brkBase);
    return brkBase;
  }
  void setGrowth(VAddr addr, bool autoGrow, bool growDown){
    I(segmentMap.find(addr)!=segmentMap.end());
    if(autoGrow){
      I(segmentMap[addr].addr==alignDown(segmentMap[addr].addr,AddrSpacPageSize));
      I(segmentMap[addr].len==alignDown(segmentMap[addr].len,AddrSpacPageSize));
    }
    segmentMap[addr].autoGrow=autoGrow;
    segmentMap[addr].growDown=growDown;
  }
  // Splits a segment into two, one that ends at the pivot and one that begins there
  // The pivot must be within an existing segment
  void splitSegment(VAddr pivot);
  void newSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec, bool shared=false, FileSys::FileStatus *fs=0, off_t offs=0);
  void protectSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec);
  VAddr newSegmentAddr(size_t len);
  void deleteSegment(VAddr addr, size_t len);
  void resizeSegment(VAddr addr, size_t len);
  void growSegmentDown(VAddr oldaddr, VAddr newaddr);
  void moveSegment(VAddr oldaddr, VAddr newaddr);
  VAddr getSegmentAddr(VAddr addr) const{
    SegmentMap::const_iterator segIt=segmentMap.lower_bound(addr);
    I(segIt!=segmentMap.end());
    I(segIt->second.addr+segIt->second.len>addr);
    return segIt->second.addr;
  }
  size_t getSegmentSize(VAddr addr) const{
    SegmentMap::const_iterator segIt=segmentMap.find(addr);
    I(segIt!=segmentMap.end());
    return segIt->second.len;
  }
  inline bool canRead(VAddr addr, size_t len) const{
    SegmentMap::const_iterator segIt=segmentMap.lower_bound(addr);
    if(segIt==segmentMap.end())
      return false;
    const SegmentDesc &mySeg=segIt->second;
    return (mySeg.addr+mySeg.len>=addr+len)&&mySeg.canRead;
  }
  inline bool canWrite(VAddr addr, size_t len) const{
    SegmentMap::const_iterator segIt=segmentMap.lower_bound(addr);
    if(segIt==segmentMap.end())
      return false;
    const SegmentDesc &mySeg=segIt->second;
    return (mySeg.addr+mySeg.len>=addr+len)&&mySeg.canWrite;
  }
  inline bool canExec(VAddr addr, size_t len) const{
    SegmentMap::const_iterator segIt=segmentMap.lower_bound(addr);
    if(segIt==segmentMap.end())
      return false;
    const SegmentDesc &mySeg=segIt->second;
    return (mySeg.addr+mySeg.len>=addr+len)&&mySeg.canExec;
  }
  inline RAddr virtToReal(VAddr addr) const{
    I(0);
    size_t pageOffs=(addr&AddrSpacPageOffsMask);
    size_t pageNum=(addr>>AddrSpacPageOffsBits);
    MemSys::FrameDesc *frame=pageTable.getFrameDesc(pageNum);
    if(!frame)
      return 0;
    return (RAddr)(frame->getData(addr));
  }
  inline PAddr virtToPhys(VAddr addr) const{
    I(0);
    size_t pageOffs=(addr&AddrSpacPageOffsMask);
    size_t pageNum=(addr>>AddrSpacPageOffsBits);
    MemSys::FrameDesc *frame=pageTable.getFrameDesc(pageNum);
    if(!frame)
      return 0;
    return frame->getPAddr(addr);
  }
  void createTrace(ThreadContext *context, VAddr addr);
  void delMapInsts(VAddr begAddr, VAddr endAddr);
  bool reqMapInst(VAddr addr){
    if(instMap.find(addr)!=instMap.end())
      return false;
    instMap[addr]=0;
    return true;
  }
  void mapInst(VAddr addr,InstDesc *inst){
    I(inst);
    I(addr);
    I(!isMappedInst(addr));
    instMap[addr]=inst;
  }
  bool isMappedInst(VAddr addr) const{
    InstMap::const_iterator it=instMap.find(addr);
    return (it!=instMap.end())&&(it->second!=0);
  }
  bool needMapInst(VAddr addr) const{
    InstMap::const_iterator it=instMap.find(addr);
    return (it!=instMap.end())&&(it->second==0);
  }
  void mapTrace(InstDesc *binst, InstDesc *einst, VAddr baddr, VAddr eaddr);
  inline InstDesc *virtToInst(VAddr addr) const{
    InstMap::const_iterator it=instMap.find(addr);
    if(it==instMap.end())
      return 0;
    return it->second;
  }
 public:
  AddressSpace(void);
  AddressSpace(AddressSpace &src);
  void clear(bool isExec);
  ~AddressSpace(void);
  // Saves this address space to a stream
  void save(ChkWriter &out) const;
  AddressSpace(ChkReader &in);

  template<class T>
  inline T readMemRaw(VAddr addr){
    size_t pageNum=getPageNum(addr);
    I(addr+sizeof(T)<=(pageNum+1)*AddrSpacPageSize);
    PageDesc &myPage=pageTable[pageNum];
    I(myPage.canRead);
#if (defined EMUL_VALGRIND)
    if(*(reinterpret_cast<T *>(myPage.frame->getData(addr)))==0)
      if(addr==0)
	printf("Never\n");
#endif
    return *(reinterpret_cast<T *>(myPage.frame->getData(addr)));
  }
  template<class T>
  inline bool readMemRaw(VAddr addr, T &val){
    size_t pageNum=getPageNum(addr);
    I(addr+sizeof(T)<=(pageNum+1)*AddrSpacPageSize);
    PageDesc &myPage=pageTable[pageNum];
    if(!myPage.canRead)
      return false;
#if (defined EMUL_VALGRIND)
    if(*(reinterpret_cast<T *>(myPage.frame->getData(addr)))==0)
      if(addr==0)
	printf("Never\n");
#endif
    val=*(reinterpret_cast<T *>(myPage.frame->getData(addr)));
    return true;
  }
  template<class T>
  inline bool writeMemRaw(VAddr addr, T val){
    size_t pageNum=getPageNum(addr);
    I(addr+sizeof(T)<=(pageNum+1)*AddrSpacPageSize);
    PageDesc &myPage=pageTable[pageNum];
    if(!myPage.canWrite)
      return false;
    if(myPage.copyOnWrite){
      myPage.copyOnWrite=false;
      if(myPage.frame->getRefCount()>1)
	myPage.frame=new MemSys::FrameDesc(*myPage.frame);
    }
#if (defined EMUL_VALGRIND)
    if(val==0)
      if(addr==0)
	printf("Never\n");
#endif
    *(reinterpret_cast<T *>(myPage.frame->getData(addr)))=val;
    return true;
  }
#if (defined HAS_MEM_STATE)
/*   inline const MemState &getState(VAddr addr) const{ */
/*     size_t pageNum=getPageNum(addr); */
/*     I(addr<(pageNum+1)*AddrSpacPageSize); */
/*     const PageDesc &myPage=getPageDesc(pageNum); */
/*     return myPage.frame->getState(addr); */
/*   } */
  inline MemState &getState(VAddr addr){
    size_t pageNum=getPageNum(addr);             
    I(addr<(pageNum+1)*AddrSpacPageSize);                         
    PageDesc &myPage=getPageDesc(pageNum);
    if(myPage.copyOnWrite){
      myPage.copyOnWrite=false;
      if(myPage.frame->getRefCount()>1)
	myPage.frame=new MemSys::FrameDesc(*myPage.frame);
    }
    return myPage.frame->getState(addr);
  }
#endif
  //
  // Mapping of function names to code addresses
  //
 private:
  struct NameEntry{
    VAddr       addr;
    std::string func;
    std::string file;
    NameEntry(VAddr addr) : addr(addr), func(), file(){
    }
    NameEntry(VAddr addr, const std::string &func, const std::string &file) : addr(addr), func(func), file(file){
    }
    struct ByAddr{
      bool operator()(const NameEntry &e1, const NameEntry &e2) const{
	if(e1.addr!=e2.addr)
	  return (e1.addr>e2.addr);
	if(e1.func!=e2.func)
	  return (e1.func<e2.func);
	return (e1.file<e2.file);
      }
    };
    struct ByName{
      bool operator()(const NameEntry *e1, const NameEntry *e2) const{
	if(e1->func!=e2->func)
	  return (e1->func<e2->func);
	if(e1->file!=e2->file)
	  return (e1->file<e2->file);
	return (e1->addr<e2->addr);
      }
    };
  };
  typedef std::set< NameEntry, NameEntry::ByAddr >   NamesByAddr;
  NamesByAddr namesByAddr;
  typedef std::set< const NameEntry *, NameEntry::ByName > NamesByName;
  NamesByName namesByName;
 public:
  // Add a new function name-address mapping
  void addFuncName(VAddr addr, const std::string &func, const std::string &file);
  // Removes all existing function name mappings in a given address range
  void delFuncNames(VAddr begAddr, VAddr endAddr);
  // Return name of the function with given entry point
  const std::string &getFuncName(VAddr addr) const;
  // Return name of the ELF file in which the function is
  const std::string &getFuncFile(VAddr addr) const;
  // Given the name, return where the function begins
  VAddr getFuncAddr(const std::string &name) const;
  // Given a code address, return where the function begins (best guess)
  VAddr getFuncAddr(VAddr addr) const;
  // Given a code address, return the function size (best guess)
  size_t getFuncSize(VAddr addr) const;
  // Print name(s) of function(s) with given entry point
  void printFuncName(VAddr addr) const;
  //
  // Interception of function calls
  //
 public:
  typedef std::list<EmulFunc *> HandlerSet;
  //  typedef std::set<EmulFunc *> HandlerSet;
 private:
/*   struct ltstr{ */
/*     bool operator()(const char *s1, const char *s2) const{ */
/*       return (strcmp(s1,s2)<0); */
/*     } */
/*   }; */
/*   typedef std::multimap<const char *,EmulFunc *,ltstr> NameToFuncMap; */
  typedef std::map<std::string,HandlerSet> NameToFuncMap;
  static NameToFuncMap nameToCallHandler;
  static NameToFuncMap nameToRetHandler;
  static void addHandler(const std::string &name, NameToFuncMap &map, EmulFunc *func);
  static void delHandler(const std::string &name, NameToFuncMap &map, EmulFunc *func);
  static bool getHandlers(const std::string &name, const NameToFuncMap &map, HandlerSet &set);
 public:
  static void addCallHandler(const std::string &name,EmulFunc *func);
  static void addRetHandler(const std::string &name,EmulFunc *func);
  static void delCallHandler(const std::string &name,EmulFunc *func);
  static void delRetHandler(const std::string &name,EmulFunc *func);
  bool getCallHandlers(VAddr addr, HandlerSet &set) const;
  bool getRetHandlers(VAddr addr, HandlerSet &set) const;
};
#endif
