#include <sys/mman.h>
#include "AddressSpace.h"
#include "nanassert.h"

namespace MemSys{

  PAddr FrameDesc::nextPAddr=AddrSpacPageSize;

  FrameDesc::PAddrSet FrameDesc::freePAddrs;

  FrameDesc::FrameDesc() : GCObject(), basePAddr(newPAddr()), shared(false), dirty(false){
    memset(data,0,AddrSpacPageSize);
  }
  FrameDesc::FrameDesc(FileSys::FileStatus *fs, off_t offs)
    : GCObject()
    , basePAddr(newPAddr())
    , shared(true)
    , dirty(false)
    , filePtr(fs)
    , fileOff(offs){
    filePtr->mmap(this,data,AddrSpacPageSize,fileOff);
#if (defined HAS_MEM_STATE)
    for(size_t s=0;s<AddrSpacPageSize/MemState::Granularity;s++)
      state[s]=src.state[s];
#endif
  }
  FrameDesc::FrameDesc(FrameDesc &src)
    : GCObject()
    ,basePAddr(newPAddr())
    ,shared(false)
    ,dirty(false)
  {
    memcpy(data,src.data,AddrSpacPageSize);
#if (defined HAS_MEM_STATE)
    for(size_t s=0;s<AddrSpacPageSize/MemState::Granularity;s++)
      state[s]=src.state[s];
#endif
  }
  FrameDesc::~FrameDesc(){
    if(filePtr){
      if(dirty&&shared)
	filePtr->msync(this,data,AddrSpacPageSize,fileOff);
      filePtr->munmap(this,data,AddrSpacPageSize,fileOff);
      filePtr=0;
    }
    I(freePAddrs.find(basePAddr)==freePAddrs.end());
    freePAddrs.insert(basePAddr);
    memset(data,0xCC,AddrSpacPageSize);
  }
  void FrameDesc::save(ChkWriter &out) const{
    out.write(reinterpret_cast<const char *>(data),AddrSpacPageSize);
#if (defined HAS_MEM_STATE)
    for(size_t s=0;s<AddrSpacPageSize/MemState::Granularity;s++)
      state[s].save(out);
#endif
    out<<endl;
  }
  FrameDesc::FrameDesc(ChkReader &in){
    in.read(reinterpret_cast<char *>(data),AddrSpacPageSize);
#if (defined HAS_MEM_STATE)
    for(size_t s=0;s<AddrSpacPageSize/MemState::Granularity;s++)
      state[s]=MemState(in);
#endif
    in>>endl;
  }
}
void AddressSpace::SegmentDesc::save(ChkWriter &out) const{
  out << "Addr " << addr << " Len " << len;
  out << "R" << canRead;
  out << "W" << canWrite;
  out << "X" << canExec;
  out << "G" << autoGrow;
  out << "D" << growDown;
  out << "S" << shared;
  fileStatus->save(out);
  out << "Offs " << fileOffset;
  out << endl;
}
ChkReader &AddressSpace::SegmentDesc::operator=(ChkReader &in){
  in >> "Addr " >> addr >> " Len " >> len;
  in >> "R" >> canRead;
  in >> "W" >> canWrite;
  in >> "X" >> canExec;
  in >> "G" >> autoGrow;
  in >> "D" >> growDown;  
  in >> "S" >> shared;
  fileStatus=new FileSys::FileStatus(in);
  in >> "Offs " >> fileOffset;
  in >> endl;
  return in;
}

