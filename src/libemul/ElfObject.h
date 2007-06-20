#if !(defined ELF_OBJECT_H)
#define ELF_OBJECT_H

#include <elf.h>

// Define MIPS-specific stuff if it is missing
#if !(defined EF_MIPS_ARCH)
/* Legal values for e_flags field of Elf32_Ehdr.  */

#define EF_MIPS_NOREORDER   1           /* A .noreorder directive was used */
#define EF_MIPS_PIC         2           /* Contains PIC code */
#define EF_MIPS_CPIC        4           /* Uses PIC calling sequence */
#define EF_MIPS_XGOT        8
#define EF_MIPS_64BIT_WHIRL 16
#define EF_MIPS_ABI2        32
#define EF_MIPS_ABI_ON32    64
#define EF_MIPS_ARCH        0xf0000000  /* MIPS architecture level */

/* Legal values for MIPS architecture level.  */

#define EF_MIPS_ARCH_1      0x00000000  /* -mips1 code.  */
#define EF_MIPS_ARCH_2      0x10000000  /* -mips2 code.  */
#define EF_MIPS_ARCH_3      0x20000000  /* -mips3 code.  */
#define EF_MIPS_ARCH_4      0x30000000  /* -mips4 code.  */
#define EF_MIPS_ARCH_5      0x40000000  /* -mips5 code.  */
#define EF_MIPS_ARCH_32     0x60000000  /* MIPS32 code.  */
#define EF_MIPS_ARCH_64     0x70000000  /* MIPS64 code.  */

/* Legal values for sh_type field of Elf32_Shdr.  */

#define SHT_MIPS_LIBLIST       0x70000000 /* Shared objects used in link */
#define SHT_MIPS_MSYM          0x70000001
#define SHT_MIPS_CONFLICT      0x70000002 /* Conflicting symbols */
#define SHT_MIPS_GPTAB         0x70000003 /* Global data area sizes */
#define SHT_MIPS_UCODE         0x70000004 /* Reserved for SGI/MIPS compilers */
#define SHT_MIPS_DEBUG         0x70000005 /* MIPS ECOFF debugging information*/
#define SHT_MIPS_REGINFO       0x70000006 /* Register usage information */
#define SHT_MIPS_PACKAGE       0x70000007
#define SHT_MIPS_PACKSYM       0x70000008
#define SHT_MIPS_RELD          0x70000009
#define SHT_MIPS_IFACE         0x7000000b
#define SHT_MIPS_CONTENT       0x7000000c
#define SHT_MIPS_OPTIONS       0x7000000d /* Miscellaneous options.  */
#define SHT_MIPS_SHDR          0x70000010
#define SHT_MIPS_FDESC         0x70000011
#define SHT_MIPS_EXTSYM        0x70000012
#define SHT_MIPS_DENSE         0x70000013
#define SHT_MIPS_PDESC         0x70000014
#define SHT_MIPS_LOCSYM        0x70000015
#define SHT_MIPS_AUXSYM        0x70000016
#define SHT_MIPS_OPTSYM        0x70000017
#define SHT_MIPS_LOCSTR        0x70000018
#define SHT_MIPS_LINE          0x70000019
#define SHT_MIPS_RFDESC        0x7000001a
#define SHT_MIPS_DELTASYM      0x7000001b
#define SHT_MIPS_DELTAINST     0x7000001c
#define SHT_MIPS_DELTACLASS    0x7000001d
#define SHT_MIPS_DWARF         0x7000001e /* DWARF debugging information.  */
#define SHT_MIPS_DELTADECL     0x7000001f
#define SHT_MIPS_SYMBOL_LIB    0x70000020
#define SHT_MIPS_EVENTS        0x70000021 /* Event section.  */
#define SHT_MIPS_TRANSLATE     0x70000022
#define SHT_MIPS_PIXIE         0x70000023
#define SHT_MIPS_XLATE         0x70000024
#define SHT_MIPS_XLATE_DEBUG   0x70000025
#define SHT_MIPS_WHIRL         0x70000026
#define SHT_MIPS_EH_REGION     0x70000027
#define SHT_MIPS_XLATE_OLD     0x70000028
#define SHT_MIPS_PDR_EXCEPTION 0x70000029

/* Entry found in sections of type SHT_MIPS_REGINFO.  */

typedef struct
{
  Elf32_Word    ri_gprmask;             /* General registers used */
  Elf32_Word    ri_cprmask[4];          /* Coprocessor registers used */
  Elf32_Sword   ri_gp_value;            /* $gp register value */
} Elf32_RegInfo;
#endif

class ThreadContext;

int  checkElfObject(const char *fname);
void loadElfObject(const char *fname, ThreadContext *threadContext);

#endif
