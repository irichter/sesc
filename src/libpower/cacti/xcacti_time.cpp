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

#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "xcacti_def.h"
#include "xcacti_area.h"

double Cwordmetal=-1;
double Cbitmetal=-1;
double Rwordmetal=-1;
double Rbitmetal=-1;
double FACwordmetal=-1;
double FACbitmetal=-1;
double FARwordmetal=-1;
double FARbitmetal=-1;

static int muxover=-1;
static double VddPow=-1;
static double VbitprePow=-1;

/*----------------------------------------------------------------------*/

static int powers (int base, int n) {
  int i,p;

  p = 1;
  for (i = 1; i <= n; ++i)
     p *= base;
  return p;
}

/*----------------------------------------------------------------------*/

static double logtwo(double x)
{
  if (x<=0) 
    printf("ERROR: negative logtwo %e\n",x);

  return( (double) (log(x)/log(2.0)) );
}
/*----------------------------------------------------------------------*/

 /* returns gate capacitance in Farads */
static double gatecap(double width,           /* gate width in um (length is Leff) */
		      double wirelength)      /* poly wire length going to gate in lambda */
{
  return(width*Leff*Cgate+wirelength*Cpolywire*Leff);
}

/* returns gate capacitance in Farads */
static double gatecappass(double width,           /* gate width in um (length is Leff) */
			  double wirelength)      /* poly wire length going to gate in lambda */
{
  return(width*Leff*Cgatepass+wirelength*Cpolywire*Leff);
}


/*----------------------------------------------------------------------*/

/* Routine for calculating drain capacitances.  The draincap routine
 * folds transistors larger than 10um */

  /* returns drain cap in Farads */
static double draincap(double width,           /* um */
		       int nchannel,           /* whether n or p-channel (boolean) */
		       int stack)              /* number of transistors in series that are on */
{
  double Cdiffside,Cdiffarea,Coverlap,cap;

  Cdiffside = (nchannel) ? Cndiffside : Cpdiffside;
  Cdiffarea = (nchannel) ? Cndiffarea : Cpdiffarea;
  Coverlap = (nchannel) ? (Cndiffovlp+Cnoxideovlp) :
    (Cpdiffovlp+Cpoxideovlp);
  /* calculate directly-connected (non-stacked) capacitance */
  /* then add in capacitance due to stacking */
  if (width >= 10) {
    cap = 3.0*Leff*width/2.0*Cdiffarea + 6.0*Leff*Cdiffside +
      width*Coverlap;
    cap += (double)(stack-1)*(Leff*width*Cdiffarea +
			      4.0*Leff*Cdiffside + 2.0*width*Coverlap);
  } else {
    cap = 3.0*Leff*width*Cdiffarea + (6.0*Leff+width)*Cdiffside +
      width*Coverlap;
    cap += (double)(stack-1)*(Leff*width*Cdiffarea +
			      2.0*Leff*Cdiffside + 2.0*width*Coverlap);
  }
  return(cap);
}

/*----------------------------------------------------------------------*/

/* The following routines estimate the effective resistance of an
   on transistor as described in the tech report.  The first routine
   gives the "switching" resistance, and the second gives the 
   "full-on" resistance */

/* returns resistance in ohms */
static double transresswitch(double width,           /* um */
			     int nchannel,           /* whether n or p-channel (boolean) */
			     int stack)              /* number of transistors in series */
{
  double restrans;
  restrans = (nchannel) ? (Rnchannelstatic):
    (Rpchannelstatic);
  /* calculate resistance of stack - assume all but switching trans
     have 0.8X the resistance since they are on throughout switching */
  return((1.0+((stack-1.0)*0.8))*restrans/width); 
}

/*----------------------------------------------------------------------*/

/* returns resistance in ohms */
static double transreson(double width,           /* um */
			 int nchannel,           /* whether n or p-channel (boolean) */
			 int stack)              /* number of transistors in series */
{
  double restrans;
  restrans = (nchannel) ? Rnchannelon : Rpchannelon;

  /* calculate resistance of stack.  Unlike transres, we don't
     multiply the stacked transistors by 0.8 */
  return(stack*restrans/width);
}

/*----------------------------------------------------------------------*/

/* This routine operates in reverse: given a resistance, it finds
 * the transistor width that would have this R.  It is used in the
 * data wordline to estimate the wordline driver size. */

/* returns width in um */
static double restowidth(double res,            /* resistance in ohms */
			 int nchannel)          /* whether N-channel or P-channel */
{
  double restrans;

  restrans = (nchannel) ? Rnchannelon : Rpchannelon;

  return(restrans/res);
}

/*----------------------------------------------------------------------*/

static double horowitz(double inputramptime,    /* input rise time */
		       double tf,               /* time constant of gate */
		       double vs1,
		       double vs2,              /* threshold voltages */
		       int rise)                /* whether INPUT rise or fall (boolean) */
{
  double a,b,td;

  a = inputramptime/tf;
  if (rise==RISE) {
    b = 0.5;
    td = tf*sqrt( log(vs1)*log(vs1)+2*a*b*(1.0-vs1)) +
      tf*(log(vs1)-log(vs2));
  } else {
    b = 0.4;
    td = tf*sqrt( log(1.0-vs1)*log(1.0-vs1)+2*a*b*(vs1)) +
      tf*(log(1.0-vs1)-log(1.0-vs2));
  }
  return(td);
}


/*======================================================================*/



/* 
 * This part of the code contains routines for each section as
 * described in the tech report.  See the tech report for more details
 * and explanations */

/*----------------------------------------------------------------------*/