AddressSpace::PageDesc::PageDesc(void)
  : frame(0),
    mapCount(0),
    canRead(0),
    canWrite(0),
    canExec(0),
    shared(false),
    copyOnWrite(false){
}
AddressSpace::PageDesc::PageDesc(PageDesc &src)
  : frame(src.frame),
    mapCount(src.mapCount),
    canRead(src.canRead),
    canWrite(src.canWrite),
    canExec(src.canExec),
    shared(src.shared),
    copyOnWrite(false)
{
  fail("copy constructor\n");
  if(frame&&(!shared)){
    copyOnWrite=true;
    src.copyOnWrite=true;
  }
}
AddressSpace::PageDesc::PageDesc(const PageDesc &src)
  : frame(src.frame),
    mapCount(src.mapCount),
    canRead(src.canRead),
    canWrite(src.canWrite),
    canExec(src.canExec),
    shared(src.shared),
    copyOnWrite(src.copyOnWrite)
{
  if(frame&&(!shared))
    fail("const copy constructor needs to modify src\n");
}
AddressSpace::PageDesc::PageDesc &AddressSpace::PageDesc::operator=(PageDesc &src){
  shared=src.shared;
  frame=src.frame;
  if(frame&&(!shared)){
    copyOnWrite=true;
    src.copyOnWrite=true;
  }else{
    copyOnWrite=false;
  }
  mapCount=src.mapCount;
  canRead =src.canRead;
  canWrite=src.canWrite;
  canExec =src.canExec;
//  insts=0;
  return *this;
}
void AddressSpace::PageDesc::save(ChkWriter &out) const{
  out << "S" << shared << " C" << copyOnWrite << " ";
  out.writeobj(getFrame());
}
ChkReader &AddressSpace::PageDesc::operator=(ChkReader &in){
//  insts=0;
  mapCount=0;
  canRead=0;
  canWrite=0;
  canExec=0;
  in >> "S" >> shared >> " C" >> copyOnWrite >> " ";
  frame=in.readobj<MemSys::FrameDesc>();
  return in;
}

void AddressSpace::createTrace(ThreadContext *context, VAddr addr){
  I((!isMappedInst(addr))||!instMap[addr]);
  //  instMap[addr]=0;
  //    TraceMap::iterator cur=traceMap.upper_bound(addr);
  //    if((cur!=traceMap.end())&&(cur->second.eaddr>addr))
  //      addr=cur->second.baddr;
  VAddr segAddr=getSegmentAddr(addr);
  VAddr segSize=getSegmentSize(segAddr);
  VAddr funcAddr=getFuncAddr(addr);
  VAddr funcSize;
  if(funcAddr){
    funcSize=getFuncSize(funcAddr);
    if((addr<funcAddr)||(addr>=funcAddr+funcSize))
      fail("createTrace: addr not within its function\n");
    if((segAddr>funcAddr)||(segAddr+segSize<funcAddr+funcSize))
      fail("createTrace: func not within its segment\n");
  }else{
    funcAddr=addr;
    funcSize=0;
  }
  if((addr<segAddr)||(addr>=segAddr+segSize))
    fail("createTrace: addr not within its segment\n");
  decodeTrace(context,funcAddr,funcSize);
}
void AddressSpace::delMapInsts(VAddr begAddr, VAddr endAddr){
  // TODO: Check if any thread is pointing to one of these insts (should never happen, but we should check)
  InstMap::iterator begIt=instMap.lower_bound(begAddr);
  InstMap::iterator endIt=instMap.lower_bound(endAddr);
  instMap.erase(begIt,endIt);
}
void AddressSpace::mapTrace(InstDesc *binst, InstDesc *einst, VAddr baddr, VAddr eaddr){
  while(true){              
    TraceMap::iterator cur=traceMap.upper_bound(eaddr);
    if(cur==traceMap.end())                 
      break;
    I(cur->second.baddr!=0);                
    I(cur->second.eaddr!=0);               
    if(cur->second.eaddr<=baddr)
      break;
    I(cur->second.baddr>=baddr);
    I(cur->second.eaddr<=eaddr);
    ID(cur->second.baddr=0);       
    ID(cur->second.eaddr=0);
//     printf("Deleting a trace to map another!\n");
//     exit(1);
    delete [] cur->second.binst;       
    traceMap.erase(cur);
  }               
  InstMap::iterator begit=instMap.upper_bound(eaddr);
  InstMap::iterator endit=instMap.lower_bound(baddr);
  InstMap::iterator begit1=instMap.begin();
  InstMap::iterator endit1=instMap.end();
  for(InstMap::iterator instit=instMap.lower_bound(baddr);instit!=instMap.lower_bound(eaddr);instit++)
    I((instit->second>=binst)&&(instit->second<einst));
  TraceDesc &tdesc=traceMap[baddr];
  tdesc.binst=binst;
  tdesc.einst=einst;
  tdesc.baddr=baddr;
  tdesc.eaddr=eaddr;
}


AddressSpace::AddressSpace(void) :
  GCObject(),
  brkBase(0)
{
}

