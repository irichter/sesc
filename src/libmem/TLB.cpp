#include "nanassert.h"

#include "MemorySystem.h"

#include "TLB.h"
#include <iostream>
using namespace std;

TLB::TLB(const char *section, bool dataTLB, int i)
  : id(i)
{
  if (dataTLB)
    cache = TLBCache::create(section, "", "P(%d)_DTLB", i);
  else
    cache = TLBCache::create(section, "", "P(%d)_ITLB", i);

  I(cache);
}

TLB::~TLB()
{
  if( cache )
    cache->destroy();
}

long TLB::translate(VAddr vAddr) 
{
  TLBCache::CacheLine *cl = cache->readLine(GMemorySystem::calcPage(vAddr));

  //GMSG(cl==0 && id==0, "[%llu] TLB MISS %lx", globalClock, vAddr>>Log2PageSize);
  if (cl == 0) 
    return -1;

  return cl->physicalPage;
}

void TLB::insert(VAddr vAddr, long  phPage) 
{
  //GMSG(id==0, "[%llu] TLB INS %lx", globalClock, vAddr>>Log2PageSize);
  TLBCache::CacheLine *cl = cache->fillLine(GMemorySystem::calcPage(vAddr));
  cl->physicalPage = phPage;
}

