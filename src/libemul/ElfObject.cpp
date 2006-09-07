#include "ElfObject.h"

#include <elf.h>
#include <fcntl.h>
#include "ThreadContext.h"
#include "CvtEndian.h"
#include "MipsRegs.h"
// To get definition of fail()
#include "EmulInit.h"

void cvtEndianBig(Elf32_Ehdr &ehdr){
  // Array e_ident is all chars and need no endian conversion
  cvtEndianBig(ehdr.e_type);
  cvtEndianBig(ehdr.e_machine);
  cvtEndianBig(ehdr.e_version);
  cvtEndianBig(ehdr.e_entry);
  cvtEndianBig(ehdr.e_phoff);
  cvtEndianBig(ehdr.e_shoff);
  cvtEndianBig(ehdr.e_flags);
  cvtEndianBig(ehdr.e_ehsize);
  cvtEndianBig(ehdr.e_phentsize);
  cvtEndianBig(ehdr.e_phnum);
  cvtEndianBig(ehdr.e_shentsize);
  cvtEndianBig(ehdr.e_shnum);
  cvtEndianBig(ehdr.e_shstrndx);
}

void cvtEndianBig(Elf32_Phdr &phdr){
  cvtEndianBig(phdr.p_type);
  cvtEndianBig(phdr.p_offset);
  cvtEndianBig(phdr.p_vaddr);
  cvtEndianBig(phdr.p_paddr);
  cvtEndianBig(phdr.p_filesz);
  cvtEndianBig(phdr.p_memsz);
  cvtEndianBig(phdr.p_flags);
  cvtEndianBig(phdr.p_align);
}

void cvtEndianBig(Elf32_Shdr &shdr){
  cvtEndianBig(shdr.sh_name);
  cvtEndianBig(shdr.sh_type);
  cvtEndianBig(shdr.sh_flags);
  cvtEndianBig(shdr.sh_addr);
  cvtEndianBig(shdr.sh_offset);
  cvtEndianBig(shdr.sh_size);
  cvtEndianBig(shdr.sh_link);
  cvtEndianBig(shdr.sh_info);
  cvtEndianBig(shdr.sh_addralign);
  cvtEndianBig(shdr.sh_entsize);
}

void cvtEndianBig(Elf32_Sym &sym){
  cvtEndianBig(sym.st_name);
  cvtEndianBig(sym.st_value);
  cvtEndianBig(sym.st_size);
  cvtEndianBig(sym.st_info);
  cvtEndianBig(sym.st_other);
  cvtEndianBig(sym.st_shndx);
}

void cvtEndianBig(Elf32_RegInfo &info){
  cvtEndianBig(info.ri_gprmask);
  for(int i=0;i<4;i++)
    cvtEndianBig(info.ri_cprmask[i]);
  cvtEndianBig(info.ri_gp_value);
}

