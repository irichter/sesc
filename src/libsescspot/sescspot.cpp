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
#include "RC.h"
#include "flp.h"
#include "util.h"
using namespace std;

// Data structure to hold values from the therm file
typedef std::vector<int> intList;

typedef struct namesList {
	std::string name;
	intList link;
	int percent;
        double value;
	double value1;
	double realoldValue;
};

typedef std::vector<namesList> strList;
strList eNamesList;		// Energy names
strList sspotNamesList;	// sescspot names list
strList tmpList;		// Temprorary list
strList steadyList;		// Steady state values
strList sescList;		// SESC Stats list
strList warmList;		// Warmup Stats list
// simulator options
static char *flp_cfg = "sescspot.flp";		/* has the floorplan configuration	*/
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
double t_sink = 0.0069;	/* heatsink thickness  - 6.9 mm	*/

/* heat spreader specs	*/
double s_spreader = 0.03;	/* spreader side - 30 mm	*/
double t_spreader = 0.001;	/* spreader thickness - 1 mm */

/* interface material specs	*/
double t_interface = 0.000075;	/* interface material thickness  - 0.075mm	*/

/* ambient temp in kelvin	*/
double ambient = 45 + 273.15;	/* 45 C in kelvin	*/

static double *temp, *power, *overall_power, *steady_temp;
static flp_t *flp;

/* file defs */

char *warmfile;
char *infile;
char *outfile;
char *steadyfile;
char *conffile = "sescspot.sspot";
char *gplotfile;
FILE *p_initfile;
FILE *p_infile;
FILE *p_outfile;
FILE *p_steady;
FILE *p_conf;
FILE *p_gplot;

std::ifstream if_warmfile;
std::ifstream if_infile;
std::ifstream if_outfile;
std::ifstream if_steady;
std::ifstream if_conf;


float cycles;  // cycle divider(10000)
float frequency;
float curcycle;	// cycle we are on

//


//----------------------------------------------------------------------------
// Tokenize : Split up a string based upon delimiteres, defaults to space-deliminted (like strok())
// 
void Tokenize(const string& str,
			  std::vector<string>& tokens,
			  const string& delimiters = " ")
{
    tokens.clear();
//Skip delimiters at beginning.
//Fine the location of the first character that is not one of the delimiters starting
//from the beginning of the line of test
	string::size_type lastPos = str.find_first_not_of(delimiters,0);
//Now find the next occurrance of a delimiter after the first one 
// (go to the end of the first character-deliminated item)
	string::size_type pos = str.find_first_of(delimiters, lastPos);
//Now keep checking until we reach the end of the line
	while (string::npos != pos || string::npos != lastPos) {
		//Found a token, add it to the vector.
		//Take the most recent token and store it to "tokens"
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		//Skip delimiters to find the beginning of the next token
		lastPos = str.find_first_not_of(delimiters,pos);
		//Find the end of the next token
		pos = str.find_first_of(delimiters, lastPos);
	}
}

//----------------------------------------------------------------------------
// sim_init : Initialise temperature/power statistics
//    

void sim_init()
{
// initialize flp, get adjacency matrix 
	flp = read_flp(flp_cfg);

// initialize the R and C matrices 
	create_RC_matrices(flp, omit_lateral);

// allocate the temp and power arrays	
// using hotspot_vector to internally allocate  whatever extra nodes needed	

	temp = hotspot_vector(flp->n_units);
	power = hotspot_vector(flp->n_units);
	steady_temp = hotspot_vector(flp->n_units);
	overall_power = hotspot_vector(flp->n_units);


}



//----------------------------------------------------------------------------
// readSescVals : Read values from the therm file
//                 


