#include "SysCall.h"

// Memory management system calls

// For definition of ENOMEM
#include <errno.h>

void SysCallMalloc::exec(ThreadContext *context, icode_ptr picode){
  size_t size=context->getGPR(Arg1stGPR);
  I((!executed)||(mySize==size));
  ID(mySize=size);
  I((!executed)||(myPid==context->getThreadPid()));
  myPid=context->getThreadPid();
  if(!executed)
    myAddr=context->getHeapManager()->allocate(size);
  if(myAddr){
    // Map real address to logical address space
    context->setGPR(RetValGPR,context->real2virt(myAddr));
  }else{
    // Return 0 and set errno to be POSIX compliant
    context->setErrno(ENOMEM);
    context->setGPR(RetValGPR,0);
  }
  /*printf("%1d: exec(%d) SysCallMalloc %8x at
   * %8x\n",myPid,executed?1:0,mySize,myAddr);*/
  executed=true;
}
void SysCallMalloc::undo(bool expectRedo){
  I(executed);
  if((!expectRedo)&&myAddr){
    I(ThreadContext::getContext(myPid));
    size_t size=ThreadContext::getContext(myPid)->getHeapManager()->deallocate(myAddr);
    I(size==mySize);
  }
/*  printf("%1d: undo(%d) SysCallMalloc %8x at
 *  %8x\n",myPid,executed?1:0,mySize,myAddr);*/
}

void SysCallFree::exec(ThreadContext *context, icode_ptr picode){
  Address addr=context->getGPR(Arg1stGPR);
  I(addr);
  I((!executed)||((Address)(context->virt2real(addr))==myAddr));
  myAddr=context->virt2real(addr);
  I((!executed)||(myPid==context->getThreadPid()));
  myPid=context->getThreadPid();
  if(!executed)
    mySize=context->getHeapManager()->deallocate(myAddr);
  /*printf("%1d: exec(%d) SysCallFree   %8x at
   * %8x\n",myPid,executed?1:0,mySize,myAddr);*/
  executed=true;
}
void SysCallFree::undo(bool expectRedo){
  I(executed);
  I(ThreadContext::getContext(myPid));
  /*printf("%1d: undo(%d) SysCallFree   %8x at
   * %8x\n",myPid,executed?1:0,mySize,myAddr);*/
  if(!expectRedo){
    Address addr=ThreadContext::getContext(myPid)->getHeapManager()->allocate(myAddr,mySize);
    I(addr==myAddr);
  }
}
void SysCallMmap::exec(ThreadContext *context, icode_ptr picode){
  // Prefered address for mmap. This is ignored in SESC.
  long addr=context->getGPR(Arg1stGPR);
  // Size of block to mmap
  size_t size=context->getGPR(Arg2ndGPR);
  // Protection flags for mmap
  int prot=context->getGPR(Arg3rdGPR);
  // PROT_READ and PROT_WRITE should be set, and nothing lese
  I(prot==0x3);
  // Flags for mmap
  int flag=context->getGPR(Arg4thGPR);
  // MAP_ANONYMOUS and MAP_PRIVATE should be set, and nothing else
  I(flag==0x802);
  I((!executed)||(mySize==size));
  ID(mySize=size);
  I((!executed)||(myPid==context->getThreadPid()));
  if(!executed){
    myAddr=context->getHeapManager()->allocate(size);
    if(myAddr==0)
      myAddr=-1;
  }
  if(myAddr!=-1){
    // Map real address to logical address space
    context->setGPR(RetValGPR,context->real2virt(myAddr));
  }else{
    // Set errno to be POSIX compliant
    context->setErrno(ENOMEM);
    context->setGPR(RetValGPR,myAddr);
  }
  executed=true;
}
void SysCallMmap::undo(bool expectRedo){
  I(executed);
  if((!expectRedo)&&(myAddr!=-1)){
    I(ThreadContext::getContext(myPid));
    size_t size=ThreadContext::getContext(myPid)->getHeapManager()->deallocate(myAddr);
    I(size==mySize);
  }
}
void SysCallMunmap::exec(ThreadContext *context, icode_ptr picode){
  // Starting address of the block
  long addr=context->getGPR(Arg1stGPR);
  I(addr);
  I((!executed)||((Address)(context->virt2real(addr))==myAddr));
  myAddr=context->virt2real(addr);
  // Size of block to munmap
  size_t wantSize=context->getGPR(Arg2ndGPR);
  I(wantSize);
  I((!executed)||(myPid==context->getThreadPid()));
  myPid=context->getThreadPid();
  if(!executed)
    mySize=context->getHeapManager()->deallocate(myAddr);
  I(mySize==wantSize);
  executed=true;
}
void SysCallMunmap::undo(bool expectRedo){
  I(executed);
  I(ThreadContext::getContext(myPid));
  if(!expectRedo){
    Address addr=ThreadContext::getContext(myPid)->getHeapManager()->allocate(myAddr,mySize);
    I(addr==myAddr);
  }
}

