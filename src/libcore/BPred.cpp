/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Smruti Sarangi

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

#include <string.h>
#include <strings.h>

#include "ReportGen.h"
#include "BPred.h"
#include "OSSim.h"
#include "SescConf.h"

#ifdef TASKSCALAR
#include "TaskContext.h"
#endif

/*****************************************
 * BPred
 */

BPred::BPred(int i, const char *s, const char *n)
  :id(i)
   ,nHit("BPred(%d)_%s:nHit",i,n)
   ,nMiss("BPred(%d)_%s:nMiss",i,n)
{
  if (strstr(n,"RAS") || strstr(n,"BTB"))
    return; // RAS and BTB have their own energy counters (do not
	    // replicate counters)

  char cadena[100];
  
  sprintf(cadena, "BPred(%d)_%s",i,n);
  bpredEnergy = new GStatsEnergy("bpredEnergy", cadena, i, BPredEnergy, EnergyMgr::get("bpredEnergy",i));
  
  // Done
}

BPred::~BPred()
{
}

/*****************************************
 * RAS
 */
BPRas::BPRas(int i, const char *section)
  :BPred(i,section,"RAS")
   ,RasSize(SescConf->getLong(section,"rasSize"))
{
  char cadena[100];

  sprintf(cadena, "BPred(%d)_RAS", i);
  rasEnergy = new GStatsEnergy("rasEnergy", cadena, i, RasEnergy,EnergyMgr::get("rasEnergy",i));

  // Constraints
  SescConf->isLong(section, "rasSize");
  SescConf->isBetween(section, "rasSize", 0, 128);    // More than 128???

  if(RasSize == 0) {
    stack = 0;
    return;
  }

  stack = new InstID[RasSize];
  I(stack);

  index = 0;
}

BPRas::~BPRas()
{
  delete stack;
}

PredType BPRas::predict(const Instruction *inst, InstID oracleID, bool doUpdate)
{
  // RAS is a little bit different than other predictors because it can update
  // the state without knowing the oracleID. All the other predictors update the
  // statistics when the branch is resolved. RAS automatically updates the
  // tables when predict is called. The update only actualizes the statistics.

  if(inst->isFuncRet()) {
    rasEnergy->inc();
    if(stack == 0)
      return CorrectPrediction;

    I(oracleID);
    if (doUpdate) {
      index--;
      if( index < 0 )
	index = RasSize-1;
    }

    if( stack[index] == oracleID )
      return CorrectPrediction;

    return MissPrediction;
  } else if(inst->isFuncCall() && stack) {
    rasEnergy->inc();

    if (doUpdate) {
      stack[index] = inst->calcNextInstID();
      index++;

      if( index >= RasSize )
	index = 0;
    }
  }

  return NoPrediction;
}

void BPRas::switchIn(Pid_t pid)
{
  // TODO: a task spawn passes the RAS. Copy it the first time (keep
  // it as processor migrates?)
}

void BPRas::switchOut(Pid_t pid)
{
  // read BPRas::switchIn
}

/*****************************************
 * BTB
 */
BPBTB::BPBTB(int i, const char *section, const char *name)
  :BPred(i,section, name ? name : "BTB")
{
  char cadena[100];
  
  sprintf(cadena, "BPred(%d)_%s",i, name ? name : "BTB");
  btbEnergy = new GStatsEnergy("btbEnergy", cadena,i,BTBEnergy,EnergyMgr::get("btbEnergy",i));

  if( SescConf->getLong(section,"btbSize") == 0 ) {
    // Oracle
    data = 0;
    return;
  }

  data = BTBCache::create(section,"btb","BPred_BTB(%d):",i);
  I(data);
}

BPBTB::~BPBTB()
{
  if( data )
    data->destroy();
}

void BPBTB::updateOnly(const Instruction *inst, InstID oracleID)
{
  if( data == 0 )
    return;

  bool ntaken = inst->calcNextInstID() == oracleID;
  ulong key   = calcInstID(inst);
  
  // Update only in taken branches
  if( ntaken )
    return;

  BTBCache::CacheLine *cl = data->fillLine(key);
  I( cl );
  
  cl->inst = oracleID;
}