static void subbank_routing_length(int C, int B, int A,
				   char fullyassoc,
				   int Ndbl, int Nspd, int Ndwl,
				   int Ntbl, int Ntwl, int Ntspd,
				   int NSubbanks,
				   double *subbank_v, 
				   double *subbank_h)
{
  double htree;
  int htree_int, tagbits;
  int cols_data_subarray,rows_data_subarray, cols_tag_subarray,rows_tag_subarray;
  double inter_v,inter_h ,sub_h, sub_v;
  double inter_subbanks;
  int cols_fa_subarray,rows_fa_subarray;

  if (!fullyassoc) {
 
    cols_data_subarray = (8*B*A*Nspd/Ndwl);
    rows_data_subarray = (C/(B*A*Ndbl*Nspd));

    if(Ndwl*Ndbl==1) {
      sub_v= rows_data_subarray;
      sub_h= cols_data_subarray;
    }
    if(Ndwl*Ndbl==2) {
      sub_v= rows_data_subarray;
      sub_h= 2*cols_data_subarray;
    }

    if(Ndwl*Ndbl>2) {
      htree=logtwo((double)(Ndwl*Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	sub_v = sqrt(Ndwl*Ndbl)*rows_data_subarray;
	sub_h = sqrt(Ndwl*Ndbl)*cols_data_subarray;
      }
      else {
	sub_v = sqrt(Ndwl*Ndbl/2)*rows_data_subarray;
	sub_h = 2*sqrt(Ndwl*Ndbl/2)*cols_data_subarray;
      }
    }
    inter_v=sub_v;
    inter_h=sub_h;

    rows_tag_subarray = C/(B*A*Ntbl*Ntspd);
    tagbits = ADDRESS_BITS+2-(int)logtwo((double)C)+(int)logtwo((double)A)-(int)(logtwo(NSubbanks));

    cols_tag_subarray = tagbits*A*Ntspd/Ntwl ;

    if(Ntwl*Ntbl==1) {
      sub_v= rows_tag_subarray;
      sub_h= cols_tag_subarray;
    }
    if(Ntwl*Ntbl==2) {
      sub_v= rows_tag_subarray;
      sub_h= 2*cols_tag_subarray;
    }

    if(Ntwl*Ntbl>2) {
      htree=logtwo((double)(Ndwl*Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	sub_v = sqrt(Ndwl*Ndbl)*rows_tag_subarray;
	sub_h = sqrt(Ndwl*Ndbl)*cols_tag_subarray;
      }
      else {
	sub_v = sqrt(Ndwl*Ndbl/2)*rows_tag_subarray;
	sub_h = 2*sqrt(Ndwl*Ndbl/2)*cols_tag_subarray;
      }
    }


    inter_v=MAX(sub_v,inter_v);
    inter_h+=sub_h;

    sub_v=0;
    sub_h=0;

    if(NSubbanks==1.0 || NSubbanks==2.0 ) {
      sub_h = 0;
      sub_v = 0;
    }
    if(NSubbanks==4.0) {
      sub_h = 0;
      sub_v = inter_v;
    }

    inter_subbanks=NSubbanks;

    while((inter_subbanks > 2.0) && (NSubbanks > 4.0))
      {

	sub_v+=inter_v;
	sub_h+=inter_h;

	inter_v=2*inter_v;
	inter_h=2*inter_h;
	inter_subbanks=inter_subbanks/4.0;

	if(inter_subbanks==4.0) {
	  inter_h = 0; }

      }
    *subbank_v=sub_v;
    *subbank_h=sub_h;
  }
  else {
    rows_fa_subarray = (C/(B*Ndbl));
    tagbits = ADDRESS_BITS+2-(int)logtwo((double)B);
    cols_fa_subarray = (8*B)+tagbits;

    if(Ndbl==1) {
      sub_v= rows_fa_subarray;
      sub_h= cols_fa_subarray;
    }
    if(Ndbl==2) {
      sub_v= rows_fa_subarray;
      sub_h= 2*cols_fa_subarray;
    }

    if(Ndbl>2) {
      htree=logtwo((double)(Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	sub_v = sqrt(Ndbl)*rows_fa_subarray;
	sub_h = sqrt(Ndbl)*cols_fa_subarray;
      }
      else {
	sub_v = sqrt(Ndbl/2)*rows_fa_subarray;
	sub_h = 2*sqrt(Ndbl/2)*cols_fa_subarray;
      }
    }
    inter_v=sub_v;
    inter_h=sub_h;

    sub_v=0;
    sub_h=0;

    if(NSubbanks==1.0 || NSubbanks==2.0 ) {
      sub_h = 0;
      sub_v = 0;
    }
    if(NSubbanks==4.0) {
      sub_h = 0;
      sub_v = inter_v;
    }

    inter_subbanks=NSubbanks;

    while((inter_subbanks > 2.0) && (NSubbanks > 4.0))
      {

	sub_v+=inter_v;
	sub_h+=inter_h;

	inter_v=2*inter_v;
	inter_h=2*inter_h;
	inter_subbanks=inter_subbanks/4.0;

	if(inter_subbanks==4.0) {
	  inter_h = 0; }

      }
    *subbank_v=sub_v;
    *subbank_h=sub_h;
  }
}

static void subbank_dim(int C, int B, int A,
			char fullyassoc,
			int Ndbl, int Ndwl, int Nspd, 
			int Ntbl, int Ntwl, int Ntspd,
			int NSubbanks,
			double *subbank_h,
			double *subbank_v)
{
  double htree;
  int htree_int, tagbits;
  int cols_data_subarray,rows_data_subarray,cols_tag_subarray,rows_tag_subarray;
  double sub_h, sub_v, inter_v, inter_h;
  int cols_fa_subarray,rows_fa_subarray;

  if (!fullyassoc) {

    /* calculation of subbank dimensions */
    cols_data_subarray = (8*B*A*Nspd/Ndwl);
    rows_data_subarray = (C/(B*A*Ndbl*Nspd));

    if(Ndwl*Ndbl==1) {
      sub_v= rows_data_subarray;
      sub_h= cols_data_subarray;
    }
    if(Ndwl*Ndbl==2) {
      sub_v= rows_data_subarray;
      sub_h= 2*cols_data_subarray;
    }
    if(Ndwl*Ndbl>2) {
      htree=logtwo((double)(Ndwl*Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	sub_v = sqrt(Ndwl*Ndbl)*rows_data_subarray;
	sub_h = sqrt(Ndwl*Ndbl)*cols_data_subarray;
      }
      else {
	sub_v = sqrt(Ndwl*Ndbl/2)*rows_data_subarray;
	sub_h = 2*sqrt(Ndwl*Ndbl/2)*cols_data_subarray;
      }
    }
    inter_v=sub_v;
    inter_h=sub_h;

    rows_tag_subarray = C/(B*A*Ntbl*Ntspd);

    tagbits = ADDRESS_BITS+2-(int)logtwo((double)C)+(int)logtwo((double)A)-(int)(logtwo(NSubbanks));
    cols_tag_subarray = tagbits*A*Ntspd/Ntwl ;

    if(Ntwl*Ntbl==1) {
      sub_v= rows_tag_subarray;
      sub_h= cols_tag_subarray;
    }
    if(Ntwl*Ntbl==2) {
      sub_v= rows_tag_subarray;
      sub_h= 2*cols_tag_subarray;
    }

    if(Ntwl*Ntbl>2) {
      htree=logtwo((double)(Ndwl*Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	sub_v = sqrt(Ndwl*Ndbl)*rows_tag_subarray;
	sub_h = sqrt(Ndwl*Ndbl)*cols_tag_subarray;
      }
      else {
	sub_v = sqrt(Ndwl*Ndbl/2)*rows_tag_subarray;
	sub_h = 2*sqrt(Ndwl*Ndbl/2)*cols_tag_subarray;
      }
    }

    inter_v=MAX(sub_v,inter_v);
    inter_h+=sub_h;

    *subbank_v=inter_v;
    *subbank_h=inter_h;
  }

  else {
    rows_fa_subarray = (C/(B*Ndbl));
    tagbits = ADDRESS_BITS+2-(int)logtwo((double)B);
    cols_fa_subarray = (8*B)+tagbits;

    if(Ndbl==1) {
      sub_v= rows_fa_subarray;
      sub_h= cols_fa_subarray;
    }
    if(Ndbl==2) {
      sub_v= rows_fa_subarray;
      sub_h= 2*cols_fa_subarray;
    }

    if(Ndbl>2) {
      htree=logtwo((double)(Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	sub_v = sqrt(Ndbl)*rows_fa_subarray;
	sub_h = sqrt(Ndbl)*cols_fa_subarray;
      }
      else {
	sub_v = sqrt(Ndbl/2)*rows_fa_subarray;
	sub_h = 2*sqrt(Ndbl/2)*cols_fa_subarray;
      }
    }
    inter_v=sub_v;
    inter_h=sub_h;

    *subbank_v=inter_v;
    *subbank_h=inter_h;
  }
}


static void subbanks_routing_power(char fullyassoc,
				   int A,
				   int  NSubbanks,
				   double *subbank_h,
				   double *subbank_v,
				   double *power)
{
  double Ceq,Ceq_outdrv;
  int i,blocks,htree_int,subbank_mod;
  double htree, wire_cap, wire_cap_outdrv, start_h,start_v,line_h,line_v;
        
  *power=0.0;

  if(fullyassoc) {
    A=1;
  }

  if(NSubbanks==1.0 || NSubbanks==2.0) {
    *power=0.0;
  }

  if(NSubbanks==4.0) {
    /* calculation of address routing power */
    wire_cap = Cbitmetal*(*subbank_v);
    Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
      gatecap(Wdecdrivep+Wdecdriven,0.0);
    *power+=2*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;
    Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
      wire_cap;
    *power+=2*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;

    /* calculation of out driver power */
    wire_cap_outdrv = Cbitmetal*(*subbank_v);
    Ceq_outdrv= draincap(Wsenseextdrv1p,PCH,1) + draincap(Wsenseextdrv1n,NCH,1) + gatecap(Wsenseextdrv2n+Wsenseextdrv2p,10.0);
    *power+=2*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;
    Ceq_outdrv= draincap(Wsenseextdrv2p,PCH,1) + draincap(Wsenseextdrv2n,NCH,1) + wire_cap_outdrv;
    *power+=2*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;

  }
        
  if(NSubbanks==8.0) {
    wire_cap = Cbitmetal*(*subbank_v)+Cwordmetal*(*subbank_h);
    /* buffer stage */
    Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
      gatecap(Wdecdrivep+Wdecdriven,0.0);
    *power+=6*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;
    Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
      wire_cap;
    *power+=4*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;
    Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
      (wire_cap-Cbitmetal*(*subbank_v));
    *power+=2*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;

    /* calculation of out driver power */
    wire_cap_outdrv = Cbitmetal*(*subbank_v)+Cwordmetal*(*subbank_h);
    Ceq_outdrv= draincap(Wsenseextdrv1p,PCH,1) + draincap(Wsenseextdrv1n,NCH,1) + gatecap(Wsenseextdrv2n+Wsenseextdrv2p,10.0);
    *power+=6*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;
    Ceq_outdrv= draincap(Wsenseextdrv2p,PCH,1) + draincap(Wsenseextdrv2n,NCH,1) + wire_cap_outdrv;
    *power+=4*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;
    Ceq_outdrv= draincap(Wsenseextdrv2p,PCH,1) + draincap(Wsenseextdrv2n,NCH,1) + (wire_cap_outdrv- Cbitmetal*(*subbank_v));
    *power+=2*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;

  }
 
  if(NSubbanks > 8.0) {
    blocks=(int) (NSubbanks/8.0);
    htree=logtwo((double)(blocks));
    htree_int = (int) htree;
    if (htree_int % 2 ==0) { 
      subbank_mod = htree_int; 
      start_h=(*subbank_h*(powers(2,((int)(logtwo(htree)))+1)-1));
      start_v=*subbank_v; 
    }
    else {
      subbank_mod = htree_int -1;
      start_h=(*subbank_h*(powers(2,(htree_int+1)/2)-1));
      start_v=*subbank_v; 
    }

    if(subbank_mod==0) {
      subbank_mod=1;
    }

    line_v= start_v;
    line_h= start_h;

    for(i=1;i<=blocks;i++) {
      wire_cap = line_v*Cbitmetal+line_h*Cwordmetal;
 
      Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
	gatecap(Wdecdrivep+Wdecdriven,0.0);
      *power+=6*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;
      Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
	wire_cap;
      *power+=4*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;
      Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
	(wire_cap-Cbitmetal*(*subbank_v));
      *power+=2*ADDRESS_BITS*Ceq*.5*VddPow*VddPow;

      /* calculation of out driver power */
      wire_cap_outdrv = line_v*Cbitmetal+line_h*Cwordmetal;

      Ceq_outdrv= draincap(Wsenseextdrv1p,PCH,1) + draincap(Wsenseextdrv1n,NCH,1) + gatecap(Wsenseextdrv2n+Wsenseextdrv2p,10.0);
      *power+=6*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;
      Ceq_outdrv= draincap(Wsenseextdrv2p,PCH,1) + draincap(Wsenseextdrv2n,NCH,1) + wire_cap_outdrv;
      *power+=4*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;
      Ceq_outdrv= draincap(Wsenseextdrv2p,PCH,1) + draincap(Wsenseextdrv2n,NCH,1) + (wire_cap_outdrv- Cbitmetal*(*subbank_v));
      *power+=2*Ceq_outdrv*VddPow*VddPow*.5*BITOUT*A*muxover;

      if(i % subbank_mod ==0) {
	line_v+=2*(*subbank_v);
      }
    }
  }
}

static double address_routing_delay(int C, int B, int A,
				    char fullyassoc,
				    int Ndwl, int Ndbl, int Nspd,
				    int Ntwl, int Ntbl, int Ntspd,
				    int    NSubbanks,
				    double *outrisetime,
				    double *power)
{
  double Ceq,tf,nextinputtime,delay_stage1,delay_stage2;
  double addr_h,addr_v;
  double wire_cap, wire_res;

  /* Calculate rise time.  Consider two inverters */

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(0.0,tf,VTHINV360x240,VTHINV360x240,FALL)/
    (VTHINV360x240);

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,
			   RISE)/
    (1.0-VTHINV360x240);
  addr_h=0; addr_v=0;
  subbank_routing_length(C,B,A,fullyassoc,Ndbl,Nspd,Ndwl,Ntbl,Ntwl,Ntspd,NSubbanks,&addr_v,&addr_h);
  wire_cap = Cbitmetal*addr_v+addr_h*Cwordmetal;
  wire_res = (Rwordmetal*addr_h + Rbitmetal*addr_v)/2.0;

  /* buffer stage */

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  delay_stage1 = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,FALL);
  nextinputtime = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,FALL)/(VTHINV360x240);
  *power=ADDRESS_BITS*Ceq*.5*VddPow*VddPow;

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    wire_cap;
  tf = Ceq*(transreson(Wdecdriven,NCH,1)+wire_res);
  delay_stage2=horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,RISE);
  nextinputtime = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,RISE)/(1.0-VTHINV360x240);
  *power+=ADDRESS_BITS*Ceq*.5*VddPow*VddPow;
  *outrisetime = nextinputtime;
  return(delay_stage1+delay_stage2);

}



/* Decoder delay:  (see section 6.1 of tech report) */

static double decoder_delay(int C, int B, int A,
			    int Ndwl, int Ndbl, int Nspd,
			    int Ntwl, int Ntbl, int Ntspd,
			    double *Tdecdrive,
			    double *Tdecoder1,
			    double *Tdecoder2,
			    double *outrisetime,
			    double inrisetime,
			    int *nor_inputs,
			    double *power)
{
  int numstack;
  double Ceq,Req,Rwire,rows,cols,tf,nextinputtime,vth;
  double l_inv_predecode, l_predec_nor_v, l_predec_nor_h; 
  double wire_cap, wire_res, total_edge_length;
  double htree;
  int htree_int,exp;

  int addr_bits=(int)logtwo((double)((double)C/(double)(B*A*Ndbl*Nspd)));
  /* Calculate rise time.  Consider two inverters */
#ifdef XCACTI
  if (PHASEDCACHE) {
    C=C/A;
    A=1;
  }
#endif

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(0.0,tf,VTHINV360x240,VTHINV360x240,FALL)/
    (VTHINV360x240);
  //      power+=Ceq*.5*VddPow;

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,
			   RISE)/
    (1.0-VTHINV360x240);
  //      power+=Ceq*.5*VddPow;

  /* First stage: driving the decoders */

  rows = C/(8*B*A*Ndbl*Nspd);
  cols = 8*B*A*Nspd/Ndwl;
  total_edge_length = 8*B*A*Ndbl*Nspd ;
        
  if(Ndwl*Ndbl==1 ) {
    l_inv_predecode = 1;
    wire_cap = Cwordmetal*l_inv_predecode*total_edge_length;
    wire_res = 0.5*Rwordmetal*l_inv_predecode*total_edge_length;

  }
  if(Ndwl*Ndbl==2 ) {
    l_inv_predecode = 0.5;
    wire_cap = Cwordmetal*l_inv_predecode*total_edge_length;
    wire_res = 0.5*Rwordmetal*l_inv_predecode*total_edge_length;
  }
  if(Ndwl*Ndbl>2) { 
    htree=(int)logtwo((double)(Ndwl*Ndbl));
    htree_int = (int) htree;
    if (htree_int % 2 ==0) {
      exp = (htree_int/2-1);
      l_inv_predecode = 0.25/(powers(2,exp));
      wire_cap = Cwordmetal*l_inv_predecode*total_edge_length;
      wire_res = 0.5*Rwordmetal*l_inv_predecode*total_edge_length;
    }
    else {
      exp = (htree_int-3)/2;
      l_inv_predecode = powers(2,exp);
      wire_cap = Cbitmetal*l_inv_predecode*rows*8;
      wire_res = 0.5*Rbitmetal*l_inv_predecode*rows*8;
    }
  }
        
  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    4*gatecap(Wdec3to8n+Wdec3to8p,10.0)*(Ndwl*Ndbl)
    +wire_cap;
  Rwire = wire_res;
 
  tf = (Rwire + transreson(Wdecdrivep,PCH,1))*Ceq;
  *Tdecdrive = horowitz(inrisetime,tf,VTHINV360x240,VTHNAND60x120,
			FALL);

  nextinputtime = *Tdecdrive/VTHNAND60x120;
  *power+=addr_bits*Ceq*.5*VddPow*VddPow;

  /* second stage: driving a bunch of nor gates with a nand */

  numstack = (int)ceil((1.0/3.0)*logtwo( (double)((double)C/(double)(B*A*Ndbl*Nspd))));
  if (numstack==0) numstack = 1;
  if (numstack>5) numstack = 5;

  if(Ndwl*Ndbl==1 || Ndwl*Ndbl==2 || Ndwl*Ndbl==4) {
    l_predec_nor_v = 0;
    l_predec_nor_h = 0;
  }
  else {
    if(Ndwl*Ndbl==8) {
      l_predec_nor_v = 0;
      l_predec_nor_h = cols;
    }
  }

  if(Ndwl*Ndbl>8) {
    htree=(int)logtwo((double)(Ndwl*Ndbl));
    htree_int = (int) htree;
    if (htree_int % 2 ==0) {
      exp = (htree_int/2-1);
      l_predec_nor_v = (powers(2,exp)-1)*rows*8;
      l_predec_nor_h = (powers(2,exp)-1)*cols;
    }
    else {
      exp = (htree_int-3)/2;
      l_predec_nor_v = (powers(2,exp)-1)*rows*8;
      l_predec_nor_h = (powers(2,(exp+1))-1)*cols;
    }
  }

  Ceq = 3*draincap(Wdec3to8p,PCH,1) +draincap(Wdec3to8n,NCH,3) +
    gatecap(WdecNORn+WdecNORp,((numstack*40)+20.0))*rows +
    Cbitmetal*(rows*8+l_predec_nor_v)+Cwordmetal*(l_predec_nor_h);
  Rwire = Rbitmetal*(rows*8+l_predec_nor_v)/2 + Rwordmetal*(l_predec_nor_h)/2;
        
  tf = Ceq*(Rwire+transreson(Wdec3to8n,NCH,3)); 
  *power+=Ndwl*Ndbl*Ceq*VddPow*VddPow*4*ceil((1.0/3.0)*logtwo( (double)((double)C/(double)(B*A*Ndbl*Nspd))));

  /* we only want to charge the output to the threshold of the
     nor gate.  But the threshold depends on the number of inputs
     to the nor.  */

  switch(numstack) {
  case 1: vth = VTHNOR12x4x1; break;
  case 2: vth = VTHNOR12x4x2; break;
  case 3: vth = VTHNOR12x4x3; break;
  case 4: vth = VTHNOR12x4x4; break;
  case 5: vth = VTHNOR12x4x4; break;
  default: printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;
  }
  *Tdecoder1 = horowitz(nextinputtime,tf,VTHNAND60x120,vth,RISE);
  nextinputtime = *Tdecoder1/(1.0-vth);

  /* Final stage: driving an inverter with the nor */

  Req = transreson(WdecNORp,PCH,numstack);
  Ceq = (gatecap(Wdecinvn+Wdecinvp,20.0)+
	 numstack*draincap(WdecNORn,NCH,1)+
	 draincap(WdecNORp,PCH,numstack));
  tf = Req*Ceq;
  *Tdecoder2 = horowitz(nextinputtime,tf,vth,VSINV,FALL);

  *outrisetime = *Tdecoder2/(VSINV);
  *nor_inputs=numstack;
  *power+=Ceq*VddPow*VddPow;
        
  //printf("%g %g %g %d %d %d\n",*Tdecdrive,*Tdecoder1,*Tdecoder2,Ndwl, Ndbl,Nspd);

  //fprintf(stderr, "%f %f %f %f %d %d %d\n", (*Tdecdrive+*Tdecoder1+*Tdecoder2)*1e9, *Tdecdrive*1e9, *Tdecoder1*1e9, *Tdecoder2*1e9, Ndwl, Ndbl, Nspd);
  return(*Tdecdrive+*Tdecoder1+*Tdecoder2);
}



/*----------------------------------------------------------------------*/

/* Decoder delay in the tag array (see section 6.1 of tech report) */


