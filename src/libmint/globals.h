/*
 * Definitions of all global variables that are used in more than one file,
 * not including globals that are exported to the simulator.
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
#ifndef GLOBALS_H
#define GLOBALS_H

#ifdef EXTERN
#undef EXTERN
#endif

#ifdef MAIN
#define EXTERN
#else
#define EXTERN extern
#endif

/* ROUNDUP() is a macro that rounds "val" up to a multiple of "align"
 * where "align" must be a power of two.
 */
#define ROUNDUP(val,align) (((val) + ((align) - 1)) & ~((align) - 1))

/* Align the simulated address spaces on an 8K boundary.
 */
#define M_ALIGN (8 * 1024)

/* File descriptor for MINT output. Defaults to stderr. Output can be
 * turned off from the user program by setting this to NULL.
 */
EXTERN FILE *Mint_output;

/* name of object file being traced */
EXTERN char *Objname;

/* name of simulator executable */
EXTERN char *Simname;

/* pointer to beginning of interpreted code */
EXTERN struct icode **Itext;

/* All the xxx_start variables are addresses in object's memory space,
 * except for Private_start.
 */
EXTERN signed long Text_start;/* text address when loaded in memory */
EXTERN signed long Text_end;		/* addr of word past end of text */
EXTERN signed long Text_entry;		/* addr of first instruction to execute */
EXTERN signed long Data_start;		/* start of data section in memory */
EXTERN signed long Data_end;		/* end of all private memory */
EXTERN signed long Rdata_start;	/* start of rdata section in memory */
EXTERN signed long Rdata_end;		/* end of rdata section */
EXTERN signed long Bss_start;		/* start of bss section in memory */
EXTERN signed long Private_start;	/* start of private memory */
EXTERN signed long Private_end;	/* end of private memory */
EXTERN signed long Heap_start;         /* start of malloc memory */
EXTERN signed long Heap_end;		/* malloc memory allocated so far */
EXTERN signed long Stack_start;         /* start of stack memory */
EXTERN signed long Stack_end;

EXTERN unsigned long Text_size;		/* text size in words (instructions) */
EXTERN unsigned long Data_size;		/* data size in bytes */
EXTERN unsigned long Rdata_size;		/* rdata size in bytes */
EXTERN signed long Rsize_round;	/* rdata size rounded up to a nice boundary */
EXTERN unsigned long Bss_size;		/* bss size in bytes */
EXTERN unsigned long Stack_size;		/* stack size in bytes */
EXTERN unsigned long Heap_size;		/* heap size in bytes (for private malloc) */
EXTERN unsigned long DB_size;		/* data + bss size in bytes */
EXTERN unsigned long Mem_size;		/* data+bss+heap+(stack) size in bytes */

EXTERN unsigned long Text_seek;		/* seek offset in file for text section */
EXTERN unsigned long Rdata_seek;		/* seek offset in file for rdata section */
EXTERN unsigned long Data_seek;		/* seek offset in file for data section */
EXTERN unsigned long Sdata_seek;		/* seek offset in file for sdata section */

/* default stack size */
#ifndef STACK_SIZE
#define STACK_SIZE (32 * 1024)
#endif

/* default heap size (for private malloc) */
#ifndef HEAP_SIZE
#define HEAP_SIZE 0x8000000
#endif

/* leave some space at the top of the stack for sproc() children */
#define FRAME_SIZE 48

/* use this to enforce a minimum size for the stack */
#ifndef STACK_SIZE_MIN
#define STACK_SIZE_MIN (8 * 1024)
#endif

/* use this to enforce a minimum size for the heap */
#ifndef HEAP_SIZE_MIN
#define HEAP_SIZE_MIN (4 * 1024)
#endif

/* address of the errno variable in the object program */
EXTERN long Errno_addr;

/* address of the environ variable in the object program */
EXTERN long Environ_addr;

/* text address of the exit() routine (used by the sproc() call) */
EXTERN long Exit_addr;

/* All queues are circular doubly linked lists with a head node.
 * When a thread exits, it goes on the Done_q until the parent waits on
 * it. If a parent exits before its children, it goes to sleep. When the
 * child exits, it wakes up its parent. The parent moves finished children
 * from the Done_q to the Free_q.
 *
 * Run_q is the head node for the run queue.
 * Free_q is the head node for the free queue.
 *  ... and so on.
 */

/* the maximum process id ever used (process ids start at zero) */
EXTERN int Maxpid;

/* The maximum number of processes that can be simulated. This is used
 * to allocate space. This value can be changed with the "-p" command
 * line option.
 */
EXTERN int Max_nprocs;

/* the number of function pointers we need to allocate */
extern int Nfuncs;

#define SESC_MAXEVENT 16

/* Extra icode pointers allocated at the end of Itext[]. */
enum {
  DONE_ICODE = 0, 
  RESTORE_ICODE, 
  MFORKPARENT_ICODE, 
  MFORKCHILD_ICODE, 
  SUSPEND_ICODE,
  SESC_FAKEICODE=SUSPEND_ICODE+SESC_MAXEVENT,
  EXTRA_ICODES
};

/* globals set by command line options */
EXTERN int Every_write_ll;	/* every write checks for a load-linked */

typedef enum OpReplayClassEnum{
  OpExposed,  // Op can not be rolled back nor replayed and is not prepared for TLS execution
  OpNoReplay, // Op can not be rolled back nor replayed, but is prepared for TLS execution
  OpUndoable, // Op can be fully rolled back and replayed with SysCall recording
  OpInternal  // Op can be fully rolled back and replayed without any special action
} OpReplayClass;

/* This structure is used to store info needed at fork time to relocate
 * pointers to functions that are used in the sgi library for calling
 * lock functions.
 */
typedef struct func_desc_t{
  char *name;	      // Name of function pointer to replace
  PFPI func;	      // Substitute function that acts as a replacement
  int no_spec;        // The func calls can not be executed speculatively, sync to non-spec
  OpReplayClass replayClass; // Replay classification of this function for TLS
} func_desc_t, *func_desc_ptr;

extern func_desc_t Func_subst[];

#ifndef MAIN
/* array of starting addresses for sections, used in relocation */
EXTERN long Section_start[];
#endif

/* The minimum value of a function symbol in the list of names looked up.
 * Anything after this point is assumed to be library code.
 */
EXTERN unsigned long Min_lib_value;

EXTERN int Nlocalsyms;
EXTERN long Gp_value;

/* function prototypes */
icode_ptr newcopy_icode(icode_ptr picode);
void copy_addr_space(thread_ptr parent, thread_ptr child);
int decode_instr(icode_ptr picode, int instr);

#endif /* GLOBALS_H */
