#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "nanassert.h"

#include "SescConf.h"
#include "MemorySystem.h"
#include "MPCache.h"

#if (defined DEBUG) && (DEBUG2==1)
#define DEBUGCONDITION 1
#else
#define DEBUGCONDITION 0
#endif

#define k_numPorts      "numPorts"
#define k_portOccp      "portOccp"
#define k_hitDelay      "hitDelay"
#define k_missDelay     "missDelay"
#define k_maxOutsLoads  "maxOutsLoads"
#define k_ldStBufSize   "ldStBufSize"
#define k_reportEviction "ReportEviction"

static pool<std::queue<MemRequest *> > activeMemReqPool;

MPCache::MPCache(MemorySystem* current, const char *section,
		 const char *name) 
  : MemObj(section, name),
    busyUntil(0),
    lineFillStall(false),
    stalledSet(0xFFFFFFFF),
    readHit("%s:readHit", name),
    readHalfMiss("%s:readHalfMiss", name),
    readMiss("%s:readMiss", name),
    writeHit("%s:writeHit", name),
    writeMiss("%s:writeMiss", name),
    writeBack("%s:writeBack", name),
    lineFill("%s:lineFill", name)
{
  MemObj *lower_level = NULL;

  cache = CacheGenericType::create(section, "", name);

  I(current);
  lower_level = current->declareMemoryObj(section, k_lowerLevel);

  if (lower_level != NULL)
    addLowerLevel(lower_level);
  else
    MSG("You are defining a cache with void as lowerLevel\n");
  
   mpLower = static_cast<MPCache *>(lower_level);

   I(SescConf);

   SescConf->isLong(section, k_numPorts);
   SescConf->isLong(section, k_portOccp);
   SescConf->isBool(section, k_reportEviction);

   cachePort = PortGeneric::create(name,SescConf->getLong(section, k_numPorts), occ = SescConf->getLong(section, k_portOccp));

   SescConf->isLong(section, k_hitDelay);
   hitDelay = SescConf->getLong(section, k_hitDelay);

   SescConf->isLong(section, k_missDelay);
   missDelay = SescConf->getLong(section, k_missDelay);
   notifyOfEviction = SescConf->getBool(section, k_reportEviction);

   SescConf->isLong(section, k_ldStBufSize);
   ldStBufSize = SescConf->getLong(section, k_ldStBufSize);
   if (!ldStBufSize)
     ldStBufSize = 1;

   defaultMask = ~(cache->getLineSize()-1);

   log2WrBufTag = cache->getLog2Ls();
}

MPCache::~MPCache()
{
}

void MPCache::access(MemRequest *mreq) {

  MemOperation memOp = mreq->getMemOperation();

  if(isStalled())
    if(memOp==MemRead || memOp==MemReadW || memOp==MemWrite) {
      registerWaitingReq(mreq);
      return;
    }
  
  switch(memOp){
  case MemRead:       read(mreq);        break;
  case MemReadW:      write(mreq);       break;
  case MemWrite:      write(mreq);       break;
  case MemInvalidate: invalidate(mreq);  break;
  case MemUpdate:     update(mreq);      break;    
  default:            mreq->goUp(1);     break;
  }
}

void MPCache::returnAccess(MemRequest *mreq)
{
  MemOperation memOp = mreq->getMemOperation();
  if(memOp == MemInvalidate)
    returnInvalidate(mreq);
  else if(memOp == MemUpdate)
    returnUpdate(mreq);
  else
    returnRead(mreq);
}

void MPCache::unBlockRequests(PAddr addr) 
{
  if( lineFillStall && cache->calcSet4Addr(addr) == stalledSet) {
    unsetLineFillLock();
    stalledSet = 0xFFFFFFFF;
    MemRequest *tmp = stalledMreq;
    stalledMreq = NULL;
    commonFastMiss(tmp,tmp->getMemOperation()==MemRead);
  }

  if( !isStalled() ){
    while (!waitList.empty() && !isStalled()) {
      GLOG(DEBUGCONDITION,"[C %llu] %s has enq=%i ", globalClock, 
	   symbolicName, waitList.size());
      
      MemRequest *mreq = waitList.front(); 

      GLOG(DEBUGCONDITION, "[C %llu] %s Dequeuing [%i]", globalClock, 
	   symbolicName,mreq->id());
      
      waitList.pop();
      
      access(mreq);
    }
  }
}

