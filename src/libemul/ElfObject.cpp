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

template<class ElfN_Ehdr, int endian>
void cvtEndianEhdr(ElfN_Ehdr &ehdr){
  // Array e_ident is all chars and need no endian conversion
  cvtEndian<typeof(ehdr.e_type),     endian,__BYTE_ORDER>(ehdr.e_type);
  cvtEndian<typeof(ehdr.e_machine),  endian,__BYTE_ORDER>(ehdr.e_machine);
  cvtEndian<typeof(ehdr.e_version),  endian,__BYTE_ORDER>(ehdr.e_version);
  cvtEndian<typeof(ehdr.e_entry),    endian,__BYTE_ORDER>(ehdr.e_entry);
  cvtEndian<typeof(ehdr.e_phoff),    endian,__BYTE_ORDER>(ehdr.e_phoff);
  cvtEndian<typeof(ehdr.e_shoff),    endian,__BYTE_ORDER>(ehdr.e_shoff);
  cvtEndian<typeof(ehdr.e_flags),    endian,__BYTE_ORDER>(ehdr.e_flags);
  cvtEndian<typeof(ehdr.e_ehsize),   endian,__BYTE_ORDER>(ehdr.e_ehsize);
  cvtEndian<typeof(ehdr.e_phentsize),endian,__BYTE_ORDER>(ehdr.e_phentsize);
  cvtEndian<typeof(ehdr.e_phnum),    endian,__BYTE_ORDER>(ehdr.e_phnum);
  cvtEndian<typeof(ehdr.e_shentsize),endian,__BYTE_ORDER>(ehdr.e_shentsize);
  cvtEndian<typeof(ehdr.e_shnum),    endian,__BYTE_ORDER>(ehdr.e_shnum);
  cvtEndian<typeof(ehdr.e_shstrndx), endian,__BYTE_ORDER>(ehdr.e_shstrndx);
}
template<> void cvtEndian<Elf32_Ehdr,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf32_Ehdr &ehdr){
  return cvtEndianEhdr<Elf32_Ehdr,__LITTLE_ENDIAN>(ehdr);
}
template<> void cvtEndian<Elf32_Ehdr,__BIG_ENDIAN,   __BYTE_ORDER>(Elf32_Ehdr &ehdr){
  return cvtEndianEhdr<Elf32_Ehdr,__BIG_ENDIAN   >(ehdr);
}
template<> void cvtEndian<Elf64_Ehdr,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf64_Ehdr &ehdr){
  return cvtEndianEhdr<Elf64_Ehdr,__LITTLE_ENDIAN>(ehdr);
}
template<> void cvtEndian<Elf64_Ehdr,__BIG_ENDIAN,   __BYTE_ORDER>(Elf64_Ehdr &ehdr){
  return cvtEndianEhdr<Elf64_Ehdr,__BIG_ENDIAN   >(ehdr);
}

template<class ElfN_Phdr, int endian>
void cvtEndianPhdr(ElfN_Phdr &phdr){
  cvtEndian<typeof(phdr.p_type),  endian,__BYTE_ORDER>(phdr.p_type);
  cvtEndian<typeof(phdr.p_offset),endian,__BYTE_ORDER>(phdr.p_offset);
  cvtEndian<typeof(phdr.p_vaddr), endian,__BYTE_ORDER>(phdr.p_vaddr);
  cvtEndian<typeof(phdr.p_paddr), endian,__BYTE_ORDER>(phdr.p_paddr);
  cvtEndian<typeof(phdr.p_filesz),endian,__BYTE_ORDER>(phdr.p_filesz);
  cvtEndian<typeof(phdr.p_memsz), endian,__BYTE_ORDER>(phdr.p_memsz);
  cvtEndian<typeof(phdr.p_flags), endian,__BYTE_ORDER>(phdr.p_flags);
  cvtEndian<typeof(phdr.p_align), endian,__BYTE_ORDER>(phdr.p_align);
}
template<> void cvtEndian<Elf32_Phdr,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf32_Phdr &phdr){
  return cvtEndianPhdr<Elf32_Phdr,__LITTLE_ENDIAN>(phdr);
}
template<> void cvtEndian<Elf32_Phdr,__BIG_ENDIAN,   __BYTE_ORDER>(Elf32_Phdr &phdr){
  return cvtEndianPhdr<Elf32_Phdr,__BIG_ENDIAN   >(phdr);
}
template<> void cvtEndian<Elf64_Phdr,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf64_Phdr &phdr){
  return cvtEndianPhdr<Elf64_Phdr,__LITTLE_ENDIAN>(phdr);
}
template<> void cvtEndian<Elf64_Phdr,__BIG_ENDIAN,   __BYTE_ORDER>(Elf64_Phdr &phdr){
  return cvtEndianPhdr<Elf64_Phdr,__BIG_ENDIAN   >(phdr);
}

