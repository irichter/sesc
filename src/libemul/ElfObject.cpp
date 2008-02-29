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
        context->getAddressSpace()->addFuncName(symNam,symAddr);
      }
      if((off_t(shdrs[sec].sh_offset+shdrs[sec].sh_size)>off)&&
         (shdrs[sec].sh_offset+shdrs[sec].sh_size<=off+len)){
        char  *endNam="SecEnd";
        size_t endNamLen=strlen(endNam);
        char symNam[secNamLen+endNamLen+1];
        strcpy(symNam,endNam);
        strcpy(symNam+endNamLen,secNam);
        VAddr symAddr=shdrs[sec].sh_addr+shdrs[sec].sh_size+loadBias;
        context->getAddressSpace()->addFuncName(symNam,symAddr);
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
	  context->getAddressSpace()->addFuncName(symNam,symAddr);
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
      context->getAddressSpace()->addFuncName("PrgHdrAddr",segFilAddr+ehdr.e_phoff-phdrs[seg].p_offset);
      context->getAddressSpace()->addFuncName("PrgHdrEnt" ,sizeof(Elf_Phdr));
      context->getAddressSpace()->addFuncName("PrgHdrNum" ,ehdr.e_phnum);
    }
  }
  context->getAddressSpace()->setBrkBase(loadAddr+(hiReqAddr-loReqAddr));
  VAddr entryAddr=loadAddr+(ehdr.e_entry-loReqAddr);
  // This is the entry point of the program (not the interpreter)
  if(!isInterpreter)
    context->getAddressSpace()->addFuncName("UserEntry" ,entryAddr);
  // If there is an interpreter, we enter there, otherwise we enter the program
  if(isInterpreter||!hasInterpreter)
    context->setIAddr(entryAddr);
  return loadAddr+(hiReqAddr-loReqAddr);
}