AddressSpace::AddressSpace(AddressSpace &src) :
  GCObject(),
  segmentMap(src.segmentMap),
  pageTable(src.pageTable),
  brkBase(src.brkBase)
{
  for(NamesByAddr::const_iterator it=src.namesByAddr.begin();it!=src.namesByAddr.end();it++)
    addFuncName(it->addr,it->func,it->file);
}

void AddressSpace::clear(bool isExec){
  // Clear function name mappings
  namesByAddr.clear();
  namesByName.clear();
  // Clear instruction decodings
  instMap.clear();
  for(TraceMap::iterator traceit=traceMap.begin();traceit!=traceMap.end();traceit++)
    delete [] traceit->second.binst;
  traceMap.clear();
  // Remove all memory segments
  VAddr lastAddr=segmentMap.begin()->second.addr+segmentMap.begin()->second.len;
  while(!segmentMap.empty()){
    SegmentDesc &mySeg=segmentMap.begin()->second;
    I(mySeg.addr+mySeg.len<=lastAddr);
    I(mySeg.len>0);
    I(mySeg.addr==segmentMap.begin()->first);
    lastAddr=mySeg.addr;
    for(size_t pageNum=mySeg.pageNumLb();pageNum<mySeg.pageNumUb();pageNum++)
      pageTable[pageNum].unmap(mySeg.canRead,mySeg.canWrite,mySeg.canExec);
    segmentMap.erase(mySeg.addr);
  }
  // Remove all pages
  pageTable.clear();
}

AddressSpace::~AddressSpace(void){
//  printf("Destroying an address space\n");
  clear(false);
}

void AddressSpace::save(ChkWriter &out) const{
  out << "BrkBase " << brkBase <<endl;
  // Page dump, dumps only pages that have any physical mapping
//   for(size_t _pageNum=0;_pageNum<AddrSpacPageNumCount;_pageNum++){
//     if(!pageMap[_pageNum].frame)
//       continue;
//     out << _pageNum << " ";
//     pageMap[_pageNum].save(out);
//   }
//   // Page number zero signals end of page dump
//   out << 0 << endl;
  // Segment dump
  out << "Segments " << segmentMap.size() << endl;
  for(SegmentMap::const_iterator segIt=segmentMap.begin();segIt!=segmentMap.end();segIt++)
    segIt->second.save(out);
  // Dump function name mappings
//   out << "FuncNames " << funcAddrToName.size() << endl;
//   for(AddrToNameMap::const_iterator addrIt=funcAddrToName.begin();
//       addrIt!=funcAddrToName.end();addrIt++){
//     const std::string &file=getFuncFile(addrIt->first);
//     I(file!="");
//     out << addrIt->first << 
//            " -> " << strlen(addrIt->second) << " " << addrIt->second <<
//       " : " << file.length() << " " << file << endl;
//   }
}

AddressSpace::AddressSpace(ChkReader &in){
  in >> "BrkBase " >> brkBase >> endl;
  while(true){
    size_t _pageNum;
    in >> _pageNum;
    if(!_pageNum)
      break;
    in >> " ";
    pageTable[_pageNum]=in;
  }
  in >> endl;
  size_t _segCount;
  in >> "Segments " >> _segCount >> endl;
  for(size_t i=0;i<_segCount;i++){
    SegmentDesc seg;
    seg=in;
    newSegment(seg.addr,seg.len,seg.canRead,seg.canWrite,seg.canExec,seg.shared,seg.fileStatus,seg.fileOffset);
    setGrowth(seg.addr,seg.autoGrow,seg.growDown);
  }
  // Load function name mappings
//   size_t _funcAddrToName;
//   in >> "FuncNames " >> _funcAddrToName >> endl;
//   for(size_t i=0;i<_funcAddrToName;i++){
//     VAddr _addr; size_t _strlen;
//     in >> _addr >> " -> " >> _strlen >> " ";
//     char func[_strlen+1];
//     in >> func >> " : " >> _strlen >> " ";
//     char file[_strlen+1];
//     in >> file >> endl;
//     addFuncName(_addr,func,file);
//   }
}

// Add a new function name-address mapping
void AddressSpace::addFuncName(VAddr addr, const std::string &func, const std::string &file){
  std::pair<NamesByAddr::iterator,bool> ins=namesByAddr.insert(NameEntry(addr,func,file));
  if(ins.second)
    namesByName.insert(&(*(ins.first)));
}

