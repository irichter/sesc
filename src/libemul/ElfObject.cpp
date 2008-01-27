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

#include "ElfObject.h"

#include <elf.h>
#include <fcntl.h>
#include "ThreadContext.h"
#include "CvtEndian.h"
#include "MipsRegs.h"
// To get definition of fail()
#include "EmulInit.h"

template<>
void cvtEndian(Elf32_Ehdr &ehdr, int byteOrder){
  // Array e_ident is all chars and need no endian conversion
  cvtEndian(ehdr.e_type,byteOrder);
  cvtEndian(ehdr.e_machine,byteOrder);
  cvtEndian(ehdr.e_version,byteOrder);
  cvtEndian(ehdr.e_entry,byteOrder);
  cvtEndian(ehdr.e_phoff,byteOrder);
  cvtEndian(ehdr.e_shoff,byteOrder);
  cvtEndian(ehdr.e_flags,byteOrder);
  cvtEndian(ehdr.e_ehsize,byteOrder);
  cvtEndian(ehdr.e_phentsize,byteOrder);
  cvtEndian(ehdr.e_phnum,byteOrder);
  cvtEndian(ehdr.e_shentsize,byteOrder);
  cvtEndian(ehdr.e_shnum,byteOrder);
  cvtEndian(ehdr.e_shstrndx,byteOrder);
}

template<>
void cvtEndian(Elf32_Phdr &phdr, int byteOrder){
  cvtEndian(phdr.p_type,byteOrder);
  cvtEndian(phdr.p_offset,byteOrder);
  cvtEndian(phdr.p_vaddr,byteOrder);
  cvtEndian(phdr.p_paddr,byteOrder);
  cvtEndian(phdr.p_filesz,byteOrder);
  cvtEndian(phdr.p_memsz,byteOrder);
  cvtEndian(phdr.p_flags,byteOrder);
  cvtEndian(phdr.p_align,byteOrder);
}

void cvtEndian(Elf32_Shdr &shdr, int byteOrder){
  cvtEndian(shdr.sh_name,byteOrder);
  cvtEndian(shdr.sh_type,byteOrder);
  cvtEndian(shdr.sh_flags,byteOrder);
  cvtEndian(shdr.sh_addr,byteOrder);
  cvtEndian(shdr.sh_offset,byteOrder);
  cvtEndian(shdr.sh_size,byteOrder);
  cvtEndian(shdr.sh_link,byteOrder);
  cvtEndian(shdr.sh_info,byteOrder);
  cvtEndian(shdr.sh_addralign,byteOrder);
  cvtEndian(shdr.sh_entsize,byteOrder);
}

void cvtEndian(Elf32_Sym &sym, int byteOrder){
  cvtEndian(sym.st_name,byteOrder);
  cvtEndian(sym.st_value,byteOrder);
  cvtEndian(sym.st_size,byteOrder);
  cvtEndian(sym.st_info,byteOrder);
  cvtEndian(sym.st_other,byteOrder);
  cvtEndian(sym.st_shndx,byteOrder);
}

void cvtEndianBig(Elf32_RegInfo &info){
  cvtEndianBig(info.ri_gprmask);
  for(int i=0;i<4;i++)
    cvtEndianBig(info.ri_cprmask[i]);
  cvtEndianBig(info.ri_gp_value);
}