PredType BPBTB::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bool ntaken = inst->calcNextInstID() == oracleID;

  btbEnergy->inc();

  if (data == 0) {
    // required when BPOracle
    nHit.cinc(doUpdate);

    I(oracleID);
    if (ntaken) {
      // Trash result because it shouldn't have reach BTB. Otherwise, the
      // worse the predictor, the better the results.
      return NoBTBPrediction;
    }
    return CorrectPrediction;
  }

  ulong key = calcInstID(inst);

  if ( ntaken || !doUpdate ) {
    // The branch is not taken. Do not update the cache
    BTBCache::CacheLine *cl = data->readLine(key);

    if( cl == 0 ) {
      nMiss.cinc(doUpdate);
      return NoBTBPrediction; // NoBTBPrediction because BTAC would hide the prediction
    }

    if( cl->inst == oracleID ) {
      nHit.cinc(doUpdate);
      return CorrectPrediction;
    }

    nMiss.cinc(doUpdate);
    return NoBTBPrediction;
  }

  I(doUpdate);

  // The branch is taken. Update the cache

  BTBCache::CacheLine *cl = data->fillLine(key);
  I( cl );
  
  InstID predictID = cl->inst;
  cl->inst = oracleID;

  if( predictID == oracleID ) {
    nHit.inc();
    return CorrectPrediction;
  }

  nMiss.inc();
  return NoBTBPrediction;
}


void BPBTB::switchIn(Pid_t pid)
{
  // Nothing to back (history does not work after all)
}

void BPBTB::switchOut(Pid_t pid)
{
}

/*****************************************
 * BPOracle
 */

PredType BPOracle::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  if( inst->calcNextInstID() == oracleID )
    return CorrectPrediction; //NT
  
  return btb.predict(inst, oracleID, doUpdate);
}

void BPOracle::switchIn(Pid_t pid)
{
  // Oracle, do nothing
}

void BPOracle::switchOut(Pid_t pid)
{
}

/*****************************************
 * BPTaken
 */

PredType BPTaken::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  if( inst->calcNextInstID() == oracleID )
    return MissPrediction;

  return btb.predict(inst, oracleID, doUpdate);
}

void BPTaken::switchIn(Pid_t pid)
{
  // Static prediction. Do nothing
}

void BPTaken::switchOut(Pid_t pid)
{
}

/*****************************************
 * BPNotTaken
 */

PredType  BPNotTaken::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  return inst->calcNextInstID() == oracleID ? CorrectPrediction : MissPrediction;
}

void BPNotTaken::switchIn(Pid_t pid)
{
  // Static prediction. Do nothing
}

void BPNotTaken::switchOut(Pid_t pid)
{
}

/*****************************************
 * BPStatic
 */

PredType BPStatic::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  bool ptaken = inst->guessAsTaken();

  bool taken = inst->calcNextInstID() != oracleID;
  
  if( taken != ptaken) {
    if (doUpdate)
      btb.updateOnly(inst, oracleID);
    return MissPrediction;
  }

  return ptaken ? btb.predict(inst, oracleID, doUpdate) : CorrectPrediction;
}

void BPStatic::switchIn(Pid_t pid)
{
  // Static prediction. Do nothing
}

void BPStatic::switchOut(Pid_t pid)
{
}

/*****************************************
 * BP2bit
 */

BP2bit::BP2bit(int i, const char *section)
  :BPred(i,section,"2bit")
  ,btb(i,  section)
  ,table(i,section
	 ,SescConf->getLong(section,"size")
	 ,SescConf->getLong(section,"bits"))
{
  // Constraints
  SescConf->isLong(section, "size");
  SescConf->isPower2(section, "size");
  SescConf->isGT(section, "size", 1);

  SescConf->isBetween(section, "bits", 1, 7);

  // Done
}