template<class ElfN_Shdr, int endian>
void cvtEndianShdr(ElfN_Shdr &shdr){
  cvtEndian<typeof(shdr.sh_name),     endian,__BYTE_ORDER>(shdr.sh_name);
  cvtEndian<typeof(shdr.sh_type),     endian,__BYTE_ORDER>(shdr.sh_type);
  cvtEndian<typeof(shdr.sh_flags),    endian,__BYTE_ORDER>(shdr.sh_flags);
  cvtEndian<typeof(shdr.sh_addr),     endian,__BYTE_ORDER>(shdr.sh_addr);
  cvtEndian<typeof(shdr.sh_offset),   endian,__BYTE_ORDER>(shdr.sh_offset);
  cvtEndian<typeof(shdr.sh_size),     endian,__BYTE_ORDER>(shdr.sh_size);
  cvtEndian<typeof(shdr.sh_link),     endian,__BYTE_ORDER>(shdr.sh_link);
  cvtEndian<typeof(shdr.sh_info),     endian,__BYTE_ORDER>(shdr.sh_info);
  cvtEndian<typeof(shdr.sh_addralign),endian,__BYTE_ORDER>(shdr.sh_addralign);
  cvtEndian<typeof(shdr.sh_entsize),  endian,__BYTE_ORDER>(shdr.sh_entsize);
}
template<> void cvtEndian<Elf32_Shdr,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf32_Shdr &shdr){
  return cvtEndianShdr<Elf32_Shdr,__LITTLE_ENDIAN>(shdr);
}
template<> void cvtEndian<Elf32_Shdr,__BIG_ENDIAN,   __BYTE_ORDER>(Elf32_Shdr &shdr){
  return cvtEndianShdr<Elf32_Shdr,__BIG_ENDIAN   >(shdr);
}
template<> void cvtEndian<Elf64_Shdr,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf64_Shdr &shdr){
  return cvtEndianShdr<Elf64_Shdr,__LITTLE_ENDIAN>(shdr);
}
template<> void cvtEndian<Elf64_Shdr,__BIG_ENDIAN,   __BYTE_ORDER>(Elf64_Shdr &shdr){
  return cvtEndianShdr<Elf64_Shdr,__BIG_ENDIAN   >(shdr);
}

template<class ElfN_Sym, int endian>
void cvtEndianSym(ElfN_Sym &sym){
  cvtEndian<typeof(sym.st_name), endian,__BYTE_ORDER>(sym.st_name);
  cvtEndian<typeof(sym.st_value),endian,__BYTE_ORDER>(sym.st_value);
  cvtEndian<typeof(sym.st_size), endian,__BYTE_ORDER>(sym.st_size);
  cvtEndian<typeof(sym.st_info), endian,__BYTE_ORDER>(sym.st_info);
  cvtEndian<typeof(sym.st_other),endian,__BYTE_ORDER>(sym.st_other);
  cvtEndian<typeof(sym.st_shndx),endian,__BYTE_ORDER>(sym.st_shndx);
}
template<> void cvtEndian<Elf32_Sym,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf32_Sym &sym){
  return cvtEndianSym<Elf32_Sym,__LITTLE_ENDIAN>(sym);
}
template<> void cvtEndian<Elf32_Sym,__BIG_ENDIAN,   __BYTE_ORDER>(Elf32_Sym &sym){
  return cvtEndianSym<Elf32_Sym,__BIG_ENDIAN   >(sym);
}
template<> void cvtEndian<Elf64_Sym,__LITTLE_ENDIAN,__BYTE_ORDER>(Elf64_Sym &sym){
  return cvtEndianSym<Elf64_Sym,__LITTLE_ENDIAN>(sym);
}
template<> void cvtEndian<Elf64_Sym,__BIG_ENDIAN,   __BYTE_ORDER>(Elf64_Sym &sym){
  return cvtEndianSym<Elf64_Sym,__BIG_ENDIAN   >(sym);
}

