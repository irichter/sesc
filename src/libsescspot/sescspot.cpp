/*
sescspot.cpp
Created by David van der Bokke and Andrew Hill for CMPE202

This application processes therm files created by SESC to report temperature data

*/
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "RC.h"
#include "flp.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

using namespace std;

/* Data structure to hold values from the therm file */
typedef std::vector<int> intList;
typedef struct namesList {
  std::string name;
  intList link;
  int percent;
  float value;
  float value1;
  float realoldValue;
};
typedef std::vector<namesList> strList;
strList eNamesList;     // Energy names
strList sspotNamesList; // sescspot names list
strList tmpList;        // Temprorary list
strList steadyList;     // Steady state values
strList sescList;       // SESC Stats list

/* simulator options	*/
static char *flp_cfg = "ev6.flp";			/* has the floorplan configuration	*/
static int omit_lateral = 0;				/* omit lateral chip resistances?	*/
static double init_temp = 60 + 273.15;		/* 60 degree C converted to Kelvin	*/
static int dtm_used = 0;					/* set accordingly	*/


/* chip specs	*/
double t_chip = 0.0005;	/* chip thickness in meters	*/
double thermal_threshold = 111.8 + 273.15;	/* temperature threshold for DTM (Kelvin)*/

/* heat sink specs	*/
double c_convec = 140.4;/* convection capacitance - 140.4 J/K */
double r_convec = 0.1;	/* convection resistance - 0.2 K/W	*/
double s_sink = 0.06;	/* heatsink side - 60 mm	*/
double t_sink = 0.0069; /* heatsink thickness  - 6.9 mm	*/

/* heat spreader specs	*/
double s_spreader = 0.03;	/* spreader side - 30 mm	*/
double t_spreader = 0.001;	/* spreader thickness - 1 mm */

/* interface material specs	*/
double t_interface = 0.000075;	/* interface material thickness  - 0.075mm	*/

/* ambient temp in kelvin	*/
double ambient = 40 + 273.15;	/* 45 C in kelvin	*/

static double *temp, *power, *overall_power, *steady_temp;
static flp_t *flp;

/* file defs */
char *infile;
char *outfile;
char *steadyfile;
char *conffile;
char *gplotfile;
FILE *p_infile;
FILE *p_outfile;
FILE *p_steady;
FILE *p_conf;
FILE *p_gplot;

std::ifstream if_infile;
std::ifstream if_outfile;
std::ifstream if_steady;
std::ifstream if_conf;


float cycles;  // cycle divider(10000)

float curcycle; // cycle we are on


void sim_init()
{
  /* initialize flp, get adjacency matrix */
  flp = read_flp(flp_cfg);
	
  /* initialize the R and C matrices */
  create_RC_matrices(flp, omit_lateral);

  /* allocate the temp and power arrays	*/
  /* using hotspot_vector to internally allocate  whatever extra nodes needed	*/

  temp = hotspot_vector(flp->n_units);
  power = hotspot_vector(flp->n_units);
  steady_temp = hotspot_vector(flp->n_units);
  overall_power = hotspot_vector(flp->n_units);
	
  /* set up initial temperatures */
  if (steadyfile != NULL) {
    if (!dtm_used)	/* initial T = steady T for no DTM	*/
      read_temp(flp, temp, steadyfile, 0);
    else	/* initial T = clipped steady T with DTM	*/
      read_temp(flp, temp, steadyfile, 1);
  }
  else	/* no input file - use init_temp as the temperature	*/
    set_temp(temp, flp->n_units, init_temp);
}

