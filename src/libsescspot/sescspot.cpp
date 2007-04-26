//----------------------------------------------------------------------------
// File: sescspot.cpp
//
// Description: Processes therm files created by SESC to report 
//              temperature data
// Authors    : David van der Bokke and Andrew Hill 
//
// Patch #1 (Javi): Read values from sesc.conf file


#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>

#include "SescConf.h"
#include "ThermTrace.h"

#include "RC.h"
#include "flp.h"
#include "util.h"
using namespace std;


// simulator options
static char *flp_cfg = "amd.flp";		/* has the floorplan configuration	*/
static int omit_lateral = 0;			/* omit lateral chip resistances?	*/
static double init_temp = 60 + 273.15;		/* 60 degree C converted to Kelvin	*/
static int dtm_used = 0;			/* set accordingly	*/

/* chip specs	*/
double t_chip = 0.0005;	/* chip thickness in meters	*/
double thermal_threshold = 111.8 + 273.15;	/* temperature threshold for DTM (Kelvin)*/

/* heat sink specs	*/
double c_convec = 140.4;/* convection capacitance - 140.4 J/K */
double r_convec = 0.1;	/* convection resistance - 0.2 K/W	*/
double s_sink = 0.06;	/* heatsink side - 60 mm	*/
double t_sink = 0.0069;	/* heatsink thickness  - 6.9 mm	*/

/* heat spreader specs	*/
double s_spreader = 0.03;	/* spreader side - 30 mm	*/
double t_spreader = 0.001;	/* spreader thickness - 1 mm */

/* interface material specs	*/
double t_interface = 0.000075;	/* interface material thickness  - 0.075mm	*/

/* ambient temp in kelvin	*/
double ambient = 45 + 273.15;	/* 45 C in kelvin	*/

static double *temp;
static double *power;
static flp_t *flp;

/* file defs */

char *infile;
char *outfile;
char *hotfile;
char *conffile = "power.conf";

FILE *p_outfile;

float cycles;  // cycle divider(10000)
float frequency;
float curcycle;	// cycle we are on

//----------------------------------------------------------------------------
// dump_tempAppend1 : Dumps values to the output file
//

void dump_tempAppend1(flp_t *flp, double *temp, char *file) {

  FILE *fp = fopen (file, "a");
  if (fp==0) {
    MSG("Error: %s could not be opened for writing\n", file);
    exit(4);
  }

  fprintf(fp, "\n");    
  fprintf(fp, "%-12d", (int)curcycle);

  // on chip temperatures	
  for (int i=0; i < flp->n_units; i++)
    fprintf(fp, "%-12f", temp[i]-273.15);

  fclose(fp);   
}


//----------------------------------------------------------------------------
// dump_tempTitles : Dumps column names to file
//

void dump_tempTitles(flp_t *flp, char *file) {

  char str[STR_SIZE];
  FILE *fp = fopen(file, "w");

  fprintf(fp, "%-12s", "Cycle");

  for (int i = 0; i < flp->n_units; i++) {
    fprintf(fp, "%-12s", flp->units[i].name);
  }

  fclose(fp);
}


//Parser the configuration file
void parseConfigFile() {

  // Read initialization parameters
  frequency = SescConf->getDouble("technology","Frequency");

  cycles    = SescConf->getInt("thermal","CyclesPerSample");
  init_temp = SescConf->getDouble("thermal","InitialTemp") + 273.15;
  ambient   = SescConf->getDouble("thermal","ambientTemp") + 273.15;

  // SESCSpot specific parameters
  dtm_used  = SescConf->getBool("thermSpot","DTMUsed");

  t_chip    = SescConf->getDouble("thermSpot","ChipThickness");

  c_convec  = SescConf->getDouble("thermSpot","ConvectionCapacitance");
  r_convec  = SescConf->getDouble("thermSpot","ConvectionResistance");

  s_sink    = SescConf->getDouble("thermSpot","HeatsinkLength");
  t_sink    = SescConf->getDouble("thermSpot","HeatsinkThinkness");

  s_spreader= SescConf->getDouble("thermSpot","SpreaderLength");
  t_spreader= SescConf->getDouble("thermSpot","SpreaderThickness");

  t_interface = SescConf->getDouble("thermSpot","InterfaceMaterialThickness");

  thermal_threshold = SescConf->getDouble("thermSpot","DTMTempThreshhold") + 273.15;
}

