/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Luis Ceze

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

#ifndef PPCDECODER_H
#define PPCDECODER_H

#include "PPCOpcodes.h"

class PPCDecoder {
 private:
  static PPCInstDef **decodeTable;

 protected:

 public:
  static PPCInstDef ppcInstTable[];

  static void Initialize();
  static void expandDecodeEntry(PPCInstDef *, int extOpSize);
  
  static PPCInstDef* getInstDef(unsigned rawInst) {
    unsigned decodeEntry = PPC_OPC_MAIN(rawInst) * PPC_EXTOPS + PPC_OPC_EXT(rawInst);
    I(decodeEntry < PPC_OPS * PPC_EXTOPS * sizeof(PPCInstDef *));
    
    I(decodeTable[decodeEntry]);
    
    return decodeTable[decodeEntry];
  }
};

#endif
