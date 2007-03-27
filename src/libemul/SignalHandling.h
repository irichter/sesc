#ifndef SIGNAL_HANDLING_H
#define SIGNAL_HANDLING_H

#include "Addressing.h"
#include <bitset>
#include <vector>

typedef enum{
  SigActTerm   = 0,
  SigActIgnore = 1,
  SigActCore   = 2,
  SigActStop   = 3,
  SigActNumber = 4,
} SignalAction;

typedef enum{
  SigNone= 0,
  SigNMin=  1,
  SigNMax=128,
  SigChld,
  SigAlrm,
  SigIO,
  NumSignals,
} SignalID;

typedef enum {SigCodeIn, SigCodeChldExit, SigCodeUser} SigCode;

class SigInfo{
 public:
  SignalID signo;
  SigCode  code;

  int pid;
  int uid;

  int data; // Status for SIGCLD, fd for SIGIO
  SigInfo(SignalID signo, SigCode code) : signo(signo), code(code){
  }
};

SignalAction getDflSigAction(SignalID sig);

typedef std::bitset<NumSignals> SignalSet;

typedef std::vector<SigInfo *>  SignalQueue;

typedef std::vector<SignalSet>  SignalSetStack;

typedef void SigCallBackFun(SigInfo *);

class SigCallBack{
 public:
  virtual void operator()(SigInfo *sigInfo) = 0;
};

class SignalState{
 public:
  SignalSetStack masks;
  SignalQueue    ready;
  SignalQueue    masked;
  SigCallBack   *wakeup;
  SignalState(void)
    :
    masks(1,SignalSet()),
    ready(),
    masked(),
    wakeup(0)
  {
  }
  SignalState(const SignalState &src)
    :
    masks(src.masks),
    ready(),
    masked(),
    wakeup(0)
  {
    I(src.ready.empty());
    I(src.masked.empty());
    I(!src.wakeup);
  }
  void setWakeup(SigCallBack *newWakeup){
    I(!wakeup);
    wakeup=newWakeup;
    if(!ready.empty()){
      SigCallBack *func=wakeup;
      wakeup=0;
      (*func)(ready.back());
      delete func;
    }
  }
  void clrWakeup(void){
    I(wakeup);
    delete wakeup;
    wakeup=0;
  }
  void raise(SigInfo *sigInfo){
    SignalID sig=sigInfo->signo;
    if(masks.back().test(sig)){
      masked.push_back(sigInfo);
    }else{
      ready.push_back(sigInfo);
      if(wakeup){
	SigCallBack *func=wakeup;
	wakeup=0;
	(*func)(sigInfo);
	delete func;
      }
    }
  }
  void pushMask(const SignalSet &newMask){
    masks.push_back(newMask);
    maskChanged();
  }
  void setMask(const SignalSet &newMask){
    masks.back()=newMask;
    maskChanged();
  }
  void popMask(void){
    masks.pop_back();
    I(!masks.empty());
    maskChanged();
  }
  const SignalSet &getMask(void) const{
    return masks.back();
  }
  void maskChanged(void){
    SignalSet &mask=masks.back();
    for(size_t i=0;i<masked.size();i++){
      SignalID sig=masked[i]->signo;
      if(!mask.test(sig)){
	ready.push_back(masked[i]);
	masked[i]=masked.back();
	masked.pop_back();
      }
    }
    for(size_t i=0;i<ready.size();i++){
      SignalID sig=ready[i]->signo;
      if(mask.test(sig)){
	masked.push_back(ready[i]);
	ready[i]=ready.back();
	ready.pop_back();
      }
    }
    if((!ready.empty())&&wakeup){
      SigCallBack *func=wakeup;
      wakeup=0;
      (*func)(ready.back());
      delete func;
    }
  }
  bool hasReady(void) const{
    return !ready.empty();
  }
  SigInfo *nextReady(void){
    I(hasReady());
    SigInfo *sigInfo=ready.back();
    ready.pop_back();
    return sigInfo;
  }
};

class SignalDesc{
 public:
  VAddr     handler;
  SignalSet mask;
  SignalDesc &operator=(const SignalDesc &src){
    handler=src.handler;
    mask=src.mask;
    return *this;
  }
};

class SignalTable{
 public:
  SignalDesc table[NumSignals];
  SignalTable(void){
    for(size_t i=0;i<NumSignals;i++){
      table[i].handler=getDflSigAction((SignalID)i);
      table[i].mask.reset();
    }
  }
  SignalTable(const SignalTable &src){
    for(size_t i=0;i<NumSignals;i++)
      table[i]=src.table[i];
  }
  SignalDesc &operator[](size_t sig){
    return table[sig];
  }
};

#endif // SIGNAL_HANDLING_H

