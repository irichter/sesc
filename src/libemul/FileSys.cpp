#include "FileSys.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
// Needed to get I()
#include "nanassert.h"
// Needed just for "fail()"
#include "EmulInit.h"
// Needed for thread suspend/resume calls
#include "OSSim.h"

namespace FileSys {

  BaseStatus::BaseStatus(FileType type, int fd, int flags)
    : type(type), fd(fd), flags(flags){
  }
  BaseStatus::~BaseStatus(void){
    close(fd);
  }
  FileStatus::FileStatus(const char *name, int fd, int flags, mode_t mode)
    : BaseStatus(Disk,fd,flags), name(strdup(name)), mode(mode){
  }
  FileStatus::~FileStatus(void){
    free(const_cast<char *>(name));
  }
  FileStatus *FileStatus::open(const char *name, int flags, mode_t mode){
    int fd=::open(name,flags,mode);
    if(fd==-1)
      return 0;
    FileStatus *retVal=new FileStatus(name,fd,flags,mode);
    if(!retVal)
      fail("FileSys::FileStatus::open can not create a new FileStatus\n");
    return retVal;
  }
  void PipeStatus::setOtherEnd(PipeStatus *oe){
    I(!otherEnd);
    I((!oe->otherEnd)||(oe->otherEnd==this));
    otherEnd=oe;
  }
  PipeStatus *PipeStatus::pipe(void){
    int fdescs[2];
    if(::pipe(fdescs))
      return 0;
    int fflags[2];
    fflags[0]=fcntl(fdescs[0],F_GETFL);
    fcntl(fdescs[0],F_SETFL,fflags[0]|O_NONBLOCK);
    fflags[1]=fcntl(fdescs[1],F_GETFL);
    fcntl(fdescs[1],F_SETFL,fflags[1]|O_NONBLOCK);
    if((fflags[0]==-1)||(fflags[1]==-1))
      fail("FileSys::PipeStatus::open could not fcntl F_GETFL\n");
    PipeStatus *rdPipe=new PipeStatus(fdescs[0],fflags[0],false);
    PipeStatus *wrPipe=new PipeStatus(fdescs[1],fflags[1],true);
    if((!rdPipe)||(!wrPipe))
      fail("FileSys::PipeStatus::pipe can not create a new PipeStatus\n");
    wrPipe->setOtherEnd(rdPipe);
    rdPipe->setOtherEnd(wrPipe);
    return rdPipe;
  }
  void PipeStatus::endReadBlock(void){
    if(!otherEnd)
      return;
    while(!otherEnd->readBlockPids.empty()){
      int pid=otherEnd->readBlockPids.back();
      otherEnd->readBlockPids.pop_back();
      ThreadContext *context=osSim->getContext(pid);
      if(context){
	SignalState *sigState=context->getSignalState();
	SigInfo *sigInfo=new SigInfo(SigIO,SigCodeIn);
	sigInfo->pid=pid;
	sigInfo->data=fd;
	sigState->raise(sigInfo);
      }
    }
  }
  StreamStatus::StreamStatus(int fd, int flags)
    : BaseStatus(Stream,fd,flags){
  }
  StreamStatus::~StreamStatus(void){
  }
  StreamStatus *StreamStatus::wrap(int fd){
    int newfd=dup(fd);
    if(newfd==-1)
      return 0;
    int flags=fcntl(newfd,F_GETFL);
    if(flags==-1){
      close(fd);
      return 0;
    }
    int rest=flags^(flags&(O_ACCMODE|O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_APPEND|
			   O_NONBLOCK|O_SYNC|O_NOFOLLOW|O_DIRECTORY|O_DIRECT|O_ASYNC|O_LARGEFILE));
    if(rest){
      printf("StreamStatus::wrap of fd %d removed extra file flags 0x%08x\n",fd,rest);
      flags-=rest;
    }
    return new StreamStatus(newfd,flags);
  }
  FileDesc::FileDesc(void)
    : st(0){
  }
  FileDesc::FileDesc(const FileDesc &src)
    : st(src.st), cloexec(src.cloexec){
    if(st)
      st->addRef();
  }
  FileDesc::~FileDesc(void){
    I((!st)||(st->getRefCount()>0));
    if(st)
      st->delRef();
  }
  FileDesc &FileDesc::operator=(const FileDesc &src){
    setStatus(src.st);
    setCloexec(src.cloexec);
    return *this;
  }
  void FileDesc::setStatus(BaseStatus *newSt){
    if(newSt)
      newSt->addRef();
    if(st){
      I(st->getRefCount()>0);
      if(st->getRefCount()==1)
	st->endReadBlock();
      st->delRef();
    }
    st=newSt;
  }
  OpenFiles::OpenFiles(void)
    : fds(){ 
  }
  OpenFiles::OpenFiles(const OpenFiles &src)
    : GCObject(), fds(src.fds){
  }
  OpenFiles::~OpenFiles(void){
    for(size_t i=0;i<fds.size();i++)
      fds[i].setStatus(0);
  }
  int OpenFiles::error(int err){
    errno=err;
    return -1;
  }
  bool OpenFiles::isOpen(int fd) const{
    I(fd>=0);
    if(fds.size()<=(size_t)fd)
      return false;
    if(!fds[fd].getStatus())
      return false;
    return true;      
  }
  FileDesc *OpenFiles::getDesc(int fd){
    I(fd>=0);
    if(fds.size()<=(size_t)fd)
      fds.resize(fd+1);
    return &(fds[fd]);
  }
  int OpenFiles::open(const char *name, int flags, mode_t mode){
    int newfd=findNextFree(0);
    FileDesc *fileDesc=getDesc(newfd);
    FileStatus *fileStat=FileStatus::open(name,flags,mode);
    if(!fileStat)
      return -1;
    fileDesc->setStatus(fileStat);
    fileDesc->setCloexec(false);
    return newfd;
  }
  int OpenFiles::dup(int oldfd){
    int newfd=findNextFree(0);
    return dup2(oldfd,newfd);
  }
  int OpenFiles::dup2(int oldfd, int newfd){
    if(!isOpen(oldfd))
      return error(EBADF);
    if(newfd==oldfd)
      return newfd;
    // Must first get the newDesc becasue it may resize the
    // vector, which can move oldDesc elesewhere
    FileDesc *newDesc=getDesc(newfd);
    FileDesc *oldDesc=getDesc(oldfd);
    I(oldDesc->getStatus());
    newDesc->setStatus(oldDesc->getStatus());
    I(newDesc->getStatus());
    I(newDesc->getStatus()==oldDesc->getStatus());
    newDesc->setCloexec(false);
    I(isOpen(newfd));
    return newfd;
  }
  int OpenFiles::dupfd(int oldfd, int minfd){
    int newfd=findNextFree(minfd);
    return dup2(oldfd,newfd);
  }
  int OpenFiles::pipe(int fds[2]){
    PipeStatus *newStat[2];
    newStat[0]=PipeStatus::pipe();
    if(!newStat[0])
      return -1;
    newStat[1]=newStat[0]->otherEnd;
    I(newStat[1]&&(newStat[1]->otherEnd==newStat[0]));
    for(int i=0;i<2;i++){
      fds[i]=findNextFree(3);
      FileDesc *desc=getDesc(fds[i]);
      desc->setStatus(newStat[i]);
      desc->setCloexec(false);
    }
    return 0;
  }
  bool OpenFiles::isLastOpen(int oldfd){
    if(!isOpen(oldfd))
      return false;
    return (getDesc(oldfd)->getStatus()->getRefCount()==1);
  }
  int OpenFiles::close(int oldfd){
    if(!isOpen(oldfd))
      return error(EBADF);
    getDesc(oldfd)->setStatus(0);
    return 0;
  }
  int OpenFiles::getfd(int fd){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    return desc->getCloexec()?FD_CLOEXEC:0;
  }
  int OpenFiles::setfd(int fd, int cloex){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    desc->setCloexec((cloex&FD_CLOEXEC)==FD_CLOEXEC);
    return 0;
  }
  int OpenFiles::getfl(int fd){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    return desc->getStatus()->flags;
  }
  ssize_t OpenFiles::read(int fd, void *buf, size_t count){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    ID(bool rdBlock=willReadBlock(fd));
    ssize_t retVal=::read(status->fd,buf,count);
    I(rdBlock==((retVal==-1)&&(errno==EAGAIN)));
    return retVal;
  }
  bool OpenFiles::willReadBlock(int fd){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    if((::read(status->fd,0,1)==-1)&&(errno==EAGAIN)){
      I((::read(status->fd,&fd,1)==-1)&&(errno==EAGAIN));
      return true;
    }
    return false;
  }
  void OpenFiles::addReadBlock(int fd, int pid){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    status->addReadBlock(pid);
  }
  ssize_t OpenFiles::write(int fd, const void *buf, size_t count){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    ssize_t retVal=::write(status->fd,buf,count);
    if(retVal>0)
      status->endReadBlock();
    return retVal;
  }
  off_t OpenFiles::seek(int fd, off_t offset, int whence){
    if(!isOpen(fd))
      return (off_t)(error(EBADF));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    return ::lseek(status->fd,offset,whence);    
  }