void loadElfObject(const char *fname, ThreadContext *threadContext){
  int fd=open(fname,O_RDONLY);
  if(fd==-1)
      fail("Could not open ELF file %s",fname);

  // Read the ELF header
  Elf32_Ehdr myElfHdr;
  if(read(fd,&myElfHdr,sizeof(myElfHdr))!=sizeof(myElfHdr))
    fail("Could not read ELF header for file %s",fname);
  char elfMag[]= {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 };
  if(memcmp(myElfHdr.e_ident,elfMag,sizeof(elfMag)))
    fail("Not an ELF file: %s",fname);
  if(myElfHdr.e_ident[EI_DATA]!=ELFDATA2MSB)
    fail("Not a big-endian ELF file: %s",fname);
  if(myElfHdr.e_ident[EI_CLASS]!=ELFCLASS32)
    fail("Not a 32-bit ELF file: %s",fname);
  cvtEndianBig(myElfHdr);
  if((myElfHdr.e_ident[EI_VERSION]!=EV_CURRENT) ||
     (myElfHdr.e_version!=EV_CURRENT))
    fail("Wrong ELF version in file %s",fname);
  if(myElfHdr.e_ehsize!=sizeof(myElfHdr))
    fail("Wrong ELF header size in file %s\n",fname);
  if(myElfHdr.e_machine!=EM_MIPS)
    fail("Not a MIPS ELF file %s",fname);
  if(myElfHdr.e_type!=ET_EXEC)
    fail("Not an executable file %s",fname);
  int mipsArch;
  switch(myElfHdr.e_flags&EF_MIPS_ARCH){
  case EF_MIPS_ARCH_1: mipsArch=1; break;    
  case EF_MIPS_ARCH_2: mipsArch=2; break;
  case EF_MIPS_ARCH_3: mipsArch=3; break;
  case EF_MIPS_ARCH_4: mipsArch=4; break;
  default:
    fail("Using architecture above MIPS4 in ELF file %s",fname);
    mipsArch=-1;
    break;
  }
  printf("Executable %s uses mips%d architecture\n",fname,mipsArch);
  if(myElfHdr.e_flags&(EF_MIPS_PIC|EF_MIPS_CPIC))
    printf("Executable %s contains or uses PIC code\n",fname);
  if(myElfHdr.e_phentsize!=sizeof(Elf32_Phdr))
    fail("Wrong ELF program table entry size in %s",fname);
  if(myElfHdr.e_shentsize!=sizeof(Elf32_Shdr))
    fail("Wrong ELF section table entry size in %s",fname);

  // It's a 32-bit file, but which ABI?
  threadContext->setMode((myElfHdr.e_flags&EF_MIPS_ABI2)?Mips64:Mips32);
  
  // Address space of the thread
  AddressSpace *addrSpace=threadContext->getAddressSpace();
  if(!addrSpace)
    fail("loadElfObject: thread has no address space\n");

  // File offset to the program (segment table)
  off_t prgTabOff=myElfHdr.e_phoff;
  // Number of program (segment) table entries
  uint16_t prgTabCnt=myElfHdr.e_phnum;
  
/*   // Parse program (segment) headers first to find out where and how much */
/*   // memory to allocate for static and bss data */
  
/*   VAddr allocBegAddr=0; */
/*   VAddr allocEndAddr=0; */
/*   if(lseek(fd,prgTabOff,SEEK_SET)!=prgTabOff) */
/*     fail("Couldn't seek to ELF program table in %s",fname); */
/*   else{ */
/*     for(uint16_t prgTabIdx=0;prgTabIdx<prgTabCnt;prgTabIdx++){ */
/*       Elf32_Phdr myPrgHdr; */
/*       if(read(fd,&myPrgHdr,sizeof(myPrgHdr))!=sizeof(myPrgHdr)) */
/* 	fail("Could not read ELF program table entry in %s",fname); */
/*       fixEndian(&myPrgHdr); */
/*       if(myPrgHdr.p_type==PT_LOAD){ */
/* 	VAddr secBegAddr=myPrgHdr.p_vaddr; */
/* 	VAddr secEndAddr=secBegAddr+myPrgHdr.p_memsz; */
/* 	if((!allocBegAddr)||(secBegAddr<allocBegAddr)) */
/* 	  allocBegAddr=secBegAddr; */
/* 	if((!allocEndAddr)||(secEndAddr>allocEndAddr)) */
/* 	  allocEndAddr=secEndAddr; */
/*       } */
/*     } */
/*   } */
  
/*   // Create real memory for loadable segments in the thread's address space */
/*   if(!addrSpace->mapMemory(allocBegAddr,allocEndAddr-allocBegAddr)){ */
/*     fprintf(stderr,"loadElfObject: Loadable segments overlap with existing simulated memory\n"); */
/*     exit(1); */
/*   } */
/*   addrSpace->initDataSegment(allocEndAddr); */

/*   // Now make another pass through the program (segment) table. This time: */
/*   // 1) Load and initialize the actual data for PT_LOAD segments */
/*   // 2) Find the initial value of the global pointer (GP) from the PT_MIPS_REGINFO segment */
/*   // 3) Check if unsupported segment types are used */

/*   { */
/*     off_t currFilePtr=prgTabOff; */
/*     for(uint16_t prgTabIdx=0;prgTabIdx<prgTabCnt;prgTabIdx++){ */
/*       if(lseek(fd,currFilePtr,SEEK_SET)!=currFilePtr) */
/* 	fail("Couldn't seek to current ELF program table position in %s",fname); */
/*       Elf32_Phdr myPrgHdr; */
/*       if(read(fd,&myPrgHdr,sizeof(myPrgHdr))!=sizeof(myPrgHdr)) */
/* 	fail("Could not read ELF program table entry in %s",fname); */
/*       fixEndian(&myPrgHdr); */
/*       if((currFilePtr=lseek(fd,0,SEEK_CUR))==(off_t)-1) */
/* 	fail("Could not get current file position in %s",fname); */
/*       switch(myPrgHdr.p_type){ */
/*       case PT_LOAD: */
/* 	if(myPrgHdr.p_filesz){ */
/* 	  if(lseek(fd,myPrgHdr.p_offset,SEEK_SET)!=myPrgHdr.p_offset) */
/* 	    fail("Couldn't seek to a PT_LOAD segment in ELF file %s",fname); */
/* 	  if(read(fd,(void *)(addrSpace->virtToReal(myPrgHdr.p_vaddr)),myPrgHdr.p_filesz)!=myPrgHdr.p_filesz) */
/* 	    fail("Could not read a PT_LOAD segment in ELF file %s",fname); */
/* 	} */
/* 	if(myPrgHdr.p_memsz>myPrgHdr.p_filesz){ */
/* 	  memset((void *)(addrSpace->virtToReal(myPrgHdr.p_vaddr+myPrgHdr.p_filesz)),0,myPrgHdr.p_memsz-myPrgHdr.p_filesz); */
/* 	} */
/* 	break; */
/*       case PT_MIPS_REGINFO: */
/* 	{ */
/* 	  if(myPrgHdr.p_filesz!=sizeof(Elf32_RegInfo)) */
/* 	    fail("PT_MIPS_REGINFO section size mismatch in ELF file %s",fname); */
/* 	  Elf32_RegInfo myRegInfo; */
/* 	  off_t regInfoOff=myPrgHdr.p_offset; */
/* 	  if(lseek(fd,regInfoOff,SEEK_SET)!=regInfoOff) */
/* 	    fail("Couldn't seek to ELF RegInfo segment in %s",fname); */
/* 	  if(read(fd,&myRegInfo,sizeof(myRegInfo))!=sizeof(myRegInfo)) */
/* 	    fail("Could not read ELF RegInfo segment in %s",fname); */
/* 	  fixEndian(&myRegInfo); */
/* 	  if(myRegInfo.ri_cprmask[0] || myRegInfo.ri_cprmask[2] || myRegInfo.ri_cprmask[3]) */
/* 	    fail("Unsupported coprocessor registers used in ELF file %s",fname); */
/* 	  threadContext->setGlobPtr(myRegInfo.ri_gp_value); */
/* 	} */
/* 	break; */
/*       case PT_NULL: // This type of segment is ignored by definition */
/*       case PT_NOTE: // Auxiliary info that is not needed for execution */
/* 	break; */
/*       default: */
/* 	printf("Unsupported segment type 0x%lx ",(unsigned long)myPrgHdr.p_type); */
/* 	printf("FOffs 0x%lx, VAddr 0x%lx, PAddr 0x%lx, FSize 0x%lx, MSize 0x%lx, Align 0x%lx, Flags %c%c%c + 0x%lx\n", */
/* 	       (unsigned long)myPrgHdr.p_offset, */
/* 	       (unsigned long)myPrgHdr.p_vaddr, */
/* 	       (unsigned long)myPrgHdr.p_paddr, */
/* 	       (unsigned long)myPrgHdr.p_filesz, */
/* 	       (unsigned long)myPrgHdr.p_memsz, */
/* 	       (unsigned long)myPrgHdr.p_align, */
/* 	       (myPrgHdr.p_flags&PF_R)?'R':' ', */
/* 	       (myPrgHdr.p_flags&PF_W)?'W':' ', */
/* 	       (myPrgHdr.p_flags&PF_X)?'X':' ', */
/* 	       (unsigned long)(myPrgHdr.p_flags^(myPrgHdr.p_flags&(PF_R|PF_W|PF_X))) */
/* 	       ); */
/*       } */
/*     } */
/*   } */
  
  // Get the section name string table
  char *secNamTab=0;
  if(myElfHdr.e_shstrndx==SHN_UNDEF)
    fail("No section name string table in ELF file %s",fname);
  else{
    if(lseek(fd,(off_t)(myElfHdr.e_shoff+myElfHdr.e_shstrndx*sizeof(Elf32_Shdr)),SEEK_SET)!=
       (off_t)(myElfHdr.e_shoff+myElfHdr.e_shstrndx*sizeof(Elf32_Shdr)))
      fail("Couldn't seek to section for section name string table in ELF file %s",fname);
    Elf32_Shdr mySecHdr;
    if(read(fd,&mySecHdr,sizeof(mySecHdr))!=sizeof(mySecHdr))
      fail("Could not read ELF section table entry %d in %s",myElfHdr.e_shstrndx,fname);
    cvtEndianBig(mySecHdr);
    if(mySecHdr.sh_type!=SHT_STRTAB)
      fail("Section table entry for section name string table is not of SHT_STRTAB type in ELF file %s",fname);
    if(lseek(fd,mySecHdr.sh_offset,SEEK_SET)!=mySecHdr.sh_offset)
      fail("Could not seek to section name string table in ELF file %s",fname);
    secNamTab=(char *)malloc(mySecHdr.sh_size);
    if(read(fd,secNamTab,mySecHdr.sh_size)!=mySecHdr.sh_size)
      fail("Could not read the section name string table in ELF file %s",fname);
  }
    
  // Read symbol tables and their string tables
  {
    off_t secTabPos=myElfHdr.e_shoff;
    for(uint16_t secTabIdx=0;secTabIdx<myElfHdr.e_shnum;secTabIdx++){
      Elf32_Shdr mySymSecHdr;
      if(lseek(fd,secTabPos,SEEK_SET)!=secTabPos)
	fail("Couldn't seek to current ELF section table position in %s",fname);
      if(read(fd,&mySymSecHdr,sizeof(mySymSecHdr))!=sizeof(mySymSecHdr))
	fail("Could not read ELF section table entry in %s",fname);
      cvtEndianBig(mySymSecHdr);
      if((secTabPos=lseek(fd,0,SEEK_CUR))==(off_t)-1)
	fail("Could not get current file position in %s",fname);
      if(mySymSecHdr.sh_type==SHT_SYMTAB){
	if(mySymSecHdr.sh_entsize!=sizeof(Elf32_Sym))
	  fail("Symbol table has entries of wrong size in ELF file %s",fname);
	Elf32_Shdr myStrSecHdr;
	uint64_t  symTabSize=mySymSecHdr.sh_size;
	Elf32_Off symTabOffs=mySymSecHdr.sh_offset;
	Elf32_Section strTabSec=mySymSecHdr.sh_link;
	if(lseek(fd,(off_t)(myElfHdr.e_shoff+strTabSec*sizeof(myStrSecHdr)),SEEK_SET)!=
	   (off_t)(myElfHdr.e_shoff+strTabSec*sizeof(myStrSecHdr)))
	  fail("Couldn't seek to string table section %d for symbol table section %d in ELF file %s",
		secTabIdx,strTabSec,fname);
	if(read(fd,&myStrSecHdr,sizeof(myStrSecHdr))!=sizeof(myStrSecHdr))
	  fail("Could not read ELF section table entry %d in %s",fname,secTabIdx);
	cvtEndianBig(myStrSecHdr);
	if(myStrSecHdr.sh_type!=SHT_STRTAB)
	  fail("SYMTAB section %d links to non-STRTAB section %d in ELF file %s",secTabIdx,strTabSec,fname);
	if(lseek(fd,myStrSecHdr.sh_offset,SEEK_SET)!=myStrSecHdr.sh_offset)
	  fail("Could not seek to string table for SYMTAB section %d in ELF file %s",secTabIdx,fname);
	char *strTab=(char *)malloc(myStrSecHdr.sh_size);
	if(read(fd,strTab,myStrSecHdr.sh_size)!=myStrSecHdr.sh_size)
	  fail("Could not read string table for SYMTAB section %d in ELF file %s",secTabIdx,fname);
	if(lseek(fd,symTabOffs,SEEK_SET)!=symTabOffs)
	  fail("Could not seek to symbol table for section %d in ELF file %s",secTabIdx,fname);
	for(uint64_t symTabIdx=0;symTabIdx<symTabSize/sizeof(Elf32_Sym);symTabIdx++){
	  Elf32_Sym mySym;
	  if(read(fd,&mySym,sizeof(mySym))!=sizeof(mySym))
	    fail("Could not read symbol in ELF file %s",fname);
	  cvtEndianBig(mySym);
	  switch(ELF32_ST_TYPE(mySym.st_info)){
	  case STT_FILE:   // Ignore file name symbols
	  case STT_OBJECT: // Ignore data symbols
	  case STT_SECTION:// Ignore section symbols
	  case STT_NOTYPE: // Ignore miscelaneous (no-type) symbols
	    break;
	  case STT_FUNC:   // Function entry point
	    char *funcName=strTab+mySym.st_name;
	    VAddr funcAddr=mySym.st_value;
	    if(funcAddr)
	      addrSpace->addFuncName(funcName,funcAddr);
	    break;
	  default:
	    fail("Unknown symbol type %d for symbol %s value %x\n",
		  ELF32_ST_TYPE(mySym.st_info),
		  strTab+mySym.st_name,
		  mySym.st_value
		  );
	  }
	}
	free(strTab);
      }
    }
  }
  
  // Decode instructions in executable sections
  // also, check for unsupported section types
  {
    VAddr brkPos=0;
    off_t secTabPos=myElfHdr.e_shoff;
    for(uint16_t secTabIdx=0;secTabIdx<myElfHdr.e_shnum;secTabIdx++){
      Elf32_Shdr mySecHdr;
      if(lseek(fd,secTabPos,SEEK_SET)!=secTabPos)
	fail("Couldn't seek to current ELF section table position in %s",fname);
      if(read(fd,&mySecHdr,sizeof(mySecHdr))!=sizeof(mySecHdr))
	fail("Could not read ELF section table entry in %s",fname);
      cvtEndianBig(mySecHdr);
      if((secTabPos=lseek(fd,0,SEEK_CUR))==(off_t)-1)
	fail("Could not get current file position in %s",fname);
      switch(mySecHdr.sh_type){
      case SHT_NULL:         // This section is unused by definition
      case SHT_MIPS_DWARF:   // Debugging info, we ignore it
      case SHT_NOTE:         // Auxiliary info that is not needed for execution
      case SHT_STRTAB:       // String table, already parsed in previous section table pass
      case SHT_SYMTAB:       // Symbol table, already parsed in previous section table pass
	break;
      case SHT_MIPS_REGINFO: {
	// Register use info, parse to get initial global pointer value
	if(mySecHdr.sh_size!=sizeof(Elf32_RegInfo))
	  fail("SHT_MIPS_REGINFO section size mismatch in ELF file %s",fname);
	Elf32_RegInfo myRegInfo;
	off_t regInfoOff=mySecHdr.sh_offset;
	if(lseek(fd,regInfoOff,SEEK_SET)!=regInfoOff)
	  fail("Couldn't seek to ELF RegInfo segment in %s",fname);
	if(read(fd,&myRegInfo,sizeof(myRegInfo))!=sizeof(myRegInfo))
	  fail("Could not read ELF RegInfo segment in %s",fname);
	cvtEndianBig(myRegInfo);
	if(myRegInfo.ri_cprmask[0] || myRegInfo.ri_cprmask[2] || myRegInfo.ri_cprmask[3])
	  fail("Unsupported coprocessor registers used in ELF file %s",fname);
	Mips::setReg<uint32_t>(threadContext,static_cast<RegName>(Mips::RegGP),myRegInfo.ri_gp_value);
      } break;
      case SHT_NOBITS: case SHT_PROGBITS: {
	// Code/data section, need to allocate space for it
	if(mySecHdr.sh_flags&SHF_ALLOC){
	  I(addrSpace->isNoSegment(mySecHdr.sh_addr,mySecHdr.sh_size));
	  addrSpace->newSegment(mySecHdr.sh_addr,mySecHdr.sh_size,false,true,false);
	  if(mySecHdr.sh_type==SHT_PROGBITS){
	    if(lseek(fd,mySecHdr.sh_offset,SEEK_SET)!=mySecHdr.sh_offset)
	      fail("Couldn't seek to a SHT_PROGBITS section in ELF file %s",fname);
	    if(threadContext->writeMemFromFile(mySecHdr.sh_addr,mySecHdr.sh_size,fd)!=mySecHdr.sh_size)
	      fail("Could not read a SHT_PROGBITS section from ELF file %s",fname);
	  }else{
	    threadContext->writeMemWithByte(mySecHdr.sh_addr,mySecHdr.sh_size,0);
	  }
	  addrSpace->protectSegment(mySecHdr.sh_addr,mySecHdr.sh_size,true,mySecHdr.sh_flags&SHF_WRITE,mySecHdr.sh_flags&SHF_EXECINSTR);
	  if(mySecHdr.sh_addr+mySecHdr.sh_size>brkPos)
	    brkPos=mySecHdr.sh_addr+mySecHdr.sh_size;
	}
      } break;
      default:
	printf("Unsupported section type 0x%08x ",mySecHdr.sh_type);
	printf("FOffs 0x%08x VAddr 0x%08x Size 0x%08x Align 0x%lx Flags %c%c%c + 0x%lx",
	       mySecHdr.sh_offset,
	       mySecHdr.sh_addr,
	       mySecHdr.sh_size,
	       (unsigned long)mySecHdr.sh_addralign,
	       (mySecHdr.sh_flags&SHF_ALLOC)?'A':' ',
	       (mySecHdr.sh_flags&SHF_WRITE)?'W':' ',
	       (mySecHdr.sh_flags&SHF_EXECINSTR)?'X':' ',
	       (unsigned long)(mySecHdr.sh_flags^(mySecHdr.sh_flags&(SHF_ALLOC|SHF_WRITE|SHF_EXECINSTR)))
	       );
	printf("\n");
      }
    }
    addrSpace->setBrkBase(brkPos);
  }
  close(fd);
  
  // Set instruction pointer to the program entry point
  VAddr entryIAddr=myElfHdr.e_entry;
  threadContext->setNextInstDesc(threadContext->virt2inst(entryIAddr));
  threadContext->setJumpInstDesc(0);
}