PredType BP2bit::predict(const Instruction *inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  if( inst->isBranchTaken() )
    return btb.predict(inst, oracleID, doUpdate);

  bool taken = (inst->calcNextInstID() != oracleID);

  bool ptaken;
  if (doUpdate)
    ptaken = table.predict(calcInstID(inst), taken);
  else
    ptaken = table.predict(calcInstID(inst));

  if( taken != ptaken ) {
    if (doUpdate)
      btb.updateOnly(inst,oracleID);
    return MissPrediction;
  }
  
  return ptaken ? btb.predict(inst, oracleID, doUpdate) : CorrectPrediction;
}

void BP2bit::switchIn(Pid_t pid)
{
  // No history to backup (local predictor use no history)
}

void BP2bit::switchOut(Pid_t pid)
{
}


/*****************************************
 * BP2level
 */

BP2level::BP2level(int i, const char *section)
  :BPred(i,section,"2level")
   ,btb(i, section)
   ,l1Size(SescConf->getLong(section,"l1Size"))
   ,l1SizeMask(l1Size - 1)
   ,historySize(SescConf->getLong(section,"historySize"))
   ,historyMask((1 << historySize) - 1)
   ,globalTable(i,section
		,SescConf->getLong(section,"l2Size")
		,SescConf->getLong(section,"l2Bits"))
{
  // Constraints
  SescConf->isLong(section, "l1Size");
  SescConf->isPower2(section, "l1Size");
  SescConf->isBetween(section, "l1Size", 0, 32768);

  SescConf->isLong(section, "historySize");
  SescConf->isBetween(section, "historySize", 1, 63);

  SescConf->isLong(section, "l2Size");
  SescConf->isPower2(section, "l2Size");
  SescConf->isBetween(section, "l2Bits", 1, 7);

  I((l1Size & (l1Size - 1)) == 0); 

  historyTable = new HistoryType[l1Size];
  I(historyTable);
}

BP2level::~BP2level()
{
  delete historyTable;
}

PredType BP2level::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  if( inst->isBranchTaken() )
    return btb.predict(inst, oracleID, doUpdate);

  bool taken = (inst->calcNextInstID() != oracleID);
  HistoryType iID     = calcInstID(inst);
  ushort      l1Index = iID & l1SizeMask;
  HistoryType l2Index = historyTable[l1Index];

  // update historyTable statistics
  if (doUpdate)
    historyTable[l1Index] = ((l2Index << 1) | ((iID>>2 & 1)^(taken?1:0))) & historyMask;
  
  // calculate Table possition
  l2Index = ((l2Index ^ iID) & historyMask ) | (iID<<historySize);

  bool ptaken;
  if (doUpdate)
    ptaken = globalTable.predict(l2Index, taken);
  else
    ptaken = globalTable.predict(l2Index);

  if( taken != ptaken) {
    if (doUpdate)
      btb.updateOnly(inst,oracleID);
    return MissPrediction;
  }

  return ptaken ? btb.predict(inst, oracleID, doUpdate) : CorrectPrediction;
}

void BP2level::switchIn(Pid_t pid)
{
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(pid);
  if (tc==0 || tc->getPid()==-1)
    return;

  historyTable[0] = tc->getBPHistory(historyTable[0]);
#endif
}

void BP2level::switchOut(Pid_t pid)
{
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(pid);
  if (tc==0 || tc->getPid()==-1)
    return;

  tc->setBPHistory(historyTable[0]);
#endif
}

/*****************************************
 * BPHybid
 */

BPHybrid::BPHybrid(int i, const char *section)
  :BPred(i,section,"Hybrid")
  ,btb(i, section)
   ,historySize(SescConf->getLong(section,"historySize"))
   ,historyMask((1 << historySize) - 1)
   ,globalTable(i,section
		,SescConf->getLong(section,"l2Size")
		,SescConf->getLong(section,"l2Bits"))
   ,localTable(i,section
	       ,SescConf->getLong(section,"localSize")
	       ,SescConf->getLong(section,"localBits"))
   ,metaTable(i,section
	      ,SescConf->getLong(section,"MetaSize")
	      ,SescConf->getLong(section,"MetaBits"))