int  checkElfObject(const char *fname){
  char *myfname=(char *)(alloca(strlen(fname)+strlen(".Sesc")+1));
  strcpy(myfname,fname);
  int fd=open(myfname,O_RDONLY);
  if(fd==-1){
    strcpy(myfname+strlen(fname),".Sesc");
    fd==open(myfname,O_RDONLY);
    if(fd==-1)
      return errno;
  }
  // Read the ELF header
  Elf32_Ehdr myElfHdr;
  if(read(fd,&myElfHdr,sizeof(myElfHdr))!=sizeof(myElfHdr))
    return ENOEXEC;
  char elfMag[]= {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 };
  if(memcmp(myElfHdr.e_ident,elfMag,sizeof(elfMag)))
    return ENOEXEC;
  int byteOrder=0;
  if(myElfHdr.e_ident[EI_DATA]==ELFDATA2MSB)
    byteOrder=__BIG_ENDIAN;
  else if(myElfHdr.e_ident[EI_DATA]==ELFDATA2LSB)
    byteOrder=__LITTLE_ENDIAN;
  if(myElfHdr.e_ident[EI_CLASS]!=ELFCLASS32)
    return ENOEXEC;
  cvtEndian(myElfHdr,byteOrder);
  if((myElfHdr.e_ident[EI_VERSION]!=EV_CURRENT) ||
     (myElfHdr.e_version!=EV_CURRENT))
    return ENOEXEC;
  if(myElfHdr.e_ehsize!=sizeof(myElfHdr))
    return ENOEXEC;
  switch(myElfHdr.e_machine){
    case EM_MIPS:
    case EM_386:
      break;
    default:
      return ENOEXEC;
  }
  if(myElfHdr.e_type!=ET_EXEC)
    return ENOEXEC;
  int mipsArch;
  switch(myElfHdr.e_flags&EF_MIPS_ARCH){
  case EF_MIPS_ARCH_1: mipsArch=1; break;    
  case EF_MIPS_ARCH_2: mipsArch=2; break;
  case EF_MIPS_ARCH_3: mipsArch=3; break;
  case EF_MIPS_ARCH_4: mipsArch=4; break;
  default:
    return ENOEXEC;
  }
  if(myElfHdr.e_phentsize!=sizeof(Elf32_Phdr))
    return ENOEXEC;
  if(myElfHdr.e_shentsize!=sizeof(Elf32_Shdr))
    return ENOEXEC;
  close(fd);
  return 0;					\
}