// Read values from the therm file
int readSescVals() 
{
  int ret = 0;
  int items = 0;
  char c;
  char name[100];
  
  float tmp;

  // Store the old values for use later(since some of them are cumulative)
  for (int x = 0; x < sspotNamesList.size(); x++) {
    sspotNamesList[x].value1 =sspotNamesList[x].realoldValue;
    sspotNamesList[x].realoldValue = 0.0;
    sspotNamesList[x].value = 0.0;
  }
  
  for (int x = 0; x < sescList.size(); x++) {
    sescList[x].value1 = sescList[x].realoldValue;
    sescList[x].realoldValue = 0;
    sescList[x].value = 0;
  }

  for (int x = 0; x < eNamesList.size(); x++) {
    eNamesList[x].value1 = eNamesList[x].realoldValue;
    eNamesList[x].realoldValue = 0;
    eNamesList[x].value = 0;
  }


  int x = 0;

  // Get the energy values
  std::string line;
  items = 0;
  if (getline(if_infile, line)) {
    std::istringstream linestream(line);
    std::string token;

    while (getline(linestream, token, '\t')) {
      sscanf(token.c_str(), "%f", &tmp);
      eNamesList[items].value = tmp;
      eNamesList[items].realoldValue = tmp;

      if (eNamesList[items].link.size() > 0) {
	for (int xx = 0; xx < eNamesList[items].link.size(); xx++) {
	  sspotNamesList[eNamesList[items].link[xx]].value += tmp;
	  sspotNamesList[eNamesList[items].link[xx]].realoldValue += tmp;
	}
      }
      items++;
    }
  } else {
    ret = -1;
  }
  // get the SESC stats
  items = 0;
  if (getline(if_infile, line)) {
    std::istringstream linestream(line);
    std::string token;

    while (getline(linestream, token, '\t')) {
      sscanf(token.c_str(), "%f", &tmp);
      sescList[items].value = tmp;
      sescList[items].realoldValue = tmp;
      items++;
    }
  } else {
    ret = -1;
  }


  // Correct the SESC Stats values since they are cumulative
  for (x = 0; x < sescList.size(); x++) {
    sescList[x].value = sescList[x].value - sescList[x].value1;
  }


  return ret;
}

// Dump values to the output file
void dump_tempAppend1(flp_t *flp, double *temp, char *file)
{
  int i;
  char str[STR_SIZE];
  FILE *fp = fopen (file, "a");
  if (!fp) {
    sprintf (str,"error: %s could not be opened for writing\n", file);
    fatal(str);
  }
  fprintf(fp, "\n");
	
  fprintf(fp, "%d\t", (int)curcycle);
  /* on chip temperatures	*/
  for (i=0; i < flp->n_units; i++)
    fprintf(fp, "%f\t", temp[i]);

  /* interface temperatures	*/
  for (i=0; i < flp->n_units; i++)
    fprintf(fp, "%f\t", temp[IFACE*flp->n_units+i]);

  /* spreader temperatures	*/
  for (i=0; i < flp->n_units; i++)
    fprintf(fp, "%f\t", temp[HSP*flp->n_units+i]);

  /* internal node temperatures	*/
  for (i=0; i < EXTRA; i++) {
    sprintf(str, "inode_%d", i);
    fprintf(fp, "%f\t", temp[i+NL*flp->n_units]);
  }
	
  float cyclenum = 0.0;
  if (curcycle == 1) 
    cyclenum = curcycle;
  else 
    cyclenum = cycles;

  for (i = 0; i < eNamesList.size(); i++) {
    fprintf(fp, "%f\t", eNamesList[i].value);
  }

  for (i = 0; i < sescList.size(); i++) {
    fprintf(fp, "%f\t", sescList[i].value/cyclenum);
  }

  fclose(fp);	
}

// Dump column names to file
void dump_tempTitles(flp_t *flp, char *file) 
{
  int i;
  char str[STR_SIZE];
  FILE *fp = fopen(file, "w");
  int x = 2;
  fprintf(fp, "Cycle\t");
  for (i = 0; i < flp->n_units; i++) {
    fprintf(fp, "%s\t", flp->units[i].name);
    printf("%s: %d\n", flp->units[i].name, x);
    x++;
  }

  for (i = 0; i < flp->n_units; i++) {
    fprintf(fp, "iface_%s\t", flp->units[i].name);
    printf("iface_%s: %d\n", flp->units[i].name, x);
    x++;
  }
  
  for (i = 0; i < flp->n_units; i++) {
    fprintf(fp, "hsp_%s\t", flp->units[i].name);
    printf("hsp_%s: %d\n", flp->units[i].name, x);
    x++;
  }
  
  for (i = 0; i < EXTRA; i++) {
    sprintf(str, "inode_%d", i);
    fprintf(fp, "%s\t", str);
    printf("%s: %d\n", str, x);
    x++;
  }

  for (i = 0; i < eNamesList.size(); i++) {
    fprintf(fp, "%s\t", eNamesList[i].name.c_str());
    printf("%s: %d\n", eNamesList[i].name.c_str(), x);
    x++;
  }

  for (i = 0; i < sescList.size(); i++) {
    fprintf(fp, "%s\t", sescList[i].name.c_str());
    printf("%s: %d\n", sescList[i].name.c_str(), x);
    x++;
  }
  fclose(fp);
}

