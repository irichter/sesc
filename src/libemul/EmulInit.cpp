#include <unistd.h>
#include <fcntl.h>
#include "ThreadContext.h"
#include "ElfObject.h"
#include "MipsSysCalls.h"
#include "MipsRegs.h" 
#include "EmulInit.h"

void fail(const char *fmt, ...){
  va_list ap;
  fflush(stdout);
  fprintf(stderr, "\nERROR: ");
  va_start(ap, fmt);
  vfprintf(stderr,fmt,ap);
  va_end(ap);
  exit(1);
}

void emulInit(int argc, char **argv, char **envp){
  int sim_stdin_fd =0;
  int sim_stdout_fd=1;
  int sim_stderr_fd=2;

  bool argErr=false;
  extern char *optarg;
  int opt;
  while((opt=getopt(argc, argv, "+i:o:e:"))!=-1){
    switch(opt){
    case 'i':
      sim_stdin_fd=open(optarg,O_RDONLY|O_DIRECT);
      if(sim_stdin_fd==-1){
	fprintf(stderr,"Could not open specified stdin file %s\n",optarg);
	argErr=true;
      }
      break;
    case 'o':
      sim_stdout_fd=open(optarg,O_WRONLY|O_DIRECT|O_CREAT,S_IRUSR|S_IWUSR);
      if(sim_stdout_fd==-1){
	fprintf(stderr,"Could not open specified stdout file %s\n",optarg);
	argErr=true;
      }
      break;
    case 'e':
      sim_stderr_fd=open(optarg,O_WRONLY|O_DIRECT|O_CREAT,S_IRUSR|S_IWUSR);
      if(sim_stderr_fd==-1){
	fprintf(stderr,"Could not open specified stderr file %s\n",optarg);
	argErr=true;
      }
      break;
    default:
      argErr=1;
      break;
    }
  }
  if(argErr){
    fprintf(stderr,"\n");
    fprintf(stderr,"Usage: %s [EmulOpts] [-- SimOpts] AppExec [AppOpts]\n",argv[0]);
    fprintf(stderr,"  EmulOpts:\n");
    fprintf(stderr,"  [-i FName] Use file FName as stdin  for AppExec\n");
    fprintf(stderr,"  [-o FName] Use file FName as stdout for AppExec\n");
    fprintf(stderr,"  [-e FName] Use file FName as stderr for AppExec\n");
    exit(1);
  }
  int    appArgc=argc-optind;
  char **appArgv=&(argv[optind]);
  char **appEnvp=envp;
  ThreadContext::staticConstructor();
  ThreadContext::initMainThread();
  ThreadContext *mainThread=ThreadContext::getMainThreadContext();
  AddressSpace  *addrSpace=new AddressSpace();
  mainThread->setAddressSpace(addrSpace);
  loadElfObject(appArgv[0],mainThread);
  I(mainThread->getMode()==Mips32);
  // Stack starts at 16KB, but can autogrow
  size_t stackSize=0x4000;
  VAddr stackStart=addrSpace->newSegmentAddr(stackSize);
  I(stackStart);
  // Stack is created with read/write permissions, and autogrows down
  addrSpace->newSegment(stackStart,stackSize,true,true,false);
  addrSpace->setGrowth(stackStart,true,true);
  mainThread->writeMemWithByte(stackStart,stackSize,0);
  mainThread->setStack(stackStart,stackStart+stackSize);
  switch(mainThread->getMode()){
  case Mips32:
    mainThread->getReg(static_cast<RegName>(Mips::RegSP))=stackStart+stackSize;
    mipsProgArgs(mainThread,appArgc,appArgv,appEnvp);
    break;
  case Mips64:
    I(0);
  }
  addrSpace->newSimFd(sim_stdin_fd, STDIN_FILENO);
  addrSpace->newSimFd(sim_stdout_fd,STDOUT_FILENO);
  addrSpace->newSimFd(sim_stderr_fd,STDERR_FILENO);
}