{
  // Constraints
  SescConf->isLong(section,    "localSize");
  SescConf->isPower2(section,  "localSize");
  SescConf->isBetween(section, "localBits", 1, 7);

  SescConf->isLong(section    , "MetaSize");
  SescConf->isPower2(section  , "MetaSize");
  SescConf->isBetween(section , "MetaBits", 1, 7);

  SescConf->isLong(section,    "historySize");
  SescConf->isBetween(section, "historySize", 1, 63);

  SescConf->isLong(section,   "l2Size");
  SescConf->isPower2(section, "l2Size");
  SescConf->isBetween(section,"l2Bits", 1, 7);
}

BPHybrid::~BPHybrid()
{
}

PredType BPHybrid::predict(const Instruction *inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  if( inst->isBranchTaken() )
    return btb.predict(inst, oracleID, doUpdate);

  bool taken = (inst->calcNextInstID() != oracleID);
  HistoryType iID     = calcInstID(inst);
  HistoryType l2Index = ghr;

  // update historyTable statistics
  if (doUpdate) {
    ghr = ((ghr << 1) | ((iID>>2 & 1)^(taken?1:0))) & historyMask;
  }

  // calculate Table possition
  l2Index = ((l2Index ^ iID) & historyMask ) | (iID<<historySize);

  bool globalTaken;
  bool localTaken;
  if (doUpdate) {
    globalTaken = globalTable.predict(l2Index, taken);
    localTaken  = localTable.predict(iID, taken);
  }else{
    globalTaken = globalTable.predict(l2Index);
    localTaken  = localTable.predict(iID);
  }

  bool metaOut;
  if (!doUpdate) {
    metaOut = metaTable.predict(l2Index); // do not update meta
  }else if( globalTaken == taken && localTaken != taken) {
    // global is correct, local incorrect
    metaOut = metaTable.predict(l2Index, false);
  }else if( globalTaken != taken && localTaken == taken) {
    // global is incorrect, local correct
    metaOut = metaTable.predict(l2Index, true);
  }else{
    metaOut = metaTable.predict(l2Index); // do not update meta
    globalTable.predict(l2Index, taken);
  }

  bool ptaken = metaOut ? localTaken : globalTaken;

  if (taken != ptaken) {
    if (doUpdate)
      btb.updateOnly(inst,oracleID);
    return MissPrediction;
  }

  return ptaken ? btb.predict(inst, oracleID, doUpdate) : CorrectPrediction;
}

void BPHybrid::switchIn(Pid_t pid)
{
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(pid);
  if (tc==0 || tc->getPid()==-1)
    return;

  historyTable[0] = tc->getBPHistory(historyTable[0]);
#endif
}

void BPHybrid::switchOut(Pid_t pid)
{
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(pid);
  if (tc==0 || tc->getPid()==-1)
    return;

  tc->setBPHistory(historyTable[0]);
#endif
}

/*****************************************
 * 2BcgSkew 
 *
 * Based on:
 *
 * "De-aliased Hybird Branch Predictors" from A. Seznec and P. Michaud
 *
 * "Design Tradeoffs for the Alpha EV8 Conditional Branch Predictor"
 * A. Seznec, S. Felix, V. Krishnan, Y. Sazeides
 */

