/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

   Contributed by Paul Sack
                  Luis Ceze

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


#include "ThreadContext.h"
#include "minidecoder.h"
#include "PPCDecoder.h"

class TraceReader {
 private:
  FILE* trace;
  VAddr PC;
  ulong inst;
  VAddr address;
  int   count;

  void readPC() { if (fread(&PC, sizeof(PC), 1, trace) != sizeof(PC)) exit(1); }
  void readInst() { if (fread(&inst, sizeof(inst), 1,trace)!=sizeof(inst)) exit(1);}
  void readAddress() { if (fread(&address, sizeof(address), 1,trace)!=sizeof(address)) exit(1);}
  void readCount() { if (fread(&count, sizeof(count), 1,trace)!=sizeof(count)) exit(1);}
 public:
  void  openTrace(char* basename);

  VAddr getNextPC();
  long  getInst() { return inst; }
  VAddr getDataAddress() { return address; }
  int   getCount() { return count; }

  bool  isBranch() { return (PPCDecoder::getInstDef(inst)->type == iBJ) ; }
  bool  isMemory() { return tt6_isMemory(inst>>26, (inst>>1) & 0x3FF) ||
      tt6_isMemoryExtended(inst>>26, (inst>>1) & 0x3FF);}
  bool  isMemoryExtended() {return tt6_isMemoryExtended(inst>>26, (inst>>1) & 0x3FF);}
};
