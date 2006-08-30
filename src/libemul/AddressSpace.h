#if !(defined ADDRESS_SPACE_H)
#define ADDRESS_SPACE_H

#include <unistd.h>
#include <vector>
#include <map>
#include "Addressing.h"
#include "CvtEndian.h"
#include "InstDesc.h"
#include "common.h"
#include "nanassert.h"

#define AddrSpacPageOffsBits (12)
#define AddrSpacPageSize     (1<<AddrSpacPageOffsBits)
#define AddrSpacPageOffsMask (AddrSpacPageSize-1)
#define AddrSpacPageNumBits  (sizeof(VAddr)*8-AddrSpacPageOffsBits)
#define AddrSpacPageNumCount (1<<AddrSpacPageNumBits)
//#define SPLIT_PAGE_TABLE
#if (defined SPLIT_PAGE_TABLE)
#define AddrSpacRootBits (AddrSpacPageNumBits/2)
#define AddrSpacLeafBits (AddrSpacPageNumBits-AddrSpacRootBits)
#define AddrSpacLeafSize (1<<AddrSpacLeafBits)
#define AddrSpacLeafMask (AddrSpacLeafSize-1)
#endif

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
  struct PageDesc{
    uint8_t   *data;
    InstDesc  *inst;
    // Number of can-read segments that overlap with this page
    size_t     canRead;
    // Number of can-write segments that overlap with this page
    size_t     canWrite;
    // Number of can-exec segments that overlap with this page
    size_t     canExec;
    PageDesc(void) : data(0), inst(0), canRead(0), canWrite(0), canExec(0) { }
  };
#if (defined SPLIT_PAGE_TABLE)
  typedef PageDesc *PageMapLeaf;
  PageMapLeaf pageMapRoot[1<<AddrSpacRootBits];
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
  VAddr brkBase;
 public:
  static inline size_t getPageSize(void){
    return AddrSpacPageSize;
  }
  // Returns true iff the specified block does not ovelap with any allocated segment
  bool isNoSegment(VAddr addr, size_t len) const{
    SegmentMap::const_iterator segIt=segmentMap.upper_bound(addr+len);
    return (segIt==segmentMap.end())||(segIt->second.addr+segIt->second.len<=addr);
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
    I(isNoSegment(addr,sizeof(uint64_t)));
    brkBase=addr;
    newSegment(brkBase,sizeof(uint64_t),true,true,false);
    uint64_t val=0;
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
    RAddr  frameBase=leafTable?((RAddr)(leafTable[leafNum].data)):0;
#else
    RAddr  frameBase=(RAddr)(pageMap[pageNum].data);
#endif
    return frameBase+(frameBase!=0)*pageOffs;
  }
  inline InstDesc *virtToInst(VAddr addr) const{
    size_t pageOffs=(addr&AddrSpacPageOffsMask);
    size_t pageNum=(addr>>AddrSpacPageOffsBits);
    //    I(addr==pageNum*PageSize+pageOffs);
#if (defined SPLIT_PAGE_TABLE)
    size_t rootNum=(pageNum>>AddrSpacLeafBits);
    size_t leafNum=(pageNum&AddrSpacLeafMask);
    PageMapLeaf leafTable=pageMapRoot[rootNum];
    InstDesc *frameBase=leafTable?(leafTable[leafNum].inst):0;
#else
    InstDesc *frameBase=pageMap[pageNum].inst;
#endif
    return (frameBase==0)?0:frameBase+pageOffs;
  }
 public:
  struct FileDesc{
    // Real open file descriptor
    int fd;
    // Close-on-exec flag, false by default
    bool cloexec;
    FileDesc(void) : fd(-1){ }
  };
  typedef std::vector<FileDesc> FileDescMap;
  FileDescMap openFiles;
  int  getRealFd(int simfd) const;
  int  newSimFd(int realfd);
  void newSimFd(int realfd, int simfd);
  void delSimFd(int simfd);
  bool getCloexec(int simfd) const;
  void setCloexec(int simfd, bool cloexec);
  AddressSpace(void);
  AddressSpace(AddressSpace &src);
  ~AddressSpace(void);
  void addReference();
  void delReference(void);
  template<class T>
  inline bool readMemRaw(VAddr addr, T &val){
    PageDesc &myPage=getPageDesc(getPageNum(addr));
    if(!myPage.canRead)
      return false;
    val=*(reinterpret_cast<T *>(myPage.data+getPageOff(addr)));
    return true;
  }
  template<class T>
  inline bool writeMemRaw(VAddr addr, const T &val){
    PageDesc &myPage=getPageDesc(getPageNum(addr));
    if(!myPage.canWrite)
      return false;
    *(reinterpret_cast<T *>(myPage.data+getPageOff(addr)))=val;
    return true;
  }
 private:
  // Number of threads that are using this address space
  size_t refCount;
 private:
  // Mapping of function names to code addresses
  typedef std::map<char *,VAddr> NameToAddrMap;
  typedef std::multimap<VAddr,char *> AddrToNameMap;
  NameToAddrMap funcNameToAddr;
  AddrToNameMap funcAddrToName;
 public:
  // Add a new function name-address mapping
  void addFuncName(const char *name, VAddr addr);
  // Return name of the function with given entry point
  const char *getFuncName(VAddr addr);
  // Print name(s) of function(s) with given entry point
  void printFuncName(VAddr addr);
};

#endif
