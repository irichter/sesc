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
#include <math.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "xcacti_def.h"
#include "xcacti_area.h"

#define NEXTINT(a) skip(); scanf("%d",&(a));
#define NEXTFLOAT(a) skip(); scanf("%lf",&(a));

#ifdef XCACTI
int PHASEDCACHE=0;

int ADDRESS_BITS=32;
int BITOUT=64;

double Wdecdrivep=	(360.0);
double Wdecdriven=	(240.0);
double Wdec3to8n=     120.0;
double Wdec3to8p=     60.0;
double WdecNORn=       2.4;
double WdecNORp=      12.0;
double Wdecinvn=      20.0;
double Wdecinvp=      40.0 ;
double Wworddrivemax= 100.0;
double Wmemcella=	(2.4);
double Wmemcellbscale=	2;		/* means 2x bigger than Wmemcella */
double Wbitpreequ=	(80.0);
double Wbitmuxn=	(10.0);
double WsenseQ1to4=	(4.0);
double Wcompinvp1=	(10.0);
double Wcompinvn1=	(6.0);
double Wcompinvp2=	(20.0);
double Wcompinvn2=	(12.0);
double Wcompinvp3=	(40.0);
double Wcompinvn3=	(24.0);
double Wevalinvp=	(80.0);
double Wevalinvn=	(40.0);

double Wfadriven=    (50.0);
double Wfadrivep=    (100.0);
double Wfadrive2n=    (200.0);
double Wfadrive2p=    (400.0);
double Wfadecdrive1n=    (5.0);
double Wfadecdrive1p=    (10.0);
double Wfadecdrive2n=    (20.0);
double Wfadecdrive2p=    (40.0);
double Wfadecdriven=    (50.0);
double Wfadecdrivep=    (100.0);
double Wfaprechn=       (6.0);
double Wfaprechp=       (10.0);
double Wdummyn=         (10.0);
double Wdummyinvn=      (60.0);
double Wdummyinvp=      (80.0);
double Wfainvn=         (10.0);
double Wfainvp=         (20.0);
double Waddrnandn=      (50.0);
double Waddrnandp=      (50.0);
double Wfanandn=        (20.0);
double Wfanandp=        (30.0);
double Wfanorn=         (5.0);
double Wfanorp=         (10.0);
double Wdecnandn=       (10.0);
double Wdecnandp=       (30.0);

double Wcompn=		(10.0);
double Wcompp=		(30.0);
double Wmuxdrv12n=	(60.0);
double Wmuxdrv12p=	(100.0);
double WmuxdrvNANDn=    (60.0);
double WmuxdrvNANDp=    (80.0);
double WmuxdrvNORn=	(40.0);
double WmuxdrvNORp=	(100.0);
double Wmuxdrv3n=	(80.0);
double Wmuxdrv3p=	(200.0);
double Woutdrvseln=	(24.0);
double Woutdrvselp=	(40.0);
double Woutdrvnandn=	(10.0);
double Woutdrvnandp=	(30.0);
double Woutdrvnorn=	(5.0);
double Woutdrvnorp=	(20.0);
double Woutdrivern=	(48.0);
double Woutdriverp=	(80.0);

double Wsenseextdrv1p= (80.0);
double Wsenseextdrv1n= (40.0);
double Wsenseextdrv2p= (240.0);
double Wsenseextdrv2n= (160.0);

double psensedata= (0.02e-9);
double psensetag = (0.02e-9);

double tsensedata= (5.82e-10);
double tsensetag = (0.02e-9);

double Vbitsense = (0.20); /* Original CACTI 0.1, but amrutur recommend 200mV */ 

double BitWidth	= (8.0);
double BitHeight	= (16.0);
double Cout = (0.5e-12);
#endif
						
/*---------------------------------------------------------------*/

static double logtwo(double x) {

  if (x<=0) 
    printf("ERROR %e\n",x);

  return (double) (log(x)/log(2.0));
}


