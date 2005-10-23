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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "xcacti_def.h"
#include "xcacti_area.h"

double area_all_datasubarrays;
double area_all_tagsubarrays;
double area_all_dataramcells;
double area_all_tagramcells;
double faarea_all_subarrays ;

double aspect_ratio_data;
double aspect_ratio_tag;
double aspect_ratio_subbank;
double aspect_ratio_total;

static double logtwo_area(double x) {

  if (x<=0) 
    printf("ERROR: nevative log %e\n",x);

  return( (double) (log(x)/log(2.0)) );
}


static area_type inverter_area(double Widthp, 
			       double Widthn) {

  double Width_n,Width_p;
  area_type invarea;
  int foldp=0, foldn=0;
  if (Widthp>10.0) { 
    Widthp=Widthp/2;
    foldp=1;
  }
  if (Widthn>10.0) { 
    Widthn=Widthn/2;
    foldn=1;
  }

  invarea.height = Widthp + Widthn + Widthptondiff + 2*Widthtrack;
  Width_n = (foldn) ? (3*Widthcontact+2*(Wpoly+2*ptocontact)) : (2*Widthcontact+Wpoly+2*ptocontact) ;
  Width_p = (foldp) ? (3*Widthcontact+2*(Wpoly+2*ptocontact)) : (2*Widthcontact+Wpoly+2*ptocontact) ;
  invarea.width = MAX(Width_n,Width_p);

  return invarea;
}

static area_type subarraymem_area(int C, 
				  int B, 
				  int A, 
				  int Ndbl, 
				  int Ndwl, 
				  int Nspd,
				  int RWP,
				  int ERP,
				  int EWP,
				  int NSER,
				  double techscaling_factor) {
  
  area_type memarea;
  int noof_rows, noof_colns;

  noof_rows = (C/(B*A*Ndbl*Nspd));
  noof_colns = (8*B*A*Nspd/Ndwl);
  memarea.height = ceil((double)(noof_rows)/16.0)*stitch_ramv+(BitHeight16x2+2*Widthtrack*2*(RWP+ERP+EWP-1))*ceil((double)(noof_rows)/2.0);
  memarea.width  = ceil((double)(noof_colns)/16.0)*(BitWidth16x2+16*(Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER));

  area_all_dataramcells=Ndwl*Ndbl*xcacti_calculate_area(memarea,techscaling_factor)/100000000.0;
  return(memarea);
}

static area_type decodemem_row(int C,
			       int B,
			       int A,
			       int Ndbl,
			       int Ndwl,
			       int Nspd,
			       int RWP,
			       int ERP,
			       int EWP) {

  int noof_colns,numstack;
  double decodeNORwidth;
  double desiredrisetime, Cline, Rpdrive, psize,nsize;
  area_type decinv,worddriveinv,postdecodearea;

  noof_colns = 8*B*A*Nspd/Ndwl;
  desiredrisetime = krise*log((double)(noof_colns))/2.0;
  Cline = (2*Wmemcella*Leff*Cgatepass+Cwordmetal)*noof_colns;
  Rpdrive = desiredrisetime/(Cline*log(VSINV)*-1.0);
  psize = Rpchannelon/Rpdrive;
  if (psize > Wworddrivemax) {
    psize = Wworddrivemax;
  }
  numstack = (int)ceil((1.0/3.0)*logtwo_area( (double)((double)C/(double)(B*A*Ndbl*Nspd))));
  if (numstack==0) numstack = 1;
  if (numstack>5) numstack = 5;
  switch(numstack) {
  case 1: decodeNORwidth = WidthNOR1; break;
  case 2: decodeNORwidth = WidthNOR2; break;
  case 3: decodeNORwidth = WidthNOR3; break;
  case 4: decodeNORwidth = WidthNOR4; break;
  case 5: decodeNORwidth = WidthNOR4; break;
  default: printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;

  }
  nsize = psize * Wdecinvn/Wdecinvp;
  decinv = inverter_area(Wdecinvp,Wdecinvn);
  worddriveinv =inverter_area(psize,nsize);
  postdecodearea.height = (BitHeight16x2+2*Widthtrack*2*(RWP+ERP+EWP-1));
  postdecodearea.width  = (decodeNORwidth + decinv.height + worddriveinv.height)*(RWP+ERP+EWP);
  return(postdecodearea);
}	

/* this puts the different predecode blocks for the different ports side by side
   and does not put them as an array or something */
static area_type predecode_area(int noof_rows,
				int RWP,
				int ERP,
				int EWP) {

  area_type predecode, predecode_temp;

  int N3to8 = (int)ceil((1.0/3.0)*logtwo_area( (double) (noof_rows)));
  if (N3to8==0) {N3to8=1;}

  switch(N3to8) {
  case 1: predecode_temp.height=Predec_height1;
    predecode_temp.width=Predec_width1;
    break;
  case 2: predecode_temp.height=Predec_height2; 
    predecode_temp.width=Predec_width2;
    break;
  case 3: predecode_temp.height=Predec_height3; 
    predecode_temp.width=Predec_width3;
    break;
  case 4: predecode_temp.height=Predec_height4; 
    predecode_temp.width=Predec_width4;
    break;
  case 5: predecode_temp.height=Predec_height5; 
    predecode_temp.width=Predec_width5;
    break;
  case 6: predecode_temp.height=Predec_height6;
    predecode_temp.width=Predec_width6;
    break;
  default: printf("error:N3to8=%d\n",N3to8);
    exit(0);

  }

  predecode.height=predecode_temp.height;
  predecode.width=predecode_temp.width*(RWP+ERP+EWP);
  return(predecode);	
}

static area_type postdecode_area(int noof_rows, 
				 int RWP,
				 int ERP,
				 int EWP) { 

  int numstack = (int)ceil((1.0/3.0)*logtwo_area((double)(noof_rows)));

  if (numstack==0) 
    numstack = 1;
  else if (numstack>5) 
    numstack = 5;

  double decodeNORwidth;
  switch(numstack) {
  case 1: decodeNORwidth = WidthNOR1; break;
  case 2: decodeNORwidth = WidthNOR2; break;
  case 3: decodeNORwidth = WidthNOR3; break;
  case 4: decodeNORwidth = WidthNOR4; break;
  case 5: decodeNORwidth = WidthNOR4; break;
  default: 
    printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;
  }

  area_type decinverter = inverter_area(Wdecinvp,Wdecinvn);
  area_type postdecode;

  postdecode.height=(BitHeight+Widthtrack*2*(RWP+ERP+EWP-1))*noof_rows;
  postdecode.width =(2*decinverter.height+ decodeNORwidth)*(RWP+ERP+EWP);

  return postdecode;
}

/* gives the height of the colmux */
static area_type colmux(int Ndbl, 
			int Nspd, 
			int RWP, 
			int ERP, 
			int EWP, 
			int NSER) { 

  area_type colmux_area;
  colmux_area.height = (2*Wbitmuxn+3*(2*Widthcontact+1))*(RWP+ERP+EWP);
  colmux_area.width  = (BitWidth+Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER); 

  return colmux_area;
}

