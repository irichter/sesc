/*------------------------------------------------------------
 *                              CACTI 3.0
 *               Copyright 2002 Compaq Computer Corporation
 *                         All Rights Reserved
 *
 * Permission to use, copy, and modify this software and its documentation is
 * hereby granted only under the following terms and conditions.  Both the
 * above copyright notice and this permission notice must appear in all copies
 * of the software, derivative works or modified versions, and any portions
 * thereof, and both notices must appear in supporting documentation.
 *
 * Users of this software agree to the terms and conditions set forth herein,
 * and hereby grant back to Compaq a non-exclusive, unrestricted, royalty-
 * free right and license under any changes, enhancements or extensions
 * made to the core functions of the software, including but not limited to
 * those affording compatibility with other hardware or software
 * environments, but excluding applications which incorporate this software.
 * Users further agree to use their best efforts to return to Compaq any
 * such changes, enhancements or extensions that they make and inform Compaq
 * of noteworthy uses of this software.  Correspondence should be provided
 * to Compaq at:
 *
 *                       Director of Licensing
 *                       Western Research Laboratory
 *                       Compaq Computer Corporation
 *                       250 University Avenue
 *                       Palo Alto, California  94301
 *
 * This software may be distributed (but not offered for sale or transferred
 * for compensation) to third parties, provided such third parties agree to
 * abide by the terms and conditions of this notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND COMPAQ COMPUTER CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL COMPAQ COMPUTER
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *------------------------------------------------------------*/
#include <stdio.h>
#include <ctype.h>

#include "def.h"
#include "areadef.h"
#include "SescConf.h"
#include "cacti_setup.h"
#include "cacti_time.h"
#include "io.h"

/*------------------------------------------------------------------------------*/
#include <vector>

double wattch2cactiFactor = 1;
double getEnergy(const char*);
void iterate();

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
	     || !strcmp(name,"revLVIDTable") ){
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

double getEnergy(const char *section)
{
  result_type result;
  arearesult_type arearesult;
  area_type arearesult_subbanked;
  parameter_type parameters;
  double NSubbanks;

  // set the input
  int cache_size = SescConf->getLong(section,"size") ;
  int block_size = SescConf->getLong(section,"bsize") ;
  int assoc = SescConf->getLong(section,"assoc") ;
  int nsets = cache_size/(block_size*assoc) ;
  int write_ports = 0 ;
  int read_ports = 0;
  int readwrite_ports = 1;
  int subbanks = 1;
  double tech = SescConf->getDouble("","tech") ;

  if (SescConf->getLong(section,"numPorts")>1) {
    read_ports      = SescConf->getLong(section,"numPorts")-2;
    readwrite_ports = 2;
  }

  if(SescConf->checkLong(section,"subBanks"))
    subbanks = SescConf->getLong(section,"subBanks");

  // input data
  int argc = 9 ;
  char *argv[9] ;
  argv[1] = strfy(cache_size) ;
  argv[2] = strfy(block_size) ;
  argv[3] = (nsets == 1) ? strdup("FA") : strfy(assoc) ;
  argv[4] = strfy(tech) ;
  argv[5] = strfy(readwrite_ports) ;
  argv[6] = strfy(read_ports) ;
  argv[7] = strfy(write_ports) ;
  argv[8] = strfy(subbanks) ;

  MSG("processing %s,%s",section,argv[3]) ;
  if(ca_input_data(argc,argv,&parameters,&NSubbanks) == ERROR){
    exit(0) ;
  }
  ca_calculate_time(&result,&arearesult,&arearesult_subbanked,&parameters,&NSubbanks);

  double div = subbanks;
  MSG("%s,%s processed %g",section,argv[3], result.total_power_allbanks * 1e9/div);

  return result.total_power_allbanks * 1e9/div;
}

void cacti_setup()
{
  const char *proc    = SescConf->getCharPtr("cpucore","cpucore");
  const char *l1Cache = SescConf->getCharPtr(proc,"dataSource");

  const char *l1CacheSpace = strstr(l1Cache," ");
  char *l1Section = strdup(l1Cache);
  if (l1CacheSpace)
    l1Section[l1CacheSpace - l1Cache] = 0;

  double l1Energy = getEnergy(l1Section);

  double WattchL1Energy = SescConf->getDouble("","wattchDataCacheEnergy");

  if (WattchL1Energy) {
    wattch2cactiFactor = WattchL1Energy/l1Energy;
    fprintf(stderr,"wattch2cacti Factor %g\n", wattch2cactiFactor);
  }else{
    fprintf(stderr,"-----WARNING: No wattch correction factor\n");
  }

  iterate();
}
