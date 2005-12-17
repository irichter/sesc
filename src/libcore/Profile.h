/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Wei Liu

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

#ifndef PROFILE_H 
#define PROFILE_H

#include <set>

#include "estl.h"
#include "nanassert.h"
#include "GStats.h"
#include "ProcessId.h"
#include "CacheCore.h"
#include "ThreadContext.h"

#undef LOCAL_WRITE
#define WRITE_REVISE

class TaskContext;

enum PCacheState {CLEAN, DIRTY};

class ProfCache {

private:
  class PCState : public StateGeneric<PAddr> {
  public:
    PCacheState s;

    PCState(PCacheState is=CLEAN) {
      s = is;
    }
    bool operator==(PCState a) const {
      return s == a.s;
    }
  };

  typedef CacheGeneric<PCState,PAddr> PCacheType;
  PCacheType *pcache;

  int nRdHit;
  int nRdMiss;
  int nWrHit;
  int nWrMiss;
  
public:
  ProfCache();
  ~ProfCache();

  bool read(PAddr addr);
  bool write(PAddr addr);

  int getNRdHit()  const { return nRdHit; }
  int getNRdMiss() const { return nRdMiss;}
  int getNWrHit()  const { return nWrHit; }
  int getNWrMiss() const { return nWrMiss;}
};

enum Note_t {
  NORMAL=0, //normal selected tasks
  DPHASE1_SMALL,        //deleted in phase 1 because of small task size
  DPHASE1_SHORTHOIST,   //deleted in phase 1 because of short hoisting distance
  DPHASE1_HUGEHOIST,    //deleted in phase 1 because of too long hoisting distance
  DPHASE2 =11,
  PREFETCH=999, //prefetch tasks
} ;

class Profile : public GStats {
private:
  class TaskInfo {
  public:
    int  predecessor;

    bool eliminated;            //mark it if it is going to be eliminated
    int  nViolations;           //number of violations happened
    long long currHoist;

	 int startAddr;
    int  nExec;                 //how many times it gets executed
    int  nSpawn;                //how many spawns inside it
    long long nInst;
    long long nStaticHoist;     //how far does it get hoisted statically
    long long nDynHoist;        //how far does it get hoisted dynamically
    
    int spawnedBy;              //parent task ID
    long long spawnPos;         //where it gets spawned
    long long startPos;         //where it starts

    Time_t beginTime;           //virtual start time
    Time_t seqBeginTime;        //virtual start time for sequential run

	 int nRdHit;
	 int nRdMiss;
	 int nWrHit;
	 int nWrMiss;
	 int nUncountRdMiss;        //Misses between two squashes
	 int nUncountWrMiss;

    Note_t note;                //Note for each task

    TaskInfo() {
      predecessor = 0;

      eliminated = false;
      nViolations = 0;
      currHoist = 0;

      startAddr = 0;
      nExec = 0;
      nSpawn = 0;
      nInst = 0;
      nStaticHoist = 0;
      nDynHoist = 0;

      spawnedBy = 0;
      spawnPos = 0;
      startPos = 0;
      beginTime = 0;
      seqBeginTime = 0;

      nRdHit = 0;
      nRdMiss = 0;
      nWrHit = 0;
      nWrMiss = 0;

      nUncountRdMiss = 0;
      nUncountWrMiss = 0;

      note = NORMAL;
    }
  };

  class SpawnInfo {
  public:
    int taskID;
    long long instPos; //spawn position
    Time_t vTime;      //virtual time
  };
  SLIST<SpawnInfo> spawns;
  typedef HASH_MAP<int,TaskInfo> TaskInfoType;
  static TaskInfoType tasks;

  class WriteInfo {
  public:
    long long instPos;
    icode_ptr picode;

    int taskID;       //which task it belongs to
    Time_t vTime;     //record the virtual time because the future 
                      //task may have the same task id, and the beginTime
                      //of each task may change. The writeTime caculation is
                      //not correct, because it depends on the beginTime of
                      //each task
    WriteInfo() {
      instPos = 0;
      picode = NULL;
      taskID = -1;
      vTime = 0;
    }
  };
  typedef HASH_MAP<VAddr, WriteInfo>  WriteInfoType;
  static WriteInfoType writes;
#ifdef LOCAL_WRITE  
  typedef set<VAddr> AddrSet;
  static AddrSet localWriteAddrs;
#endif  
#ifdef WRITE_REVISE
  typedef SLIST<WriteInfo *> WriteList;
  static WriteList localWrites;
#endif
  
  int   taskSizeThrd;
  int   staticHoistThrd;
  int   maxHoistThrd;
  int   dynamicHoistThrd;
  float violationThrd;
  int   spawnOverhead;
  float l2MissOccThrd;
  float sHoistOccThrd;
  float dHoistOccThrd;
  float extraWorkRate;
  int   latReadMiss;
  int   latReadHit;
  int   latWriteMiss;
  int   latWriteHit;

  long long startInst;
  long long stopInst;

  long long lastRecInst;
  Time_t currTime;      //Virtual time, TLS execution time
  Time_t maxCurrTime;

  int currTaskID;

  bool started;

  ProfCache profCache;

  void Profile::readParameters(const char *);

public:
  Profile();
  virtual ~Profile();

  void reportValue() const;
  bool notStart() {
    return !started;
  }

  void updateTime();

  void recStartInst();
  void recStopInst();

  void recSpawn(int pid);
  void recCommit(int pid, int tid);
  void recWrite(VAddr vaddr, icode_ptr picode, bool silent);
  void recRead(VAddr vaddr, icode_ptr picode);

  void recInitial(int pid);     //record first task
  void recTermination(int pid); //record last task

  void mergeProfFile(const char *dstFile, char *srcFile1, char *srcFile2) const;

}; 

#endif // PROFILE_H 