static area_type precharge(int Ndbl, 
			   int Nspd, 
			   int RWP,
			   int ERP,
			   int EWP,
			   int NSER) {

  area_type precharge_area;

  if (Ndbl*Nspd > 1) {
    precharge_area.height =(Wbitpreequ+2*Wbitdropv+Wwrite+2*(2*Widthcontact+
							     1)+3*Widthptondiff)*0.5*(RWP+EWP);
    precharge_area.width  = 2*(BitWidth+Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER); }
  else {
    precharge_area.height = (Wbitpreequ+2*Wbitdropv+Wwrite+2*(2*Widthcontact+1)+3*Widthptondiff)*(RWP+EWP);
    precharge_area.width  = BitWidth+Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER; 
  }

  return precharge_area;
}

static area_type senseamp(int Ndbl,
			  int Nspd, 
			  int RWP,
			  int ERP,
			  int EWP,
			  int NSER) {

  area_type senseamp_area;
  if (Ndbl*Nspd > 1) {
    senseamp_area.height =0.5*SenseampHeight*(RWP+ERP);
    senseamp_area.width  = 2*(BitWidth+Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER); }
  else {
    senseamp_area.height = SenseampHeight*(RWP+ERP);
    senseamp_area.width  = BitWidth+Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER; 
  }

  return senseamp_area;
}

/* define OutdriveHeight OutdriveWidth DatainvHeight DatainvWidth */
static area_type subarraytag_area(int baddr, 
				  int C, int B, int A,
				  int Ntdbl, int Ntdwl, int Ntspd,
				  double NSubbanks,
				  int RWP, int ERP, int EWP, int NSER,
				  double techscaling_factor) {
  area_type tagarea;
  int noof_rows, noof_colns, Tagbits;
  int conservative_NSER;
	
  conservative_NSER =0;
	
  Tagbits = baddr-(int)(logtwo_area((double)(C)))+(int)(logtwo_area((double)(A)))+2-(int)(logtwo_area(NSubbanks));
       
  noof_rows = (C/(B*A*Ntdbl*Ntspd));
  noof_colns = (Tagbits*A*Ntspd/Ntdwl);
  tagarea.height = (int)ceil((double)(noof_rows)/16.0)*stitch_ramv+(BitHeight16x2+2*Widthtrack*2*(RWP+ERP+EWP-1))*ceil((double)(noof_rows)/2.0);
  tagarea.width  = (int)ceil((double)(noof_colns)/16.0)*(BitWidth16x2+16*(Widthtrack*2*(RWP+(ERP-conservative_NSER)+EWP-1)+Widthtrack*conservative_NSER));

  area_all_tagramcells=Ntdwl*Ntdbl*xcacti_calculate_area(tagarea,techscaling_factor)/100000000.0;

  return tagarea;
}

static area_type decodetag_row(int baddr,
			       int C, int B, int A,
			       int Ntdbl, int Ntdwl, int Ntspd,
			       double NSubbanks,
			       int RWP, int ERP, int EWP) {

  int numstack, Tagbits;
  double decodeNORwidth;
  area_type decinv,worddriveinv,postdecodearea;

  Tagbits = baddr-
    (int)logtwo_area((double)(C))+
    (int)logtwo_area((double)(A))+2-
    (int)logtwo_area(NSubbanks);
       
  numstack = (int)ceil((1.0/3.0)*logtwo_area( (double)((double)C/(double)(B*A*Ntdbl*Ntspd))));

  if (numstack==0) numstack = 1;
  if (numstack>5) numstack = 5;
  switch(numstack) {
  case 1: decodeNORwidth = WidthNOR1; break;
  case 2: decodeNORwidth = WidthNOR2; break;
  case 3: decodeNORwidth = WidthNOR3; break;
  case 4: decodeNORwidth = WidthNOR4; break;
  case 5: decodeNORwidth = WidthNOR4; break;
  default: printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;

  }

  decinv = inverter_area(Wdecinvp,Wdecinvn);
  worddriveinv =inverter_area(Wdecinvp,Wdecinvn);
  postdecodearea.height = (BitHeight16x2+2*Widthtrack*2*(RWP+ERP+EWP-1));
  postdecodearea.width  = (decodeNORwidth + decinv.height + worddriveinv.height)*(RWP+ERP+EWP);

  return postdecodearea;
}

static area_type comparatorbit(int RWP,
			       int ERP,
			       int EWP) {
  
  area_type compbit_area;

  compbit_area.width = 3*Widthcontact+2*(3*Wpoly+2*ptocontact);
  compbit_area.height= (Wcompn+2*(2*Widthcontact+1))*(RWP+ERP);

  return compbit_area;
}

static area_type muxdriverdecode(int B, 
				 int b0, 
				 int RWP, int ERP, int EWP) {      
  
  int noof_rows;	
  area_type muxdrvdecode_area, predecode,postdecode;

  noof_rows=(8*B)/b0;
  predecode=predecode_area(noof_rows,RWP,ERP,EWP);
  postdecode=postdecode_area(noof_rows,RWP,ERP,EWP);
  muxdrvdecode_area.width=predecode.height+postdecode.width+noof_rows*Widthtrack*(RWP+ERP+EWP);
  muxdrvdecode_area.height=MAX(predecode.width,postdecode.height);

  return muxdrvdecode_area;
}
 
