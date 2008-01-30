#include <unistd.h>
#include <fcntl.h>
#include "ThreadContext.h"
#include "ElfObject.h"
//#include "MipsSysCalls.h"
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

void emulInit(int32_t argc, char **argv, char **envp){
  FileSys::BaseStatus *inStatus=0;
  FileSys::BaseStatus *outStatus=0;
  FileSys::BaseStatus *errStatus=0;

  extern char *optarg;
  int32_t opt;
  while((opt=getopt(argc, argv, "+hi:o:e:"))!=-1){
    switch(opt){
    case 'i':
      inStatus=FileSys::FileStatus::open(optarg,O_RDONLY,S_IRUSR);
      if(!inStatus)
	fail("Could not open `%s' as simulated stdin file\n",optarg);
      break;
    case 'o':
      outStatus=FileSys::FileStatus::open(optarg,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
      if(!outStatus)
	fail("Could not open `%s' as simulated stdout file\n",optarg);
      break;
    case 'e':
      errStatus=FileSys::FileStatus::open(optarg,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
      if(!errStatus)
	fail("Could not open `%s' as simulated stderr file %s\n",optarg);
      break;
    case 'h':
    default:
      fail("\n"
	   "Usage: %s [EmulOpts] [-- SimOpts] AppExec [AppOpts]\n"
	   "  EmulOpts:\n"
	   "  [-i FName] Use file FName as stdin  for AppExec\n"
	   "  [-o FName] Use file FName as stdout for AppExec\n"
	   "  [-e FName] Use file FName as stderr for AppExec\n",
	   argv[0]);
    }
  }
  if(!inStatus){
    inStatus=FileSys::StreamStatus::wrap(STDIN_FILENO);
    if(!inStatus)
      fail("Could not wrap stdin\n");
  }
  if(!outStatus){
    outStatus=FileSys::StreamStatus::wrap(STDOUT_FILENO);
    if(!outStatus)
      fail("Could not wrap stdout\n");
  }
  if(!errStatus){
    errStatus=FileSys::StreamStatus::wrap(STDERR_FILENO);
    if(!errStatus)
      fail("Could not wrap stderr\n");
  }
  int32_t    appArgc=argc-optind;
  char **appArgv=&(argv[optind]);
  char **appEnvp=envp;
  // Count environment variables
  int32_t    appEnvc=0;
  while(appEnvp[appEnvc])
    appEnvc++;
  
  ThreadContext *mainThread=new ThreadContext();
  loadElfObject(appArgv[0],mainThread);
  mainThread->getSystem()->initSystem(mainThread);
  mainThread->getSystem()->createStack(mainThread);
  mainThread->getSystem()->setProgArgs(mainThread,appArgc,appArgv,appEnvc,appEnvp);
  FileSys::OpenFiles *openFiles=mainThread->getOpenFiles();
  openFiles->getDesc(STDIN_FILENO )->setStatus(inStatus);
  openFiles->getDesc(STDIN_FILENO )->setCloexec(false);
  openFiles->getDesc(STDOUT_FILENO)->setStatus(outStatus);
  openFiles->getDesc(STDOUT_FILENO)->setCloexec(false);
  openFiles->getDesc(STDERR_FILENO)->setStatus(errStatus);
  openFiles->getDesc(STDERR_FILENO)->setCloexec(false);
}
