/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

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

#ifndef CLUSTER_H
#define CLUSTER_H

#include "nanassert.h"

#include "DepWindow.h"
#include "GStats.h"
#include "Instruction.h"

class Resource;
class GMemorySystem;
class GStatsEnergyCGBase;
class GProcessor;

class Cluster {
 private:
  void buildUnit(const char *clusterName
                 ,GMemorySystem *ms
                 ,Cluster *cluster
                 ,InstType type
                 ,GStatsEnergyCGBase *ecgbase);

 protected:
  DepWindow window;

  const long MaxWinSize;
  long windowSize;

  GProcessor *const gproc;

  GStatsAvg winNotUsed;

  Resource   *res[MaxInstType];

 protected:
  void delEntry() {
    windowSize++;
    I(windowSize<=MaxWinSize);
  }
  
  virtual ~Cluster();
  Cluster(const char *clusterName, GProcessor *gp);

 public:
  void newEntry() {
    windowSize--;
    I(windowSize>=0);
  }
  bool canIssue(DInst *dinst) const { return windowSize>0; }

  void wakeUpDeps(DInst *dinst) { window.wakeUpDeps(dinst); }

  virtual void executed(DInst *dinst) = 0;
  virtual void retire(DInst *dinst) = 0;

  static Cluster *create(const char *clusterName, GMemorySystem *ms, GProcessor *gproc);

  Resource *getResource(InstType type) const {
    I(type < MaxInstType);
    return res[type];
  }

  void addInst(DInst *dinst);

#ifdef SESC_DDIS
  bool hasDepTableSpace(const DInst *dinst) const { return window.hasDepTableSpace(dinst); }
#endif

  GProcessor *getGProcessor() const { return gproc; }
};

class ExecutedCluster : public Cluster {
 public:
  virtual ~ExecutedCluster() {
  }
    
  ExecutedCluster(const char *clusterName, GProcessor *gp)
    : Cluster(clusterName, gp) { }
    
  void executed(DInst *dinst);
  void retire(DInst *dinst);
};

class RetiredCluster : public Cluster {
 public:
  virtual ~RetiredCluster() {
  }
  RetiredCluster(const char *clusterName, GProcessor *gp)
    : Cluster(clusterName, gp) { }

  void executed(DInst *dinst);
  void retire(DInst *dinst);
};


class ClusterManager {
 private:
  Resource   *res[MaxInstType];
 protected:
 public:
  ClusterManager(GMemorySystem *ms, GProcessor *gproc);

  Resource *getResource(InstType type) const {
    return res[type];
  }

#ifdef SESC_MISPATH
  void misBranchRestore();
#endif
};

#endif // CLUSTER_H