/* generates the 8B/b0*A signals */
static area_type muxdrvsig(int A, int B, int b0) {

  int noof_rows;
  area_type outdrvsig_area;
  area_type muxdrvsig_area;
  noof_rows=(8*B)/b0; 
  //debug
  muxdrvsig_area.height = 0;
  muxdrvsig_area.width = 0;

  outdrvsig_area.height=0.5*(WmuxdrvNORn+WmuxdrvNORp)+9*Widthcontact+0.5*(Wmuxdrv3n+Wmuxdrv3p)+Widthptondiff+3*Widthcontact;
  outdrvsig_area.width =(3*Widthcontact+2*(3*Wpoly+2*ptocontact))*noof_rows;
  switch(A) {
  case 1:  
    muxdrvsig_area.height=outdrvsig_area.height+noof_rows*Widthtrack*2+A*Widthtrack;
    muxdrvsig_area.width = outdrvsig_area.width+noof_rows*Widthtrack+A*Widthtrack;
    break;
  case 2: 
    muxdrvsig_area.height= outdrvsig_area.height*2+noof_rows*Widthtrack*3+A*Widthtrack;     
    muxdrvsig_area.width = outdrvsig_area.width+noof_rows*Widthtrack+A*Widthtrack;
    break;
  case 4: 
    muxdrvsig_area.height= outdrvsig_area.height*2+noof_rows*Widthtrack*5+A*Widthtrack; 
    muxdrvsig_area.width = outdrvsig_area.width*2+noof_rows*Widthtrack+A*Widthtrack;
    break;
  case 8:
    muxdrvsig_area.height= outdrvsig_area.height*2+noof_rows*Widthtrack*9+A*Widthtrack; 
    muxdrvsig_area.width = outdrvsig_area.width*4+noof_rows*Widthtrack+A*Widthtrack;
    break;
  case 16: 
    muxdrvsig_area.height= outdrvsig_area.height*4+noof_rows*Widthtrack*18+A*Widthtrack; 
    muxdrvsig_area.width = outdrvsig_area.width*4+noof_rows*Widthtrack+A*Widthtrack;
    break;
  case 32: 
    muxdrvsig_area.height= outdrvsig_area.height*4+noof_rows*Widthtrack*35+2*A*Widthtrack; 
    muxdrvsig_area.width = 2*(outdrvsig_area.width*4+noof_rows*Widthtrack+A*Widthtrack);
    break;
  default: printf("error:Associativity=%d\n",A);
  }

  return(muxdrvsig_area);
}

	  
static area_type datasubarray(int C, int B, int A,
			      int Ndbl, int Ndwl, int Nspd,
			      int RWP, int ERP, int EWP, int NSER,
			      double techscaling_factor) {

  area_type datasubarray_area,mem_area,postdecode_area,colmux_area,precharge_area,senseamp_area;

  mem_area=subarraymem_area(C,B,A,Ndbl,Ndwl,Nspd,RWP,ERP,EWP,NSER,techscaling_factor);
  postdecode_area=decodemem_row(C,B,A,Ndbl,Nspd,Ndwl,RWP,ERP,EWP);
  colmux_area=colmux(Ndbl, Nspd,RWP,ERP,EWP,NSER);
  precharge_area=precharge(Ndbl,Nspd,RWP,ERP,EWP,NSER);
  senseamp_area=senseamp(Ndbl,Nspd,RWP,ERP,EWP,NSER);
  datasubarray_area.height=mem_area.height+colmux_area.height+precharge_area.height+senseamp_area.height+DatainvHeight*(RWP+EWP)+OutdriveHeight*(RWP+ERP);
  datasubarray_area.width =mem_area.width+postdecode_area.width;

  return datasubarray_area;
}
 
static area_type datasubblock(int C, int B, int A,
			      int Ndbl, int Ndwl, int Nspd, 
			      int SB, int b0, 
			      int RWP, int ERP, int EWP, int NSER,
			      double techscaling_factor) {

  int colmuxtracks_rem, outrdrvtracks_rem, writeseltracks_rem;
  int SB_ ;
  double tracks_h, tracks_w;
  area_type datasubarray_area,datasubblock_area;
  SB_ =SB;
  if (SB_ == 0) { 
    SB_ = 1; 
  }
  
  colmuxtracks_rem = (Ndbl*Nspd >tracks_precharge_p) ? (Ndbl*Nspd-tracks_precharge_p) : 0;
  outrdrvtracks_rem = ((2*B*A)/(b0) > tracks_outdrvselinv_p) ? ((2*B*A)/(b0)-tracks_outdrvselinv_p) : 0;
  writeseltracks_rem = ((2*B*A)/(b0) > tracks_precharge_nx2) ? ((2*B*A)/(b0)-tracks_precharge_nx2) : 0;

  int N3to8= (int)ceil((1.0/3.0)*logtwo_area( (double)(C/(B*A*Ndbl*Nspd))));
  
  if (N3to8==0) {
    N3to8=1;
  }
	
  tracks_h=Widthtrack*(N3to8*8*(RWP+ERP+EWP)+(RWP+EWP)*colmuxtracks_rem+Ndbl*Nspd*ERP+4*outrdrvtracks_rem*(RWP+ERP)+4*writeseltracks_rem*(RWP+EWP)+(RWP+ERP+EWP)*b0/SB_);
  tracks_w=Widthtrack*(N3to8*8)*(RWP+ERP+EWP);
  datasubarray_area=datasubarray(C,B,A,Ndbl,Ndwl,Nspd,RWP,ERP,EWP,NSER,techscaling_factor);
  datasubblock_area.height=2*datasubarray_area.height+tracks_h; 
  datasubblock_area.width =2*datasubarray_area.width +tracks_w;

  return datasubblock_area;
}

static area_type dataarray(int C, int B, int A,
			   int Ndbl, int Ndwl, int Nspd,
			   int b0, int RWP, int ERP, int EWP, int NSER,
			   double techscaling_factor) {

  area_type dataarray_area, datasubarray_area, datasubblock_area;
  area_type temp;
  double temp_aspect;
  double fixed_tracks_internal,fixed_tracks_external,variable_tracks;
  double data,driver_select,colmux,predecode,addresslines;
  int blocks, htree, htree_half, i, multiplier, iter_height;
  double inter_height,inter_width, total_height, total_width;
	
  int SB = Ndwl*Ndbl/4;
  int N3to8 = (int)ceil((1.0/3.0)*logtwo_area( (double)(C/(B*A*Ndbl*Nspd))));
  if (N3to8==0) {
    N3to8=1;
  }

  data=b0*(RWP+ERP+EWP)*Widthtrack;
  driver_select=(2*RWP+ERP+EWP)*8*B*A/b0*Widthtrack;
  colmux=Ndbl*Nspd*(RWP+EWP+ERP)*Widthtrack;
  predecode=(RWP+ERP+EWP)*N3to8*8*Widthtrack;
  addresslines=ADDRESS_BITS*(RWP+ERP+EWP)*Widthtrack;

  fixed_tracks_internal=colmux+predecode+driver_select;
  fixed_tracks_external=colmux+driver_select+addresslines;
  variable_tracks=data;

  datasubarray_area=datasubarray(C,B,A,Ndbl,Ndwl,Nspd,RWP,ERP,EWP,NSER,techscaling_factor);
  datasubblock_area=datasubblock(C,B,A,Ndbl,Ndwl,Nspd,SB,b0,RWP,ERP,EWP,NSER,techscaling_factor);
  area_all_datasubarrays=Ndbl*Ndwl*xcacti_calculate_area(datasubarray_area,techscaling_factor)/100000000.0;


  if(SB==0) {
    if(Ndbl*Ndwl==1) {
      total_height=datasubarray_area.height+fixed_tracks_external+data;
      total_width =datasubarray_area.width+predecode;
    }else {
      total_height=2*datasubarray_area.height+fixed_tracks_external+data;
      total_width =datasubarray_area.width+predecode; 
    }
  }else if(SB==1) {
    total_height=datasubblock_area.height;
    total_width =datasubblock_area.width;
  }else if(SB==2) {
    total_height=datasubblock_area.height;
    total_width = 2*datasubblock_area.width + fixed_tracks_external+data ;	
  }else if(SB==4) {
    total_height=2*datasubblock_area.height+fixed_tracks_external+data;
    total_width =2*datasubblock_area.width+fixed_tracks_internal+variable_tracks/2;
  }else if(SB==8) {
    total_height=2*datasubblock_area.height+fixed_tracks_internal+variable_tracks/2;
    total_width =2*(2*datasubblock_area.width+variable_tracks/4)+fixed_tracks_external+data;
  }else {
    blocks = SB / 4;
    htree = (int) (logtwo_area((double) (blocks)));
    inter_height = datasubblock_area.height;
    inter_width = datasubblock_area.width;
    multiplier =1;
 
    total_height = 0;
    total_width  = 0;

    if ( htree % 2 == 0) {
      iter_height = htree/2; 
    } 

    if (htree % 2 == 0) {
      for (i=0;i<=iter_height;i++) {
	if (i==iter_height) {
	  total_height = 2*inter_height+data/blocks*multiplier+fixed_tracks_external; 
	  total_width = 2*inter_width + data/(2*blocks)*multiplier+fixed_tracks_internal; 
	} else {
	  total_height = 2*inter_height+data/blocks*multiplier+fixed_tracks_internal;
	  total_width = 2*inter_width+data/(2*blocks)*multiplier+fixed_tracks_internal;    
	  inter_height = total_height ;
	  inter_width = total_width ;
	  multiplier = multiplier*4;
	}
      }
    } else {
      htree_half = htree-1;
      iter_height = htree_half/2; 
      for (i=0;i<=iter_height;i++) {
	total_height = 2*inter_height+data/blocks*multiplier+fixed_tracks_internal;
	total_width = 2*inter_width+data/(2*blocks)*multiplier+fixed_tracks_internal;
	inter_height = total_height ;
	inter_width = total_width ;
	multiplier = multiplier*4;
      }
      total_width = 2*inter_width+data/(2*blocks)*multiplier+fixed_tracks_external;
    }
  }

  dataarray_area.width = total_width;
  dataarray_area.height =total_height;

  temp.height =dataarray_area.width ;
  temp.width  = dataarray_area.height;

  temp_aspect = ((temp.height/temp.width) > 1.0) ? (temp.height/temp.width) : 1.0/(temp.height/temp.width);
  aspect_ratio_data = ((dataarray_area.height/dataarray_area.width) > 1.0) ? (dataarray_area.height/dataarray_area.width) : 1.0/(dataarray_area.height/dataarray_area.width) ;
  if (aspect_ratio_data > temp_aspect) {
    dataarray_area.height = temp.height;
    dataarray_area.width  = temp.width ;
  }

  aspect_ratio_data = ((dataarray_area.height/dataarray_area.width) > 1.0) ? (dataarray_area.height/dataarray_area.width) : 1.0/(dataarray_area.height/dataarray_area.width) ;

  return(dataarray_area);
}

