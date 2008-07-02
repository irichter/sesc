#if !(defined FILE_SYS_H)
#define FILE_SYS_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <deque>
#include <string>
#include <vector>
#include <map>
#include "GCObject.h"
#include "Checkpoint.h"
#include "SignalHandling.h"

#define OLD_FDESC

namespace MemSys{
  class FrameDesc;
}

// Included only for "struct pollfd"
#include <poll.h>
namespace FileSys {
  // Type of a file descriptor
  typedef typeof(((struct pollfd *)0)->fd) fd_t;
}
namespace FileSys {

  // Type of flags (O_RDONLY, etc.) 
  typedef int32_t flags_t;

  typedef enum {
    Disk,
    Pipe,
    Stream,
  } FileType;

  class Node{
  private:
    dev_t  dev;
    ino_t  ino;
    uid_t  uid;
    gid_t  gid;
    mode_t mode;
  protected:
    Node(dev_t dev, ino_t ino, uid_t uid, gid_t gid, mode_t mode);
  public:
    virtual ~Node(void);
    dev_t getDev(void) const{ return dev; }
    ino_t getIno(void) const{ return ino; }
    uid_t getUid(void) const{ return uid; }
    gid_t getGid(void) const{ return gid; }
    mode_t getMode(void) const{ return mode; }
    virtual std::string getName(void) const = 0;
  };
  class Description : public GCObject{
  public:
    typedef SmartPtr<Description> pointer;
  protected:
    Node *node;
    flags_t flags;
    Description(Node *node, flags_t flags);
  public:
    static Description *open(const std::string &name, flags_t flags, mode_t mode);
    virtual ~Description(void);
    const Node *getNode(void) const{ return node; }
    virtual std::string getName(void) const;
    virtual bool canRd(void) const;
    virtual bool canWr(void) const;
    virtual bool isNonBlock(void) const;
    virtual flags_t getFlags(void) const;
    virtual ssize_t read(void *buf, size_t count) = 0;
    virtual ssize_t write(const void *buf, size_t count) = 0;
  };
  class NullNode : public Node{
  protected:
    NullNode(void);
    static NullNode node;
  public:
    static NullNode *create(void);
    virtual std::string getName(void) const;
  };
  class NullDescription : public Description{
  public:
    NullDescription(flags_t flags);
    virtual ssize_t read(void *buf, size_t count);
    virtual ssize_t write(const void *buf, size_t count);
  };
  class SeekableNode : public Node{
  private:
    off_t       len;
  protected:
    SeekableNode(dev_t dev, ino_t ino, uid_t uid, gid_t gid, mode_t mode, off_t len);
  public:
    virtual off_t getSize(void) const;
    virtual void  setSize(off_t nlen);
    virtual ssize_t pread(void *buf, size_t count, off_t offs) = 0;
    virtual ssize_t pwrite(const void *buf, size_t count, off_t offs) = 0;
  };
  class SeekableDescription : public Description{
  public:
    typedef SmartPtr<SeekableDescription> pointer;
  private:
    off_t pos;
  protected:
    SeekableDescription(Node *node, flags_t flags);
  public:
    virtual off_t getSize(void) const;
    virtual void  setSize(off_t nlen);
    virtual off_t getPos(void) const;
    virtual void  setPos(off_t npos);
    virtual ssize_t read(void *buf, size_t count);
    virtual ssize_t write(const void *buf, size_t count);
    virtual ssize_t pread(void *buf, size_t count, off_t offs);
    virtual ssize_t pwrite(const void *buf, size_t count, off_t offs);
    virtual void mmap(void *data, size_t size, off_t offs);
    virtual void msync(void *data, size_t size, off_t offs);
  };
  class FileNode : public SeekableNode, public GCObject{
  private:
    std::string name;
    fd_t fd;
  public:
    FileNode(const std::string &name, fd_t fd, struct stat &buf);
    virtual ~FileNode(void);
    virtual std::string getName(void) const;
    virtual void setSize(off_t nlen);
    virtual ssize_t pread(void *buf, size_t count, off_t offs);
    virtual ssize_t pwrite(const void *buf, size_t count, off_t offs);
  };
  class FileDescription : public SeekableDescription{
  public:
    FileDescription(FileNode *node, flags_t flags);
    virtual ~FileDescription(void);
  };
  class DirectoryNode : public SeekableNode, public GCObject{
  private:
    std::string name;
    DIR *dirp;
    typedef std::vector<std::string> Entries;
    Entries entries;
  public:
    DirectoryNode(const std::string &name, fd_t fd, struct stat &buf);
    virtual ~DirectoryNode(void);
    virtual std::string getName(void) const;
    virtual void  setSize(off_t nlen);
    virtual ssize_t pread(void *buf, size_t count, off_t offs);
    virtual ssize_t pwrite(const void *buf, size_t count, off_t offs);
    virtual void refresh(void);
    virtual std::string getEntry(off_t index);
  };
  class DirectoryDescription : public SeekableDescription{
  public:
    DirectoryDescription(DirectoryNode *node, flags_t flags);
    virtual ~DirectoryDescription(void);
    virtual void setPos(off_t npos);
    virtual std::string readDir(void);
  };
  class StreamNode : public Node{
  protected:
    dev_t rdev;
    class PidSet : public std::vector<int>{};
    // Threads blocked on a read from this file
    PidSet rdBlocked;
    // Threads blocked on a write from this file 
    PidSet wrBlocked;
    void unblock(PidSet &pids, SigCode sigCode);
  public:
    StreamNode(dev_t dev, dev_t rdev, ino_t ino, uid_t uid, gid_t gid, mode_t mode);
    virtual ~StreamNode(void);
    dev_t getRdev(void) const{ return rdev; }
    virtual bool willRdBlock(void) const = 0;
    virtual bool willWrBlock(void) const = 0;
    void rdBlock(pid_t pid);
    void wrBlock(pid_t pid);
    void rdUnblock(void);
    void wrUnblock(void);
    virtual ssize_t read(void *buf, size_t count) = 0;
    virtual ssize_t write(const void *buf, size_t count) = 0;
  };
  class StreamDescription : public Description{
  protected:
    StreamDescription(StreamNode *node, flags_t flags);
  public:
    bool willRdBlock(void) const;
    bool willWrBlock(void) const;
    void rdBlock(pid_t pid);
    void wrBlock(pid_t pid);
    virtual ssize_t read(void *buf, size_t count);
    virtual ssize_t write(const void *buf, size_t count);
  };
  class PipeNode : public StreamNode{
    typedef std::deque<uint8_t> Data;
    Data data;
    size_t readers;
    size_t writers;
  public:
    PipeNode(void);
    virtual ~PipeNode(void);
    virtual std::string getName(void) const;
    virtual void addReader(void);
    virtual void delReader(void);
    virtual void addWriter(void);
    virtual void delWriter(void);
    virtual ssize_t read(void *buf, size_t count);
    virtual ssize_t write(const void *buf, size_t count);
    virtual bool willRdBlock(void) const;
    virtual bool willWrBlock(void) const;    
  };
  class PipeDescription : public StreamDescription{
  public:
    PipeDescription(PipeNode *node, flags_t flags);
    virtual ~PipeDescription(void);
  };
  class TtyNode : public StreamNode, public GCObject{
  private:
    fd_t fd;
  public:
    TtyNode(fd_t srcfd);
    ~TtyNode(void);
    virtual std::string getName(void) const;
    virtual ssize_t read(void *buf, size_t count);
    virtual ssize_t write(const void *buf, size_t count);
    virtual bool willRdBlock(void) const;
    virtual bool willWrBlock(void) const;    
  };
  class TtyDescription : public StreamDescription{
  protected:
    TtyDescription(TtyNode *node, flags_t flags);
    virtual ~TtyDescription(void);
  public:
    static TtyDescription *wrap(fd_t fd);
  };