  FileNames *FileNames::fileNames;

  FileNames* FileNames::getFileNames(void){
    if(!fileNames){
      fileNames=new FileNames();
      fileNames->cwd=getcwd(0,0);
      I(fileNames->getCwd());
      fileNames->mount("/bin","/net/hc291/milos/sim/apps/system/bin");
    }
    return fileNames;
  }

  bool FileNames::setCwd(const char *newcwd){
    char *mycwd=strdup(newcwd);
    if(!mycwd)
      return false;
    I(cwd);
    free(cwd);
    cwd=mycwd;
    return true;
  }
  
  bool FileNames::mount(const char *sim, const char *real){
    char *simCopy=strdup(sim);
    if(!simCopy)
      return false;
    char *realCopy=strdup(real);
    if(!realCopy){
      free(simCopy);
      return false;
    }
    if(!simToReal.insert(StringMap::value_type(simCopy,realCopy)).second){
      free(simCopy);
      free(realCopy);
      return false;
    }
    return true;
  }

  size_t FileNames::getReal(const char *sim, size_t len, char *real){
    char  *buf=strcpy((char *)(alloca(strlen(sim)+1)),sim);
    if(buf[0]!='/'){
      char *tmp=(char *)(alloca(strlen(cwd)+1+strlen(buf)+1));
      strcpy(tmp,cwd);
      strcat(tmp,"/");
      if(strncmp(buf,"./",2)==0)
	buf+=2;
      buf=strcat(tmp,buf);
    }
    char *curslash=strrchr(buf,'/');
    while(curslash!=0){
      *curslash=(char)0;
      if(simToReal.find(buf)!=simToReal.end()){
	const char *mnt=simToReal[buf];
	*curslash='/';
	buf=(char *)(alloca(strlen(mnt)+strlen(curslash)+1));
	strcpy(buf,mnt);
	strcat(buf,curslash);
	break;
      }
      char *nextslash=strrchr(buf,'/');
      *curslash='/';
      curslash=nextslash;
    }
    size_t buflen=strlen(buf)+1;
    if(len>=buflen)
      strcpy(real,buf);
    return buflen;
  }
  
}
