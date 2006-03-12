/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2005 University California, Santa Cruz.

   Contributed by Saangetha
                  Keertika
		  Jose Renau

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

#include <sys/types.h>
#include <dirent.h>

#include "nanassert.h"
#include "RSTReader.h"
#include "rstf.h"
#include "Rstzip.h"

RSTReader::RSTReader()
  : Max_Num_Recs(4096) {

  buf = (rstf_unionT *)malloc(sizeof(rstf_unionT)*Max_Num_Recs);
  buf_pos = 0;
  buf_end = 0;
}

void RSTReader::openTrace(const char *filename) {

  rz = new Rstzip;
  int rv=rz->open(filename, "r", "verbose=0");
  if (rv != RSTZIP_OK) {
    MSG("ERROR: RSTReader::openTrace(%s) error opening input trace", filename);
    exit(1);
  }

  end_of_trace = false;
  advancePC(0);
}

void RSTReader::closeTrace() {
  rz->close();
  delete rz;
}

void RSTReader::advancePC(int fid) {

  if (end_of_trace) {
    PC =  0xffffffff;
    return;
  }

  readInst(fid);
}

void RSTReader::readInst(int fid) { 

  do {
    while (buf_pos < buf_end) {
      rstf_unionT * rp = &buf[buf_pos++];

      // skip markers, only INSTR_T
      if (rp->proto.rtype != INSTR_T)
	continue;

      I(rstf_instrT_get_cpuid(&(rp->instr))==fid); // FIXME: not multi now
      
      PC      = rp->instr.pc_va;
      inst    = rp->instr.instr;
      address = rp->instr.ea_va;

      //      printf("cpu=%d PC=0x%4x LD=0x%4x opcode=0x%4x\n"
      //	     ,rstf_instrT_get_cpuid(&(rp->instr))
      //	     , PC, address, inst);

      return;
    }

    // Fill Buffer
    I(buf_pos == buf_end);

    buf_end = rz->decompress(buf, Max_Num_Recs);
    if (buf_end == 0) {
      end_of_trace = true;
    }

  }while(!end_of_trace);

}

void RSTReader::fillTraceEntry(TraceEntry *te, int fid) {
  I(fid == 0);

  if(end_of_trace) {
    te->eot = true;
    return;
  }
  
  te->rawInst = getCurrentInst(fid);
  te->iAddr   = getCurrentPC(fid);
  
  te->dAddr   = getCurrentDataAddress(fid);
  
  advancePC(fid);

  te->nextIAddr = getCurrentPC(fid);
}