static double decoder_tag_delay(int C, int B, int A,
				int Ndwl, int Ndbl, int Nspd,
				int Ntwl, int Ntbl, int Ntspd,
				int  NSubbanks,
				double *Tdecdrive,
				double *Tdecoder1,
				double *Tdecoder2,
				double inrisetime,
				double *outrisetime,
				int *nor_inputs,
				double *power)
{
  double Ceq,Req,Rwire,rows,cols,tf,nextinputtime,vth;
  int numstack,tagbits;
  double htree;
  int htree_int,exp;
  double l_inv_predecode,l_predec_nor_v,l_predec_nor_h;
  double wire_cap, wire_res, total_edge_length; 
  int addr_bits=(int)logtwo( (double)((double)C/(double)(B*A*Ntbl*Ntspd)));

  /* Calculate rise time.  Consider two inverters */

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(0.0,tf,VTHINV360x240,VTHINV360x240,FALL)/
    (VTHINV360x240);

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,
			   RISE)/
    (1.0-VTHINV360x240);

  /* First stage: driving the decoders */

  rows = C/(8*B*A*Ntbl*Ntspd);
        
  tagbits = ADDRESS_BITS+2-(int)logtwo((double)C)+(int)logtwo((double)A)-(int)(logtwo(NSubbanks));

  cols = tagbits*A*Ntspd/Ntwl ;
  total_edge_length = cols*Ntwl*Ntbl ;

  if(Ntwl*Ntbl==1 ) {
    l_inv_predecode = 1;
    wire_cap = Cwordmetal*l_inv_predecode*total_edge_length;
    wire_res = 0.5*Rwordmetal*l_inv_predecode*total_edge_length;

  }
  if(Ntwl*Ntbl==2 ) {
    l_inv_predecode = 0.5;
    wire_cap = Cwordmetal*l_inv_predecode*total_edge_length;
    wire_res = 0.5*Rwordmetal*l_inv_predecode*total_edge_length;
  }
  if(Ntwl*Ntbl>2) {
    htree=logtwo((double)(Ntwl*Ntbl));
    htree_int = (int) htree;
    if (htree_int % 2 ==0) {
      exp = (htree_int/2-1);
      l_inv_predecode = 0.25/(powers(2,exp));
      wire_cap = Cwordmetal*l_inv_predecode*total_edge_length;
      wire_res = 0.5*Rwordmetal*l_inv_predecode*total_edge_length;
    }
    else {
      exp = (htree_int-3)/2;
      l_inv_predecode = powers(2,exp);
      wire_cap = Cbitmetal*l_inv_predecode*rows*8;
      wire_res = 0.5*Rbitmetal*l_inv_predecode*rows*8;
    }
  }

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    4*gatecap(Wdec3to8n+Wdec3to8p,10.0)*(Ntwl*Ntbl)+
    wire_cap;
  Rwire = wire_res;

  tf = (Rwire + transreson(Wdecdrivep,PCH,1))*Ceq;
  *Tdecdrive = horowitz(inrisetime,tf,VTHINV360x240,VTHNAND60x120,
			FALL);
  nextinputtime = *Tdecdrive/VTHNAND60x120;
  *power+=addr_bits*Ceq*.5*VddPow*VddPow;

  /* second stage: driving a bunch of nor gates with a nand */

  numstack = (int)ceil((1.0/3.0)*logtwo( (double)((double)C/(double)(B*A*Ntbl*Ntspd))));
  if (numstack==0) numstack = 1;
  if (numstack>5) numstack = 5;

  if(Ntwl*Ntbl==1 || Ntwl*Ntbl==2 || Ntwl*Ntbl==4) {
    l_predec_nor_v = 0;
    l_predec_nor_h = 0;
  }
  else {
    if(Ntwl*Ntbl==8) {
      l_predec_nor_v = 0;
      l_predec_nor_h = cols;
    }
  }

  if(Ntwl*Ntbl>8) {
    htree=logtwo((double)(Ntwl*Ntbl));
    htree_int = (int) htree;
    if (htree_int % 2 ==0) {
      exp = (htree_int/2-1);
      l_predec_nor_v = (powers(2,exp)-1)*rows*8;
      l_predec_nor_h = (powers(2,exp)-1)*cols;
    }
    else {
      exp = (htree_int-3)/2;
      l_predec_nor_v = (powers(2,exp)-1)*rows*8;
      l_predec_nor_h = (powers(2,(exp+1))-1)*cols;
    }
  }

  Ceq = 3*draincap(Wdec3to8p,PCH,1) +draincap(Wdec3to8n,NCH,3) +
    gatecap(WdecNORn+WdecNORp,((numstack*40)+20.0))*rows +
    Cbitmetal*(rows*8 + l_predec_nor_v) +Cwordmetal*(l_predec_nor_h);
  Rwire = Rbitmetal*(rows*8+l_predec_nor_v)/2 + Rwordmetal*(l_predec_nor_h)/2;    

  tf = Ceq*(Rwire+transreson(Wdec3to8n,NCH,3)); 
  *power+=Ntwl*Ntbl*Ceq*VddPow*VddPow*4*ceil((1.0/3.0)*logtwo( (double)((double)C/(double)(B*A*Ntbl*Ntspd))));

  /* we only want to charge the output to the threshold of the
     nor gate.  But the threshold depends on the number of inputs
     to the nor.  */

  switch(numstack) {
  case 1: vth = VTHNOR12x4x1; break;
  case 2: vth = VTHNOR12x4x2; break;
  case 3: vth = VTHNOR12x4x3; break;
  case 4: vth = VTHNOR12x4x4; break;
  case 5: vth = VTHNOR12x4x4; break;
  case 6: vth = VTHNOR12x4x4; break;
  default: printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;

  }
        
  *Tdecoder1 = horowitz(nextinputtime,tf,VTHNAND60x120,vth,RISE);
  nextinputtime = *Tdecoder1/(1.0-vth);

  /* Final stage: driving an inverter with the nor */

  Req = transreson(WdecNORp,PCH,numstack);
  Ceq = (gatecap(Wdecinvn+Wdecinvp,20.0)+
	 numstack*draincap(WdecNORn,NCH,1)+
	 draincap(WdecNORp,PCH,numstack));
  tf = Req*Ceq;
  *Tdecoder2 = horowitz(nextinputtime,tf,vth,VSINV,FALL);
  *outrisetime = *Tdecoder2/(VSINV);


  *nor_inputs=numstack;
  *power+=Ceq*VddPow*VddPow;

  return(*Tdecdrive+*Tdecoder1+*Tdecoder2);
}

static double fa_tag_delay(int C, int B,
			   int Ndwl, int Ndbl, int Nspd,
			   int Ntwl, int Ntbl, int Ntspd,
			   double *Tagdrive,
			   double *Tag1,
			   double *Tag2,
			   double *Tag3,
			   double *Tag4,
			   double *Tag5,
			   double *outrisetime,
			   int *nor_inputs,
			   double *power)
{
  double Ceq,Req,Rwire,rows,tf,nextinputtime;
  double Tagdrive1=0, Tagdrive2=0;
  double Tagdrive0a=0, Tagdrive0b=0;
  double TagdriveA=0, TagdriveB=0;
  double TagdriveA1=0, TagdriveB1=0;
  double TagdriveA2=0, TagdriveB2=0;
  rows = C/(B*Ntbl);


  /* Calculate rise time.  Consider two inverters */

  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdriven,NCH,1);
  nextinputtime = horowitz(0.0,tf,VTHINV360x240,VTHINV360x240,FALL)/
    (VTHINV360x240);
        
  Ceq = draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1) +
    gatecap(Wdecdrivep+Wdecdriven,0.0);
  tf = Ceq*transreson(Wdecdrivep,PCH,1);
  nextinputtime = horowitz(nextinputtime,tf,VTHINV360x240,VTHINV360x240,
			   RISE)/(1.0-VTHINV360x240);
        
  // If tag bitline divisions, add extra driver

  if (Ntbl>1)
    {
      Ceq = draincap(Wfadrivep,PCH,1)+draincap(Wfadriven,NCH,1) +
	gatecap(Wfadrive2p+Wfadrive2n,0.0);
      tf = Ceq*transreson(Wfadriven,NCH,1);
      TagdriveA = horowitz(nextinputtime,tf,VSINV,VSINV,FALL);
      nextinputtime = TagdriveA/(VSINV);
      *power+=.5*Ceq*VddPow*VddPow*ADDRESS_BITS;
            
      if (Ntbl<=4)
	{
	  Ceq = draincap(Wfadrive2p,PCH,1)+draincap(Wfadrive2n,NCH,1) +
	    gatecap(Wfadecdrive1p+Wfadecdrive1n,10.0)*2+
	    + FACwordmetal*sqrt((rows+1)*Ntbl)/2
	    + FACbitmetal*sqrt((rows+1)*Ntbl)/2;
	  Rwire = FARwordmetal*sqrt((rows+1)*Ntbl)*.5/2+
	    FARbitmetal*sqrt((rows+1)*Ntbl)*.5/2;
	  tf = Ceq*(transreson(Wfadrive2p,PCH,1)+Rwire);
	  TagdriveB = horowitz(nextinputtime,tf,VSINV,VSINV,RISE);
	  nextinputtime = TagdriveB/(1.0-VSINV);
	  *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*2;
	}
      else
	{
	  Ceq = draincap(Wfadrive2p,PCH,1)+draincap(Wfadrive2n,NCH,1) +
	    gatecap(Wfadrivep+Wfadriven,10.0)*2+
	    + FACwordmetal*sqrt((rows+1)*Ntbl)/2
	    + FACbitmetal*sqrt((rows+1)*Ntbl)/2;
	  Rwire = FARwordmetal*sqrt((rows+1)*Ntbl)*.5/2+
	    FARbitmetal*sqrt((rows+1)*Ntbl)*.5/2;
	  tf = Ceq*(transreson(Wfadrive2p,PCH,1)+Rwire);
	  TagdriveB = horowitz(nextinputtime,tf,VSINV,VSINV,RISE);
	  nextinputtime = TagdriveB/(1.0-VSINV);
	  *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*4;
                
	  Ceq = draincap(Wfadrivep,PCH,1)+draincap(Wfadriven,NCH,1) +
	    gatecap(Wfadrive2p+Wfadrive2n,10.0);
	  tf = Ceq*transreson(Wfadriven,NCH,1);
	  TagdriveA1 = horowitz(nextinputtime,tf,VSINV,VSINV,FALL);
	  nextinputtime = TagdriveA1/(VSINV);
	  *power+=.5*Ceq*VddPow*VddPow*ADDRESS_BITS;
                
	  if (Ntbl<=16)
	    {
	      Ceq = draincap(Wfadrive2p,PCH,1)+draincap(Wfadrive2n,NCH,1) +
		gatecap(Wfadecdrive1p+Wfadecdrive1n,10.0)*2+
		+ FACwordmetal*sqrt((rows+1)*Ntbl)/4
		+ FACbitmetal*sqrt((rows+1)*Ntbl)/4;
	      Rwire = FARwordmetal*sqrt((rows+1)*Ntbl)*.5/4+
		FARbitmetal*sqrt((rows+1)*Ntbl)*.5/4;
	      tf = Ceq*(transreson(Wfadrive2p,PCH,1)+Rwire);
	      TagdriveB1 = horowitz(nextinputtime,tf,VSINV,VSINV,RISE);
	      nextinputtime = TagdriveB1/(1.0-VSINV);
	      *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*8;
	    }
	  else
	    {
	      Ceq = draincap(Wfadrive2p,PCH,1)+draincap(Wfadrive2n,NCH,1) +
		gatecap(Wfadrivep+Wfadriven,10.0)*2+
		+ FACwordmetal*sqrt((rows+1)*Ntbl)/4
		+ FACbitmetal*sqrt((rows+1)*Ntbl)/4;
	      Rwire = FARwordmetal*sqrt((rows+1)*Ntbl)*.5/4+
		FARbitmetal*sqrt((rows+1)*Ntbl)*.5/4;
	      tf = Ceq*(transreson(Wfadrive2p,PCH,1)+Rwire);
	      TagdriveB1 = horowitz(nextinputtime,tf,VSINV,VSINV,RISE);
	      nextinputtime = TagdriveB1/(1.0-VSINV);
	      *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*8;

	      Ceq = draincap(Wfadrivep,PCH,1)+draincap(Wfadriven,NCH,1) +
		gatecap(Wfadrive2p+Wfadrive2n,10.0);
	      tf = Ceq*transreson(Wfadriven,NCH,1);
	      TagdriveA2 = horowitz(nextinputtime,tf,VSINV,VSINV,FALL);
	      nextinputtime = TagdriveA2/(VSINV);
	      *power+=.5*Ceq*VddPow*VddPow*ADDRESS_BITS;
                
	      Ceq = draincap(Wfadrive2p,PCH,1)+draincap(Wfadrive2n,NCH,1) +
		gatecap(Wfadecdrive1p+Wfadecdrive1n,10.0)*2+
		+ FACwordmetal*sqrt((rows+1)*Ntbl)/8
		+ FACbitmetal*sqrt((rows+1)*Ntbl)/8;
	      Rwire = FARwordmetal*sqrt((rows+1)*Ntbl)*.5/8+
		FARbitmetal*sqrt((rows+1)*Ntbl)*.5/8;
	      tf = Ceq*(transreson(Wfadrive2p,PCH,1)+Rwire);
	      TagdriveB2 = horowitz(nextinputtime,tf,VSINV,VSINV,RISE);
	      nextinputtime = TagdriveB2/(1.0-VSINV);
	      *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*16;
	    }             
	}
    }
        
  /* Two more inverters for enable delay */
        
  Ceq = draincap(Wfadecdrive1p,PCH,1)+draincap(Wfadecdrive1n,NCH,1)
    +gatecap(Wfadecdrive2p+Wfadecdrive2n,0.0);
  tf = Ceq*transreson(Wfadecdrive1n,NCH,1);
  Tagdrive0a = horowitz(nextinputtime,tf,VSINV,VSINV,
			FALL);
  nextinputtime = Tagdrive0a/(VSINV);
  *power+=.5*Ceq*VddPow*VddPow*ADDRESS_BITS*Ntbl;

  Ceq = draincap(Wfadecdrive2p,PCH,1)+draincap(Wfadecdrive2n,NCH,1) +
    +gatecap(Wfadecdrivep+Wfadecdriven,10.0)
    +gatecap(Wfadecdrive2p+Wfadecdrive2n,10.0);
  tf = Ceq*transreson(Wfadecdrive2p,PCH,1);
  Tagdrive0b = horowitz(nextinputtime,tf,VSINV,VSINV,
			RISE);
  nextinputtime = Tagdrive0b/(VSINV);
  *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*Ntbl;

  /* First stage */

  Ceq = draincap(Wfadecdrive2p,PCH,1)+draincap(Wfadecdrive2n,NCH,1) +
    gatecap(Wfadecdrivep+Wfadecdriven,10.0);
  tf = Ceq*transresswitch(Wfadecdrive2n,NCH,1);
  Tagdrive1 = horowitz(nextinputtime,tf,VSINV,VTHFA1,FALL);
  nextinputtime = Tagdrive1/VTHFA1;
  *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*.5*Ntbl;

  Ceq = draincap(Wfadecdrivep,PCH,2)+draincap(Wfadecdriven,NCH,2)
    + draincap(Wfaprechn,NCH,1)
    + gatecap(Wdummyn,10.0)*(rows+1)
    + FACbitmetal*(rows+1);

  Rwire = FARbitmetal*(rows+1)*.5;
  tf = (Rwire + transreson(Wfadecdrivep,PCH,1) + 
	transresswitch(Wfadecdrivep,PCH,1))*Ceq;
  Tagdrive2 = horowitz(nextinputtime,tf,VTHFA1,VTHFA2,
		       RISE);
  nextinputtime = Tagdrive2/(1-VTHFA2);

  *Tagdrive=Tagdrive1+Tagdrive2+TagdriveA+TagdriveB+TagdriveA1+TagdriveA2+TagdriveB1+TagdriveB2+Tagdrive0a+Tagdrive0b;
  *power+=Ceq*VddPow*VddPow*ADDRESS_BITS*Ntbl;
        
  /* second stage */

  Ceq = .5*ADDRESS_BITS*2*draincap(Wdummyn,NCH,2)
    + draincap(Wfaprechp,PCH,1)
    + gatecap(Waddrnandn+Waddrnandp,10.0)
    + FACwordmetal*ADDRESS_BITS;
  Rwire = FARwordmetal*ADDRESS_BITS*.5;
  tf = Ceq*(Rwire+transreson(Wdummyn,NCH,1)+transreson(Wdummyn,NCH,1));
  *Tag1 = horowitz(nextinputtime,tf,VTHFA2,VTHFA3,FALL);
  nextinputtime = *Tag1/VTHFA3;
  *power+=Ceq*VddPow*VddPow*rows*Ntbl;

  /* third stage */
        
  Ceq = draincap(Waddrnandn,NCH,2)
    + 2*draincap(Waddrnandp,PCH,1)
    + gatecap(Wdummyinvn+Wdummyinvp,10.0);
  tf = Ceq*(transresswitch(Waddrnandp,PCH,1));
  *Tag2 = horowitz(nextinputtime,tf,VTHFA3,VTHFA4,RISE);
  nextinputtime = *Tag2/(1-VTHFA4);
  *power+=Ceq*VddPow*VddPow*rows*Ntbl;

  /* fourth stage */

  Ceq = (rows)*(gatecap(Wfanorn+Wfanorp,10.0))
    +draincap(Wdummyinvn,NCH,1)
    +draincap(Wdummyinvp,PCH,1)
    + FACbitmetal*rows;
  Rwire = FARbitmetal*rows*.5;
  Req = Rwire+transreson(Wdummyinvn,NCH,1);
  tf = Req*Ceq;
  *Tag3 = horowitz(nextinputtime,tf,VTHFA4,VTHFA5,FALL);
  *outrisetime = *Tag3/VTHFA5;
  *power+=Ceq*VddPow*VddPow*Ntbl;

  /* fifth stage */
        
  Ceq = draincap(Wfanorp,PCH,2)
    + 2*draincap(Wfanorn,NCH,1)
    + gatecap(Wfainvn+Wfainvp,10.0);
  tf = Ceq*(transresswitch(Wfanorp,PCH,1) + transreson(Wfanorp,PCH,1));
  *Tag4 = horowitz(nextinputtime,tf,VTHFA5,VTHFA6,RISE);
  nextinputtime = *Tag4/(1-VTHFA6);
  *power+=Ceq*VddPow*VddPow;
        
  /* final stage */

  Ceq = (gatecap(Wdecinvn+Wdecinvp,20.0)+
	 + draincap(Wfainvn,NCH,1)
	 +draincap(Wfainvp,PCH,1));
  Req = transresswitch(Wfainvn,NCH,1);
  tf = Req*Ceq;
  *Tag5 = horowitz(nextinputtime,tf,VTHFA6,VSINV,FALL);
  *outrisetime = *Tag5/VSINV;
  *power+=Ceq*VddPow*VddPow;

  //      if (Ntbl==32)
  //        fprintf(stderr," 1st - %f %f\n 2nd - %f %f\n 3rd - %f %f\n 4th - %f %f\n 5th - %f %f\nPD : %f\nNAND : %f\n INV : %f\n NOR : %f\n INV : %f\n", TagdriveA*1e9, TagdriveB*1e9, TagdriveA1*1e9, TagdriveB1*1e9, TagdriveA2*1e9, TagdriveB2*1e9, Tagdrive0a*1e9,Tagdrive0b*1e9,Tagdrive1*1e9, Tagdrive2*1e9, *Tag1*1e9, *Tag2*1e9, *Tag3*1e9, *Tag4*1e9, *Tag5*1e9);
  return(*Tagdrive+*Tag1+*Tag2+*Tag3+*Tag4+*Tag5);
}


