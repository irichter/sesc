#include <sys/mman.h>
#include "AddressSpace.h"
#include "nanassert.h"

AddressSpace::FrameDesc::FrameDesc() : GCObject(), cpOnWrCount(0){
  memset(data,0,AddrSpacPageSize);
  addRef();
}
AddressSpace::FrameDesc::FrameDesc(FrameDesc &src) : GCObject(), cpOnWrCount(0){
  memcpy(data,src.data,AddrSpacPageSize);
  addRef();
  I(src.getRefCount()>1);
  I(src.getCpOnWrCount()>0);
  src.delRef();
  I(getRefCount()==1);
}
AddressSpace::FrameDesc::~FrameDesc(){
  I(getRefCount()==0);
  I(cpOnWrCount==0);
  memset(data,0xCC,AddrSpacPageSize);
}

AddressSpace::PageDesc::PageDesc &AddressSpace::PageDesc::operator=(const PageDesc &src){
  shared=src.shared;
  frame=src.frame;
  if(frame){
    frame->addRef();
    if(!shared)
      frame->addCpOnWr();
  }
  canRead =src.canRead;
  canWrite=src.canWrite;
  canExec =src.canExec;
  insts=src.insts;
  if(insts){
    insts->addRef();
  }
  return *this;
}

AddressSpace::AddressSpace(void) :
  brkBase(0), refCount(0)
{
#if (defined SPLIT_PAGE_TABLE)
#else
  I(sizeof(pageMap)==AddrSpacPageNumCount*sizeof(PageDesc));
#endif
}  

AddressSpace::AddressSpace(AddressSpace &src) :
  segmentMap(src.segmentMap),
  brkBase(src.brkBase),
  //  openFiles(src.openFiles),
  refCount(0),
  funcAddrToName(src.funcAddrToName)
{
  // Copy page map and pages
#if (defined SPLIT_PAGE_TABLE)
  I(0);
#else
  I(sizeof(pageMap)==AddrSpacPageNumCount*sizeof(PageDesc));
  for(size_t i=0;i<AddrSpacPageNumCount;i++){
    PageDesc &srcPage=src.pageMap[i];
    PageDesc &dstPage=pageMap[i];
    dstPage=srcPage;
  }
#endif
  // Copy function name mappings
  for(AddrToNameMap::iterator addrIt=funcAddrToName.begin();
      addrIt!=funcAddrToName.end();addrIt++){
    addrIt->second=strdup(addrIt->second);
  }
}

void AddressSpace::removePage(PageDesc &pageDesc, size_t pageNum){
  I(!pageDesc.canRead);
  I(!pageDesc.canWrite);
  I(!pageDesc.canExec);
  if(pageDesc.insts){
    I(pageDesc.insts->getRefCount()>0);
    pageDesc.insts->delRef();
    pageDesc.insts=0;
  }
  if(pageDesc.frame){
    I(pageDesc.frame->getRefCount()>0);
    pageDesc.frame->delRef();
    pageDesc.frame=0;
  }
}

void AddressSpace::clear(bool isExec){
  // Clear function name mappings
  while(!funcAddrToName.empty()){
    free(funcAddrToName.begin()->second);
    funcAddrToName.erase(funcAddrToName.begin());
  }
  // Remove all memory segments
  VAddr lastAddr=segmentMap.begin()->second.addr+segmentMap.begin()->second.len;
  while(!segmentMap.empty()){
    SegmentDesc &mySeg=segmentMap.begin()->second;
    I(mySeg.addr+mySeg.len<=lastAddr);
    I(mySeg.len>0);
    I(mySeg.addr==segmentMap.begin()->first);
    lastAddr=mySeg.addr;
    delPages(mySeg.pageNumLb,mySeg.pageNumUb,mySeg.canRead,mySeg.canWrite,mySeg.canExec);
    segmentMap.erase(mySeg.addr);
  }
  // Remove all pages
#if (defined SPLIT_PAGE_TABLE)
  for(size_t rootPageNum=0;rootPageNum<AddrSpacRootSize;rootPageNum++){
    PageMapLeaf pageMapLeaf=pageMapRoot[rootNum];
    if(pageMap){
      for(size_t leafPageNum=0;leafPageNum<AddrSpacLeafSize;leafPageNum++){
	removePage(pageMapLeaf[leafPageNum],(rootPageNum<<AddrSpacLeafBits)|leafPageNum);
      }
      delete [] pageMapLeaf;
    }
  }
#else
  for(size_t pageNum=0;pageNum<AddrSpacPageNumCount;pageNum++){
    I(!pageMap[pageNum].canRead);
    I(!pageMap[pageNum].canWrite);
    I(!pageMap[pageNum].canExec);
    I(!pageMap[pageNum].frame);
    I(!pageMap[pageNum].insts);
  }
#endif
//   // Close files that are still open
//   for(FileDescMap::iterator fileIt=openFiles.begin();fileIt!=openFiles.end();fileIt++){
//     if(fileIt->fd==-1)
//       continue;
//     // If this clear is due to an exec call, only close cloexec files
//     if(isExec&&!fileIt->cloexec)
//       continue;
//     int cret=close(fileIt->fd);
//     I(!cret);
//     fileIt->fd=-1;
//   }
}