template<>
void cvtEndian(Elf32_Ehdr &ehdr, int32_t byteOrder){
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
void cvtEndian(Elf32_Phdr &phdr, int32_t byteOrder){
  cvtEndian(phdr.p_type,byteOrder);
  cvtEndian(phdr.p_offset,byteOrder);
  cvtEndian(phdr.p_vaddr,byteOrder);
  cvtEndian(phdr.p_paddr,byteOrder);
  cvtEndian(phdr.p_filesz,byteOrder);
  cvtEndian(phdr.p_memsz,byteOrder);
  cvtEndian(phdr.p_flags,byteOrder);
  cvtEndian(phdr.p_align,byteOrder);
}

template<>
void cvtEndian(Elf32_Shdr &shdr, int32_t byteOrder){
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

template<>
void cvtEndian(Elf32_Sym &sym, int32_t byteOrder){
  cvtEndian(sym.st_name,byteOrder);
  cvtEndian(sym.st_value,byteOrder);
  cvtEndian(sym.st_size,byteOrder);
  cvtEndian(sym.st_info,byteOrder);
  cvtEndian(sym.st_other,byteOrder);
  cvtEndian(sym.st_shndx,byteOrder);
}

template<>
void cvtEndian(Elf32_RegInfo &info, int32_t byteOrder){
  cvtEndian(info.ri_gprmask,byteOrder);
  for(int32_t i=0;i<4;i++)
    cvtEndian(info.ri_cprmask[i],byteOrder);
  cvtEndian(info.ri_gp_value,byteOrder);
}

template<>
void cvtEndian(Elf32_Rel &rel, int32_t byteOrder){
  cvtEndian(rel.r_offset,byteOrder);
  cvtEndian(rel.r_info,byteOrder);
}

int32_t  checkElfObject(const char *fname){
  fail("Using obsolete checkElfObject, should never happen\n");
  char *myfname=(char *)(alloca(strlen(fname)+strlen(".Sesc")+1));
  strcpy(myfname,fname);
  int32_t fd=open(myfname,O_RDONLY);
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
  int32_t byteOrder=0;
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
  switch(myElfHdr.e_type){
  case ET_EXEC: break;
  case ET_DYN: break;
  default: return ENOEXEC;
  }
  int32_t mipsArch;
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
  fail("Using obsolete loadElfObject, should never happen\n");
  // True is load address can no longer be changed, false if we can still change it
  bool  loadBiasSet=false;
  // Difference between where we actually load and where the file says we should load
  VAddr loadBias=0;
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
  int32_t byteOrder=0;
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
  switch(myElfHdr.e_type){
  case ET_EXEC:
    // Non-relocatable executable, loadbias must stay zero
    loadBiasSet=true;
    break;
  case ET_DYN:
    break;
  default: fail("Not an executable file %s",myfname);
  }
  if(myElfHdr.e_machine==EM_MIPS){
    int32_t mipsArch;
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
    case PT_LOAD: {
      // Loadable program segment
      VAddr loadAddr=myPrgHdr.p_vaddr;
      if(!loadBiasSet){
	// TODO: Use ELF_ET_DYN_BASE instead of a constant here
	loadBias=0x200000+(myPrgHdr.p_vaddr-myPrgHdr.p_offset);
        I(!(loadBias%addrSpace->getPageSize()));
	loadBiasSet=true;
      }
      loadAddr+=loadBias;
      VAddr  segAddr=loadAddr-(loadAddr%addrSpace->getPageSize());
      size_t segLen=(loadAddr-segAddr)+myPrgHdr.p_memsz;
      I(addrSpace->isNoSegment(segAddr,segLen));
      addrSpace->newSegment(segAddr,segLen,false,true,false);
      if(myPrgHdr.p_filesz){
	if(lseek(fd,myPrgHdr.p_offset,SEEK_SET)==(off_t)-1)
	  fail("Couldn't seek to a PT_LOAD segment in ELF file %s",myfname);
	if(threadContext->writeMemFromFile(loadAddr,myPrgHdr.p_filesz,fd,true)!=ssize_t(myPrgHdr.p_filesz))
	  fail("Could not read a PT_LOAD segment from ELF file %s",myfname);
      }
      addrSpace->protectSegment(segAddr,segLen,
				myPrgHdr.p_flags&PF_R,
				myPrgHdr.p_flags&PF_W,
				myPrgHdr.p_flags&PF_X);      
      if(loadAddr+myPrgHdr.p_memsz>brkPos)
	brkPos=loadAddr+myPrgHdr.p_memsz;
      if((myPrgHdr.p_offset<=myElfHdr.e_phoff)&&(myPrgHdr.p_offset+myPrgHdr.p_filesz>=myElfHdr.e_phoff+myElfHdr.e_phnum*myElfHdr.e_phentsize)){
	addrSpace->addFuncName("PrgHdrAddr",loadAddr+myElfHdr.e_phoff-myPrgHdr.p_offset);
	addrSpace->addFuncName("PrgHdrNum" ,myElfHdr.e_phnum);
      }
    } break;
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
    case PT_DYNAMIC:
      // Dynamic linking info
      break;
    case PT_GNU_RELRO:
      // Read-only after relocation
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
	      VAddr  funcAddr=mySym.st_value+loadBias;
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
      // Sections related to dynamic linking
      case SHT_REL: {
//         VAddr secAddr=mySecHdr.sh_addr+loadBias;
// 	I(mySecHdr.sh_flags&SHF_ALLOC);
// 	I(addrSpace->isInSegment(secAddr,mySecHdr.sh_size));
// 	for(VAddr ptr=secAddr;ptr<secAddr+mySecHdr.sh_size;ptr+=sizeof(Elf32_Rel)){
// 	  Elf32_Rel myRel=threadContext->readMemRaw<Elf32_Rel>(ptr);
// 	  cvtEndian(myRel,byteOrder);
// 	  printf("Reloc Offs 0x%08x Sym 0x%08x Type 0x%08x\n",myRel.r_offset,
// 		 ELF32_R_SYM(myRel.r_info), ELF32_R_TYPE(myRel.r_info));
// 	}
      } break;
      case SHT_HASH:
      case SHT_DYNAMIC:
      case SHT_DYNSYM:
      case SHT_GNU_verdef:
      case SHT_GNU_versym:
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
	cvtEndian(myRegInfo,byteOrder);
	if(myRegInfo.ri_cprmask[0] || myRegInfo.ri_cprmask[2] || myRegInfo.ri_cprmask[3])
	  fail("Unsupported coprocessor registers used in ELF file %s",myfname);
//	Mips::setRegAny<Mips32,uint32_t,Mips::RegGP>(threadContext,static_cast<RegName>(Mips::RegGP),myRegInfo.ri_gp_value+loadBias);
      } break;
      case SHT_NOBITS:
	// For a NOBITS section with SHF_ALLOC and SHF_TLS, we don't really allocate memory
	if((mySecHdr.sh_flags&SHF_ALLOC)&&(mySecHdr.sh_flags&SHF_TLS))
	  break;
	// Other NOBITS sections get actual memory allocated for them
      case SHT_PROGBITS: {
        VAddr secAddr=mySecHdr.sh_addr+loadBias;
	// Code/data section, need to allocate space for it
	if(mySecHdr.sh_flags&SHF_ALLOC){
	  I(addrSpace->isInSegment(secAddr,mySecHdr.sh_size));
// 	  I(addrSpace->isNoSegment(secAddr,mySecHdr.sh_size));
// 	  addrSpace->newSegment(secAddr,mySecHdr.sh_size,false,true,false);
// 	  if(mySecHdr.sh_type==SHT_PROGBITS){
// 	    if(lseek(fd,mySecHdr.sh_offset,SEEK_SET)==(off_t)-1)
// 	      fail("Couldn't seek to a SHT_PROGBITS section in ELF file %s",myfname);
// 	    if(threadContext->writeMemFromFile(secAddr,mySecHdr.sh_size,fd,true)!=ssize_t(mySecHdr.sh_size))
// 	      fail("Could not read a SHT_PROGBITS section from ELF file %s",myfname);
// 	  }else{
// 	    threadContext->writeMemWithByte(secAddr,mySecHdr.sh_size,0);
// 	  }
          bool canWr=(mySecHdr.sh_flags&SHF_WRITE);
          bool canEx=(mySecHdr.sh_flags&SHF_EXECINSTR);
 	  //addrSpace->protectSegment(secAddr,mySecHdr.sh_size,true,canWr,canEx);
          if(canEx&&(addrSpace->getFuncName(secAddr+mySecHdr.sh_size)==0)){
            char  *secNam=secNamTab+mySecHdr.sh_name;
            size_t secNamLen=strlen(secNam);
            char  *endNam=" End ";
            size_t endNamLen=strlen(endNam);
            char symNam[secNamLen+endNamLen+1];
            strcpy(symNam,endNam);
            strcpy(symNam+endNamLen,secNam);
            addrSpace->addFuncName(symNam,secAddr+mySecHdr.sh_size);
          }
// 	  if(secAddr+mySecHdr.sh_size>brkPos)
// 	    brkPos=secAddr+mySecHdr.sh_size;
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
  VAddr entryIAddr=myElfHdr.e_entry+loadBias;
  // This is the entry point of the program, even if there is an interpreter
  addrSpace->addFuncName("UserEntry" ,entryIAddr);
  // This is the entry point of the interpeter, if there is one
  threadContext->setIAddr(entryIAddr);
}