BP2BcgSkew::BP2BcgSkew(int i, const char *section)
  : BPred(i,section,"2BcgSkew")
  ,btb(i, section)
  ,BIM(i,section,SescConf->getLong(section,"BIMSize"))
  ,G0(i,section,SescConf->getLong(section,"G0Size"))
  ,G0HistorySize(SescConf->getLong(section,"G0HistorySize"))
  ,G0HistoryMask((1 << G0HistorySize) - 1)
  ,G1(i,section,SescConf->getLong(section,"G1Size"))
  ,G1HistorySize(SescConf->getLong(section,"G1HistorySize"))
  ,G1HistoryMask((1 << G1HistorySize) - 1)
  ,metaTable(i,section,SescConf->getLong(section,"MetaSize"))
  ,MetaHistorySize(SescConf->getLong(section,"MetaHistorySize"))
  ,MetaHistoryMask((1 << MetaHistorySize) - 1)
{
  // Constraints
  SescConf->isLong(section    , "BIMSize");
  SescConf->isPower2(section  , "BIMSize");
  SescConf->isGT(section      , "BIMSize", 1);

  SescConf->isLong(section    , "G0Size");
  SescConf->isPower2(section  , "G0Size");
  SescConf->isGT(section      , "G0Size", 1);

  SescConf->isLong(section    , "G0HistorySize");
  SescConf->isBetween(section , "G0HistorySize", 1, 63);

  SescConf->isLong(section    , "G1Size");
  SescConf->isPower2(section  , "G1Size");
  SescConf->isGT(section      , "G1Size", 1);

  SescConf->isLong(section    , "G1HistorySize");
  SescConf->isBetween(section , "G1HistorySize", 1, 63);

  SescConf->isLong(section    , "MetaSize");
  SescConf->isPower2(section  , "MetaSize");
  SescConf->isGT(section      , "MetaSize", 1);

  SescConf->isLong(section    , "MetaHistorySize");
  SescConf->isBetween(section , "MetaHistorySize", 1, 63);

  history = 0x55555555;
}

BP2BcgSkew::~BP2BcgSkew()
{
  // Nothing?
}


PredType BP2BcgSkew::predict(const Instruction * inst, InstID oracleID, bool doUpdate)
{
  bpredEnergy->inc();

  if (inst->isBranchTaken())
    return btb.predict(inst, oracleID, doUpdate);

  HistoryType iID = calcInstID(inst);
  
  bool taken = (inst->calcNextInstID() != oracleID);

  HistoryType xorKey1    = history^iID;
  HistoryType xorKey2    = history^(iID>>2);
  HistoryType xorKey3    = history^(iID>>4);

  HistoryType metaIndex = (xorKey1 & MetaHistoryMask) | iID<<MetaHistorySize;
  HistoryType G0Index   = (xorKey2 & G0HistoryMask)   | iID<<G0HistorySize;
  HistoryType G1Index   = (xorKey3 & G1HistoryMask)   | iID<<G1HistorySize;

  bool metaOut = metaTable.predict(metaIndex);

  bool BIMOut   = BIM.predict(iID);
  bool G0Out    = G0.predict(G0Index);
  bool G1Out    = G1.predict(G1Index);

  bool gskewOut = (G0Out?1:0) + (G1Out?1:0) + (BIMOut?1:0) >= 2;

  bool ptaken = metaOut ? BIMOut : gskewOut;

  if (ptaken != taken) {
    if (!doUpdate)
      return MissPrediction;
    
    BIM.predict(iID,taken);
    G0.predict(G0Index,taken);
    G1.predict(G1Index,taken);

    BIMOut   = BIM.predict(iID);
    G0Out    = G0.predict(G0Index);
    G1Out    = G1.predict(G1Index);
      
    gskewOut = (G0Out?1:0) + (G1Out?1:0) + (BIMOut?1:0) >= 2;
    if (BIMOut != gskewOut)
      metaTable.predict(metaIndex,(BIMOut == taken));
    else
      metaTable.reset(metaIndex,(BIMOut == taken));
    
    I(doUpdate);
    btb.updateOnly(inst, oracleID);
    return MissPrediction;
  }

  if (doUpdate) {
    if (metaOut) {
      BIM.predict(iID,taken);
    }else{
      if (BIMOut == taken)
	BIM.predict(iID,taken);
      if (G0Out == taken)
	G0.predict(G0Index,taken);
      if (G1Out == taken)
	G1.predict(G1Index,taken);
    }
    
    if (BIMOut != gskewOut)
      metaTable.predict(metaIndex,(BIMOut == taken));
    
    history = history<<1 | ((iID>>2 & 1)^(taken?1:0));
  }

  return ptaken ? btb.predict(inst, oracleID, doUpdate) : CorrectPrediction;
}

void BP2BcgSkew::switchIn(Pid_t pid)
{
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(pid);
  if (tc==0 || tc->getPid()==-1)
    return;

  history = tc->getBPHistory(history);
#endif
}

