#ifndef MCOFF_H_
#define MCOFF_H_
/* structure definitions needed to read MIPS coff files */

struct filehdr {
    unsigned short f_magic;
    unsigned short f_nscns;
	 int f_timdat;
	 int f_symptr;
	 int f_nsyms;
    unsigned short f_opthdr;
    unsigned short f_flags;
};

struct scnhdr {
    char s_name[8];
	 int s_paddr;
	 int s_vaddr;
	 int s_size;
	 int s_scnptr;
	 int s_relptr;
	 int s_lnnoptr;
    unsigned short s_nreloc;
    unsigned short s_nlnno;
	 int s_flags;
};

struct aouthdr {
    short magic;
    short vstamp;
	 int tsize;
	 int dsize;
	 int bsize;
	 int entry;
	 int text_start;
	 int data_start;
	 int bss_start;
	 int gprmask;
	 int cprmask[4];
	 int gp_value;
};

typedef struct symhdr_t {
    short magic;
    short vstamp;
	 int ilineMax;
	 int cbLine;
	 int cbLineOffset;
	 int idnMax;
	 int cbDnOffset;
	 int ipdMax;
	 int cbPdOffset;
	 int isymMax;
	 int cbSymOffset;
	 int ioptMax;
	 int cbOptOffset;
	 int iauxMax;
	 int cbAuxOffset;
	 int issMax;
	 int cbSsOffset;
	 int issExtMax;
	 int cbSsExtOffset;
	 int ifdMax;
	 int cbFdOffset;
	 int crfd;
	 int cbRfdOffset;
	 int iextMax;
	 int cbExtOffset;
} HDRR;

#define magicSym 0x7009

typedef struct fdr {
	 unsigned int adr;
	 int rss;
	 int issBase;
	 int cbSs;
	 int isymBase;
	 int csym;
	 int ilineBase;
	 int cline;
	 int ioptBase;
	 int copt;
    unsigned short ipdFirst;
    unsigned short cpd;
	 int iauxBase;
	 int caux;
	 int rfdBase;
	 int crfd;
    unsigned lang :5;
    unsigned fMerge :1;
    unsigned fReadin :1;
    unsigned fBigendian :1;
    unsigned reserved :24;
	 int cbLineOffset;
	 int cbLine;
} FDR;

typedef struct pdr {
	 unsigned int adr;
	 int isym;
	 int iline;
	 int regmask;
	 int regoffset;
	 int iopt;
	 int fregmask;
	 int fregoffset;
	 int frameoffset;
    short framereg;
    short pcreg;
	 int lnLow;
	 int lnHigh;
	 int cbLineOffset;
} PDR;

typedef struct {
	 int iss;
	 int value;
    unsigned st :6;
    unsigned sc :5;
    unsigned reserved :1;
    unsigned index :20;
} SYMR;

typedef struct {
    short reserved;
    short ifd;
    SYMR asym;
} EXTR;

#ifndef R_SN_BSS
#define R_SN_TEXT 1
#define R_SN_RDATA 2
#define R_SN_DATA 3
#define R_SN_SDATA 4
#define R_SN_SBSS 5
#define R_SN_BSS 6

#define STYP_TEXT 0x20
#define STYP_RDATA 0x100
#define STYP_DATA 0x40
#define STYP_SDATA 0x200
#define STYP_SBSS 0x400
#define STYP_BSS 0x80

#define stNil           0
#define stGlobal        1
#define stStatic        2
#define stParam         3
#define stLocal         4
#define stLabel         5
#define stProc          6
#define stBlock         7
#define stEnd           8
#define stMember        9
#define stTypedef       10
#define stFile          11
#define stRegReloc	12
#define stForward	13
#define stStaticProc	14
#define stConstant	15

#endif
#endif