static void show_usage() {

  printf("XCACTI 1.1 Usage Summary:\n");
  printf("Arguments:\n");
  printf("\t-s <int>        ; cache size (power of two)\n");
  printf("\t-l <int>        ; cache line size (power of two)\n");
  printf("\t-a <int>|FA|DM  ; associativity\n");
  printf("\t-t <float>      ; technology (nm) 800nm .. 65nm\n");
  printf("\t-r <int>        ; number of read ports\n");
  printf("\t-w <int>        ; number of write ports\n");
  printf("\t-x <int>        ; number of read/write ports\n");
  printf("\t-b <int>        ; number of banks\n");
#ifdef XCACTI
  printf("\t-S              ; latch sense amplifier (no wada)\n");
  printf("\t-D              ; ignore tag energy (data only)\n");
  printf("\t-P              ; phase cache\n");
#endif
}

static void output_time_components(const result_type *result,const parameter_type *parameters)
{
  double datapath,tagpath;
  
  int A=parameters->associativity;

  datapath = result->subbank_address_routing_delay+result->decoder_delay_data+result->wordline_delay_data+result->bitline_delay_data+result->sense_amp_delay_data+result->total_out_driver_delay_data+result->data_output_delay;
  if (parameters->associativity == 1) {
    tagpath = result->subbank_address_routing_delay+result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_valid_delay;
  } else {
    tagpath = result->subbank_address_routing_delay+result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_mux_delay+result->selb_delay+result->data_output_delay+result->total_out_driver_delay_data;
  }
	
  if (!parameters->fully_assoc) {
    printf(" decode data      : %9.3fps %9.3fpJ\n"
	   ,result->decoder_delay_data/PICOS
	   ,result->decoder_power_data*PICOJ);
  }else{
    printf(" tag comparison   : %9.3fps %9.3fpJ\n"
	   ,result->decoder_delay_data/PICOS
	   ,result->decoder_power_data*PICOJ);
  }
	
  printf(" w&b line  data   : %9.3fps %9.3fpJ\n"
	 ,(result->wordline_delay_data
	   +result->bitline_delay_data
	   )/PICOS
	 ,(result->wordline_power_data
	   +result->bitline_power_data
	   )*PICOJ);
	
  printf("        wordline  : %9.3fps %9.3fpJ\n"
	 ,result->wordline_delay_data/PICOS
	 ,result->wordline_power_data*PICOJ);
  printf("        bitline   : %9.3fps %9.3fpJ\n"
	 ,result->bitline_delay_data/PICOS
	 ,result->bitline_power_data*PICOJ);
  printf(" sense amp data   : %9.3fps %9.3fpJ\n"
	 ,result->sense_amp_delay_data/PICOS
	 ,result->sense_amp_power_data*PICOJ);
	
  if (!parameters->fully_assoc) {
    printf(" decode tag       : %9.3fps %9.3fpJ\n"
	   ,result->decoder_delay_tag/PICOS
	   ,result->decoder_power_tag*PICOJ);
    printf(" w&b line tag     : %9.3fps %9.3fpJ\n"
	   ,(result->wordline_delay_tag+result->bitline_delay_tag)/PICOS
	   ,(result->wordline_power_tag+result->bitline_power_tag)*PICOJ);
    printf("       wordline   : %9.3fps %9.3fpJ\n"
	   ,result->wordline_delay_tag/PICOS
	   ,result->wordline_power_tag*PICOJ);
    printf("       bitline    : %9.3fps %9.3fpJ\n"
	   ,result->bitline_delay_tag/PICOS
	   ,result->bitline_power_tag*PICOJ);
    printf(" sense amp tag    : %9.3fps %9.3fpJ\n"
	   ,result->sense_amp_delay_tag/PICOS
	   ,result->sense_amp_power_tag*PICOJ);
    printf(" compare address  : %9.3fps %9.3fpJ\n"
	   ,result->compare_part_delay/PICOS
	   ,result->compare_part_power*PICOJ);
    if (A == 1) {
      printf(" valid driver     : %9.3fps %9.3fpJ\n"
	     ,result->drive_valid_delay/PICOS
	     ,result->drive_valid_power*PICOJ);
    }else {
      printf(" mux driver       : %9.3fps %9.3fpJ\n"
	     ,result->drive_mux_delay/PICOS
	     ,result->drive_mux_power*PICOJ);
      printf(" select inverter  : %9.3fps %9.3fpJ\n"
	     ,result->selb_delay/PICOS
	     ,result->selb_power*PICOJ);
    }
  }
  printf(" data output drv  : %9.3fps %9.3fpJ\n"
	 ,result->data_output_delay/PICOS
	 ,result->data_output_power*PICOJ);
   
  printf(" total data ~drv  : %9.3fps %9.3fpJ\n"
	 ,(result->decoder_delay_data+result->wordline_delay_data
	   +result->bitline_delay_data+result->sense_amp_delay_data
	   )/PICOS
	 ,(result->decoder_power_data+result->wordline_power_data
	   +result->bitline_power_data+result->sense_amp_power_data
	   )*PICOJ);
   
  if (!parameters->fully_assoc) {
    if (A==1){
      printf(" total tag (DM)   : %9.3fps %9.3fpJ\n"
	     ,(result->decoder_delay_tag+result->wordline_delay_tag
	       +result->bitline_delay_tag+result->sense_amp_delay_tag
	       +result->compare_part_delay+result->drive_valid_delay)/PICOS
	     ,(result->decoder_power_tag+result->wordline_power_tag
	       +result->bitline_power_tag+result->sense_amp_power_tag
	       +result->compare_part_power+result->drive_valid_power)*PICOJ);
    }else{
      printf(" total tag (~DM)  : %9.3fps %9.3fpJ\n"
	     ,(result->decoder_delay_tag+result->wordline_delay_tag
	       +result->bitline_delay_tag+result->sense_amp_delay_tag
	       +result->compare_part_delay+result->drive_mux_delay
	       +result->selb_delay)/PICOS
	     ,(result->decoder_power_tag+result->wordline_power_tag
	       +result->bitline_power_tag+result->sense_amp_power_tag
	       +result->compare_part_power+result->drive_mux_power
	       +result->selb_power)*PICOJ);
    }
  }

   
  if( parameters->fully_assoc ){
    printf("Total Data        : %9.3fps %9.3fpJ\n"
	   ,datapath/PICOS
	   ,(result->wordline_power_data+result->bitline_power_data
	     +result->sense_amp_power_data+result->data_output_power)*PICOJ);
	   
    printf("Total TAG         : %9.3fps %9.3fpJ\n"
	   ,0.0
	   ,result->decoder_power_data*PICOJ);
  }else{
    printf("Total Data        : %9.3fps %9.3fpJ\n"
	   ,datapath/PICOS
	   ,(result->decoder_power_data+result->wordline_power_data
	     +result->bitline_power_data+result->sense_amp_power_data
	     +result->drive_mux_power
	     +result->selb_power+result->data_output_power)*PICOJ);

    printf("Total TAG         : %9.3fps %9.3fpJ\n"
	   ,tagpath/PICOS
	   ,(result->decoder_power_tag+result->wordline_power_tag+result->bitline_power_tag
	     +result->sense_amp_power_tag+result->compare_part_power)*PICOJ);
  }
  printf("\n");
  printf("Read Energy       :             %9.3fpJ\n",result->total_power*PICOJ);
#ifdef XCACTI
  printf("Write Energy      :             %9.3fpJ\n",result->total_wrpower*PICOJ);
#endif
   
  printf("\n");
   
  printf("Access Time       : %9.3fps\n", result->access_time/PICOS);
  printf("Max Precharge     : %9.3fps\n", result->precharge_delay/PICOS);
  if( result->precharge_delay * 2 > result->access_time )
    printf("Pipe Time (1clk)  : %9.3fps (data=%9.3fps) (tag=%9.3fps)\n"
	   ,(2*result->precharge_delay)/PICOS
	   ,(2*result->precharge_delay)/PICOS
	   ,(tagpath)/PICOS
	   );
  else
    printf("Pipe Time (1clk)  : %9.3fps (data=%9.3fps) (tag=%9.3fps)\n"
	   ,(result->access_time)/PICOS
	   ,(result->access_time)/PICOS
	   ,(tagpath)/PICOS
	   );

  printf("Pipe Time (2clk)  : %9.3fps (data=%9.3fps) (tag=%9.3fps)\n"
	 ,(result->access_time/2+result->precharge_delay)/PICOS
	 ,(datapath/2+result->precharge_delay)/PICOS
	 ,((tagpath)/2+result->precharge_delay)/PICOS
	 );
  printf("Pipe Time (3clk)  : %9.3fps (data=%9.3fps) (tag=%9.3fps)\n"
	 ,result->cycle_time/PICOS
	 ,(datapath/3+result->precharge_delay)/PICOS
	 ,((tagpath)/3+result->precharge_delay)/PICOS
	 );
}