void MPCache::commonFastMiss(MemRequest *mreq, bool is_read)
{
  ulong addr = mreq->getPAddr();

  GLOG(DEBUGCONDITION,"[C %llu] MPCache::commonFastMiss %s adr %lu",globalClock,getDescrSection(),addr&defaultMask);

  ulong tmp = addr & defaultMask;
  PenReqMapper::iterator it = pLdStReqs.find(tmp);

  GLOG(DEBUGCONDITION, "[C %llu] %s M%c ad %lu set %lu %s [%i] %c",  
       globalClock, descrSection, is_read ? 'R' : 'W', 
       addr& defaultMask,cache->calcSet4Addr(addr), 
       (it == pLdStReqs.end()) ? "MISS-LEADER" : "MISS-LOSER", 
       mreq->id(), mreq ? 'F' : 'T');

  if (it == pLdStReqs.end()) {
    PAddr rplcAddr=0xFFFFFFFF;
    Line *l = cache->fillLine(addr, rplcAddr);

    if(l == NULL) {
      GLOG(DEBUGCONDITION,"[C %llu] %s Cache stalled...",globalClock,
	   descrSection);
      stalledMreq = mreq;
      stalledSet = cache->calcSet4Addr(addr);
      setLineFillLock();
      return;
    }

    GLOG(DEBUGCONDITION,"Filled Line for %lu",tmp);

    IS(l->start = globalClock);

    I(l);
    lineFill.inc();

    if( rplcAddr!=0xFFFFFFFF ) {
      upperLevelInvalidate(rplcAddr,l->getState()); 
      //l->invalidate();
    }

    pLdStReqs[tmp] = activeMemReqPool.out();

    if (is_read) {
      l->setLocks();
      readMiss.inc();
    } else if(l->isState(MPCState::MP_SHRD)) {
      l->setWrLock();
      writeMiss.inc();
    } else {
      l->setLocks();
      writeMiss.inc();
    }

    mreq->mutateWriteToRead();
    issueMemReq(mreq,missDelay);
  } else {
        
    if (is_read)
      readHalfMiss.inc();
    else
      writeHit.inc();

    (*it).second->push(mreq);
  }
  GLOG(DEBUGCONDITION,"[C %llu] MPCache::commonFastMiss LEAVE %s adr %lu",globalClock,getDescrSection(),(addr&defaultMask));
}

void MPCache::invalidate(PAddr addr, ushort size, MemObj *oc) 
{
  invUpperLevel(addr, size, oc);
  while (size) {
    Line *l = cache->readLine(addr);
    if(l)
      l->invalidate();

    addr += cache->getLineSize();
    size -= cache->getLineSize();
  } 
}

bool MPCache::canAcceptStore(PAddr addr) 
{
  return !isStalled();
}

void MPCache::read(MemRequest *mreq)
{  
  PAddr addr = mreq->getPAddr();
  GLOG(DEBUGCONDITION,"[C %llu] MPCache::read %s adr %lu",globalClock,symbolicName,addr&defaultMask);

  Line *l = cache->readLine(addr);
  
// compile warning  GLOG(l!=NULL && DEBUGCONDITION,"[C %llu] read LINE CONTENTS %lu",globalClock,l->getState());
  
  //I think there is a bug here...
  bool isHit = ((l != 0) && !l->isRdLocked() && 
		!l->isGLocked());

  GLOG(DEBUGCONDITION && (l!=0) && (l->isGLocked()||l->isRdLocked()),"[C %llu] %lu Line is locked! %s",globalClock,addr&defaultMask,symbolicName);
  if (isHit) {
    Time_t newTime = hitDelay + occupyPort();
    readHit.inc();
    GLOG(DEBUGCONDITION,
	 "[C %llu] %s FR ad %lu HIT at %llu [%i] %c",  
	 globalClock, symbolicName, addr& defaultMask, newTime, mreq->id(), 
	 mreq ? 'F' : 'T');

    mreq->goUpAbs(newTime);
  }
  else
    commonFastMiss(mreq,true);
}

