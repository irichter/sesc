#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEF_H
#define DEF_H
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

/*  The following are things you might want to change
 *  when compiling
 */

/*
 * The output can be in 'long' format, which shows everything, or
 * 'short' format, which is just what a program like 'grap' would
 * want to see
 */

#define LONG 1
#define SHORT 2

#define OUTPUTTYPE LONG

/*
 * Address bits in a word, and number of output bits from the cache 
 */

#define ADDRESS_BITS 32
#define BITOUT 64


/* limits on the various N parameters */

#define MAXN 32          /* Maximum for Ndwl,Ntwl,Ndbl,Ntbl */
#define MAXSUBARRAYS 32    /* Maximum subarrays for data and tag arrays */
#define MAXSPD 32         /* Maximum for Nspd, Ntspd */



/*
 * The following scale factor can be used to scale between technologies.
 * To convert from 0.8um to 0.5um, make FUDGEFACTOR = 1.6
 */
 
static float FUDGEFACTOR;
#define FEATURESIZE 0.8

/*===================================================================*/

/*
 * Cache layout parameters and process parameters 
 */


/*
 * CMOS 0.8um model parameters
 *   - directly from Appendix II of tech report
 */

/*#define WORDWIRELENGTH (8+2*WIREPITCH*(EXTRAWRITEPORTS)+2*WIREPITCH*(EXTRAREADPORTS))*/
/*#define BITWIRELENGTH (16+2*WIREPITCH*(EXTRAWRITEPORTS+EXTRAREADPORTS))*/
#define WIRESPACING (2*FEATURESIZE)
#define WIREWIDTH (3*FEATURESIZE)
#define WIREPITCH (WIRESPACING+WIREWIDTH)
#define Cmetal 275e-18
#define Rmetal 48e-3

static double Cwordmetal;
static double Cbitmetal;
static double Rwordmetal;
static double Rbitmetal;
static double FACwordmetal;
static double FACbitmetal;
static double FARwordmetal;
static double FARbitmetal;

static int muxover;

/* fF/um2 at 1.5V */
#define Cndiffarea    0.137e-15

/* fF/um2 at 1.5V */
#define Cpdiffarea    0.343e-15

/* fF/um at 1.5V */
#define Cndiffside    0.275e-15

/* fF/um at 1.5V */
#define Cpdiffside    0.275e-15

/* fF/um at 1.5V */
#define Cndiffovlp    0.138e-15

/* fF/um at 1.5V */
#define Cpdiffovlp    0.138e-15

/* fF/um assuming 25% Miller effect */
#define Cnoxideovlp   0.263e-15

/* fF/um assuming 25% Miller effect */
#define Cpoxideovlp   0.338e-15

/* um */
#define Leff          (0.8)

/* fF/um2 */
#define Cgate         1.95e-15	

/* fF/um2 */
#define Cgatepass     1.45e-15	

/* note that the value of Cgatepass will be different depending on 
   whether or not the source and drain are at different potentials or
   the same potential.  The two values were averaged */

/* fF/um */
#define Cpolywire	(0.25e-15)			 

/* ohms*um of channel width */
#define Rnchannelstatic	(25800)

/* ohms*um of channel width */
#define Rpchannelstatic	(61200)

#define Rnchannelon	(8751)

#define Rpchannelon	(20160)


#define Vdd		5
static float VddPow;
/* Threshold voltages (as a proportion of Vdd)
   If you don't know them, set all values to 0.5 */

#define VTHNAND       0.561
#define VTHFA1        0.452
#define VTHFA2        0.304
#define VTHFA3        0.420
#define VTHFA4        0.413
#define VTHFA5        0.405
#define VTHFA6        0.452
#define VSINV         0.452   
#define VTHINV100x60  0.438   /* inverter with p=100,n=60 */
#define VTHINV360x240 0.420   /* inverter with p=360, n=240 */
#define VTHNAND60x90  0.561   /* nand with p=60 and three n=90 */
#define VTHNOR12x4x1  0.503   /* nor with p=12, n=4, 1 input */
#define VTHNOR12x4x2  0.452   /* nor with p=12, n=4, 2 inputs */
#define VTHNOR12x4x3  0.417   /* nor with p=12, n=4, 3 inputs */
#define VTHNOR12x4x4  0.390   /* nor with p=12, n=4, 4 inputs */
#define VTHOUTDRINV    0.437
#define VTHOUTDRNOR   0.379
#define VTHOUTDRNAND  0.63
#define VTHOUTDRIVE   0.425
#define VTHCOMPINV    0.437
#define VTHMUXNAND    0.548
#define VTHMUXDRV1    0.406
#define VTHMUXDRV2    0.334
#define VTHMUXDRV3    0.478
#define VTHEVALINV    0.452
#define VTHSENSEEXTDRV  0.438

#define VTHNAND60x120 0.522

/* transistor widths in um (as described in tech report, appendix 1) */

#define Wdecdrivep	(360.0)
#define Wdecdriven	(240.0)
#define Wdec3to8n     120.0
#define Wdec3to8p     60.0
#define WdecNORn       2.4
#define WdecNORp      12.0
#define Wdecinvn      20.0
#define Wdecinvp      40.0 
#define Wworddrivemax 100.0
#define Wmemcella	(2.4)
#define Wmemcellbscale	2		/* means 2x bigger than Wmemcella */
#define Wbitpreequ	(80.0)
#define Wbitmuxn	(10.0)
#define WsenseQ1to4	(4.0)
#define Wcompinvp1	(10.0)
#define Wcompinvn1	(6.0)
#define Wcompinvp2	(20.0)
#define Wcompinvn2	(12.0)
#define Wcompinvp3	(40.0)
#define Wcompinvn3	(24.0)
#define Wevalinvp	(80.0)
#define Wevalinvn	(40.0)

