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
#include <string.h>
#include "def.h"
#include "areadef.h"
#include "io.h"
#include "cacti_time.h"

#define NEXTINT(a) skip(); scanf("%d",&(a));
#define NEXTFLOAT(a) skip(); scanf("%lf",&(a));
						
/*---------------------------------------------------------------*/

extern double calculate_area(area_type,double);

int ca_input_data(argc,argv,parameters,NSubbanks)
int argc; char *argv[];
parameter_type *parameters;
double *NSubbanks;
{
   int C,B,A,ERP,EWP,RWP,NSER;
   float tech;
   double logbanks, assoc;
   double logbanksfloor, assocfloor;


   /* input parameters:
         C B A ERP EWP */
   if ((argc!=6) && (argc!=9)) {
      printf("Cmd+line parameters: C B A TECH NSubbanks\n");
      printf("                 OR: C B A TECH RWP ERP EWP NSubbanks\n");
      exit(0);
   }

   B = atoi(argv[2]);
   if ((B < 1)) {
       printf("Block size must >=1\n");
       exit(0);
   }

   if ((B*8 < BITOUT)) {
       printf("Block size must be at least %d\n", BITOUT/8);
       exit(0);
   }
   

   tech = atof(argv[4]);
   if ((tech <= 0)) {
       printf("Feature size must be > 0\n");
       exit(0);
   }
   if ((tech <= 0)) {
       printf("Feature size must be <= 0.80 (um)\n");
       exit(0);
   }

   if (argc==9)
     {
       RWP = atoi(argv[5]);
       ERP = atoi(argv[6]);
       EWP = atoi(argv[7]);
       NSER = ERP;

       if ((RWP < 0) || (EWP < 0) || (ERP < 0)) {
	 printf("Ports must >=0\n");
	 exit(0);
       }
       if (RWP > 2) {
	 printf("Maximum of 2 read/write ports\n");
	 return ERROR ;
       }
       if ((RWP+ERP+EWP) < 1) {
       	 printf("Must have at least one port\n");
       	 exit(0);
       }
       *NSubbanks = atoi(argv[8]);

       if (*NSubbanks < 1 ) {
         printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
         exit(0);
       }

       logbanks = logtwo((double)(*NSubbanks));
       logbanksfloor = floor(logbanks);
      
       if(logbanks > logbanksfloor){
         printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
         exit(0);
       }

     }
   else
     {
       RWP=1;
       ERP=0;
       EWP=0;
       NSER=0;

       *NSubbanks = atoi(argv[5]);
       if (*NSubbanks < 1 ) {
         printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
         exit(0); 
       }
       logbanks = logtwo((double)(*NSubbanks));
       logbanksfloor = floor(logbanks);

       if(logbanks > logbanksfloor){
         printf("Number of subbanks should be greater than or equal to 1 and should be a power of 2\n");
         exit(0);
       }

     }
 
   C = atoi(argv[1])/((int) (*NSubbanks));
   if ((C < 64)) {
       printf("Cache size must >=64\n");
       exit(0);
   }
 
   if (!strcmp(argv[3],"FA"))
     {
       A=C/B;
       parameters->fully_assoc = 1;
     }
   else
     {
       if (!strcmp(argv[3],"DM"))
         {
           A=1;
           parameters->fully_assoc = 0;
         }
       else
         {
           parameters->fully_assoc = 0;
           A = atoi(argv[3]);
           if ((A < 1)) {
             printf("Associativity must >= 1\n");
             exit(0);
           }
           assoc = logtwo(atof(argv[3]));
           printf("%lf",assoc) ;
           assocfloor = floor(assoc);

           if(assoc  > assocfloor){
             printf("Associativity should be a power of 2\n");
             exit(0);
           }

           if ((A > 32)) {
             printf("Associativity must <= 32\n or try FA (fully associative)\n");
             exit(0);
           }
         }
     }

   if (C/(8*B*A)<0 && !parameters->fully_assoc) {
     printf("Number of sets is too small:\n  Need to either increase cache size, or decrease associativity or block size\n  (or use fully associative cache)\n");
       exit(0);
   }
 
   parameters->cache_size = C;
   parameters->block_size = B;
   parameters->associativity = A;
   parameters->num_readwrite_ports = RWP;
   parameters->num_read_ports = ERP;
   parameters->num_write_ports = EWP;
   parameters->num_single_ended_read_ports =NSER;
   parameters->number_of_sets = C/(B*A);
   parameters->fudgefactor = .8/tech;   
   parameters->tech_size=tech;
   if (parameters->number_of_sets < 1) {
      printf("Less than one set...\n");
      exit(0);
   }   
 
   return(OK);
}

