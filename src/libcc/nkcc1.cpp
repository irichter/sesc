/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by James Tuck

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
 
/*
 * This launches the whole SESC simulator environment
 */
  
#include <stdlib.h>

#include <vector>

#include "nanassert.h"

#include "ReportGen.h"
#include "SescConf.h"
#include "ProtocolBase.h"
#include "InterConn.h"
#include "SlaveMPCoh.h"
#include "MasterMPCoh.h"
#include "CMemorySystem.h"
#include "callback.h"

#include "Processor.h"
#include "OSSim.h"
#include "SescConf.h"

#include "NetAddrMap.h"
#ifdef TASKSCALAR
#include "TaskHandler.h"
#endif

int main(int argc, char **argv, char **envp)
{ 
#ifdef TASKSCALAR
  taskHandler = new TaskHandler();
#endif

  osSim = new OSSim(argc, argv, envp);

  int nProcs = SescConf->getRecordSize("","cpucore");
  int nRouters = 0;
  InterConnection *net = NULL;
  
  if (SescConf->checkCharPtr("","gNetwork")) {
    const char *gNetwork = SescConf->getCharPtr("","gNetwork");

    I(gNetwork);
    
    NetAddrMap::InitMap("AdvMemMap");
    
    net = new InterConnection(gNetwork);
    
    nRouters = net->getnRouters();
  }

  // Begin Processor memory buildup
  std::vector<CMemorySystem *> ms(nProcs);
  std::vector<GProcessor *>    pr(nProcs);

  for(int i = 0; i < nProcs; i++) {
    CMemorySystem *gms = new CMemorySystem(i, net);
    gms->buildMemorySystem();
    ms[i] = gms;
    pr[i] = new Processor(ms[i],i); 
  }

  std::vector<MasterMPCoh *>  mpcoh(nRouters);

  // Coherence buildup
  if (nRouters) {
    for(int i = 0; i < nRouters; i++) {
      char temp[25];
      sprintf(temp,"ccmaster%d",i);
      mpcoh[i] = new MasterMPCoh(ms[0], net, i, 
				 strdup("CacheCoherenceMaster"),
				 strdup(temp));
    }
  }

  osSim->boot();
  
  for(int i = 0; i < nProcs; i++) { 
    delete ms[i];
    delete pr[i];
  }

  if (nRouters) {
    for(int i = 0; i < nRouters; i++) {
      delete mpcoh[i];
    }
  }

  delete net;

  delete osSim;

  return 0;
}