/*----------------------------------------------------------------------*/

/* Data array wordline delay (see section 6.2 of tech report) */


static double wordline_delay(int B, int A,
			     int Ndwl, int Nspd,
			     double inrisetime,
			     double *outrisetime,
			     double *power)
{
  double Rpdrive;
  double desiredrisetime,psize,nsize;
  double tf,nextinputtime,Ceq,Rline,Cline;
  int cols;
  double Tworddrivedel,Twordchargedel;

#ifdef XCACTI
  if (PHASEDCACHE) {
    A=1;
  }
#endif

  cols = 8*B*A*Nspd/Ndwl;

  /* Choose a transistor size that makes sense */
  /* Use a first-order approx */

  desiredrisetime = krise*log((double)(cols))/2.0;
  Cline = (gatecappass(Wmemcella,0.0)+
	   gatecappass(Wmemcella,0.0)+
	   Cwordmetal)*cols;
  Rpdrive = desiredrisetime/(Cline*log(VSINV)*-1.0);
  psize = restowidth(Rpdrive,PCH);
  if (psize > Wworddrivemax) {
    psize = Wworddrivemax;
  }


  /* Now that we have a reasonable psize, do the rest as before */
  /* If we keep the ratio the same as the tag wordline driver,
     the threshold voltage will be close to VSINV */

  nsize = psize * Wdecinvn/Wdecinvp;

  Ceq = draincap(Wdecinvn,NCH,1) + draincap(Wdecinvp,PCH,1) +
    gatecap(nsize+psize,20.0);
  tf = transreson(Wdecinvp,PCH,1)*Ceq;
  *power+=Ceq*VddPow*VddPow;

  Tworddrivedel = horowitz(inrisetime,tf,VSINV,VSINV,RISE);
  nextinputtime = Tworddrivedel/(1.0-VSINV);

  Cline = (gatecappass(Wmemcella,(BitWidth-2*Wmemcella)/2.0)+
	   gatecappass(Wmemcella,(BitWidth-2*Wmemcella)/2.0)+
	   Cwordmetal)*cols+
    draincap(nsize,NCH,1) + draincap(psize,PCH,1);
  Rline = Rwordmetal*cols/2;
  tf = (transreson(psize,PCH,1)+Rline)*Cline;
  Twordchargedel = horowitz(nextinputtime,tf,VSINV,VSINV,FALL);
  *outrisetime = Twordchargedel/VSINV;
  *power+=Cline*VddPow*VddPow;

  //      fprintf(stderr, "%d %f %f\n", cols, Tworddrivedel*1e9, Twordchargedel*1e9);

  return(Tworddrivedel+Twordchargedel);
}


/*----------------------------------------------------------------------*/

/* Tag array wordline delay (see section 6.3 of tech report) */


static double wordline_tag_delay(int C, int A,
				 int Ntspd, int Ntwl,
				 int  NSubbanks,
				 double inrisetime,
				 double *outrisetime,
				 double *power)
{
  double tf;
  double Cline,Rline,Ceq,nextinputtime;
  int tagbits;
  double Tworddrivedel,Twordchargedel;

  /* number of tag bits */

  tagbits = ADDRESS_BITS+2-(int)logtwo((double)C)+(int)logtwo((double)A)-(int)(logtwo(NSubbanks));

  /* first stage */

  Ceq = draincap(Wdecinvn,NCH,1) + draincap(Wdecinvp,PCH,1) +
    gatecap(Wdecinvn+Wdecinvp,20.0);
  tf = transreson(Wdecinvn,NCH,1)*Ceq;

  Tworddrivedel = horowitz(inrisetime,tf,VSINV,VSINV,RISE);
  nextinputtime = Tworddrivedel/(1.0-VSINV);
  *power+=Ceq*VddPow*VddPow;

  /* second stage */
  Cline = (gatecappass(Wmemcella,(BitWidth-2*Wmemcella)/2.0)+
	   gatecappass(Wmemcella,(BitWidth-2*Wmemcella)/2.0)+
	   Cwordmetal)*tagbits*A*Ntspd/Ntwl+
    draincap(Wdecinvn,NCH,1) + draincap(Wdecinvp,PCH,1);
  Rline = Rwordmetal*tagbits*A*Ntspd/(2*Ntwl);
  tf = (transreson(Wdecinvp,PCH,1)+Rline)*Cline;
  Twordchargedel = horowitz(nextinputtime,tf,VSINV,VSINV,FALL);
  *outrisetime = Twordchargedel/VSINV;
  *power+=Cline*VddPow*VddPow;
  return(Tworddrivedel+Twordchargedel);

}

/*----------------------------------------------------------------------*/

/* Data array bitline: (see section 6.4 in tech report) */


static double bitline_delay(int C, int A, int B,
			    int Ndwl, int Ndbl, int Nspd,
			    double inrisetime,
			    double *outrisetime,
			    double *power,
			    double *wrpower) // write power
{
  double Tbit,Cline,Ccolmux,Rlineb,r1,r2,c1,c2,a,b,c;
  double m,tstep;
  double Cbitrow;    /* bitline capacitance due to access transistor */
  int rows,cols;
  int muxway;

#ifdef XCACTI
  if (PHASEDCACHE) {
    C=C/A;
    A=1;
  }
#endif

  Cbitrow = draincap(Wmemcella,NCH,1)/2.0; /* due to shared contact */
  rows = C/(B*A*Ndbl*Nspd);
  cols = 8*B*A*Nspd/Ndwl;
  if (8*B/BITOUT == 1 && Ndbl*Nspd==1) {
    Cline = rows*(Cbitrow+Cbitmetal)+2*draincap(Wbitpreequ,PCH,1);
    Ccolmux = 2*gatecap(WsenseQ1to4,10.0);
    Rlineb = Rbitmetal*rows/2.0;
    r1 = Rlineb;
    //muxover=1;
  } else { 
    if (Ndbl*Nspd > MAX_COL_MUX)
      {
	//muxover=8*B/BITOUT;
	muxway=MAX_COL_MUX;
      }
    else
      if (8*B*Ndbl*Nspd/BITOUT > MAX_COL_MUX)
	{
	  muxway=MAX_COL_MUX;
	  // muxover=(8*B/BITOUT)/(MAX_COL_MUX/(Ndbl*Nspd));
	}
      else
	{
	  muxway=8*B*Nspd*Ndbl/BITOUT;
	  // muxover=1;
	}
          
    Cline = rows*(Cbitrow+Cbitmetal) + 2*draincap(Wbitpreequ,PCH,1) +
      draincap(Wbitmuxn,NCH,1);
    Ccolmux = muxway*(draincap(Wbitmuxn,NCH,1))+2*gatecap(WsenseQ1to4,10.0);
    Rlineb = Rbitmetal*rows/2.0;
    r1 = Rlineb + 
      transreson(Wbitmuxn,NCH,1);
  }
  r2 = transreson(Wmemcella,NCH,1) +
    transreson(Wmemcella*Wmemcellbscale,NCH,1);
  c1 = Ccolmux;
  c2 = Cline;

  *power+=c1*VddPow*VddPow*BITOUT*A*muxover;
#ifdef XCACTI
  *power+=2*c2*VbitprePow*VbitprePow*cols;

  *wrpower += c2*VddPow*VddPow*cols;
#else
  *power+=c2*VddPow*VbitprePow*cols;
#endif


  //fprintf(stderr, "Pow %f %f\n", c1*VddPow*VddPow*BITOUT*A*muxover*1e9, c2*VddPow*VbitprePow*cols*1e9);
  tstep = (r2*c2+(r1+r2)*c1)*log((Vbitpre)/(Vbitpre-Vbitsense));

  /* take input rise time into account */

  m = Vdd/inrisetime;
  a = m;
  b = 2*((Vdd*0.5)-Vt);
  c = -2*tstep*(Vdd-Vt)+1/m*((Vdd*0.5)-Vt)*((Vdd*0.5)-Vt);
  if (tstep <= (0.5*(Vdd-Vt)/m) &&
      (b < sqrt(b*b-4*a*c)) ) {
    Tbit = (-b+sqrt(b*b-4*a*c))/(2*a);
  } else {
    Tbit = tstep + (Vdd+Vt)/(2*m) - (Vdd*0.5)/m;
  }

  *outrisetime = Tbit/(log((Vbitpre-Vbitsense)/Vdd));
  return(Tbit);
}




/*----------------------------------------------------------------------*/

/* Tag array bitline: (see section 6.4 in tech report) */


static double bitline_tag_delay(int C, int A, int B,
				int Ntwl, int Ntbl, int Ntspd,
				int  NSubbanks,
				double inrisetime,
				double *outrisetime,
				double *power,
				double *wrpower) {

  double Tbit,Cline,Ccolmux,Rlineb,r1,r2,c1,c2,a,b,c;
  double m,tstep;
  double Cbitrow;    /* bitline capacitance due to access transistor */
  int tagbits;
  int rows,cols;

  tagbits = ADDRESS_BITS+2-(int)logtwo((double)C)+(int)logtwo((double)A)-(int)(logtwo(NSubbanks));

  Cbitrow = draincap(Wmemcella,NCH,1)/2.0; /* due to shared contact */
  rows = C/(B*A*Ntbl*Ntspd);
  cols = tagbits*A*Ntspd/Ntwl;
  if (Ntbl*Ntspd == 1) {
    Cline = rows*(Cbitrow+Cbitmetal)+2*draincap(Wbitpreequ,PCH,1);
    Ccolmux = 2*gatecap(WsenseQ1to4,10.0);
    Rlineb = Rbitmetal*rows/2.0;
    r1 = Rlineb;
  } else { 
    Cline = rows*(Cbitrow+Cbitmetal) + 2*draincap(Wbitpreequ,PCH,1) +
      draincap(Wbitmuxn,NCH,1);
    Ccolmux = Ntspd*Ntbl*(draincap(Wbitmuxn,NCH,1))+2*gatecap(WsenseQ1to4,10.0);
    Rlineb = Rbitmetal*rows/2.0;
    r1 = Rlineb + 
      transreson(Wbitmuxn,NCH,1);
  }
  r2 = transreson(Wmemcella,NCH,1) +
    transreson(Wmemcella*Wmemcellbscale,NCH,1);

  c1 = Ccolmux;
  c2 = Cline;     

  *power+=c1*VddPow*VddPow;
#ifdef XCACTI
  *power+=2*c2*VbitprePow*VbitprePow*cols;
  *wrpower+=c2*VddPow*VddPow*tagbits;
#else
  *power+=c2*VddPow*VbitprePow*cols;
#endif

  //fprintf(stderr, "Pow %f %f\n", c1*VddPow*VddPow*1e9, c2*VddPow*VbitprePow*cols*1e9);

  tstep = (r2*c2+(r1+r2)*c1)*log((Vbitpre)/(Vbitpre-Vbitsense));

  /* take into account input rise time */

  m = Vdd/inrisetime;
  a = m;
  b = 2*((Vdd*0.5)-Vt);
  c = -2*tstep*(Vdd-Vt)+1/m*((Vdd*0.5)-Vt)*((Vdd*0.5)-Vt);
  if (tstep <= (0.5*(Vdd-Vt)/m) &&
      b < sqrt(b*b-4*a*c) ) {
    Tbit = (-b+sqrt(b*b-4*a*c))/(2*a);
  } else {
    Tbit = tstep + (Vdd+Vt)/(2*m) - (Vdd*0.5)/m;
  }

  *outrisetime = Tbit/(log((Vbitpre-Vbitsense)/Vdd));
  return(Tbit);
}



/*----------------------------------------------------------------------*/

/* It is assumed the sense amps have a constant delay
   (see section 6.5) */

static double sense_amp_delay(int A,
			      double inrisetime,
			      double *outrisetime,
			      int rows,
			      double *power) {

  *outrisetime = tfalldata;

#ifdef XCACTI
  if (PHASEDCACHE) {
    *power*=(psensedata/A);
  }else{
    *power*=(psensedata);
  }
#else
  *power*=psensedata;
#endif

  return(tsensedata+2.5e-10);
}

/*--------------------------------------------------------------*/

static double sense_amp_tag_delay(double inrisetime,
				  double *outrisetime,
				  double *power) {
  
  *outrisetime = tfalltag;
  *power*=psensetag;
  return(tsensetag+2.5e-10);
}


