%{
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Config.h"

  extern int CFlineno; /* Defined in conflex.l */

  const char *blockName=""; /* No name by default */

  Config *configptr;
  bool errorFound = false;

  /* To avoid warnings at compile time */
  int  yyConflex();
  int  yyConfparse();
  void yyConferror(const char *txt);

%}

%union {
  bool Bool;
  long Long;
  double Double;
  const char *CharPtr;
}

%type <Bool>    CFBOOL
%type <Long>    CFLONG lexp
%type <Double>  CFDOUBLE dexp
%type <CharPtr> CFQSTRING CFNAME CFNAMEREF CFSTRNAMEREF texp

%token CFDOUBLE
%token CFLONG
%token CFBOOL
%token CFQSTRING
%token CFNAME
%token CFNAMEREF
%token CFSTRNAMEREF

%token CFOB
%token CFCB
%token CFEQUAL
%token CFDDOT
%token CFOP
%token CFCP
%token CFPLUS
%token CFMINUS
%token CFMULT
%token CFDIV
%token CFEXP

%left CFMINUS CFPLUS
%left CFMULT CFDIV
%right CFEXP    /* exponentiation        */

%%

initial: /* empty */
         | initial rules
        ;


rules:
 CFOB CFNAME CFCB  { blockName=$2; }
|
 CFNAME CFEQUAL texp    { configptr->addRecord(blockName,$1,$3); }
|
 CFNAME CFEQUAL CFQSTRING { configptr->addRecord(blockName,$1,$3); }
|
 CFNAME CFEQUAL CFNAME    { configptr->addRecord(blockName,$1,$3); }
|
 CFNAME CFEQUAL CFBOOL    { configptr->addRecord(blockName,$1,$3); }
|
 CFNAME CFEQUAL lexp      { configptr->addRecord(blockName,$1,$3); }
|
 CFNAME CFEQUAL dexp      { configptr->addRecord(blockName,$1,$3); }
|
 CFNAME CFOB lexp CFCB CFEQUAL CFQSTRING { configptr->addVRecord(blockName,$1,$6,$3,$3); }
|
 CFNAME CFOB lexp CFDDOT lexp CFCB CFEQUAL CFQSTRING { configptr->addVRecord(blockName,$1,$8,$3,$5); }
;


texp:  CFSTRNAMEREF            { $$ = configptr->getCharPtr("",$1); configptr->isCharPtr("",$1); }


lexp:     CFLONG               { $$ = $1; }
        | CFNAMEREF            { $$ = configptr->getLong("",$1); configptr->isLong("",$1); }
        | lexp CFPLUS  lexp    { $$ = $1 + $3;    }
        | lexp CFMINUS lexp    { $$ = $1 - $3;    }
        | lexp CFMULT  lexp    { $$ = $1 * $3;    }
        | lexp CFDIV   lexp    { $$ = $1 / $3;    }
        | lexp CFEXP   lexp    { $$ = static_cast<long>(pow((double)$1, (double)$3)); }
        | CFOP lexp CFCP       { $$ = $2; }
        | CFMINUS lexp  %prec CFMINUS { $$ = -$2; }
;

dexp:      CFDOUBLE        { $$ = $1; }
/*        | CFNAMEREF              { $$ = configptr->getLong("",$1); configptr->isLong("",$1); } */
        | dexp CFPLUS dexp       { $$ = $1 + $3; }
        | lexp CFPLUS dexp       { $$ = $1   + $3; }
        | dexp CFPLUS lexp       { $$ = $1 + $3;   }

        | dexp CFMINUS dexp      { $$ = $1 - $3; }
        | lexp CFMINUS dexp      { $$ = $1   - $3; }
        | dexp CFMINUS lexp      { $$ = $1 - $3;   }

        | dexp CFMULT dexp       { $$ = $1 * $3; }
        | lexp CFMULT dexp       { $$ = $1   * $3; }
        | dexp CFMULT lexp       { $$ = $1 * $3;   }

        | dexp CFDIV dexp        { $$ = $1 / $3; }
        | lexp CFDIV dexp        { $$ = $1   / $3; }
        | dexp CFDIV lexp        { $$ = $1 / $3;   }

        | dexp CFEXP dexp        { $$ = pow ($1, $3); }
        | lexp CFEXP dexp        { $$ = pow ((double)$1, $3); }
        | dexp CFEXP lexp        { $$ = pow ($1, (double)$3);   }

        | CFOP dexp CFCP         { $$ = $2; }

        | CFMINUS dexp  %prec CFMINUS { $$ = -$2;  }
;


%%

/* Used by flex "conflex.l" */
extern FILE *yyConfin, *yyConfout;
extern const char *currentFile;

void yyConferror(const char *txt)
{
	fprintf(stderr,"conflex:<%s> Line %d error:%s\n",currentFile,CFlineno,txt);

	errorFound=true;
}

bool readConfigFile(Config *ptr,FILE *fp,const char *fpname)
{
	yyConfin = fp;

	configptr  = ptr;
	errorFound = false;

	currentFile = fpname;
	
	yyConfparse();

	return !errorFound;
}