void MPCache::write(MemRequest *mreq)
{
  ulong addr = mreq->getPAddr();

  GLOG(DEBUGCONDITION,"[C %llu] %s:MPCache::write() ad %lu %d",globalClock,symbolicName,addr&defaultMask,mreq->getMemOperation());

  Line *l = cache->writeLine(addr);

  if(l) {

    if((l->isState(MPCState::MP_MOD) || l->isState(MPCState::MP_EX)) && 
       (!l->isWrLocked() && !l->isGLocked()) )      {
      l->setDirty();
      l->setState(MPCState::MP_MOD);

      Time_t newTime = occupyPort()+missDelay;
      writeHit.inc();

      GLOG(DEBUGCONDITION,"[C %llu] %s FW ad %lu HIT at %llu [%i] %c",  
	   globalClock, symbolicName, addr& defaultMask, newTime, mreq->id(), 
	   mreq ? 'F' : 'T');

      mreq->goUpAbs(newTime);

      return;
    }
  } 
  
  commonFastMiss(mreq,false);
}

void MPCache::invalidate(MemRequest *mreq) 
{
  PAddr paddr = mreq->getPAddr() & defaultMask;
  GLOG(DEBUGCONDITION,"[C %llu] MPCache::invalidate %s adr %lu",globalClock,symbolicName,paddr&defaultMask);
  
  Line *l = cache->readLine(paddr); // readLine is like findLine, but
				    // charges energy and keeps
				    // statistics

  int sz = upperLevel.size();

  PenReqMapper::iterator it = pInvUpdReqs.find(paddr&defaultMask);

  if(it != pInvUpdReqs.end()) {
    (*it).second->push(mreq);
  } else if(l == NULL || isHighestLevel()) {
    if(l!=NULL && (l->isRdLocked() || l->isWrLocked()) ) {
      l->setLocks();
      l->setState(MPCState::MP_INV);
    } else {
      mpFreeLine(paddr);
    }
    issueMemReq(mreq,missDelay);
  } else {
    l->setGLock();
    l->setInvReq();
    if(it == pInvUpdReqs.end()) {
      pInvUpdReqs[paddr&defaultMask] = activeMemReqPool.out();
      mreq->goDownAbs(occupyPort()+missDelay,upperLevel[0]);
    } else {
      (*it).second->push(mreq);
    }
  }
}

void MPCache::update(MemRequest *mreq)
{
  PAddr paddr = mreq->getPAddr() & defaultMask;

  Line *l = cache->readLine(paddr);

  GLOG(DEBUGCONDITION,"[C %llu] MPCache::update %s %lu...",globalClock,
       getDescrSection(), paddr);

  if(l==NULL) {
    issueMemReq(mreq,missDelay);
  } else if(isHighestLevel()) {
    l->setState(MPCState::MP_SHRD);
    l->clrDirty();
    issueMemReq(mreq,missDelay);
  } else {
    l->setUpdateReq();
    PenReqMapper::iterator it = pInvUpdReqs.find(paddr&defaultMask);
    if(it == pInvUpdReqs.end()) {
      pInvUpdReqs[paddr&defaultMask] = activeMemReqPool.out();
      mreq->goDownAbs(occupyPort()+missDelay,upperLevel[0]);
    } else {
      (*it).second->push(mreq);
    }
  }
}

