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
#include "Checkpoint.h"

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
  // Information about pages of physical memory
  class FrameDesc : public GCObject{
  public:
    typedef SmartPtr<FrameDesc> pointer;
  private:
    MemAlignType data[AddrSpacPageSize/sizeof(MemAlignType)];
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
    void save(ChkWriter &out) const;
    FrameDesc(ChkReader &in);
  };
  // Information about decoded instructions for a page of virtual memory
  class TraceDesc : public GCObject{
  private:
    InstDesc inst[AddrSpacPageSize];
  public:
    TraceDesc(VAddr addr) : GCObject(){
      for(size_t pageOffs=0;pageOffs<AddrSpacPageSize;pageOffs++)
	inst[pageOffs].addr=addr+pageOffs;
      addRef();
    }
    InstDesc *getInst(VAddr addr){
      size_t offs=(addr&AddrSpacPageOffsMask);
      I(inst[offs].addr=addr);
      return &(inst[offs]);
    }
  };
  // Information about pages of virtual memory (in each AddressSpace)
  class PageDesc{
  public:
    FrameDesc::pointer frame;
    TraceDesc *insts;
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
      if(insts){
	I(insts->getRefCount()>0);
	insts->delRef();
	insts=0;
      }
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
  inline size_t getPageNum(VAddr addr){
    return (addr>>AddrSpacPageOffsBits);
  }
  inline size_t getPageOff(VAddr addr){
    return (addr&AddrSpacPageOffsMask);
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
  inline InstDesc *virtToInst(VAddr addr){
    size_t pageNum=(addr>>AddrSpacPageOffsBits);
#if (defined SPLIT_PAGE_TABLE)
    size_t rootNum=(pageNum>>AddrSpacLeafBits);
    size_t leafNum=(pageNum&AddrSpacLeafMask);
    PageMapLeaf leafTable=pageMapRoot[rootNum];
    if(!leafTable)
      return 0;
    PageDesc &page=leafTable[leafNum];
#else
    PageDesc &page=pageMap[pageNum];
#endif
    if(!page.insts){
      if(!page.canExec)
	return 0;
      page.insts=new TraceDesc(pageNum*AddrSpacPageSize);
      I(page.insts);
    }
    return page.insts->getInst(addr);
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

  //
  // Mapping of function names to code addresses
  //
 private:
  typedef std::multimap<VAddr,char *> AddrToNameMap;
  AddrToNameMap funcAddrToName;
 public:
  // Add a new function name-address mapping
  void addFuncName(const char *name, VAddr addr);
  // Return name of the function with given entry point
  const char *getFuncName(VAddr addr) const;
  // Given a code address, return where the function begins (best guess)
  VAddr getFuncAddr(VAddr addr) const;
  // Given a code address, return the function size (best guess)
  size_t getFuncSize(VAddr addr) const;
  // Print name(s) of function(s) with given entry point
  void printFuncName(VAddr addr);
};
#endif