static void output_area_components(const arearesult_type *arearesult,
				   const area_type *arearesult_subbanked,
				   const parameter_type *parameters)
{
    printf("\nArea Components:\n\n");
/*
    printf("Aspect Ratio Data height/width: %f\n", aspect_ratio_data);
    printf("Aspect Ratio Tag height/width: %f\n", aspect_ratio_tag);
    printf("Aspect Ratio Subbank height/width: %f\n", aspect_ratio_subbank);
    printf("Aspect Ratio Total height/width: %f\n\n", aspect_ratio_total);
*/
    printf("Aspect Ratio Total height/width: %f\n\n", arearesult->aspect_ratio_total);

    printf("Data array (cm^2): %f\n",xcacti_calculate_area(arearesult->dataarray_area,parameters->fudgefactor)/100000000.0);
    printf("Data predecode (cm^2): %f\n",xcacti_calculate_area(arearesult->datapredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Data colmux predecode (cm^2): %f\n",xcacti_calculate_area(arearesult->datacolmuxpredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Data colmux post decode (cm^2): %f\n",xcacti_calculate_area(arearesult->datacolmuxpostdecode_area,parameters->fudgefactor)/100000000.0);
    printf("Data write signal (cm^2): %f\n",(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports)*xcacti_calculate_area(arearesult->datawritesig_area,parameters->fudgefactor)/100000000.0);

    printf("\nTag array (cm^2): %f\n",xcacti_calculate_area(arearesult->tagarray_area,parameters->fudgefactor)/100000000.0);
    printf("Tag predecode (cm^2): %f\n",xcacti_calculate_area(arearesult->tagpredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag colmux predecode (cm^2): %f\n",xcacti_calculate_area(arearesult->tagcolmuxpredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag colmux post decode (cm^2): %f\n",xcacti_calculate_area(arearesult->tagcolmuxpostdecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag output driver decode (cm^2): %f\n",xcacti_calculate_area(arearesult->tagoutdrvdecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag output driver enable signals (cm^2): %f\n",(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports)*xcacti_calculate_area(arearesult->tagoutdrvsig_area,parameters->fudgefactor)/100000000.0);

   printf("\nPercentage of data ramcells alone of total area: %f %%\n",100*area_all_dataramcells/(arearesult->totalarea/100000000.0));
    printf("Percentage of tag ramcells alone of total area: %f %%\n",100*area_all_tagramcells/(arearesult->totalarea/100000000.0));
    printf("Percentage of total control/routing alone of total area: %f %%\n",100*(arearesult->totalarea/100000000.0-area_all_dataramcells-area_all_tagramcells)/(arearesult->totalarea/100000000.0));
    printf("\nSubbank Efficiency : %f\n",(area_all_dataramcells+area_all_tagramcells)*100/(arearesult->totalarea/100000000.0));
    printf("Total Efficiency : %f\n",(parameters->NSubbanks)*(area_all_dataramcells+area_all_tagramcells)*100/(xcacti_calculate_area(*arearesult_subbanked,parameters->fudgefactor)/100000000.0));
    printf("\nTotal area One Subbank (cm^2): %f\n",arearesult->totalarea/100000000.0);
    printf("Total area subbanked (cm^2): %f\n",xcacti_calculate_area(*arearesult_subbanked,parameters->fudgefactor)/100000000.0); 

}

int xcacti_input_data(int argc,  char *argv[], parameter_type *parameters)
{
  int C=0;
  int B=32;
  int A=4;
  int num_read_ports = 1;
  int num_write_ports = 1;
  int num_readwrite_ports = 1;
  int num_single_ended_read_ports = 0;
  int NSubbanks=1;
  float tech=0.09;

#ifdef XCACTI
  int latchsa = 0;
  int ignore_tag = 0;
#endif

  int total_size=0;

  int opt;

  while ((opt = getopt(argc, argv, "s:l:a:t:x:r:w:b:SDP")) != -1) {
    switch (opt) {
    case 's':
      total_size = atoi(optarg);
      break;
    case 'l':
      B = atoi(optarg);
      break;
    case 'a':
      printf("here[%s]\n",optarg);
      if (!strcmp(optarg,"FA")) {
	parameters->fully_assoc = 1;
	A = 0 ;
      }else if (!strcmp(optarg,"DM")) {
	A=1;
	parameters->fully_assoc = 0;
      }else{
	A = atoi(optarg);
	parameters->fully_assoc = 0;
      }
      break;
    case 't':
      tech = atof(optarg)/1000;
      if (tech <= 0 || tech >= 0.8) {
	printf("Feature size must be <= 800nm\n");
	exit(0);
      }
      break;
    case 'r':
      num_read_ports = atoi(optarg);
      break;
    case 'w':
      num_write_ports = atoi(optarg);
      break;
    case 'x':
      num_readwrite_ports = atoi(optarg);
      break;
    case 'b':
      NSubbanks = atoi(optarg);
      break;
#ifdef XCACTI
    case 'S':
      latchsa = 1;
      break;
    case 'D':
      ignore_tag = 1;
      break;
    case 'P':
      PHASEDCACHE = 1;
      break;
#endif
    default:
      show_usage();
      exit(1);
      break;
    }
  }
   
  if (NSubbanks == 0 ) {
    printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
    exit(0);
  }

  C = total_size/NSubbanks;

  if (parameters->fully_assoc) {
    A=C/B; // re-adjust associativity
  }

  num_single_ended_read_ports = num_read_ports;

#ifdef XCACTI
  parameters->latchsa    = latchsa;
  parameters->ignore_tag = ignore_tag;
#endif
  parameters->cache_size = C;
  parameters->block_size = B;
  parameters->associativity = A;
  parameters->num_readwrite_ports = num_readwrite_ports;
  parameters->num_read_ports = num_read_ports;
  parameters->num_write_ports = num_write_ports;
  parameters->num_single_ended_read_ports =num_single_ended_read_ports;
  parameters->number_of_sets = C/(B*A);
  parameters->fudgefactor = .8/tech;   
  parameters->tech_size=tech;
  parameters->NSubbanks = NSubbanks;

  if (!xcacti_parameter_check(parameters)) {
    show_usage();
    exit(0);
  }

  //   int rows = parameters->number_of_sets;
  //   int columns = 8*parameters->block_size*parameters->associativity;

  return(OK);
}

void xcacti_parameter_adjust(parameter_type *parameters) {

  parameters->VddPow=4.5/(pow(parameters->fudgefactor,(2.0/3.0)));
  if (parameters->VddPow < 0.7)
    parameters->VddPow=0.7;
  if (parameters->VddPow > 5.0)
    parameters->VddPow=5.0;
  parameters->VbitprePow=parameters->VddPow*3.3/4.5;

#ifdef XCACTI
  parameters->gscalingfactor = 1;
  Wdecdrivep *= parameters->gscalingfactor;
  Wdecdriven *= parameters->gscalingfactor;
  Wdec3to8n  *= parameters->gscalingfactor;
  Wdec3to8p  *= parameters->gscalingfactor;
  WdecNORn   *= parameters->gscalingfactor;
  WdecNORp   *= parameters->gscalingfactor;
  Wdecinvn   *= parameters->gscalingfactor;
  Wdecinvp   *= parameters->gscalingfactor;
  Wworddrivemax  *= parameters->gscalingfactor;
  Wmemcella  *= parameters->gscalingfactor;
  Wmemcellbscale *= parameters->gscalingfactor;

  Wbitpreequ  *= parameters->gscalingfactor;
  Wbitmuxn    *= parameters->gscalingfactor;
  WsenseQ1to4 *= parameters->gscalingfactor;
  Wcompinvp1  *= parameters->gscalingfactor;
  Wcompinvn1  *= parameters->gscalingfactor;
  Wcompinvp2  *= parameters->gscalingfactor;
  Wcompinvn2  *= parameters->gscalingfactor;
  Wcompinvp3  *= parameters->gscalingfactor;
  Wcompinvn3  *= parameters->gscalingfactor;
  Wevalinvp   *= parameters->gscalingfactor;
  Wevalinvn   *= parameters->gscalingfactor;

  Wfadriven   *= parameters->gscalingfactor;
  Wfadrivep   *= parameters->gscalingfactor;
  Wfadrive2n  *= parameters->gscalingfactor;
  Wfadrive2p  *= parameters->gscalingfactor;
  Wfadecdrive1n *= parameters->gscalingfactor;
  Wfadecdrive1p *= parameters->gscalingfactor;
  Wfadecdrive2n *= parameters->gscalingfactor;
  Wfadecdrive2p *= parameters->gscalingfactor;
  Wfadecdriven *= parameters->gscalingfactor;
  Wfadecdrivep *= parameters->gscalingfactor;
  Wfaprechn  *= parameters->gscalingfactor;
  Wfaprechp  *= parameters->gscalingfactor;
  Wdummyn    *= parameters->gscalingfactor;
  Wdummyinvn *= parameters->gscalingfactor;
  Wdummyinvp *= parameters->gscalingfactor;
  Wfainvn    *= parameters->gscalingfactor;
  Wfainvp    *= parameters->gscalingfactor;
  Waddrnandn *= parameters->gscalingfactor;
  Waddrnandp *= parameters->gscalingfactor;
  Wfanandn   *= parameters->gscalingfactor;
  Wfanandp   *= parameters->gscalingfactor;
  Wfanorn    *= parameters->gscalingfactor;
  Wfanorp    *= parameters->gscalingfactor;
  Wdecnandn  *= parameters->gscalingfactor;
  Wdecnandp  *= parameters->gscalingfactor;

  Wcompn     *= parameters->gscalingfactor; 
  Wcompp     *= parameters->gscalingfactor; 
  Wmuxdrv12n *= parameters->gscalingfactor; 
  Wmuxdrv12p *= parameters->gscalingfactor; 
  WmuxdrvNANDn *= parameters->gscalingfactor; 
  WmuxdrvNANDp *= parameters->gscalingfactor; 
  WmuxdrvNORn  *= parameters->gscalingfactor; 
  WmuxdrvNORp  *= parameters->gscalingfactor; 
  Wmuxdrv3n  *= parameters->gscalingfactor; 
  Wmuxdrv3p  *= parameters->gscalingfactor; 

  //  parameters->gscalingfactor = parameters->tech_size/0.8; 

  Woutdrvseln  *= parameters->gscalingfactor;
  Woutdrvselp  *= parameters->gscalingfactor;
  Woutdrvnandn *= parameters->gscalingfactor;
  Woutdrvnandp *= parameters->gscalingfactor;
  Woutdrvnorn  *= parameters->gscalingfactor;
  Woutdrvnorp  *= parameters->gscalingfactor;
  Woutdrivern  *= parameters->gscalingfactor;
  Woutdriverp  *= parameters->gscalingfactor;

  BitWidth   *= parameters->gscalingfactor;
  BitHeight  *= parameters->gscalingfactor;

  Cout *= (parameters->gscalingfactor*parameters->gscalingfactor);

  Wsenseextdrv1p *= parameters->gscalingfactor;
  Wsenseextdrv1n *= parameters->gscalingfactor;
  Wsenseextdrv2p *= parameters->gscalingfactor;
  Wsenseextdrv2n *= parameters->gscalingfactor;

  psensedata *= (parameters->VddPow/Vdd)*(parameters->VddPow/Vdd);
  psensetag  *= (parameters->VddPow/Vdd)*(parameters->VddPow/Vdd);
  Vbitsense  *= parameters->VddPow/Vdd;
#endif

}

void xcacti_parameters_dump(const parameter_type *parameters) {

  printf("\nCache Parameters:\n");
  printf("  Number of Subbanks: %d\n",(int)(parameters->NSubbanks));
  printf("  Total Cache Size: %d\n",(int) (parameters->cache_size*(parameters->NSubbanks)));
  printf("  Size in bytes of Subbank: %d\n",parameters->cache_size);
  printf("  Number of sets: %d\n",parameters->number_of_sets);
  if (parameters->fully_assoc)
    printf("  Associativity: fully associative\n");
  else
    {
      if (parameters->associativity==1)
	printf("  Associativity: direct mapped\n");
      else
	printf("  Associativity: %d\n",parameters->associativity);
    }
  printf("  Block Size (bytes): %d\n",parameters->block_size);
  printf("  Read/Write Ports: %d\n",parameters->num_readwrite_ports);
  printf("  Read Ports: %d\n",parameters->num_read_ports);
  printf("  Write Ports: %d\n",parameters->num_write_ports);
  printf("  Technology Size: %2.2fum\n", parameters->tech_size);
  printf("  Vdd: %2.1fV\n", parameters->VddPow);
}


void xcacti_output_data(const result_type *result,
		 const arearesult_type *arearesult,
		 const area_type *arearesult_subbanked,
		 const parameter_type *parameters)
{
  double datapath,tagpath;
   
  datapath = result->subbank_address_routing_delay+result->decoder_delay_data+result->wordline_delay_data+result->bitline_delay_data+result->sense_amp_delay_data+result->total_out_driver_delay_data+result->data_output_delay;
  if (parameters->associativity == 1) {
    tagpath = result->subbank_address_routing_delay+result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_valid_delay;
  } else {
    tagpath = result->subbank_address_routing_delay+result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_mux_delay+result->selb_delay+result->data_output_delay+result->total_out_driver_delay_data;
  }
  
  printf("\n---------- CACTI version 3.2 ----------\n");
  xcacti_parameters_dump(parameters);
  printf("\nAccess Time (ns): %g\n",result->access_time*1e9);
  printf("Cycle Time (wave pipelined) (ns):  %g\n",result->cycle_time*1e9);
  if (parameters->fully_assoc)
    {
      printf("Total Power all Banks (nJ): %g\n",result->total_power_allbanks*1e9);
      printf("Total Power Without Routing (nJ): %g\n",result->total_power_without_routing*1e9);
      printf("Total Routing Power (nJ): %g\n",result->total_routing_power*1e9);

      printf("Maximum Bank Power (nJ):  %g\n",(result->subbank_address_routing_power+result->decoder_power_data+result->wordline_power_data+result->bitline_power_data+result->sense_amp_power_data+result->data_output_power+result->total_out_driver_power_data)*1e9);
      //      printf("Power (W) - 500MHz:  %g\n",(result->decoder_power_data+result->wordline_power_data+result->bitline_power_data+result->sense_amp_power_data+result->data_output_power)*500*1e6);
    }
  else
    {
      printf("Total Power all Banks (nJ): %g\n",result->total_power_allbanks*1e9);
      printf("Total Power Without Routing (nJ): %g\n",result->total_power_without_routing*1e9);
      printf("Total Routing Power (nJ): %g\n",result->total_routing_power*1e9);
      printf("Maximum Bank Power (nJ):  %g\n",(result->subbank_address_routing_power+result->decoder_power_data+result->wordline_power_data+result->bitline_power_data+result->sense_amp_power_data+result->total_out_driver_power_data+result->decoder_power_tag+result->wordline_power_tag+result->bitline_power_tag+result->sense_amp_power_tag+result->compare_part_power+result->drive_valid_power+result->drive_mux_power+result->selb_power+result->data_output_power)*1e9);
      //      printf("Power (W) - 500MHz:  %g\n",(result->decoder_power_data+result->wordline_power_data+result->bitline_power_data+result->sense_amp_power_data+result->total_out_driver_power_data+result->decoder_power_tag+result->wordline_power_tag+result->bitline_power_tag+result->sense_amp_power_tag+result->compare_part_power+result->drive_valid_power+result->drive_mux_power+result->selb_power+result->data_output_power)*500*1e6);
    }
  printf("\nBest Ndwl (L1): %d\n",result->best_Ndwl);
  printf("Best Ndbl (L1): %d\n",result->best_Ndbl);
  printf("Best Nspd (L1): %d\n",result->best_Nspd);
  printf("Best Ntwl (L1): %d\n",result->best_Ntwl);
  printf("Best Ntbl (L1): %d\n",result->best_Ntbl);
  printf("Best Ntspd (L1): %d\n",result->best_Ntspd);
  printf("Nor inputs (data): %d\n",result->data_nor_inputs);
  printf("Nor inputs (tag): %d\n",result->tag_nor_inputs);

  output_area_components(arearesult,arearesult_subbanked,parameters);
  printf("\nTime Components:\n");

  output_time_components(result,parameters);
}


int xcacti_parameter_check(const parameter_type *parameters) {

   double logbanks, assoc;
   double logbanksfloor, assocfloor;

   if (parameters->NSubbanks < 1 ) {
     printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
     return 0;
   }
   
   logbanks = logtwo((double)(parameters->NSubbanks));
   logbanksfloor = floor(logbanks);
   
   if(logbanks > logbanksfloor){
     printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
     return 0;
   }

   if ((parameters->cache_size < 64)) {
     printf("Cache size must >=64\n");
     return 0;
   }

   if ((parameters->block_size*8 < BITOUT)) {
     printf("Block size must be at least %d\n", BITOUT/8);
     return 0;
   }

   if ((parameters->num_readwrite_ports < 0) || (parameters->num_write_ports < 0) || (parameters->num_read_ports < 0)) {
     printf("Ports must >=0\n");
     return 0;
   }

   if (parameters->num_readwrite_ports > 2) {
     printf("Maximum of 2 read/write ports\n");
     return 0;
   }

   if ((parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports) < 1) {
     printf("Must have at least one port\n");
     return 0;
   }

   if ((parameters->associativity < 1)) {
     printf("Associativity must >= 1\n");
     return 0;
   }

   assoc = logtwo((double)(parameters->associativity));
   assocfloor = floor(assoc);
   
   if(assoc > assocfloor){
     printf("Associativity should be a power of 2 [%g vs %g]\n",assoc, assocfloor);
     return 0;
   }

   if (parameters->associativity > 32 && !parameters->fully_assoc) {
     printf("Associativity must <= 32\n or try FA (fully associative)\n");
     return 0;
   }

   if (parameters->cache_size/(8*parameters->block_size*parameters->associativity)<=0 && !parameters->fully_assoc) {
     printf("Number of sets is too small:\n  Need to either increase cache size, or decrease associativity or block size\n  (or use fully associative cache)\n");
     return 0;
   }

   if (parameters->number_of_sets < 1) {
     printf("Less than one set...\n");
     return 0;
   }   

   return 1;
}
