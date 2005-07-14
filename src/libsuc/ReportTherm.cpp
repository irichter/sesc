/* Added 11/19/04 DVDB
   The original release notes for this file are below.  This was originally ReportGen.cpp

   Edited by David van der Bokke
*/
/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela

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

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <alloca.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <ctype.h>

#include "nanassert.h"
#include "ReportTherm.h"
#include "GProcessor.h"
#include "OSSim.h"
#include "GEnergy.h"

using namespace std;


FILE *ReportTherm::rfd[MAXREPORTSTACK];
int ReportTherm::cyclesPerSample=0; //number of cycles between stats/thermal dumps
int ReportTherm::tos=0;
int ReportTherm::rep=0;

StaticCallbackFunction0<ReportTherm::report> ReportTherm::reportCB;

//----------------------------------------------------------------------------
// Tokenize : Split up a string based upon delimiteres, defaults to space-deliminted (like strok())
//
void Tokenize(const std::string& str,std::vector<string>& tokens,const std::string& delimiters)
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

ReportTherm::ReportTherm()
{
  rfd[0]=stdout;
  tos=1;
}

//  Initializes the report file if this is the first write Dumps Energy and
//  Statistics to the thermal report file
void ReportTherm::report() 
{
  if (rep == 0) {
    GStatsEnergy::setupDump(0);
    ReportTherm::field("\n");
    ReportTherm::flush();
    rep = 1;
  }

  GStatsEnergy::printDump(0, 5);
  ReportTherm::field("\n");
  ReportTherm::flush();

  if (rep == 1)
    reportCB.schedule(ReportTherm::cyclesPerSample); //Schedule dumps every so many cycles
}

// Stops the callback
void ReportTherm::stopCB() 
{
  rep = 2;
}

void ReportTherm::openFile(char *name)
{
  I(tos<MAXREPORTSTACK);

  FILE *ffd1;
  rep = 0;
  if(strstr(name, "XXXXXX")) {
    int fd;
    
    fd = mkstemp(name);
    ffd1 = fdopen(fd, "a");
  }else{
    ffd1 = fopen(name, "a");
  }
  if(ffd1 == 0) {
    fprintf(stderr, "NANASSERT::REPORT could not open temporal file [%s]\n", name);
    exit(-3);
  }

  rfd[tos++]=ffd1;
  reportCB.schedule(1);

//We attempt to open sescspot.sspot to obtain frequency information
//At this point, we assume that the configuration file is sescspot.sspot
std::ifstream if_conf;
if_conf.open("sescspot.sspot", std::ifstream::in);
if (!if_conf)
	fatal("Cannot open \"sescspot.sspot\" thermal configuration file.");
std::string tmpstring;
std::vector<string> tokens;
while(getline(if_conf,tmpstring)){
	if((int)tmpstring.find("[SescSpot]",0) != -1)
		break;
}
if(if_conf.eof())
	fatal("\"sescspot.sspot\" is missing configuration data.");


while(getline(if_conf,tmpstring)){
	if(tmpstring.empty())
		continue; //ignore blank lines
	Tokenize(tmpstring, tokens, " \t");
	if(tokens[0] == "#" || tokens[1] == "#"){
		continue; //skip comments
	}
	if(tokens[0] == "CyclesPerSample"){
		ReportTherm::cyclesPerSample=atoi(tokens[1].c_str());
		break; //end after the value was found
	}
}
if(ReportTherm::cyclesPerSample==0){
	clog << "Could Not Find \"CyclesPerSample\" Data in \"sescspot.sspot\"" << endl;
	clog << "Default to 10,000 Cycles Between Dumps" << endl;
	ReportTherm::cyclesPerSample=10000;
}

if_conf.close();

}
void ReportTherm::close()
{
  rep = 2;
  while( tos ) {
    printf(".");
    tos--;
    fclose(rfd[tos]);
  }
}

void ReportTherm::field(int fn, const char *format,...)
{
  va_list ap;

  I( fn < tos );
  FILE *ffd = rfd[fn];
  
  va_start(ap, format);

  vfprintf(ffd, format, ap);

  va_end(ap);

  //fprintf(ffd, "\n");
}

void ReportTherm::field(const char *format, ...)
{
  va_list ap;

  I( tos );
  FILE *ffd = rfd[tos-1];
  
  va_start(ap, format);

  vfprintf(ffd, format, ap);

  va_end(ap);

  //fprintf(ffd, "\n");
}

/*
void ReportTherm::addLine(char *str) {
  FILE *ffd = rfd[tos-1];
  fprintf(ffd, "%s\n", str);
  }*/

void ReportTherm::flush()
{
  if( tos == 0 )
    return;
  
  fflush(rfd[tos-1]);
}


