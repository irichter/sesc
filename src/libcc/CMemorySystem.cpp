/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Karin Strauss

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

#include "CMemorySystem.h"
#include "SlaveMPCoh.h"
#include "MPCache.h"

#define k_mpcache      "mpcache"
#define k_slaveMPProt  "SlaveMPProt"

CMemorySystem::CMemorySystem(int processorId, InterConnection *net)
  : MemorySystem(processorId)
{
  network = net;
}

MemObj *CMemorySystem::buildMemoryObj(const char *type, 
				      const char *section, 
				      const char *name)
{
  MemObj *obj;

  if (!strcasecmp(type, k_mpcache)) {

    obj = new MPCache(this, section, name);

  } else if (!strcasecmp(type, k_slaveMPProt)){

    I(network);

    obj = new SlaveMPCoh(this, 
			 network,
			 pID/procsPerNode,
			 section,
			 name);
  } else {

    obj = MemorySystem::buildMemoryObj(type, section, name);

  }

  return obj;
}

