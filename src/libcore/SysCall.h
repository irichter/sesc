#if !(defined _SysCall_h_)
#define _SysCall_h_

// For the definition of ThreadContext
#include "ThreadContext.h"

class SysCall{
 protected:
  ID(char *type);
  bool executed;
  SysCall(void) : executed(false){
    ID(type="SysCall");
  }
 public:
  virtual void exec(ThreadContext *context, icode_ptr picode) = 0;
  virtual void undo(bool expectRedo){};
  virtual void done(void){};
};

// Memory management system calls

// For the definition of Address
#include "Snippets.h"

class SysCallMalloc : public SysCall{
 private:
  // Real address of the allocated block
  // Contains 0 if the malloc call failed
  Address myAddr;
  // Size of the requested allocation
  ID(size_t  mySize);
  // Process ID of the thread
  // We need it to get the correct manager in undo
  Pid_t myPid;
 public:
  ID(SysCallMalloc(void){type="SysCallMalloc";});
  virtual void exec(ThreadContext *context, icode_ptr picode);
  virtual void undo(bool expectRedo);
};
class SysCallFree : public SysCall{
 private:
  // Real address of the freed block
  Address myAddr;
  // Size of the freed block
  size_t  mySize;
  // Process ID of the thread
  // We need it to get the correct manager in undo
  Pid_t myPid;
 public:
  ID(SysCallFree(void){type="SysCallFree";});
  virtual void exec(ThreadContext *context, icode_ptr picode);
  virtual void undo(bool expectRedo);
};
class SysCallMmap : public SysCall{
 private:
  // Real address of the mmap-ed block
  // Contains -1 if the mmap call failed
  Address myAddr;
  // Size of the requested allocation
  ID(size_t  mySize);
  // Process ID of the thread
  // We need it to get the correct manager in undo
  Pid_t myPid;
 public:
  ID(SysCallMmap(void){type="SysCallMmap";});
  virtual void exec(ThreadContext *context, icode_ptr picode);
  virtual void undo(bool expectRedo);  
};
class SysCallMunmap : public SysCall{
 private:
  // Real address of the unmapped block
  Address myAddr;
  // Size of the unmapped block
  size_t  mySize;
  // Process ID of the thread
  // We need it to get the correct manager in undo
  Pid_t myPid;
 public:
  ID(SysCallMunmap(void){type="SysCallMunmap";});
  virtual void exec(ThreadContext *context, icode_ptr picode);
  virtual void undo(bool expectRedo);  
};

// File I/O system calls

// For the definition of rsesc_OS_read_string
#include "Events.h"
// For open, close
#include <fcntl.h>
// For read, write
#include <unistd.h>
// For dummy files when re-getting original fd
#include <stack>
#define MAX_FILENAME_LENGTH 64

int conv_flags_to_native(int flags);

class SysCallFileIO : public SysCall{
 protected:
  struct OpenFileInfo{
    int flags;
    mode_t mode;
    char pathname[MAX_FILENAME_LENGTH];
    off_t offset;
    OpenFileInfo(char *name, int flags, mode_t mode, off_t offset)
      : flags(flags), mode(mode), offset(offset){
      I(strlen(name)<MAX_FILENAME_LENGTH);
      strcpy(pathname,name);
    }
    OpenFileInfo(const OpenFileInfo &other)
      : flags(other.flags), mode(other.mode), offset(other.offset){
      I(strlen(other.pathname)<MAX_FILENAME_LENGTH);
      strcpy(pathname,other.pathname);
    }
  };
  // Mapping of an open file's file descriptor to its undo/redo information. 
  typedef std::vector<OpenFileInfo *> OpenFileVector;
  static OpenFileVector openFiles;
 public:
  virtual void exec(ThreadContext *context, icode_ptr picode) = 0;
};
class SysCallOpen : public SysCallFileIO{
 private:
  // File descriptor for the opened file, -1 if open failed
  int myFd;
 public: 
  ID(SysCallOpen(void){type="SysCallOpen";});
  void exec(ThreadContext *context,icode_ptr picode);
  void undo(bool expectRedo);
};
class SysCallClose : public SysCallFileIO{
 private:
  int myFd;
  // Null if close fails, otherwise points to info needed to reopen the file
  OpenFileInfo *myInfo;
 public:
  ID(SysCallClose(void){type="SysCallClose";});
  virtual void exec(ThreadContext *context, icode_ptr picode);
  virtual void undo(bool expectRedo);
  virtual void done(void);
};
class SysCallRead : public SysCallFileIO {
 private:
  // Descriptor of the file to be read 
  int myFd;
  // Return value of call to read (no of bytes actualy read, -1 if error)
  ssize_t bytesRead;
  // Remember other params for debugging
  ID(void *myBuf);
  ID(size_t myCount);
  ID(off_t oldOffs);
 public:
  ID(SysCallRead(void){type="SysCallRead";});
  virtual void exec(ThreadContext *context,icode_ptr picode);
  virtual void undo(bool expectRedo);
};
class SysCallWrite : public SysCallFileIO {
 private:
  // Descriptor of the file to be written
  int myFd;
  // Return value of call to write (no of bytes actually written, -1 if error)
  ssize_t bytesWritten;
  // Buffered data for non-seekable devices
  void *bufData;
  // Remember other params for debugging
  ID(void *myBuf);
  ID(size_t myCount);
  ID(off_t oldOffs);
 public:
  ID(SysCallWrite(void){type="SysCallWrite";});
  virtual void exec(ThreadContext *context,icode_ptr picode);
  virtual void undo(bool expectRedo);
  virtual void done(void);
};

// Thread management system calls

class SysCallSescSpawn : public SysCall{
  // ID of the created child thread
  Pid_t childPid;
 public:
  ID(SysCallSescSpawn(void){type="SysCallSescSpawn";});
  virtual void exec(ThreadContext *context,icode_ptr picode);
  virtual void undo(bool expectRedo);
  virtual void done(void);
};

class SysCallExit : public SysCall{
 private:
  Pid_t myThread;
 public:
  ID(SysCallExit(void){type="SysCallExit";});
  virtual void exec(ThreadContext *context,icode_ptr picode);
  virtual void undo(bool expectRedo);
};

class SysCallWait : public SysCall{
 private:
  Pid_t childThread;
 public:
  ID(SysCallWait(void){type="SysCallWait";});
  virtual void exec(ThreadContext *context,icode_ptr picode);
  virtual void undo(bool expectRedo);
  void setChild(Pid_t child){
    I((childThread==-1)||(child==childThread));
    childThread=child;
  }
  Pid_t getChild(void){
    return childThread;
  }
};

// Time-related system calls

// For the definition of struct tms used in the times() call
#include <sys/times.h>

class SysCallTimes : public SysCall{
  clock_t    retVal;
  struct tms tmsStruct;
 public:
  ID(SysCallTimes(void){type="SysCallTimes";});
  virtual void exec(ThreadContext *context,icode_ptr picode){
    if(!executed){
      retVal=globalClock/1024;
      tmsStruct.tms_utime=globalClock/1024;
      tmsStruct.tms_stime=0;
      tmsStruct.tms_cutime=0;
      tmsStruct.tms_cstime=0;
    }
    Address buf=static_cast<Address>(context->getGPR(Arg1stGPR));
    if(buf)
      rsesc_OS_write_block(context->getPid(),picode->addr,(void *)buf,&tmsStruct,sizeof(tmsStruct));
    context->setGPR(RetValGPR,retVal);
    executed=true;
  }
};

#endif // !(defined _SysCall_h_)