void MPCache::returnRead(MemRequest *mreq)
{
  PAddr addr;
  MPCState newState;

  addr = (mreq->getPAddr() & defaultMask);
  GLOG(DEBUGCONDITION, "[C %llu] %s MPCache::returnRead adr %lu siz %d", globalClock, symbolicName,addr&defaultMask,waitList.size());
  
  MPCState *l = cache->writeLine(addr);

  GLOG(l==NULL,"Return read failed with l==NULL %s, address = %x",symbolicName, mreq->getPAddr());
  I(l);

  MemOperation memOp=mreq->getMemOperation();
  switch(memOp){
  case MemRead:           l->setState(MPCState::MP_SHRD);   break;
  case MemReadX:          
                          GLOG(DEBUGCONDITION,"[C %llu] %s MPCache::returnRead adr %lu SET MP_EX", globalClock, symbolicName,addr&defaultMask);
                          l->setState(MPCState::MP_EX);     break;
  case MemReadW:          
  case MemWrite:          
    GLOG(DEBUGCONDITION,"[C %llu] %s MPCache::returnRead adr %lu SET MP_MOD", globalClock, symbolicName,addr&defaultMask);
    l->setState(MPCState::MP_MOD);    
    l->setDirty();
    break;
  default:
    l->setState(MPCState::MP_INV);
    GLOG(DEBUGCONDITION, "[C %llu] %s failing with invalid state", globalClock, symbolicName);
    exit(0);
    break;
  }

  std::queue<MemRequest *> *tmpReqQueue;

  occupyPort();

  PenReqMapper::iterator it = pLdStReqs.find(addr);

  I(it != pLdStReqs.end());

  tmpReqQueue = (*it).second;

  bool blocked=false;

  MPCState stemp = l->getState();
  newState = stemp;

  while (tmpReqQueue->size() && !blocked) {
    MemRequest *tmpreq = tmpReqQueue->front();

    MemOperation memOp = tmpreq->getMemOperation();
    if(stemp.isGLocked()) {
      blocked = true;
      tmpreq->mutateWriteToRead();
      issueMemReq(tmpreq,missDelay);
    } else if(stemp.isState(MPCState::MP_SHRD)) {
      if(memOp==MemReadW || memOp==MemWrite) {
	writeMiss.inc();
	blocked = true;
	tmpreq->mutateWriteToRead();
	issueMemReq(tmpreq,missDelay);
      } else {	
	tmpreq->goUp(hitDelay);
      }
    } else if(stemp.isState(MPCState::MP_EX) || stemp.isState(MPCState::MP_MOD) ) { 

      if( memOp==MemReadW || memOp==MemWrite ) {
	newState.setDirty();
	newState.setState(MPCState::MP_MOD);
      }

      tmpreq->goUp(hitDelay);
    } 
      
    tmpReqQueue->pop();
  }

  if(!blocked){
    *l = newState;
    pLdStReqs.erase(addr);
    activeMemReqPool.in(tmpReqQueue);
    l->clrLocks();
  } else {
    l->clrRdLock();
  }

  mreq->goUpAbs(globalClock); //newTime 

  if(!blocked)
    unBlockRequests(addr);
}

void MPCache::upperLevelInvalidate(PAddr paddr,MPCState st) 
{

  GLOG(DEBUGCONDITION,"[C %llu] upperLevelInvalidate %s padr %lu",globalClock,getDescrSection(),paddr&defaultMask);

  if(!isHighestLevel()) {
    PenReqMapper::iterator it = pInvUpdReqs.find(paddr&defaultMask);
    selfInvMap[paddr] = st;
    if(it == pInvUpdReqs.end()) {
      IntlMemRequest::create(upperLevel[0],paddr,MemInvalidate,
			     occupyPort()-globalClock,
			     &MPCache::StaticInvalidateReturn,this);
      pInvUpdReqs[paddr&defaultMask] = activeMemReqPool.out();
    }     
  } else {
    GLOG(DEBUGCONDITION,"[C %llu] upperLevelInvalidate At HIGHEST for paddr %lu",globalClock,paddr&defaultMask);

    if ( st.isState(MPCState::MP_MOD) )
      issueWrBackReq(paddr,missDelay);
    else if (notifyOfEviction && (st.isState(MPCState::MP_SHRD)|| st.isState(MPCState::MP_EX))) {
      GLOG(DEBUGCONDITION,"[C %llu] upperLevelInvalidate At HIGHEST for paddr %lu SEND MemRmSharer",globalClock,paddr&defaultMask);  

      IntlMemRequest::create(lowerLevel[0], paddr,MemRmSharer,
			     occupyPort()+missDelay-globalClock);
    }
  }
}