AddressSpace::~AddressSpace(void){
  printf("Destroying an address space\n");
  clear(false);
}

void AddressSpace::addReference(){
  refCount++;
}

void AddressSpace::delReference(void){
  I(refCount!=0);
  refCount--;
  if(!refCount)
    delete this;
}

// Add a new function name-address mapping
void AddressSpace::addFuncName(const char *name, VAddr addr){
  char *myName=strdup(name);
  I(myName);
  funcAddrToName.insert(AddrToNameMap::value_type(addr,myName));
}

// Return name of the function with given entry point
const char *AddressSpace::getFuncName(VAddr addr) const{
  AddrToNameMap::const_iterator nameIt=funcAddrToName.find(addr);
  if(nameIt->first!=addr)
    return 0;
  return nameIt->second;
}


// Given a code address, return where the function begins (best guess)
VAddr AddressSpace::getFuncAddr(VAddr addr) const{
  AddrToNameMap::const_iterator nameIt=funcAddrToName.upper_bound(addr);
  if(nameIt==funcAddrToName.begin())
    return 0;
  nameIt--;
  return nameIt->first;
}

// Given a code address, return the function size (best guess)
size_t AddressSpace::getFuncSize(VAddr addr) const{
  return 0;
}

// Print name(s) of function(s) with given entry point
void AddressSpace::printFuncName(VAddr addr){
  AddrToNameMap::iterator nameBegIt=funcAddrToName.lower_bound(addr);
  AddrToNameMap::iterator nameEndIt=funcAddrToName.upper_bound(addr);
  while(nameBegIt!=nameEndIt){
    printf("%s",nameBegIt->second);
    nameBegIt++;
    if(nameBegIt!=nameEndIt)
      printf(", ");
  }
}