// File I/O system calls

SysCallFileIO::OpenFileVector SysCallFileIO::openFiles;

void SysCallOpen::exec(ThreadContext *context,icode_ptr picode){
  // Get the file name from versioned memory
  char pathname[MAX_FILENAME_LENGTH];
  rsesc_OS_read_string(context->getPid(),picode->addr,pathname, 
		       (const void *)context->getGPR(Arg1stGPR),
		       MAX_FILENAME_LENGTH);
  int flags=conv_flags_to_native((int)context->getGPR(Arg2ndGPR));
  mode_t mode=(mode_t)context->getGPR(Arg3rdGPR);
  int newFd=open(pathname,flags,mode);
  if(executed){
    I(newFd==myFd);
  }else{
    myFd=newFd;
  }
  if(myFd==-1){
    // Open failed, set errno in simulated thread
    context->setErrno(errno);
  }else{
    // Add file's info to vector of currently open files
    if(openFiles.size()<=(size_t)myFd)
      openFiles.resize(myFd+1,0);
    I(openFiles[myFd]==0);
    openFiles[myFd]=new OpenFileInfo(pathname,flags,mode,0);
  }
  context->setGPR(RetValGPR,myFd);
  executed=true;
}

void SysCallOpen::undo(bool expectRedo){
  I(executed);
  if(myFd!=-1){
    // Close file and remove from vector of open files
    int err=close(myFd);
    I(!err);
    delete openFiles[myFd];
    openFiles[myFd]=0;
  }
}

void SysCallClose::exec(ThreadContext *context, icode_ptr picode){
  int fd=context->getGPR(Arg1stGPR);
  if(executed){
    I(fd==myFd);
  }else{
    myFd=fd;
  }
  if((openFiles.size()<=(size_t)fd)||!openFiles[fd]){
    // Descriptor is not that of an open file
    if(executed){
      I(myInfo==0);
    }else{
      myInfo=0;
    }
    context->setErrno(EBADF);
  }else{  
    int err=close(fd);
    if(err==0){
      if(executed){
	I(strcmp(myInfo->pathname,openFiles[fd]->pathname)==0);
	I(myInfo->flags==openFiles[fd]->flags);
	I(myInfo->mode==openFiles[fd]->mode);
	delete myInfo;
      }
      // Store info needed to reopen file in undo
      myInfo=openFiles[fd];
      // Remove from vector of open files
      openFiles[fd]=0;
    }else{
      I(err==-1);
      if(executed){
	I(myInfo==0);
      }else{
	myInfo=0;
      }
      context->setErrno(errno);
    }
  }
  context->setGPR(RetValGPR,myInfo?0:-1);
  executed=true;
}

