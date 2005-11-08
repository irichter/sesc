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

#ifdef SESC_THERM
#include "ThermTrace.h"

ThermTrace  *sescTherm=0;
#endif


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
                 ,int bits
                 ,xcacti_flp *xflp
                 );
double getEnergy(const char *,xcacti_flp *);


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

#ifdef SESC_THERM
static void update_layout_bank(const char *blockName, xcacti_flp *xflp, const ThermTrace::FLPUnit *flp) {

  double dx;
  double dy;
  double a;
  //    +------------------------------+
  //    |         bank ctrl            |
  //    +------------------------------+
  //    |  tag   | de |  data          |
  //    | array  | co |  array         |
  //    |        | de |                |
  //    +------------------------------+
  //    | tag_ctrl | data_ctrl         |
  //    +------------------------------+

  const char *flpSec = SescConf->getCharPtr("","floorplan");
  size_t max = SescConf->getRecordMax(flpSec,"blockDescr");
  max++;
  
  char cadena[1024];

  //--------------------------------
  // Find top block bank_ctrl
  dy = flp->delta_y*(xflp->bank_ctrl_a/xflp->total_a);

  if (xflp->bank_ctrl_e) {
    // Only if bankCtrl consumes energy
    sprintf(cadena, "%sBankCtrl %g %g %g %g", blockName, flp->delta_x, dy, flp->x, flp->y);
    SescConf->updateRecord(flpSec, "blockDescr", strdup(cadena) , max);
    max++;
    sprintf(cadena,"%sBankCtrlEnergy", blockName);
    SescConf->updateRecord(flpSec, "blockMatch", strdup(cadena) , max);
    max++;
  }

  double tag_start_y = dy + flp->y;
  
  //--------------------------------
  // Find lower blocks tag_ctrl and data_ctrl
  dy = flp->delta_y*((3*xflp->tag_ctrl_a+3*xflp->data_ctrl_a)/xflp->total_a);

  a  = xflp->tag_ctrl_a+xflp->data_ctrl_a;
  dx = flp->delta_x*(xflp->tag_ctrl_a/a);

  double tag_end_y = flp->y+flp->delta_y-dy;
  if (xflp->tag_array_e) {
    // Only if tag consumes energy
    sprintf(cadena,"%sTagCtrl   %g %g %g %g", blockName, dx/3, dy, flp->x+dx/3, tag_end_y);
    SescConf->updateRecord(flpSec, "blockDescr", strdup(cadena) , max);
    max++;
    sprintf(cadena,"%sTagCtrlEnergy", blockName);
    SescConf->updateRecord(flpSec, "blockMatch", strdup(cadena) , max);
    max++;
  }

  sprintf(cadena,"%sDataCtrl  %g %g %g %g", blockName, (flp->delta_x-dx)/3, dy, flp->x+dx+(flp->delta_x-dx)/3, tag_end_y);
  SescConf->updateRecord(flpSec, "blockDescr", strdup(cadena) , max);
  max++;
  sprintf(cadena,"%sDataCtrlEnergy", blockName);
  SescConf->updateRecord(flpSec, "blockMatch", strdup(cadena) , max);
  max++;

  //--------------------------------
  // Find middle blocks tag array, decode, and data array
  a  = xflp->tag_array_a+xflp->data_array_a+xflp->decode_a;
  dy = tag_end_y - tag_start_y;

  if (xflp->tag_array_e) {
    dx = flp->delta_x*(xflp->tag_array_a/a);
    sprintf(cadena, "%sTagArray  %g %g %g %g", blockName, dx, dy, flp->x, tag_start_y);
    SescConf->updateRecord(flpSec, "blockDescr", strdup(cadena) , max);
    max++;
    sprintf(cadena,"%sTagArrayEnergy", blockName);
    SescConf->updateRecord(flpSec, "blockMatch", strdup(cadena) , max);
    max++;
  }

  double x = flp->x + dx;
  dx = flp->delta_x*((xflp->decode_a)/a);
  sprintf(cadena, "%sDecode    %g %g %g %g", blockName, dx, dy, x, tag_start_y);
  SescConf->updateRecord(flpSec, "blockDescr", strdup(cadena) , max);
  max++;
  sprintf(cadena,"%sDecodeEnergy", blockName);
  SescConf->updateRecord(flpSec, "blockMatch", strdup(cadena) , max);
  max++;

  dx = flp->delta_x*((xflp->data_array_a)/a);
  sprintf(cadena, "%sDataArray %g %g %g %g", blockName, dx, dy, flp->x+flp->delta_x-dx, tag_start_y);
  SescConf->updateRecord(flpSec, "blockDescr", strdup(cadena) , max);
  max++;
  sprintf(cadena,"%sDataArrayEnergy", blockName);
  SescConf->updateRecord(flpSec, "blockMatch", strdup(cadena) , max);
  max++;

}

