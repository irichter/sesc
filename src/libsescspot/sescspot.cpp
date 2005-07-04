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
#include "RC.h"
#include "flp.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>

using namespace std;

// Data structure to hold values from the therm file
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
strList eNamesList;		// Energy names
strList sspotNamesList;	// sescspot names list
strList tmpList;		// Temprorary list
strList steadyList;		// Steady state values
strList sescList;		// SESC Stats list
strList warmList;		// Warmup Stats list
// simulator options
static char *flp_cfg = "sescspot.flp";			/* has the floorplan configuration	*/
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
double ambient = 40 + 273.15;	/* 45 C in kelvin	*/

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

// set up initial temperatures 
	if (steadyfile != NULL) {
		// initial T = steady T for no DTM
		if (!dtm_used)
			read_temp(flp, temp, steadyfile, 0);
		// initial T = clipped steady T with DTM
		else
			read_temp(flp, temp, steadyfile, 1);
	}
// no input file - use init_temp as the temperature
	else
		set_temp(temp, flp->n_units, init_temp);
}

/*
//Read the init/warmup file
void read_warmup()
{
  std::string line;
  
  getline(if_initfile, line); //read and discard line 1
  getline(if_initfile, line); //read and discard line 2

  items = 0;
  if (getline(if_initfile, line)) {
	std::istringstream linestream(line);
	std::string token;

	while (getline(linestream, token, '\t')) {
	  sscanf(token.c_str(), "%f", &tmp);
	  eNamesList[items].value = tmp;

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
	while (getline(linestream, token, '\t')) {
	  nn.name = token.c_str();
	  warmList.push_back(nn);
	}

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
// Read values from the therm file
*/

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

