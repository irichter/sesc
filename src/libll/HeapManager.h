#if !(defined HeapManager_h)
#define HeapManager_h

#include <set>

#include "Snippets.h"
#include "nanassert.h"

class HeapManager {
private:
  // Reference counter for garbage collection
  size_t refCount;
  // Used to determine minimum required heap size
  Address begAddr;
  Address endAddr;

  // Minimum block size. Everything is aligned to this size
  enum {MinBlockSize=32, MinBlockMask=MinBlockSize-1};
  struct BlockInfo {
    Address addr;
    size_t  size;
    BlockInfo(Address addr, size_t size)
      : addr(addr), size(size){
    }
    struct lessBySize {
      bool operator()(const BlockInfo &x, const BlockInfo &y) const{
        return (x.size<y.size)||((x.size==y.size)&&(x.addr<y.addr));
      }
    };
    struct lessByAddr {
      bool operator()(const BlockInfo &x, const BlockInfo &y) const{
        return x.addr<y.addr;
      }
    };
  };
  size_t roundUp(size_t size){
    return (size+MinBlockMask)&(~MinBlockMask);
  }
  typedef std::set<BlockInfo,BlockInfo::lessByAddr> BlocksByAddr;
  typedef std::set<BlockInfo,BlockInfo::lessBySize> BlocksBySize;
  BlocksByAddr busyByAddr;
  BlocksByAddr freeByAddr;
  BlocksBySize freeBySize;
  HeapManager(Address base, size_t size);
  ~HeapManager(void);
public:
  static HeapManager *create(Address base, size_t size){
    return new HeapManager(base,size);
  }
  void addReference(void) {
    refCount++;
  }
  void delReference(void) {
    I(refCount>0);
    refCount--;
    if(!refCount){
      delete this;
    }
  }

  Address allocate(size_t size);
  Address allocate(Address addr, size_t size);
  size_t deallocate(Address addr);
};

#endif