VAddr AddressSpace::newSegmentAddr(size_t len){
  VAddr retVal=alignDown((VAddr)-1,AddrSpacPageSize);
  retVal=alignDown(retVal-len,AddrSpacPageSize);
  for(SegmentMap::const_iterator segIt=segmentMap.begin();segIt!=segmentMap.end();segIt++){
    const SegmentDesc &curSeg=segIt->second;
    if(retVal>=curSeg.addr+curSeg.len)
      break;
    if(alignUp(len+AddrSpacPageSize,AddrSpacPageSize)>curSeg.addr){
      retVal=0;
    }else{
      retVal=alignDown(curSeg.addr-len,AddrSpacPageSize);
    }
  }
  return retVal;
}
void AddressSpace::splitSegment(VAddr pivot){
  // Find segment that begins below pivot - this will be the one to split
  SegmentMap::iterator segIt=segmentMap.upper_bound(pivot);
  // Return if no such segment
  if(segIt==segmentMap.end())
    return;
  SegmentDesc &oldSeg=segIt->second;
  // If segment ends at or below pivot, no split needed
  if(oldSeg.addr+oldSeg.len<=pivot)
    return;
  SegmentDesc &newSeg=segmentMap[pivot];
  newSeg.setAddr(pivot);
  newSeg.setLen(oldSeg.addr+oldSeg.len-newSeg.addr);
  newSeg.copyFlags(oldSeg);
  oldSeg.setLen(oldSeg.len-newSeg.len);
  if(newSeg.pageNumLb<oldSeg.pageNumUb){
    I(newSeg.pageNumLb+1==oldSeg.pageNumUb);
    addPages(newSeg.pageNumLb,newSeg.pageNumUb,newSeg.canRead,newSeg.canWrite,newSeg.canExec);
  }
}
void AddressSpace::growSegmentDown(VAddr oldaddr, VAddr newaddr){
  I(segmentMap.find(oldaddr)!=segmentMap.end());
  I(newaddr<oldaddr);
  SegmentDesc &oldSeg=segmentMap[oldaddr];
  I(isNoSegment(newaddr,oldaddr-newaddr));
  SegmentDesc &newSeg=segmentMap[newaddr];
  newSeg.setAddr(newaddr);
  newSeg.setLen(oldaddr+oldSeg.len-newaddr);
  newSeg.copyFlags(oldSeg);
  I(newSeg.pageNumUb==oldSeg.pageNumUb);
  if(newSeg.pageNumLb<oldSeg.pageNumLb)
    addPages(newSeg.pageNumLb,oldSeg.pageNumLb,newSeg.canRead,newSeg.canWrite,newSeg.canExec);
  segmentMap.erase(oldaddr);
}
void AddressSpace::resizeSegment(VAddr addr, size_t newlen){
  I(segmentMap.find(addr)!=segmentMap.end());
  I(newlen>0);
  SegmentDesc &mySeg=segmentMap[addr];
  I(mySeg.addr==addr);
  size_t oldPageNumUb=mySeg.pageNumUb;
  mySeg.setLen(newlen);
  size_t newPageNumUb=mySeg.pageNumUb;
  if(oldPageNumUb<newPageNumUb){
    addPages(oldPageNumUb,newPageNumUb,mySeg.canRead,mySeg.canWrite,mySeg.canExec);
  }else if(oldPageNumUb>newPageNumUb){
    delPages(newPageNumUb,oldPageNumUb,mySeg.canRead,mySeg.canWrite,mySeg.canExec);
  }
}
void AddressSpace::moveSegment(VAddr oldaddr, VAddr newaddr){
  I(segmentMap.find(oldaddr)!=segmentMap.end());
  SegmentDesc &oldSeg=segmentMap[oldaddr];
  I(isNoSegment(newaddr,oldSeg.len));
  SegmentDesc &newSeg=segmentMap[newaddr];
  newSeg.setAddr(newaddr);
  newSeg.setLen(oldSeg.len);
  newSeg.copyFlags(oldSeg);
  size_t oldPg=oldSeg.pageNumLb;
  size_t newPg=newSeg.pageNumLb;
  while(oldPg<oldSeg.pageNumUb){
    I(newPg<newSeg.pageNumUb);
    PageDesc &oldPageDesc=getPageDesc(oldPg);
    PageDesc &newPageDesc=getPageDesc(newPg);
    newPageDesc.frame=oldPageDesc.frame;
    oldPageDesc.frame=0;
    I(newPageDesc.canRead==0);
    I(oldPageDesc.canRead==(oldSeg.canRead?1:0));
    newPageDesc.canRead=oldPageDesc.canRead;
    oldPageDesc.canRead=0;
    I(newPageDesc.canWrite==0);
    I(oldPageDesc.canWrite==(oldSeg.canWrite?1:0));
    newPageDesc.canWrite=oldPageDesc.canWrite;
    oldPageDesc.canWrite=0;
    I(!oldPageDesc.insts);
    I(oldPageDesc.canExec==0);
    I(newPageDesc.canExec==0);
    I(oldPageDesc.canExec==(oldSeg.canExec?1:0));
    newPageDesc.canExec=oldPageDesc.canExec;
    oldPageDesc.canExec=0;
    oldPg++;
    newPg++;
  }
  I(newPg==newSeg.pageNumUb);
  segmentMap.erase(oldaddr);
}
void AddressSpace::deleteSegment(VAddr addr, size_t len){
  while(true){
    SegmentMap::iterator segIt=segmentMap.upper_bound(addr+len);
    if(segIt==segmentMap.end())
      break;
    SegmentDesc &oldSeg=segIt->second;
    if(oldSeg.addr+oldSeg.len<=addr)
      break;
    if(oldSeg.addr+oldSeg.len>addr+len){
      I(0);
      SegmentDesc &newSeg=segmentMap[addr+len];
      newSeg.setAddr(addr+len);
      newSeg.setLen(oldSeg.addr+oldSeg.len-newSeg.addr);
      newSeg.copyFlags(oldSeg);
      oldSeg.setLen(oldSeg.len-newSeg.len);
      if(newSeg.pageNumLb<oldSeg.pageNumUb){
	I(newSeg.pageNumLb+1==oldSeg.pageNumUb);
	addPages(newSeg.pageNumLb,newSeg.pageNumUb,newSeg.canRead,newSeg.canWrite,newSeg.canExec);
      }
    }else if(oldSeg.addr<addr){
      size_t oldPageNumUb=oldSeg.pageNumUb;
      oldSeg.setLen(addr-oldSeg.addr);
      if(oldSeg.pageNumUb<oldPageNumUb){
	delPages(oldSeg.pageNumUb,oldPageNumUb,oldSeg.canRead,oldSeg.canWrite,oldSeg.canExec);
      }
    }else{
      I(oldSeg.addr>=addr);
      I(oldSeg.addr+oldSeg.len<=addr+len);
      if(oldSeg.len>0){
	I(oldSeg.pageNumLb<oldSeg.pageNumUb);
	delPages(oldSeg.pageNumLb,oldSeg.pageNumUb,oldSeg.canRead,oldSeg.canWrite,oldSeg.canExec);
      }
      segmentMap.erase(segIt);
    }
  }
}
void AddressSpace::newSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec, bool shared, bool fileMap){
  I(len>0);
  I(isNoSegment(addr,len));
  SegmentDesc &newSeg=segmentMap[addr];
  newSeg.setAddr(addr);
  newSeg.setLen(len);
  newSeg.canRead=canRead;
  newSeg.canWrite=canWrite;
  newSeg.canExec=canExec;
  newSeg.autoGrow=false;
  newSeg.growDown=false;
  I(!shared);
  newSeg.shared=shared;
  newSeg.fileMap=fileMap;
  I(!fileMap);
  if(len>0){
    I(newSeg.pageNumLb<newSeg.pageNumUb);
    addPages(newSeg.pageNumLb,newSeg.pageNumUb,canRead,canWrite,canExec);
  }
}
void AddressSpace::protectSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec){
  splitSegment(addr);
  splitSegment(addr+len);
  I(isSegment(addr,len));
  SegmentDesc &mySeg=segmentMap[addr];
  I(mySeg.addr==addr);
  I(mySeg.len==len);
  bool addCanRead=(canRead&&!mySeg.canRead);
  bool addCanWrite=(canWrite&&!mySeg.canWrite);
  bool addCanExec=(canExec&&!mySeg.canExec);
  if(addCanRead||addCanWrite||addCanExec)
    addPages(mySeg.pageNumLb,mySeg.pageNumUb,addCanRead,addCanWrite,addCanExec);
  bool delCanRead =(mySeg.canRead&&!canRead);
  bool delCanWrite=(mySeg.canWrite&&!canWrite);
  bool delCanExec =(mySeg.canExec&&!canExec);
  if(delCanRead||delCanWrite||delCanExec)
    delPages(mySeg.pageNumLb,mySeg.pageNumUb,delCanRead,delCanWrite,delCanExec);
  mySeg.canRead=canRead;
  mySeg.canWrite=canWrite;
  mySeg.canExec=canExec;
}

