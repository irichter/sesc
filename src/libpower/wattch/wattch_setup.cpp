#include "setup.h"
#include "wattch_setup.h"
#include "machine.h"
#include "wattch_power.h"
#include "wattch_cache.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <SescConf.h>
#include <stdlib.h>
#include <math.h>

using namespace std;
/*********************************************************
 * configurations
 *********************************************************/

extern "C" void dump_power_stats(power_result_type *power);

double Mhz ;

int decode_stages;
int rename_stages;
int wakeup_stages;
int ruu_fetch_width;
int ruu_decode_width;
int ruu_issue_width;
int ruu_commit_width;
double areaFactor;
double dieLenght;
int RUU_size;
int REG_size;
int LSQ_size;
int data_width;
int res_ialu;
int res_fpalu;
int res_memport;
int ras_size ;
int rob_size ;
enum EPREDTYPE bp_type ;
extern double crossover_scaling;

struct cache_t *cache_dl1;
struct cache_t *cache_il1;
struct cache_t *cache_dl2;

struct cache_t *dtlb;
struct cache_t *itlb;

int bimod_config[1];
int twolev_config[4];
int comb_config[1];
int btb_config[2];

/**********************************************************/

void processAlu(const char* proc) ;
void processBranch(const char* proc) ;
void processLVIDTable();
void processCombTable(const char* proc);
int getInstQueueSize(const char* proc);

void setup()
{
  char c = 'l' ;

  Mhz = 1e9 ;

  ruu_decode_width = 4 ;
  ruu_issue_width = 4 ;
  ruu_commit_width = 4 ;
  RUU_size = 16 ;
  REG_size = 32 ; 
  LSQ_size = 32 ;
  data_width = 32 ;
  res_ialu = 4 ;
  res_fpalu = 4 ;
  res_memport = 2 ;
  ras_size = 8 ;
  rob_size = 32 ;
  bp_type = STATIC ;

  cache_dl2 = cache_create("dl2", 2048, 64, /* balloc */0,
				   /* usize */0, 8, cache_char2policy(c),
				   /* hit lat */10);

  
  cache_il1 = cache_create("il1", 128, 64, /* balloc */0,
			       /* usize */0, 2, cache_char2policy(c),
			       /* hit lat */2);

  
  
  dtlb = cache_create("dtlb", 64, 8, /* balloc */0,
			  /* usize */sizeof(md_addr_t), 64,
			  cache_char2policy(c), 
			  1);

  itlb = cache_create("itlb", 32, 8, /* balloc */0,
			  /* usize */sizeof(md_addr_t), 32,
			  cache_char2policy(c), 
			  /* hit latency */1);

  bimod_config[0] = 128 ;

  twolev_config[0] = 1 ;
  twolev_config[1] = 1024 ;
  twolev_config[2] = 8 ;
  twolev_config[3] = 0;

  comb_config[0] = 1024 ;

  btb_config[0] = 128 ;
  btb_config[1] = 4 ;

}

