/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela

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

#include "SescConf.h"
#include "MemorySystem.h"
#include "Bus.h"

Bus::Bus(MemorySystem* current
	 ,const char *section
	 ,const char *name)
  : MemObj(section, name)
    ,delay(SescConf->getLong(section, "delay"))
{
  MemObj *lower_level = NULL;

  SescConf->isLong(section, "numPorts");
  SescConf->isLong(section, "portOccp");
  SescConf->isLong(section, "delay");

  NumUnits_t  num = SescConf->getLong(section, "numPorts");
  TimeDelta_t occ = SescConf->getLong(section, "portOccp");

  char cadena[100];
  sprintf(cadena,"Data%s", name);
  dataPort = PortGeneric::create(cadena, num, occ);
  sprintf(cadena,"Cmd%s", name);
  cmdPort  = PortGeneric::create(cadena, num, 1);

  I(current);
  lower_level = current->declareMemoryObj(section, k_lowerLevel);   
  if (lower_level != NULL)
    addLowerLevel(lower_level);
}

void Bus::access(MemRequest *mreq)
{
  if (mreq->getMemOperation() == MemWrite) {
    mreq->goDownAbs(dataPort->nextSlot()+delay, lowerLevel[0]);
  }else{
    mreq->goDownAbs(cmdPort->nextSlot(), lowerLevel[0]);
  }
}

void Bus::returnAccess(MemRequest *mreq)
{
  if (mreq->getMemOperation() == MemWrite) {
    mreq->goUpAbs(cmdPort->nextSlot());
  }else{
    mreq->goUpAbs(dataPort->nextSlot()+delay);
  }
}

bool Bus::canAcceptStore(PAddr addr) const 
{
  return true;
}

void Bus::invalidate(PAddr addr,ushort size,MemObj *oc)
{ 
  invUpperLevel(addr,size,oc); 
}

Time_t Bus::getNextFreeCycle() const
{ 
  return cmdPort->calcNextSlot();
}
