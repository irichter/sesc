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

#include <stdlib.h>

#include <vector>

// sesc internal stuff
#include "ReportGen.h"
#include "SescConf.h"
#include "callback.h"

// sesc OS model
#include "OSSim.h"

// hardware models
#include "Processor.h"
#include "DMemorySystem.h"

#include "NetAddrMap.h"

// debugging defines
#include "DSMDebug.h"

int main(int argc, char**argv, char **envp)
{
  osSim = new OSSim(argc, argv, envp);

  int nProcs = SescConf->getRecordSize("","cpucore");

  GLOG(DSMDBG_CONSTR, "Number of Processors: %d", nProcs);

  // processor and memory build
  std::vector<GProcessor *>    pr(nProcs);
  std::vector<GMemorySystem *> ms(nProcs);

  for(int i = 0; i < nProcs; i++) {
    GLOG(DSMDBG_CONSTR, "Building processor %d and its memory subsystem", i);
    GMemorySystem *gms = new DMemorySystem(i);
    gms->buildMemorySystem();
    ms[i] = gms;
    pr[i] = new Processor(ms[i],i);
  }

  NetAddrMap::InitMap("AdvMemMap");

  GLOG(DSMDBG_CONSTR, "I am booting now");
  osSim->boot();
  GLOG(DSMDBG_CONSTR, "Terminating simulation");

  for(int i = 0; i < nProcs; i++) {
    delete pr[i];
    delete ms[i];
  }
  
  delete osSim;

  return 0;
}