static area_type tagsubarray(int baddr, 
			     int C, int B, int A,
			     int Ndbl, int Ndwl, int Nspd,
			     double NSubbanks,
			     int RWP, int ERP, int EWP, int NSER,
			     double techscaling_factor) {

  int conservative_NSER;
  area_type tagsubarray_area,tag_area,postdecode_area,colmux_area,precharge_area,senseamp_area,comp_area;

  conservative_NSER=0;
  tag_area=subarraytag_area(baddr,C,B,A,Ndbl,Ndwl,Nspd,NSubbanks,RWP,ERP,EWP,conservative_NSER,techscaling_factor);
  postdecode_area=decodetag_row(baddr,C,B,A,Ndbl,Nspd,Ndwl,NSubbanks,RWP,ERP,EWP);
  colmux_area=colmux(Ndbl,Nspd,RWP,ERP,EWP,conservative_NSER);
  precharge_area=precharge(Ndbl,Nspd,RWP,ERP,EWP,conservative_NSER);
  senseamp_area=senseamp(Ndbl,Nspd, RWP,ERP,EWP,conservative_NSER);
  comp_area=comparatorbit(RWP,ERP,EWP);
  tagsubarray_area.height=tag_area.height+colmux_area.height+precharge_area.height+senseamp_area.height+comp_area.height;
  tagsubarray_area.width =tag_area.width+postdecode_area.width;

  return tagsubarray_area;
}

static area_type tagsubblock(int baddr,
			     int C, int B, int A,
			     int Ndbl, int Ndwl, int Nspd,
			     double NSubbanks,
			     int SB, int RWP, int ERP, int EWP, int NSER,
			     double techscaling_factor) {	
  double tracks_h, tracks_w;
  area_type tagsubarray_area,tagsubblock_area;

  int conservative_NSER=0;
  int SB_ = SB;
  if (SB_ == 0) { 
    SB_ = 1; 
  }

  int N3to8 = (int)ceil((1.0/3.0)*logtwo_area( (double)(C/(B*A*Ndbl*Nspd))));
  if (N3to8==0) {
    N3to8=1;
  }

  int T= baddr-
    (int)logtwo_area((double)(C))+
    (int)logtwo_area((double)(A))+2-
    (int)logtwo_area((double)(NSubbanks));
 
  int colmuxtracks_rem = (Ndbl*Nspd >tracks_precharge_p) ? (Ndbl*Nspd-tracks_precharge_p) : 0;
  /*writeseltracks_rem = ((2*B*A)/(b0) > tracks_precharge_nx2) ? ((2*B*A)/(b0)-tracks_precharge_nx2) : 0; */
        
  tracks_h=Widthtrack*(N3to8*8*(RWP+ERP+EWP)+(RWP+EWP)*colmuxtracks_rem+Ndbl*Nspd*ERP+(RWP+ERP+EWP)*T/SB_+(ERP+RWP)*A);
  tracks_w=Widthtrack*(N3to8*8)*(RWP+ERP+EWP);
  tagsubarray_area=tagsubarray(baddr,C,B,A,Ndbl,Ndwl,Nspd,NSubbanks,RWP,ERP,EWP,conservative_NSER,techscaling_factor);
  tagsubblock_area.height=2*tagsubarray_area.height+tracks_h;
  tagsubblock_area.width =2*tagsubarray_area.width +tracks_w;

  return tagsubblock_area;
}