void BP2BcgSkew::switchOut(Pid_t pid)
{
#ifdef TASKSCALAR
  TaskContext *tc = TaskContext::getTaskContext(pid);
  if (tc==0 || tc->getPid()==-1)
    return;

  tc->setBPHistory(history);
#endif
}

/*****************************************
 * YAGS
 * 
 * Based on:
 *
 * "The YAGS Brnach Prediction Scheme" by A. N. Eden and T. Mudge
 *
 * Arguments to the predictor:
 *     type    = "yags"
 *     l1size  = (in power of 2) Taken Cache Size.
 *     l2size  = (in power of 2) Not-Taken Cache Size.
 *     l1bits  = Number of bits for Cache Taken Table counter (2).
 *     l2bits  = Number of bits for Cache NotTaken Table counter (2).
 *     size    = (in power of 2) Size of the Choice predictor.
 *     bits    = Number of bits for Choice predictor Table counter (2).
 *     tagbits = Number of bits used for storing the address in
 *               direction cache.
 *
 * Description:
 *
 * This predictor tries to address the conflict aliasing in the choice
 * predictor by having two direction caches. Depending on the
 * prediction, the address is looked up in the opposite direction and if
 * there is a cache hit then that predictor is used otherwise the choice
 * predictor is used. The choice predictor and the direction predictor
 * used are updated based on the outcome.
 *
 */

BPyags::BPyags(int i, const char *section)
  :BPred(i,section,"yags")
  ,btb(i,  section)
  ,historySize(24)
  ,historyMask((1 << 24) - 1)
  ,table(i,section
           ,SescConf->getLong(section,"size")
           ,SescConf->getLong(section,"bits"))
  ,ctableTaken(i,section
                ,SescConf->getLong(section,"l1size")
                ,SescConf->getLong(section,"l1bits"))
  ,ctableNotTaken(i,section
                   ,SescConf->getLong(section,"l2size")
                   ,SescConf->getLong(section,"l2bits"))
{
  // Constraints
  SescConf->isLong(section, "size");
  SescConf->isPower2(section, "size");
  SescConf->isGT(section, "size", 1);

  SescConf->isBetween(section, "bits", 1, 7);

  SescConf->isLong(section, "l1size");
  SescConf->isPower2(section, "l1bits");
  SescConf->isGT(section, "l1size", 1);

  SescConf->isBetween(section, "l1bits", 1, 7);

  SescConf->isLong(section, "l2size");
  SescConf->isPower2(section, "l2bits");
  SescConf->isGT(section, "size", 1);

  SescConf->isBetween(section, "l2bits", 1, 7);

  SescConf->isBetween(section, "tagbits", 1, 7);

  CacheTaken = new uchar[SescConf->getLong(section,"l1size")];
  CacheTakenMask = SescConf->getLong(section,"l1size") - 1;
  CacheTakenTagMask = (1 << SescConf->getLong(section,"tagbits")) - 1;

  CacheNotTaken = new uchar[SescConf->getLong(section,"l2size")];
  CacheNotTakenMask = SescConf->getLong(section,"l2size") - 1;
  CacheNotTakenTagMask = (1 << SescConf->getLong(section,"tagbits")) - 1;

  // Done
}

BPyags::~BPyags()
{

}

