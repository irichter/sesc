#if !(defined _SysCall_h_)
#define _SysCall_h_

// For definition of ENOMEM
#include <errno.h>
// For the definition of Address
#include "Snippets.h"
// For the definition of ThreadContext
#include "ThreadContext.h"

class SysCall{
 public:
  virtual void redo(ThreadContext *context) = 0;
  virtual void undo(ThreadContext *context) = 0;
};
class SysCallMalloc : public SysCall{
 private:
  // Real address of the allocated block
  // Contains 0 if the malloc call failed
  Address myAddr;
  // Size of the requested allocation
  size_t  mySize;
 public:
  SysCallMalloc(ThreadContext *context);
  virtual void redo(ThreadContext *context);
  virtual void undo(ThreadContext *context);
};
class SysCallFree : public SysCall{
 private:
  // Real address of the freed block
  Address myAddr;
  // Size of the freed block
  size_t  mySize;
 public:
  SysCallFree(ThreadContext *context);
  virtual void redo(ThreadContext *context);
  virtual void undo(ThreadContext *context);
};

#endif // !(defined _SysCall_h_)
