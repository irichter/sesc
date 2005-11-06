#ifndef ICODE_H
#define ICODE_H
/*
 * Definitions of icodes, threads, queues, and locks. Macros for
 * address mapping, and register dereferencing.
 *
 * Copyright (C) 1993 by Jack E. Veenstra (veenstra@cs.rochester.edu)
 * 
 * This file is part of MINT, a MIPS code interpreter and event generator
 * for parallel programs.
 * 
 * MINT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 * 
 * MINT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MINT; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "event.h"
#include "export.h"

#define _BSD_SIGNALS
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>

#include "mendian.h"

#if defined(DARWIN) || (defined(sparc) && !defined(__svr4__))
typedef unsigned long ulong;
#endif

typedef struct icode *icode_ptr;

typedef class ThreadContext *thread_ptr;

typedef icode_ptr (*PFPI)(icode_ptr, thread_ptr);
typedef icode_ptr (*PPFPI[])(icode_ptr, thread_ptr);
typedef icode_ptr (*PPFPI6[6])(icode_ptr, thread_ptr);
typedef icode_ptr (*PPFPI12[12])(icode_ptr, thread_ptr);

#include "globals.h"
#include "event.h"
#include "export.h"
#include "common.h"
#include "non_mips.h"
#include "Snippets.h"
#include "nanassert.h"

/* mnemonics for the register indices into the args[] array */
#define RS 0
#define RT 1
#define RD 2
#define SA 3

/* mnemonics for the coprocessor register indices into the args[] array */
#define FMT 0
#define ICODEFT 1
#define ICODEFS 2
#define ICODEFD 3

class icode {
 private:
#if (defined TLS)
  OpReplayClass myReplayClass;
#endif
public:
  unsigned int instID;
  PFPI func;			/* function that simulates this instruction */
  long addr;			/* text address of this instruction */
  char args[4];		/* the pre-shifted register args */
  short immed;		/* bits 0 - 15 */
  icode_ptr next;		/* pointer to next icode to execute */
  icode_ptr target;		/* for branches and jumps */
  icode_ptr not_taken;	/* target when branch not taken */
  unsigned int is_target : 1;	/* =1 if this instruction is a branch target */
  unsigned int opnum : 15;	/* arbitrary index for an instruction */
  unsigned int opflags : 16;	/* tells what kind of instruction */
  long instr;			/* undecoded instruction */

#if defined(LENDIAN)
  int getFPN(int R) { return args[R] >> 2; }
#else
  int getFPN(int R) { return (args[R] >> 2) ^ 1; }
#endif

  int getRN(int R) {
    return (R == SA ? (args[R]) : (args[R] >> 2));
  }
  int getDPN(int R) {
    return args[R] >> 2;
  }
  const char *getFPFMT(int F) {
    return ((F) ==  16 ? "s" : (F) == 17 ? "d" : (F) == 20 ? "w" : "?");
  }

  void dump();
  const char *dis_instr();
#if (defined TLS)
  void setReplayClass(OpReplayClass replayClass){
    myReplayClass=replayClass;
  }
  OpReplayClass getReplayClass(void) const{
    return myReplayClass;
  }
#endif
};

typedef struct icode  icode_t;

extern icode_ptr icodeArray;
extern size_t    icodeArraySize;

extern signed long Text_start;
extern signed long Text_end;
extern struct icode **Itext;

// Takes the logical address of an instruction, returns pointer to its icode
inline icode_ptr addr2icode(Address addr) {
  I(addr>=Text_start);
  icode_ptr picode=icodeArray+(addr-Text_start)/sizeof(icodeArray->instr);
  I((size_t)(picode-icodeArray)<icodeArraySize);
  return picode;
}

// Takes a pointer to an icode, returns the logical instruction address
inline Address icode2addr(icode_ptr picode) {
  I((size_t)(picode-icodeArray)<icodeArraySize);
  Address addr=Text_start+sizeof(icodeArray->instr)*(picode-icodeArray);
  I(addr>=Text_start);
  return addr;
}

#endif /* ICODE_H */