void AddressSpace::addPages(size_t pageNumLb, size_t pageNumUb, bool canRead, bool canWrite, bool canExec){
  for(size_t pageNum=pageNumLb;pageNum<pageNumUb;pageNum++){
    PageDesc &pageDesc=getPageDesc(pageNum);
    if((!pageDesc.frame)&&(canRead||canWrite||canExec))
      pageDesc.frame=new FrameDesc();
    if((!pageDesc.insts)&&canExec)
      pageDesc.insts=new TraceDesc(pageNum*AddrSpacPageSize);
    if(canRead)
      pageDesc.canRead++;
    if(canWrite)
      pageDesc.canWrite++;
    if(canExec)
      pageDesc.canExec++;
  }
}
void AddressSpace::delPages(size_t pageNumLb, size_t pageNumUb, bool canRead, bool canWrite, bool canExec){
  for(size_t pageNum=pageNumLb;pageNum<pageNumUb;pageNum++){
    PageDesc &pageDesc=getPageDesc(pageNum);
    if(canRead){
      I(pageDesc.canRead>0);
      pageDesc.canRead--;
    }
    if(canWrite){
      I(pageDesc.canWrite>0);
      pageDesc.canWrite--;
    }
    if(canExec){
      I(pageDesc.canExec>0);
      pageDesc.canExec--;
    }
    if((!pageDesc.canRead)&&(!pageDesc.canWrite)&&(!pageDesc.canExec))
      removePage(pageDesc,pageNum);
  }
}

// int AddressSpace::getRealFd(int simfd) const{
//   if(openFiles.size()<=(size_t)simfd)
//     return -1;
//   return openFiles[simfd].fd;
// }
// int  AddressSpace::newSimFd(int realfd){
//   int retVal=0;
//   while((size_t)retVal<openFiles.size()){
//     if(openFiles[retVal].fd==-1)
//       break;
//     I(openFiles[retVal].fd!=realfd);
//     retVal++;
//   }
//   newSimFd(realfd,retVal);
//   return retVal;
// }
// void AddressSpace::newSimFd(int realfd, int simfd){
//   I(getRealFd(simfd)==-1);
//   if((size_t)simfd>=openFiles.size())
//     openFiles.resize(simfd+1);
//   openFiles[simfd].fd=realfd;
//   openFiles[simfd].cloexec=false;
// }
// void AddressSpace::delSimFd(int simfd){
//   I(getRealFd(simfd)!=-1);
//   openFiles[simfd].fd=-1;
// }
// bool AddressSpace::getCloexec(int simfd) const{
//   I(openFiles.size()>(size_t)simfd);
//   I(openFiles[simfd].fd!=-1);
//   return openFiles[simfd].cloexec;
// }
// void AddressSpace::setCloexec(int simfd, bool cloexec){
//   I(openFiles.size()>(size_t)simfd);
//   I(openFiles[simfd].fd!=-1);
//   openFiles[simfd].cloexec=cloexec;
// }
