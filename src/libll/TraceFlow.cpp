/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Luis Ceze
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

#include "TraceFlow.h"
#include "OSSim.h"
#include "SescConf.h"
#include "TT6Reader.h"
#include "QemuSescReader.h"
#ifdef SESC_SIMICS
#include "SimicsReader.h"
#endif

char *TraceFlow::traceFile = 0;
TraceReader *TraceFlow::trace = 0;
TraceFlow::TraceFlow(int cId, int i, GMemorySystem *gms) 
  : GFlow(i, cId, gms)
{
#if (defined(TASKSCALAR) || defined(SESC_MISPATH))
  MSG("TraceFlow::TASKSCALAR or SESC_MISPATH not supported yet");
  exit(-5);
#endif
  bool createReader = (!trace);
  // all traceflow instances share the same reader obj
  
  const char *traceMode = SescConf->getCharPtr("","traceMode");
  
  if(strcasecmp(traceMode, "ppctt6") == 0) {
    if(createReader)
      trace = new TT6Reader();
    mode = PPCTT6;
  }else if (strcmp (traceMode,"qemusparc")==0) {
    if (createReader) {
      trace = new QemuSescReader(); 
      MSG("Creating object");
    }
    mode = QemuSp;
  } else if(strcmp(traceMode, "simics") == 0) {

#ifdef SESC_SIMICS
    if(createReader)
      trace = new SimicsReader();
#else
    MSG("Simics mode not fully supported yet. Sorry.");
    exit(0);
#endif

    mode = Simics;
  } else {
      I(0);
  }
  
  if(createReader) {
    I(traceFile);
    MSG("TraceFlow::TraceFlow() traceFile=%s", traceFile);
    trace->openTrace(traceFile);
  }
  
  nextPC = 0;
  hasTrace = true;
}

DInst *TraceFlow::executePC() 
{ 
  I(hasTrace);

  TraceEntry te = trace->getTraceEntry(0);   
	
  if(te.eot) { // end of trace
    hasTrace = false; // FIXME: remove 
    //return te;
  }

  if(te.stallEntry) //  
    return 0;

  const Instruction *inst;

  if(mode == PPCTT6) {
    inst = Instruction::getPPCInstByPC(te.iAddr, te.rawInst);    
  }else if (mode == QemuSp) {   
    inst = Instruction::getSescInstByPC(te.iAddr, static_cast<QemuSescReader *>(trace)->qst);  
  } else {                           
    I(mode == Simics);
#ifdef SESC_SIMICS
    inst = Instruction::getSimicsInst((TraceSimicsOpc_t) te.rawInst);
#else
    inst = 0; //avoiding warning
#endif
  }

  nextPC = te.nextIAddr;   

  return DInst::createDInst(inst, te.dAddr, cpuId);   
}

void TraceFlow::dump(const char *str) const
{
  MSG("TraceFlow::dump() not implemented. stop being lazy and do it!");
}

