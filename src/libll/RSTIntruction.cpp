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

#include "icode.h"
#include "opcodes.h"
#include "globals.h"

#include "Instruction.h"
#include "SPARCInstruction.h"

void Instruction::RSTDecodeInstruction(Instruction *inst, uint rawInst)
{
  InstType     type;
  InstSubType  subType;
  unsigned int rd;
  unsigned int rs1;
  unsigned int rs2;

  disas_sparc_insn(rawInst, type, subType, rd, rs1, rs2);

  inst->src1 = static_cast<RegType>(rs1);
  inst->src2 = static_cast<RegType>(rs2);
  inst->dest = static_cast<RegType>(rd);
  inst->uEvent = NoEvent;
  inst->condLikely = false;
  inst->guessTaken = true;
  inst->jumpLabel = false;   //FIXME

  inst->opcode = type;
  inst->subCode = subType;
}
