
#ifndef MELF_H
#define MELF_H

/* this structure is at the beginning of the ELF object file */
#define EI_NIDENT	16
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned long e_version;
    unsigned long e_entry;
    unsigned long e_phoff;
    unsigned long e_shoff;
    unsigned long e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
} Elf32_Ehdr;

/* the type for statically linked executables */
#define ET_EXEC	2
#define ET_DYN	3

#define EI_DATA            5       /* Data encoding */
#define ELFDATA2MSB        2

#define EF_MIPS_PIC		0x00000002
#define EF_MIPS_CPIC		0x00000004
#define EF_MIPS_ARCH		0xf0000000
#define EF_MIPS_ARCH_1		0x00000000
#define EF_MIPS_ARCH_2		0x10000000
#define EF_MIPS_ARCH_3		0x20000000
#define EF_MIPS_ARCH_4		0x30000000

#define EM_MIPS	8

/* section header */
typedef struct {
    unsigned long sh_name;
    unsigned long sh_type;
    unsigned long sh_flags;
    unsigned long sh_addr;
    unsigned long sh_offset;
    unsigned long sh_size;
    unsigned long sh_link;
    unsigned long sh_info;
    unsigned long sh_addralign;
    unsigned long sh_entsize;
} Elf32_Shdr;

#define SHT_PROGBITS	1
#define SHT_STRTAB	3
#define SHT_MDEBUG	0x70000005

typedef struct {
    unsigned long st_name;
    unsigned long st_value;
    unsigned long st_size;
    unsigned char st_info;
    unsigned char st_other;
    unsigned short st_shndx;
} Elf32_Sym;

#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2

#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION	3
#define STT_FILE	4

void elf_read_hdrs(char *objfile);
void elf_read_nmlist();


#endif