void loadElfObject(const char *fname, ThreadContext *threadContext){
  size_t realNameLen=FileSys::FileNames::getFileNames()->getReal(fname,0,0);
  char realName[realNameLen];
  FileSys::FileNames::getFileNames()->getReal(fname,realNameLen,realName);
  char *myfname=realName;
  //  char *myfname=(char *)(alloca(strlen(fname)+strlen(".Sesc")+1));
  //  strcpy(myfname,fname);
  int fd=open(myfname,O_RDONLY);
  //  if(fd==-1){
  //    strcpy(myfname+strlen(fname),".Sesc");
  //    fd==open(myfname,O_RDONLY);
  //    if(fd==-1)
  //      fail("Could not open ELF file %s (nor %s)",fname,myfname);
  //  }
  // Read the ELF header
  Elf32_Ehdr myElfHdr;
  if(read(fd,&myElfHdr,sizeof(myElfHdr))!=sizeof(myElfHdr))
    fail("Could not read ELF header for file %s",myfname);
  char elfMag[]= {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 };
  if(memcmp(myElfHdr.e_ident,elfMag,sizeof(elfMag)))
    fail("Not an ELF file: %s",myfname);
  int byteOrder=0;
  if(myElfHdr.e_ident[EI_DATA]==ELFDATA2MSB)
    byteOrder=__BIG_ENDIAN;
  else if(myElfHdr.e_ident[EI_DATA]==ELFDATA2LSB)
    byteOrder=__LITTLE_ENDIAN;
  if(myElfHdr.e_ident[EI_CLASS]!=ELFCLASS32)
    fail("Not a 32-bit ELF file: %s",myfname);
  cvtEndian(myElfHdr,byteOrder);
  if((myElfHdr.e_ident[EI_VERSION]!=EV_CURRENT) ||
     (myElfHdr.e_version!=EV_CURRENT))
    fail("Wrong ELF version in file %s",myfname);
  if(myElfHdr.e_ehsize!=sizeof(myElfHdr))
    fail("Wrong ELF header size in file %s\n",myfname);
  if(myElfHdr.e_type!=ET_EXEC)
    fail("Not an executable file %s",myfname);
  if(myElfHdr.e_machine==EM_MIPS){
    int mipsArch;
    switch(myElfHdr.e_flags&EF_MIPS_ARCH){
    case EF_MIPS_ARCH_1: mipsArch=1; break;    
    case EF_MIPS_ARCH_2: mipsArch=2; break;
    case EF_MIPS_ARCH_3: mipsArch=3; break;
    case EF_MIPS_ARCH_4: mipsArch=4; break;
    default:
      fail("Using architecture above MIPS4 in ELF file %s",myfname);
      mipsArch=-1;
      break;
    }
//     printf("Executable %s uses mips%d architecture\n",fname,mipsArch);
    // It's a 32-bit MIPS file, but which ABI?
    threadContext->setMode((myElfHdr.e_flags&EF_MIPS_ABI2)?Mips64:Mips32);
  }else if(myElfHdr.e_machine==EM_386){
    printf("Executable %s uses the x86 architecture\n",fname);
    threadContext->setMode(x86_32);
  }else{
    fail("Unknown e_machine in ELF file %s",myfname);
  }
  //  if(myElfHdr.e_flags&(EF_MIPS_PIC|EF_MIPS_CPIC))
  //    printf("Executable %s contains or uses PIC code\n",fname);
  if(myElfHdr.e_phentsize!=sizeof(Elf32_Phdr))
    fail("Wrong ELF program table entry size in %s",myfname);
  if(myElfHdr.e_shentsize!=sizeof(Elf32_Shdr))
    fail("Wrong ELF section table entry size in %s",myfname);

  // Clear all the registers (this is actually needed for correct operation)
  threadContext->clearRegs();
  
  // Address space of the thread
  AddressSpace *addrSpace=threadContext->getAddressSpace();
  if(!addrSpace)
    fail("loadElfObject: thread has no address space\n");

  // Get the section name string table
  char *secNamTab=0;
  if(myElfHdr.e_shstrndx==SHN_UNDEF)
    fail("No section name string table in ELF file %s",myfname);
  else{
    if(lseek(fd,(off_t)(myElfHdr.e_shoff+myElfHdr.e_shstrndx*sizeof(Elf32_Shdr)),SEEK_SET)!=
       (off_t)(myElfHdr.e_shoff+myElfHdr.e_shstrndx*sizeof(Elf32_Shdr)))
      fail("Couldn't seek to section for section name string table in ELF file %s",myfname);
    Elf32_Shdr mySecHdr;
    if(read(fd,&mySecHdr,sizeof(mySecHdr))!=sizeof(mySecHdr))
      fail("Could not read ELF section table entry %d in %s",myElfHdr.e_shstrndx,myfname);
    cvtEndian(mySecHdr,byteOrder);
    if(mySecHdr.sh_type!=SHT_STRTAB)
      fail("Section table entry for section name string table is not of SHT_STRTAB type in ELF file %s",myfname);
    if(lseek(fd,mySecHdr.sh_offset,SEEK_SET)==(off_t)-1)
      fail("Could not seek to section name string table in ELF file %s",myfname);
    secNamTab= new char[mySecHdr.sh_size];
    if(read(fd,secNamTab,mySecHdr.sh_size)!=ssize_t(mySecHdr.sh_size))
      fail("Could not read the section name string table in ELF file %s",myfname);
  }
    
  // Read symbol tables and their string tables
  {
    off_t secTabPos=myElfHdr.e_shoff;
    for(uint16_t secTabIdx=0;secTabIdx<myElfHdr.e_shnum;secTabIdx++){
      Elf32_Shdr mySymSecHdr;
      if(lseek(fd,secTabPos,SEEK_SET)!=secTabPos)
	fail("Couldn't seek to current ELF section table position in %s",myfname);
      if(read(fd,&mySymSecHdr,sizeof(mySymSecHdr))!=sizeof(mySymSecHdr))
	fail("Could not read ELF section table entry in %s",myfname);
      cvtEndian(mySymSecHdr,byteOrder);
      if((secTabPos=lseek(fd,0,SEEK_CUR))==(off_t)-1)
	fail("Could not get current file position in %s",myfname);
      if(mySymSecHdr.sh_type==SHT_SYMTAB){
	if(mySymSecHdr.sh_entsize!=sizeof(Elf32_Sym))
	  fail("Symbol table has entries of wrong size in ELF file %s",myfname);
	Elf32_Shdr myStrSecHdr;
	uint64_t   symTabSize=mySymSecHdr.sh_size;
	Elf32_Off  symTabOffs=mySymSecHdr.sh_offset;
	Elf32_Word strTabSec=mySymSecHdr.sh_link;
	if(lseek(fd,(off_t)(myElfHdr.e_shoff+strTabSec*sizeof(myStrSecHdr)),SEEK_SET)!=
	   (off_t)(myElfHdr.e_shoff+strTabSec*sizeof(myStrSecHdr)))
	  fail("Couldn't seek to string table section %d for symbol table section %d in ELF file %s",
		secTabIdx,strTabSec,myfname);
	if(read(fd,&myStrSecHdr,sizeof(myStrSecHdr))!=sizeof(myStrSecHdr))
	  fail("Could not read ELF section table entry %d in %s",myfname,secTabIdx);
	cvtEndian(myStrSecHdr,byteOrder);
	if(myStrSecHdr.sh_type!=SHT_STRTAB)
	  fail("SYMTAB section %d links to non-STRTAB section %d in ELF file %s",secTabIdx,strTabSec,myfname);
	if(lseek(fd,myStrSecHdr.sh_offset,SEEK_SET)==(off_t)-1)
	  fail("Could not seek to string table for SYMTAB section %d in ELF file %s",secTabIdx,myfname);
	char *strTab=(char *)malloc(myStrSecHdr.sh_size);
	if(read(fd,strTab,myStrSecHdr.sh_size)!=ssize_t(myStrSecHdr.sh_size))
	  fail("Could not read string table for SYMTAB section %d in ELF file %s",secTabIdx,myfname);
	if(lseek(fd,symTabOffs,SEEK_SET)==(off_t)-1)
	  fail("Could not seek to symbol table for section %d in ELF file %s",secTabIdx,myfname);
	for(uint64_t symTabIdx=0;symTabIdx<symTabSize/sizeof(Elf32_Sym);symTabIdx++){
	  Elf32_Sym mySym;
	  if(read(fd,&mySym,sizeof(mySym))!=sizeof(mySym))
	    fail("Could not read symbol in ELF file %s",myfname);
	  cvtEndian(mySym,byteOrder);
	  switch(ELF32_ST_TYPE(mySym.st_info)){
	  case STT_FILE:   // Ignore file name symbols
          case STT_COMMON: // Ignore common data objects
	  case STT_OBJECT: // Ignore data objects
          case STT_TLS:    // Ignore thread-local data objects
	  case STT_SECTION:// Ignore section symbols
	  case STT_NOTYPE: // Ignore miscelaneous (no-type) symbols
	    break;
	  case STT_FUNC:   // Function entry point
	    {
	      char  *funcName=strTab+mySym.st_name;
	      VAddr  funcAddr=mySym.st_value;
	      if(funcAddr)
		addrSpace->addFuncName(funcName,funcAddr);
	    } break;
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
  
  // File offset to the program (segment table)
  off_t prgTabOff=myElfHdr.e_phoff;
  // Number of program (segment) table entries
  uint16_t prgTabCnt=myElfHdr.e_phnum;
  
  // Read the program table and map loadable segments
  off_t prgTabPos=myElfHdr.e_phoff;
  VAddr brkPos=0;
  for(uint16_t prgTabIdx=0;prgTabIdx<prgTabCnt;prgTabIdx++){
    Elf32_Phdr myPrgHdr;
    if(lseek(fd,prgTabPos,SEEK_SET)!=prgTabPos)
      fail("Couldn't seek to current ELF program table position in %s",myfname);
    if(read(fd,&myPrgHdr,sizeof(myPrgHdr))!=sizeof(myPrgHdr))
      fail("Could not read ELF program table entry in %s",myfname);
    cvtEndian(myPrgHdr,byteOrder);
    if((prgTabPos=lseek(fd,0,SEEK_CUR))==(off_t)-1)
      fail("Could not get current file position in %s",myfname);
    switch(myPrgHdr.p_type){
    case PT_NULL: break;
    case PT_LOAD:
      // Loadable program segment
      I(addrSpace->isNoSegment(myPrgHdr.p_vaddr,myPrgHdr.p_memsz));
      addrSpace->newSegment(myPrgHdr.p_vaddr,myPrgHdr.p_memsz,false,true,false);
      if(myPrgHdr.p_filesz){
	if(lseek(fd,myPrgHdr.p_offset,SEEK_SET)==(off_t)-1)
	  fail("Couldn't seek to a PT_LOAD segment in ELF file %s",myfname);
	if(threadContext->writeMemFromFile(myPrgHdr.p_vaddr,myPrgHdr.p_filesz,fd,true)!=ssize_t(myPrgHdr.p_filesz))
	  fail("Could not read a PT_LOAD segment from ELF file %s",myfname);
      }
      if(myPrgHdr.p_memsz>myPrgHdr.p_filesz){
	threadContext->writeMemWithByte(myPrgHdr.p_vaddr+myPrgHdr.p_filesz,myPrgHdr.p_memsz-myPrgHdr.p_filesz,0);
      }
      addrSpace->protectSegment(myPrgHdr.p_vaddr,myPrgHdr.p_memsz,
				myPrgHdr.p_flags&PF_R,
				myPrgHdr.p_flags&PF_W,
				myPrgHdr.p_flags&PF_X);      
      if(myPrgHdr.p_vaddr+myPrgHdr.p_memsz>brkPos)
	brkPos=myPrgHdr.p_vaddr+myPrgHdr.p_memsz;
      if((myPrgHdr.p_offset<=myElfHdr.e_phoff)&&(myPrgHdr.p_offset+myPrgHdr.p_filesz>=myElfHdr.e_phoff+myElfHdr.e_phnum*myElfHdr.e_phentsize)){
	addrSpace->addFuncName("PrgHdrAddr",myPrgHdr.p_vaddr+myElfHdr.e_phoff-myPrgHdr.p_offset);
	addrSpace->addFuncName("PrgHdrNum" ,myElfHdr.e_phnum);
      }
      break;
    case PT_MIPS_REGINFO:
      // MIPS Register usage information
      break;
    case PT_NOTE: {
      // Auxiliary information
//       char buf[myPrgHdr.p_filesz];
//       lseek(fd,myPrgHdr.p_offset,SEEK_SET);
//       read(fd,&buf,myPrgHdr.p_filesz);
//       printf("PT_NOTE information:\n");
//       for(size_t i=0;i<myPrgHdr.p_filesz;i++)
// 	printf(" %02x",buf[i]);
//       printf("\n");
    } break;
    case PT_TLS:
      // Thread-local storage segment
      break;
    case PT_PHDR:
      // Location of the program header table
      break;
    case PT_GNU_EH_FRAME: break;
    case PT_INTERP: {
      // Interpreter information
      char buf[myPrgHdr.p_filesz];
      lseek(fd,myPrgHdr.p_offset,SEEK_SET);
      read(fd,&buf,myPrgHdr.p_filesz);
      printf("PT_INTERP information:\n");
      for(size_t i=0;i<myPrgHdr.p_filesz;i++)
        printf(" %02x",buf[i]);
      printf("\n");
      printf("As a string %s\n",buf);
    }  break;
    default:
      fail("Unknown program table entry type %d (0x%08x) for entry %d %s",
	   myPrgHdr.p_type,myPrgHdr.p_type,prgTabIdx,myfname);
    }
  }
  addrSpace->setBrkBase(brkPos);

  // Decode instructions in executable sections
  // also, check for unsupported section types
  {
//     VAddr brkPos=0;
    off_t secTabPos=myElfHdr.e_shoff;
    for(uint16_t secTabIdx=0;secTabIdx<myElfHdr.e_shnum;secTabIdx++){
      Elf32_Shdr mySecHdr;
      if(lseek(fd,secTabPos,SEEK_SET)!=secTabPos)
	fail("Couldn't seek to current ELF section table position in %s",myfname);
      if(read(fd,&mySecHdr,sizeof(mySecHdr))!=sizeof(mySecHdr))
	fail("Could not read ELF section table entry in %s",myfname);
      cvtEndian(mySecHdr,byteOrder);
      if((secTabPos=lseek(fd,0,SEEK_CUR))==(off_t)-1)
	fail("Could not get current file position in %s",myfname);
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
	  fail("SHT_MIPS_REGINFO section size mismatch in ELF file %s",myfname);
	Elf32_RegInfo myRegInfo;
	off_t regInfoOff=mySecHdr.sh_offset;
	if(lseek(fd,regInfoOff,SEEK_SET)!=regInfoOff)
	  fail("Couldn't seek to ELF RegInfo segment in %s",myfname);
	if(read(fd,&myRegInfo,sizeof(myRegInfo))!=sizeof(myRegInfo))
	  fail("Could not read ELF RegInfo segment in %s",myfname);
	cvtEndianBig(myRegInfo);
	if(myRegInfo.ri_cprmask[0] || myRegInfo.ri_cprmask[2] || myRegInfo.ri_cprmask[3])
	  fail("Unsupported coprocessor registers used in ELF file %s",myfname);
	Mips::setRegAny<Mips32,uint32_t,Mips::RegGP>(threadContext,static_cast<RegName>(Mips::RegGP),myRegInfo.ri_gp_value);
      } break;
      case SHT_NOBITS:
	// For a NOBITS section with SHF_ALLOC and SHF_TLS, we don't really allocate memory
	if((mySecHdr.sh_flags&SHF_ALLOC)&&(mySecHdr.sh_flags&SHF_TLS))
	  break;
	// Other NOBITS sections get actual memory allocated for them
      case SHT_PROGBITS: {
	// Code/data section, need to allocate space for it
	if(mySecHdr.sh_flags&SHF_ALLOC){
	  I(addrSpace->isInSegment(mySecHdr.sh_addr,mySecHdr.sh_size));
// 	  I(addrSpace->isNoSegment(mySecHdr.sh_addr,mySecHdr.sh_size));
// 	  addrSpace->newSegment(mySecHdr.sh_addr,mySecHdr.sh_size,false,true,false);
// 	  if(mySecHdr.sh_type==SHT_PROGBITS){
// 	    if(lseek(fd,mySecHdr.sh_offset,SEEK_SET)==(off_t)-1)
// 	      fail("Couldn't seek to a SHT_PROGBITS section in ELF file %s",myfname);
// 	    if(threadContext->writeMemFromFile(mySecHdr.sh_addr,mySecHdr.sh_size,fd,true)!=ssize_t(mySecHdr.sh_size))
// 	      fail("Could not read a SHT_PROGBITS section from ELF file %s",myfname);
// 	  }else{
// 	    threadContext->writeMemWithByte(mySecHdr.sh_addr,mySecHdr.sh_size,0);
// 	  }
          bool canWr=(mySecHdr.sh_flags&SHF_WRITE);
          bool canEx=(mySecHdr.sh_flags&SHF_EXECINSTR);
 	  //addrSpace->protectSegment(mySecHdr.sh_addr,mySecHdr.sh_size,true,canWr,canEx);
          if(canEx&&(addrSpace->getFuncName(mySecHdr.sh_addr+mySecHdr.sh_size)==0)){
            char  *secNam=secNamTab+mySecHdr.sh_name;
            size_t secNamLen=strlen(secNam);
            char  *endNam=" End ";
            size_t endNamLen=strlen(endNam);
            char symNam[secNamLen+endNamLen+1];
            strcpy(symNam,endNam);
            strcpy(symNam+endNamLen,secNam);
            addrSpace->addFuncName(symNam,mySecHdr.sh_addr+mySecHdr.sh_size);
          }
// 	  if(mySecHdr.sh_addr+mySecHdr.sh_size>brkPos)
// 	    brkPos=mySecHdr.sh_addr+mySecHdr.sh_size;
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
//    addrSpace->setBrkBase(brkPos);
  }
  delete [] secNamTab;
  close(fd);
  
  // Set instruction pointer to the program entry point
  VAddr entryIAddr=myElfHdr.e_entry;
  threadContext->setIAddr(entryIAddr);
}
