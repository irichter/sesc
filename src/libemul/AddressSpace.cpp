#include <sys/mman.h>
#include "AddressSpace.h"
#include "nanassert.h"

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
  brkBase(src.brkBase)
{
  // Copy page map and pages
  // Copy function name mappings
  // Copy open file descriptors
}

AddressSpace::~AddressSpace(void){
  // Free function name strings  
  for(AddrToNameMap::iterator funcIt=funcAddrToName.begin();funcIt!=funcAddrToName.end();funcIt++)  
    free(funcIt->second);
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
#if (defined SPLIT_PAGE_TABLE)
  I(0);
#else
  for(size_t pg=0;pg<AddrSpacPageNumCount;pg++){
    PageDesc &myPage=pageMap[pg];
    I(!myPage.canRead);
    I(!myPage.canWrite);
    I(!myPage.canExec);
    delete [] myPage.inst;
    delete [] myPage.data;
  }
#endif
}

void AddressSpace::addReference(){
  refCount++;
}

void AddressSpace::delReference(void){
  I(refCount!=0);
  refCount--;
}

// Add a new function name-address mapping
void AddressSpace::addFuncName(const char *name, VAddr addr){
  char *myName=strdup(name);
  I(myName);
  I(funcNameToAddr.find(myName)==funcNameToAddr.end());
  funcNameToAddr.insert(NameToAddrMap::value_type(myName,addr));
  funcAddrToName.insert(AddrToNameMap::value_type(addr,myName));
}

// Return name of the function with given entry point
const char *AddressSpace::getFuncName(VAddr addr){
  AddrToNameMap::iterator nameIt=funcAddrToName.lower_bound(addr);
  if(nameIt->first!=addr)
    return "";
  return nameIt->second;
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
    newPageDesc.data=oldPageDesc.data;
    oldPageDesc.data=0;
    I(newPageDesc.canRead==0);
    I(oldPageDesc.canRead==(oldSeg.canRead?1:0));
    newPageDesc.canRead=oldPageDesc.canRead;
    oldPageDesc.canRead=0;
    I(newPageDesc.canWrite==0);
    I(oldPageDesc.canWrite==(oldSeg.canWrite?1:0));
    newPageDesc.canWrite=oldPageDesc.canWrite;
    oldPageDesc.canWrite=0;
    I(!oldPageDesc.inst);
    I(oldPageDesc.canExec==0);
    I(newPageDesc.canExec==0);
    I(oldPageDesc.canExec==(oldSeg.canExec?1:0));
    newPageDesc.canExec=oldPageDesc.canExec;
    oldPageDesc.canExec=0;
    oldPg++;
    newPg++;
  }
  I(newPg==newSeg.pageNumUb);
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
  newSeg.shared=shared;
  newSeg.fileMap=fileMap;
  I(!fileMap);
  if(len>0){
    I(newSeg.pageNumLb<newSeg.pageNumUb);
    addPages(newSeg.pageNumLb,newSeg.pageNumUb,canRead,canWrite,canExec);
  }
}
void AddressSpace::protectSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec){
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
    if((!pageDesc.data)&&(canRead||canWrite||canExec)){
      pageDesc.data=reinterpret_cast<uint8_t *>(new uint64_t[AddrSpacPageSize/sizeof(uint64_t)]);
    }
    if((!pageDesc.inst)&&canExec){
      pageDesc.inst=new InstDesc[AddrSpacPageSize];
      for(size_t pageOffs=0;pageOffs<AddrSpacPageSize;pageOffs++)
	pageDesc.inst[pageOffs].addr=pageNum*AddrSpacPageSize+pageOffs;
    }
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
    // TODO: Remove pages that have no remaining segment overlap
  }
}

int AddressSpace::getRealFd(int simfd) const{
  if(openFiles.size()<=(size_t)simfd)
    return -1;
  return openFiles[simfd].fd;
}
int  AddressSpace::newSimFd(int realfd){
  int retVal=0;
  while((size_t)retVal<openFiles.size()){
    if(openFiles[retVal].fd==-1)
      break;
    I(openFiles[retVal].fd!=realfd);
    retVal++;
  }
  newSimFd(realfd,retVal);
  return retVal;
}
void AddressSpace::newSimFd(int realfd, int simfd){
  I(getRealFd(simfd)==-1);
  if((size_t)simfd>=openFiles.size())
    openFiles.resize(simfd+1);
  openFiles[simfd].fd=realfd;
  openFiles[simfd].cloexec=false;
}
void AddressSpace::delSimFd(int simfd){
  I(getRealFd(simfd)!=-1);
  openFiles[simfd].fd=-1;
}
bool AddressSpace::getCloexec(int simfd) const{
  I(openFiles.size()>(size_t)simfd);
  I(openFiles[simfd].fd!=-1);
  return openFiles[simfd].cloexec;
}
void AddressSpace::setCloexec(int simfd, bool cloexec){
  I(openFiles.size()>(size_t)simfd);
  I(openFiles[simfd].fd!=-1);
  openFiles[simfd].cloexec=cloexec;
}