void wattch_setup()
{
  // set up the variables
  setup() ;

  power_result_type pp;
  
  const char *proc = SescConf->getCharPtr("cpucore","cpucore") ;
  cout << "proc = " << proc << endl ;
  Mhz = SescConf->getDouble(proc,"frequency") ;

  decode_stages = SescConf->getLong(proc,"decodeDelay");
  rename_stages = SescConf->getLong(proc,"renameDelay");
  wakeup_stages = SescConf->getLong(proc,"wakeupDelay");

  ruu_fetch_width  = SescConf->getLong(proc,"fetchWidth") ;
  ruu_decode_width = SescConf->getLong(proc,"issueWidth") ;
  ruu_issue_width  = SescConf->getLong(proc,"issueWidth") ;
  ruu_commit_width = SescConf->getLong(proc,"retireWidth") ;
  areaFactor       = SescConf->getDouble(proc,"areaFactor");

  double tech = SescConf->getDouble("","tech");

  dieLenght =.018*sqrt(areaFactor)*(tech/0.18);
  printf("areaFactor=%g estimated area = %g mm^2 at %.0fnm (per core)\n"
	 ,areaFactor
	 ,18*18*(tech/0.18)*(tech/0.18)*areaFactor
	 ,1000*tech);

  RUU_size = getInstQueueSize(proc);
  printf("RUU_size = %d cro = %g\n", RUU_size, crossover_scaling);
  
  // TODO: Currently, only IREgs. Add FPReg
  REG_size = SescConf->getLong(proc,"intRegs");
  LSQ_size = SescConf->getLong(proc,"maxLoads") + SescConf->getLong(proc,"maxStores") ;
  rob_size = SescConf->getLong(proc,"robSize") ;
  data_width = 32 ;
  const char *l1Cache = SescConf->getCharPtr(proc,"dataSource");
  const char *l1CacheSpace = strstr(l1Cache," ");
  char *l1Section = strdup(l1Cache);
  if (l1CacheSpace)
    l1Section[l1CacheSpace - l1Cache] = 0;
  
  int sets = SescConf->getLong(l1Section,"size");
  sets = sets / SescConf->getLong(l1Section,"bsize");
  sets = sets / SescConf->getLong(l1Section,"assoc");

  res_memport = SescConf->getLong(l1Section,"numPorts");
  cache_dl1 = cache_create("dl1"
			   ,sets
			   ,SescConf->getLong(l1Section,"bsize")
			   ,/* balloc */0
			   ,/* usize */0
			   ,SescConf->getLong(l1Section,"assoc")
			   ,cache_char2policy('l')
			   ,/* hit lat */3);


  printf("ALU Begin\n");
  processAlu(proc) ;
  printf("Branch Begin\n");
  processBranch(proc) ;

  if (SescConf->checkLong("TaskScalar", "VersionSize")) {
    printf("LVIDTable Begin\n");
    processLVIDTable();
    printf("Combine Table Begin\n");
    processCombTable(proc);
  }

  printf("Core Begin\n");
  // calculate the power
  calculate_power(&pp) ;

  // write the variables
  // output the energy in nano joules
  // compute the factor
  double dfac1 = Mhz/1e9 ;

  SescConf->updateRecord("","wattchDataCacheEnergy",pp.dcache_power/dfac1);
  
  double div = dfac1;
  SescConf->updateRecord(proc,"renameEnergy",pp.rename_power/div);

  // Originally, it was increasing linearly the energy per access with
  // the issue width. I do not think that this is right. It should
  // increase the result bus (done in resultbus), or the scheduler,
  // but not the arithmetic operation per se.
  div = dfac1 * res_ialu;
  SescConf->updateRecord(proc,"iALUEnergy",pp.ialu_power/div);

  div = dfac1 * res_fpalu;
  SescConf->updateRecord(proc,"fpALUEnergy",pp.falu_power/div);

  // Result/forward bus increase energy as width increases
  div = dfac1* ((double) ruu_issue_width);
  SescConf->updateRecord(proc,"resultBusEnergy",pp.resultbus/div) ;
  SescConf->updateRecord(proc,"forwardBusEnergy",pp.resultbus/div) ;
  
  // Instruction window energy increases as we increase the issue
  // width. Design something better if you do not like it
  div = dfac1 * 3; // * ((double) ruu_issue_width);
  SescConf->updateRecord(proc,"windowPregEnergy",pp.rs_power/div) ;

  div = dfac1; // * ((double) ruu_issue_width) ;
  SescConf->updateRecord(proc,"windowSelEnergy",pp.selection/div) ;
  SescConf->updateRecord(proc,"wakeupEnergy",pp.wakeup_power/div) ;

  // Assume that ROB is a single port structure. It reads the
  // instructions to retire in consecutive possition. So no need to
  // access ramdonly, and no need to have more ports.
  div = dfac1 * ((double) ruu_commit_width);
  SescConf->updateRecord(proc,"robEnergy",pp.reorder_power/div) ;

  // Conservatively assume that some "magical" structure would have
  // the same energy per access independent of the number of
  // ports. This magical structure may be possible because the LSQ is
  // a FIFO like structure. Therefore, we can "insert two consecutive"
  // elements at once. (Tough but possible).
  div = dfac1 * ((double) res_memport) ;
  SescConf->updateRecord(proc,"lsqWakeupEnergy",pp.lsq_wakeup_power/div) ;
  SescConf->updateRecord(proc,"lsqPregEnergy",pp.lsq_rs_power/div) ;
  
  // The more ports in the RF, the more the energy. Maybe some smart
  // designed can propose to have a single port multi-banked
  // structure. The truth is that most current processors increase the
  // number of ports as the issue with increases
  div = dfac1 * 3; // 3 = 2 RD + 1 WR
  SescConf->updateRecord(proc,"wrRegEnergy",pp.regfile_power/div);
  SescConf->updateRecord(proc,"rdRegEnergy",pp.regfile_power/div);

  div = 2.0 * dfac1;
  SescConf->updateRecord(proc,"bpredEnergy",pp.bpred_power/div) ;
  SescConf->updateRecord(proc,"btbEnergy",pp.btb/div) ;
  SescConf->updateRecord(proc,"rasEnergy",pp.ras/div) ;
  
  SescConf->updateRecord(proc,"clockEnergy",pp.clock_power/dfac1) ;
  SescConf->updateRecord(proc,"totEnergy",pp.total_power/dfac1) ;

  if (verbose)
    dump_power_stats(&pp);
}