static void update_sublayout(const char *blockName, xcacti_flp *xflp, const ThermTrace::FLPUnit *flp, int id) {

  if(id==1) {
    update_layout_bank(blockName,xflp,flp);
  }else if ((id % 2) == 0) {
    // even number
    ThermTrace::FLPUnit flp1 = *flp;
    ThermTrace::FLPUnit flp2 = *flp;
    if (flp->delta_x > flp->delta_y) {
      // x-axe is bigger
      flp1.delta_x = flp->delta_x/2;

      flp2.delta_x = flp->delta_x/2;
      flp2.x       = flp->x+flp->delta_x/2;
    }else{
      // y-axe is bigger
      flp1.delta_y = flp->delta_x/2;

      flp2.delta_y = flp->delta_y/2;
      flp2.y       = flp->y + flp->delta_y/2;
    }
    update_sublayout(blockName, xflp, &flp1, id/2);
    update_sublayout(blockName, xflp, &flp2, id/2);
  }else{
    MSG("Invalid number of banks to partition. Please use power of two");
    exit(-1);
    I(0); // In
  }
}
#endif

void update_layout(const char *blockName, xcacti_flp *xflp) {

#ifdef SESC_THERM
  const ThermTrace::FLPUnit *flp = sescTherm->findBlock(blockName);
  if (flp == 0) {
    MSG("Error: blockName[%s] not found in blockDescr",blockName);
    exit(-1);
    return; // no match found
  }

  update_sublayout(blockName, xflp, flp, xflp->NSubbanks*xflp->assoc);
#endif  
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

    if(strcasecmp(name,"vbus")==0){
      SescConf->updateRecord(block,"busEnergy",0.0) ; // FIXME: compute BUS energy
    }else if (strcasecmp(name,"niceCache") == 0) {
      // No energy for ideal caches (DRAM bank)
      SescConf->updateRecord(block, "RdHitEnergy"   ,0.0);
      SescConf->updateRecord(block, "RdMissEnergy"  ,0.0);
      SescConf->updateRecord(block, "WrHitEnergy"   ,0.0);
      SescConf->updateRecord(block, "WrMissEnergy"  ,0.0);
      
    }else if(strstr(name,"cache") 
             || strstr(name,"tlb")
             || strstr(name,"mem")
             || strstr(name,"dir") 
             || !strcmp(name,"revLVIDTable") ) {

      xcacti_flp xflp;
      double eng = getEnergy(block, &xflp);

      if (SescConf->checkCharPtr(block,"blockName")) {
        const char *blockName = SescConf->getCharPtr(block,"blockName");
        update_layout(blockName, &xflp);
      }
      
#ifdef SESC_THERM2
      // FIXME: partition energies per structure
#else
      // write it
      SescConf->updateRecord(block, "RdHitEnergy"   ,eng);
      SescConf->updateRecord(block, "RdMissEnergy"  ,eng * 2); // Rd miss + lineFill
      SescConf->updateRecord(block, "WrHitEnergy"   ,eng);
      SescConf->updateRecord(block, "WrMissEnergy"  ,eng * 2); // Wr miss + lineFill
#endif
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
                 ,int bits
                 ,xcacti_flp *xflp
                 ) {
  int nsets = size/(bsize*assoc);

  if (nsets == 0) {
    printf("Invalid cache parameters size[%d], bsize[%d], assoc[%d]\n", size, bsize, assoc);
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
  parameters.number_of_sets = size/(bsize*parameters.associativity);
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
  xcacti_power_flp(&result,&arearesult,&arearesult_subbanked,&parameters, xflp);

  return wattch2cactiFactor * 1e9*(result.total_power_without_routing/subBanks + result.total_routing_power);
}


double getEnergy(const char *section, xcacti_flp *xflp) {
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
                   ,bits
                   ,xflp);

}

void processBranch(const char *proc)
{
  // FIXME: add thermal block to branch predictor

  // get the branch
  const char* bpred = SescConf->getCharPtr(proc,"bpred") ;

  // get the type
  const char* type = SescConf->getCharPtr(bpred,"type") ;

  xcacti_flp xflp;

  double bpred_power;
  // switch based on the type
  if(!strcmp(type,"Taken") || 
     !strcmp(type,"Oracle") || 
     !strcmp(type,"NotTaken") || 
     !strcmp(type,"Static")) {
    // No tables
    bpred_power= 0;
  }else if(!strcmp(type,"2bit")) {
    long size = SescConf->getLong(bpred,"size") ;    

    // 32 = 8bytes line * 4 predictions per byte (assume 2bit counter)
    bpred_power = getEnergy(size/32, 8, 1, 1, 1, 1, 0, 8, &xflp);
    bpred_power= 0;

  }else if(!strcmp(type,"2level")) {
    long size = SescConf->getLong(bpred,"l2size") ;

    // 32 = 8bytes line * 4 predictions per byte (assume 2bit counter)
    bpred_power = getEnergy(size/32, 8, 1, 1, 1, 1, 0, 8, &xflp);

  }else if(!strcmp(type,"hybrid")) {
    long size = SescConf->getLong(bpred,"localSize") ;

    // 32 = 8bytes line * 4 predictions per byte (assume 2bit counter)
    bpred_power = getEnergy(size/32, 8, 1, 1, 1, 1, 0, 8, &xflp);
#ifdef SESC_THERM
    // FIXME: update layout
#endif
    size = SescConf->getLong(bpred,"metaSize");

    // 32 = 8bytes line * 4 predictions per byte (assume 2bit counter)
    bpred_power += getEnergy(size/32, 8, 1, 1, 1, 1, 0, 8, &xflp);

  }else{
    MSG("Unknown energy for branch predictor type [%s]", type);
    exit(-1);
  }

  update_layout("BPred", &xflp);
#ifdef SESC_THERM2
  // FIXME: partition energies per structure
#else
  SescConf->updateRecord(proc,"bpredEnergy",bpred_power) ;
#endif

  long btbSize  = SescConf->getLong(bpred,"btbSize");
  long btbAssoc = SescConf->getLong(bpred,"btbAssoc");
  double btb_power = 0;

  if (btbSize) {
    btb_power = getEnergy(btbSize*8, 8, btbAssoc, 1, 0, 1, 1, 64, &xflp);
    update_layout("BTB", &xflp);
#ifdef SESC_THERM2
    // FIXME: partition energies per structure
#else
    SescConf->updateRecord(proc,"btbEnergy",btb_power) ;
#endif
  }else{
    SescConf->updateRecord(proc,"btbEnergy",0.0) ;
  }

  double ras_power =0;
  long ras_size = SescConf->getLong(bpred,"rasSize");
  if (ras_size) {
    ras_power = getEnergy(ras_size*8, 8, 1, 1, 0, 1, 0, 64, &xflp);
    update_layout("RAS", &xflp);
#ifdef SESC_THERM2
    // FIXME: partition energies per structure
#else
    SescConf->updateRecord(proc,"rasEnergy",ras_power) ;
#endif
  }else{
    SescConf->updateRecord(proc,"rasEnergy",0.0) ;
  }

}

void processorCore()
{
  const char *proc = SescConf->getCharPtr("","cpucore",0) ;
  fprintf(stderr,"proc = [%s]\n",proc);

  xcacti_flp xflp;

  //----------------------------------------------
  // Branch Predictor
  processBranch(proc);
  
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

  double regEnergy = getEnergy(size*bytes, bytes, 1, rdPorts, wrPorts, banks, 0, bits, &xflp);

  printf("\nRegister [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*bytes, banks, rdPorts+wrPorts, regEnergy);

  update_layout("IntReg", &xflp);
  update_layout("FPReg" , &xflp); // FIXME: different energy for FP register
#ifdef SESC_THERM2
  // FIXME: partition energies per structure
#else
  SescConf->updateRecord(proc,"wrRegEnergy",regEnergy);
  SescConf->updateRecord(proc,"rdRegEnergy",regEnergy);
#endif


  //----------------------------------------------
  // Load/Store Queue
  size      = SescConf->getLong(proc,"maxLoads");
  banks     = 1; 
  rdPorts   = res_memport;
  wrPorts   = res_memport;

  if(SescConf->checkLong(proc,"lsqBanks"))
    banks = SescConf->getLong(proc,"lsqBanks");

  regEnergy = getEnergy(size*2*bytes,2*bytes,size,rdPorts,wrPorts,banks,1, 2*bits, &xflp);
  update_layout("LDQ", &xflp);
#ifdef SESC_THERM2
  // FIXME: partition energies per structure
#else
  SescConf->updateRecord(proc,"ldqRdWrEnergy",regEnergy);
#endif
  printf("\nLoad Queue [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*2*bytes, banks, 2*res_memport, regEnergy);

  size      =  SescConf->getLong(proc,"maxStores");
 
  regEnergy = getEnergy(size*4*bytes,4*bytes,size,rdPorts,wrPorts,banks,1, 2*bits, &xflp);
  update_layout("STQ", &xflp);
#ifdef SESC_THERM2
  // FIXME: partition energies per structure
#else
  SescConf->updateRecord(proc,"stqRdWrEnergy",regEnergy);
#endif
  printf("\nStore Queue [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*4*bytes, banks, 2*res_memport, regEnergy);


#ifdef SESC_INORDER 
  size      =  size/4;
 
  regEnergy = getEnergy(size*4*bytes,4*bytes,size,rdPorts,wrPorts,banks,1, 2*bits, &xflp);

  printf("\nStore Inorder Queue [%d bytes] banks[%d] ports[%d] Energy[%g]\n"
         ,size*4*bytes, banks, 2*res_memport, regEnergy);

  SescConf->updateRecord(proc,"stqRdWrEnergyInOrder",regEnergy);

#ifdef SESC_THERM
  I(0);
  exit(-1); // Not supported
#endif

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

  regEnergy = getEnergy(size*2,2*issueWidth,1,rdPorts,wrPorts,banks,0,16*issueWidth, &xflp);
  update_layout("ROB", &xflp);
#ifdef SESC_THERM2
  // FIXME: partition energies per structure
#else
  SescConf->updateRecord(proc,"robEnergy",regEnergy);
#endif

  printf("\nROB [%d bytes] banks[%d] ports[%d] Energy[%g]\n",size*2, banks, 2*rdPorts, regEnergy);

  //----------------------------------------------
  // Rename Table
  {
    double bitsPerEntry = log(SescConf->getLong(proc,"intRegs"))/log(2);
    
    size      = roundUpPower2(static_cast<unsigned int>(32*bitsPerEntry/8));
    banks     = 1;
    rdPorts   = 2*issueWidth;
    wrPorts   = issueWidth;

    regEnergy = getEnergy(size,1,1,rdPorts,wrPorts,banks,0,1, &xflp);
    update_layout("IntRAT", &xflp);
#ifdef SESC_THERM
    // FIXME: partition energies per structure
#endif

    printf("\nrename [%d bytes] banks[%d] Energy[%g]\n",size, banks, regEnergy);

    regEnergy += getEnergy(size,1,1,rdPorts/2+1,wrPorts/2+1,banks,0,1, &xflp);
    update_layout("FPRAT", &xflp);
#ifdef SESC_THERM2
    // FIXME: partition energies per structure
#else
    // unified FP+Int RAT energy counter
    SescConf->updateRecord(proc,"renameEnergy",regEnergy);
#endif
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
        tableBytes = roundUpPower2(tableBytes);
        regEnergy = getEnergy(tableBytes,tableBytes/assoc,assoc,rdPorts,wrPorts,banks,1,static_cast<int>(entryBits), &xflp);
        
        printf("\nWindow [%d bytes] assoc[%d] banks[%d] ports[%d] Energy[%g]\n"
               ,tableBytes, assoc, banks, rdPorts+wrPorts, regEnergy);

        if (SescConf->checkCharPtr(cluster,"blockName")) {
          const char *blockName = SescConf->getCharPtr(cluster,"blockName");
          update_layout(blockName, &xflp);
        }
#ifdef SESC_THERM2
        // FIXME: partition energies per structure
#else
        // unified FP+Int RAT energy counter
        SescConf->updateRecord(proc,"windowRdWrEnergy" ,regEnergy);
#endif
        
        SescConf->updateRecord(proc,"depTableEnergy",0.0);
      }else{
        // SEED ISSUE LOGIC

#ifdef SESC_THERM
        I(0);
        exit(-1); // Not supported
#endif
        // RAT has register and token
        {
          double bitsPerEntry = 2*log(SescConf->getLong(proc,"intRegs"))/log(2);
    
          size      = roundUpPower2(static_cast<unsigned int>(32*bitsPerEntry/8));
          banks     = 1; 
          rdPorts   = 2*issueWidth;
          wrPorts   = issueWidth;
          
          regEnergy = getEnergy(size,1,1,rdPorts,wrPorts,banks,0,1, &xflp);
          
          SescConf->updateRecord(proc,"renameEnergy",regEnergy);
        }

        SescConf->updateRecord(proc,"windowCheckEnergy",0.0);
        SescConf->updateRecord(proc,"windowRdWrEnergy" ,0.0);
        SescConf->updateRecord(proc,"windowSelEnergy"  ,0.0);

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

        regEnergy = getEnergy(tableBytes*size,tableBytes+1,1,rdPorts,wrPorts,banks,0,tableBits, &xflp);

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

#ifdef SESC_THERM
  sescTherm = new ThermTrace(0); // No input trace, just read conf
#endif

  const char *proc    = SescConf->getCharPtr("","cpucore",0);
  const char *l1Cache = SescConf->getCharPtr(proc,"dataSource");

  const char *l1CacheSpace = strstr(l1Cache," ");
  char *l1Section = strdup(l1Cache);
  if (l1CacheSpace)
    l1Section[l1CacheSpace - l1Cache] = 0;

  res_memport = SescConf->getLong(l1Section,"numPorts");

  xcacti_flp xflp;
  double l1Energy = getEnergy(l1Section, &xflp);

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
