#if !(defined FILE_SYS_H)
#define FILE_SYS_H

#include <stddef.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <map>
#include "GCObject.h"
#include "Checkpoint.h"

namespace MemSys{
  class FrameDesc;
}

namespace FileSys {

  typedef enum {
    Disk,
    Pipe,
    Stream,
  } FileType;
  
  class BaseStatus : public GCObject {
  public:
    typedef SmartPtr<BaseStatus> pointer;
    // Type of this file
    FileType type;
    // Native file descriptor 
    int32_t      fd;
    // Simulated flags
    int32_t      flags;
    // IDs of threads blocked on reads from this file
    class PidSet : public std::vector<int>{};
    //    typedef std::vector<int> PidSet;
    PidSet readBlockPids;
    PidSet writeBlockPids;
  protected:
    BaseStatus(FileType type, int32_t fd, int32_t flags);
    virtual ~BaseStatus(void);
  public:
    virtual void addReadBlock(int32_t pid){
      readBlockPids.push_back(pid);
    }
    virtual void addWriteBlock(int32_t pid){
      writeBlockPids.push_back(pid);
    }
    virtual void endReadBlock(void){
    }
    virtual void endWriteBlock(void){
    }
    virtual void save(ChkWriter &out) const;
    static BaseStatus *create(ChkReader &in);
    BaseStatus(FileType type, ChkReader &in);
    FileType getType(void) const{ return type; }
    virtual void mmap(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs){
      printf("BaseStatus::mmap should never be called"); exit(1);
    }
    virtual void munmap(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs){
      printf("BaseStatus::munmap should never be called"); exit(1);
    }
    virtual void msync(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs){
      printf("BaseStatus::msync should never be called"); exit(1);
    }
  };

  class FileStatus : public BaseStatus{
  public:
    typedef SmartPtr<FileStatus> pointer;
    // Name of the file that was **intended** to be opened
    const char *name;
    // Mode with which the file was **intended** to be opened
    mode_t      mode;
  protected:
    FileStatus(const char *name, int32_t fd, int32_t flags, mode_t mode);
    virtual ~FileStatus(void);
  public:
    static FileStatus *open(const char *name, int32_t flags, mode_t mode);
    virtual void save(ChkWriter &out) const;
    FileStatus(ChkReader &in);
    virtual void mmap(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs);
    virtual void munmap(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs);
    virtual void msync(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs);
  };
  class PipeStatus : public BaseStatus{
  public:
    // Is this the write or the read end of the pipe
    bool        isWrite;
    // The other end of the pipe
    PipeStatus *otherEnd;
    // Native descriptor of the other end
    int32_t otherFd;
  protected:
    PipeStatus(int32_t fd, int32_t otherFd, int32_t flags, bool isWrite)
      : BaseStatus(Pipe,fd,flags), isWrite(isWrite), otherEnd(0), otherFd(otherFd){
    }
    ~PipeStatus(void){
      if(otherEnd)
	otherEnd->otherEnd=0;
    }
  public:
    static PipeStatus *pipe(void);
    void setOtherEnd(PipeStatus *oe);
    virtual void endReadBlock(void);
    virtual void endWriteBlock(void);
    virtual void save(ChkWriter &out) const;
    PipeStatus(ChkReader &in);
  };
  class StreamStatus : public BaseStatus{
  protected:
    StreamStatus(int32_t fd, int32_t flags);
    ~StreamStatus(void);
  public:
    static StreamStatus *wrap(int32_t fd);
    virtual void save(ChkWriter &out) const;
    StreamStatus(ChkReader &in);
  };
  class FileDesc {
  private:
    // The underlying file status
    BaseStatus::pointer st;
    // Cloexec flag
    bool cloexec;
  public:
    FileDesc(void);
    FileDesc(const FileDesc &src);
    ~FileDesc(void);
    FileDesc &operator=(const FileDesc &src);
    BaseStatus *getStatus(void) const{
      return st;
    }
    // Sets a new status for this file descriptor
    // The new status can be 0 (closed) and the old status need not be 0
    void setStatus(BaseStatus *newSt);
    bool getCloexec(void) const{
      return cloexec;
    }
    void setCloexec(bool cloex){
      cloexec=cloex;
    }
    void exec(void){
      if(st&&cloexec)
	setStatus(0);
    }
    void save(ChkWriter &out) const;
    FileDesc(ChkReader &in);
  };