class Elf32Defs{
  public:
  typedef Elf32_Ehdr Elf_Ehdr;
  typedef Elf32_Phdr Elf_Phdr;
  typedef Elf32_Shdr Elf_Shdr;
  typedef Elf32_Sym  Elf_Sym;
};
class Elf64Defs{
  public:
  typedef Elf64_Ehdr Elf_Ehdr;
  typedef Elf64_Phdr Elf_Phdr;
  typedef Elf64_Shdr Elf_Shdr;
  typedef Elf64_Sym  Elf_Sym;
};

// Helper functions for getExecMode
template<class ElfNDefs>
ExecMode getExecModeN(FileSys::FileStatus *fs, ExecMode mode, unsigned char ei_data);

template<class ElfNDefs, int endian>
ExecMode getExecModeE(FileSys::FileStatus *fs, ExecMode mode);

ExecMode getExecMode(FileSys::FileStatus *fs){
  // Read the e_ident part of the ELF header
  unsigned char e_ident[EI_NIDENT];
  if(pread(fs->fd,e_ident,EI_NIDENT,(off_t)0)!=EI_NIDENT)
    return ExecModeNone;
  if((e_ident[0]!=ELFMAG0)||(e_ident[1]!=ELFMAG1)||
     (e_ident[2]!=ELFMAG2)||(e_ident[3]!=ELFMAG3))
    return ExecModeNone;
  if(e_ident[EI_VERSION]!=EV_CURRENT)
    return ExecModeNone;
  switch(e_ident[EI_OSABI]){
  case ELFOSABI_NONE:
    break;
  default:
    return ExecModeNone;
  }
  if(e_ident[EI_ABIVERSION]!=0)
    return ExecModeNone;
  switch(e_ident[EI_CLASS]){
  case ELFCLASS32: return getExecModeN<Elf32Defs>(fs,ExecModeBits32,e_ident[EI_DATA]);
  case ELFCLASS64: return getExecModeN<Elf64Defs>(fs,ExecModeBits64,e_ident[EI_DATA]);
  }
  return ExecModeNone;
}
template<class ElfNDefs>
ExecMode getExecModeN(FileSys::FileStatus *fs, ExecMode mode, unsigned char ei_data){
  switch(ei_data){
  case ELFDATA2LSB: return getExecModeE<ElfNDefs,__LITTLE_ENDIAN>(fs,static_cast<ExecMode>(mode|ExecModeEndianLittle));
  case ELFDATA2MSB: return getExecModeE<ElfNDefs,__BIG_ENDIAN   >(fs,static_cast<ExecMode>(mode|ExecModeEndianBig));
  }
  return ExecModeNone;
}

template<class ElfNDefs, int endian>
ExecMode getExecModeE(FileSys::FileStatus *fs, ExecMode mode){
  typedef typename ElfNDefs::Elf_Ehdr Elf_Ehdr;
  typedef typename ElfNDefs::Elf_Phdr Elf_Phdr;
  typedef typename ElfNDefs::Elf_Shdr Elf_Shdr;
  Elf_Ehdr ehdr;
  if(pread(fs->fd,&ehdr,sizeof(Elf_Ehdr),(off_t)0)!=sizeof(Elf_Ehdr))
    return ExecModeNone;
  cvtEndian<Elf_Ehdr,endian,__BYTE_ORDER>(ehdr);
  if(ehdr.e_version!=EV_CURRENT)
    return ExecModeNone;
  if(ehdr.e_ehsize!=sizeof(Elf_Ehdr))
    return ExecModeNone;
  if(ehdr.e_phentsize!=sizeof(Elf_Phdr))
    return ExecModeNone;
  if(ehdr.e_shentsize!=sizeof(Elf_Shdr))
    return ExecModeNone;
  switch(ehdr.e_type){
  case ET_EXEC:
  case ET_DYN:
    break;
  default: fail("e_type is not ET_EXEC or ET_DYN\n");
  }
  switch(ehdr.e_machine){
  case EM_MIPS: {
    mode=static_cast<ExecMode>(mode|ExecModeArchMips);
    if(!(ehdr.e_flags&EF_MIPS_32BITMODE))
      fail("EM_MIPS e_flags doesn't set EF_MIPS_32BITMODE\n");
    switch(ehdr.e_flags&EF_MIPS_ABI){
    case EF_MIPS_ABI_O32:
      break;
    default:
      fail("EF_MIPS_ABI is not EF_MIPS_ABI_O32\n");
    }
    switch(ehdr.e_flags&EF_MIPS_ARCH){
    case EF_MIPS_ARCH_1:
    case EF_MIPS_ARCH_2:
    case EF_MIPS_ARCH_3:
    case EF_MIPS_ARCH_4:
      break;
    default:
      fail("EF_MIPS_ARCH above EF_MIPS_ARCH4 was used\n");
    }
    if(ehdr.e_flags&~(EF_MIPS_ARCH|EF_MIPS_ABI|EF_MIPS_NOREORDER|EF_MIPS_PIC|EF_MIPS_CPIC|EF_MIPS_32BITMODE))
      fail("Unknown e_machine (0x%08x) for EM_MIPS\n",ehdr.e_flags);
  } break;
  default: fail("e_machine is %d not EM_MIPS for %s\n",ehdr.e_machine,fs->name);
  }
  return mode;
}