void output_time_components(result,parameters)
result_type *result;
parameter_type *parameters;
{
int A;
       printf(" address routing delay (ns): %g\n",result->subbank_address_routing_delay/1e-9);
       printf(" address routing power (nJ): %g\n",result->subbank_address_routing_power*1e9);
 
  A=parameters->associativity;
   if (!parameters->fully_assoc)
     {
       printf(" decode_data (ns): %g\n",result->decoder_delay_data/1e-9);
       printf("             (nJ): %g\n",result->decoder_power_data*1e9);
     }
   else
     {
       printf(" tag_comparison (ns): %g\n",result->decoder_delay_data/1e-9);
       printf("                (nJ): %g\n",result->decoder_power_data*1e9);
     }
   printf(" wordline and bitline data (ns): %g\n",(result->wordline_delay_data+result->bitline_delay_data)/1e-9);
   printf("            wordline power (nJ): %g\n",result->wordline_power_data*1e9);
   printf("             bitline power (nJ): %g\n",result->bitline_power_data*1e9);
   printf(" sense_amp_data (ns): %g\n",result->sense_amp_delay_data/1e-9);
   printf("                (nJ): %g\n",result->sense_amp_power_data*1e9);
   if (!parameters->fully_assoc)
     {
       printf(" decode_tag (ns): %g\n",result->decoder_delay_tag/1e-9);
       printf("            (nJ): %g\n",result->decoder_power_tag*1e9);
       printf(" wordline and bitline tag (ns): %g\n",(result->wordline_delay_tag+result->bitline_delay_tag)/1e-9);

       printf("           wordline power (nJ): %g\n",result->wordline_power_tag*1e9);
       printf("            bitline power (nJ): %g\n",result->bitline_power_tag*1e9);
       printf(" sense_amp_tag (ns): %g\n",result->sense_amp_delay_tag/1e-9);
       printf("               (nJ): %g\n",result->sense_amp_power_tag*1e9);
       printf(" compare (ns): %g\n",result->compare_part_delay/1e-9);
       printf("         (nJ): %g\n",result->compare_part_power*1e9);
       if (A == 1)
         {
           printf(" valid signal driver (ns): %g\n",result->drive_valid_delay/1e-9);
           printf("                     (nJ): %g\n",result->drive_valid_power*1e9);
         }
       else {
         printf(" mux driver (ns): %g\n",result->drive_mux_delay/1e-9);
         printf("            (nJ): %g\n",result->drive_mux_power*1e9);
         printf(" sel inverter (ns): %g\n",result->selb_delay/1e-9);
         printf("              (nJ): %g\n",result->selb_power*1e9);
       }
     }
   printf(" data output driver (ns): %g\n",result->data_output_delay/1e-9);
   printf("                    (nJ): %g\n",result->data_output_power*1e9);
   printf(" total_out_driver (ns): %g\n", result->total_out_driver_delay_data/1e-9);
   printf("                 (nJ): %g\n", result->total_out_driver_power_data*1e9);

   printf(" total data path (without output driver) (ns): %g\n",result->subbank_address_routing_delay/1e-9+result->decoder_delay_data/1e-9+result->wordline_delay_data/1e-9+result->bitline_delay_data/1e-9+result->sense_amp_delay_data/1e-9);
   if (!parameters->fully_assoc)
     {
       if (A==1)
         printf(" total tag path is dm (ns): %g\n", result->subbank_address_routing_delay/1e-9+result->decoder_delay_tag/1e-9+result->wordline_delay_tag/1e-9+result->bitline_delay_tag/1e-9+result->sense_amp_delay_tag/1e-9+result->compare_part_delay/1e-9);
       else
         printf(" total tag path is set assoc (ns): %g\n", result->subbank_address_routing_delay/1e-9+result->decoder_delay_tag/1e-9+result->wordline_delay_tag/1e-9+result->bitline_delay_tag/1e-9+result->sense_amp_delay_tag/1e-9+result->compare_part_delay/1e-9+result->drive_mux_delay/1e-9+result->selb_delay/1e-9);
     }
}