void SysCallClose::undo(bool expectRedo){
  I(executed);
  if(myInfo){
    int currFD=-1; /* because desired FD could be 0 */ 
    int fd; 
    std::stack<int> dummyFDs;     
    
    // Ensure the re-opened file has the same file descriptor as before it 
    // was closed. POSIX open() will always return the lowest available.  So, 
    // open dummy files until open() returns the desired fd. 
    int dummyNum;
    for(dummyNum=0; currFD!=myFd; dummyNum++){ 
      char *dummyName=(char *)malloc(9*sizeof(char)); 
      snprintf(dummyName,9,"dummy%d",dummyNum);  
      currFD=open(dummyName,O_RDONLY|O_CREAT|O_EXCL,0600);  
      dummyFDs.push(currFD); 
      free(dummyName); 
      I(dummyNum<1000); /* probably shouldn't open more than 1000 files.. */ 
    } 
    // Close the dummy file that received the fd needed 
    close(dummyFDs.top());  
    dummyFDs.pop();  
    // Reopen the file and update things accordingly 
    fd=open(myInfo->pathname,myInfo->flags&~O_TRUNC,myInfo->mode);
    I(fd==myFd);
    I((openFiles.size()>(size_t)fd)&&!openFiles[fd]);
    openFiles[fd]=myInfo;
    if(expectRedo)
      myInfo=new OpenFileInfo(*(openFiles[fd]));
    if(openFiles[fd]->offset>0){
      off_t undoOffs=lseek(fd,openFiles[fd]->offset,SEEK_SET);
      I(undoOffs==openFiles[fd]->offset);
    }
    // Close remaining dummies
    while(!dummyFDs.empty()){ 
      close(dummyFDs.top()); 
      dummyFDs.pop(); 
    }
    // Delete dummy files
    while(dummyNum>=0){
      char *dummyName=(char *)malloc(9);
      snprintf(dummyName,9,"dummy%d",dummyNum);
      remove(dummyName);
      free(dummyName);
      dummyNum--;
    }
  }
}

void SysCallClose::done(){
  if(myInfo)
    delete myInfo;
}

void SysCallRead::exec(ThreadContext *context,icode_ptr picode){
  int fd=context->getGPR(Arg1stGPR);
  I((!executed)||(fd==myFd));
  myFd=fd;
  I((openFiles.size()>(size_t)myFd)&&openFiles[myFd]);
  void *buf=(void *)(context->getGPR(Arg2ndGPR));
  I((!executed)||(myBuf==buf));
  ID(myBuf=buf);
  size_t count=context->getGPR(Arg3rdGPR);
  I((!executed)||(myCount==count));
  ID(myCount=count);
  void *tempbuff=alloca(count);
  I(tempbuff);
  I((!executed)||(oldOffs==lseek(myFd,0,SEEK_CUR)));
  ID(oldOffs=lseek(myFd,0,SEEK_CUR));
  ssize_t nowBytesRead=read(myFd,tempbuff,executed?bytesRead:count);
  I((!executed)||(nowBytesRead==bytesRead));
  bytesRead=nowBytesRead;
  context->setGPR(RetValGPR,bytesRead);
  if(bytesRead==-1){
    context->setErrno(errno);
    I(lseek(myFd,0,SEEK_CUR)==oldOffs);
  }else{
    I(lseek(myFd,0,SEEK_CUR)==oldOffs+bytesRead);
    openFiles[myFd]->offset+=bytesRead;
    rsesc_OS_write_block(context->getPid(),picode->addr,
			 buf,tempbuff,(size_t)bytesRead);
  }
  executed=true;
}
void SysCallRead::undo(bool expectRedo){
  I(executed);
  if(bytesRead!=-1){
    I((openFiles.size()>(size_t)myFd)&&openFiles[myFd]);
    off_t undoOffs=lseek(myFd,-bytesRead,SEEK_CUR);
    I(undoOffs==oldOffs);
    I(lseek(myFd,0,SEEK_CUR)==oldOffs);
  }
}

