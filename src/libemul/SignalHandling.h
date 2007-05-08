#ifndef SIGNAL_HANDLING_H
#define SIGNAL_HANDLING_H

#include "Addressing.h"
#include "GCObject.h"
#include "Checkpoint.h"
#include <bitset>
#include <vector>

// Define this to enable debugging of signal handling and delivery
//#define DEBUG_SIGNALS

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
  SigInfo(){
  }
  SigInfo(SignalID signo, SigCode code) : signo(signo), code(code){
  }
  void save(ChkWriter &out) const;
  void restore(ChkReader &in);
};
inline ChkWriter &operator<< (ChkWriter &out, const SigInfo &si){
  si.save(out); return out;
}
inline ChkReader &operator>> (ChkReader &in, SigInfo &si){
  si.restore(in); return in;
}

SignalAction getDflSigAction(SignalID sig);

typedef std::bitset<NumSignals> SignalSet;

typedef std::vector<SigInfo *>  SignalQueue;

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

class SignalTable : public GCObject{
 public:
  typedef SmartPtr<SignalTable> pointer;
 private:
  SignalDesc table[NumSignals];
 public:
  SignalTable(void) : GCObject(){
    for(size_t i=0;i<NumSignals;i++){
      table[i].handler=getDflSigAction((SignalID)i);
      table[i].mask.reset();
    }
  }
  SignalTable(const SignalTable &src) : GCObject(){
    for(size_t i=0;i<NumSignals;i++)
      table[i]=src.table[i];
  }
  ~SignalTable(void);
  SignalDesc &operator[](size_t sig){
    return table[sig];
  }
  void save(ChkWriter &out) const;
  void restore(ChkReader &in);
};
inline ChkWriter &operator<< (ChkWriter &out, const SignalTable &st){
  st.save(out); return out;
}
inline ChkReader &operator>> (ChkReader &in, SignalTable &st){
  st.restore(in); return in;
}


#endif // SIGNAL_HANDLING_H