#define Wfadriven    (50.0)
#define Wfadrivep    (100.0)
#define Wfadrive2n    (200.0)
#define Wfadrive2p    (400.0)
#define Wfadecdrive1n    (5.0)
#define Wfadecdrive1p    (10.0)
#define Wfadecdrive2n    (20.0)
#define Wfadecdrive2p    (40.0)
#define Wfadecdriven    (50.0)
#define Wfadecdrivep    (100.0)
#define Wfaprechn       (6.0)
#define Wfaprechp       (10.0)
#define Wdummyn         (10.0)
#define Wdummyinvn      (60.0)
#define Wdummyinvp      (80.0)
#define Wfainvn         (10.0)
#define Wfainvp         (20.0)
#define Waddrnandn      (50.0)
#define Waddrnandp      (50.0)
#define Wfanandn        (20.0)
#define Wfanandp        (30.0)
#define Wfanorn         (5.0)
#define Wfanorp         (10.0)
#define Wdecnandn       (10.0)
#define Wdecnandp       (30.0)

#define Wcompn		(10.0)
#define Wcompp		(30.0)
#define Wmuxdrv12n	(60.0)
#define Wmuxdrv12p	(100.0)
#define WmuxdrvNANDn    (60.0)
#define WmuxdrvNANDp    (80.0)
#define WmuxdrvNORn	(40.0)
#define WmuxdrvNORp	(100.0)
#define Wmuxdrv3n	(80.0)
#define Wmuxdrv3p	(200.0)
#define Woutdrvseln	(24.0)
#define Woutdrvselp	(40.0)
#define Woutdrvnandn	(10.0)
#define Woutdrvnandp	(30.0)
#define Woutdrvnorn	(5.0)
#define Woutdrvnorp	(20.0)
#define Woutdrivern	(48.0)
#define Woutdriverp	(80.0)

#define Wsenseextdrv1p (80.0)
#define Wsenseextdrv1n (40.0)
#define Wsenseextdrv2p (240.0)
#define Wsenseextdrv2n (160.0)

/* other stuff (from tech report, appendix 1) */

#define krise		(0.4e-9)
#define tsensedata	(5.8e-10)
// #define psensedata      (0.025e-9)
#define psensedata      (0.02e-9)
#define tsensescale     0.02e-10
#define tsensetag	(2.6e-10)
// #define psensetag       (0.01e-9)
#define psensetag	(0.016e-9)
#define tfalldata	(7e-10)
#define tfalltag	(7e-10)
#define Vbitpre		(3.3)
static float VbitprePow;
#define Vt		(1.09)
#define Vbitsense	(0.10)

/* bit width of RAM cell in um */
#define BitWidth	(8.0)

/* bit height of RAM cell in um */
#define BitHeight	(16.0)

#define Cout		(0.5e-12)

/*===================================================================*/

/*
 * The following are things you probably wouldn't want to change.  
 */


#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif
#define OK 1
#define ERROR 0
#define BIGNUM 1e30
#define DIVIDE(a,b) ((b)==0)? 0:(a)/(b)
#define MAX(a,b) (((a)>(b))?(a):(b))

#define WAVE_PIPE 3
#define MAX_COL_MUX 16

/* Used to communicate with the horowitz model */

#define RISE 1
#define FALL 0
#define NCH  1
#define PCH  0


/* Used to pass values around the program */

typedef struct {
   int cache_size;
   int number_of_sets;
   int associativity;
   int block_size;
   int num_write_ports;
   int num_readwrite_ports;
   int num_read_ports;
   int num_single_ended_read_ports;
  char fully_assoc;
  float fudgefactor;
  float tech_size;
  float VddPow;
} parameter_type;

typedef struct {
   double access_time,cycle_time;
  double senseext_scale;
  double total_power;
   int best_Ndwl,best_Ndbl;
  double max_power, max_access_time;
   int best_Nspd;
   int best_Ntwl,best_Ntbl;
   int best_Ntspd;
  int best_muxover;
   double total_routing_power;
   double total_power_without_routing, total_power_allbanks;
   double subbank_address_routing_delay,subbank_address_routing_power;
   double decoder_delay_data,decoder_delay_tag;
   double decoder_power_data,decoder_power_tag;
   double dec_data_driver,dec_data_3to8,dec_data_inv;
   double dec_tag_driver,dec_tag_3to8,dec_tag_inv;
   double wordline_delay_data,wordline_delay_tag;
   double wordline_power_data,wordline_power_tag;
   double bitline_delay_data,bitline_delay_tag;
   double bitline_power_data,bitline_power_tag;
  double sense_amp_delay_data,sense_amp_delay_tag;
  double sense_amp_power_data,sense_amp_power_tag;
  double total_out_driver_delay_data;
  double total_out_driver_power_data;
   double compare_part_delay;
   double drive_mux_delay;
   double selb_delay;
   double compare_part_power;
   double drive_mux_power;
   double selb_power;
   double data_output_delay;
   double data_output_power;
   double drive_valid_delay;
   double drive_valid_power;
   double precharge_delay;
  int data_nor_inputs;
  int tag_nor_inputs;
} result_type;
#endif
#ifdef __cplusplus
}
#endif
