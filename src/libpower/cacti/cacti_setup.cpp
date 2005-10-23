/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2005 University of California, Santa Cruz

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

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "nanassert.h"
#include "Snippets.h"
#include "SescConf.h"

#include "xcacti_def.h"
#include "xcacti_area.h"


/*------------------------------------------------------------------------------*/
#include <vector>

static double tech;
static int res_memport;
static double wattch2cactiFactor = 1;

double getEnergy(int size
                 ,int bsize
                 ,int assoc
                 ,int rdPorts
                 ,int wrPorts
                 ,int subBanks
                 ,int useTag
                 ,int bits);
double getEnergy(const char*);


extern "C" void output_data(result_type *result, arearesult_type *arearesult,
                            area_type *arearesult_subbanked, 
                            parameter_type *parameters, double *NSubbanks);


void iterate();

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
    fprintf(stderr,"no clusters\n");
    exit(-1);
  }

  return total/num;
}

void iterate()
{
  std::vector<char *> sections;
  std::vector<char *>::iterator it; 

  SescConf->getAllSections(sections) ;

  char line[100] ;
  for(it = sections.begin();it != sections.end(); it++) {
    const char *block = *it;

    if (!SescConf->checkCharPtr(block,"deviceType")) 
      continue;

    const char *name = SescConf->getCharPtr(block,"deviceType") ;

    if (strcasecmp(name,"niceCache") == 0) {
      // No energy for ideal caches (DRAM bank)
      SescConf->updateRecord(block, "RdHitEnergy"   ,0);
      SescConf->updateRecord(block, "RdMissEnergy"  ,0);
      SescConf->updateRecord(block, "WrHitEnergy"   ,0);
      SescConf->updateRecord(block, "WrMissEnergy"  ,0);
      
    }else if(strstr(name,"cache") 
             || strstr(name,"tlb")
             || strstr(name,"mem")
             || strstr(name,"dir") 
             || !strcmp(name,"revLVIDTable") ) {
      double eng = wattch2cactiFactor * getEnergy(block);
      
      // write it
      SescConf->updateRecord(block, "RdHitEnergy"   ,eng);
      SescConf->updateRecord(block, "RdMissEnergy"  ,eng * 2); // Rd miss + lineFill
      SescConf->updateRecord(block, "WrHitEnergy"   ,eng);
      SescConf->updateRecord(block, "WrMissEnergy"  ,eng * 2); // Wr miss + lineFill
    }
  }
}

char * strfy(int v){
  char *t = new char[10] ;
  sprintf(t,"%d",v);
  return t ;
}
char *strfy(double v){
  char *t = new char[10] ;
  sprintf(t,"%lf",v);
  return t ;
}


double getEnergy(int size
                 ,int bsize
                 ,int assoc
                 ,int rdPorts
                 ,int wrPorts
                 ,int subBanks
                 ,int useTag
                 ,int bits)
{
  int nsets = size/(bsize*assoc);

  if (nsets == 0) {
    printf("Invalid cache parameters\n");
    exit(0);
  }
  if (subBanks == 0) {
    printf("Invalid cache subbanks parameters\n");
    exit(0);
  }

  if ((size/subBanks)<64) {
    printf("size %d: subBanks %d: assoc %d : nset %d\n",size,subBanks,assoc,nsets);
    size =64*subBanks;
  }

  if (rdPorts>1) {
    wrPorts = rdPorts-2;
    rdPorts = 2;
  }

  BITOUT = bits;

  parameter_type parameters;

  if (size == bsize * assoc) {
    parameters.fully_assoc = 1;
  }else{
    parameters.fully_assoc = 0;
  }

  size = roundUpPower2(size);

  if (parameters.fully_assoc) {
    parameters.associativity = size/bsize;
  }else{
    parameters.associativity = assoc;
  }

  parameters.latchsa    = 1;
  parameters.ignore_tag = !useTag;
  parameters.cache_size = size;
  parameters.block_size = bsize;
  parameters.num_readwrite_ports = 0;
  parameters.num_read_ports = rdPorts;
  parameters.num_write_ports = wrPorts;
  parameters.num_single_ended_read_ports =0;
  parameters.number_of_sets = size/(bsize*assoc);
  parameters.fudgefactor = .8/tech;   
  parameters.tech_size=tech;
  parameters.NSubbanks = subBanks;

  if (!xcacti_parameter_check(&parameters)) {
    xcacti_parameters_dump(&parameters);
    exit(0);
  }

  xcacti_parameter_adjust(&parameters);

  area_type arearesult_subbanked;
  arearesult_type arearesult;
  result_type result;

  xcacti_calculate_time(&parameters,&result,&arearesult,&arearesult_subbanked);

#ifdef DEBUG
  xcacti_output_data(&result,&arearesult,&arearesult_subbanked,&parameters);
#endif

  return 1e9*(result.total_power_without_routing/subBanks + result.total_routing_power);
}