static area_type tagarray(int baddr,
			  int C, int B, int A,
			  int Ndbl, int Ndwl, int Nspd,
			  double NSubbanks,
			  int RWP, int ERP, int EWP, int NSER,
			  double techscaling_factor) {

  int SB, T;
  area_type tagarray_area, tagsubarray_area, tagsubblock_area;
  area_type temp;
  double temp_aspect;
  int conservative_NSER;

  double fixed_tracks_internal,fixed_tracks_external,variable_tracks;
  double tag,assoc,colmux,predecode,addresslines;
  int blocks, htree, htree_half, i, multiplier, iter_height;
  double inter_height,inter_width, total_height, total_width;

  conservative_NSER=0;
  SB = Ndwl*Ndbl/4;
  int N3to8= (int)ceil((1.0/3.0)*logtwo_area( (double)(C/(B*A*Ndbl*Nspd))));
  if (N3to8==0) {
    N3to8=1;
  }

  T= baddr-
    (int)logtwo_area((double)(C))+
    (int)logtwo_area((double)(A))+2-
    (int)logtwo_area((double)(NSubbanks));	
        
  tag=T*(RWP+ERP+EWP)*Widthtrack;
  assoc=(RWP+ERP)*A*Widthtrack;
  colmux=Ndbl*Nspd*(RWP+EWP+ERP)*Widthtrack;
  predecode=(RWP+ERP+EWP)*N3to8*8*Widthtrack;
  addresslines=ADDRESS_BITS*(RWP+ERP+EWP)*Widthtrack;

  tagsubarray_area=tagsubarray(baddr,C,B,A,Ndbl,Ndwl,Nspd,NSubbanks,RWP,ERP,EWP,conservative_NSER,techscaling_factor);
  tagsubblock_area=tagsubblock(baddr,C,B,A,Ndbl,Ndwl,Nspd,NSubbanks,SB,RWP,ERP,EWP,conservative_NSER,techscaling_factor);
  area_all_tagsubarrays=Ndbl*Ndwl*xcacti_calculate_area(tagsubarray_area,techscaling_factor)/100000000.0;

  fixed_tracks_internal=colmux+predecode+assoc;
  fixed_tracks_external=colmux+assoc+addresslines;
  variable_tracks=tag;

  if(SB==0) {
    if(Ndbl*Ndwl==1) {
      total_height=tagsubarray_area.height+fixed_tracks_external+tag;
      total_width =tagsubarray_area.width+predecode;
    } else {
      total_height=2*tagsubarray_area.height+fixed_tracks_external+tag;
      total_width =tagsubarray_area.width+predecode; 
    }
  }else if(SB==1) {
    total_height=tagsubblock_area.height;
    total_width =tagsubblock_area.width;
  }else if(SB==2) {
    total_height= tagsubblock_area.height;
    total_width = 2*tagsubblock_area.width + fixed_tracks_external+tag ;
  }else if(SB==4) {
    total_height=2*tagsubblock_area.height+fixed_tracks_external+tag;
    total_width =2*tagsubblock_area.width+fixed_tracks_internal+variable_tracks/2;
  }else if(SB==8) {
    total_height=2*tagsubblock_area.height+fixed_tracks_internal+variable_tracks/2;
    total_width =2*(2*tagsubblock_area.width+variable_tracks/4)+fixed_tracks_external+tag;
  }else {
    blocks = SB / 4;
    htree = (int) (logtwo_area((double) (blocks)));
    inter_height = tagsubblock_area.height;
    inter_width = tagsubblock_area.width;
    multiplier =1;

    if ( htree % 2 == 0) {
      iter_height = htree/2; 
    }else{
      iter_height = 0; 
    }

    total_height = 0;
    total_width  = 0;

    if (htree % 2 == 0) {
      for (i=0;i<=iter_height;i++) {
	if (i==iter_height) {
	  total_height = 2*inter_height+tag/blocks*multiplier+fixed_tracks_external;
	  total_width = 2*inter_width + tag/(2*blocks)*multiplier+fixed_tracks_internal;
	} else {
	  total_height = 2*inter_height+tag/blocks*multiplier+fixed_tracks_internal;
	  total_width = 2*inter_width+tag/(2*blocks)*multiplier+fixed_tracks_internal;
	  inter_height = total_height ;
	  inter_width = total_width ;
	  multiplier = multiplier*4;
	}
      }
    } else {
      htree_half = htree-1;
      iter_height = htree_half/2;
      for (i=0;i<=iter_height;i++) {
	total_height = 2*inter_height+tag/blocks*multiplier+fixed_tracks_internal;
	total_width = 2*inter_width+tag/(2*blocks)*multiplier+fixed_tracks_internal;
	inter_height = total_height ;
	inter_width = total_width ;
	multiplier = multiplier*4;
      }
      total_width = 2*inter_width+tag/(2*blocks)*multiplier+fixed_tracks_external;
    }
  }

  tagarray_area.width = total_width;
  tagarray_area.height =total_height;

  temp.height = tagarray_area.width;
  temp.width  = tagarray_area.height;
  temp_aspect = ((temp.height/temp.width) > 1.0) ? (temp.height/temp.width) : 1.0/(temp.height/temp.width);
  aspect_ratio_tag = ((tagarray_area.height/tagarray_area.width) > 1.0) ? (tagarray_area.height/tagarray_area.width) : 1.0/(tagarray_area.height/tagarray_area.width) ;
  if (aspect_ratio_tag > temp_aspect) {
    tagarray_area.height = temp.height;
    tagarray_area.width  = temp.width ;
  }

  aspect_ratio_tag = ((tagarray_area.height/tagarray_area.width) > 1.0) ? (tagarray_area.height/tagarray_area.width) : 1.0/(tagarray_area.height/tagarray_area.width) ;

  return(tagarray_area);
}

