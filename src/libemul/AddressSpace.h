#if !(defined ADDRESS_SPACE_H)
#define ADDRESS_SPACE_H

#include <unistd.h>
#include <vector>
#include <set>
#include <map>
#include "Addressing.h"
#include "CvtEndian.h"
#include "InstDesc.h"
#include "common.h"
#include "nanassert.h"
#include "GCObject.h"
#include "DbgObject.h"
#include "Checkpoint.h"

#include "MemState.h"

#define AddrSpacPageOffsBits (12)
#define AddrSpacPageSize     (1<<AddrSpacPageOffsBits)
#define AddrSpacPageOffsMask (AddrSpacPageSize-1)
#define AddrSpacPageNumBits  (sizeof(VAddr)*8-AddrSpacPageOffsBits)
#define AddrSpacPageNumCount (1<<AddrSpacPageNumBits)
//#define SPLIT_PAGE_TABLE
#if (defined SPLIT_PAGE_TABLE)
#define AddrSpacRootBits (AddrSpacPageNumBits/2)
#define AddrSpacRootSize (1<<AddrSpacRootBits)
#define AddrSpacLeafBits (AddrSpacPageNumBits-AddrSpacRootBits)
#define AddrSpacLeafSize (1<<AddrSpacLeafBits)
#define AddrSpacLeafMask (AddrSpacLeafSize-1)
#endif

