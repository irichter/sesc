#include "SysCall.h"

SysCallMalloc::SysCallMalloc(ThreadContext *context){
  mySize=context->getGPR(Arg1stGPR);
  myAddr=context->getHeapManager()->allocate(mySize);
  if(myAddr){
    // Map real address to logical address space
    context->setGPR(RetValGPR,context->real2virt(myAddr));
  }else{
    // Return 0 and set errno to be POSIX compliant
    context->setErrno(ENOMEM);
    context->setGPR(RetValGPR,0);
  }
}
void SysCallMalloc::redo(ThreadContext *context){
  ID(size_t size=context->getGPR(Arg1stGPR));
  I(size==mySize);
  if(myAddr){
    context->getHeapManager()->allocate(myAddr,mySize);
    // Map real address to logical address space
    context->setGPR(RetValGPR,context->real2virt(myAddr));
  }else{
    // Return 0 and set errno to be POSIX compliant
    context->setErrno(ENOMEM);
    context->setGPR(RetValGPR,0);
  }
}
void SysCallMalloc::undo(ThreadContext *context){
  if(myAddr){
    context->getHeapManager()->deallocate(myAddr);
  }
}
SysCallFree::SysCallFree(ThreadContext *context){
  Address addr=context->getGPR(Arg1stGPR);
  I(addr);
  myAddr=context->virt2real(addr);
  mySize==context->getHeapManager()->deallocate(addr);
}
void SysCallFree::redo(ThreadContext *context){
  ID(Address addr=context->getGPR(Arg1stGPR));
  I((Address)(context->virt2real(addr))==myAddr);
  size_t size=context->getHeapManager()->deallocate(myAddr);
  I(size==mySize);
}
void SysCallFree::undo(ThreadContext *context){
  Address addr=context->getHeapManager()->allocate(myAddr,mySize);
  I(addr==myAddr);
}