void MPCache::returnInvalidate(MemRequest *mreq)
{
  PAddr addr = mreq->getPAddr()&defaultMask;

  Line *l = cache->writeLine(addr);

  GLOG(DEBUGCONDITION,"[C %llu] returnInvalidate %s adr %lu",globalClock,
       getDescrSection(),addr);

  if(l!=NULL) {
    if(!l->isRdLocked() && !l->isWrLocked()) {
      l->clrGLock();
      mpFreeLine(addr);
    } else {
      l->clrGLock();
      l->setLocks();
      l->setState(MPCState::MP_INV);
    }
  }

  issueMemReq(mreq,missDelay);

  PenReqMapper::iterator it = pInvUpdReqs.find(addr);
  I(it != pInvUpdReqs.end());

  while((*it).second->size()) 
  {
    issueMemReq((*it).second->front(),missDelay);
    (*it).second->pop();
  }

  activeMemReqPool.in((*it).second);
  pInvUpdReqs.erase(addr);

  unBlockRequests(addr);
}

//TODO: change this to MemRequest parameter
void MPCache::returnUpdate(MemRequest *mreq) 
{
  PAddr addr = mreq->getPAddr() & defaultMask;
  Line *l = cache->findLine(addr);
  I(l);

  GLOG(DEBUGCONDITION,"[C %llu] MPCache::returnUpdate %s %lu...",globalClock,
       getDescrSection(), addr);

  issueMemReq(mreq,missDelay);
  l->setState(MPCState::MP_SHRD);
  l->clrUpdateReq();

  PenReqMapper::iterator it = pInvUpdReqs.find(addr);

  I(it != pInvUpdReqs.end());

  while((*it).second->size()) {
    if( (*it).second->front()->getMemOperation() == MemUpdate ) {
      issueMemReq((*it).second->front(),missDelay);
      (*it).second->pop();
    } else {
      (*it).second->front()->goDownAbs(occupyPort(), upperLevel[0]);
      (*it).second->pop();
      break;
    }
  }
  
  if ( (*it).second->size() == 0 )
    pInvUpdReqs.erase(addr);

  unBlockRequests(addr);
}

void MPCache::handleInvalidate(MemRequest *mreq) 
{
  //Line has already been freed when this is called
  GLOG(DEBUGCONDITION,"[C %llu] MPCache::handleInvalidate ...",globalClock);
  PAddr paddr = mreq->getPAddr()&defaultMask;

  MPCState st = selfInvMap[paddr];

  if ( st.isState(MPCState::MP_MOD) )
    issueWrBackReq(paddr,missDelay);
  else if (notifyOfEviction && (st.isState(MPCState::MP_SHRD)|| st.isState(MPCState::MP_EX)))
    IntlMemRequest::create(lowerLevel[0], paddr,MemRmSharer,
			     occupyPort()-globalClock);

  selfInvMap.erase(paddr);

  PenReqMapper::iterator it = pInvUpdReqs.find(paddr);
  I(it != pInvUpdReqs.end());

  if( (*it).second->size() > 0 )
    while((*it).second->size()) {
      issueMemReq((*it).second->front(),missDelay);
      (*it).second->pop();
    }

  activeMemReqPool.in((*it).second);
  pInvUpdReqs.erase(paddr);

  unBlockRequests(paddr);
}

void MPCache::issueMemReq(MemRequest *mreq, TimeDelta_t waitTime) 
{
  MemOperation memOp = mreq->getMemOperation();

  if(memOp == MemInvalidate || memOp == MemUpdate)
    mreq->goUpAbs(occupyPort()+waitTime);
  else
    mreq->goDownAbs(occupyPort()+waitTime, lowerLevel[0]);  
}

void MPCache::issueWrBackReq(PAddr paddr, TimeDelta_t waitTime) 
{
  IntlMemRequest::create(lowerLevel[0], paddr, MemWrite, 
			 occupyPort() + waitTime - globalClock);
  writeBack.inc();
}

void MPCache::mpFreeLine(PAddr addr) 
{
  Line *l = cache->findLine(addr);

  if(l == 0)
    return;

  I(!l->isLocked());

  l->invalidate();
}

Time_t MPCache::getNextFreeCycle() const 
{
  return cachePort->calcNextSlot();
}


void MPCache::StaticInvalidateReturn(IntlMemRequest *ireq) 
{
  ((MPCache*)ireq->getWorkData())->handleInvalidate(ireq);
}