// Removes all existing function name mappings in a given address range
void AddressSpace::delFuncNames(VAddr begAddr, VAddr endAddr){
  NamesByAddr::iterator begIt=namesByAddr.upper_bound(endAddr);
  NamesByAddr::iterator endIt=begIt;
  while(endIt->addr>=begAddr){
    namesByName.erase(&(*endIt));
    endIt++;
  }
  namesByAddr.erase(begIt,endIt);
}


// Return name of the function with given entry point
const std::string &AddressSpace::getFuncName(VAddr addr) const{
  NamesByAddr::const_iterator nameIt=namesByAddr.lower_bound(addr);
  if(nameIt==namesByAddr.end())
    fail("");
  if(nameIt->addr!=addr)
    fail("");
  return nameIt->func;
}

// Return name of the ELF file in which the function is, given the entry point
const std::string &AddressSpace::getFuncFile(VAddr addr) const{
  NamesByAddr::const_iterator nameIt=namesByAddr.lower_bound(addr);
  if(nameIt==namesByAddr.end())
    fail("");
  if(nameIt->addr!=addr)
    fail("");
  return nameIt->func;
}

// Given the name, return where the function begins
VAddr AddressSpace::getFuncAddr(const std::string &name) const{
  NameEntry key(0,name,"");
  NamesByName::const_iterator nameIt=namesByName.lower_bound(&key);
  if(nameIt==namesByName.end())
    return 0;
  if((*nameIt)->func!=name)
    return 0;
  return (*nameIt)->addr;
}

// Given a code address, return where the function begins (best guess)
VAddr AddressSpace::getFuncAddr(VAddr addr) const{
  NamesByAddr::const_iterator nameIt=namesByAddr.lower_bound(addr);
  if(nameIt==namesByAddr.end())
    fail("");
  return nameIt->addr;
}

// Given a code address, return the function size (best guess)
size_t AddressSpace::getFuncSize(VAddr addr) const{
  NamesByAddr::const_iterator nameIt=namesByAddr.lower_bound(addr);
  I(nameIt!=namesByAddr.end());
  VAddr abeg=nameIt->addr;
  do{
    nameIt--;
    I(nameIt!=namesByAddr.end());
  }while(nameIt->addr==abeg);
  VAddr aend=nameIt->addr;
  return aend-abeg;
}

// Print name(s) of function(s) with given entry point
void AddressSpace::printFuncName(VAddr addr) const{
  NamesByAddr::const_iterator it=namesByAddr.lower_bound(addr);
  bool first=true;
  while((it!=namesByAddr.end())&&(it->addr==addr)){
    std::cout << (first?"":", ") << it->file << ":" << it->func;
    first=false;
    it++;
  }
}

