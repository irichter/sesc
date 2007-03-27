#if !(defined FILE_SYS_H)
#define FILE_SYS_H

#include <stddef.h>
#include <sys/types.h>
#include <vector>
#include <map>
#include "GCObject.h"

namespace FileSys {

  typedef enum {
    Disk,
    Pipe,
    Stream,
  } FileType;
  
  class BaseStatus : public GCObject {
  public:
    // Type of this file
    FileType type;
    // Native file descriptor 
    int      fd;
    // Simulated flags
    int      flags;
    // IDs of threads blocked on reads from this file
    typedef std::vector<int> PidSet;
    PidSet readBlockPids;
  protected:
    BaseStatus(FileType type, int fd, int flags);
    virtual ~BaseStatus(void);
  public:
    virtual void addReadBlock(int pid){
      readBlockPids.push_back(pid);
    }
    virtual void endReadBlock(void){
    }
  };
  class FileStatus : public BaseStatus{
  public:
    // Name of the file that was **intended** to be opened
    const char *name;
    // Mode with which the file was **intended** to be opened
    mode_t      mode;
  protected:
    FileStatus(const char *name, int fd, int flags, mode_t mode);
    virtual ~FileStatus(void);
  public:
    static FileStatus *open(const char *name, int flags, mode_t mode);
  };
  class PipeStatus : public BaseStatus{
  public:
    // Is this the write or the read end of the pipe
    bool        isWrite;
    // The other end of the pipe
    PipeStatus *otherEnd;
  protected:
    PipeStatus(int fd, int flags, bool isWrite)
      : BaseStatus(Pipe,fd,flags), isWrite(isWrite), otherEnd(0){
    }
    ~PipeStatus(void){
      if(otherEnd)
	otherEnd->otherEnd=0;
    }
  public:
    static PipeStatus *pipe(void);
    void setOtherEnd(PipeStatus *oe);
    virtual void endReadBlock(void);
  };
  class StreamStatus : public BaseStatus{
  protected:
    StreamStatus(int fd, int flags);
    ~StreamStatus(void);
  public:
    static StreamStatus *wrap(int fd);
  };
  class FileDesc {
  private:
    // The underlying file status
    BaseStatus *st;
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
  };

  class OpenFiles : public GCObject{
    typedef std::vector<FileDesc> FdArray;
    FdArray fds;
    static int error(int err);
  public:
    OpenFiles(void);
    OpenFiles(const OpenFiles &src);
    virtual ~OpenFiles(void);
    bool isOpen(int fd) const;
    FileDesc *getDesc(int fd);
    int findNextFree(int start){
      int retVal=start;
      while(isOpen(retVal))
	retVal++;
      return retVal;
    }
    /*
    int findFirstUsed(int start){
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
    int open(const char *name, int flags, mode_t mode);
    int dup(int oldfd);
    int dup2(int oldfd, int newfd);
    int dupfd(int oldfd, int minfd);
    int pipe(int fds[2]);
    // Returns true if this file is open and there are no more open duplicates
    // When the last copy is closed, we may need to take additional action
    // For example, when the write end of a pipe is closed we need to wake up any blocked readers
    bool isLastOpen(int oldfd);
    int close(int oldfd);
    int getfd(int fd);
    int setfd(int fd, int cloex);
    int getfl(int fd);
    ssize_t read(int fd, void *buf, size_t count);
    bool willReadBlock(int fd);
    void addReadBlock(int fd, int pid);
    ssize_t write(int fd, const void *buf, size_t count);
    int  popReadBlock(int fd);
    off_t seek(int fd, off_t offset, int whence);
  };
  
  class strlt{
  public:
    bool operator()(const char *first, const char *second) const{
      return (strcmp(first,second)<0);
    }
  };
   
  class FileNames{
    static FileNames* fileNames;
    char *cwd;
    typedef std::map<char *,char *,strlt> StringMap;
    StringMap simToReal;
  public:
    static FileNames* getFileNames(void);
    bool setCwd(const char *newcwd);
    const char *getCwd(void){
      return cwd;
    }
    bool mount(const char *sim, const char *real);
    size_t getReal(const char *sim, size_t len, char *real);
  };
}

#endif // FILE_SYS_H
