/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of California Santa Cruz

   Contributed by Jose Renau

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

#ifndef DEPWINDOW_H
#define DEPWINDOW_H

#include "Instruction.h"
#include "EnergyMgr.h"
#include "nanassert.h"
#include "Port.h"
#include "Snippets.h"
#ifdef SESC_DDIS
#include "DDIS.h"
#endif

class DInst;
class GProcessor;

class DepWindow {
private:
  const int Id;

  const TimeDelta_t InterClusterLat;
#ifndef SESC_DDIS
  const TimeDelta_t WakeUpDelay;
#endif
  const TimeDelta_t SchedDelay;
  const TimeDelta_t RegFileDelay;

  const bool InOrderCore;

#ifdef SESC_DDIS
  DDIS ddis;
  static GStatsCntr *nDeps[];
#endif

  GStatsEnergy *resultBusEnergy;
  GStatsEnergy *forwardBusEnergy;

  GStatsEnergy *windowRdWrEnergy;  // read/write on an individual window entry
  GStatsEnergy *windowCheckEnergy; // Check for dependences on the window
  GStatsEnergy *windowSelEnergy;   // instruction selection

  DInst **RAT;

  PortGeneric *wakeUpPort;
  PortGeneric *schedPort;

protected:
public:
  ~DepWindow();
  DepWindow(GProcessor *gproc, const char *clusterName);

  void addInst(DInst *dinst);
  void wakeUpDeps(DInst *dinst);
  void select(DInst *dinst);
  void executed(DInst *dinst);

  bool canIssue(DInst *dinst) const;

  void retire(DInst *dinst) {
#ifdef SESC_DDIS
    ddis.retire(dinst);
#endif
  }
};

#endif // DEPWINDOW_H