// Read energy column names from therm file
void getColNames() 
{
  std::string line;
  
  if (getline(if_infile, line)) {
    std::istringstream linestream(line);
    std::string token;
    namesList nn;
    nn.link.clear();
    nn.value = 0;
    while (getline(linestream, token, '\t')) {
      nn.name = token.c_str();
      eNamesList.push_back(nn);
    }
  }
}

// Read sESC Stat names from therm file
void getSescNames() {
  std::string line;
  
  if (getline(if_infile, line)) {
    std::istringstream linestream(line);
    std::string token;
    namesList nn;
    nn.link.clear();
    nn.value = 0;
    while (getline(linestream, token, '\t')) {
      nn.name = token.c_str();
      sescList.push_back(nn);
    }
  }
}

void showUsage() 
{
  printf("SescSpot 1.0 Usage Summary:\n");
  printf("Arguments:\n");
  printf("-i <infile>  Input file(required) - The therm file for which you want temperature data\n");
  printf("-o <outfile> Output file(required) - Output file name for either steady state or transient temperature\n");
  printf("             This file has steady state temps if -s is not used, and transient if it is\n");
  printf("-c <conffile> Configuration file(required) - This is the mappings from floorplan variables to sesc variables\n");
  printf("-s <steadyfile> Steady state file - If this option is specified, the values from this file will be used as steady state values, and sescspot will output a transient temperatures file\n");
  printf("-f <floorplan> Floorplan file - Set to the floorplan you wish to use.  Default is 'ev6.flp'\n");
}