typedef uint64_t MemAlignType;

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
    // Is this segment a file mapping (mmap)
    bool fileMap;
    SegmentDesc &operator=(const SegmentDesc &src){
      addr=src.addr;
      len=src.len;
      canRead =src.canRead;
      canWrite=src.canWrite;
      canExec =src.canExec;
      autoGrow=src.autoGrow;
      growDown=src.growDown;
      shared=src.shared;
      fileMap=src.fileMap;
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
  typedef std::map<VAddr, InstDesc *, std::greater<VAddr> > InstMap;
  InstMap  instMap;
  // Information about pages of physical memory
  class FrameDesc : public GCObject{
  public:
    typedef SmartPtr<FrameDesc> pointer;
  private:
    MemAlignType data[AddrSpacPageSize/sizeof(MemAlignType)];
#if (defined HAS_MEM_STATE)
    MemState state[AddrSpacPageSize/MemState::Granularity];
#endif
  public:
    FrameDesc();
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
  // Information about pages of virtual memory (in each AddressSpace)
  class PageDesc
#if (defined DEBUG_PageDesc)
 : public DbgObject<PageDesc>
#endif
  {
  public:
    FrameDesc::pointer frame;
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
    PageDesc &operator=(PageDesc &src);
    void map(bool r, bool w, bool x){
      if(!mapCount){
	frame=new FrameDesc();
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
    FrameDesc *getFrame(void) const{
      return frame;
    }
    void save(ChkWriter &out) const;
    ChkReader &operator=(ChkReader &in);
  };
#if (defined SPLIT_PAGE_TABLE)
  typedef PageDesc *PageMapLeaf;
  PageMapLeaf pageMapRoot[AddrSpacRootSize];
#else
  PageDesc pageMap[AddrSpacPageNumCount];
#endif
  static inline size_t getPageNum(VAddr addr){
    return (addr>>AddrSpacPageOffsBits);
  }
  static inline size_t getPageOff(VAddr addr){
    return (addr&AddrSpacPageOffsMask);
  }
  inline const PageDesc &getPageDesc(size_t pageNum) const{
#if (defined SPLIT_PAGE_TABLE)
    size_t rootNum=(pageNum>>AddrSpacLeafBits);
    PageMapLeaf pageMap=pageMapRoot[rootNum];
    I(pageMap);
    pageNum=(pageNum&AddrSpacLeafMask);
#endif
    return pageMap[pageNum];
  }
  inline PageDesc &getPageDesc(size_t pageNum){
#if (defined SPLIT_PAGE_TABLE)
    size_t rootNum=(pageNum>>AddrSpacLeafBits);
    PageMapLeaf pageMap=pageMapRoot[rootNum];
    if(!pageMap){
      pageMap=new PageDesc[AddrSpacLeafSize];
      pageMapRoot[rootNum]=pageMap;
    }
    pageNum=(pageNum&AddrSpacLeafMask);
#endif
    return pageMap[pageNum];
  }
  VAddr brkBase;
 public:
  static inline size_t getPageSize(void){
    return AddrSpacPageSize;
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
      if(endAddr>=addr+len)
	return true;
      len-=(endAddr-addr);
      addr=endAddr;
    }
  }
  void setBrkBase(VAddr addr){
    while(addr%sizeof(MemAlignType))
      addr++;
    I(isNoSegment(addr,sizeof(MemAlignType)));
    brkBase=addr;
    newSegment(brkBase,sizeof(MemAlignType),true,true,false);
    MemAlignType val=0;
    writeMemRaw(brkBase,val);
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
  void newSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec, bool shared=false, bool fileMap=false);
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
    size_t pageOffs=(addr&AddrSpacPageOffsMask);
    size_t pageNum=(addr>>AddrSpacPageOffsBits);
    //    I(addr==pageNum*PageSize+pageOffs);
#if (defined SPLIT_PAGE_TABLE)
    size_t rootNum=(pageNum>>AddrSpacLeafBits);
    size_t leafNum=(pageNum&AddrSpacLeafMask);
    PageMapLeaf leafTable=pageMapRoot[rootNum];
    if(!leafTable)
      return 0;
    FrameDesc *frame=leafTable[leafNum].frame;
#else
    FrameDesc *frame=pageMap[pageNum].frame;
#endif
    if(!frame)
      return 0;
    return (RAddr)(frame->getData(addr));
  }
  void createTrace(ThreadContext *context, VAddr addr){
    I((!isMappedInst(addr))||!instMap[addr]);
    instMap[addr]=0;
    //    TraceMap::iterator cur=traceMap.upper_bound(addr);
    //    if((cur!=traceMap.end())&&(cur->second.eaddr>addr))
    //      addr=cur->second.baddr;
    VAddr segBegAddr=getSegmentAddr(addr);
    VAddr segEndAddr=segBegAddr+getSegmentSize(segBegAddr);
    VAddr funcBegAddr=getFuncAddr(addr);
    VAddr funcEndAddr=funcBegAddr+getFuncSize(funcBegAddr);
    if((addr<segBegAddr)||(addr>=segEndAddr))
      fail("createTrace: addr not within its segment\n");
    if((addr<funcBegAddr)||(addr>=funcEndAddr))
      fail("createTrace: addr not within its function\n");
    if((segBegAddr>funcBegAddr)||(segEndAddr<funcEndAddr))
      fail("createTrace: func not within its segment\n");
    decodeTrace(context,getFuncAddr(addr),getFuncSize(addr));
  }
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
    PageDesc &myPage=getPageDesc(pageNum);
    I(myPage.canRead);
    return *(reinterpret_cast<T *>(myPage.frame->getData(addr)));
  }
  template<class T>
  inline bool readMemRaw(VAddr addr, T &val){
    size_t pageNum=getPageNum(addr);
    I(addr+sizeof(T)<=(pageNum+1)*AddrSpacPageSize);
    PageDesc &myPage=getPageDesc(pageNum);
    if(!myPage.canRead)
      return false;
    val=*(reinterpret_cast<T *>(myPage.frame->getData(addr)));
    return true;
  }
  template<class T>
  inline bool writeMemRaw(VAddr addr, const T &val){
    size_t pageNum=getPageNum(addr);
    I(addr+sizeof(T)<=(pageNum+1)*AddrSpacPageSize);
    PageDesc &myPage=getPageDesc(pageNum);
    if(!myPage.canWrite)
      return false;
    if(myPage.copyOnWrite){
      myPage.copyOnWrite=false;
      if(myPage.frame->getRefCount()>1)
	myPage.frame=new FrameDesc(*myPage.frame);
    }
    *(reinterpret_cast<T *>(myPage.frame->getData(addr)))=val;
    return true;
  }
#if (defined HAS_MEM_STATE)
  inline const MemState &getState(VAddr addr) const{
    size_t pageNum=getPageNum(addr);
    I(addr<(pageNum+1)*AddrSpacPageSize);
    const PageDesc &myPage=getPageDesc(pageNum);
    return myPage.frame->getState(addr);
  }
  inline MemState &getState(VAddr addr){
    size_t pageNum=getPageNum(addr);             
    I(addr<(pageNum+1)*AddrSpacPageSize);                         
    PageDesc &myPage=getPageDesc(pageNum);
    if(myPage.copyOnWrite){
      myPage.copyOnWrite=false;
      if(myPage.frame->getRefCount()>1)
	myPage.frame=new FrameDesc(*myPage.frame);
    }
    return myPage.frame->getState(addr);
  }
#endif
  //
  // Mapping of function names to code addresses
  //
 private:
  struct ltstr{
    bool operator()(const char *s1, const char *s2) const{
      return (strcmp(s1,s2)<0);
    }
  };
  typedef std::multimap<VAddr,const char *,std::greater<VAddr> > AddrToNameMap;
  AddrToNameMap funcAddrToName;
  typedef std::multimap<const char *,VAddr,ltstr> NameToAddrMap;
  NameToAddrMap funcNameToAddr;
 public:
  // Add a new function name-address mapping
  void addFuncName(const char *name, VAddr addr);
  // Return name of the function with given entry point
  const char *getFuncName(VAddr addr) const;
  // Given the name, return where the function begins
  VAddr getFuncAddr(const char *name) const;
  // Given a code address, return where the function begins (best guess)
  VAddr getFuncAddr(VAddr addr) const;
  // Given a code address, return the function size (best guess)
  size_t getFuncSize(VAddr addr) const;
  // Print name(s) of function(s) with given entry point
  void printFuncName(VAddr addr);
  //
  // Interception of function calls
  //
 private:
  typedef std::multimap<const char *,EmulFunc *,ltstr> NameToFuncMap;
  static NameToFuncMap nameToCallHandler;
  static NameToFuncMap nameToRetHandler;
 public:
  static void addCallHandler(const char *name,EmulFunc *func);
  static void addRetHandler(const char *name,EmulFunc *func);
  static void delCallHandler(const char *name,EmulFunc *func);
  static void delRetHandler(const char *name,EmulFunc *func);
  bool hasCallHandler(VAddr addr) const{
    AddrToNameMap::const_iterator nameBeg=funcAddrToName.lower_bound(addr);
    AddrToNameMap::const_iterator nameEnd=funcAddrToName.upper_bound(addr);
    for(AddrToNameMap::const_iterator nameCur=nameBeg;nameCur!=nameEnd;nameCur++){
      const char *name=nameCur->second;
      I(name);
      NameToFuncMap::const_iterator handBeg=nameToCallHandler.lower_bound(name);
      NameToFuncMap::const_iterator handEnd=nameToCallHandler.upper_bound(name);
      if(handBeg!=handEnd)
        return true;
    }
    return false;
  }
  bool hasRetHandler(VAddr addr) const{
    AddrToNameMap::const_iterator nameBeg=funcAddrToName.lower_bound(addr);
    AddrToNameMap::const_iterator nameEnd=funcAddrToName.upper_bound(addr);
    for(AddrToNameMap::const_iterator nameCur=nameBeg;nameCur!=nameEnd;nameCur++){
      const char *name=nameCur->second;
      I(name);
      NameToFuncMap::const_iterator handBeg=nameToRetHandler.lower_bound(name);
      NameToFuncMap::const_iterator handEnd=nameToRetHandler.upper_bound(name);
      if(handBeg!=handEnd)
        return true;
    }
    return false;
  }
  typedef std::set<EmulFunc *> HandlerSet;
  void getCallHandlers(VAddr addr, HandlerSet &set) const;
  void getRetHandlers(VAddr addr, HandlerSet &set) const;
};
#endif
