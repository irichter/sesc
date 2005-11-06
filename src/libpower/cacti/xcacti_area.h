#ifndef AREADEF_H
#define AREADEF_H
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

extern double area_all_datasubarrays;
extern double area_all_tagsubarrays;
extern double area_all_dataramcells;
extern double area_all_tagramcells;
extern double faarea_all_subarrays ;

extern double aspect_ratio_data;
extern double aspect_ratio_tag;
extern double aspect_ratio_subbank;
extern double aspect_ratio_total;

#define Widthptondiff 4.0
#define Widthtrack    3.2
#define Widthcontact  1.6
#define Wpoly         0.8
#define ptocontact    0.4
#define stitch_ramv   6.0 
#define BitHeight16x2 33.6
#define stitch_ramh   12.0
#define BitWidth16x2  192.8
#define WidthNOR1     11.6
#define WidthNOR2     13.6
#define WidthNOR3     20.8
#define WidthNOR4     28.8
#define WidthNOR5     34.4
#define WidthNOR6     41.6
#define Predec_height1    140.8
#define Predec_width1     270.4
#define Predec_height2    140.8
#define Predec_width2     539.2
#define Predec_height3    281.6    
#define Predec_width3     584.0
#define Predec_height4    281.6
#define Predec_width4     628.8
#define Predec_height5    422.4
#define Predec_width5     673.6
#define Predec_height6    422.4
#define Predec_width6     718.4
#define Wwrite		  1.2
#define SenseampHeight    152.0
#define OutdriveHeight	  200.0
#define FAOutdriveHeight  229.2
#define FArowWidth	  382.8
#define CAM2x2Height_1p	  48.8
#define CAM2x2Width_1p	  44.8
#define CAM2x2Height_2p   80.8    
#define CAM2x2Width_2p    76.8
#define DatainvHeight     25.6
#define Wbitdropv 	  30.0
#define decNandWidth      34.4
#define FArowNANDWidth    71.2
#define FArowNOR_INVWidth 28.0  

#define FAHeightIncrPer_first_rw_or_w_port 16.0
#define FAHeightIncrPer_later_rw_or_w_port 16.0
#define FAHeightIncrPer_first_r_port       12.0
#define FAHeightIncrPer_later_r_port       12.0
#define FAWidthIncrPer_first_rw_or_w_port  16.0 
#define FAWidthIncrPer_later_rw_or_w_port  9.6
#define FAWidthIncrPer_first_r_port        12.0
#define FAWidthIncrPer_later_r_port        9.6

#define tracks_precharge_p    12
#define tracks_precharge_nx2   5 
#define tracks_outdrvselinv_p  3
#define tracks_outdrvfanand_p  6  

struct area_type {
  double height;
  double width;
};

struct arearesult_type {
  area_type dataarray_area,datapredecode_area;
  area_type datacolmuxpredecode_area,datacolmuxpostdecode_area;
  area_type datawritesig_area;
  area_type tagarray_area,tagpredecode_area;
  area_type tagcolmuxpredecode_area,tagcolmuxpostdecode_area;
  area_type tagoutdrvdecode_area;
  area_type tagoutdrvsig_area;
  double totalarea;
  double max_efficiency, efficiency;
  double max_aspect_ratio_total, aspect_ratio_total;
} ;


void xcacti_area_subbanked(int baddr,
			   int b0,
			   int RWP, int ERP, int EWP,
			   int Ndbl, int Ndwl, int Nspd,
			   int Ntbl, int Ntwl, int Ntspd,
			   const parameter_type *parameters,
			   area_type *result_subbanked,
			   arearesult_type *result);

int xcacti_organizational_parameters_valid(int B, int A, int C, 
					   int Ndwl, int Ndbl, int Nspd, 
					   int Ntwl, int Ntbl, int Ntspd,
					   char assoc);

double xcacti_calculate_area(area_type module_area, double techscaling_factor);

#endif
