#ifndef GRIDS_H
#define GRIDS_H

#include <iostream>
#include <vector>

#include "FBlocks.h"

using namespace std;

class Grids {

public:

	//Constructor
	Grids (int r, int c, int n, double chip_length, double chip_height);

	//Inspectors
	double power() const;

	double temp() const;
	
	double x1 () const;
	double y1 () const;
	double x2 () const;
	double y2 () const;

	double gx1 () const;
	double gy1 () const;
	double gx2 () const;
	double gy2 () const;

	//Mutators
	void power(double pow);

	void temp(double temp_);

	void ResetTemp();
	
	void AddGrid(int n, double gridarea);
	
	double CalculateGridTemp(vector<double> &block_t, int offset);

	double CalculateGridPower(vector<double> &block_pow, int offset, vector<FBlocks> &F);	

//private:

	double x_1;
	double y_1;
	double x_2;
	double y_2;

	double gx_1;
	double gy_1;
	double gx_2;
	double gy_2;

	double power_;

	double temperature;
	
	struct BlkDetails {
		int number;
		double area;
	};

	vector<BlkDetails> blocks;

};

#endif

