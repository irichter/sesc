#ifndef MENDIAN_H_
#define MENDIAN_H_

#include "elf.h"
#include "coff.h"

void EndianElf32_Ehdr(Elf32_Ehdr *ehdr);
void EndianElf32_Shdr(Elf32_Shdr *shdr);
void EndianElf32_Sym(Elf32_Sym *sym);
void Endianaouthdr(struct aouthdr *h);
void Endiansymhdr(HDRR *h);
void Endianfilehdr(struct filehdr *h);
void Endianscnhdr(struct scnhdr *h);
void EndianFDR(FDR *h);
void EndianPDR(PDR *h);
void EndianSYMR(SYMR *h);
void EndianEXTR(EXTR *h);

#ifdef LENDIAN

#define SWAP_LONG(X) (( ((unsigned long long)SWAP_WORD((unsigned long)((X)>>32))) | \
                        ((unsigned long long)SWAP_WORD((unsigned long)(X)))<<32) )


#define SWAP_WORD(X) (((((unsigned long)(X)) >> 24) & 0x000000fful) | \
		      ((((unsigned long)(X)) >>  8) & 0x0000ff00ul) | \
                      ((((unsigned long)(X)) <<  8) & 0x00ff0000ul) | \
                      ((((unsigned long)(X)) << 24) & 0xff000000ul))

#define SWAP_SHORT(X) ( ((((unsigned short)X)& 0xff00) >> 8) | ((((unsigned short)X)& 0x00ff) << 8) )

#else

#define SWAP_LONG(X) (X)
#define SWAP_WORD(X) (X)
#define SWAP_SHORT(X) (X)

#endif

#endif /* MENDIAN_H_ */
