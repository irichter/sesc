/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Milos Prvulovic

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "FileSys.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
// Needed to get I()
#include "nanassert.h"
// Needed just for "fail()"
#include "EmulInit.h"
// Needed for thread suspend/resume calls
#include "OSSim.h"
// Need to access config info for mount points
#include "SescConf.h"

using std::cout;
using std::endl;

namespace FileSys {

  BaseStatus::BaseStatus(FileType type, int fd, int flags)
    : type(type), fd(fd), flags(flags){
  }
  BaseStatus::~BaseStatus(void){
    close(fd);
  }
  void BaseStatus::save(ChkWriter &out) const{
    out << "Type " << type;
    out << " Flags " << flags << " RdBlocked " << readBlockPids.size() << " WrBlocked " << writeBlockPids.size();
    for(PidSet::const_iterator i=readBlockPids.begin();i!=readBlockPids.end();i++)
      out << " " << *i;
    for(PidSet::const_iterator i=writeBlockPids.begin();i!=writeBlockPids.end();i++)
      out << " " << *i;
    out << endl;
  }
  BaseStatus *BaseStatus::create(ChkReader &in){
    size_t _type;
    in >> "Type " >> _type;
    switch(static_cast<FileType>(_type)){
    case Disk:   return new FileStatus(in);
    case Pipe:   return new PipeStatus(in);
    case Stream: return new StreamStatus(in);
    }
    return 0;
  }
  BaseStatus::BaseStatus(FileType type, ChkReader &in)
    : type(type), fd(-1){
    size_t _readBlockPids, _writeBlockPids;
    in >> " Flags " >> flags >> " Rd Blocked " >> _readBlockPids >> " WrBlocked " >> _writeBlockPids;
    while(_readBlockPids){
      int _pid;
      in >> " " >> _pid;
      readBlockPids.push_back(_pid);
      _readBlockPids--;
    }
    while(_writeBlockPids){
      int _pid;
      in >> " " >> _pid;
      writeBlockPids.push_back(_pid);
      _writeBlockPids--;
    }
    in >> endl;
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
  FileStatus::FileStatus(ChkReader &in)
    : BaseStatus(Disk,in){
    off_t _pos;
    size_t _nameLen;
    in >> "Mode " >> mode >> " Pos " >> _pos >> " NameLen " >> _nameLen;
    name=static_cast<char *>(malloc(_nameLen+1));
    in >> name >> endl;
    int fd=::open(name,flags,mode);
    I(fd!=-1);
    ::lseek(fd,_pos,SEEK_SET);
  }
  void FileStatus::save(ChkWriter &out) const{
    BaseStatus::save(out);
    out << "Mode " << mode << " Pos " << ::lseek(fd,0,SEEK_CUR) << " NameLen " << strlen(name) << " Name " << name << endl;
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
    PipeStatus *rdPipe=new PipeStatus(fdescs[0],fdescs[1],fflags[0],false);
    PipeStatus *wrPipe=new PipeStatus(fdescs[1],fdescs[0],fflags[1],true);
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
	SigInfo *sigInfo=new SigInfo(SigIO,SigCodeIn);
	sigInfo->pid=pid;
	sigInfo->data=fd;
	context->signal(sigInfo);
      }
    }
  }
  void PipeStatus::endWriteBlock(void){
    if(!otherEnd)
      return;
    while(!otherEnd->writeBlockPids.empty()){
      int pid=otherEnd->writeBlockPids.back();
      otherEnd->writeBlockPids.pop_back();
      ThreadContext *context=osSim->getContext(pid);
      if(context){
	SigInfo *sigInfo=new SigInfo(SigIO,SigCodeIn);
	sigInfo->pid=pid;
	sigInfo->data=fd;
	context->signal(sigInfo);
      }
    }
  }
  void PipeStatus::save(ChkWriter &out) const{
    BaseStatus::save(out);
    out << "IsWrite " << (isWrite?'+':'-');
    out << " OtherEnd " << (out.hasIndex(otherEnd)?'+':'-');
    if(out.hasIndex(otherEnd))
      out << out.getIndex(otherEnd);
    out << endl;
    if(!isWrite){
      I(otherEnd);
      std::vector<unsigned char> cv;
      while(true){
	unsigned char c;
	ssize_t rdRet=::read(fd,&c,sizeof(c));
	if(rdRet==-1)
	  break;
	I(rdRet==1);
	cv.push_back(c);
      }
      out << "Data size " << cv.size();
      if(cv.size()){
	out << " Data";
	for(size_t i=0;i<cv.size();i++){
	  out << " " << cv[i];
	  ssize_t wrRet=::write(otherFd,&(cv[i]),sizeof(cv[i]));
	  I(wrRet==1);
	}
      }
      out << endl;
    }
  }
  PipeStatus::PipeStatus(ChkReader &in)
    : BaseStatus(Pipe,in){
    char _isWrite;
    in >> "IsWrite " >> _isWrite;
    isWrite=(_isWrite=='+');
    char _hasIndex;
    in >> " OtherEnd " >> _hasIndex;
    if(_hasIndex=='+'){
      size_t _index;
      in >> _index;
      otherEnd=static_cast<PipeStatus *>(in.getObject(_index));
      if(otherEnd){
        otherEnd->otherEnd=this;
        fd=otherEnd->otherFd;
        otherFd=otherEnd->fd;
      }
    }else{
      int fdescs[2];
      ::pipe(fdescs);
      if(isWrite){
        fd=fdescs[1];
        otherFd=fdescs[0];
      }else{
        fd=fdescs[0];
        otherFd=fdescs[1];
      }
    }
    I(flags==fcntl(fd,F_GETFL));
    fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    in >> endl;
    if(!isWrite){
      size_t _dataSize;
      in >> "Data size " >> _dataSize;
      if(_dataSize){
        in >> " Data";
        while(_dataSize){
          char _c;
	  in >> " " >> _c;
	  ssize_t wrRet=::write(otherFd,&_c,sizeof(_c));
	  I(wrRet==1);
          _dataSize--;
        }
      }
      in >> endl;
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
			   O_NONBLOCK|O_SYNC|O_NOFOLLOW|O_DIRECT
#ifdef O_DIRECTORY
			   |O_DIRECTORY
#endif
#ifdef O_ASYNC
			   |O_ASYNC
#endif
#ifdef O_LARGEFILE
			   |O_LARGEFILE
#endif
			   ));
    if(rest){
      printf("StreamStatus::wrap of fd %d removed extra file flags 0x%08x\n",fd,rest);
      flags-=rest;
    }
    return new StreamStatus(newfd,flags);
  }
  void StreamStatus::save(ChkWriter &out) const{
    BaseStatus::save(out);    
  }
  StreamStatus::StreamStatus(ChkReader &in)
    : BaseStatus(Stream,in){
  }
  FileDesc::FileDesc(void)
    : st(0){
  }
  FileDesc::FileDesc(const FileDesc &src)
    : st(src.st), cloexec(src.cloexec){
  }
  FileDesc::~FileDesc(void){
  }
  FileDesc &FileDesc::operator=(const FileDesc &src){
    setStatus(src.st);
    setCloexec(src.cloexec);
    return *this;
  }
  void FileDesc::setStatus(BaseStatus *newSt){
    st=newSt;
  }
  void FileDesc::save(ChkWriter &out) const{
    out << "Cloexec " << (cloexec?'+':'-');
    bool hasIndex=out.hasIndex(getStatus());
    out << " Status " << out.getIndex(getStatus()) << endl;
    if(!hasIndex)
      st->save(out);
  }
  FileDesc::FileDesc(ChkReader &in){
    char _cloexec;
    in >> "Cloexec " >> _cloexec;
    cloexec=(_cloexec=='+');
    size_t _st;
    in >> " Status " >> _st >> endl;
    if(!in.hasObject(_st)){
      in.newObject(_st);
      st=BaseStatus::create(in);
      in.setObject(_st,getStatus());
    }
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
    ssize_t retVal=::read(status->fd,buf,count);
    if(retVal>0)
      status->endWriteBlock();
    return retVal;
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
  bool OpenFiles::willWriteBlock(int fd){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    if((::write(status->fd,0,1)==-1)&&(errno==EAGAIN)){
      I((::write(status->fd,&fd,1)==-1)&&(errno==EAGAIN));
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
  void OpenFiles::addWriteBlock(int fd, int pid){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    status->addWriteBlock(pid);
  }
  off_t OpenFiles::seek(int fd, off_t offset, int whence){
    if(!isOpen(fd))
      return (off_t)(error(EBADF));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    return ::lseek(status->fd,offset,whence);    
  }

  void OpenFiles::save(ChkWriter &out) const{
    out << "Descriptors: " << fds.size() << endl;
    for(size_t f=0;f<fds.size();f++)
      fds[f].save(out);
  }
  OpenFiles::OpenFiles(ChkReader &in){
    size_t _fds;
    in >> "Descriptors: " >> _fds >> endl;
    for(size_t f=0;f<_fds;f++)
      fds.push_back(FileDesc(in));
  }

  FileNames *FileNames::fileNames=0;

  FileNames* FileNames::getFileNames(void){
    if(!fileNames){
      fileNames=new FileNames();
      fileNames->cwd=getcwd(0,0);
      I(fileNames->getCwd());
      const char *mnts=SescConf->getCharPtr("FileSys","mount");
      if(*mnts==0)
	fail("Error: Section FileSys entry mounts is empty\n");
      do{
	const char *mend=strchr(mnts,':');
	size_t mlen=(mend?(mend-mnts):strlen(mnts));
	char buf[mlen+1];
	strncpy(buf,mnts,mlen);
	buf[mlen]=0;
	char *beql=strchr(buf,'=');
	if(!beql)
	  fail("No '=' in section FileSys entry mount for %s\n",buf);
	*beql++=0;
	fileNames->mount(buf,beql);
	mnts+=mlen;
      }while(*mnts++!=0);
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
