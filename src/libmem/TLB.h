#ifndef TLB_H
#define TLB_H

#include "Snippets.h"

#include "ThreadContext.h"
#include "CacheCore.h"

class TLB {
private:
  class TLBState : public StateGeneric<> {
  public:
    long  physicalPage;

    TLBState(long iphysicalPage = -1) {  physicalPage = iphysicalPage; }

    bool operator==(TLBState s) const {
      return physicalPage == s.physicalPage;
    }
  };

  typedef CacheGeneric<TLBState, VAddr> TLBCache;

  const ushort id;
  TLBCache *cache;

public:
  TLB(const char *section, bool dataTLB, int i);
  ~TLB();

  long translate(VAddr vAddr);
  void insert(VAddr vAddr, long  phPage);
};

#endif

