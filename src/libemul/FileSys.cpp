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
  Node::Node(dev_t dev, ino_t ino, uid_t uid, gid_t gid, mode_t mode)
    : dev(dev), ino(ino), uid(uid), gid(gid), mode(mode){
  }
  Node::~Node(void){
  }
  Description *Description::open(const std::string &name, flags_t flags, mode_t mode){
    if(name.compare("/dev/null")==0)
      return new NullDescription(flags);
    if(name.find("/dev/")==0){
      errno=ENOENT;
      return 0;
    }
    fd_t fd=::open(name.c_str(),flags,mode);
    if(fd==-1)
      return 0;
    struct stat stbuf;
    if(fstat(fd,&stbuf)!=0)
      fail("Description::open could not stat %s\n",name.c_str());
    switch(stbuf.st_mode&S_IFMT){
    case S_IFREG: {
      FileNode *fnode=new FileNode(name,fd,stbuf);
      I(fnode);
      FileDescription *desc=new FileDescription(fnode,flags);
      I(desc);
      return desc;
    } break;
    case S_IFDIR: {
      DirectoryNode *dnode=new DirectoryNode(name,fd,stbuf);
      I(dnode);
      DirectoryDescription *desc=new DirectoryDescription(dnode,flags);
      I(desc);
      return desc;
    } break;
    }
    fail("Description::open for unknown st_mode\n");
  }
  Description::Description(Node *node, flags_t flags)
    : node(node), flags(flags){
  }
  Description::~Description(void){
  }
  std::string Description::getName(void) const{
    return node->getName();
  }
  bool Description::canRd(void) const{
    return ((flags&O_ACCMODE)!=O_WRONLY);
  }
  bool Description::canWr(void) const{
    return ((flags&O_ACCMODE)!=O_RDONLY);
  }
  bool Description::isNonBlock(void) const{
    return ((flags&O_NONBLOCK)==O_NONBLOCK);
  }
  flags_t Description::getFlags(void) const{
    return flags;
  }
  NullNode::NullNode()
    : Node(0x000d,0,0,0,S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH){
  }
  NullNode NullNode::node;
  NullNode *NullNode::create(void){
    return &node;
  }
  std::string NullNode::getName(void) const{
    return "NullDevice";
  }
  NullDescription::NullDescription(flags_t flags)
    : Description(NullNode::create(),flags){
  }
  ssize_t NullDescription::read(void *buf, size_t count){
    return 0;
  }
  ssize_t NullDescription::write(const void *buf, size_t count){
    return count;
  }
  SeekableNode::SeekableNode(dev_t dev, ino_t ino, uid_t uid, gid_t gid, mode_t mode, off_t len)
    : Node(dev,ino,uid,gid,mode), len(len){
  }
  off_t SeekableNode::getSize(void) const{
    return len;
  }
  void  SeekableNode::setSize(off_t nlen){
    len=nlen;
  }
  SeekableDescription::SeekableDescription(Node *node, flags_t flags)
    : Description(node,flags), pos(0){
  }
  off_t SeekableDescription::getSize(void) const{
    return dynamic_cast<SeekableNode *>(node)->getSize();
  }
  void  SeekableDescription::setSize(off_t nlen){
    dynamic_cast<SeekableNode *>(node)->setSize(nlen);
  }
  off_t SeekableDescription::getPos(void) const{
    return pos;
  }
  void  SeekableDescription::setPos(off_t npos){
    pos=npos;
  }
  ssize_t SeekableDescription::read(void *buf, size_t count){
    ssize_t rcount=pread(buf,count,pos);
    if(rcount>0){
      pos+=rcount;
      I(pos<=getSize());
    }
    return rcount;
  }
  ssize_t SeekableDescription::write(const void *buf, size_t count){
    ssize_t wcount=pwrite(buf,count,pos);
    if(wcount>0){
      pos+=wcount;
      I(pos<=getSize());
    }
    return wcount;
  }
  ssize_t SeekableDescription::pread(void *buf, size_t count, off_t offs){
    return dynamic_cast<SeekableNode *>(node)->pread(buf,count,offs);
  }
  ssize_t SeekableDescription::pwrite(const void *buf, size_t count, off_t offs){
    return dynamic_cast<SeekableNode *>(node)->pwrite(buf,count,offs);
  }
  void SeekableDescription::mmap(void *data, size_t size, off_t offs){
    ssize_t rsize=pread(data,size,offs);
    if(rsize<0)
      fail("FileStatus::mmap failed with error %d\n",errno);
    memset((void *)((char *)data+rsize),0,size-rsize);
  }
  void SeekableDescription::msync(void *data, size_t size, off_t offs){
    off_t endoff=getSize();
    if(offs>=endoff)
      return;
    size_t wsize=(size_t)(endoff-offs);
    if(size<wsize)
      wsize=size;
    ssize_t nbytes=pwrite(data,wsize,offs);
    I(getSize()==endoff);
    I(nbytes==(ssize_t)wsize);
  }
  FileNode::FileNode(const std::string &name, fd_t fd, struct stat &buf)
    : SeekableNode(buf.st_dev,buf.st_ino,buf.st_uid,buf.st_gid,buf.st_mode,buf.st_size), name(name), fd(fd) {
  }
  FileNode::~FileNode(void){
    if(close(fd)!=0)
      fail("FileSys::FileNode destructor could not close file %s\n",name.c_str());
  }
  std::string FileNode::getName(void) const{
    return name;
  }
  void FileNode::setSize(off_t nlen){
    if(ftruncate(fd,nlen)==-1)
      fail("FileNode::setSize ftruncate failed\n");
    SeekableNode::setSize(nlen);
  }
  ssize_t FileNode::pread(void *buf, size_t count, off_t offs){
    return ::pread(fd,buf,count,offs);
  }
  ssize_t FileNode::pwrite(const void *buf, size_t count, off_t offs){
    off_t olen=getSize();
    ssize_t ncount=::pwrite(fd,buf,count,offs);
    if((ncount>0)&&(offs+ncount>olen))
      SeekableNode::setSize(offs+ncount);
    return ncount;
  }
  FileDescription::FileDescription(FileNode *node, flags_t flags)
    : SeekableDescription(node,flags){
    node->addRef();
  }
  FileDescription::~FileDescription(void){
    dynamic_cast<FileNode *>(node)->delRef();
  }
  DirectoryNode::DirectoryNode(const std::string &name, fd_t fd, struct stat &buf)
    : SeekableNode(buf.st_dev,buf.st_ino,buf.st_uid,buf.st_gid,buf.st_mode,0), name(name), dirp(opendir(name.c_str())){
    if(!dirp)
      fail("FileSys::DirectoryNode constructor could not opendir %s\n",name.c_str());
    if(close(fd)!=0)
      fail("FileSys::DirectoryNode constructor could not close fd %d for %s\n",fd,name.c_str());
    refresh();
  }
  DirectoryNode::~DirectoryNode(void){
    if(closedir(dirp)!=0)
      fail("FileSys::DirectoryNode destructor could not closedir %s\n",name.c_str());
  }
  std::string DirectoryNode::getName(void) const{
    return name;
  }
  void DirectoryNode::setSize(off_t nlen){
    fail("DirectoryNode::setSize called\n");
  }
  ssize_t DirectoryNode::pread(void *buf, size_t count, off_t offs){
    fail("DirectoryNode::pread called\n");
  }
  ssize_t DirectoryNode::pwrite(const void *buf, size_t count, off_t offs){
    fail("DirectoryNode::pwrite called\n");
  }
  void DirectoryNode::refresh(void){
    entries.clear();
    rewinddir(dirp);
    for(size_t i=0;true;i++){
      struct dirent *dent=readdir(dirp);
      if(!dent)
	break;
      entries.push_back(dent->d_name);
    }
    SeekableNode::setSize(entries.size());
  }
  std::string DirectoryNode::getEntry(off_t index){
    I((index>=0)&&(entries.size()>(size_t)index));
    return entries[index];
  }
  DirectoryDescription::DirectoryDescription(DirectoryNode *node, flags_t flags)
    : SeekableDescription(node,flags){
  }
  DirectoryDescription::~DirectoryDescription(void){
  }
  void DirectoryDescription::setPos(off_t npos){
    dynamic_cast<DirectoryNode *>(node)->refresh();
    SeekableDescription::setPos(npos);
  }
  std::string DirectoryDescription::readDir(void){
    off_t opos=SeekableDescription::getPos();
    if(opos>=getSize())
      fail("DirectoryDescription::readDir past the end\n");
    SeekableDescription::setPos(opos+1);
    return dynamic_cast<DirectoryNode *>(node)->getEntry(opos);
  }
  void StreamNode::unblock(PidSet &pids, SigCode sigCode){
    for(PidSet::iterator it=pids.begin();it!=pids.end();it++){
      pid_t pid=*it;
      ThreadContext *context=osSim->getContext(pid);
      if(context){   
        SigInfo *sigInfo=new SigInfo(SigIO,sigCode);
	//        sigInfo->pid=pid;
	//        sigInfo->data=fd;     
        context->signal(sigInfo);
      }
    }
    pids.clear();
  }
  StreamNode::StreamNode(dev_t dev, dev_t rdev, ino_t ino, uid_t uid, gid_t gid, mode_t mode)
    : Node(dev,ino,uid,gid,mode), rdev(rdev), rdBlocked(), wrBlocked(){
  }
  StreamNode::~StreamNode(void){
    I(rdBlocked.empty());
    I(wrBlocked.empty());
  }
  void StreamNode::rdBlock(pid_t pid){
    I(willRdBlock());
    rdBlocked.push_back(pid);    
  }
  void StreamNode::wrBlock(pid_t pid){
    I(willWrBlock());
    wrBlocked.push_back(pid);
  }
  void StreamNode::rdUnblock(void){
    I(!willRdBlock());
    if(!rdBlocked.empty())
      unblock(rdBlocked,SigCodeIn);
    I(rdBlocked.empty());
  }
  void StreamNode::wrUnblock(void){
    I(!willWrBlock());
    if(!wrBlocked.empty())
      unblock(wrBlocked,SigCodeOut);
    I(wrBlocked.empty());
  }
  bool StreamDescription::willRdBlock(void) const{
    return dynamic_cast<const StreamNode *>(node)->willRdBlock();
  }
  bool StreamDescription::willWrBlock(void) const{
    return dynamic_cast<const StreamNode *>(node)->willWrBlock();
  }
  void StreamDescription::rdBlock(pid_t pid){
    dynamic_cast<StreamNode *>(node)->rdBlock(pid);
  }
  void StreamDescription::wrBlock(pid_t pid){
    dynamic_cast<StreamNode *>(node)->wrBlock(pid);
  }
  StreamDescription::StreamDescription(StreamNode *node, flags_t flags)
    : Description(node,flags){
  }
  ssize_t StreamDescription::read(void *buf, size_t count){
    return dynamic_cast<StreamNode *>(node)->read(buf,count);
  }
  ssize_t StreamDescription::write(const void *buf, size_t count){
    return dynamic_cast<StreamNode *>(node)->write(buf,count);
  }
  
  PipeNode::PipeNode(void)
    : StreamNode(0x0007,0x0000,1,getuid(),getgid(),S_IFIFO|S_IREAD|S_IWRITE), data(), readers(0), writers(0){
  }
  PipeNode::~PipeNode(void){
    I(!readers);
    I(!writers);
  }
  void PipeNode::addReader(void){
    readers++;
  }
  void PipeNode::delReader(void){
    I(readers);
    readers--;
    if((readers==0)&&(writers==0))
      delete this;
  }
  void PipeNode::addWriter(void){
    writers++;
  }
  void PipeNode::delWriter(void){
    I(writers);
    writers--;
    if((readers==0)&&(writers==0))
      delete this;
  }
  ssize_t PipeNode::read(void *buf, size_t count){
    I(readers);
    if(data.empty()){
      I(!writers);
      return 0;
    }
    size_t ncount=count;
    if(data.size()<ncount)
      ncount=data.size();
    Data::iterator begIt=data.begin();
    Data::iterator endIt=begIt+ncount;
    copy(begIt,endIt,(uint8_t *)buf);
    data.erase(begIt,endIt);
    I(ncount>0);
    wrUnblock();
    return ncount;
  }
  ssize_t PipeNode::write(const void *buf, size_t count){
    I(writers);
    const uint8_t *ptr=(const uint8_t *)buf;
    data.resize(data.size()+count);
    copy(ptr,ptr+count,data.end()-count);
    rdUnblock();
    return count;
  }
  bool PipeNode::willRdBlock(void) const{
    return data.empty()&&writers;
  }
  bool PipeNode::willWrBlock(void) const{
    return false;
  }
  std::string PipeNode::getName(void) const{
    return "AnonymousPipe";
  }
  PipeDescription::PipeDescription(PipeNode *node, flags_t flags)
    : StreamDescription(node,flags){
    if(canRd())
      dynamic_cast<PipeNode *>(node)->addReader();
    if(canWr())
      dynamic_cast<PipeNode *>(node)->addWriter();
  }
  PipeDescription::~PipeDescription(void){
    if(canRd())
      dynamic_cast<PipeNode *>(node)->delReader();
    if(canWr())
      dynamic_cast<PipeNode *>(node)->delWriter();
  }
  TtyDescription::TtyDescription(TtyNode *node, flags_t flags)
    : StreamDescription(node,flags){
    node->addRef();
  }
  TtyDescription::~TtyDescription(void){
    dynamic_cast<TtyNode *>(node)->delRef();
  }
  TtyNode::TtyNode(fd_t srcfd)
    : StreamNode(0x009,0x8803,(ino_t)1,getuid(),getgid(),S_IFCHR|S_IRUSR|S_IWUSR), fd(dup(srcfd)){
    if(fd==-1)
      fail("TtyNode constructor cannot dup()\n");
  }
  TtyNode::~TtyNode(void){
    close(fd);
  }
  ssize_t TtyNode::read(void *buf, size_t count){
    return ::read(fd,buf,count);
  }
  ssize_t TtyNode::write(const void *buf, size_t count){
    return ::write(fd,buf,count);
  }
  bool TtyNode::willRdBlock(void) const{
    struct pollfd pollFd;
    pollFd.fd=fd;
    pollFd.events=POLLIN;
    int32_t res=poll(&pollFd,1,0);
    return (res<=0);    
  }
  bool TtyNode::willWrBlock(void) const{
    struct pollfd pollFd;
    pollFd.fd=fd;
    pollFd.events=POLLOUT;
    int32_t res=poll(&pollFd,1,0);
    return (res<=0);
  }
  std::string TtyNode::getName(void) const{
    return "TtyDevice";
  }
  TtyDescription *TtyDescription::wrap(fd_t fd){
    flags_t flags=fcntl(fd,F_GETFL);
    if(flags==-1)
      return 0;
    TtyNode *node=new TtyNode(fd);
    return new TtyDescription(node,flags);
  }

  OpenFiles::FileDescriptor::FileDescriptor(void)
    : description(0), cloexec(false){
  }
  OpenFiles::OpenFiles(void)
    : GCObject(), fileDescriptors(){ 
  }
  OpenFiles::OpenFiles(const OpenFiles &src)
    : GCObject(), fileDescriptors(src.fileDescriptors){
  }
  OpenFiles::~OpenFiles(void){
  }
  fd_t OpenFiles::nextFreeFd(fd_t minfd) const{
    for(FileDescriptors::const_iterator it=fileDescriptors.lower_bound(minfd);
        (it!=fileDescriptors.end())&&(it->first==minfd);minfd++,it++);
    return minfd;
  }
  void OpenFiles::openDescriptor(fd_t fd, Description *description){
    I(fd>=0);
    std::pair<FileDescriptors::iterator,bool> res=fileDescriptors.insert(FileDescriptors::value_type(fd,FileDescriptor()));
    I(res.second&&(res.first->first==fd));
    res.first->second.description=description;
    res.first->second.cloexec=false;
  }
  void OpenFiles::closeDescriptor(fd_t fd){
    I(fileDescriptors.count(fd)==1);
    fileDescriptors.erase(fd);
    I(!isOpen(fd));
  }
  bool OpenFiles::isOpen(fd_t fd) const{
    I(fd>=0);
    I((fileDescriptors.count(fd)==0)||fileDescriptors.find(fd)->second.description);
    return fileDescriptors.count(fd);
  }
  Description *OpenFiles::getDescription(fd_t fd){
    FileDescriptors::iterator it=fileDescriptors.find(fd);
    I(it!=fileDescriptors.end());
    I(it->second.description);
    return it->second.description;
  }
  void OpenFiles::setCloexec(fd_t fd, bool cloex){
    FileDescriptors::iterator it=fileDescriptors.find(fd);
    I(it!=fileDescriptors.end());
    I(it->second.description);
    it->second.cloexec=cloex;
  }
  bool OpenFiles::getCloexec(fd_t fd) const{
    FileDescriptors::const_iterator it=fileDescriptors.find(fd);
    I(it!=fileDescriptors.end());
    I(it->second.description);
    return it->second.cloexec;
  }
  void OpenFiles::exec(void){
    FileDescriptors::iterator it=fileDescriptors.begin();
    while(it!=fileDescriptors.end()){
      FileDescriptors::iterator nxit=it;
      nxit++;
      I(it->second.description);
      if(it->second.cloexec)
	fileDescriptors.erase(it);
      it=nxit;
    }
  }
  void OpenFiles::save(ChkWriter &out) const{
    out << "Descriptors: " << fileDescriptors.size() << endl;
    for(FileDescriptors::const_iterator it=fileDescriptors.begin();
        it!=fileDescriptors.end();it++){
      out << "Desc " << it->first << " Cloex " << (it->second.cloexec?'+':'-');
      /*
      BaseStatus *st=it->second.description_old;
      bool hasIndex=out.hasIndex(st);
      out << " Status " << out.getIndex(st) << endl;
      if(!hasIndex)
        st->save(out);
      */
    }
  }
  OpenFiles::OpenFiles(ChkReader &in){
    size_t _size;
    in >> "Descriptors: " >> _size >> endl;
    for(size_t i=0;i<_size;i++){
      fd_t _fd;
      char _cloex;
      in >> "Desc " >> _fd >> " Cloex " >> _cloex;
      /*
      size_t _st;
      in >> " Status " >> _st >> endl;
      BaseStatus *st;
      if(!in.hasObject(_st)){
        in.newObject(_st);
        st=BaseStatus::create(in);
        in.setObject(_st,st);
      }else{
        st=reinterpret_cast<BaseStatus *>(in.getObject(_st));
      }
      fileDescriptors[_fd].description_old=st;
      fileDescriptors[_fd].cloexec=(_cloex=='+');
      */
    }
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