static double half_compare_time(int C, int A,
				int Ntbl, int Ntspd,
				int  NSubbanks,
				double inputtime,
				double *outputtime,
				double *power) {

  double Req,Ceq,tf,st1del,st2del,st3del,nextinputtime,m;
  double c1,c2,r1,r2,tstep,a,b,c;
  double Tcomparatorni;
  int cols,tagbits;

  /* First Inverter */

  Ceq = gatecap(Wcompinvn2+Wcompinvp2,10.0) +
    draincap(Wcompinvp1,PCH,1) + draincap(Wcompinvn1,NCH,1);
  Req = transreson(Wcompinvp1,PCH,1);
  tf = Req*Ceq;
  st1del = horowitz(inputtime,tf,VTHCOMPINV,VTHCOMPINV,FALL);
  nextinputtime = st1del/VTHCOMPINV;
  *power+=Ceq*VddPow*VddPow*2*A;

  /* Second Inverter */

  Ceq = gatecap(Wcompinvn3+Wcompinvp3,10.0) +
    draincap(Wcompinvp2,PCH,1) + draincap(Wcompinvn2,NCH,1);
  Req = transreson(Wcompinvn2,NCH,1);
  tf = Req*Ceq;
  st2del = horowitz(nextinputtime,tf,VTHCOMPINV,VTHCOMPINV,RISE);
  nextinputtime = st2del/(1.0-VTHCOMPINV);
  *power+=Ceq*VddPow*VddPow*2*A;

  /* Third Inverter */

  Ceq = gatecap(Wevalinvn+Wevalinvp,10.0) +
    draincap(Wcompinvp3,PCH,1) + draincap(Wcompinvn3,NCH,1);
  Req = transreson(Wcompinvp3,PCH,1);
  tf = Req*Ceq;
  st3del = horowitz(nextinputtime,tf,VTHCOMPINV,VTHEVALINV,FALL);
  nextinputtime = st3del/(VTHEVALINV);
  *power+=Ceq*VddPow*VddPow*2*A;

  /* Final Inverter (virtual ground driver) discharging compare part */
  
  tagbits = ADDRESS_BITS+2 - (int)logtwo((double)C) + (int)logtwo((double)A)-(int)(logtwo(NSubbanks));
  tagbits=tagbits/2;
  cols = tagbits*Ntbl*Ntspd;

  r1 = transreson(Wcompn,NCH,2);
  r2 = transreson(Wevalinvn,NCH,1); /* was switch */
  c2 = (tagbits)*(draincap(Wcompn,NCH,1)+draincap(Wcompn,NCH,2))+
    draincap(Wevalinvp,PCH,1) + draincap(Wevalinvn,NCH,1);
  c1 = (tagbits)*(draincap(Wcompn,NCH,1)+draincap(Wcompn,NCH,2))
    +draincap(Wcompp,PCH,1) + gatecap(WmuxdrvNANDn+WmuxdrvNANDp,20.0) +
    cols*Cwordmetal;
  *power+=c2*VddPow*VddPow*2*A;
  *power+=c1*VddPow*VddPow*(A-1);


  /* time to go to threshold of mux driver */

  tstep = (r2*c2+(r1+r2)*c1)*log(1.0/VTHMUXNAND);

  /* take into account non-zero input rise time */

  m = Vdd/nextinputtime;

  if ((tstep) <= (0.5*(Vdd-Vt)/m)) {
    a = m;
    b = 2*((Vdd*VTHEVALINV)-Vt);
    c = -2*(tstep)*(Vdd-Vt)+1/m*((Vdd*VTHEVALINV)-Vt)*((Vdd*VTHEVALINV)-Vt);
    Tcomparatorni = (-b+sqrt(b*b-4*a*c))/(2*a);
  } else {
    Tcomparatorni = (tstep) + (Vdd+Vt)/(2*m) - (Vdd*VTHEVALINV)/m;
  }
  *outputtime = Tcomparatorni/(1.0-VTHMUXNAND);

                        
  return(Tcomparatorni+st1del+st2del+st3del);
}

/*----------------------------------------------------------------------*/

static double mux_driver_delay_dualin(int C, int B, int A,
				      int Ndbl, int Nspd, int Ndwl,
				      int Ntbl, int Ntspd,
				      double inputtime1,
				      double *outputtime,
				      double wirelength_v, 
				      double wirelength_h,
				      double *power) {

  double Ceq,Req,tf,nextinputtime;
  double Tst1,Tst2,Tst3;
   
  /* first driver stage - Inverte "match" to produce "matchb" */
  /* the critical path is the DESELECTED case, so consider what
     happens when the address bit is true, but match goes low */

  Ceq = gatecap(WmuxdrvNORn+WmuxdrvNORp,15.0)*muxover
    +draincap(WmuxdrvNANDn,NCH,2) + 2*draincap(WmuxdrvNANDp,PCH,1);
  Req = transreson(WmuxdrvNANDp,PCH,1);
  tf = Ceq*Req;
  Tst1 = horowitz(inputtime1,tf,VTHMUXNAND,VTHMUXDRV2,FALL);
  nextinputtime = Tst1/VTHMUXDRV2;
  *power+=Ceq*VddPow*VddPow*(A-1);

  /* second driver stage - NOR "matchb" with address bits to produce sel */

  Ceq = gatecap(Wmuxdrv3n+Wmuxdrv3p,15.0) + 2*draincap(WmuxdrvNORn,NCH,1) +
    draincap(WmuxdrvNORp,PCH,2);
  Req = transreson(WmuxdrvNORn,NCH,1);
  tf = Ceq*Req;
  Tst2 = horowitz(nextinputtime,tf,VTHMUXDRV2,VTHMUXDRV3,RISE);
  nextinputtime = Tst2/(1-VTHMUXDRV3);
  *power+=Ceq*VddPow*VddPow;

  /* third driver stage - invert "select" to produce "select bar" */

  /*   fprintf(stderr, "%f %f %f %f\n", 
       ((Rwordmetal*wirelength)/2.0 + transreson(Wmuxdrv3p,PCH,1)), 
       ((Rwordmetal*0)/2.0 + transreson(Wmuxdrv3p,PCH,1)), 
       (BITOUT*gatecap(Woutdrvseln+Woutdrvselp+Woutdrvnorn+Woutdrvnorp,20.0)+
       draincap(Wmuxdrv3p,PCH,1) + draincap(Wmuxdrv3n,NCH,1) +
       Cwordmetal*wirelength),
       (BITOUT*gatecap(Woutdrvseln+Woutdrvselp+Woutdrvnorn+Woutdrvnorp,20.0)+
       draincap(Wmuxdrv3p,PCH,1) + draincap(Wmuxdrv3n,NCH,1) +
       Cwordmetal*0));
  */

  Ceq = BITOUT*gatecap(Woutdrvseln+Woutdrvselp+Woutdrvnorn+Woutdrvnorp,20.0)+
    draincap(Wmuxdrv3p,PCH,1) + draincap(Wmuxdrv3n,NCH,1) +
    Cwordmetal*wirelength_h+Cbitmetal*wirelength_v;
  Req = (Rwordmetal*wirelength_h+Rbitmetal*wirelength_v)/2.0 + transreson(Wmuxdrv3p,PCH,1);
  tf = Ceq*Req;
  Tst3 = horowitz(nextinputtime,tf,VTHMUXDRV3,VTHOUTDRINV,FALL);
  *outputtime = Tst3/(VTHOUTDRINV);
  *power+=Ceq*VddPow*VddPow;


  return(Tst1 + Tst2 + Tst3);

}

static double senseext_driver_delay(int C, int B, int A,
				    char fullyassoc,
				    int Ndbl, int Nspd, int Ndwl,
				    int Ntbl, int Ntwl, int Ntspd,
				    double inputtime,
				    double *outputtime,
				    double wirelength_sense_v, 
				    double wirelength_sense_h,
				    double *power)
{
  double Ceq,Req,tf,nextinputtime;
  double Tst1,Tst2;

  if(fullyassoc) {
    A=1;
  }

#ifdef XCACTI
  if (PHASEDCACHE) {
    C=C/A;
    A=1;
  }
#endif

  /* first driver stage */

  Ceq = draincap(Wsenseextdrv1p,PCH,1) + draincap(Wsenseextdrv1n,NCH,1) +
    gatecap(Wsenseextdrv2n+Wsenseextdrv2p,10.0);
  Req = transreson(Wsenseextdrv1n,NCH,1);
  tf = Ceq*Req;
  Tst1 = horowitz(inputtime,tf,VTHSENSEEXTDRV,VTHSENSEEXTDRV,FALL);
  nextinputtime = Tst1/VTHSENSEEXTDRV;
  *power+=Ceq*VddPow*VddPow*.5*BITOUT*A*muxover;

  /* second driver stage */

  Ceq = draincap(Wsenseextdrv2p,PCH,1) + draincap(Wsenseextdrv2n,NCH,1) +
    Cwordmetal*wirelength_sense_h + Cbitmetal*wirelength_sense_v;

  Req = (Rwordmetal*wirelength_sense_h + Rbitmetal*wirelength_sense_v)/2.0 + transreson(Wsenseextdrv2p,PCH,1);

  tf = Ceq*Req;
  Tst2 = horowitz(nextinputtime,tf,VTHSENSEEXTDRV,VTHOUTDRNAND,RISE);

  *outputtime = Tst2/(1-VTHOUTDRNAND);
  *power+=Ceq*VddPow*VddPow*.5*BITOUT*A*muxover;

  //   fprintf(stderr, "Pow %f %f\n", Ceq*VddPow*VddPow*.5*BITOUT*A*muxover*1e9, Ceq*VddPow*VddPow*.5*BITOUT*A*muxover*1e9);

  return(Tst1 + Tst2);
}

/* Valid driver (see section 6.9 of tech report)
   Note that this will only be called for a direct mapped cache */

static double valid_driver_delay(int C, int B, int A,
				 char fullyassoc,
				 int Ntbl, int Ntwl, int Ntspd,
				 int Ndbl, int Ndwl, int Nspd,
				 int NSubbanks,
				 double inputtime,
				 double *power) {
  double Ceq,Tst1,tf;
  int rows,tagbits,cols,htree_int,l_valdrv_v,l_valdrv_h,exp;
  double wire_cap,wire_res;
  double htree;
  double subbank_v,subbank_h;

#ifdef XCACTI
  if (PHASEDCACHE) {
    C=C/A;
    A=1;
  }
#endif

  rows = C/(8*B*A*Ntbl*Ntspd);

  tagbits = ADDRESS_BITS+2-(int)logtwo((double)C)+(int)logtwo((double)A)-(int)(logtwo(NSubbanks));
  cols = tagbits*A*Ntspd/Ntwl ;

  /* calculate some layout info */

  if(Ntwl*Ntbl==1) {
    l_valdrv_v= 0;
    l_valdrv_h= cols;
  }
  if(Ntwl*Ntbl==2 || Ntwl*Ntbl==4) {
    l_valdrv_v= 0;
    l_valdrv_h= 2*cols;
  }

  if(Ntwl*Ntbl>4) {
    htree=logtwo((double)(Ntwl*Ntbl));
    htree_int = (int) htree;
    if (htree_int % 2 ==0) {
      exp = (htree_int/2-1);
      l_valdrv_v = (powers(2,exp)-1)*rows;
      l_valdrv_h = (int)sqrt(Ntwl*Ntbl)*cols;
    }
    else {
      exp = (htree_int+1)/2-1;
      l_valdrv_v = (powers(2,exp)-1)*rows;
      l_valdrv_h = (int)sqrt(Ntwl*Ntbl/2)*cols;
    }
  }

  subbank_routing_length(C,B,A,fullyassoc,Ndbl,Nspd,Ndwl,Ntbl,Ntwl,Ntspd,NSubbanks,&subbank_v,&subbank_h);

  wire_cap = Cbitmetal*(l_valdrv_v+subbank_v) +Cwordmetal*(l_valdrv_h+subbank_h);
  wire_res = Rwordmetal*(l_valdrv_h+subbank_h)*0.5+Rbitmetal*(l_valdrv_v+subbank_v)*0.5;

  Ceq = draincap(Wmuxdrv12n,NCH,1)+draincap(Wmuxdrv12p,PCH,1)+wire_cap+Cout;
  tf = Ceq*(transreson(Wmuxdrv12p,PCH,1)+wire_res);
  Tst1 = horowitz(inputtime,tf,VTHMUXDRV1,0.5,FALL);
  *power+=Ceq*VddPow*VddPow;

  return(Tst1);
}


/*----------------------------------------------------------------------*/

/* Data output delay (data side) -- see section 6.8
   This is the time through the NAND/NOR gate and the final inverter 
   assuming sel is already present */