static void area(int baddr, int b0,
		 int Ndbl, int Ndwl, int Nspd, 
		 int Ntbl, int Ntwl, int Ntspd,
		 const parameter_type *parameters,
		 arearesult_type *result) {

  double NSubbanks = parameters->NSubbanks;

  int rows_datasubarray, colns_datasubarray;
  int rows_tagsubarray, colns_tagsubarray;

  rows_datasubarray=(parameters->cache_size/((parameters->block_size)*(parameters->associativity)*Ndbl*Nspd));
  colns_datasubarray=(Ndbl*Nspd);
  rows_tagsubarray=(parameters->cache_size/((parameters->block_size)*(parameters->associativity)*Ntbl*Ntspd));
  colns_tagsubarray=(Ntbl*Ntspd);

  result->dataarray_area=dataarray(parameters->cache_size,parameters->block_size,parameters->associativity,Ndbl,Ndwl,Nspd,b0,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,parameters->num_single_ended_read_ports,parameters->fudgefactor);

  result->datapredecode_area=predecode_area(rows_datasubarray,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);	
  result->datacolmuxpredecode_area=predecode_area(colns_datasubarray,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  result->datacolmuxpostdecode_area=postdecode_area(colns_datasubarray,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  result->datawritesig_area=muxdrvsig(parameters->associativity,parameters->block_size,b0);
  result->tagarray_area=tagarray(baddr,parameters->cache_size,parameters->block_size,parameters->associativity,Ntbl,Ntwl,Ntspd,NSubbanks,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,parameters->num_single_ended_read_ports,parameters->fudgefactor);
  result->tagpredecode_area=predecode_area(rows_tagsubarray,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  result->tagcolmuxpredecode_area=predecode_area(colns_tagsubarray,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  result->tagcolmuxpostdecode_area=postdecode_area(colns_tagsubarray,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  result->tagoutdrvdecode_area=muxdriverdecode(parameters->block_size,b0,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  result->tagoutdrvsig_area=muxdrvsig(parameters->associativity,parameters->block_size,b0);
  result->totalarea=xcacti_calculate_area(result->dataarray_area,parameters->fudgefactor)+xcacti_calculate_area(result->datapredecode_area,parameters->fudgefactor)+xcacti_calculate_area(result->datacolmuxpredecode_area,parameters->fudgefactor)+xcacti_calculate_area(result->datacolmuxpostdecode_area,parameters->fudgefactor)+(parameters->num_readwrite_ports+parameters->num_write_ports)*xcacti_calculate_area(result->datawritesig_area,parameters->fudgefactor)+xcacti_calculate_area(result->tagarray_area,parameters->fudgefactor)+xcacti_calculate_area(result->tagpredecode_area,parameters->fudgefactor)+xcacti_calculate_area(result->tagcolmuxpredecode_area,parameters->fudgefactor)+xcacti_calculate_area(result->tagcolmuxpostdecode_area,parameters->fudgefactor)+xcacti_calculate_area(result->tagoutdrvdecode_area,parameters->fudgefactor)+(parameters->num_readwrite_ports+parameters->num_read_ports)*xcacti_calculate_area(result->tagoutdrvsig_area,parameters->fudgefactor);
}

/*returns area of post decode */
static area_type fadecode_row(int C, int B,
			      int Ndbl,
			      int RWP, int ERP, int EWP) {
  
  double decodeNORwidth, firstinv;
  area_type decinv,worddriveinv,postdecodearea;

  int numstack = (int)ceil((1.0/3.0)*logtwo_area((double)((double)C/(double)(B))));

  if (numstack==0) numstack = 1;
  if (numstack>6) numstack = 6;
  switch(numstack) {
  case 1: decodeNORwidth = WidthNOR1; break;
  case 2: decodeNORwidth = WidthNOR2; break;
  case 3: decodeNORwidth = WidthNOR3; break;
  case 4: decodeNORwidth = WidthNOR4; break;
  case 5: decodeNORwidth = WidthNOR5; break;
  case 6: decodeNORwidth = WidthNOR6; break;
  default: printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;

  }
  decinv = inverter_area(Wdecinvp,Wdecinvn);
  worddriveinv =inverter_area(Wdecinvp,Wdecinvn);
  switch(numstack) {
  case 1: firstinv = decinv.height; break;
  case 2: firstinv = decinv.height; break;
  case 3: firstinv = decinv.height; break;
  case 4: firstinv = decNandWidth; break;
  case 5: firstinv = decNandWidth; break;
  case 6: firstinv = decNandWidth; break;
  default: printf("error:numstack=%d\n",numstack);
    printf("Cacti does not support a series stack of %d transistors !\n",numstack);
    exit(0);
    break;

  }

  postdecodearea.height = BitHeight16x2;
  postdecodearea.width  = (decodeNORwidth + firstinv + worddriveinv.height)*(RWP+EWP);
  return(postdecodearea);
}

area_type fasubarray(int baddr, 
		     int C, 
		     int B, 
		     int Ndbl,
		     int RWP, int ERP, int EWP, int NSER,
		     double techscaling_factor) {

  area_type FAarea, fadecoderow, faramcell;
  int noof_rowsdata, noof_colnsdata;
  int Tagbits, HTagbits;
  double precharge, widthoverhead, heightoverhead;

  noof_rowsdata = (C/(B*Ndbl));
  noof_colnsdata = (8*B);
  Tagbits = baddr-(int)logtwo_area((double)(B))+2;
  HTagbits = (int)ceil((double)(Tagbits)/2.0);

  precharge=Wbitpreequ+2*Wbitdropv+Wwrite+2*(2*Widthcontact+1)+3*Widthptondiff;

  if ((RWP==1) && (ERP==0) && (EWP==0)) {
    heightoverhead=0;
    widthoverhead =0; }
  else {
    if ((RWP==1) && (ERP==1) && (EWP==0)) {
      widthoverhead=FAWidthIncrPer_first_r_port ;
      heightoverhead=FAHeightIncrPer_first_r_port ; }
    else {
      if ((RWP==1) && (ERP==0) && (EWP==1)) {
	widthoverhead=FAWidthIncrPer_first_rw_or_w_port ;
	heightoverhead=FAHeightIncrPer_first_rw_or_w_port ; }
      else {
	if (RWP+EWP >=2) {
	  widthoverhead = FAWidthIncrPer_first_rw_or_w_port+(RWP+EWP-2)*FAWidthIncrPer_later_rw_or_w_port+ERP*FAWidthIncrPer_later_r_port;
	  heightoverhead= FAHeightIncrPer_first_rw_or_w_port+(RWP+EWP-2)*FAHeightIncrPer_later_rw_or_w_port+ERP*FAHeightIncrPer_later_r_port; }
	else {
	  if ((RWP==0) && (EWP==0)) {
	    widthoverhead=FAWidthIncrPer_first_r_port + (ERP-1)*FAWidthIncrPer_later_r_port;
	    heightoverhead=FAHeightIncrPer_first_r_port+ (ERP-1)*FAHeightIncrPer_later_r_port; ; }
	  else {
	    if ((RWP==0) && (EWP==1)) {
              widthoverhead=ERP*FAWidthIncrPer_later_r_port ;
              heightoverhead=ERP*FAHeightIncrPer_later_r_port ; }
	    else {
              if ((RWP==1) && (EWP==0)) {
		widthoverhead=ERP*FAWidthIncrPer_later_r_port ;
		heightoverhead=ERP*FAHeightIncrPer_later_r_port ; } } } } } } }

  faramcell.height= (int)ceil((double)(noof_rowsdata)/16.0)*stitch_ramv+(CAM2x2Height_1p+2*heightoverhead)*ceil((double)(noof_rowsdata)/2.0);

  faramcell.width= (int)(ceil((double)(noof_colnsdata)/16.0))*(BitWidth16x2+16*(Widthtrack*2*(RWP+(ERP-NSER)+EWP-1)+Widthtrack*NSER))+
    2*(HTagbits*((CAM2x2Width_1p+2*widthoverhead)-Widthcontact))+
    (BitWidth+Widthtrack*2*(RWP+ERP+EWP-1))+
    (FArowNANDWidth+FArowNOR_INVWidth)*(RWP+ERP+EWP);

  FAarea.height = faramcell.height+precharge*(RWP+EWP)+SenseampHeight*(RWP+ERP)+DatainvHeight*(RWP+EWP)+FAOutdriveHeight*(RWP+ERP);
  FAarea.width  = faramcell.width;

  fadecoderow = fadecode_row(C,B,Ndbl,RWP,ERP,EWP);
  FAarea.width = FAarea.width + fadecoderow.width;

  area_all_dataramcells=Ndbl*xcacti_calculate_area(faramcell,techscaling_factor)/100000000.0;
  faarea_all_subarrays=Ndbl*xcacti_calculate_area(FAarea,techscaling_factor)/100000000.0;
  return(FAarea);
}

area_type faarea(int baddr, int b0,
		 int C, int B,
		 int Ndbl,
		 int RWP, int ERP, int EWP, int NSER,
		 double techscaling_factor) {

  area_type fasubarray_area,fa_area;
  int Tagbits, blocksel;
  double fixed_tracks, predecode, base_height, base_width;
  area_type temp;
  double temp_aspect;
       
  int blocks, htree, htree_half, i, iter;
  double inter_height,inter_width, total_height, total_width;
	
  int N3to8 = (int)ceil((1.0/3.0)*logtwo_area( (double)((double)C/(double)(B))));
  if (N3to8==0) {
    N3to8=1;
  }

  Tagbits = baddr-(int)logtwo_area((double)(B))+2;
  fasubarray_area = fasubarray(baddr,C,B,Ndbl,RWP,ERP,EWP,NSER,techscaling_factor);
  blocksel = MAX((int)logtwo_area((double)(B)),(8*B)/b0);
  blocksel = (blocksel > tracks_outdrvfanand_p) ? (blocksel-tracks_outdrvfanand_p) : 0;
       
  fixed_tracks = Widthtrack*(1*(RWP+EWP)+b0*(RWP+ERP+EWP)+Tagbits*(RWP+ERP+EWP)+blocksel*(RWP+ERP+EWP));
  predecode = Widthtrack*(N3to8*8)*(RWP+EWP);

  if(Ndbl==1) {
    total_height=fasubarray_area.height+fixed_tracks;
    total_width=fasubarray_area.width+predecode;
  }
  if(Ndbl==2) {
    total_height=2*fasubarray_area.height+fixed_tracks;
    total_width =fasubarray_area.width+predecode;
  }
  if(Ndbl==4) {
    total_height=2*fasubarray_area.height+fixed_tracks+predecode;
    total_width =2*fasubarray_area.width+predecode;
  }
  if(Ndbl>4) {
    blocks=Ndbl/4;
    htree = (int)(logtwo_area ( (double) (blocks)));
    base_height=2*fasubarray_area.height+fixed_tracks+predecode;
    base_width= 2*fasubarray_area.width+predecode;
	
    inter_height=base_height;
    inter_width=base_width;
	
    if(htree % 2 ==0) {
      iter = htree/2; }
		  
    if(htree % 2 ==0) {
      for (i=1;i<=iter;i++)
	{
	  total_height = 2*(inter_height)+fixed_tracks+predecode;
	  inter_height = total_height ;
	  total_width = 2*(inter_width)+fixed_tracks+predecode;
	  inter_width = total_width ;
	}
    }
    else {
      htree_half = htree-1;
      iter = htree_half/2;
      if(iter==0) {
	total_height = base_height;
	total_width  = 2*base_width+fixed_tracks+predecode; 
      }
      else {
	for (i=0;i<=iter;i++)
	  {
	    total_height = 2*inter_height+fixed_tracks+predecode;
	    total_width = 2*inter_width+fixed_tracks+predecode;
	    inter_height = total_height ;
	    inter_width = total_width ;
	  }
	total_width = 2*inter_width+fixed_tracks+predecode;
      }
    }
  }
	
  fa_area.height = total_height;
  fa_area.width  = total_width;
	
  temp.height = fa_area.width;
  temp.width  = fa_area.height;
  temp_aspect = ((temp.height/temp.width) > 1.0) ? (temp.height/temp.width) : 1.0/(temp.height/temp.width);
  aspect_ratio_data = ((fa_area.height/fa_area.width) > 1.0) ? (fa_area.height/fa_area.width) : 1.0/(fa_area.height/fa_area.width) ;
  if (aspect_ratio_data > temp_aspect) {
    fa_area.height = temp.height;
    fa_area.width  = temp.width ;
  }

  aspect_ratio_data = ((fa_area.height/fa_area.width) > 1.0) ? (fa_area.height/fa_area.width) : 1.0/(fa_area.height/fa_area.width) ;

  return(fa_area);
}

void fatotalarea(int baddr, int b0, int Ndbl,
		 const parameter_type *parameters,
		 arearesult_type *faresult) {

  area_type null_area;

  null_area.height=0.0;
  null_area.width=0.0;

  faresult->dataarray_area=faarea(baddr,b0,parameters->cache_size,parameters->block_size,Ndbl,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports,parameters->num_single_ended_read_ports,parameters->fudgefactor);
  faresult->datapredecode_area=predecode_area(parameters->cache_size/parameters->block_size,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  faresult->datacolmuxpredecode_area=null_area;
  faresult->datacolmuxpostdecode_area=null_area;
  faresult->datawritesig_area=null_area;

  faresult->tagarray_area=null_area;
  faresult->tagpredecode_area=null_area;
  faresult->tagcolmuxpredecode_area=null_area;
  faresult->tagcolmuxpostdecode_area=null_area;
  faresult->tagoutdrvdecode_area=muxdriverdecode(parameters->block_size,b0,parameters->num_readwrite_ports,parameters->num_read_ports,parameters->num_write_ports);
  faresult->tagoutdrvsig_area=null_area;
  faresult->totalarea=(xcacti_calculate_area(faresult->dataarray_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->datapredecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->datacolmuxpredecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->datacolmuxpostdecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->tagarray_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->tagpredecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->tagcolmuxpredecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->tagcolmuxpostdecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->tagoutdrvdecode_area,parameters->fudgefactor)+xcacti_calculate_area(faresult->tagoutdrvsig_area,parameters->fudgefactor));

}

void xcacti_area_subbanked(int baddr,
			   int b0,
			   int RWP, int ERP, int EWP,
			   int Ndbl, int Ndwl, int Nspd,
			   int Ntbl, int Ntwl, int Ntspd,
			   const parameter_type *parameters,
			   area_type *result_subbanked,
			   arearesult_type *result)
{
  arearesult_type result_area;
  area_type temp;
  double temp_aspect;
  int blocks,htree,htree_double;
  double base_height,base_width,inter_width,inter_height,total_height,total_width;
  int base_subbanks,inter_subbanks;
  int i,iter_height,iter_width,iter_width_double;

  area_all_dataramcells = 0.0;
  area_all_tagramcells = 0.0;
  aspect_ratio_data = 1.0;
  aspect_ratio_tag = 1.0;
  aspect_ratio_subbank = 1.0;
  aspect_ratio_total = 1.0;


  if (parameters->fully_assoc==0) {
    area(baddr,b0,Ndbl,Ndwl,Nspd,Ntbl,Ntwl,Ntspd,parameters,&result_area); }
  else {
    fatotalarea(baddr,b0,Ndbl,parameters,&result_area); }

  result->dataarray_area = result_area.dataarray_area;
  result->datapredecode_area = result_area.datapredecode_area;
  result->datacolmuxpredecode_area = result_area.datacolmuxpredecode_area;
  result->datacolmuxpostdecode_area = result_area.datacolmuxpostdecode_area;
  result->datawritesig_area = result_area.datawritesig_area;
  result->tagarray_area = result_area.tagarray_area;
  result->tagpredecode_area = result_area.tagpredecode_area;
  result->tagcolmuxpredecode_area = result_area.tagcolmuxpredecode_area;
  result->tagcolmuxpostdecode_area = result_area.tagcolmuxpostdecode_area;
  result->tagoutdrvdecode_area = result_area.tagoutdrvdecode_area;
  result->tagoutdrvsig_area = result_area.tagoutdrvsig_area;
  result->totalarea = result_area.totalarea;
  result->total_dataarea = result_area.total_dataarea;
  result->total_tagarea = result_area.total_tagarea;

  if (parameters->NSubbanks == 1) {
    total_height = result_area.dataarray_area.height;
    total_width = result_area.dataarray_area.width+result_area.tagarray_area.width;
  }
  if (parameters->NSubbanks == 2) {
    total_height = result_area.dataarray_area.height + (RWP+ERP+EWP)*ADDRESS_BITS;
    total_width = (result_area.dataarray_area.width+result_area.tagarray_area.width)*2 + (ADDRESS_BITS+BITOUT)*parameters->NSubbanks*(RWP+ERP+EWP);
  }
  if (parameters->NSubbanks == 4) {
    total_height = 2*result_area.dataarray_area.height+2*(RWP+ERP+EWP)*ADDRESS_BITS;
    total_width = (result_area.dataarray_area.width+result_area.tagarray_area.width)*2 + (ADDRESS_BITS+BITOUT)*parameters->NSubbanks*(RWP+ERP+EWP);

  }
  if (parameters->NSubbanks == 8) {
    total_height = (result_area.dataarray_area.width+result_area.tagarray_area.width)*2 + (ADDRESS_BITS+BITOUT)*parameters->NSubbanks*(RWP+ERP+EWP)*0.5;
    total_width = 2*(2*result_area.dataarray_area.height+2*(RWP+ERP+EWP)*ADDRESS_BITS)+(ADDRESS_BITS+BITOUT)*parameters->NSubbanks*(RWP+ERP+EWP);
  }

  if (parameters->NSubbanks > 8 ) {
    blocks = parameters->NSubbanks / 16;
    htree = (int) (logtwo_area((double) (blocks)));
    base_height = 2*((result_area.dataarray_area.width+result_area.tagarray_area.width)*2 + (ADDRESS_BITS+BITOUT)*16*(RWP+ERP+EWP)*0.25) + (ADDRESS_BITS+BITOUT)*16*(RWP+ERP+EWP)*0.5;
    base_width = 2*(2*result_area.dataarray_area.height+2*(RWP+ERP+EWP)*ADDRESS_BITS)+(ADDRESS_BITS+BITOUT)*16*(RWP+ERP+EWP)*0.25; 
    base_subbanks = 16;
    if ( htree % 2 == 0) {
      iter_height = htree/2; }
    else {
      iter_height = (htree-1)/2 ; }

    inter_height=base_height;
    inter_subbanks=base_subbanks;

    if (iter_height==0) {
      total_height = base_height; }
    else {
      for (i=1;i<=iter_height;i++)
	{
	  total_height = 2*(inter_height)+(ADDRESS_BITS+BITOUT)*4*inter_subbanks*(RWP+ERP+EWP)*0.5; 
	  inter_height = total_height ;
	  inter_subbanks = inter_subbanks*4;
	}
    }   

    inter_width =base_width ;
    inter_subbanks=base_subbanks;
    iter_width= 10;

    if ( htree % 2 == 0) {
      iter_width = htree/2; }

    if (iter_width==0) {
      total_width = base_width; }
    else {
      if ( htree % 2 == 0) { 
	for (i=1;i<=iter_width;i++)
	  {
	    total_width = 2*(inter_width)+(ADDRESS_BITS+BITOUT)*inter_subbanks*(RWP+ERP+EWP);
	    inter_width = total_height ;
	    inter_subbanks = inter_subbanks*4;
	  }
      }
      else {
	htree_double = htree + 1;
	iter_width_double=htree_double/2;
	for (i=1;i<=iter_width_double;i++)
	  {
	    total_width = 2*(inter_width)+(ADDRESS_BITS+BITOUT)*inter_subbanks*(RWP+ERP+EWP);
	    inter_width = total_height ;
	    inter_subbanks = inter_subbanks*4;
	  }
	total_width+=(ADDRESS_BITS+BITOUT)*(RWP+ERP+EWP)*parameters->NSubbanks/2;
      }
    }	
  }

  result_subbanked->height = total_height ;
  result_subbanked->width = total_width ;

  temp.width =  result_subbanked->height;
  temp.height = result_subbanked->width;

  temp_aspect = ((temp.height/temp.width) > 1.0) ? (temp.height/temp.width) : 1.0/(temp.height/temp.width);

  aspect_ratio_total = (result_subbanked->height/result_subbanked->width);

  aspect_ratio_total = (aspect_ratio_total > 1.0) ? (aspect_ratio_total) : 1.0/(aspect_ratio_total) ;


  if (aspect_ratio_total > temp_aspect) {
    result_subbanked->height = temp.height;
    result_subbanked->width  = temp.width ;
  }

  aspect_ratio_subbank = (result_area.dataarray_area.height/(result_area.dataarray_area.width + result_area.tagarray_area.width));
  aspect_ratio_subbank = (aspect_ratio_subbank > 1.0) ? (aspect_ratio_subbank) : 1.0/(aspect_ratio_subbank) ;
  aspect_ratio_total = (result_subbanked->height/result_subbanked->width);
  aspect_ratio_total = (aspect_ratio_total > 1.0) ? (aspect_ratio_total) : 1.0/(aspect_ratio_total) ;

}


int xcacti_organizational_parameters_valid(int B, int A, int C, 
					   int Ndwl, int Ndbl, int Nspd, 
					   int Ntwl, int Ntbl, int Ntspd,
					   char assoc)
{
   /* don't want more than 8 subarrays for each of data/tag */

   if (assoc==0) {
   if (Ndwl*Ndbl>MAXSUBARRAYS) return(FALSE);
   if (Ntwl*Ntbl>MAXSUBARRAYS) return(FALSE);
   /* add more constraints here as necessary */
   if (C/(8*B*A*Ndbl*Nspd)<=0) return (FALSE);
   if ((8*B*A*Nspd/Ndwl)<=0) return (FALSE);
   if (C/(8*B*A*Ntbl*Ntspd)<=0) return (FALSE);
   if ((8*B*A*Ntspd/Ntwl)<=0) return (FALSE); }

   else {
   if (C/(2*B*Ndbl)<=0) return (FALSE);
   if (Ndbl>MAXN) return(FALSE);
   if (Ntbl>MAXN) return(FALSE);
   }
   
   return(TRUE);
}

double xcacti_calculate_area(area_type module_area,
			     double techscaling_factor) {

  return(module_area.height*module_area.width*(1/techscaling_factor)*(1/techscaling_factor));
}
