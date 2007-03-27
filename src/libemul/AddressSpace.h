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

class AddressSpace{
  struct SegmentDesc{
    // Starting adress and length of the segment
    VAddr  addr;
    size_t len;
    // Page number (not address) of the first page that overlaps with this segment
    size_t pageNumLb;
    // Page number (not address) of the first page after this segment with no overlap with it
    size_t pageNumUb;
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
    void setAddr(VAddr newAddr){
      addr=newAddr;
      pageNumLb=(addr>>AddrSpacPageOffsBits);
    }
    void setLen(size_t newLen){
      len=newLen;
      pageNumUb=((addr+len+AddrSpacPageSize-1)>>AddrSpacPageOffsBits);
    }
    void copyFlags(const SegmentDesc &src){
      canRead =src.canRead;
      canWrite=src.canWrite;
      canExec =src.canExec;
      autoGrow=src.autoGrow;
      growDown=src.growDown;
      shared=src.shared;
      fileMap=src.fileMap;
    }
  };
  typedef std::map<VAddr,SegmentDesc,std::greater<VAddr> > SegmentMap;
  SegmentMap segmentMap;
  // Information about pages of physical memory
  class FrameDesc : public GCObject{
  private:
    MemAlignType data[AddrSpacPageSize/sizeof(MemAlignType)];
    size_t cpOnWrCount;
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
    void addCpOnWr(void){
      cpOnWrCount++;
    }
    size_t getCpOnWrCount(void) const{
      return cpOnWrCount;
    }
    void delRef(void){
      I(getRefCount()>0);
      I(getRefCount()==cpOnWrCount+1);
      if(cpOnWrCount)
	cpOnWrCount--;
      GCObject::delRef();
    }
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
  struct PageDesc{
    FrameDesc *frame;
    TraceDesc *insts;
    // Number of can-read segments that overlap with this page
    size_t     canRead;
    // Number of can-write segments that overlap with this page
    size_t     canWrite;
    // Number of can-exec segments that overlap with this page
    size_t     canExec;
    // Is this page shared
    bool shared;
    PageDesc(void) : frame(0), insts(0), canRead(0), canWrite(0), canExec(0), shared(false){ }
    PageDesc &operator=(const PageDesc &src);
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
  void addPages(size_t pageNumLb, size_t pageNumUb, bool canRead, bool canWrite, bool canExec);
  void delPages(size_t pageNumLb, size_t pageNumUb, bool canRead, bool canWrite, bool canExec);
  void removePage(PageDesc &pageDesc, size_t pageNum);
  VAddr brkBase;
 public:
  static inline size_t getPageSize(void){
    return AddrSpacPageSize;
  }
  // Returns true iff the specified block does not ovelap with any allocated segment
  bool isNoSegment(VAddr addr, size_t len) const{
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
  inline InstDesc *virtToInst(VAddr addr) const{
    size_t pageNum=(addr>>AddrSpacPageOffsBits);
    //    I(addr==pageNum*PageSize+pageOffs);
#if (defined SPLIT_PAGE_TABLE)
    size_t rootNum=(pageNum>>AddrSpacLeafBits);
    size_t leafNum=(pageNum&AddrSpacLeafMask);
    PageMapLeaf leafTable=pageMapRoot[rootNum];
    if(!leafTable)
      return 0;
    TraceDesc *insts=leafTable[leafNum].insts;
#else
    TraceDesc *insts=pageMap[pageNum].insts;
#endif
    if(!insts)
      return 0;
    return insts->getInst(addr);
  }
 public:

/*   struct FileDesc{ */
/*     // Real open file descriptor */
/*     int fd; */
/*     // Close-on-exec flag, false by default */
/*     bool cloexec; */
/*     FileDesc(void) : fd(-1){ } */
/*     FileDesc(const FileDesc &src) : fd(src.fd), cloexec(src.cloexec){ */
/*       if(fd!=-1){ */
/* 	fd=dup(fd); */
/* 	I(fd>=0); */
/*       } */
/*     } */
/*   }; */
/*   typedef std::vector<FileDesc> FileDescMap; */
/*   FileDescMap openFiles; */
/*   int  getRealFd(int simfd) const; */
/*   int  newSimFd(int realfd); */
/*   void newSimFd(int realfd, int simfd); */
/*   void delSimFd(int simfd); */
/*   bool getCloexec(int simfd) const; */
/*   void setCloexec(int simfd, bool cloexec); */

  AddressSpace(void);
  AddressSpace(AddressSpace &src);
  void clear(bool isExec);
  ~AddressSpace(void);
  void addReference();
  void delReference(void);
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
    I(myPage.frame->getCpOnWrCount()==myPage.frame->getRefCount()-1);
    if(myPage.frame->getCpOnWrCount())
      myPage.frame=new FrameDesc(*myPage.frame);
    I(!myPage.frame->getCpOnWrCount());
    I(myPage.frame->getCpOnWrCount()==myPage.frame->getRefCount()-1);    
    *(reinterpret_cast<T *>(myPage.frame->getData(addr)))=val;
    return true;
  }
 private:
  // Number of threads that are using this address space
  size_t refCount;

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
