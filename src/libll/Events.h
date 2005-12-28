/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Milos Prvulovic

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
#ifndef EVENTS_H
#define EVENTS_H

#include "icode.h"
#include "ThreadContext.h"

enum EventType {
  NoEvent = 0,
#ifdef TASKSCALAR
  SpawnEvent,
#endif
  PreEvent,
  PostEvent,
  FetchOpEvent,    // Memory rd/wr performed atomicaly. Notified as MemFence op
  MemFenceEvent,   // Release Consistency Barrier or memory fence
  AcquireEvent,    // Release Consistency Release
  ReleaseEvent,    // Release Consistency Ackquire
  UnlockEvent,     // Like above, but implemented
  FastSimBeginEvent,
  FastSimEndEvent,
  LibCallEvent,
  FakeInst,
  FakeInstMax = FakeInst+32,
  MaxEvent
};
// I(MaxEvent < SESC_MAXEVENT);

/* WARNING: If you add more events (MaxEvent) and MIPSInstruction.cpp
 * adds them, be sure that the Itext size also grows. (mint:globals.h)
 */

/* System events supported by sesc_postevent(....)
 */
static const int EvSimStop  = 1;
static const int EvSimStart = 2;

/*
 * The first 10 types are reserved for communicate with the
 * system. This means that you CAN NOT use sesc_preevent(...,3,...);
 */
static const int EvMinType  = 10;

extern icode_t invalidIcode;

#if defined(TASKSCALAR)
/* Event.cpp defined functions */
int  rsesc_become_safe(int pid);
int  rsesc_is_safe(int pid);
int  rsesc_is_versioned(int pid);
int rsesc_OS_prewrite(int pid, int addr, int iAddr, int flags);
void rsesc_OS_postwrite(int pid, int addr, int iAddr, int flags);
int rsesc_OS_read(int pid, int addr, int iAddr, int flags);
void rsesc_exception(int pid);
void mint_termination(int pid);
#endif

#if (defined TLS) || (defined TASKSCALAR)
// Reads a string from simulated virtual address srcStart into simulator's address dstStart,
// copying at most maxSize bytes in the process. Returns true if the entire string fits in dstStart, 
// false if destination buffer is full and the terminating null character has not been copied yet
bool rsesc_OS_read_string(int pid, VAddr iAddr, void *dst, VAddr src, size_t size);
// Writes a block to the simulated virtual address dstStart from simulator's address srcStart
void rsesc_OS_write_block(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size);
// Reads a block from simulated virtual address srcStart into simulator's address dstStart
void rsesc_OS_read_block(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size);
#endif

#endif   // EVENTS_H
