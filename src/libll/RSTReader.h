/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2006 University California, Santa Cruz.

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


#ifndef RST_READER_H
#define RST_READER_H

#include "ThreadContext.h"
#include "minidecoder.h"
#include "TraceEntry.h"
#include "TraceReader.h"

#include "rstf.h"
#include "Rstzip.h"

class RSTReader : public TraceReader {
 private:
  const int Max_Num_Recs;

  Rstzip *rz;
  
  // Current inst
  VAddr   PC;
  uint    inst;
  VAddr   address;

  // Decompressed buffer
  bool end_of_trace;
  int buf_pos;
  int buf_end;
  rstf_unionT *buf;
  
  void readInst(int fid);
  void advancePC(int fid);

  VAddr getCurrentPC(int fid)    const { return PC; }
  int   getCurrentInst(int fid)  const { return inst; }
  VAddr getCurrentDataAddress(int fid) const { return address; }

 public:
  rstf_unionT *currentInst() {
    return &buf[buf_pos];
  }
  RSTReader();

  void openTrace(const char* basename);
  void closeTrace();

  void fillTraceEntry(TraceEntry *te, int id);

};

#endif