static double dataoutput_delay(int C, int B, int A,
			       char fullyassoc,
			       int Ndbl, int Nspd, int Ndwl,
			       double inrisetime,
			       double *outrisetime,
			       double *power)
{
  double Ceq,Rwire;
  double tf;
  double nordel,outdel,nextinputtime;       
  double l_outdrv_v,l_outdrv_h;
  double rows,cols,rows_fa_subarray,cols_fa_subarray;
  double htree;
  int htree_int,exp,tagbits;
  
  if(!fullyassoc) {

    rows = (C/(B*A*Ndbl*Nspd));
    cols = (8*B*A*Nspd/Ndwl);

    /* calculate some layout info */

    if(Ndwl*Ndbl==1) {
      l_outdrv_v= 0;
      l_outdrv_h= cols;
    }
    if(Ndwl*Ndbl==2 || Ndwl*Ndbl==4) {
      l_outdrv_v= 0;
      l_outdrv_h= 2*cols;
    }

    if(Ndwl*Ndbl>4) {
      htree=logtwo((double)(Ndwl*Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	exp = (htree_int/2-1);
	l_outdrv_v = (powers(2,exp)-1)*rows;
	l_outdrv_h = sqrt(Ndwl*Ndbl)*cols;
      }
      else {
	exp = (htree_int+1)/2-1;
	l_outdrv_v = (powers(2,exp)-1)*rows;
	l_outdrv_h = sqrt(Ndwl*Ndbl/2)*cols;
      }
    }
  }
  else {
    rows_fa_subarray = (C/(B*Ndbl));
    tagbits = ADDRESS_BITS+2-(int)logtwo((double)B);
    cols_fa_subarray = (8*B)+tagbits;
    if(Ndbl==1) {
      l_outdrv_v= 0;
      l_outdrv_h= cols_fa_subarray;
    }
    if(Ndbl==2 || Ndbl==4) {
      l_outdrv_v= 0;
      l_outdrv_h= 2*cols_fa_subarray;
    }

    if(Ndbl>4) {
      htree=logtwo((double)(Ndbl));
      htree_int = (int) htree;
      if (htree_int % 2 ==0) {
	exp = (htree_int/2-1);
	l_outdrv_v = (powers(2,exp)-1)*rows_fa_subarray;
	l_outdrv_h = sqrt(Ndbl)*cols_fa_subarray;
      }
      else {
	exp = (htree_int+1)/2-1;
	l_outdrv_v = (powers(2,exp)-1)*rows_fa_subarray;
	l_outdrv_h = sqrt(Ndbl/2)*cols_fa_subarray;
      }
    }
  }

  /* Delay of NOR gate */

  Ceq = 2*draincap(Woutdrvnorn,NCH,1)+draincap(Woutdrvnorp,PCH,2)+
    gatecap(Woutdrivern,10.0);
  tf = Ceq*transreson(Woutdrvnorp,PCH,2);
  nordel = horowitz(inrisetime,tf,VTHOUTDRNOR,VTHOUTDRIVE,FALL);
  nextinputtime = nordel/(VTHOUTDRIVE);
  *power+=Ceq*VddPow*VddPow*.5*BITOUT;

  /* Delay of final output driver */

  Ceq = (draincap(Woutdrivern,NCH,1)+draincap(Woutdriverp,PCH,1))*A*muxover
    +Cwordmetal*l_outdrv_h + Cbitmetal*l_outdrv_v + gatecap(Wsenseextdrv1n+Wsenseextdrv1p,10.0);
  Rwire = (Rwordmetal*l_outdrv_h + Rbitmetal*l_outdrv_v)/2;

  *power+=Ceq*VddPow*VddPow*.5*BITOUT;

  tf = Ceq*(transreson(Woutdrivern,NCH,1)+Rwire);
  outdel = horowitz(nextinputtime,tf,VTHOUTDRIVE,0.5,RISE);
  *outrisetime = outdel/0.5;
  return(outdel+nordel);
}

/*----------------------------------------------------------------------*/

/* Sel inverter delay (part of the output driver)  see section 6.8 */

static double selb_delay_tag_path(double inrisetime,
				  double *outrisetime,
				  double *power) {

  double Ceq,Tst1,tf;

  Ceq = draincap(Woutdrvseln,NCH,1)+draincap(Woutdrvselp,PCH,1)+
    gatecap(Woutdrvnandn+Woutdrvnandp,10.0);
  tf = Ceq*transreson(Woutdrvseln,NCH,1);
  Tst1 = horowitz(inrisetime,tf,VTHOUTDRINV,VTHOUTDRNAND,RISE);
  *outrisetime = Tst1/(1.0-VTHOUTDRNAND);
  *power+=Ceq*VddPow*VddPow;

  return(Tst1);
}

/*======================================================================*/


void xcacti_calculate_time(const parameter_type *parameters,
			   result_type *result,
			   arearesult_type *arearesult,
			   area_type *arearesult_subbanked) {

  arearesult_type arearesult_temp;
  area_type arearesult_subbanked_temp;

  int Ndwl,Ndbl,Nspd,Ntwl,Ntbl,Ntspd,cols_subarray,rows_subarray;
  int l_muxdrv_v, l_muxdrv_h, exp, htree_int;
  double Subbank_Efficiency, Total_Efficiency, max_efficiency, efficiency, min_efficiency;
  double max_aspect_ratio_total ,aspect_ratio_total_temp;
  double htree;
  double bank_h, bank_v, subbank_h, subbank_v; 
  double wirelength_v, wirelength_h;
  double access_time=0;
  double total_power=0;
  double before_mux=0.0,after_mux=0, total_power_allbanks=0.0;
  double total_address_routing_power=0.0, total_power_without_routing=0.0;
  double subbankaddress_routing_delay=0.0, subbankaddress_routing_power=0.0;
  double decoder_data_driver=0.0,decoder_data_3to8=0.0,decoder_data_inv=0.0;
  double decoder_data=0.0,decoder_tag=0.0,wordline_data=0.0,wordline_tag=0.0;
  double decoder_data_power=0.0,decoder_tag_power=0.0,wordline_data_power=0.0,wordline_tag_power=0.0;
  double decoder_tag_driver=0.0,decoder_tag_3to8=0.0,decoder_tag_inv=0.0;
  double bitline_data=0.0,bitline_tag=0.0,sense_amp_data=0.0,sense_amp_tag=0.0;
  double bitline_data_power=0.0,bitline_tag_power=0.0,sense_amp_data_power=0.0,sense_amp_tag_power=0.0;
#ifdef XCACTI
  double write_power_data=0.0,write_power_tag=0.0;
#endif
  double compare_tag=0.0,mux_driver=0.0,data_output=0.0,selb=0.0;
  double compare_tag_power=0.0, selb_power=0.0, mux_driver_power=0.0, valid_driver_power=0.0;
  double data_output_power=0.0;
  double time_till_compare=0.0,time_till_select=0.0,valid_driver=0.0;
  double cycle_time=0.0, precharge_del=0.0;
  double outrisetime=0.0,inrisetime=0.0,addr_inrisetime=0.0;   
  double senseext_scale=1.0;
  double total_out_driver=0.0;
  double total_out_driver_power=0.0;
  float scale_init;
  int data_nor_inputs=1, tag_nor_inputs=1;
  double tag_delay_part1=0.0, tag_delay_part2=0.0, tag_delay_part3=0.0, tag_delay_part4=0.0,tag_delay_part5=0.0,tag_delay_part6=0.0;
  double max_delay=0;
  int counter;

  float FUDGEFACTOR = parameters->fudgefactor;

  /* go through possible Ndbl,Ndwl and find the smallest */
  /* Because of area considerations, I don't think it makes sense
     to break either dimension up larger than MAXN */

  /* only try moving output drivers for set associative cache */
  if (parameters->associativity!=1)
    scale_init=0.1;
  else
    scale_init=1.0;

  if (parameters->fully_assoc)
    /* If model is a fully associative cache - use larger cell size */
    {
      FACbitmetal=((32+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1)+parameters->num_read_ports))*Cmetal);
      FACwordmetal=((8+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1))+WIREPITCH*(parameters->num_read_ports))*Cmetal);
      FARbitmetal=(((32+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1)+parameters->num_read_ports))/WIREWIDTH)*Rmetal);
      FARwordmetal=(((8+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1))+WIREPITCH*(parameters->num_read_ports))/WIREWIDTH)*Rmetal);
    }     
  Cbitmetal=((16+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1)+parameters->num_read_ports))*Cmetal);
  Cwordmetal=((8+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1))+WIREPITCH*(parameters->num_read_ports))*Cmetal);
  Rbitmetal=(((16+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1)+parameters->num_read_ports))/WIREWIDTH)*Rmetal);
  Rwordmetal=(((8+2*WIREPITCH*((parameters->num_write_ports+parameters->num_readwrite_ports-1))+WIREPITCH*(parameters->num_read_ports))/WIREWIDTH)*Rmetal);

  VddPow = parameters->VddPow;
  VbitprePow  = parameters->VbitprePow;

  result->cycle_time = BIGNUM;
  result->access_time = BIGNUM;
  result->total_power = BIGNUM;
  result->best_muxover=8*parameters->block_size/BITOUT;
  result->max_access_time = 0;
  result->max_power = 0;
  arearesult_temp.max_efficiency = 1.0/BIGNUM;
  max_efficiency = 1.0/BIGNUM;
  min_efficiency = BIGNUM;
  arearesult->efficiency = 1.0/BIGNUM;
  max_aspect_ratio_total = 1.0/BIGNUM;
  arearesult->aspect_ratio_total = BIGNUM;

  for (counter=0; counter<2; counter++) {
    if (!parameters->fully_assoc) {
      /* Set associative or direct map cache model */
      for (Nspd=1;Nspd<=MAXSPD;Nspd=Nspd*2) {
	for (Ndwl=1;Ndwl<=MAXN;Ndwl=Ndwl*2) {
	  for (Ndbl=1;Ndbl<=MAXN;Ndbl=Ndbl*2) {
	    for (Ntspd=1;Ntspd<=MAXSPD;Ntspd=Ntspd*2) {
	      for (Ntwl=1;Ntwl<=1;Ntwl=Ntwl*2) {
		for (Ntbl=1;Ntbl<=MAXN;Ntbl=Ntbl*2) {
		  if (xcacti_organizational_parameters_valid
		      (parameters->block_size, parameters->associativity, parameters->cache_size,Ndwl,Ndbl,Nspd,Ntwl,Ntbl,Ntspd,parameters->fully_assoc)) {
        
		    bank_h=0;
		    bank_v=0;

		    if (8*parameters->block_size/BITOUT == 1 && Ndbl*Nspd==1) {
		      muxover=1;
		    } 
		    else {
		      if (Ndbl*Nspd > MAX_COL_MUX)
			{
			  muxover=8*parameters->block_size/BITOUT;
			}
		      else {
			if (8*parameters->block_size*Ndbl*Nspd/BITOUT > MAX_COL_MUX)
			  {
			    muxover=(8*parameters->block_size/BITOUT)/(MAX_COL_MUX/(Ndbl*Nspd));
			  }
			else
			  {
			    muxover=1;
			  }
		      }
		    }

		    xcacti_area_subbanked(ADDRESS_BITS,BITOUT,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters,&arearesult_subbanked_temp,&arearesult_temp);
           
		    Subbank_Efficiency= (area_all_dataramcells+area_all_tagramcells)*100/(arearesult_temp.totalarea/100000000.0);
		    Total_Efficiency= (parameters->NSubbanks)*(area_all_dataramcells+area_all_tagramcells)*100/(xcacti_calculate_area(arearesult_subbanked_temp,parameters->fudgefactor)/100000000.0);
		    //efficiency = Subbank_Efficiency;
		    efficiency = Total_Efficiency;

		    arearesult_temp.efficiency = efficiency;
		    aspect_ratio_total_temp = (arearesult_subbanked_temp.height/arearesult_subbanked_temp.width);
		    aspect_ratio_total_temp = (aspect_ratio_total_temp > 1.0) ? (aspect_ratio_total_temp) : 1.0/(aspect_ratio_total_temp) ;

		    arearesult_temp.aspect_ratio_total = aspect_ratio_total_temp;

		    subbank_dim(parameters->cache_size,parameters->block_size,parameters->associativity,parameters->fully_assoc,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters->NSubbanks,&bank_h,&bank_v);
		    subbanks_routing_power(parameters->fully_assoc,parameters->associativity,parameters->NSubbanks,&bank_h,&bank_v,&total_address_routing_power);
		    total_address_routing_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);

		    if(parameters->NSubbanks > 2 ) {
		      subbankaddress_routing_delay= address_routing_delay(parameters->cache_size, parameters->block_size, parameters->associativity, parameters->fully_assoc, Ndwl, Ndbl, Nspd, Ntwl, Ntbl,Ntspd,parameters->NSubbanks,&outrisetime,&subbankaddress_routing_power);
		    }

		    subbankaddress_routing_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);

		    /* Calculate data side of cache */
		    inrisetime = outrisetime;
		    addr_inrisetime=outrisetime;

		    max_delay=0;
		    decoder_data_power=0;
		    decoder_data = decoder_delay(parameters->cache_size,parameters->block_size, parameters->associativity,
						 Ndwl,Ndbl,Nspd,
						 Ntwl,Ntbl,Ntspd,
						 &decoder_data_driver,&decoder_data_3to8,
						 &decoder_data_inv,
						 &outrisetime, 
						 inrisetime,
						 &data_nor_inputs,
						 &decoder_data_power);
		    max_delay=MAX(max_delay, decoder_data);
		    inrisetime = outrisetime;
		    decoder_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
		    wordline_data_power=0;
		    wordline_data = wordline_delay(parameters->block_size,
						   parameters->associativity,Ndwl,Nspd,
						   inrisetime,&outrisetime,
						   &wordline_data_power);
		    wordline_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
		    inrisetime = outrisetime;
		    max_delay=MAX(max_delay, wordline_data);

		    bitline_data_power=0;
#ifdef XCACTI
		    write_power_data=0;
		    bitline_data = bitline_delay(parameters->cache_size,parameters->associativity,
						 parameters->block_size,Ndwl,Ndbl,Nspd,
						 inrisetime,&outrisetime, &bitline_data_power, &write_power_data);
#else
		    bitline_data = bitline_delay(parameters->cache_size,parameters->associativity,
						 parameters->block_size,Ndwl,Ndbl,Nspd,
						 inrisetime,&outrisetime, &bitline_data_power, 0);
#endif
		    bitline_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
#ifdef XCACTI
		    write_power_data  *=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
#endif

		    inrisetime = outrisetime;
		    max_delay=MAX(max_delay, bitline_data);

		    {
		      int temp_row;
		      sense_amp_data_power=BITOUT*parameters->associativity*muxover/2;
		      temp_row = parameters->cache_size/(parameters->block_size*parameters->associativity*Ndbl*Nspd);
		      sense_amp_data = sense_amp_delay(parameters->associativity, inrisetime,&outrisetime, temp_row, &sense_amp_data_power);
		      sense_amp_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
		      max_delay=MAX(max_delay, sense_amp_data);
		    }
        
		    inrisetime = outrisetime;
        
		    data_output_power=0;
		    data_output = dataoutput_delay(parameters->cache_size,parameters->block_size,
						   parameters->associativity,parameters->fully_assoc,Ndbl,Nspd,Ndwl,
						   inrisetime,&outrisetime, &data_output_power);
		    data_output_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
		    max_delay=MAX(max_delay, data_output);
		    inrisetime = outrisetime;
           
		    total_out_driver_power=0;
            
		    subbank_v=0;
		    subbank_h=0;

		    subbank_routing_length(parameters->cache_size,parameters->block_size,parameters->associativity,parameters->fully_assoc,Ndbl,Nspd,Ndwl,Ntbl,Ntwl,Ntspd,parameters->NSubbanks,&subbank_v,&subbank_h) ;

		    if(parameters->NSubbanks > 2 ) {
		      total_out_driver = senseext_driver_delay(parameters->cache_size,parameters->block_size,parameters->associativity,parameters->fully_assoc,Ndbl,Nspd,Ndwl,Ntbl,Ntwl,Ntspd,inrisetime,&outrisetime, subbank_v, subbank_h, &total_out_driver_power);
		    }

		    total_out_driver_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
		    inrisetime = outrisetime;
		    max_delay=MAX(max_delay, total_out_driver);
 
            
		    /* if the associativity is 1, the data output can come right
		       after the sense amp.   Otherwise, it has to wait until 
		       the data access has been done. */
            
		    if (parameters->associativity==1) {
		      before_mux = subbankaddress_routing_delay +decoder_data + wordline_data + bitline_data +
			sense_amp_data + total_out_driver + data_output;
		      after_mux = 0;
		    } else {  
		      before_mux = subbankaddress_routing_delay+ decoder_data + wordline_data + bitline_data +
			sense_amp_data;
		      after_mux = data_output + total_out_driver;
		    }
            
            
		    /*
		     * Now worry about the tag side.
		     */
            
            
		    decoder_tag_power=0;
		    decoder_tag = decoder_tag_delay(parameters->cache_size,
						    parameters->block_size,parameters->associativity,
						    Ndwl,Ndbl,Nspd,Ntwl,Ntbl,Ntspd,parameters->NSubbanks,
						    &decoder_tag_driver,&decoder_tag_3to8,
						    &decoder_tag_inv,addr_inrisetime,&outrisetime,&tag_nor_inputs, &decoder_tag_power);
		    max_delay=MAX(max_delay, decoder_tag);
		    inrisetime = outrisetime;
		    decoder_tag_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
            
		    wordline_tag_power=0;
		    wordline_tag = wordline_tag_delay(parameters->cache_size,
						      parameters->associativity,Ntspd,Ntwl,parameters->NSubbanks,
						      inrisetime,&outrisetime, &wordline_tag_power);
		    max_delay=MAX(max_delay, wordline_tag);
		    inrisetime = outrisetime;
		    wordline_tag_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
            
		    bitline_tag_power=0;
#ifdef XCACTI
		    write_power_tag=0;
		    bitline_tag = bitline_tag_delay(parameters->cache_size,parameters->associativity,
						    parameters->block_size,Ntwl,Ntbl,Ntspd,parameters->NSubbanks,
						    inrisetime,&outrisetime, &bitline_tag_power, &write_power_tag);
#else
		    bitline_tag = bitline_tag_delay(parameters->cache_size,parameters->associativity,
						    parameters->block_size,Ntwl,Ntbl,Ntspd,parameters->NSubbanks,
						    inrisetime,&outrisetime, &bitline_tag_power, 0);
#endif
		    max_delay=MAX(max_delay, bitline_tag);
		    inrisetime = outrisetime;
		    bitline_tag_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
#ifdef XCACTI
		    write_power_tag  *=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
#endif
          
		    sense_amp_tag_power=ADDRESS_BITS+2 - (int)logtwo((double)parameters->cache_size) + (int)logtwo((double)parameters->associativity);

		    sense_amp_tag = sense_amp_tag_delay(inrisetime,&outrisetime,&sense_amp_tag_power);
          
		    max_delay=MAX(max_delay, sense_amp_tag);
		    inrisetime = outrisetime;
		    sense_amp_tag_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);


		    /* split comparator - look at half the address bits only */
		    compare_tag_power=0;
		    compare_tag = half_compare_time(parameters->cache_size,parameters->associativity,
						    Ntbl,Ntspd,parameters->NSubbanks,
						    inrisetime,&outrisetime, &compare_tag_power);
		    compare_tag_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
		    inrisetime = outrisetime;
		    max_delay=MAX(max_delay, compare_tag);
            
		    valid_driver_power=0;
		    mux_driver_power=0;
		    selb_power=0;
		    if (parameters->associativity == 1) {
		      mux_driver = 0;
		      valid_driver = valid_driver_delay(parameters->cache_size,parameters->block_size,
							parameters->associativity,parameters->fully_assoc,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters->NSubbanks,inrisetime,&valid_driver_power);
		      valid_driver_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
		      max_delay=MAX(max_delay, valid_driver);
		      time_till_compare = subbankaddress_routing_delay+ decoder_tag + wordline_tag + bitline_tag +
			sense_amp_tag;
              
		      time_till_select = time_till_compare+ compare_tag + valid_driver;
              
              
		      /*
		       * From the above info, calculate the total access time
		       */

		      if( PHASEDCACHE ){
			access_time = before_mux+after_mux+time_till_select;
		      }else{
			access_time = MAX(before_mux+after_mux,time_till_select);
		      }
              
		    } else {
              
		      /* default scale is full wirelength */

		      cols_subarray = (8*parameters->block_size*parameters->associativity*Nspd/Ndwl);
		      rows_subarray = (parameters->cache_size/(parameters->block_size*parameters->associativity*Ndbl*Nspd));

		      if(Ndwl*Ndbl==1) {
			l_muxdrv_v= 0;
			l_muxdrv_h= cols_subarray;
		      }
		      if(Ndwl*Ndbl==2 || Ndwl*Ndbl==4) {
			l_muxdrv_v= 0;
			l_muxdrv_h= 2*cols_subarray;
		      }

		      if(Ndwl*Ndbl>4) {
			htree=logtwo((double)(Ndwl*Ndbl));
			htree_int = (int) htree;
			if (htree_int % 2 ==0) {
			  exp = (htree_int/2-1);
			  l_muxdrv_v = (powers(2,exp)-1)*rows_subarray;
			  l_muxdrv_h = (int)sqrt(Ndwl*Ndbl)*cols_subarray;
			}
			else {
			  exp = (htree_int+1)/2-1;
			  l_muxdrv_v = (powers(2,exp)-1)*rows_subarray;
			  l_muxdrv_h = (int)sqrt(Ndwl*Ndbl/2)*cols_subarray;
			}
		      }

		      wirelength_v=(l_muxdrv_v);
		      wirelength_h=(l_muxdrv_h);
 
		      /* dualin mux driver - added for split comparator
			 - inverter replaced by nand gate */
		      mux_driver = mux_driver_delay_dualin(parameters->cache_size,
							   parameters->block_size,
							   parameters->associativity,
							   Ndbl,Nspd,Ndwl,Ntbl,Ntspd,
							   inrisetime,&outrisetime, 
							   wirelength_v,wirelength_h,&mux_driver_power);
		      max_delay=MAX(max_delay, mux_driver);
		      mux_driver_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
              
		      selb = selb_delay_tag_path(inrisetime,&outrisetime,&selb_power);
		      max_delay=MAX(max_delay, selb);
		      selb_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
              
		      valid_driver = 0;
              
		      time_till_compare = subbankaddress_routing_delay+decoder_tag + wordline_tag + bitline_tag +
			sense_amp_tag;
              
		      time_till_select = time_till_compare+ compare_tag + mux_driver
			+ selb;

		      if( PHASEDCACHE ){
			access_time = before_mux+time_till_select+after_mux;
		      }else{
			access_time = MAX(before_mux,time_till_select) +after_mux;
		      }
		    }
            
		    /*
		     * Calcuate the cycle time
		     */
            
		    // precharge_del = precharge_delay(wordline_data);
            
		    //      cycle_time = access_time + precharge_del;
            
		    cycle_time=access_time/WAVE_PIPE;
		    if (max_delay>cycle_time)
		      cycle_time=max_delay;

		    /*
		     * The parameters are for a 0.8um process.  A quick way to
		     * scale the results to another process is to divide all
		     * the results by FUDGEFACTOR.  Normally, FUDGEFACTOR is 1.
		     */

#ifdef XCACTI
		    /* Security checks */
		    if( bitline_data < 0 )
		      continue;
            
		    if( bitline_tag < 0 )
		      continue;
            
		    /*
		     * The parameters are for a 0.8um process.  A quick way to
		     * scale the results to another process is to divide all
		     * the results by FUDGEFACTOR.  Normally, FUDGEFACTOR is 1.
		     */
            
		    if( parameters->latchsa ) {
		      /* Fancy SA assumes a latched sense amp. Consist of three
			 modifications:

			 1-Latched Sense Amp: Once the time for the bitline and the
			 sense amp time is over I assume that it can be latched.
                 
			 2-Temperature margin: Since we need to give margin to for
			 temperature limits and fabrication margin, I give an extra
			 10%. The number is a CRUDE approximation that I get from
			 "Synonym Hit RAM - A 500Mhz CMOS SRAM Macro with 576-bit
			 Parallel Comparison and Parity Check Functions" - ISSC 2000

		      */
                        
		      sense_amp_data_power*=(wordline_data+bitline_data+sense_amp_data)*1.1*1e9/2;
                        
		      if (!parameters->fully_assoc){
			sense_amp_tag_power*=(wordline_tag+bitline_tag+sense_amp_tag)*1.1*1e9/2;
		      }
		    }else{
		      sense_amp_data_power*=access_time*1e9/2;
                        
		      if (!parameters->fully_assoc){
			sense_amp_tag_power*=access_time*1e9/2;
		      }
		    }

		    write_power_data   *=access_time*1e9/2;
		    write_power_tag    *=access_time*1e9/2;
#else
		    sense_amp_data_power+=(data_output + total_out_driver)*500e-6*5;
		    if (!parameters->fully_assoc)
		      sense_amp_tag_power+=(time_till_compare+ compare_tag + mux_driver+ selb +data_output + total_out_driver)*500e-6*5;
#endif

#ifdef XCACTI
		    if ( parameters->ignore_tag ) {
		      total_power=
			(subbankaddress_routing_power
			 +decoder_data_power
			 +wordline_data_power
			 +bitline_data_power
			 +sense_amp_data_power
			 +total_out_driver_power
			 // +decoder_tag_power
			 // +wordline_tag_power
			 // +bitline_tag_power
			 // +sense_amp_tag_power
			 // +compare_tag_power
			 // +valid_driver_power
			 +mux_driver_power
			 +selb_power
			 +data_output_power
			 )/FUDGEFACTOR;   
		      total_power_without_routing=
			(parameters->NSubbanks)
			*(decoder_data_power
			  +wordline_data_power
			  +bitline_data_power
			  +sense_amp_data_power
			  // +decoder_tag_power
			  // +wordline_tag_power
			  // +bitline_tag_power
			  // +sense_amp_tag_power
			  // +compare_tag_power
			  +mux_driver_power
			  +selb_power
			  +data_output_power
			  )/FUDGEFACTOR
			+valid_driver_power/FUDGEFACTOR;

		      total_power_allbanks = 
			total_power_without_routing 
			+total_address_routing_power/FUDGEFACTOR;
		    }else{
		      total_power=
			(subbankaddress_routing_power
			 +decoder_data_power
			 +wordline_data_power
			 +bitline_data_power
			 +sense_amp_data_power
			 +total_out_driver_power
			 +decoder_tag_power
			 +wordline_tag_power
			 +bitline_tag_power
			 +sense_amp_tag_power
			 +compare_tag_power
			 +valid_driver_power
			 +mux_driver_power
			 +selb_power
			 +data_output_power
			 )/FUDGEFACTOR;   
		      total_power_without_routing=
			(parameters->NSubbanks)
			*(decoder_data_power
			  +wordline_data_power
			  +bitline_data_power
			  +sense_amp_data_power
			  +decoder_tag_power
			  +wordline_tag_power
			  +bitline_tag_power
			  +sense_amp_tag_power
			  +compare_tag_power
			  +mux_driver_power
			  +selb_power
			  +data_output_power
			  )/FUDGEFACTOR
			+valid_driver_power/FUDGEFACTOR;

		      total_power_allbanks = 
			total_power_without_routing 
			+total_address_routing_power/FUDGEFACTOR;
		    }
#else
		    total_power=(subbankaddress_routing_power+decoder_data_power+wordline_data_power+bitline_data_power+sense_amp_data_power+total_out_driver_power+decoder_tag_power+wordline_tag_power+bitline_tag_power+sense_amp_tag_power+compare_tag_power+valid_driver_power+mux_driver_power+selb_power+data_output_power)/FUDGEFACTOR;   
		    total_power_without_routing=(parameters->NSubbanks)*(decoder_data_power+wordline_data_power+bitline_data_power+sense_amp_data_power+decoder_tag_power+wordline_tag_power+bitline_tag_power+sense_amp_tag_power+compare_tag_power+mux_driver_power+selb_power+data_output_power)/FUDGEFACTOR+valid_driver_power/FUDGEFACTOR;
		    total_power_allbanks= total_power_without_routing + total_address_routing_power/FUDGEFACTOR;
#endif

 
		    //      if (counter==1)
		    //        fprintf(stderr, "Pow - %f, Acc - %f, Pow - %f, Acc - %f, Combo - %f\n", total_power*1e9, access_time*1e9, total_power/result->max_power, access_time/result->max_access_time, total_power/result->max_power*access_time/result->max_access_time);

		    if (counter==1)
		      {
			// if ((result->total_power/result->max_power)/2+(result->access_time/result->max_access_time) > ((total_power/result->max_power)/2+access_time/(result->max_access_time*FUDGEFACTOR))) {

			if ((result->total_power/result->max_power)*4+(result->access_time/result->max_access_time)*2 + (1.0/arearesult->efficiency)*4+ (arearesult->aspect_ratio_total/max_aspect_ratio_total) > ((total_power/result->max_power)*4 +access_time/(result->max_access_time*FUDGEFACTOR)*2 + (1.0/efficiency)*4 + (arearesult_temp.aspect_ratio_total/max_aspect_ratio_total) )) {
			  //              if (result->access_time+1e-11*(result->best_Ndwl+result->best_Ndbl+result->best_Nspd+result->best_Ntwl+result->best_Ntbl+result->best_Ntspd) > access_time/FUDGEFACTOR+1e-11*(Ndwl+Ndbl+Nspd+Ntwl+Ntbl+Ntspd)) {
			  //if (result->access_time > access_time/FUDGEFACTOR) {

			  result->senseext_scale = senseext_scale;
			  result->total_power=total_power;
			  result->total_power_without_routing=total_power_without_routing;
			  result->total_routing_power=total_address_routing_power/FUDGEFACTOR;
			  result->total_power_allbanks=total_power_allbanks;

#ifdef XCACTI
			  if ( parameters->ignore_tag ) {
			    result->total_wrpower=
			      total_power
			      +(write_power_data - bitline_data_power)/FUDGEFACTOR;
			  }else{
			    result->total_wrpower=
			      total_power 
			      +(write_power_data - bitline_data_power)/FUDGEFACTOR
			      +(write_power_tag  - bitline_tag_power)/FUDGEFACTOR;
			  }
#endif

			  result->subbank_address_routing_delay = subbankaddress_routing_delay/FUDGEFACTOR;
			  result->subbank_address_routing_power = subbankaddress_routing_power/FUDGEFACTOR;

			  result->cycle_time = cycle_time/FUDGEFACTOR;
			  result->access_time = access_time/FUDGEFACTOR;
			  result->best_muxover = muxover;
			  result->best_Ndwl = Ndwl;
			  result->best_Ndbl = Ndbl;
			  result->best_Nspd = Nspd;
			  result->best_Ntwl = Ntwl;
			  result->best_Ntbl = Ntbl;
			  result->best_Ntspd = Ntspd;
			  result->decoder_delay_data = decoder_data/FUDGEFACTOR;
			  result->decoder_power_data = decoder_data_power/FUDGEFACTOR;
			  result->decoder_delay_tag = decoder_tag/FUDGEFACTOR;
			  result->decoder_power_tag = decoder_tag_power/FUDGEFACTOR;
			  result->dec_tag_driver = decoder_tag_driver/FUDGEFACTOR;
			  result->dec_tag_3to8 = decoder_tag_3to8/FUDGEFACTOR;
			  result->dec_tag_inv = decoder_tag_inv/FUDGEFACTOR;
			  result->dec_data_driver = decoder_data_driver/FUDGEFACTOR;
			  result->dec_data_3to8 = decoder_data_3to8/FUDGEFACTOR;
			  result->dec_data_inv = decoder_data_inv/FUDGEFACTOR;
			  result->wordline_delay_data = wordline_data/FUDGEFACTOR;
			  result->wordline_power_data = wordline_data_power/FUDGEFACTOR;
			  result->wordline_delay_tag = wordline_tag/FUDGEFACTOR;
			  result->wordline_power_tag = wordline_tag_power/FUDGEFACTOR;
			  result->bitline_delay_data = bitline_data/FUDGEFACTOR;
			  result->bitline_power_data = bitline_data_power/FUDGEFACTOR;
			  result->bitline_delay_tag = bitline_tag/FUDGEFACTOR;
			  result->bitline_power_tag = bitline_tag_power/FUDGEFACTOR;
			  result->sense_amp_delay_data = sense_amp_data/FUDGEFACTOR;
			  result->sense_amp_power_data = sense_amp_data_power/FUDGEFACTOR;
			  result->sense_amp_delay_tag = sense_amp_tag/FUDGEFACTOR;
			  result->sense_amp_power_tag = sense_amp_tag_power/FUDGEFACTOR;
			  result->total_out_driver_delay_data = total_out_driver/FUDGEFACTOR;
			  result->total_out_driver_power_data = total_out_driver_power/FUDGEFACTOR;
			  result->compare_part_delay = compare_tag/FUDGEFACTOR;
			  result->compare_part_power = compare_tag_power/FUDGEFACTOR;
			  result->drive_mux_delay = mux_driver/FUDGEFACTOR;
			  result->drive_mux_power = mux_driver_power/FUDGEFACTOR;
			  result->selb_delay = selb/FUDGEFACTOR;
			  result->selb_power = selb_power/FUDGEFACTOR;
			  result->drive_valid_delay = valid_driver/FUDGEFACTOR;
			  result->drive_valid_power = valid_driver_power/FUDGEFACTOR;
			  result->data_output_delay = data_output/FUDGEFACTOR;
			  result->data_output_power = data_output_power/FUDGEFACTOR;
			  result->precharge_delay = precharge_del/FUDGEFACTOR;
                  
			  result->data_nor_inputs = data_nor_inputs;
			  result->tag_nor_inputs = tag_nor_inputs;
			  xcacti_area_subbanked(ADDRESS_BITS,BITOUT,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters,arearesult_subbanked,arearesult);
			  arearesult->efficiency = (parameters->NSubbanks)*(area_all_dataramcells+area_all_tagramcells)*100/(xcacti_calculate_area(*arearesult_subbanked,parameters->fudgefactor)/100000000.0);
			  arearesult->aspect_ratio_total = (arearesult_subbanked->height/arearesult_subbanked->width);
			  arearesult->aspect_ratio_total = (arearesult->aspect_ratio_total > 1.0) ? (arearesult->aspect_ratio_total) : 1.0/(arearesult->aspect_ratio_total) ;
			  arearesult->max_efficiency = max_efficiency;
			  arearesult->max_aspect_ratio_total = max_aspect_ratio_total;
			}
		      }
		    else
		      {
			if (result->max_access_time < access_time/FUDGEFACTOR)  
			  result->max_access_time = access_time/FUDGEFACTOR; 
			if (result->max_power < total_power) 
			  result->max_power = total_power;
			if (arearesult_temp.max_efficiency < efficiency) {
			  arearesult_temp.max_efficiency = efficiency;
			  max_efficiency = efficiency; }
			if (min_efficiency > efficiency) 
			  min_efficiency = efficiency; 
			if (max_aspect_ratio_total < aspect_ratio_total_temp) 
			  max_aspect_ratio_total = aspect_ratio_total_temp;
		      }
		  }
         
		}
	      }
	    }
	  }
	}
      }
    } else {
      /* Fully associative model - only vary Ndbl|Ntbl */

      for (Ndbl=1;Ndbl<=MAXN;Ndbl=Ndbl*2) {
	Ntbl=Ndbl;
	Ndwl=Nspd=Ntwl=Ntspd=1;


	if (xcacti_organizational_parameters_valid
	    (parameters->block_size, 1, parameters->cache_size,Ndwl,Ndbl,Nspd,Ntwl,Ntbl,Ntspd,parameters->fully_assoc)) {

	  if (8*parameters->block_size/BITOUT == 1 && Ndbl*Nspd==1) {
	    muxover=1;
	  } 
	  else {
	    if (Ndbl*Nspd > MAX_COL_MUX)
	      {
		muxover=8*parameters->block_size/BITOUT;
	      }
	    else {
	      if (8*parameters->block_size*Ndbl*Nspd/BITOUT > MAX_COL_MUX)
		{
		  muxover=(8*parameters->block_size/BITOUT)/(MAX_COL_MUX/(Ndbl*Nspd));
		}
	      else
		{
		  muxover=1;
		}
	    }
	  }


	  xcacti_area_subbanked(ADDRESS_BITS,BITOUT,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters,&arearesult_subbanked_temp,&arearesult_temp);

	  Subbank_Efficiency= (area_all_dataramcells+area_all_tagramcells)*100/(arearesult_temp.totalarea/100000000.0);
	  Total_Efficiency= (parameters->NSubbanks)*(area_all_dataramcells+area_all_tagramcells)*100/(xcacti_calculate_area(arearesult_subbanked_temp,parameters->fudgefactor)/100000000.0);
	  // efficiency = Subbank_Efficiency;
	  efficiency = Total_Efficiency;
        
	  arearesult_temp.efficiency = efficiency;
	  aspect_ratio_total_temp = (arearesult_subbanked_temp.height/arearesult_subbanked_temp.width);
	  aspect_ratio_total_temp = (aspect_ratio_total_temp > 1.0) ? (aspect_ratio_total_temp) : 1.0/(aspect_ratio_total_temp) ;

	  arearesult_temp.aspect_ratio_total = aspect_ratio_total_temp;
 
	  bank_h=0;
	  bank_v=0;

	  subbank_dim(parameters->cache_size,parameters->block_size,parameters->associativity,parameters->fully_assoc,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters->NSubbanks,&bank_h,&bank_v);

	  subbanks_routing_power(parameters->fully_assoc,parameters->associativity,parameters->NSubbanks,&bank_h,&bank_v,&total_address_routing_power);
	  total_address_routing_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);

	  if(parameters->NSubbanks > 2 ) {
            subbankaddress_routing_delay= address_routing_delay(parameters->cache_size, parameters->block_size, parameters->associativity, parameters->fully_assoc, Ndwl, Ndbl, Nspd, Ntwl, Ntbl,Ntspd,parameters->NSubbanks,&outrisetime,&subbankaddress_routing_power);
	  }

	  subbankaddress_routing_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);

	  /* Calculate data side of cache */
	  inrisetime = outrisetime;
	  addr_inrisetime=outrisetime;

	  max_delay=0;
	  /* tag path contained here */
	  decoder_data_power=0;
	  decoder_data = fa_tag_delay(parameters->cache_size,
				      parameters->block_size,
				      Ndwl,Ndbl,Nspd,Ntwl,Ntbl,Ntspd,
				      &tag_delay_part1,&tag_delay_part2,
				      &tag_delay_part3,&tag_delay_part4,
				      &tag_delay_part5,&tag_delay_part6,
				      &outrisetime,&tag_nor_inputs, &decoder_data_power);
	  decoder_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
	  inrisetime = outrisetime;
	  max_delay=MAX(max_delay, decoder_data);
         

	  wordline_data_power=0;
	  wordline_data = wordline_delay(parameters->block_size,
					 1,Ndwl,Nspd,
					 inrisetime,&outrisetime, &wordline_data_power);
	  wordline_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
	  inrisetime = outrisetime;
	  max_delay=MAX(max_delay, wordline_data);
         
	  bitline_data_power=0;
#ifdef XCACTI
	  write_power_data=0;
	  bitline_data = bitline_delay(parameters->cache_size,1,
				       parameters->block_size,Ndwl,Ndbl,Nspd,
				       inrisetime,&outrisetime,&bitline_data_power,&write_power_data);
#else
	  bitline_data = bitline_delay(parameters->cache_size,1,
				       parameters->block_size,Ndwl,Ndbl,Nspd,
				       inrisetime,&outrisetime,&bitline_data_power,0);
#endif
	  bitline_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
#ifdef XCACTI
	  write_power_data  *=(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports);
#endif

	  inrisetime = outrisetime;
	  max_delay=MAX(max_delay, bitline_data);
         
	  {
	    int temp_row;
	    temp_row = parameters->cache_size/(parameters->block_size*Ndbl);
	    sense_amp_data_power=BITOUT*muxover/2;
	    sense_amp_data = sense_amp_delay(parameters->associativity, inrisetime,&outrisetime, temp_row, &sense_amp_data_power);
	    sense_amp_data_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
	    max_delay=MAX(max_delay, sense_amp_data);
	  }
	  inrisetime = outrisetime;
        
	  data_output_power=0;
	  data_output = dataoutput_delay(parameters->cache_size,parameters->block_size,1,parameters->fully_assoc,Ndbl,Nspd,Ndwl,inrisetime,&outrisetime, &data_output_power);
	  data_output_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
        
	  inrisetime = outrisetime;                           
	  max_delay=MAX(max_delay, data_output);

	  total_out_driver_power=0;

	  subbank_v=0;
	  subbank_h=0;

	  subbank_routing_length(parameters->cache_size,parameters->block_size,parameters->associativity,parameters->fully_assoc,Ndbl,Nspd,Ndwl,Ntbl,Ntwl,Ntspd,parameters->NSubbanks,&subbank_v,&subbank_h) ;

	  if(parameters->NSubbanks > 2 ) {
	    total_out_driver = senseext_driver_delay(parameters->cache_size,parameters->block_size,parameters->associativity,parameters->fully_assoc,Ndbl,Nspd,Ndwl,Ntbl,Ntwl,Ntspd,inrisetime,&outrisetime, subbank_v, subbank_h, &total_out_driver_power);
	  }

	  total_out_driver_power*=(parameters->num_readwrite_ports+parameters->num_read_ports);
	  inrisetime = outrisetime;
	  max_delay=MAX(max_delay, total_out_driver);
         
	  access_time=subbankaddress_routing_delay+ decoder_data+wordline_data+bitline_data+sense_amp_data+data_output+total_out_driver;
         
	  /*
	   * Calcuate the cycle time
	   */
         
	  //      precharge_del = precharge_delay(wordline_data);
         
	  //      cycle_time = access_time + precharge_del;
         
	  cycle_time=access_time/WAVE_PIPE;
	  if (max_delay>cycle_time)
	    cycle_time=max_delay;
         
	  /*
	   * The parameters are for a 0.8um process.  A quick way to
	   * scale the results to another process is to divide all
	   * the results by FUDGEFACTOR.  Normally, FUDGEFACTOR is 1.
	   */

#ifdef XCACTI
	  if( parameters->latchsa ) {
	    /* See previous Fancy SA comment */
	    sense_amp_data_power*=(wordline_data+bitline_data+sense_amp_data)*1.1*1e9*0.5/2;
	  }else{
	    sense_amp_data_power*=access_time*1e9/2;
	  }
	  write_power_data    *=access_time*1e9/2;
#else         
	  sense_amp_data_power+=(data_output + total_out_driver)*500e-6*5;
#endif

	  // Fully-assoc model does not have ignore_tag parameter

	  total_power=(subbankaddress_routing_power+decoder_data_power+wordline_data_power+bitline_data_power+sense_amp_data_power+data_output_power+total_out_driver_power)/FUDGEFACTOR;          
	  total_power_without_routing=(parameters->NSubbanks)*(decoder_data_power+wordline_data_power+bitline_data_power+sense_amp_data_power+data_output_power)/FUDGEFACTOR;
	  total_power_allbanks= total_power_without_routing + total_address_routing_power/FUDGEFACTOR;

	  if (counter==1)
	    {
              // if ((result->total_power/result->max_power)/2+(result->access_time/result->max_access_time) > ((total_power/result->max_power)/2+access_time/(result->max_access_time*FUDGEFACTOR))) {

              if ((result->total_power/result->max_power)/2+(result->access_time/result->max_access_time) + (1.0/arearesult->efficiency)*10 + (arearesult->aspect_ratio_total/max_aspect_ratio_total)/(4*10) > ((total_power/result->max_power)/2 +access_time/(result->max_access_time*FUDGEFACTOR) + (1.0/efficiency)*10 + (arearesult_temp.aspect_ratio_total/max_aspect_ratio_total)/(4*10) )) {
		// if ((result->total_power/result->max_power)/2+(result->access_time/result->max_access_time) + (min_efficiency/arearesult->efficiency)/4 + (arearesult->aspect_ratio_total/max_aspect_ratio_total)/3 > ((total_power/result->max_power)/2+access_time/(result->max_access_time*FUDGEFACTOR)+ (min_efficiency/efficiency)/4 + (arearesult_temp.aspect_ratio_total/max_aspect_ratio_total)/3)) {

		//          if (result->cycle_time+1e-11*(result->best_Ndwl+result->best_Ndbl+result->best_Nspd+result->best_Ntwl+result->best_Ntbl+result->best_Ntspd) > cycle_time/FUDGEFACTOR+1e-11*(Ndwl+Ndbl+Nspd+Ntwl+Ntbl+Ntspd)) {
            
 
		result->senseext_scale = senseext_scale;
		result->total_power=total_power;
		result->total_power_without_routing=total_power_without_routing;
		result->total_routing_power=total_address_routing_power/FUDGEFACTOR;
		result->total_power_allbanks=total_power_allbanks;

#ifdef XCACTI
		// FA no tags
		result->total_wrpower=total_power + (write_power_data - bitline_data_power)/FUDGEFACTOR;
#endif

		result->subbank_address_routing_delay = subbankaddress_routing_delay/FUDGEFACTOR;
		result->subbank_address_routing_power = subbankaddress_routing_power/FUDGEFACTOR;

		result->cycle_time = cycle_time/FUDGEFACTOR;
		result->access_time = access_time/FUDGEFACTOR;
		result->best_Ndwl = Ndwl;
		result->best_Ndbl = Ndbl;
		result->best_Nspd = Nspd;
		result->best_Ntwl = Ntwl;
		result->best_Ntbl = Ntbl;
		result->best_Ntspd = Ntspd;
		result->decoder_delay_data = decoder_data/FUDGEFACTOR;
		result->decoder_power_data = decoder_data_power/FUDGEFACTOR;
		result->decoder_delay_tag = decoder_tag/FUDGEFACTOR;
		result->dec_tag_driver = decoder_tag_driver/FUDGEFACTOR;
		result->dec_tag_3to8 = decoder_tag_3to8/FUDGEFACTOR;
		result->dec_tag_inv = decoder_tag_inv/FUDGEFACTOR;
		result->dec_data_driver = decoder_data_driver/FUDGEFACTOR;
		result->dec_data_3to8 = decoder_data_3to8/FUDGEFACTOR;
		result->dec_data_inv = decoder_data_inv/FUDGEFACTOR;
		result->wordline_delay_data = wordline_data/FUDGEFACTOR;
		result->wordline_power_data = wordline_data_power/FUDGEFACTOR;
		result->wordline_delay_tag = wordline_tag/FUDGEFACTOR;
		result->bitline_delay_data = bitline_data/FUDGEFACTOR;
		result->bitline_power_data = bitline_data_power/FUDGEFACTOR;
		result->bitline_delay_tag = bitline_tag/FUDGEFACTOR;
		result->sense_amp_delay_data = sense_amp_data/FUDGEFACTOR;
		result->sense_amp_power_data = sense_amp_data_power/FUDGEFACTOR;
		result->sense_amp_delay_tag = sense_amp_tag/FUDGEFACTOR;
		result->total_out_driver_delay_data = total_out_driver/FUDGEFACTOR;
		result->total_out_driver_power_data = total_out_driver_power/FUDGEFACTOR;
		result->compare_part_delay = compare_tag/FUDGEFACTOR;
		result->drive_mux_delay = mux_driver/FUDGEFACTOR;
		result->selb_delay = selb/FUDGEFACTOR;
		result->drive_valid_delay = valid_driver/FUDGEFACTOR;
		result->data_output_delay = data_output/FUDGEFACTOR;
		result->data_output_power = data_output_power/FUDGEFACTOR;
		result->precharge_delay = precharge_del/FUDGEFACTOR;
               
		result->data_nor_inputs = data_nor_inputs;
		result->tag_nor_inputs = tag_nor_inputs;

		xcacti_area_subbanked(ADDRESS_BITS,BITOUT,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters,arearesult_subbanked,arearesult);
		arearesult->efficiency = (parameters->NSubbanks)*(area_all_dataramcells+area_all_tagramcells)*100/(xcacti_calculate_area(*arearesult_subbanked,parameters->fudgefactor)/100000000.0);
		arearesult->aspect_ratio_total = (arearesult_subbanked->height/arearesult_subbanked->width);
		arearesult->aspect_ratio_total = (arearesult->aspect_ratio_total > 1.0) ? (arearesult->aspect_ratio_total) : 1.0/(arearesult->aspect_ratio_total) ;
		arearesult->max_efficiency = max_efficiency;
		arearesult->max_aspect_ratio_total = max_aspect_ratio_total;

	      }
	    }
	  else
	    {

	      if (result->max_access_time < access_time/FUDGEFACTOR) 
		result->max_access_time = access_time/FUDGEFACTOR;
	      if (result->max_power < total_power) 
		result->max_power = total_power;
	      if (arearesult_temp.max_efficiency < efficiency) {
		arearesult_temp.max_efficiency = efficiency;
                max_efficiency = efficiency; }
	      if (min_efficiency > efficiency) {
		min_efficiency = efficiency; }
	      if (max_aspect_ratio_total < aspect_ratio_total_temp)
		max_aspect_ratio_total = aspect_ratio_total_temp;

	    }
	}
       
      }
    }
  }
}