void SysCallWrite::exec(ThreadContext *context,icode_ptr picode){
  int fd=context->getGPR(Arg1stGPR);
  I((!executed)||(fd==myFd));
  myFd=fd;
  void *buf=(void *)context->getGPR(Arg2ndGPR);
  I((!executed)||(myBuf==buf));
  ID(myBuf=buf);
  size_t count=context->getGPR(Arg3rdGPR);
  I((!executed)||(myCount==count));
  ID(myCount=count);
  off_t currentOffset=lseek(myFd,0,SEEK_CUR);
  if(currentOffset==-1){
    if(errno==EBADF){
      // Invalid file handle, fail with errno of EBADF
      context->setErrno(EBADF);
      bytesWritten=-1;
      context->setGPR(RetValGPR,-1);
    }else if(errno==ESPIPE){
      // Non-seekable file, must buffer writes instead of undoing them
      // We can't yet handle open-ed files that are non-seekable
      I((openFiles.size()<=(size_t)myFd)||!openFiles[myFd]);
      if(executed){
	I(bytesWritten==(ssize_t)count);
	void *tempbuff=alloca(count);
	I(tempbuff);
	// Read data from versioned memory into a temporary buffer
	rsesc_OS_read_block(context->getPid(),picode->addr,tempbuff,buf,count);
	I(bufData&&(memcmp(bufData,tempbuff,count)==0));
      }else{
	bufData=malloc(count);
	I(bufData);
	// Read data from versioned memory into a temporary buffer
	rsesc_OS_read_block(context->getPid(),picode->addr,bufData,buf,count);
	bytesWritten=count;
      }
    }else{
      I(0);
    }
  }else{
    // File is seekable so writes can be undone
    I((openFiles.size()>(size_t)myFd)&&openFiles[myFd]);
    I(!executed||!bufData);
    bufData=0;
    void *tempbuff=alloca(count);
    I(tempbuff);
    // Read data from versioned memory into a temporary buffer
    rsesc_OS_read_block(context->getPid(),picode->addr,tempbuff,buf,count);
    // Get current position and verify that we are in append mode
    ID(oldOffs=currentOffset);
    I(oldOffs==lseek(myFd,0,SEEK_END));
    I(oldOffs==lseek(myFd,0,SEEK_CUR));
    // Write to file and free the temporary buffer
    ssize_t nowBytesWritten=write(myFd,tempbuff,executed?bytesWritten:count);
    I((!executed)||(nowBytesWritten==bytesWritten));
    bytesWritten=nowBytesWritten;
    context->setGPR(RetValGPR,bytesWritten);
    if(bytesWritten==-1){
      context->setErrno(errno);
      I(lseek(myFd,0,SEEK_CUR)==oldOffs);
    }else{
      I(lseek(myFd,0,SEEK_CUR)==oldOffs+bytesWritten);
      openFiles[myFd]->offset+=bytesWritten;
    }
  }
  executed=true;
}
void SysCallWrite::undo(bool expectRedo){
  I(executed);
  if(bufData){
    I(bytesWritten!=-1);
    if(!expectRedo)
      free(bufData);
  }else if(bytesWritten!=-1){
    I((openFiles.size()>(size_t)myFd)&&openFiles[myFd]);
    off_t currOffs=lseek(myFd,-bytesWritten,SEEK_END);
    I(currOffs==oldOffs);
    int err=ftruncate(myFd,currOffs);
    I(!err);
    I(oldOffs==lseek(myFd,0,SEEK_END));
    I(oldOffs==lseek(myFd,0,SEEK_CUR));
  }
}
void SysCallWrite::done(void){
  if(bufData){
    I(bytesWritten!=-1);    
    ssize_t nowBytesWritten=write(myFd,bufData,bytesWritten);
    I(nowBytesWritten==bytesWritten);
    free(bufData);
  }
}

