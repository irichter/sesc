#include "nanassert.h"
#include "SignalHandling.h"

SignalAction getDflSigAction(SignalID sig){
  switch(sig){
  case SigNone:
    return SigActCore;
  case SigChld:
  case SigIO:
    return SigActIgnore;
  case SigAlrm:
    return SigActTerm;
  default:
    I(sig<=NumSignals);
  }
  return SigActTerm;
}