template<class ElfNDefs>
void mapFuncNamesN(ThreadContext *context, FileSys::FileStatus *fs, ExecMode mode, VAddr addr, size_t len, off_t off);
template<class ElfNDefs, int endian>
void mapFuncNamesE(ThreadContext *context, FileSys::FileStatus *fs, ExecMode mode, VAddr addr, size_t len, off_t off);

void mapFuncNames(ThreadContext *context, FileSys::FileStatus *fs, ExecMode mode, VAddr addr, size_t len, off_t off){
  switch(mode&ExecModeBitsMask){
  case ExecModeBits32: return mapFuncNamesN<Elf32Defs>(context,fs,mode,addr,len,off);
  case ExecModeBits64: return mapFuncNamesN<Elf64Defs>(context,fs,mode,addr,len,off);
  defualt: fail("mapFuncNames: ExecModeBits is not 32 or 64\n");
  }
}
template<class ElfNDefs>
void mapFuncNamesN(ThreadContext *context, FileSys::FileStatus *fs, ExecMode mode, VAddr addr, size_t len, off_t off){
  switch(mode&ExecModeEndianMask){
  case ExecModeEndianBig:    return mapFuncNamesE<ElfNDefs,__BIG_ENDIAN   >(context,fs,mode,addr,len,off);
  case ExecModeEndianLittle: return mapFuncNamesE<ElfNDefs,__LITTLE_ENDIAN>(context,fs,mode,addr,len,off);
  defualt: fail("mapFuncNames: ExecModeEndian is not Little or Big\n");
  }
}
template<class ElfNDefs, int endian>
void mapFuncNamesE(ThreadContext *context, FileSys::FileStatus *fs, ExecMode mode, VAddr addr, size_t len, off_t off){
  typedef typename ElfNDefs::Elf_Ehdr Elf_Ehdr;
  typedef typename ElfNDefs::Elf_Shdr Elf_Shdr;
  typedef typename ElfNDefs::Elf_Sym  Elf_Sym;
  // Read in the ELF header
  Elf_Ehdr ehdr;
  ssize_t ehdrSiz=pread(fs->fd,&ehdr,sizeof(Elf_Ehdr),0);
  cvtEndian<Elf_Ehdr,endian,__BYTE_ORDER>(ehdr);
  I(ehdrSiz==sizeof(Elf_Ehdr));
  // Read in section headers
  Elf_Shdr shdrs[ehdr.e_shnum];
  ssize_t shdrsSiz=pread(fs->fd,shdrs,sizeof(Elf_Shdr)*ehdr.e_shnum,ehdr.e_shoff);
  I(shdrsSiz==(ssize_t)(sizeof(Elf_Shdr)*ehdr.e_shnum));
  I(shdrsSiz==(ssize_t)(sizeof(shdrs)));
  for(size_t sec=0;sec<ehdr.e_shnum;sec++)
    cvtEndian<Elf_Shdr,endian,__BYTE_ORDER>(shdrs[sec]);
  // Read in section name strings
  I((ehdr.e_shstrndx>0)&&(ehdr.e_shstrndx<ehdr.e_shnum));
  char secStrTab[shdrs[ehdr.e_shstrndx].sh_size];
  ssize_t secStrTabSiz=pread(fs->fd,secStrTab,shdrs[ehdr.e_shstrndx].sh_size,shdrs[ehdr.e_shstrndx].sh_offset);
  I(secStrTabSiz==(ssize_t)(shdrs[ehdr.e_shstrndx].sh_size));
  // Iterate over all sections
  for(size_t sec=0;sec<ehdr.e_shnum;sec++){
    switch(shdrs[sec].sh_type){
    case SHT_PROGBITS: {
      if(!(shdrs[sec].sh_flags&SHF_EXECINSTR))
        break;
      ssize_t loadBias=(addr-shdrs[sec].sh_addr)+(shdrs[sec].sh_offset-off);
      char  *secNam=secStrTab+shdrs[sec].sh_name;
      size_t secNamLen=strlen(secNam);
      if((off_t(shdrs[sec].sh_offset)>=off)&&(shdrs[sec].sh_offset<off+len)){
        char  *begNam="SecBeg";
        size_t begNamLen=strlen(begNam);
        char symNam[secNamLen+begNamLen+1];
        strcpy(symNam,begNam);
        strcpy(symNam+begNamLen,secNam);
        VAddr symAddr=shdrs[sec].sh_addr+loadBias;
        context->getAddressSpace()->addFuncName(symAddr,symNam,fs->name);
      }
      if((off_t(shdrs[sec].sh_offset+shdrs[sec].sh_size)>off)&&
         (shdrs[sec].sh_offset+shdrs[sec].sh_size<=off+len)){
        char  *endNam="SecEnd";
        size_t endNamLen=strlen(endNam);
        char symNam[secNamLen+endNamLen+1];
        strcpy(symNam,endNam);
        strcpy(symNam+endNamLen,secNam);
        VAddr symAddr=shdrs[sec].sh_addr+shdrs[sec].sh_size+loadBias;
        context->getAddressSpace()->addFuncName(symAddr,symNam,fs->name);
      }
    } break;
    case SHT_SYMTAB:
    case SHT_DYNSYM: {
      I(shdrs[sec].sh_entsize==sizeof(Elf_Sym));
      // Read in the symbols
      size_t symnum=shdrs[sec].sh_size/sizeof(Elf_Sym);
      Elf_Sym syms[symnum];
      ssize_t symsSiz=pread(fs->fd,syms,shdrs[sec].sh_size,shdrs[sec].sh_offset);
      I(symsSiz==(ssize_t)(sizeof(Elf_Sym)*symnum));
      I(symsSiz==(ssize_t)(sizeof(syms)));
      for(size_t sym=0;sym<symnum;sym++)
        cvtEndian<Elf_Sym,endian,__BYTE_ORDER>(syms[sym]);
      // Read in the symbol name strings
      char strTab[shdrs[shdrs[sec].sh_link].sh_size];
      ssize_t strTabSiz=pread(fs->fd,strTab,shdrs[shdrs[sec].sh_link].sh_size,shdrs[shdrs[sec].sh_link].sh_offset);
      I(strTabSiz==(ssize_t)(shdrs[shdrs[sec].sh_link].sh_size));
      for(size_t sym=0;sym<symnum;sym++){
        I(ELF32_ST_TYPE(syms[sym].st_info)==ELF64_ST_TYPE(syms[sym].st_info));
        switch(ELF64_ST_TYPE(syms[sym].st_info)){
        case STT_FUNC: {
          if(!syms[sym].st_shndx)
            break;
          ssize_t loadBias=(addr-shdrs[syms[sym].st_shndx].sh_addr)+
                           (shdrs[syms[sym].st_shndx].sh_offset-off);
//          printf("sh_type %s st_name %s st_value 0x%08x st_size 0x%08x st_shndx 0x%08x\n",
//                 shdrs[sec].sh_type==SHT_SYMTAB?"SYMTAB":"DYNSYM",
//                 strTab+syms[sym].st_name,
//                 (int)(syms[sym].st_value),(int)(syms[sym].st_size),(int)(syms[sym].st_shndx));
          char *symNam=strTab+syms[sym].st_name;
          VAddr symAddr=syms[sym].st_value+loadBias;
          if((symAddr<addr)||(symAddr>=addr+len))
            break;
	  context->getAddressSpace()->addFuncName(symAddr,symNam,fs->name);
        }  break;
        }
      }
    }  break;
    }
  }
}