// Thread management system calls

#include "Epoch.h"

void SysCallSescSpawn::exec(ThreadContext *context,icode_ptr picode){
  // Get parent thread and spawning epoch
  tls::Epoch *oldEpoch=context->getEpoch();
  I(oldEpoch==tls::Epoch::getEpoch(context->getPid()));
  I(oldEpoch);
  Pid_t ppid=oldEpoch->getTid();
  // Arguments of the sesc_spawn call
  RAddr entry = context->getGPR(Arg1stGPR);
  RAddr arg   = context->getGPR(Arg2ndGPR);
  long  flags = context->getGPR(Arg3rdGPR);
  I(entry);
  ThreadContext *childContext=0;
  tls::Thread   *childThread=0;
  tls::Epoch    *childEpoch=0;
  if(!executed){
    // Allocate a new thread for the child
    childContext=ThreadContext::newActual();
    // Process ID of the child thread
    childPid=childContext->getPid();
  }else{
    childThread=tls::Thread::getByID(static_cast<tls::ThreadID>(childPid));
    if(!childThread){
      childContext=ThreadContext::newActual(childPid);
    }else{
      childEpoch=childThread->getInitialEpoch();
    }
  }
  // The return value for the parent is the child's pid
  context->setGPR(RetValGPR,childPid);
  if(childContext){
    // Eerything is shared, stack is not copied
    childContext->shareAddrSpace(context,PR_SADDR,false);
    childContext->init();
    // The first instruction for the child is the entry point passed in
    childContext->setIP(addr2icode(entry));
    childContext->setGPR(Arg1stGPR,arg);
    childContext->setGPR(Arg2ndGPR,Stack_size); /* for sprocsp() */
    // In position-independent code every function expects to find
    // its own entry address in register jp (also known as t9 and R25)
    childContext->reg[25]= entry;
    // When the child returns from the 'entry' function,
    // it will go directly to the exit() function
    childContext->setGPR(RetAddrGPR,Exit_addr);
    // The return value for the child is 0
    childContext->setGPR(RetValGPR,0);
    // Inform SESC of what we have done here
    osSim->eventSpawn(ppid,childPid,flags);
  }
  if(!childThread){
    // Create child's initial epoch
    childEpoch=tls::Epoch::initialEpoch(static_cast<tls::ThreadID>(childPid),oldEpoch);
  }else{
    oldEpoch->changeEpoch();
    if(!childEpoch){
      // Re-create child's initial epoch
      I(0);
    }
    childEpoch->run();
  }
  executed=true;
}

void SysCallSescSpawn::undo(bool expectRedo){
}

void SysCallSescSpawn::done(void){
}

void SysCallExit::exec(ThreadContext *context,icode_ptr picode){
  I((!executed)||(myThread==context->getEpoch()->getTid()));
  myThread=context->getEpoch()->getTid();
  context->getEpoch()->exitCalled();
  executed=true;
}

void SysCallExit::undo(bool expectRedo){
  I(executed);
  tls::Thread *thisThread=tls::Thread::getByID(myThread);
  I(thisThread);
  thisThread->undoExitCall();
}

void SysCallWait::exec(ThreadContext *context,icode_ptr picode){
  if(!executed){
    childThread=-1;
  }else if(childThread!=-1){
    tls::Thread *child=tls::Thread::getByID(childThread);
    if(!child)
      return;
  }
  context->getEpoch()->waitCalled();
  executed=true;
}

void SysCallWait::undo(bool expectRedo){
  I(executed);
  if(childThread==-1)
    return;
  tls::Thread *child=tls::Thread::getByID(childThread);
  if(!child)
    return;
  tls::ThreadID parentThread=child->getParentID();
  I(parentThread!=-1);
  tls::Thread *parent=tls::Thread::getByID(parentThread);
  I(parent);
  parent->undoWaitCall(child);
}