void AddressSpace::addHandler(const std::string &name, NameToFuncMap &map, EmulFunc *func){
  map[name].push_back(func);
  //  map.insert(NameToFuncMap::value_type(name.c_str(),func));
}
void AddressSpace::delHandler(const std::string &name, NameToFuncMap &map, EmulFunc *func){
  HandlerSet::iterator curIt=map[name].begin();
  HandlerSet::iterator endIt=map[name].end();
  while((curIt!=endIt)&&(*curIt!=func))
    curIt++;
  if(curIt==endIt)
    return;
  map[name].erase(curIt);
  if(!map.count(name))
    map.erase(name);
//   NameToFuncMap::iterator begIt=map.lower_bound(name.c_str());
//   NameToFuncMap::iterator endIt=map.upper_bound(name.c_str());
//   NameToFuncMap::iterator curIt=begIt;
//   while((curIt!=endIt)&&(curIt->second!=func))
//     curIt++;
//   if(curIt==endIt)
//     return;
//   map.erase(curIt);
}
bool AddressSpace::getHandlers(const std::string &name, const NameToFuncMap &map, HandlerSet &set){
  NameToFuncMap::const_iterator it=map.find(name);
  if(it==map.end())
    return false;
  const HandlerSet &srcSet=it->second;
  set.insert(set.end(),srcSet.begin(),srcSet.end());
//   NameToFuncMap::const_iterator handBeg=map.lower_bound(name.c_str());
//   NameToFuncMap::const_iterator handEnd=map.upper_bound(name.c_str());
//   for(NameToFuncMap::const_iterator handCur=handBeg;handCur!=handEnd;handCur++)
//     set.push_back(handCur->second);
  //    set.insert(handCur->second);
  return true;
}
AddressSpace::NameToFuncMap AddressSpace::nameToCallHandler;
AddressSpace::NameToFuncMap AddressSpace::nameToRetHandler;
void AddressSpace::addCallHandler(const std::string &name,EmulFunc *func){
  addHandler(name,nameToCallHandler,func);
}
void AddressSpace::addRetHandler(const std::string &name,EmulFunc *func){
  addHandler(name,nameToRetHandler,func);
}
void AddressSpace::delCallHandler(const std::string &name,EmulFunc *func){
  delHandler(name,nameToCallHandler,func);
}
void AddressSpace::delRetHandler(const std::string &name,EmulFunc *func){
  delHandler(name,nameToRetHandler,func);
}
bool AddressSpace::getCallHandlers(VAddr addr, HandlerSet &set) const{
  bool rv=false;
  for(NamesByAddr::iterator it=namesByAddr.lower_bound(addr);(it!=namesByAddr.end())&&(it->addr==addr);it++)
    rv=rv||getHandlers(it->func,nameToCallHandler,set);
  return rv||getHandlers("",nameToCallHandler,set);
}
bool AddressSpace::getRetHandlers(VAddr addr, HandlerSet &set) const{
  bool rv=false;
  for(NamesByAddr::iterator it=namesByAddr.lower_bound(addr);(it!=namesByAddr.end())&&(it->addr==addr);it++)
    rv=rv||getHandlers(it->func,nameToRetHandler,set);
  return rv||getHandlers("",nameToRetHandler,set);
}