int readSescVals() 
{
	int x = 0;
	int ret = 0;
	int items = 0;
	char c;
	char name[100];

	float tmp;
	std::string line;
	
// Store the old values for use later(since some of them are cumulative)
	for (int x = 0; x < (int)sspotNamesList.size(); x++) {
		sspotNamesList[x].value1 =sspotNamesList[x].realoldValue;
		sspotNamesList[x].realoldValue = 0.0;
		sspotNamesList[x].value = 0.0;
	}

	for (int x = 0; x < (int)sescList.size(); x++) {
		sescList[x].value1 = sescList[x].realoldValue;
		sescList[x].realoldValue = 0;
		sescList[x].value = 0;
	}

	for (int x = 0; x < (int)eNamesList.size(); x++) {
		eNamesList[x].value1 = eNamesList[x].realoldValue;
		eNamesList[x].realoldValue = 0;
		eNamesList[x].value = 0;
	}
	
	if(!getline(if_infile, line))
	  return(-1); //get a new line. If we are at the end of the file, return (-1)
	std::istringstream linestream(line);
	std::string token;
	items=0;
	
	while (getline(linestream, token, '\t')) {
	  sscanf(token.c_str(), "%f", &tmp);
	    eNamesList[items].value=tmp;
	  items++;
	}
    
	// Go though the eNamesList, accumulating the power data for each floorplan block in sspotNamesList
	// If eNamesList[i].link[j]>0, then list item i has power data to be added to floorplan block sspotNamesList[eNamesList[i].link[j]]
	for(int i=0; i<(int)eNamesList.size();i++){
	  if (eNamesList[i].link.size() > 0) {
	    for (int j = 0; j < (int)eNamesList[i].link.size(); j++) {
	      sspotNamesList[eNamesList[i].link[j]].value += eNamesList[i].value; //store accumulate the average power for each block    
	    }
	  }
	}
	for (int x = 0; x < (int)sspotNamesList.size(); x++) {
	  char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
	  if (sspotNamesList[x].percent != 100) 
	    sspotNamesList[x].value = sspotNamesList[x].value * ((float)sspotNamesList[x].percent/100.0);
	  power[get_blk_index(flp, tmpname)] = sspotNamesList[x].value;
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

// Correct the SESC Stats values since they are cumulative! 
// This was needed to solve a bug in which the temp would not
// plateau at all
	for (x = 0; x < (int)sescList.size(); x++) {
		sescList[x].value = sescList[x].value - sescList[x].value1;
	}

	return ret;
}


//----------------------------------------------------------------------------
// dump_tempAppend1 : Dumps values to the output file
//

void dump_tempAppend1(flp_t *flp, double *temp, char *file)
{
	int i;
	float cyclenum = 0.0;
	char str[STR_SIZE];
	FILE *fp = fopen (file, "a");

	if (!fp) {
		sprintf (str,"error: %s could not be opened for writing\n", file);
		fatal(str);
	}
	fprintf(fp, "\n");    
	fprintf(fp, "%-12d", (int)curcycle);

// on chip temperatures	
	for (i=0; i < flp->n_units; i++)
		fprintf(fp, "%-12f", temp[i]-217.15);

// interface temperatures
	for (i=0; i < flp->n_units; i++)
		fprintf(fp, "%-12f", temp[IFACE*flp->n_units+i]-217.15);

// spreader temperatures	
	for (i=0; i < flp->n_units; i++)
		fprintf(fp, "%-12f", temp[HSP*flp->n_units+i]-217.15);

// internal node temperatures	
	for (i=0; i < EXTRA; i++) {
		sprintf(str, "inode_%d", i);
		fprintf(fp, "%-12f", temp[i+NL*flp->n_units]-217.15);
	}

	if (curcycle == 1)
		cyclenum = curcycle;
	else
		cyclenum = cycles;

	for (i = 0; i < (int)eNamesList.size(); i++)
		fprintf(fp, "%-12f\t", eNamesList[i].value);

	for (i = 0; i < (int)sescList.size(); i++)
		fprintf(fp, "%-12f\t", sescList[i].value/cyclenum);

	fclose(fp);   
}


//----------------------------------------------------------------------------
// dump_tempTitles : Dumps column names to file
//

void dump_tempTitles(flp_t *flp, char *file) 
{
	int i;
	char str[STR_SIZE];
	FILE *fp = fopen(file, "w");
	int x = 2;

	fprintf(fp, "%-12s", "Cycle");

	for (i = 0; i < flp->n_units; i++) {
		fprintf(fp, "%-12s", flp->units[i].name);
		//printf("%s: %d\n", flp->units[i].name, x);
		x++;
	}

	for (i = 0; i < flp->n_units; i++) {
		fprintf(fp, "iface_%-6s", flp->units[i].name);
		//printf("iface_%s: %d\n", flp->units[i].name, x);
		x++;
	}

	for (i = 0; i < flp->n_units; i++) {
		fprintf(fp, "hsp_%-8s", flp->units[i].name);
		//printf("hsp_%s: %d\n", flp->units[i].name, x);
		x++;
	}

	for (i = 0; i < EXTRA; i++) {
		sprintf(str, "inode_%-2d", i);
		fprintf(fp, "%-4s", str);
		//printf("%s: %d\n", str, x);
		x++;
	}

	for (i = 0; i < (int)eNamesList.size(); i++) {
		fprintf(fp, "%-12s", eNamesList[i].name.c_str());
		//printf("%s: %d\n", eNamesList[i].name.c_str(), x);
		x++;
	}
	fprintf(fp, "\n");
	for (i = 0; i < (int)sescList.size(); i++) {
		fprintf(fp, "%-12s", sescList[i].name.c_str());
		//printf("%s: %d\n", sescList[i].name.c_str(), x);
		x++;
	}

	fclose(fp);
}


//----------------------------------------------------------------------------
// getColNames : Read energy column names from therm file
//

void getColNames(std::ifstream &if_file) 
{
	std::string line;
	if (getline(if_file, line)) {
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
void getSescNames(std::ifstream &if_file) {
	std::string line;
	if (getline(if_file, line)) {
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

//Parser the configuration file
void parseConfigFile(int printflag){

//Now correlate the Energy Labels (FULoad, FUStore etc)  with the Corresponding Floorplan Block (FPAdd, FPReg etc)
	namesList nn;
	std::vector<string> tokens;
	std::string tmpstring;



	int items = 0; 

	//Find "[SescSpot]" in power.conf or other specified configuration file 
	while(getline(if_conf,tmpstring)){
		if((int)tmpstring.find("[SescSpot]",0) != -1)
			break;
	}
	for(int y=0; y<15 ; y++){
	  if(!getline(if_conf, tmpstring))
	    fatal("Sescspot: Configuration File Missing Data");
	  if(!printflag)
	    continue; //If printflag is false, then don't print anything
	  if(tmpstring.empty())
	    continue; //ignore blank lines
	  Tokenize(tmpstring, tokens, " \t");
	  if(tokens[0] == "#" || tokens[1] == "#"){
	    continue;//skip comments
	  }
	  if(tokens[0] == "Frequency"){ 
	    frequency=atof(tokens[1].c_str());
	    cout << "Frequency: " << frequency << endl;
	  }   
	  else if(tokens[0] == "CyclesPerSample"){
	    cycles=(float)atoi(tokens[1].c_str());
	    cout << "CyclesPerSample: " << cycles << endl;
	  }
	  else if(tokens[0] == "InitialTemp"){
	    init_temp=atof(tokens[1].c_str());
	    cout << "InitialTemp: " << init_temp << endl;
	    init_temp+=217.15;
	  }
	  else if(tokens[0] == "DTMUsed"){
	    dtm_used=atoi(tokens[1].c_str());
	    cout << "DTMUsed: " << (dtm_used? "yes" : "no") << endl;
	  }
	  else if(tokens[0] == "ChipThickness"){
	    t_chip=atof(tokens[1].c_str());
	    cout << "ChipThickness: " << t_chip << endl;
	  }
	  else if(tokens[0] == "DTMTempThreshhold"){
	    thermal_threshold=atof(tokens[1].c_str());
	    cout << "DTMTempThreshhold: " << thermal_threshold << endl;
	    init_temp+=217.15;
	  }
	  else if(tokens[0] == "ConvectionCapacitance"){
	    c_convec=atof(tokens[1].c_str());
	    cout << "ConvectionCapacitance: " << c_convec << endl;
	  }
	  else if(tokens[0] == "ConvectionResistance"){
	    r_convec=atof(tokens[1].c_str());
	    cout << "ConvectionResistance: " << r_convec << endl;
	  }
	  else if(tokens[0] == "HeatsinkLength"){                
	    s_sink=atof(tokens[1].c_str());
	    cout << "HeatsinkLength: " << s_sink << endl;
	  }
	  else if(tokens[0] == "HeatsinkThinkness"){
	    t_sink=atof(tokens[1].c_str());
	    cout << "HeatsinkThickness: " << t_sink << endl;
	  }
	  else if(tokens[0] == "SpreaderLength"){
	    s_spreader=atof(tokens[1].c_str());
	    cout << "SpreaderLength: " << s_spreader << endl;
	  }
	  else if(tokens[0] == "SpreaderThickness"){
	    t_spreader=atof(tokens[1].c_str());
	    cout << "SpreaderThickness: " << t_spreader << endl;
	  }
	  else if(tokens[0] == "InterfaceMaterialThickness"){
	    t_interface=atof(tokens[1].c_str());
	    cout << "InterfaceMaterialThickness: " << t_interface << endl;
	  }
	  else if(tokens[0] == "AmbientTemperature"){
	    ambient=atof(tokens[1].c_str());
	    cout << "AmbientTemperature: " << ambient << endl;
	    ambient+=217.15;
	  }
	  else
	    fatal("SescSpot: Error in Configuration File, Cannot Read Settings\n"); 
	}
	
	  
	
	while(getline(if_conf, tmpstring)){
	    if(tmpstring.empty())
			continue; //ignore blank lines
		Tokenize(tmpstring, tokens, " \t");  
		if(tokens[0] == "#")
            continue; //skip comments
		
	    
		//Store a new entry for the Floorplan Block Name at the beginning of the line in the .sspot config file
		items++;   
		nn.name = tokens[0];
		nn.value = 0;
		nn.value1 = 0;
		nn.realoldValue = 0;
		nn.percent = 100;		
		nn.link.clear();
		sspotNamesList.push_back(nn);

		//Now add all of the energy names that follow the floorplan block name 
		//on that given line of text to the list of "links" that correspond to the floorplan block name. 
		//Handle wildcards and percentages accordingly.
		//A percent sign indicates that the Energy Parameter represents some percentage of the overall energy.
		//A wildcard indicates that any energy parameter that begin with that prefix should be included
		//continue until the entire line has been read
		for(int y=1; y<(int)tokens.size(); y++){
			if(tokens[y] == "#")
                break; //skip comments
			   // cout << tokens[y] << endl; //This is for texting
			//If an asterisk was encountered (wildcard), then find any elements with that prefix
			if((tokens[y].c_str())[tokens[y].length()-1] == '*'){  
				for (int i = 0; i < (int)eNamesList.size(); i++) {   
					if ((int)eNamesList[i].name.find(tokens[y].substr(0, tokens[y].length()-1), 0) != -1) {
					  //  printf("%s: %s\n", nn.name.c_str(), eNamesList[i].name.c_str());
						eNamesList[i].link.push_back((int)sspotNamesList.size() - 1);
					}
				}
			}
			else if((tokens[y].c_str())[tokens[y].length()-1]== '%') {
				sspotNamesList[items-1].percent = atoi(tokens[y].substr(0, tokens[y].length()-1).c_str());
			   // printf("%s: %d\n", sspotNamesList[items-1].name.c_str(), sspotNamesList[items-1].percent);
			} 
			else {
				for (int i = 0; i < (int)eNamesList.size(); i++) {  
					if (eNamesList[i].name == tokens[y]) {
						eNamesList[i].link.push_back((int)sspotNamesList.size() - 1);
					 //   printf("%s: %s\n", nn.name.c_str(), eNamesList[i].name.c_str());
					}
				}
			}
		}
	}
}

void calcAveragePower(std::ifstream &if_file){
    std::string line;
	//ignore the first two line
    if_file.clear();
    if_file.seekg(0, ios::beg);

    for(int i=0;i<(int)eNamesList.size();i++){
      eNamesList[i].value=(float)0;
    }
    
    getline(if_file,line); //skip first two lines
    getline(if_file,line);

    /* The following algorith is used to calculate the running average (this is to prevent overflow)
     *  mean := x[0];
     * for i:= 0 to number_of_data_points do
     * mean := ((i/i+1)*mean) + (x[i]/(i+1)); 
     */
    
    
    getline(if_file,line);
    std::istringstream linestream(line);
    std:string token;
     
    float tmp;
    int items=0;

    while (getline(linestream, token, '\t')) {
      sscanf(token.c_str(), "%f", &tmp);
      eNamesList[items].value = tmp; // mean := x[0]
      items++;
    }
    getline(if_file,line);	//ignore the next line that contains just stats
    float i=1;
    while(getline(if_file, line)) {
      std::istringstream linestream(line);
      std::string token;
      float tmp;
      int items=0;
      while (getline(linestream, token, '\t')) {
	sscanf(token.c_str(), "%f", &tmp);
	eNamesList[items].value = (i/(i + 1.0))*eNamesList[items].value + tmp/(i+1);
	items++;
      }
      getline(if_file,line);	//ignore the next line that contains just stats
      i++;
    }
    //reset the file pointer to the beginning of the file again (this is to handle the case where the warmupfile equals the input file
    

        // Go though the eNamesList, accumulating the power data for each floorplan block in sspotNamesList
        // If eNamesList[i].link[j]>0, then list item i has power data to be added to floorplan block sspotNamesList[eNamesList[i].link[j]]
        for(int i=0; i<(int)eNamesList.size();i++){
            if (eNamesList[i].link.size() > 0) {
                for (int j = 0; j < (int)eNamesList[i].link.size(); j++) {
                    sspotNamesList[eNamesList[i].link[j]].value += eNamesList[i].value; //store accumulate the average power for each block    
				}
			}
        }

	for (int x = 0; x < (int)sspotNamesList.size(); x++) {
	  char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
	  if (sspotNamesList[x].percent != 100) 
	    sspotNamesList[x].value = sspotNamesList[x].value * ((float)sspotNamesList[x].percent/100.0);
	  overall_power[get_blk_index(flp, tmpname)] = sspotNamesList[x].value;
	}
        //Now sspotNamesList contains the average power data for each floorplan block

}

void calcMaxPower(std::ifstream &if_file){
    std::string line;
    if_file.clear();
    if_file.seekg(0, ios::beg);
    for(int i=0;i<(int)eNamesList.size();i++){
      eNamesList[i].value=(float)0;
    }

    getline(if_file,line);
    getline(if_file,line);
    while(getline(if_file, line)) {
      std::istringstream linestream(line);
      std::string token;
      float tmp;
      int items=0;
      
      while (getline(linestream, token, '\t')) {
	sscanf(token.c_str(), "%f", &tmp);
	if(tmp > eNamesList[items].value)
	  eNamesList[items].value=tmp;
	items++;
      }
      getline(if_file,line);
    }
    //reset the file pointer to the beginning of the file again (this is to handle the case where the warmupfile equals the input file
    
    // Go though the eNamesList, accumulating the power data for each floorplan block in sspotNamesList
    // If eNamesList[i].link[j]>0, then list item i has power data to be added to floorplan block sspotNamesList[eNamesList[i].link[j]]
    for(int i=0; i<(int)eNamesList.size();i++){
      if (eNamesList[i].link.size() > 0) {
	for (int j = 0; j < (int)eNamesList[i].link.size(); j++) {
	  sspotNamesList[eNamesList[i].link[j]].value += eNamesList[i].value; //store accumulate the average power for each block    
	}
      }
    }
    for (int x = 0; x < (int)sspotNamesList.size(); x++) {
      if (sspotNamesList[x].percent != 100) sspotNamesList[x].value = sspotNamesList[x].value * ((float)sspotNamesList[x].percent/100.0);
    }
    //Now sspotNamesList contains the Max power data for each floorplan block at the end of each sample period
    
}

void calcMinPower(std::ifstream &if_file){
    std::string line;
		if_file.clear();
    if_file.seekg(0, ios::beg);
	getline(if_file,line);
	getline(if_file,line);

	for(int i=0;i<(int)eNamesList.size();i++){
		eNamesList[i].value=(float)-1;
	}

	while(getline(if_file, line)) {
		std::istringstream linestream(line);
		std::string token;
        float tmp;
        int items=0;
		while (getline(linestream, token, '\t')) {
			sscanf(token.c_str(), "%f", &tmp);
			 if(tmp < eNamesList[items].value || eNamesList[items].value<0)
				eNamesList[items].value=tmp;
            items++;
        }
		getline(if_file, line);
    }
    //reset the file pointer to the beginning of the file again (this is to handle the case where the warmupfile equals the input file

    
        // Go though the eNamesList, accumulating the power data for each floorplan block in sspotNamesList
        // If eNamesList[i].link[j]>0, then list item i has power data to be added to floorplan block sspotNamesList[eNamesList[i].link[j]]
        for(int i=0; i<(int)eNamesList.size();i++){
            if (eNamesList[i].link.size() > 0) {
                for (int j = 0; j < (int)eNamesList[i].link.size(); j++) {
                    sspotNamesList[eNamesList[i].link[j]].value += eNamesList[i].value; //store accumulate the average power for each block    
				}
			}
        }
	for (int x = 0; x < (int)sspotNamesList.size(); x++) {
  if (sspotNamesList[x].percent != 100) sspotNamesList[x].value = sspotNamesList[x].value * ((float)sspotNamesList[x].percent/100.0);
	}
        //Now sspotNamesList contains the average power data for each floorplan block

}

void showUsage() 
{
	printf("SescSpot 1.0 Usage Summary:\n");
	printf("Arguments:\n");
	printf("-i <--infile>  Input file(required) - The therm file for which you want temperature data\n");
	printf("-o <--outfile> Output file(required) - Output file name for either steady state or transient temperature\n");
	printf("This file has steady state temps if -s is not used, and transient if it is\n");
	printf("-c <--conffile> Configuration file(required) - This is the mappings from floorplan variables to sesc variables\n");
	printf("-f <--floorplan> Floorplan file - Set to the floorplan you wish to use.  Default is 'ev6.flp'\n");
	printf("<--warmfile> Warmup file - The warmup file is used to generate average power information for the heatsink.\n");
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
		{"warmfile", required_argument,   0, 'q'},
		{"outfile", required_argument, 0, 'o'},
		{"configfile", required_argument, 0, 'c'},
		{"floorplan", required_argument, 0, 'f'},
		{0,0,0,0}
	};
// process parameters
	while ((opt = getopt_long(argc, argv, "q:f:c:i:o:", long_options, &longindex)) != -1) {
		switch (opt) {
		case 'q':
			warmfile = optarg;
			break;
		case 'c':
			conffile = optarg;
			break;
		case 'f':
			flp_cfg = optarg;
			break;
		case 'g':
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

	if (infile == NULL || outfile == NULL) {
		showUsage();
		exit(1);
	}



	if (!(p_outfile = fopen(outfile, "w")))
		fatal("Unable to open output file\n");

//open the input file 
	if_infile.open(infile, std::ifstream::in);

	if (!if_infile)
		fatal("Cannot open input file\n");

//open the configuration file 
	if_conf.open(conffile, std::ifstream::in);

	if (!if_conf)
		fatal("Cannot open the configuration file\n");


	
	if(warmfile != NULL)
	  if_warmfile.open(warmfile, std::ifstream::in);
	
	if(!if_warmfile){
	  fatal("Cannot open warmup file\n");
	}

    // initialize flp, get adjacency matrix 
    flp = read_flp(flp_cfg);
    
    // initialize the R and C matrices 
    create_RC_matrices(flp, omit_lateral);
    
    // allocate the temp and power arrays	
    // using hotspot_vector to internally allocate  whatever extra nodes needed	
    
    temp = hotspot_vector(flp->n_units);
    power = hotspot_vector(flp->n_units);
    steady_temp = hotspot_vector(flp->n_units);
    overall_power = hotspot_vector(flp->n_units);   
    
   //if we specified a warmup file, initial warmup stage
    if(warmfile != NULL){
     //Store the list of Energy Labels (on the first line of the therm file) in enamesList
    
    getColNames(if_warmfile);
    
    //Store the list of Stats Labels (on the second line of the therm file) in sescList
    
    getSescNames(if_warmfile);
    
    //Parse the configuration file correlating the energy labels with their respecitve floorplan units
    //Initialize sspotNamesList Entries (but do not store values)
    parseConfigFile(1);
    

      //Begin reading the warmup file (just the labels on the top two lines) 
      strList tmpeNamesList=eNamesList; //save eNamesList data  	
      strList tmpsspotNamesList=sspotNamesList;
      
      //Calculate the max energy values for each of the Energy labels, storing the data to enameslist[i].value entities
      //Then accumulate all the energy numbers in sspotNamesList
      calcMaxPower(if_warmfile);
      strList MaxPower=sspotNamesList;
      
      
      eNamesList=tmpeNamesList;
      sspotNamesList=tmpsspotNamesList;
      
      //Calculate the average energy values for each of the Energy labels, storing the data to enameslist[i].value entities
      //Then accumulate all the energy numbers in sspotNamesList
      calcAveragePower(if_warmfile);
      strList AveragePower=sspotNamesList;
      
      
      eNamesList=tmpeNamesList;
      sspotNamesList=tmpsspotNamesList;
      
      
      
      //Calculate the min energy values for each of the Energy labels, storing the data to enameslist[i].value entities
      //Then accumulate all the energy numbers in sspotNamesList
      calcMinPower(if_warmfile);
      strList MinPower=sspotNamesList;
      
      eNamesList=tmpeNamesList;
      sspotNamesList=tmpsspotNamesList;
      
      
      
      cout << "FloorPlan Unit \t";
      cout << "MAX Power \t MIN Power \t AVERAGE Power" << endl;
      /* on chip temperatures	*/
      cout.fill(' ');
      cout.setf(ios::left);
      for (int i=0; i < (int)sspotNamesList.size(); i++){
	cout.width(16);
	cout << sspotNamesList[i].name;
	cout.width(17);
	cout << MaxPower[i].value;
	cout.width(16);
	cout << MinPower[i].value;
	cout << AveragePower[i].value << endl;
      }
      
      steady_state_temp(overall_power, temp, flp->n_units);
      
      int idx;
      double max;
      // find maximum steady-state on chip temperature 
      for (int i=0; i < flp->n_units; i++){
	if(temp[i]> max)
	  max = temp[i];
      }
      // If clipping is enabled, and the maximum steady state temperature is higher than the threshhold,
      // then scale all the temperatures such that maximum does not exceed the threshold
      // Note: DTM requires that the steady-state be clipped
      if (dtm_used && (max > thermal_threshold)) {
	/* if max has to be brought down to thermal_threshold, 
	 * (w.r.t the ambient) what is the scale down factor?
	 */
	double factor = (thermal_threshold - ambient) / (max - ambient);
	
	/* scale down all temperature differences (from ambient) by the same factor	*/
	for (int i=0; i < NL*flp->n_units + EXTRA; i++)
	  temp[i] = (temp[i]-ambient)*factor + ambient;
      }
      
      printf("\t\t**** WARMUP STAGE ****\n");
      printf("%-20s\t%-15s\n","Unit","Steady-State Temperatures After Warmup");
      n = sspotNamesList.size();
      for (int i=0; i < n; i++)
	printf("%-20s\t%.2f\n", flp->units[i].name, temp[i]-273.15);
      for (int i=0; i < n; i++)
	printf("interface_%-10s\t%.2f\n", flp->units[i].name, temp[IFACE*n+i]-273.15);
      for (int i=0; i < n; i++)
	printf("spreader_%-11s\t%.2f\n", flp->units[i].name, temp[HSP*n+i]-273.15);
      printf("%-20s\t%.2f\n", "spreader_west", temp[NL*n+SP_W]-273.15);
      printf("%-20s\t%.2f\n", "spreader_east", temp[NL*n+SP_E]-273.15);
      printf("%-20s\t%.2f\n", "spreader_north", temp[NL*n+SP_N]-273.15);
      printf("%-20s\t%.2f\n", "spreader_south", temp[NL*n+SP_S]-273.15);
      printf("%-20s\t%.2f\n", "spreader_bottom", temp[NL*n+SP_B]-273.15);
      printf("%-20s\t%.2f\n", "sink_west", temp[NL*n+SINK_W]-273.15);
      printf("%-20s\t%.2f\n", "sink_east", temp[NL*n+SINK_E]-273.15);
      printf("%-20s\t%.2f\n", "sink_north", temp[NL*n+SINK_N]-273.15);
      printf("%-20s\t%.2f\n", "sink_south", temp[NL*n+SINK_S]-273.15);
      printf("%-20s\t%.2f\n", "sink_bottom", temp[NL*n+SINK_B]-273.15);
    }
    else{
      if_infile.clear();
      if_infile.seekg(0, ios::beg);  
      getColNames(if_infile);
      
      //Store the list of Stats Labels (on the second line of the therm file) in sescList
      
      getSescNames(if_infile);
      
      //Parse the configuration file correlating the energy labels with their respecitve floorplan units
      //Initialize sspotNamesList Entries (but do not store values)
      parseConfigFile(1);
      
      set_temp(temp, flp->n_units, init_temp);
      //Begin reading the warmup file (just the labels on the top two lines) 
      
      printf("\t\t**** NO WARMUP ****\n");
      printf("%-20s\t%-15s\n","Unit","Steady-State Temperatures Without Warmup");
      n = sspotNamesList.size();
      for (int i=0; i < n; i++)
	printf("%-20s\t%.2f\n", flp->units[i].name, temp[i]-273.15);
      for (int i=0; i < n; i++)
	printf("interface_%-10s\t%.2f\n", flp->units[i].name, temp[IFACE*n+i]-273.15);
      for (int i=0; i < n; i++)
	printf("spreader_%-11s\t%.2f\n", flp->units[i].name, temp[HSP*n+i]-273.15);
      printf("%-20s\t%.2f\n", "spreader_west", temp[NL*n+SP_W]-273.15);
      printf("%-20s\t%.2f\n", "spreader_east", temp[NL*n+SP_E]-273.15);
      printf("%-20s\t%.2f\n", "spreader_north", temp[NL*n+SP_N]-273.15);
      printf("%-20s\t%.2f\n", "spreader_south", temp[NL*n+SP_S]-273.15);
      printf("%-20s\t%.2f\n", "spreader_bottom", temp[NL*n+SP_B]-273.15);
      printf("%-20s\t%.2f\n", "sink_west", temp[NL*n+SINK_W]-273.15);
      printf("%-20s\t%.2f\n", "sink_east", temp[NL*n+SINK_E]-273.15);
      printf("%-20s\t%.2f\n", "sink_north", temp[NL*n+SINK_N]-273.15);
      printf("%-20s\t%.2f\n", "sink_south", temp[NL*n+SINK_S]-273.15);
      printf("%-20s\t%.2f\n", "sink_bottom", temp[NL*n+SINK_B]-273.15);
    }
    if_infile.clear();
    if_infile.seekg(0, ios::beg);
    if_conf.clear();
    if_conf.seekg(0,ios::beg);

    eNamesList.clear();
    sspotNamesList.clear();
    sescList.clear();

    //Store the list of Energy Labels (on the first line of the therm file) in enamesList
    
    getColNames(if_infile);
    

    //Store the list of Stats Labels (on the second line of the therm file) in sescList

    getSescNames(if_infile);
    
    //Parse the configuration file correlating the energy labels with their respecitve floorplan units
    //Initialize sspotNamesList Entries (but do not store values)
    parseConfigFile(0);
    
    if_infile.clear();
    if_infile.seekg(0,ios::beg);

    dump_tempTitles(flp, outfile);             
    readSescVals();	// we ignore the first set of values
    int iter = 0;;
    while (readSescVals() != -1) {
      cout << power[0] << "\t" << power[1] << "\t" << power[2] << endl;

      // Compute Temperatures
      compute_temp(power, temp, flp->n_units, (float)cycles/frequency);
      
      dump_tempAppend1(flp, temp, outfile);
      
      curcycle += (float)cycles;
    }
    return 0;
}

