#ifndef MCOFF_H_
#define MCOFF_H_
/* structure definitions needed to read MIPS coff files */

struct filehdr {
    unsigned short f_magic;
    unsigned short f_nscns;
    long f_timdat;
    long f_symptr;
    long f_nsyms;
    unsigned short f_opthdr;
    unsigned short f_flags;
};

struct scnhdr {
    char s_name[8];
    long s_paddr;
    long s_vaddr;
    long s_size;
    long s_scnptr;
    long s_relptr;
    long s_lnnoptr;
    unsigned short s_nreloc;
    unsigned short s_nlnno;
    long s_flags;
};

struct aouthdr {
    short magic;
    short vstamp;
    long tsize;
    long dsize;
    long bsize;
    long entry;
    long text_start;
    long data_start;
    long bss_start;
    long gprmask;
    long cprmask[4];
    long gp_value;
};

typedef struct symhdr_t {
    short magic;
    short vstamp;
    long ilineMax;
    long cbLine;
    long cbLineOffset;
    long idnMax;
    long cbDnOffset;
    long ipdMax;
    long cbPdOffset;
    long isymMax;
    long cbSymOffset;
    long ioptMax;
    long cbOptOffset;
    long iauxMax;
    long cbAuxOffset;
    long issMax;
    long cbSsOffset;
    long issExtMax;
    long cbSsExtOffset;
    long ifdMax;
    long cbFdOffset;
    long crfd;
    long cbRfdOffset;
    long iextMax;
    long cbExtOffset;
} HDRR;

#define magicSym 0x7009

typedef struct fdr {
    unsigned long adr;
    long rss;
    long issBase;
    long cbSs;
    long isymBase;
    long csym;
    long ilineBase;
    long cline;
    long ioptBase;
    long copt;
    unsigned short ipdFirst;
    unsigned short cpd;
    long iauxBase;
    long caux;
    long rfdBase;
    long crfd;
    unsigned lang :5;
    unsigned fMerge :1;
    unsigned fReadin :1;
    unsigned fBigendian :1;
    unsigned reserved :24;
    long cbLineOffset;
    long cbLine;
} FDR;

typedef struct pdr {
    unsigned long adr;
    long isym;
    long iline;
    long regmask;
    long regoffset;
    long iopt;
    long fregmask;
    long fregoffset;
    long frameoffset;
    short framereg;
    short pcreg;
    long lnLow;
    long lnHigh;
    long cbLineOffset;
} PDR;

typedef struct {
    long iss;
    long value;
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