PredType BPyags::predict(const Instruction *inst, InstID oracleID,bool doUpdate)
{
  bpredEnergy->inc();

  if( inst->isBranchTaken() )
    return btb.predict(inst, oracleID, doUpdate);

  bool taken = (inst->calcNextInstID() != oracleID);
  HistoryType iID      = calcInstID(inst);
  HistoryType iIDHist  = ghr;


  bool choice;
  if (doUpdate) {
    ghr = ((ghr << 1) | ((iID>>2 & 1)^(taken?1:0))) & historyMask;
    choice = table.predict(iID, taken);
  }else
    choice = table.predict(iID);

  iIDHist = ((iIDHist ^ iID) & historyMask ) | (iID<<historySize);

  bool ptaken;
  if (choice) {
    ptaken = true;

    // Search the not taken cache. If we find an entry there, the
    // prediction from the cache table will override the choice table.

    HistoryType cacheIndex = iIDHist & CacheNotTakenMask;
    HistoryType tag        = iID & CacheNotTakenTagMask;
    bool cacheHit          = (CacheNotTaken[cacheIndex] == tag);

    if (cacheHit) {
      if (doUpdate) {
        CacheNotTaken[cacheIndex] = tag;
        ptaken = ctableNotTaken.predict(iIDHist, taken);
      } else {
        ptaken = ctableNotTaken.predict(iIDHist);
      }
    } else if ((doUpdate) && (taken == false)) {
      CacheNotTaken[cacheIndex] = tag;
      (void)ctableNotTaken.predict(iID, taken);
    }
  } else {
    ptaken = false;
    // Search the taken cache. If we find an entry there, the prediction
    // from the cache table will override the choice table.

    HistoryType cacheIndex = iIDHist & CacheTakenMask;
    HistoryType tag        = iID & CacheTakenTagMask;
    bool cacheHit          = (CacheTaken[cacheIndex] == tag);

    if (cacheHit) {
      if (doUpdate) {
        CacheTaken[cacheIndex] = tag;
        ptaken = ctableTaken.predict(iIDHist, taken);
      } else {
        ptaken = ctableTaken.predict(iIDHist);
      }
    } else if ((doUpdate) && (taken == true)) {
        CacheTaken[cacheIndex] = tag;
        (void)ctableTaken.predict(iIDHist, taken);
	ptaken = false;
    }
  }

  if( taken != ptaken ) {
    if (doUpdate)
      btb.updateOnly(inst,oracleID);
    return MissPrediction;
  }
  
  return ptaken ? btb.predict(inst, oracleID, doUpdate) : CorrectPrediction;
}

void BPyags::switchIn(Pid_t pid)
{

}

void BPyags::switchOut(Pid_t pid)
{

}



/*****************************************
 * BPredictor
 */


BPred *BPredictor::getBPred(int id, const char *sec)
{
  BPred *pred;
  
  const char *type = SescConf->getCharPtr(sec, "type");

  // Normal Predictor
  if (strcasecmp(type, "oracle") == 0) {
    pred = new BPOracle(id, sec);
  } else if (strcasecmp(type, "NotTaken") == 0) {
    pred = new BPNotTaken(id, sec);
  } else if (strcasecmp(type, "Taken") == 0) {
    pred = new BPTaken(id, sec);
  } else if (strcasecmp(type, "2bit") == 0) {
    pred = new BP2bit(id, sec);
  } else if (strcasecmp(type, "2level") == 0) {
    pred = new BP2level(id, sec);
  } else if (strcasecmp(type, "Static") == 0) {
    pred = new BPStatic(id, sec);
  } else if (strcasecmp(type, "2BcgSkew") == 0) {
    pred = new BP2BcgSkew(id, sec);
  } else if (strcasecmp(type, "Hybrid") == 0) {
    pred = new BPHybrid(id, sec);
  } else if (strcasecmp(type, "yags") == 0) {
    pred = new BPyags(id, sec);
  } else {
    MSG("BPredictor::BPredictor Invalid branch predictor type [%s] in section [%s]", type,sec);
    exit(0);
  }
  I(pred);

  return pred;
}

BPredictor::BPredictor(int i, const char *sec, BPredictor *bpred)
  :id(i)
   ,SMTcopy(bpred == 0 ? false : true)
   ,ras(i, sec)
   ,nBranches("BPred(%d):nBranches", i)
   ,nTaken("BPred(%d):nTaken", i)
   ,nMiss("BPred(%d):nMiss", i)
   ,section(strdup(sec ? sec : "null" ))
{
  // Threads in SMT system share the predictor. Only the Ras is
  // duplicated

  if (bpred)
    pred = bpred->pred;
  else
    pred = getBPred(id, section);
    
}

BPredictor::~BPredictor()
{
  dump(section);

  if (!SMTcopy)
    delete pred;
    
  free(section);
}

void BPredictor::dump(const char *str) const
{
  // nothing?
}