int main(int argc, char **argv)
{
  int i, n, num,x, lines = 0;
  int opt;
  //FILE *pin, *tout;	/* trace file pointers	*/
  char names[MAX_UNITS][STR_SIZE];
  double vals[MAX_UNITS];
  char linen[4000], *src;
  char name[100];
  char c;

  curcycle = 1.0;
  cycles = 10000.0;

  conffile = NULL;
  steadyfile = NULL;

  // process parameters
  while((opt = getopt(argc, argv, "f:n:c:i:o:s:")) != -1) {
    switch (opt) {
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
    case 's':
      steadyfile = optarg;
      break;
    case 'n':
      cycles = atoi(optarg);
      break;
    default:
      showUsage();
      exit(1);
      break;
    }
  }

  if (infile == NULL || outfile == NULL || conffile == NULL) {
    showUsage();
    exit(1);
  }

  // now read the config file

  if (!(p_outfile = fopen(outfile, "w")))
    fatal("Unable to open output file\n");

  if(!(p_conf = fopen(conffile, "r")))
    fatal("Unable to open configuration file\n");


  if_infile.open(infile, std::ifstream::in);

  if (steadyfile != NULL) if_steady.open(steadyfile, std::ifstream::in);



  getColNames();
  getSescNames();
	
  //now we need to read in the sescspot file to link items with the floorplan
  // eNamesList.link has a link to these names.. so when their #s are encountered
  // we already know where to add to
  // we do this the long way to be forgiving with delimeters

  namesList nn;
  nn.link.clear();
  nn.value = 0;
  nn.value1 = 0;
  nn.realoldValue = 0;
  nn.percent = 100;

  memset(name, '\0', sizeof(name));
  x = 0;
  int items = 0;
  int firstitem = 0;
  std::string tmp;
  nn.link.clear();
  while(c = fgetc(p_conf)) {
    if (isspace(c) || c == '\n') {
      //printf(".");
      if (firstitem == 0) {
	firstitem = 1;
	items++;
	x = 0;
	nn.name = name;
	nn.link.clear();
	sspotNamesList.push_back(nn);
	      
	//printf("%s\n", name);
      } else {
	tmp = name;
	for (int y = 0; y < eNamesList.size(); y++) {
	  if (name[x - 1] == '*') {
	    //printf("%s\n",eNamesList[y].name.c_str());
	    if (eNamesList[y].name.find(tmp.substr(0, x - 1), 0) != -1) {
	      //printf("%s: %s\n", name, eNamesList[y].name.c_str());
	      eNamesList[y].link.push_back(sspotNamesList.size() - 1);
	    }
	  } else if(name[x-1] == '%') {
	    sspotNamesList[items-1].percent = atoi(tmp.substr(0, x-1).c_str());
	    printf("%s: %d\n", sspotNamesList[items-1].name.c_str(), sspotNamesList[items-1].percent);
	  }else {
	    if (eNamesList[y].name.compare(name) == 0) {
	      eNamesList[y].link.push_back(sspotNamesList.size() - 1);
	    }
	  }
	}
      }
      while(isspace(c)) {
	if (c == '\n') {
	  firstitem = 0;
	}
	c = fgetc(p_conf);
	      
      }
      x = 0;
      memset(name, '\0', sizeof(name));
    }
    if (c == EOF) break;
    if (c == '\n') {x=0;firstitem = 0; c= fgetc(p_conf);}
	  
    name[x] = c;
    x++;
  }




  // already read one line of the infile, skip the next line.. only need to process every other line

  // if a steady state file isnt specified.. we know thats we are doing
  
  sim_init();
  if (steadyfile != NULL)
    dump_tempTitles(flp, outfile);
  readSescVals(); // we ignore the first set of values
  int iter = 0;;
  while(readSescVals() != -1) {
    iter++;
    // sspotNamesList has the current aggregate values for 10000 clock cycles
    // make sure #s are in floorplan order.. or we could have trouble
    for (x = 0; x < sspotNamesList.size(); x++) {
      char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
      // Energy fix 11/29/04
      if (sspotNamesList[x].percent != 100) sspotNamesList[x].value = sspotNamesList[x].value * ((float)sspotNamesList[x].percent/100.0);
      power[get_blk_index(flp, tmpname)] = sspotNamesList[x].value;
    }
    // Compute Temperatures
    compute_temp(power, temp, flp->n_units, (2e-6));
    
    // lets go back through and dump the correct values back into sspotNamesList and then dump them
    
    
    for (x = 0; x < sspotNamesList.size(); x++) {
      char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
      overall_power[get_blk_index(flp,tmpname)] += power[get_blk_index(flp, tmpname)];
      sspotNamesList[x].value = temp[get_blk_index(flp, tmpname)];
    }
    if (steadyfile != NULL) { // we have real data.. so lets use it..
      dump_tempAppend1(flp, temp, outfile);
    }
    for (x = 0; x < sspotNamesList.size(); x++) power[x] = 0;
    curcycle += cycles;
  }
    
  for (x = 0; x < sspotNamesList.size();x++) {
    char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
    overall_power[get_blk_index(flp, tmpname)] /= iter;
  }
  steady_state_temp(overall_power, steady_temp, flp->n_units);
  printf("Unit\tSteady\n");
  n = sspotNamesList.size();
  for (i=0; i < n; i++)
    printf("%s\t%.2f\n", flp->units[i].name, steady_temp[i]-273.15);
  for (i=0; i < n; i++)
    printf("interface_%s\t%.2f\n", flp->units[i].name, steady_temp[IFACE*n+i]-273.15);
  for (i=0; i < n; i++)
    printf("spreader_%s\t%.2f\n", flp->units[i].name, steady_temp[HSP*n+i]-273.15);
  printf("%s\t%.2f\n", "spreader_west", steady_temp[NL*n+SP_W]-273.15);
  printf("%s\t%.2f\n", "spreader_east", steady_temp[NL*n+SP_E]-273.15);
  printf("%s\t%.2f\n", "spreader_north", steady_temp[NL*n+SP_N]-273.15);
  printf("%s\t%.2f\n", "spreader_south", steady_temp[NL*n+SP_S]-273.15);
  printf("%s\t%.2f\n", "spreader_bottom", steady_temp[NL*n+SP_B]-273.15);
  printf("%s\t%.2f\n", "sink_west", steady_temp[NL*n+SINK_W]-273.15);
  printf("%s\t%.2f\n", "sink_east", steady_temp[NL*n+SINK_E]-273.15);
  printf("%s\t%.2f\n", "sink_north", steady_temp[NL*n+SINK_N]-273.15);
  printf("%s\t%.2f\n", "sink_south", steady_temp[NL*n+SINK_S]-273.15);
  printf("%s\t%.2f\n", "sink_bottom", steady_temp[NL*n+SINK_B]-273.15);

  if (steadyfile == NULL) 
    dump_temp(flp, steady_temp, outfile);
	
  return 0;
}
