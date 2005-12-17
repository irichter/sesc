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

#include "icode.h"
#include "opcodes.h"
#include "globals.h"

#include "Instruction.h"
# include "QemuSescTrace.h"

void Instruction::initializeQemuSescTrace(int argc, char **argv, char **envp) {
  
}

void Instruction::QemuSparcDecodeInstruction(Instruction *inst, QemuSescTrace *qst)
{
  inst->src1 = static_cast <RegType> (qst->src1);
  inst->dest = static_cast <RegType> (qst->dest);
  inst->src2 = static_cast <RegType> (qst->src2);
  inst->uEvent = NoEvent;
  inst->condLikely = false;
  inst->guessTaken = true;   //FIXME
  inst->jumpLabel = false;
  inst->addr = qst->pc;

  inst->opcode = static_cast <InstType> (qst->type);
  inst->subCode = static_cast <InstSubType> (0); // FIXME
}