// Get the energy values  
	items = 0;
	if (getline(if_infile, line)) {
		std::istringstream linestream(line);
		std::string token;

		while (getline(linestream, token, '\t')) {
			sscanf(token.c_str(), "%f", &tmp);
			eNamesList[items].value = tmp;
			eNamesList[items].realoldValue = tmp;

			if (eNamesList[items].link.size() > 0) {
				for (int xx = 0; xx < (int)eNamesList[items].link.size(); xx++) {
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
	fprintf(fp, "%d\t", (int)curcycle);

// on chip temperatures	
	for (i=0; i < flp->n_units; i++)
		fprintf(fp, "%f\t", temp[i]);

// interface temperatures
	for (i=0; i < flp->n_units; i++)
		fprintf(fp, "%f\t", temp[IFACE*flp->n_units+i]);

// spreader temperatures	
	for (i=0; i < flp->n_units; i++)
		fprintf(fp, "%f\t", temp[HSP*flp->n_units+i]);

// internal node temperatures	
	for (i=0; i < EXTRA; i++) {
		sprintf(str, "inode_%d", i);
		fprintf(fp, "%f\t", temp[i+NL*flp->n_units]);
	}

	if (curcycle == 1)
		cyclenum = curcycle;
	else
		cyclenum = cycles;

	for (i = 0; i < (int)eNamesList.size(); i++)
		fprintf(fp, "%f\t", eNamesList[i].value);

	for (i = 0; i < (int)sescList.size(); i++)
		fprintf(fp, "%f\t", sescList[i].value/cyclenum);

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

	for (i = 0; i < (int)eNamesList.size(); i++) {
		fprintf(fp, "%s\t", eNamesList[i].name.c_str());
		printf("%s: %d\n", eNamesList[i].name.c_str(), x);
		x++;
	}

	for (i = 0; i < (int)sescList.size(); i++) {
		fprintf(fp, "%s\t", sescList[i].name.c_str());
		printf("%s: %d\n", sescList[i].name.c_str(), x);
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
	std::string token;

	if (getline(if_file, line)) {
		std::istringstream linestream(line);
		// std::string token;
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
void parseConfigFile(){

//Now correlate the Energy Labels (FULoad, FUStore etc)  with the Corresponding Floorplan Block (FPAdd, FPReg etc)
	namesList nn;
	std::vector<string> tokens;
	std::string tmpstring;

	nn.link.clear();
	nn.value = 0;
	nn.value1 = 0;
	nn.realoldValue = 0;
	nn.percent = 100;

	int items = 0;
	int firstitem = 0;
	nn.link.clear(); 

	//Find "[SescSpot]" in power.conf or other specified configuration file 
	while(getline(if_conf,tmpstring)){
		if((int)tmpstring.find("[SescSpot]",0) != -1)
			break;
	}
	for(int y=0; y<15 ; y++){
		
		    if(!getline(if_conf, tmpstring))
                fatal("Sescspot: Configuration File Missing Data");
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
            }
             else if(tokens[0] == "DTMUsed"){
                 dtm_used=atoi(tokens[1].c_str());
                cout << "DTMUsed: " << dtm_used << endl;
             }
              else if(tokens[0] == "ChipThickness"){
            	  t_chip=atof(tokens[1].c_str());
                  cout << "ChipThickness: " << t_chip << endl;
              }
			else if(tokens[0] == "DTMTempThreshhold"){
            	   thermal_threshold=atof(tokens[1].c_str());
                   cout << "DTMTempThreshhold: " << thermal_threshold << endl;
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
	while(getline(if_file, line)) {
		std::istringstream linestream(line);
		std::string token;
        float tmp;
        int items=0;
		while (getline(linestream, token, '\t')) {
			sscanf(token.c_str(), "%f", &tmp);
			eNamesList[items].value += tmp;
            items++;
        }
    }
    //reset the file pointer to the beginning of the file again (this is to handle the case where the warmupfile equals the input file
    if_file.seekg(0);

        for(int i=0;i< (int)eNamesList.size();i++){
            eNamesList[i].value/=eNamesList.size(); //calculate average power for each Element in eNamesList
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
	printf("-s <--steadyfile> Steady state file - If this option is specified, the values from this file will be used as steady state values, and sescspot will output a transient temperatures file\n");
	printf("-f <--floorplan> Floorplan file - Set to the floorplan you wish to use.  Default is 'ev6.flp'\n");
	printf("<--initfile> Warmup file - The warmup file is used to generate average power information for the heatsink.\n");
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

	curcycle = 1.0;

	std::string tmpstring;

	static struct option long_options[] =
	{ 
		{"infile", required_argument, 0, 'i'},
		{"warmfile", required_argument,   0, 'q'},
		{"outfile", required_argument, 0, 'o'},
		{"configfile", required_argument, 0, 'c'},
		{"steadyfile", required_argument, 0, 's'},
		{"floorplan", required_argument, 0, 'f'},
		{0,0,0,0}
	};
// process parameters
	while ((opt = getopt_long(argc, argv, "q:f:n:c:i:o:s:", long_options, &longindex)) != -1) {
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

//try to open the steady-state file (optional)
	if (steadyfile != NULL)
		if_steady.open(steadyfile, std::ifstream::in);

    if(!if_steady) {
        fatal("Cannot open steady file\n");
    }


    if(warmfile != NULL)
        if_warmfile.open(warmfile, std::ifstream::in);

    if(!if_warmfile){
           fatal("Cannot open warmup file\n");
    }



   //if we specified a warmup file, initial warmup stage
    if(warmfile != NULL){
    
        //Begin reading the warmup file (just the labels on the top two lines) 
             
        //Store the list of Energy Labels (on the first line of the therm file) in enamesList
        
        getColNames(if_warmfile);
             
        //Store the list of Stats Labels (on the second line of the therm file) in sescList
        
        getSescNames(if_warmfile);

        //Parse the configuration file correlating the energy labels with their respecitve floorplan units
        parseConfigFile();



        //Calculate the average energy values for each of the Energy labels, storing the data to enameslist[i].value entities
        calcAveragePower(if_warmfile);


        // Compute Temperatures
		compute_temp(power, temp, flp->n_units, (float)cycles/frequency);

        //calculate the total power
        for (x = 0; x < (int)sspotNamesList.size(); x++) {
            char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
            overall_power[get_blk_index(flp,tmpname)] += power[get_blk_index(flp, tmpname)];
            sspotNamesList[x].value = temp[get_blk_index(flp, tmpname)];
        }
    }
    //Begin reading the therm file (just the labels on the top two lines) 
             
    //Store the list of Energy Labels (on the first line of the therm file) in enamesList

    getColNames(if_infile);
         
    //Store the list of Stats Labels (on the second line of the therm file) in sescList

    getSescNames(if_infile);

         
    //Parse the configuration file correlating the energy labels with their respecitve floorplan units
     parseConfigFile();

     // already read one line of the infile, skip the next line.. only need to process every other line
	
	// if a steady state file isnt specified.. we know thats we are doing
	
	sim_init();
	if (steadyfile != NULL)
		dump_tempTitles(flp, outfile);
	readSescVals();	// we ignore the first set of values
	int iter = 0;;
	while (readSescVals() != -1) {
		iter++;
		// sspotNamesList has the current aggregate values for 10000 clock cycles
		// make sure #s are in floorplan order.. or we could have trouble
		for (x = 0; x < (int)sspotNamesList.size(); x++) {
			char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
			// Energy fix 11/29/04
			if (sspotNamesList[x].percent != 100) sspotNamesList[x].value = sspotNamesList[x].value * ((float)sspotNamesList[x].percent/100.0);
            //cout << sspotNamesList[x].value << endl;
			power[get_blk_index(flp, tmpname)] = sspotNamesList[x].value;
		}
		// Compute Temperatures
		compute_temp(power, temp, flp->n_units, (float)cycles/frequency);

		// lets go back through and dump the correct values back into sspotNamesList and then dump them


		for (x = 0; x < (int)sspotNamesList.size(); x++) {
			char  *tmpname = (char*)(sspotNamesList[x].name.c_str());
			overall_power[get_blk_index(flp,tmpname)] += power[get_blk_index(flp, tmpname)];
			sspotNamesList[x].value = temp[get_blk_index(flp, tmpname)];
		}
		if (steadyfile != NULL) { // we have real data.. so lets use it..
			dump_tempAppend1(flp, temp, outfile);
		}
		for (x = 0; x < (int)sspotNamesList.size(); x++)	power[x] = 0;
		curcycle += cycles;
	}

	for (x = 0; x < (int)sspotNamesList.size();x++) {
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