void processAlu(const char* proc)
{
  // check
  res_ialu = -1 ;
  res_fpalu = -1 ;

  // get the clusters
  int min = SescConf->getRecordMin(proc,"cluster") ;
  int max = SescConf->getRecordMax(proc,"cluster") ;

  for(int i = min ; i <= max ; i++){
    const char* cc = SescConf->getCharPtr(proc,"cluster",i) ;
    if(SescConf->checkCharPtr(cc,"fpALUUnit")){
      const char *sec = SescConf->getCharPtr(cc,"fpALUUnit") ;
      res_fpalu = (int)SescConf->getLong(sec,"Num") ;
    }
    if(SescConf->checkCharPtr(cc,"iALUUnit")){
      const char *sec = SescConf->getCharPtr(cc,"iALUUnit") ;
      res_ialu = (int)SescConf->getLong(sec,"Num") ;
    }
  }

  // check
  if(res_ialu == -1) {
    cout << "ialu not set" << endl ;
    exit(-1) ;
  }
  if(res_fpalu == -1){
    cout << "fpalu not set" << endl ;
    exit(-1) ;
  }
}

int getInstQueueSize(const char* proc)
{
   // get the clusters
  int min = SescConf->getRecordMin(proc,"cluster") ;
  int max = SescConf->getRecordMax(proc,"cluster") ;
  int total = 0;
  int num = 0;
  
  for(int i = min ; i <= max ; i++){
    const char* cc = SescConf->getCharPtr(proc,"cluster",i) ;
    if(SescConf->checkLong(cc,"winSize")){
      int sz = SescConf->getLong(cc,"winSize") ;
      total += sz;
      num++;
    }
  }

  // check
  if(!num){
    cout << "No clusters" << endl;
    exit(-1);
  }

  return total/num;
}

