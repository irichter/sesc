#include <unistd.h>
#include <fcntl.h>
#include "ThreadContext.h"
#include "ElfObject.h"
#include "MipsSysCalls.h"
#include "MipsRegs.h" 
#include "EmulInit.h"
#include "FileSys.h"

// For Mips32_STD*_FILENO
//#include "Mips32Defs.h"

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
  int sim_stdin_fd =-1;
  int sim_stdout_fd=-1;
  int sim_stderr_fd=-1;

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
  if(sim_stdin_fd==-1){
    sim_stdin_fd=dup(STDIN_FILENO);
    if(sim_stdin_fd==-1){
      fprintf(stderr,"Could not duplicate stdin\n");
      argErr=true;
    }
  }
  if(sim_stdout_fd==-1){
    sim_stdout_fd=dup(STDOUT_FILENO);
    if(sim_stdout_fd==-1){
      fprintf(stderr,"Could not duplicate stdout\n");
      argErr=true;
    }
  }
  if(sim_stderr_fd==-1){
    sim_stderr_fd=dup(STDERR_FILENO);
    if(sim_stderr_fd==-1){
      fprintf(stderr,"Could not duplicate stderr\n");
      argErr=true;
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
  // Count environment variables
  int    appEnvc=0;
  while(appEnvp[appEnvc])
    appEnvc++;
  ThreadContext::staticConstructor();
  ThreadContext::initMainThread();
  ThreadContext *mainThread=ThreadContext::getMainThreadContext();
  mainThread->setSignalTable(new SignalTable());
  mainThread->setSignalState(new SignalState());
  AddressSpace  *addrSpace=new AddressSpace();
  mainThread->setAddressSpace(addrSpace);
  loadElfObject(appArgv[0],mainThread);
  I(mainThread->getMode()==Mips32);
  switch(mainThread->getMode()){
  case Mips32:
//     I(Mips32_STDIN_FILENO == STDIN_FILENO );
//     I(Mips32_STDOUT_FILENO== STDOUT_FILENO);
//     I(Mips32_STDERR_FILENO== STDERR_FILENO);
    Mips::initSystem(mainThread);
    Mips::createStack(mainThread);
    Mips::setProgArgs(mainThread,appArgc,appArgv,appEnvc,appEnvp);
    break;
  default:
    I(0);
  }
  FileSys::OpenFiles *openFiles=new FileSys::OpenFiles();
  openFiles->getDesc(STDIN_FILENO )->setStatus(FileSys::StreamStatus::wrap(sim_stdin_fd ));
  openFiles->getDesc(STDIN_FILENO )->setCloexec(false);
  openFiles->getDesc(STDOUT_FILENO)->setStatus(FileSys::StreamStatus::wrap(sim_stdout_fd));
  openFiles->getDesc(STDOUT_FILENO)->setCloexec(false);
  openFiles->getDesc(STDERR_FILENO)->setStatus(FileSys::StreamStatus::wrap(sim_stderr_fd));
  openFiles->getDesc(STDERR_FILENO)->setCloexec(false);
  mainThread->setOpenFiles(openFiles);
  close(sim_stdin_fd );
  close(sim_stdout_fd);
  close(sim_stderr_fd);
}