VAddr loadElfObject(ThreadContext *context, FileSys::FileStatus *fs,
                    VAddr addr, ExecMode mode, bool isInterpreter);
template<class ElfNDefs>
VAddr loadElfObjectN(ThreadContext *context, FileSys::FileStatus *fs,
                     VAddr addr, ExecMode mode, bool isInterpreter);
template<class ElfNDefs, int endian>
VAddr loadElfObjectE(ThreadContext *context, FileSys::FileStatus *fs,
                     VAddr addr, ExecMode mode, bool isInterpreter);

VAddr loadElfObject(ThreadContext *context, char *fname){
  size_t realNameLen=FileSys::FileNames::getFileNames()->getReal(fname,0,0);
  char realName[realNameLen];
  FileSys::FileNames::getFileNames()->getReal(fname,realNameLen,realName);
  FileSys::FileStatus *fs=FileSys::FileStatus::open(realName,O_RDONLY,0);
  ExecMode mode=getExecMode(fs);
  // TODO: Use ELF_ET_DYN_BASE instead of a constant here
  VAddr addr=0x200000;
  return loadElfObject(context,fs,addr,mode,false);  
}
VAddr loadElfObject(ThreadContext *context, FileSys::FileStatus *fs,
                    VAddr addr, ExecMode mode, bool isInterpreter){
  if(mode==ExecModeNone)
    mode=getExecMode(fs);  
  switch(mode&ExecModeBitsMask){
  case ExecModeBits32: return loadElfObjectN<Elf32Defs>(context,fs,addr,mode,isInterpreter);
  case ExecModeBits64: return loadElfObjectN<Elf64Defs>(context,fs,addr,mode,isInterpreter);
  defualt: fail("mapFuncNames: ExecModeBits is not 32 or 64\n");
  }
  return VAddr(-1);
}
template<class ElfNDefs>
VAddr loadElfObjectN(ThreadContext *context, FileSys::FileStatus *fs,
                     VAddr addr, ExecMode mode, bool isInterpreter){
  switch(mode&ExecModeEndianMask){
  case ExecModeEndianBig:    return loadElfObjectE<ElfNDefs,__BIG_ENDIAN   >(context,fs,addr,mode,isInterpreter);
  case ExecModeEndianLittle: return loadElfObjectE<ElfNDefs,__LITTLE_ENDIAN>(context,fs,addr,mode,isInterpreter);
  defualt: fail("mapFuncNames: ExecModeEndian is not Little or Big\n");
  }
  return VAddr(-1);
}
template<class ElfNDefs, int endian>
VAddr loadElfObjectE(ThreadContext *context, FileSys::FileStatus *fs,
                     VAddr addr, ExecMode mode, bool isInterpreter){
  // Set if this is a dynamically linked executable and we found the interpreter
  bool hasInterpreter=false;
  // Types for ELF structs
  typedef typename ElfNDefs::Elf_Ehdr Elf_Ehdr;
  typedef typename ElfNDefs::Elf_Phdr Elf_Phdr;
  // Read in the ELF header
  Elf_Ehdr ehdr;
  ssize_t ehdrSiz=pread(fs->fd,&ehdr,sizeof(Elf_Ehdr),0);
  cvtEndian<Elf_Ehdr,endian,__BYTE_ORDER>(ehdr);
  I(ehdrSiz==sizeof(Elf_Ehdr));
  // Clear all the registers (this is actually needed for correct operation)
  context->clearRegs();
  // Set the ExecMode of the processor to match executable
  // TODO: Do this only for executable, for interpreter check if it matches
  I(mode==ExecModeMips32);
  context->setMode(Mips32);
  // Read in program (segment) headers
  Elf_Phdr phdrs[ehdr.e_phnum];
  ssize_t phdrsSiz=pread(fs->fd,phdrs,sizeof(Elf_Phdr)*ehdr.e_phnum,ehdr.e_phoff);
  I(phdrsSiz==(ssize_t)(sizeof(Elf_Phdr)*ehdr.e_phnum));
  I(phdrsSiz==(ssize_t)(sizeof(phdrs)));
  for(size_t seg=0;seg<ehdr.e_phnum;seg++)
    cvtEndian<Elf_Phdr,endian,__BYTE_ORDER>(phdrs[seg]);
  // Iterate over all segments to load interpreter and find top and bottom of address range
  VAddr  loReqAddr=0;
  VAddr  hiReqAddr=0;
  for(size_t seg=0;seg<ehdr.e_phnum;seg++){
    switch(phdrs[seg].p_type){
    case PT_INTERP: {
      char interpName[phdrs[seg].p_filesz];
      ssize_t interpNameSiz=pread(fs->fd,interpName,phdrs[seg].p_filesz,phdrs[seg].p_offset);
      I(interpNameSiz==ssize_t(phdrs[seg].p_filesz));
      FileSys::FileStatus *ifs=FileSys::FileStatus::open(interpName,O_RDONLY,0);
      addr=loadElfObject(context,ifs,addr,ExecModeNone,true);
      hasInterpreter=true;
    }  break;
    case PT_LOAD:
      if(loReqAddr==hiReqAddr){
	loReqAddr=phdrs[seg].p_vaddr;
	hiReqAddr=phdrs[seg].p_vaddr+phdrs[seg].p_memsz;
      }
      if(phdrs[seg].p_vaddr<loReqAddr)
	loReqAddr=phdrs[seg].p_vaddr;
      if(phdrs[seg].p_vaddr+phdrs[seg].p_memsz>hiReqAddr)
	hiReqAddr=phdrs[seg].p_vaddr+phdrs[seg].p_memsz;
      break;
    }
  }
  VAddr loadAddr=addr+context->getAddressSpace()->getPageSize()-1;
  loadAddr=loadAddr-(loadAddr%context->getAddressSpace()->getPageSize());
  switch(ehdr.e_type){
  case ET_EXEC: loadAddr=loReqAddr; break;
  case ET_DYN:  break;
  }
  I(context->getAddressSpace()->isNoSegment(loadAddr,hiReqAddr-loReqAddr));
  for(size_t seg=0;seg<ehdr.e_phnum;seg++){
    if(phdrs[seg].p_type!=PT_LOAD)
      continue;
    VAddr  segFilAddr=loadAddr+(phdrs[seg].p_vaddr-loReqAddr);
    size_t segFilLen=phdrs[seg].p_filesz;
    VAddr  segMapAddr=context->getAddressSpace()->pageAlignDown(segFilAddr);
    size_t segMapLen=context->getAddressSpace()->pageAlignUp((segFilAddr-segMapAddr)+phdrs[seg].p_memsz);
    // Map segment into address space
    I(context->getAddressSpace()->isNoSegment(segMapAddr,segMapLen));
    context->getAddressSpace()->newSegment(segMapAddr,segMapLen,false,true,false);
    ssize_t segRdLen=context->writeMemFromFile(segFilAddr,segFilLen,fs->fd,true,true,phdrs[seg].p_offset);
    I(segRdLen==ssize_t(segFilLen));
    context->getAddressSpace()->protectSegment(segMapAddr,segMapLen,
					       phdrs[seg].p_flags&PF_R,
					       phdrs[seg].p_flags&PF_W,
					       phdrs[seg].p_flags&PF_X);
    mapFuncNames(context,fs,mode,segFilAddr,segFilLen,phdrs[seg].p_offset);
    if((!isInterpreter)&&(phdrs[seg].p_offset<=ehdr.e_phoff)&&
       (phdrs[seg].p_offset+phdrs[seg].p_filesz>=ehdr.e_phoff+ehdr.e_phnum*sizeof(Elf_Phdr))){
      context->getAddressSpace()->addFuncName(segFilAddr+ehdr.e_phoff-phdrs[seg].p_offset,"PrgHdrAddr","");
      context->getAddressSpace()->addFuncName(sizeof(Elf_Phdr),"PrgHdrEnt" ,"");
      context->getAddressSpace()->addFuncName(ehdr.e_phnum,"PrgHdrNum","");
    }
  }
  context->getAddressSpace()->setBrkBase(loadAddr+(hiReqAddr-loReqAddr));
  VAddr entryAddr=loadAddr+(ehdr.e_entry-loReqAddr);
  // This is the entry point of the program (not the interpreter)
  if(!isInterpreter)
    context->getAddressSpace()->addFuncName(entryAddr,"UserEntry","");
  // If there is an interpreter, we enter there, otherwise we enter the program
  if(isInterpreter||!hasInterpreter)
    context->setIAddr(entryAddr);
  return loadAddr+(hiReqAddr-loReqAddr);
}
