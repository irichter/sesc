#if !(defined _GCOBJECT_H_)
#define _GCOBJECT_H_

class GCObject{
 private:
  size_t refCount;
 protected:
  GCObject(void) : refCount(0){
  }
  virtual ~GCObject(void){
  }
 public:
  void addRef(void){
    refCount++; 
  }
  void delRef(void){
    refCount--;
    if(!refCount)
      delete this; 
  }
  size_t getRefCount(void) const{
    return refCount;
  }
};

#endif // !(defined _GCOBJECT_H_)
