#if !(defined CHECKPOINT_H)
#define CHECKPOINT_H

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
using std::endl;
using std::hex;
using std::dec;
#include "GCObject.h"

class match{
  const char *str;
 public:
  match(const char *s) : str(s){}
  const char *getstr(void) const{
    return str;
  }
};

inline std::istream &operator>>(std::istream &in, const match &m){
  const char *str=m.getstr();
  while(*str){
    char c;
    in.get(c);
    I(c==*str);
    str++;
  }
  return in;
}

template<class T>
class wrap{
 private:
  T &ref;
 public:
  wrap(T &ref) : ref(ref){
  }
  operator T() const{
    return ref;
  }
  size_t &operator=(size_t &val){
    ref=static_cast<T>(val);
    return val;
  }
};

template<class T>
inline std::istream &operator>>(std::istream &in, wrap<T> &w){
  size_t v;
  in >> v;
  w=static_cast<T>(w);
  return in;
}

inline std::istream& endl(std::istream& is){
  char c;
  is.get(c);
  I(c=='\n');
  return is;
}

class ChkWriter : public std::ostream{
 public:
  std::map<const void *,size_t> objectMap;
  ChkWriter(std::streambuf *sb) : std::ostream(sb), objectMap(){
    objectMap[0]=0;
  }
  bool hasIndex(const void *obj) const{
    return objectMap.count(obj)?true:false;
  }
  size_t getIndex(const void *obj){
    if(objectMap.count(obj))
      return objectMap[obj];
    size_t ind=objectMap.size();
    objectMap[obj]=ind;
    return ind;
  }
  ChkWriter &writehex(const int64_t &v){
    for(size_t i=0;i<16;i++){
      size_t c=((v>>(60-4*i))&0xF);
      if(c<10)
	(*this)<<c;
      else
	(*this)<<static_cast<char>('A'+c-10);
    }
    return *this;
  }
  template<class T>
  void writeobj(const T *obj){
    bool seen=objectMap.count(obj);
    (*this) << getIndex(obj) << endl;
    if(!seen)
      obj->save(*this);
  }
};

class ChkReader : public std::istream{
 public:
  std::vector<void *> objectMap;
  ChkReader(std::streambuf *sb) : std::istream(sb), objectMap(){
    objectMap.push_back(0);
  }
  bool hasObject(size_t index) const{
    return (objectMap.size()>index);
  }
  void *getObject(size_t index) const{
    return objectMap[index];
  }
  void newObject(void *obj){
    objectMap.push_back(obj);
  }
  void newObject(void *obj, size_t index){
    I(objectMap.size()==index);
    objectMap.push_back(obj);
  }
  void newObject(size_t index){
    I(objectMap.size()==index);
    objectMap.push_back(0);
  }
  void setObject(size_t index, void *obj){
    I(index<objectMap.size());
    I(objectMap[index]==0);
    objectMap[index]=obj;
  }
  ChkReader &readhex(int64_t &dst){
    uint64_t v=0;
    for(size_t i=0;i<16;i++){
      uint64_t c=get();
      if((c>='0')&&(c<='9'))
	v|=((c-'0')<<(60-4*i));
      else if((c>='a')&&(c<='f'))
	v|=((c-'a'+10)<<(60-4*i));
      else if((c>='A')&&(c<='F'))
	v|=((c-'A'+10)<<(60-4*i));
    }
    dst=v;
    return *this;
  }
  template<class T>
  T *readobj(void){
    size_t _obj;
    (*this) >> _obj;
    (*this) >> endl;
    if(hasObject(_obj))
      return static_cast<T *>(getObject(_obj));
    newObject(_obj);
    T *retVal=new T(*this);
    setObject(_obj,retVal);
    return retVal;
  }
};

#endif // CHECKPOINT_H