void output_area_components(arearesult,arearesult_subbanked,parameters,NSubbanks)
arearesult_type *arearesult;
area_type *arearesult_subbanked;
parameter_type *parameters;
double *NSubbanks;
{
    printf("\nArea Components:\n\n");
/*
    printf("Aspect Ratio Data height/width: %f\n", aspect_ratio_data);
    printf("Aspect Ratio Tag height/width: %f\n", aspect_ratio_tag);
    printf("Aspect Ratio Subbank height/width: %f\n", aspect_ratio_subbank);
    printf("Aspect Ratio Total height/width: %f\n\n", aspect_ratio_total);
*/
    printf("Aspect Ratio Total height/width: %f\n\n", arearesult->aspect_ratio_total);

    printf("Data array (cm^2): %f\n",calculate_area(arearesult->dataarray_area,parameters->fudgefactor)/100000000.0);
    printf("Data predecode (cm^2): %f\n",calculate_area(arearesult->datapredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Data colmux predecode (cm^2): %f\n",calculate_area(arearesult->datacolmuxpredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Data colmux post decode (cm^2): %f\n",calculate_area(arearesult->datacolmuxpostdecode_area,parameters->fudgefactor)/100000000.0);
    printf("Data write signal (cm^2): %f\n",(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports)*calculate_area(arearesult->datawritesig_area,parameters->fudgefactor)/100000000.0);

    printf("\nTag array (cm^2): %f\n",calculate_area(arearesult->tagarray_area,parameters->fudgefactor)/100000000.0);
    printf("Tag predecode (cm^2): %f\n",calculate_area(arearesult->tagpredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag colmux predecode (cm^2): %f\n",calculate_area(arearesult->tagcolmuxpredecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag colmux post decode (cm^2): %f\n",calculate_area(arearesult->tagcolmuxpostdecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag output driver decode (cm^2): %f\n",calculate_area(arearesult->tagoutdrvdecode_area,parameters->fudgefactor)/100000000.0);
    printf("Tag output driver enable signals (cm^2): %f\n",(parameters->num_readwrite_ports+parameters->num_read_ports+parameters->num_write_ports)*calculate_area(arearesult->tagoutdrvsig_area,parameters->fudgefactor)/100000000.0);

   printf("\nPercentage of data ramcells alone of total area: %f %%\n",100*area_all_dataramcells/(arearesult->totalarea/100000000.0));
    printf("Percentage of tag ramcells alone of total area: %f %%\n",100*area_all_tagramcells/(arearesult->totalarea/100000000.0));
    printf("Percentage of total control/routing alone of total area: %f %%\n",100*(arearesult->totalarea/100000000.0-area_all_dataramcells-area_all_tagramcells)/(arearesult->totalarea/100000000.0));
    printf("\nSubbank Efficiency : %f\n",(area_all_dataramcells+area_all_tagramcells)*100/(arearesult->totalarea/100000000.0));
    printf("Total Efficiency : %f\n",(*NSubbanks)*(area_all_dataramcells+area_all_tagramcells)*100/(calculate_area(*arearesult_subbanked,parameters->fudgefactor)/100000000.0));
    printf("\nTotal area One Subbank (cm^2): %f\n",arearesult->totalarea/100000000.0);
    printf("Total area subbanked (cm^2): %f\n",calculate_area(*arearesult_subbanked,parameters->fudgefactor)/100000000.0); 

}


void output_data(result,arearesult,arearesult_subbanked,parameters,NSubbanks)
result_type *result;
arearesult_type *arearesult;
area_type *arearesult_subbanked;
parameter_type *parameters;
double *NSubbanks;
{
   double datapath,tagpath;

   FILE *stream;

   stream=fopen("cache_params.aux", "w");

   datapath = result->subbank_address_routing_delay+result->decoder_delay_data+result->wordline_delay_data+result->bitline_delay_data+result->sense_amp_delay_data+result->total_out_driver_delay_data+result->data_output_delay;
   if (parameters->associativity == 1) {
         tagpath = result->subbank_address_routing_delay+result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_valid_delay;
   } else {
         tagpath = result->subbank_address_routing_delay+result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_mux_delay+result->selb_delay+result->data_output_delay+result->total_out_driver_delay_data;
   }

   if (stream)
     {
       fprintf(stream, "#define PARAMNDWL %d\n#define PARAMNDBL %d\n#define PARAMNSPD %d\n#define PARAMNTWL %d\n#define PARAMNTBL %d\n#define PARAMNTSPD %d\n#define PARAMSENSESCALE %f\n#define PARAMGS %d\n#define PARAMDNOR %d\n#define PARAMTNOR %d\n#define PARAMRPORTS %d\n#define PARAMWPORTS %d\n#define PARAMRWPORTS %d\n#define PARAMMUXOVER %d\n", result->best_Ndwl, result->best_Ndbl, result->best_Nspd, result->best_Ntwl, result->best_Ntbl, result->best_Ntspd, result->senseext_scale, (result->senseext_scale==1.0), result->data_nor_inputs, result->tag_nor_inputs, parameters->num_read_ports, parameters->num_write_ports, parameters->num_readwrite_ports, result->best_muxover);
     }
#  if OUTPUTTYPE == LONG
      printf("\nCache Parameters:\n");
      printf("  Number of Subbanks: %d\n",(int)(*NSubbanks));
      printf("  Total Cache Size: %d\n",(int) (parameters->cache_size*(*NSubbanks)));
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

      output_area_components(arearesult,arearesult_subbanked,parameters,NSubbanks);
      printf("\nTime Components:\n");
     
      printf(" data side (with Output driver) (ns): %g\n",datapath/1e-9);
      if (!parameters->fully_assoc)
        printf(" tag side (with Output driver) (ns): %g\n",(tagpath)/1e-9);
      output_time_components(result,parameters);

#  else
      printf("%d %d %d  %d %d %d %d %d %d  %e %e %e %e  %e %e %e %e  %e %e %e %e  %e %e %e %e  %e %e\n",
               parameters->cache_size,
               parameters->block_size,
               parameters->associativity,
               result->best_Ndwl,
               result->best_Ndbl,
               result->best_Nspd,
               result->best_Ntwl,
               result->best_Ntbl,
               result->best_Ntspd,
               result->access_time,
               result->cycle_time,
               datapath,
               tagpath,
               result->decoder_delay_data,
               result->wordline_delay_data,
               result->bitline_delay_data,
               result->sense_amp_delay_data,
               result->decoder_delay_tag,
               result->wordline_delay_tag,
               result->bitline_delay_tag,
               result->sense_amp_delay_tag,
               result->compare_part_delay,
               result->drive_mux_delay,
               result->selb_delay,
               result->drive_valid_delay,
               result->data_output_delay,
               result->precharge_delay);



# endif

}