double getEnergy(const char *section)
{
  // set the input
  int cache_size = SescConf->getLong(section,"size") ;
  int block_size = SescConf->getLong(section,"bsize") ;
  int assoc = SescConf->getLong(section,"assoc") ;
  int write_ports = 0 ;
  int read_ports = SescConf->getLong(section,"numPorts");
  int readwrite_ports = 1;
  int subbanks = 1;
  int bits = 32;

  if(SescConf->checkLong(section,"subBanks"))
    subbanks = SescConf->getLong(section,"subBanks");

  if(SescConf->checkLong(section,"bits"))
    bits = SescConf->getLong(section,"bits");

  printf("Module [%s]...\n", section);

  return getEnergy(cache_size
                   ,block_size
                   ,assoc
                   ,read_ports
                   ,readwrite_ports
                   ,subbanks
                   ,1
                   ,bits);
}

void processorCore()
{
  const char *proc = SescConf->getCharPtr("","cpucore",0) ;
  fprintf(stderr,"proc = [%s]\n",proc);

  //----------------------------------------------
  // Register File
  int issueWidth= SescConf->getLong(proc,"issueWidth");
  int size    = SescConf->getLong(proc,"intRegs");
  int banks   = 1; 
  int rdPorts = 2*issueWidth;
  int wrPorts = issueWidth;
  int bits = 32;
  int bytes = 8;

  if(SescConf->checkLong(proc,"bits")) {
    bits = SescConf->getLong(proc,"bits");
    bytes = bits/8;
    if (bits*8 != bytes) {
      fprintf(stderr,"Not valid number of bits for the processor core [%d]\n",bits);
      exit(-2);
    }
  }

  if(SescConf->checkLong(proc,"intRegBanks"))
    banks = SescConf->getLong(proc,"intRegBanks");

  if(SescConf->checkLong(proc,"intRegRdPorts"))
    rdPorts = SescConf->getLong(proc,"intRegRdPorts");

  if(SescConf->checkLong(proc,"intRegWrPorts"))
    wrPorts = SescConf->getLong(proc,"intRegWrPorts");

  double regEnergy = getEnergy(size*bytes,bytes,1,rdPorts,wrPorts,banks,0,bits);

  printf("\nRegister [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*bytes, banks, rdPorts+wrPorts, regEnergy);

  SescConf->updateRecord(proc,"wrRegEnergy",regEnergy);
  SescConf->updateRecord(proc,"rdRegEnergy",regEnergy);

  //----------------------------------------------
  // Load/Store Queue
  size      = SescConf->getLong(proc,"maxLoads");
  banks     = 1; 
  rdPorts   = res_memport;
  wrPorts   = res_memport;

  if(SescConf->checkLong(proc,"lsqBanks"))
    banks = SescConf->getLong(proc,"lsqBanks");

  regEnergy = getEnergy(size*2*bytes,2*bytes,size,rdPorts,wrPorts,banks,1, 2*bits);

  printf("\nLoad Queue [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*2*bytes, banks, 2*res_memport, regEnergy);

  SescConf->updateRecord(proc,"ldqRdWrEnergy",regEnergy);

  size      =  SescConf->getLong(proc,"maxStores");
 
  regEnergy = getEnergy(size*4*bytes,4*bytes,size,rdPorts,wrPorts,banks,1, 2*bits);

  printf("\nStore Queue [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*4*bytes, banks, 2*res_memport, regEnergy);

  SescConf->updateRecord(proc,"stqRdWrEnergy",regEnergy);

#ifdef SESC_INORDER 
  size      =  size/4;
 
  regEnergy = getEnergy(size*4*bytes,4*bytes,size,rdPorts,wrPorts,banks,1, 2*bits);

  printf("\nStore Inorder Queue [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*4*bytes, banks, 2*res_memport, regEnergy);

  SescConf->updateRecord(proc,"stqRdWrEnergyInOrder",regEnergy);
 #endif 
 
  //----------------------------------------------
  // Reorder Buffer
  size      = SescConf->getLong(proc,"robSize");
  banks     = size/64;
  if (banks == 0) {
    banks = 1;
  }else{
    banks = roundUpPower2(banks);
  }
  
  // Retirement should hit another bank
  rdPorts   = 1; // continuous possitions
  wrPorts   = 1;

  regEnergy = getEnergy(size*2,2*issueWidth,1,rdPorts,wrPorts,banks,0,16*issueWidth);

  printf("\nROB [%d bytes] banks[%d] ports[%d] Energy[%g]\n",size*2, banks, 2*rdPorts, regEnergy);

  SescConf->updateRecord(proc,"robEnergy",regEnergy);

  //----------------------------------------------
  // Rename Table
  {
    double bitsPerEntry = log(SescConf->getLong(proc,"intRegs"))/log(2);
    
    size      = roundUpPower2(static_cast<unsigned int>(32*bitsPerEntry/8));
    banks     = 1;
    rdPorts   = 2*issueWidth;
    wrPorts   = issueWidth;

    regEnergy = getEnergy(size,1,1,rdPorts,wrPorts,banks,0,1);

    printf("\nrename [%d bytes] banks[%d] Energy[%g]\n",size, banks, regEnergy);
    
    SescConf->updateRecord(proc,"renameEnergy",regEnergy);
  }

  //----------------------------------------------
  // Window Energy & Window + DDIS

  {
    int min = SescConf->getRecordMin(proc,"cluster") ;
    int max = SescConf->getRecordMax(proc,"cluster") ;
    I(min==0);

    for(int i = min ; i <= max ; i++) {
      const char *cluster = SescConf->getCharPtr(proc,"cluster",i) ;

      
      bool useSEED = false; 
      if(SescConf->checkLong(cluster,"depTableNumPorts"))
        useSEED = SescConf->getLong(cluster,"depTableNumPorts") != 0;

      if (!useSEED) {
#if 0
        // TRADITIONAL COLLAPSING ISSUE LOGIC

        // Keep SescConf->updateRecord(proc,"windowCheckEnergy",0);
        // Keep SescConf->updateRecord(proc,"windowSelEnergy" ,0);

        // Recalculate windowRdWrEnergy

        size      = SescConf->getLong(cluster,"winSize");
        banks     = 1;
        rdPorts   = SescConf->getLong(cluster,"wakeUpNumPorts");
        wrPorts   = issueWidth;
        int robSize          = SescConf->getLong(proc,"robSize");
        float entryBits = 4*(log(robSize)/log(2)); // src1, src2, dest, instID
        entryBits += 7; // opcode
        entryBits += 1; // ready bit
        
        int tableBits = static_cast<int>(entryBits * size);
        int tableBytes;
        if (tableBits < 8) {
          tableBits  = 8;
          tableBytes = 1;
        }else{
          tableBytes = tableBits/8;
        }
        int assoc= roundUpPower2(static_cast<unsigned int>(entryBits/8));

        regEnergy = getEnergy(tableBytes,assoc,assoc,rdPorts,wrPorts,banks,1,static_cast<int>(entryBits));
        
        printf("\nWindow [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
               ,tableBytes, banks, rdPorts+wrPorts, regEnergy);
        
        SescConf->updateRecord(proc,"windowRdWrEnergy" ,regEnergy);
#endif

        SescConf->updateRecord(proc,"depTableEnergy",0);
      }else{
        // SEED ISSUE LOGIC

        // RAT has register and token
        {
          double bitsPerEntry = 2*log(SescConf->getLong(proc,"intRegs"))/log(2);
    
          size      = roundUpPower2(static_cast<unsigned int>(32*bitsPerEntry/8));
          banks     = 1; 
          rdPorts   = 2*issueWidth;
          wrPorts   = issueWidth;
          
          regEnergy = getEnergy(size,1,1,rdPorts,wrPorts,banks,0,1);
          
          SescConf->updateRecord(proc,"renameEnergy",regEnergy);
        }

        SescConf->updateRecord(proc,"windowCheckEnergy",0);
        SescConf->updateRecord(proc,"windowRdWrEnergy" ,0);
        SescConf->updateRecord(proc,"windowSelEnergy" ,0);

        //----------------------------------------------
        // DepTable
        int robSize          = SescConf->getLong(proc,"robSize");
        size                 = SescConf->getLong(cluster,"winSize");
        banks                = roundUpPower2(SescConf->getLong(cluster,"banks"));
        int depTableEntries  = SescConf->getLong(cluster,"depTableEntries");
        rdPorts   = SescConf->getLong(cluster,"depTableNumPorts");
        wrPorts   = 0;
        float entryBits = 4*(log(robSize)/log(2)); // src1, src2, dest, instID
        entryBits += 7 ; // opcode
        entryBits += log(depTableEntries)/log(2); // use pos
        entryBits += 1; // speculative bit
        
        int tableBits = static_cast<int>(entryBits * depTableEntries + log(robSize)/log(2)); // + BBid
        int tableBytes;
        if (tableBits < 8) {
          tableBits  = 8;
          tableBytes = 1;
        }else{
          tableBytes = tableBits/8;
        }

        regEnergy = getEnergy(tableBytes*size,tableBytes+1,1,rdPorts,wrPorts,banks,0,tableBits);

        printf("\ndepTable [%d bytes] [bytes read %d] [bits per entry %d] size[%d] Energy[%g] ports[%d]\n"
               ,size*tableBytes,tableBytes,tableBits/depTableEntries, size, regEnergy, wrPorts+ rdPorts);
        
        SescConf->updateRecord(proc,"depTableEnergy",regEnergy);
      }
    }
  }
}

void cacti_setup()
{
  const char *technology = SescConf->getCharPtr("","technology");
  fprintf(stderr,"technology = [%s]\n",technology);
  tech = SescConf->getLong(technology,"tech");
  fprintf(stderr, "tech : %9.0fnm\n" , tech);
  tech /= 1000;

  const char *proc    = SescConf->getCharPtr("","cpucore",0);
  const char *l1Cache = SescConf->getCharPtr(proc,"dataSource");

  const char *l1CacheSpace = strstr(l1Cache," ");
  char *l1Section = strdup(l1Cache);
  if (l1CacheSpace)
    l1Section[l1CacheSpace - l1Cache] = 0;

  res_memport = SescConf->getLong(l1Section,"numPorts");

  double l1Energy = getEnergy(l1Section);

  double WattchL1Energy = SescConf->getDouble("","wattchDataCacheEnergy");

  if (WattchL1Energy) {
    wattch2cactiFactor = WattchL1Energy/l1Energy;
    fprintf(stderr,"wattch2cacti Factor %g\n", wattch2cactiFactor);
  }else{
    fprintf(stderr,"-----WARNING: No wattch correction factor\n");
  }

  processorCore();

  iterate();
}
