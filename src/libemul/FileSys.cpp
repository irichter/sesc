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
#include <poll.h>
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

  BaseStatus::BaseStatus(FileType type, int32_t fd, int32_t flags)
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
      int32_t _pid;
      in >> " " >> _pid;
      readBlockPids.push_back(_pid);
      _readBlockPids--;
    }
    while(_writeBlockPids){
      int32_t _pid;
      in >> " " >> _pid;
      writeBlockPids.push_back(_pid);
      _writeBlockPids--;
    }
    in >> endl;
  }
  FileStatus::FileStatus(const char *name, int32_t fd, int32_t flags, mode_t mode)
    : BaseStatus(Disk,fd,flags), name(strdup(name)), mode(mode){
  }
  FileStatus::~FileStatus(void){
    free(const_cast<char *>(name));
  }
  FileStatus *FileStatus::open(const char *name, int32_t flags, mode_t mode){
    int32_t fd=::open(name,flags,mode);
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
    int32_t fd=::open(name,flags,mode);
    I(fd!=-1);
    ::lseek(fd,_pos,SEEK_SET);
  }
  void FileStatus::save(ChkWriter &out) const{
    BaseStatus::save(out);
    out << "Mode " << mode << " Pos " << ::lseek(fd,0,SEEK_CUR) << " NameLen " << strlen(name) << " Name " << name << endl;
  }
  void FileStatus::mmap(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs){
    I(0);
    off_t curoff=::lseek(fd,0,SEEK_CUR);
    I(curoff!=(off_t)-1);
    off_t endoff=::lseek(fd,0,SEEK_END);
    I(endoff!=(off_t)-1);
    off_t finoff=::lseek(fd,curoff,SEEK_SET);
    I(finoff==curoff);
    if(offs>=endoff){
      memset(data,0,size);
    }else if(offs+(ssize_t)size<=endoff){
      ssize_t nbytes=::pread(fd,data,size,offs);
      I(nbytes==(ssize_t)size);
    }else{
      ssize_t nbytes=::pread(fd,data,endoff-offs,offs);
      I(nbytes==endoff-offs);
      memset((void *)((char *)data+nbytes),0,size-nbytes);
    }
    // TODO: Remember the mapping
  }
  void FileStatus::munmap(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs){
    // TODO: Remove the mapping
  }
  void FileStatus::msync(MemSys::FrameDesc *frame, void *data, size_t size, off_t offs){
    off_t curoff=::lseek(fd,0,SEEK_CUR);
    I(curoff!=(off_t)-1);
    off_t endoff=::lseek(fd,0,SEEK_END);
    I(endoff!=(off_t)-1);
    off_t finoff=::lseek(fd,curoff,SEEK_SET);
    I(finoff==curoff);
    if(offs>=endoff){
      // Do nothing
    }else if(offs+(ssize_t)size<=endoff){
      ssize_t nbytes=::pwrite(fd,data,size,offs);
      I(nbytes==(ssize_t)size);
    }else{
      ssize_t nbytes=::pwrite(fd,data,endoff-offs,offs);
      I(nbytes==endoff-offs);
    }    
  }
 
  void PipeStatus::setOtherEnd(PipeStatus *oe){
    I(!otherEnd);
    I((!oe->otherEnd)||(oe->otherEnd==this));
    otherEnd=oe;
  }
  PipeStatus *PipeStatus::pipe(void){
    int32_t fdescs[2];
    if(::pipe(fdescs))
      return 0;
    int32_t fflags[2];
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
      int32_t pid=otherEnd->readBlockPids.back();
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
      int32_t pid=otherEnd->writeBlockPids.back();
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
      std::vector<uint8_t> cv;
      while(true){
	uint8_t c;
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
      int32_t fdescs[2];
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

  StreamStatus::StreamStatus(int32_t fd, int32_t flags)
    : BaseStatus(Stream,fd,flags){
  }
  StreamStatus::~StreamStatus(void){
  }
  StreamStatus *StreamStatus::wrap(int32_t fd){
    int32_t newfd=dup(fd);
    if(newfd==-1)
      return 0;
    int32_t flags=fcntl(newfd,F_GETFL);
    if(flags==-1){
      close(fd);
      return 0;
    }
    int32_t rest=flags^(flags&(O_ACCMODE|O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_APPEND|
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
  int32_t OpenFiles::error(int32_t err){
    errno=err;
    return -1;
  }
  bool OpenFiles::isOpen(int32_t fd) const{
    I(fd>=0);
    if(fds.size()<=(size_t)fd)
      return false;
    if(!fds[fd].getStatus())
      return false;
    return true;      
  }
  FileDesc *OpenFiles::getDesc(int32_t fd){
    I(fd>=0);
    if(fds.size()<=(size_t)fd)
      fds.resize(fd+1);
    return &(fds[fd]);
  }
  int32_t OpenFiles::open(const char *name, int32_t flags, mode_t mode){
    int32_t newfd=findNextFree(0);
    FileDesc *fileDesc=getDesc(newfd);
    FileStatus *fileStat=FileStatus::open(name,flags,mode);
    if(!fileStat)
      return -1;
    fileDesc->setStatus(fileStat);
    fileDesc->setCloexec(false);
    return newfd;
  }
  int32_t OpenFiles::dup(int32_t oldfd){
    int32_t newfd=findNextFree(0);
    return dup2(oldfd,newfd);
  }
  int32_t OpenFiles::dup2(int32_t oldfd, int32_t newfd){
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
  int32_t OpenFiles::dupfd(int32_t oldfd, int32_t minfd){
    int32_t newfd=findNextFree(minfd);
    return dup2(oldfd,newfd);
  }
  int32_t OpenFiles::pipe(int32_t fds[2]){
    PipeStatus *newStat[2];
    newStat[0]=PipeStatus::pipe();
    if(!newStat[0])
      return -1;
    newStat[1]=newStat[0]->otherEnd;
    I(newStat[1]&&(newStat[1]->otherEnd==newStat[0]));
    for(int32_t i=0;i<2;i++){
      fds[i]=findNextFree(3);
      FileDesc *desc=getDesc(fds[i]);
      desc->setStatus(newStat[i]);
      desc->setCloexec(false);
    }
    return 0;
  }
  bool OpenFiles::isLastOpen(int32_t oldfd){
    if(!isOpen(oldfd))
      return false;
    return (getDesc(oldfd)->getStatus()->getRefCount()==1);
  }
  int32_t OpenFiles::close(int32_t oldfd){
    if(!isOpen(oldfd))
      return error(EBADF);
    getDesc(oldfd)->setStatus(0);
    return 0;
  }
  int32_t OpenFiles::getfd(int32_t fd){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    return desc->getCloexec()?FD_CLOEXEC:0;
  }
  int32_t OpenFiles::setfd(int32_t fd, int32_t cloex){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    desc->setCloexec((cloex&FD_CLOEXEC)==FD_CLOEXEC);
    return 0;
  }
  int32_t OpenFiles::getfl(int32_t fd){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    return desc->getStatus()->flags;
  }
  ssize_t OpenFiles::read(int32_t fd, void *buf, size_t count){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    ssize_t retVal=::read(status->fd,buf,count);
    if(retVal>0)
      status->endWriteBlock();
    return retVal;
  }
  ssize_t OpenFiles::pread(int32_t fd, void *buf, size_t count, off_t offs){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    ssize_t retVal=::pread(status->fd,buf,count,offs);
    if(retVal>0)
      status->endWriteBlock();
    return retVal;
  }
  ssize_t OpenFiles::write(int32_t fd, const void *buf, size_t count){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    ssize_t retVal=::write(status->fd,buf,count);
    if(retVal>0)
      status->endReadBlock();
    return retVal;
  }
  ssize_t OpenFiles::pwrite(int32_t fd, void *buf, size_t count, off_t offs){
    if(!isOpen(fd))
      return error(EBADF);
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    ssize_t retVal=::pwrite(status->fd,buf,count,offs);
    if(retVal>0)
      status->endReadBlock();
    return retVal;
  }
  bool OpenFiles::willReadBlock(int32_t fd){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    struct pollfd pollFd;
    pollFd.fd=status->fd;
    pollFd.events=POLLIN;
    int32_t res=poll(&pollFd,1,0);
//    I((res<=0)==((::read(status->fd,0,1)==-1)&&(errno==EAGAIN)));
    return (res<=0);
//    if((::read(status->fd,0,1)==-1)&&(errno==EAGAIN)){
//      I((::read(status->fd,&fd,1)==-1)&&(errno==EAGAIN));
//      return true;
//    }
//    return false;
  }
  bool OpenFiles::willWriteBlock(int32_t fd){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    if((::write(status->fd,0,1)==-1)&&(errno==EAGAIN)){
      I((::write(status->fd,&fd,1)==-1)&&(errno==EAGAIN));
      return true;
    }
    return false;
  }
  void OpenFiles::addReadBlock(int32_t fd, int32_t pid){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    status->addReadBlock(pid);
  }
  void OpenFiles::addWriteBlock(int32_t fd, int32_t pid){
    I(isOpen(fd));
    FileDesc *desc=getDesc(fd);
    BaseStatus *status=desc->getStatus();
    status->addWriteBlock(pid);
  }
  off_t OpenFiles::seek(int32_t fd, off_t offset, int32_t whence){
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

  NameSpace::NameSpace(const string &mtlist) : GCObject(), mounts() {
    string::size_type bpos=0;
    while(bpos!=string::npos){
      string::size_type mpos=mtlist.find('=',bpos);
      string::size_type epos=mtlist.find(':',bpos);
      mounts[string(mtlist,bpos,mpos-bpos)]=string(mtlist,mpos+1,epos-mpos-1);
      bpos=epos+((epos==string::npos)?0:1);
    }
  }
  NameSpace::NameSpace(const NameSpace &src) : GCObject(), mounts() {
    fail("FileSys::NameSpace copying not supported!\n");
  }
  const string NameSpace::toNative(const string &fname) const{
    Mounts::const_iterator it=mounts.lower_bound(fname);
    if(it==mounts.end())
      return fname;
    const string &simmap=it->first;
    if(fname.compare(0,simmap.length(),simmap)!=0)
      return fname;
    if(fname.length()==simmap.length())
      return it->second;
    if(string(fname,simmap.length(),1)!="/")
      return fname;
    return it->second + string(fname,simmap.length());
  }
  FileSys::FileSys(NameSpace *ns, const string &cwd)
    : GCObject()
    , nameSpace(ns)
    , cwd(cwd){
  }
  FileSys::FileSys(const FileSys &src, bool newNameSpace)
    : GCObject()
    , nameSpace(newNameSpace?(NameSpace *)(new NameSpace(*(src.nameSpace))):(NameSpace *)(src.nameSpace))
    , cwd(src.cwd){
  }
  void FileSys::setCwd(const string &newCwd){
    cwd=normalize(newCwd);
  }
  const string FileSys::normalize(const string &fname) const{
    string rv(fname);
    if(string(fname,0,1)!="/")
      rv=cwd+"/"+rv;
    while(true){
      string::size_type pos=rv.find("//");
      if(pos==string::npos)
        break;
      rv.erase(pos,1);
    }
    while(true){
      string::size_type pos=rv.find("/./");
      if(pos==string::npos)
        break;
      rv.erase(pos,2);
    }
    while(true){
      string::size_type epos=rv.find("/../");
      if(epos==string::npos)
        break;
      string::size_type bpos=(epos==0)?0:rv.rfind('/',epos-1);
      if(bpos==string::npos)
        fail("Found /../ with no /dir before it\n"); 
      rv.erase(bpos,epos-bpos+3);
    }
    return rv;
  }
  const string FileSys::toNative(const string &fname) const{
    return nameSpace->toNative(normalize(fname));
  }

}