  class OpenFiles : public GCObject{
  public:
    typedef SmartPtr<OpenFiles> pointer;
  private:
    typedef std::vector<FileDesc> FdArray;
    FdArray fds;
    static int32_t error(int32_t err);
  public:
    OpenFiles(void);
    OpenFiles(const OpenFiles &src);
    virtual ~OpenFiles(void);
    bool isOpen(int32_t fd) const;
    FileDesc *getDesc(int32_t fd);
    int32_t findNextFree(int32_t start){
      int32_t retVal=start;
      while(isOpen(retVal))
	retVal++;
      return retVal;
    }
    /*
    int32_t findFirstUsed(int32_t start){
      for(size_t retVal=start;retVal<fds.size();retVal++)
	if(isOpen(retVal))
	  return (int)retVal;
      return -1;
    }
    */
    void exec(void){
      for(size_t i=0;i<fds.size();i++)
	fds[i].exec();
    }
    int32_t open(const char *name, int32_t flags, mode_t mode);
    int32_t dup(int32_t oldfd);
    int32_t dup2(int32_t oldfd, int32_t newfd);
    int32_t dupfd(int32_t oldfd, int32_t minfd);
    int32_t pipe(int32_t fds[2]);
    // Returns true if this file is open and there are no more open duplicates
    // When the last copy is closed, we may need to take additional action
    // For example, when the write end of a pipe is closed we need to wake up any blocked readers
    bool isLastOpen(int32_t oldfd);
    int32_t close(int32_t oldfd);
    int32_t getfd(int32_t fd);
    int32_t setfd(int32_t fd, int32_t cloex);
    int32_t getfl(int32_t fd);
    ssize_t read(int32_t fd, void *buf, size_t count);
    ssize_t pread(int32_t fd, void *buf, size_t count, off_t offs);
    ssize_t write(int32_t fd, const void *buf, size_t count);
    ssize_t pwrite(int32_t fd, void *buf, size_t count, off_t offs);
    bool willReadBlock(int32_t fd);
    bool willWriteBlock(int32_t fd);
    void addReadBlock(int32_t fd, int32_t pid);
    void addWriteBlock(int32_t fd, int32_t pid);
    //    int32_t popReadBlock(int32_t fd);
    //    int32_t popWriteBlock(int32_t fd);
    off_t seek(int32_t fd, off_t offset, int32_t whence);
    void save(ChkWriter &out) const;
    OpenFiles(ChkReader &in);
  };
  
  class strlt{
  public:
    bool operator()(const char *first, const char *second) const{
      return (strcmp(first,second)<0);
    }
  };

  // Name space (mount and umount) information and file name translation
  class NameSpace : public GCObject{
  public:
    typedef SmartPtr<NameSpace> pointer;
  private:
    typedef std::map<std::string,std::string,std::greater<std::string> > Mounts;
    Mounts mounts;
  public:
    NameSpace(const std::string &mtlist);
    NameSpace(const NameSpace &src);
    const std::string toNative(const std::string &fname) const;
  };

  // File system (chroot, chdir, and umask) information and file name translation
  class FileSys : public GCObject{
  public:
    typedef SmartPtr<FileSys> pointer;
  private:
    NameSpace::pointer nameSpace;
    std::string cwd;
  public:
    FileSys(NameSpace *ns, const std::string &cwd);
    FileSys(const FileSys &src, bool newNameSpace);
    const std::string &getCwd(void) const{
      return cwd;
    }
    void setCwd(const std::string &newCwd);
    const std::string normalize(const std::string &fname) const;
    const std::string toNative(const std::string &fname) const;
  };

}

#endif // FILE_SYS_H