void processBranch(const char* proc)
{
  // get the branch
  const char* bpred = SescConf->getCharPtr(proc,"bpred") ;

  // get the type
  const char* type = SescConf->getCharPtr(bpred,"type") ;

  // switch based on the type
  EPREDTYPE bpType = OTHER ;
  if(!strcmp(type,"Taken") || !strcmp(type,"NotTaken") || 
      !strcmp(type,"Static"))
  bp_type = STATIC ;

  if(!strcmp(type,"Oracle")) bp_type = ORACLE ;
  if(!strcmp(type,"2bit")) bp_type = BIMOD ;
  if(!strcmp(type,"2level")) bp_type = TWOLEV ;
  if(!strcmp(type,"hybrid")) bp_type = COMB ;

  if(bp_type != ORACLE){
    ras_size = (int) SescConf->getLong(bpred,"rasSize") ;
    int btbSize = (int)SescConf->getLong(bpred,"btbSize") ;
    int btbAssoc = (int)SescConf->getLong(bpred,"btbAssoc") ;
    btb_config[0] = btbSize/btbAssoc ;
    btb_config[1] = btbAssoc ;
  }
  
  if(bp_type == BIMOD){
    bimod_config[0] = (int) SescConf->getLong(bpred,"size") ;    
  }
  if((bp_type == TWOLEV) || (bp_type == COMB)){
    int l1size = (int) SescConf->getLong(bpred,"l1size") ;
    int l2size = (int) SescConf->getLong(bpred,"l2size") ;
    int historySize = (int) SescConf->getLong(bpred,"historySize") ;
    bool xorflag = false ;
    if(SescConf->checkBool(bpred,"xor"))
      xorflag = (bool) SescConf->getBool(bpred,"xor") ;
    
    twolev_config[0] = l1size ;
    twolev_config[1] = l2size ;
    twolev_config[2] = historySize ;
    twolev_config[3] = (int) xorflag ;
  }
  if(bp_type == COMB){
    bimod_config[0] = (int) SescConf->getLong(bpred,"localSize") ;
    comb_config[0] = (int) SescConf->getLong(bpred,"Metasize") ;
  }
  if(bp_type == OTHER){
    cout << "branch predictor type " << bpred << " not recognized" << endl ;
    abort() ;
  }
}
int getLog(int val){
  double v = (double) val;
  double l2 = log(v)/log(2.0);
  l2 = ceil(l2);
  return (int)l2;
}

int getLVIDBlkSize(const char* section)
{
  int gtid = SescConf->getLong("TaskScalar","VersionSize");
  int lidoff = getLog(SescConf->getLong(section,"nSubLVID"));
  int statoff = 2;
  int nextfree = getLog(SescConf->getLong(section,"nLVID"));

  int mm = SescConf->getLong("TaskScalar","MLThreshold");
  int tpointer = getLog(mm);

  int size = SescConf->getLong(section,"size");
  int assoc = SescConf->getLong(section,"assoc");
  int bsize = SescConf->getLong(section,"bsize");
  int lines = size/(assoc * bsize);
  int llines = getLog(lines);

  return (gtid+lidoff+statoff+nextfree+tpointer+llines);
}

void processLVIDTable()
{
  vector<char *> sections;
  vector<char *>::iterator it; 
  SescConf->getAllSections(sections) ;

  for(it = sections.begin();it != sections.end();it++) {
    char *block = *it ;

    if (!SescConf->checkCharPtr(block,"deviceType")) 
      continue;

    const char *name = SescConf->getCharPtr(block,"deviceType") ;
   
    if(!strcasecmp(name,"mvcache") || !strcasecmp(name,"dirmvcache")) {
      int rows = SescConf->getLong(block,"nLVID");
      int cols = getLVIDBlkSize(block);
      int ports = SescConf->getLong(block,"LVIDTablePort");
      double eng = simple_array_power(rows,cols,ports-1,1,0);

      // write it
      double dfac1 = Mhz/1e9;
      double div = dfac1 * ((double) ports);
      SescConf->updateRecord(block,"rdLVIDEnergy",eng*crossover_scaling/div) ;
      SescConf->updateRecord(block,"wrLVIDEnergy",eng*crossover_scaling/div) ;
    }
  }
}

void processCombTable(const char* proc)
{
   const char *section = SescConf->getCharPtr(proc,"dataSource");
   char *tok = strdup(section);
   const char *tok1 = strtok(tok," ");
   int bsize = SescConf->getLong(tok1,"bsize");
   
   double eng = simple_array_power(4,bsize,1,1,0);
   double div = Mhz/1e9;
   SescConf->updateRecord("miscEnergy","combWriteEnergy",eng*crossover_scaling/div);
 }