VAddr AddressSpace::newSegmentAddr(size_t len){
  //  VAddr retVal=alignDown((VAddr)-1,AddrSpacPageSize);
  VAddr retVal=alignDown((VAddr)0x7fffffff,AddrSpacPageSize);
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
  I(!(pivot%AddrSpacPageSize));
  SegmentDesc &newSeg=segmentMap[pivot];
  newSeg=oldSeg;
  newSeg.addr=pivot;
  newSeg.len=oldSeg.addr+oldSeg.len-newSeg.addr;
  oldSeg.len=oldSeg.len-newSeg.len;
  if(oldSeg.fileStatus)
    newSeg.fileOffset=oldSeg.fileOffset+oldSeg.len;
  I((newSeg.pageNumLb()+1==oldSeg.pageNumUb())||(newSeg.pageNumLb()==oldSeg.pageNumUb()));
  if(newSeg.pageNumLb()!=oldSeg.pageNumUb())
    pageTable[newSeg.pageNumLb()].map(newSeg.canRead,newSeg.canWrite,newSeg.canExec);
}
void AddressSpace::growSegmentDown(VAddr oldaddr, VAddr newaddr){
  I(!(newaddr%AddrSpacPageSize));
  I(segmentMap.find(oldaddr)!=segmentMap.end());
  I(newaddr<oldaddr);
  SegmentDesc &oldSeg=segmentMap[oldaddr];
  I(isNoSegment(newaddr,oldaddr-newaddr));
  SegmentDesc &newSeg=segmentMap[newaddr];
  newSeg=oldSeg;
  newSeg.addr=newaddr;
  newSeg.len=oldaddr+oldSeg.len-newaddr;
  I(!oldSeg.fileStatus);
  I(newSeg.pageNumUb()==oldSeg.pageNumUb());
  for(size_t pageNum=newSeg.pageNumLb();pageNum<oldSeg.pageNumLb();pageNum++)
    pageTable[pageNum].map(newSeg.canRead,newSeg.canWrite,newSeg.canExec);
  segmentMap.erase(oldaddr);
}
void AddressSpace::resizeSegment(VAddr addr, size_t newlen){
  I(segmentMap.find(addr)!=segmentMap.end());
  I(newlen>0);
  SegmentDesc &mySeg=segmentMap[addr];
  I(mySeg.addr==addr);
  I(!mySeg.fileStatus);
  size_t oldPageNumUb=mySeg.pageNumUb();
  mySeg.len=newlen;
  size_t newPageNumUb=mySeg.pageNumUb();
  if(oldPageNumUb<newPageNumUb){
    for(size_t pageNum=oldPageNumUb;pageNum<newPageNumUb;pageNum++)
      pageTable[pageNum].map(mySeg.canRead,mySeg.canWrite,mySeg.canExec);
  }else if(oldPageNumUb>newPageNumUb){
    for(size_t pageNum=newPageNumUb;pageNum<oldPageNumUb;pageNum++)
      pageTable[pageNum].unmap(mySeg.canRead,mySeg.canWrite,mySeg.canExec);
  }
}
void AddressSpace::moveSegment(VAddr oldaddr, VAddr newaddr){
  I(!(newaddr%AddrSpacPageSize));
  I(segmentMap.find(oldaddr)!=segmentMap.end());
  SegmentDesc &oldSeg=segmentMap[oldaddr];
  I(!oldSeg.fileStatus);
  I(isNoSegment(newaddr,oldSeg.len));
  SegmentDesc &newSeg=segmentMap[newaddr];
  newSeg=oldSeg;
  newSeg.addr=newaddr;
  size_t oldPg=oldSeg.pageNumLb();
  size_t newPg=newSeg.pageNumLb();
  while(oldPg<oldSeg.pageNumUb()){
    I(newPg<newSeg.pageNumUb());
    PageDesc &oldPageDesc=pageTable[oldPg];
    PageDesc &newPageDesc=pageTable[newPg];
    newPageDesc.frame=oldPageDesc.frame;
    oldPageDesc.frame=0;
    I(newPageDesc.mapCount==0);
    I(oldPageDesc.mapCount==1);
    newPageDesc.mapCount=oldPageDesc.mapCount;
    oldPageDesc.mapCount=0;
    I(newPageDesc.canRead==0);
    I(oldPageDesc.canRead==(oldSeg.canRead?1:0));
    newPageDesc.canRead=oldPageDesc.canRead;
    oldPageDesc.canRead=0;
    I(newPageDesc.canWrite==0);
    I(oldPageDesc.canWrite==(oldSeg.canWrite?1:0));
    newPageDesc.canWrite=oldPageDesc.canWrite;
    oldPageDesc.canWrite=0;
//    I(!oldPageDesc.insts);
    I(oldPageDesc.canExec==0);
    I(newPageDesc.canExec==0);
    I(oldPageDesc.canExec==(oldSeg.canExec?1:0));
    newPageDesc.canExec=oldPageDesc.canExec;
    oldPageDesc.canExec=0;
    oldPg++;
    newPg++;
  }
  I(newPg==newSeg.pageNumUb());
  segmentMap.erase(oldaddr);
}
void AddressSpace::deleteSegment(VAddr addr, size_t len){
  splitSegment(addr);
  splitSegment(addr+len);
  // Find the segment that begins before the end of deletion region
  SegmentMap::iterator begIt=segmentMap.upper_bound(addr+len);
  // If nothing begins before the end of deletion, nothing to do
  if(begIt==segmentMap.end())
    return;
  // This segment should not go past the end of deletion (we did the split)
  I(begIt->second.addr+begIt->second.len<=addr+len);
  // Find the segment that begins before the start of deletion region
  SegmentMap::iterator endIt=segmentMap.upper_bound(addr);
  // This segment should not go past the start of deletion (we did the split)
  I((endIt==segmentMap.end())||(endIt->second.addr+endIt->second.len<=addr));
  for(SegmentMap::iterator segIt=begIt;segIt!=endIt;segIt++){
    SegmentDesc &seg=segIt->second;
    I(!seg.fileStatus);
    I((seg.addr>=addr)&&(seg.addr+seg.len<=addr+len));
    for(size_t pageNum=seg.pageNumLb();pageNum<seg.pageNumUb();pageNum++)
      pageTable[pageNum].unmap(seg.canRead,seg.canWrite,seg.canExec);
  }
  segmentMap.erase(begIt,endIt);
#if (defined DEBUG)
  for(SegmentMap::iterator segIt=segmentMap.begin();segIt!=segmentMap.end();segIt++){
    SegmentDesc &seg=segIt->second;    
    I((seg.addr+seg.len<=addr)||(seg.addr>=addr+len));
  }
#endif
  // Remove function name mappings for this segment
  delFuncNames(addr,addr+len);
//   while(true){
//     // Find the segment that begins before the end of deletion
//     SegmentMap::iterator segIt=segmentMap.upper_bound(addr+len);
//     // If no segment begins before end of deletion, no deletion is needed
//     if(segIt==segmentMap.end())
//       break;
//     SegmentDesc &oldSeg=segIt->second;
//     // If the segment end before the start of deletion, no deletion is needed
//     if(oldSeg.addr+oldSeg.len<=addr)
//       break;
//     // If the segment ends after the end of deletion, we must split it
//     if(oldSeg.addr+oldSeg.len>addr+len){
//       I(0);
//       SegmentDesc &newSeg=segmentMap[addr+len];
//       newSeg=oldSeg;
//       newSeg.addr=addr+len;
//       newSeg.len=oldSeg.addr+oldSeg.len-newSeg.addr;
//       oldSeg.len=oldSeg.len-newSeg.len;
//       if(newSeg.pageNumLb()<oldSeg.pageNumUb()){
// 	I(newSeg.pageNumLb()+1==oldSeg.pageNumUb());
// 	addPages(newSeg.pageNumLb(),newSeg.pageNumUb(),newSeg.canRead,newSeg.canWrite,newSeg.canExec);
//       }
//       continue;
//     }
//     // If we got here, the segment ends in the deletion region
//     // If it begins before the deletion region, we must truncate it
//     if(oldSeg.addr<addr){
//       size_t oldPageNumUb=oldSeg.pageNumUb();
//       oldSeg.len=addr-oldSeg.addr;
//       if(oldSeg.pageNumUb()<oldPageNumUb)
// 	for(size_t pageNum=oldSeg.pageNumUb();pageNum<oldPageNumUb;pageNum++)
// 	  getPageDesc(pageNum).unmap(oldSeg.canRead,oldSeg.canWrite,oldSeg.canExec);
//       continue;
//     }
//     // If we got here, the segment is within the deletion region and is deleted
//     I(oldSeg.addr>=addr);
//     I(oldSeg.addr+oldSeg.len<=addr+len);
//     I(oldSeg.pageNumLb()<=oldSeg.pageNumUb());
//     for(size_t pageNum=oldSeg.pageNumLb();pageNum<oldSeg.PageNumUb();pageNum++)
//       getPageDesc(pageNum).unmap(oldSeg.canRead,oldSeg.canWrite,oldSeg.canExec);
//     segmentMap.erase(segIt);
//   }
}
void AddressSpace::newSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec, bool shared, FileSys::FileStatus *fs, off_t offs){
  I(!(addr%AddrSpacPageSize));
  I(len>0);
  I(isNoSegment(addr,len));
  SegmentDesc &newSeg=segmentMap[addr];
  newSeg.addr=addr;
  newSeg.len=len;
  newSeg.canRead=canRead;
  newSeg.canWrite=canWrite;
  newSeg.canExec=canExec;
  newSeg.autoGrow=false;
  newSeg.growDown=false;
  I(!shared);
  newSeg.shared=shared;
  newSeg.fileStatus=fs;
  newSeg.fileOffset=offs;
  for(size_t pageNum=newSeg.pageNumLb();pageNum<newSeg.pageNumUb();pageNum++)
    pageTable[pageNum].map(canRead,canWrite,canExec);
}
void AddressSpace::protectSegment(VAddr addr, size_t len, bool canRead, bool canWrite, bool canExec){
  splitSegment(addr);
  splitSegment(addr+len);
  I(isSegment(addr,len));
  SegmentDesc &mySeg=segmentMap[addr];
  I(mySeg.addr==addr);
  I(mySeg.len==len);
  I(!mySeg.fileStatus);
  for(size_t pageNum=mySeg.pageNumLb();pageNum<mySeg.pageNumUb();pageNum++){
    pageTable[pageNum].allow(canRead&&!mySeg.canRead,canWrite&&!mySeg.canWrite,canExec&&!mySeg.canExec);
    pageTable[pageNum].deny(mySeg.canRead&&!canRead,mySeg.canWrite&&!canWrite,mySeg.canExec&&!canExec);
  }
  mySeg.canRead=canRead;
  mySeg.canWrite=canWrite;
  mySeg.canExec=canExec;
}