  class OpenFiles : public GCObject{
  public:
    typedef SmartPtr<OpenFiles> pointer;
  private:
    struct FileDescriptor{
      // The underlying file description
      Description::pointer description;
      // Cloexec flag for this descriptor
      bool                cloexec;
      FileDescriptor(void);
    };
    typedef std::map<fd_t,FileDescriptor> FileDescriptors;
    FileDescriptors fileDescriptors;
  public:
    OpenFiles(void);
    OpenFiles(const OpenFiles &src);
    ~OpenFiles(void);
    // Returns the first available (non-open) fd whose number is at least minfd
    fd_t nextFreeFd(fd_t minfd) const;
    // Opens fd and associates it with the given description
    void openDescriptor(fd_t fd, Description *description);
    // Closes fd
    void closeDescriptor(fd_t fd);
    // Returns true iff the fd is open
    bool isOpen(fd_t fd) const;
    // Returns the file description that corresponds to fd
    Description *getDescription(fd_t fd);
    // Sets the cloexec flag associated with fd
    void setCloexec(fd_t fd, bool cloex);
    // Returns the cloexec flag associated with fd
    bool getCloexec(fd_t fd) const;
    // Closes all file descriptors whose cloexec flag is set
    void exec(void);
    // Checkpointing save/restore 
    void save(ChkWriter &out) const;
    OpenFiles(ChkReader &in);
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
