#ifndef __FLP_H_
#define __FLP_H_

#define STR_SIZE	256
#define MAX_UNITS	50

/* functional unit placement definition	*/
typedef struct unit_t_st
{
	char name[STR_SIZE];
	double width;
	double height;
	double leftx;
	double bottomy;
}unit_t;

/* floorplan definition	*/
typedef struct flp_t_st
{
	unit_t *units;
	int n_units;
} flp_t;

/* routines	*/

/* 
 * print the floorplan in a FIG like format 
 * that can be read by tofig.pl to produce 
 * an xfig output 
 */
void print_flp_fig (flp_t *flp);

/* read floorplan file and allocate memory	*/
flp_t *read_flp(char *file);

/* memory uninitialization	*/
void free_flp(flp_t *flp);

/* get unit index from its name	*/
int get_blk_index(flp_t *flp, char *name);

/* are the units horizontally adjacent?	*/
int is_horiz_adj(flp_t *flp, int i, int j);

/* are the units vertically adjacent?	*/
int is_vert_adj (flp_t *flp, int i, int j);

/* shared length between units	*/
double get_shared_len(flp_t *flp, int i, int j);

/* total chip width	*/
double get_total_width(flp_t *flp);

/* total chip height */
double get_total_height(flp_t *flp);

#endif
