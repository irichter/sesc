#include "flp.h"
#include "util.h"

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

void print_flp_fig (flp_t *flp)
{
	int i;
	unit_t *unit;

	fprintf(stderr, "FIG starts\n");
	for (i=0; i< flp->n_units; i++) {
		unit = &flp->units[i];
		fprintf(stderr, "%.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f\n", unit->leftx, unit->bottomy, \
				unit->leftx, unit->bottomy + unit->height, unit->leftx + unit->width, unit->bottomy + \
				unit->height, unit->leftx + unit->width, unit->bottomy, unit->leftx, unit->bottomy);
		fprintf(stderr, "%s\n", unit->name);
	}
	fprintf(stderr, "FIG ends\n");
}


flp_t *read_flp(char *file)
{
	char str[STR_SIZE], copy[STR_SIZE];
	FILE *fp = fopen(file, "r");
	flp_t *flp;
	int i=0, count = 0;
	char *ptr;

	if (!fp) {
		sprintf(str, "error opening file %s\n", file);
		fatal(str);
	}

	while(!feof(fp))		/* first pass	*/
	{
		fgets(str, STR_SIZE, fp);
		if (feof(fp))
			break;
		ptr =	strtok(str, " \t\n");
		if ( ptr && ptr[0] != '#')	/* ignore comments and empty lines	*/
			count++;
	}

	if(!count)
		fatal("no units specified in the floorplan file\n");

	flp = (flp_t *) calloc (1, sizeof(flp_t));
	if(!flp)
		fatal("memory allocation error\n");
	flp->units = (unit_t *) calloc (count, sizeof(unit_t));
	if (!flp->units)
		fatal("memory allocation error\n");
	flp->n_units = count;

	fseek(fp, 0, SEEK_SET);
	while(!feof(fp))		/* second pass	*/
	{
		fgets(str, STR_SIZE, fp);
		if (feof(fp))
			break;
		strcpy(copy, str);
		ptr =	strtok(str, " \t\n");
		if ( ptr && ptr[0] != '#') {	/* ignore comments and empty lines	*/
			if (sscanf(copy, "%s%lf%lf%lf%lf", flp->units[i].name, &flp->units[i].width, \
			&flp->units[i].height, &flp->units[i].leftx, &flp->units[i].bottomy) != 5)
				fatal("invalid floorplan file format\n");
			i++;
		}	
	}

	fclose(fp);	
	print_flp_fig(flp);
	return flp;
}

void free_flp(flp_t *flp)
{
	free(flp->units);
	free(flp);
}

int get_blk_index(flp_t *flp, char *name)
{
	int i;
	char msg[STR_SIZE];

	if (!flp)
		fatal("null pointer in get_blk_index\n");

	for (i = 0; i < flp->n_units; i++) {
		if (!strcasecmp(name, flp->units[i].name)) {
			return i;
		}
	}

	sprintf(msg, "block %s not found\n", name);
	fatal(msg);
	return -1;
}

int is_horiz_adj(flp_t *flp, int i, int j)
{
	double x1, x2, x3, x4;
	double y1, y2, y3, y4;

	if (i == j) 
		return 0;

	x1 = flp->units[i].leftx;
	x2 = x1 + flp->units[i].width;
	x3 = flp->units[j].leftx;
	x4 = x3 + flp->units[j].width;

	y1 = flp->units[i].bottomy;
	y2 = y1 + flp->units[i].height;
	y3 = flp->units[j].bottomy;
	y4 = y3 + flp->units[j].height;

	/* diagonally adjacent => not adjacent */
	if (eq(x2,x3) && eq(y2,y3))
		return 0;
	if (eq(x1,x4) && eq(y1,y4))
		return 0;
	if (eq(x2,x3) && eq(y1,y4))
		return 0;
	if (eq(x1,x4) && eq(y2,y3))
		return 0;

	if (eq(x1,x4) || eq(x2,x3))
		if ((y3 >= y1 && y3 <= y2) || (y4 >= y1 && y4 <= y2) ||
		    (y1 >= y3 && y1 <= y4) || (y2 >= y3 && y2 <= y4))
			return 1;

	return 0;
}

int is_vert_adj (flp_t *flp, int i, int j)
{
	double x1, x2, x3, x4;
	double y1, y2, y3, y4;

	if (i == j)
		return 0;

	x1 = flp->units[i].leftx;
	x2 = x1 + flp->units[i].width;
	x3 = flp->units[j].leftx;
	x4 = x3 + flp->units[j].width;

	y1 = flp->units[i].bottomy;
	y2 = y1 + flp->units[i].height;
	y3 = flp->units[j].bottomy;
	y4 = y3 + flp->units[j].height;

	/* diagonally adjacent => not adjacent */
	if (eq(x2,x3) && eq(y2,y3))
		return 0;
	if (eq(x1,x4) && eq(y1,y4))
		return 0;
	if (eq(x2,x3) && eq(y1,y4))
		return 0;
	if (eq(x1,x4) && eq(y2,y3))
		return 0;

	if (eq(y1,y4) || eq(y2,y3))
		if ((x3 >= x1 && x3 <= x2) || (x4 >= x1 && x4 <= x2) ||
		    (x1 >= x3 && x1 <= x4) || (x2 >= x3 && x2 <= x4))
			return 1;

	return 0;
}

double get_shared_len(flp_t *flp, int i, int j)
{
	double p11, p12, p21, p22;
	p11 = p12 = p21 = p22 = 0.0;

	if (i==j) 
		return 0;

	if (is_horiz_adj(flp, i, j)) {
		p11 = flp->units[i].bottomy;
		p12 = p11 + flp->units[i].height;
		p21 = flp->units[j].bottomy;
		p22 = p21 + flp->units[j].height;
	}

	if (is_vert_adj(flp, i, j)) {
		p11 = flp->units[i].leftx;
		p12 = p11 + flp->units[i].width;
		p21 = flp->units[j].leftx;
		p22 = p21 + flp->units[j].width;
	}

	return (MIN(p12, p22) - MAX(p11, p21));
}

double get_total_width(flp_t *flp)
{	
	int i;
	double min_x = flp->units[0].leftx;
	double max_x = flp->units[0].leftx + flp->units[0].width;
	
	for (i=1; i < flp->n_units; i++) {
		if (flp->units[i].leftx < min_x)
			min_x = flp->units[i].leftx;
		if (flp->units[i].leftx + flp->units[i].width > max_x)
			max_x = flp->units[i].leftx + flp->units[i].width;
	}

	return (max_x - min_x);
}

double get_total_height(flp_t *flp)
{	
	int i;
	double min_y = flp->units[0].bottomy;
	double max_y = flp->units[0].bottomy + flp->units[0].height;
	
	for (i=1; i < flp->n_units; i++) {
		if (flp->units[i].bottomy < min_y)
			min_y = flp->units[i].bottomy;
		if (flp->units[i].bottomy + flp->units[i].height > max_y)
			max_y = flp->units[i].bottomy + flp->units[i].height;
	}

	return (max_y - min_y);
}