//----------------------------------------------------------------------------
// sim_init : Initialise temperature/power statistics
//    

void sim_init() {

  parseConfigFile();

  flp = read_flp(flp_cfg);

  create_RC_matrices(flp, omit_lateral);

  // allocate the temp and power arrays	
  temp  = hotspot_vector(flp->n_units);
  power = hotspot_vector(flp->n_units);
}

void showUsage() 
{
	printf("SESCSpot 1.0 Usage Summary:\n");
	printf("Arguments:\n");
	printf("\t-h <--hotfile>   HotSpot Temp Trace  - file that can be used by HotSpot3\n");
	printf("\t-i <--infile>    Input file          - The therm file for which you want temperature data\n");
	printf("\t-o <--outfile>   Output file         - Output file name for either steady state or transient temperature\n");
	printf("\t-c <--conffile>  Configuration file) - This is the mappings from floorplan variables to sesc variables\n");
	printf("\t-f <--floorplan> Floorplan file      - Set to the floorplan you wish to use.  Default is 'ev6.flp'\n");
}

int main(int argc, char **argv)
{
  int i, n, num,x, longindex, lines = 0;
  int opt;
  //FILE *pin, *tout;	/* trace file pointers	*/
  char names[MAX_UNITS][STR_SIZE];
  double vals[MAX_UNITS];
  char linen[4000], *src;
  char name[100];
  char c;

  curcycle = 0;

  std::string tmpstring;

  static struct option long_options[] =
    { 
      {"infile", required_argument, 0, 'i'},
      {"outfile", required_argument, 0, 'o'},
      {"configfile", required_argument, 0, 'c'},
      {"floorplan", required_argument, 0, 'f'},
      {0,0,0,0}
    };
  // process parameters
  while ((opt = getopt_long(argc, argv, "h:c:f:i:o:", long_options, &longindex)) != -1) {
    switch (opt) {
    case 'h':
      hotfile = optarg;
      break;
    case 'c':
      conffile = optarg;
      break;
    case 'f':
      flp_cfg = optarg;
      break;
    case 'i':
      infile = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    default:
      showUsage();
      exit(1);
      break;
    }
  }

  if (infile == NULL || (outfile == NULL && hotfile==NULL)) {
    showUsage();
    exit(1);
  }

  if (outfile)
    if (!(p_outfile = fopen(outfile, "w")))
      fatal("Unable to open output file\n");

  SescConf = new SConfig(conffile);
	
  sim_init();

  set_temp(temp, flp->n_units, init_temp);

  printf("Chip area %gx%gmm^2\n", 1e3*get_total_width(flp), 1e3*get_total_height(flp));
  printf("AmbientTemp %gC\n",ambient-273.15);

  printf("%-20s\t%-15s\n","Unit","Steady-State Temperatures Without Warmup");

  for (int i=0; i < flp->n_units; i++)
    printf("%-20s\t%.2f\n", flp->units[i].name, temp[i]-273.15);

  ThermTrace trace(infile);

  FILE *p_hotfile=0;
  if (hotfile) {
    if (!(p_hotfile = fopen(hotfile, "w")))
      fatal("Unable to open hotfile file\n");

    for (int i=0; i < flp->n_units; i++)
      fprintf(p_hotfile,"%-20s ", flp->units[i].name);
    fprintf(p_hotfile,"\n");
  }else{
    dump_tempTitles(flp, outfile);
  }

  I(flp->n_units == (int)trace.get_energy_size());

  double total;
  while(trace.read_energy()) {
    total = 0;
    for(size_t i=0;i<trace.get_energy_size();i++) {
      power[i] = trace.get_energy(i); // TODO: adjust energy -> power?
      total += power[i];
    }

    if (p_hotfile) {
      for (int i=0; i < flp->n_units; i++)
	fprintf(p_hotfile,"%g ", power[i]);
      fprintf(p_hotfile,"\n");
    }else{
      // Compute Temperatures
      compute_temp(power, temp, flp->n_units, (cycles/frequency));
      
      dump_tempAppend1(flp, temp, outfile);
    }
      
    curcycle += (float)cycles;
  }

  return 0;
}

