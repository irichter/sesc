#ifndef FBLOCKS_H
#define FBLOCKS_H

#include <iostream>
#include <vector>

using namespace std;

class Grids;

class FBlocks {

friend class Grids;
public:

	//Constructor
	FBlocks (double xcord1, double ycord1, double xcord2, double ycord2, double pow, int n);

	//Inspectors
	double power() const;
	double area() const;
	double temp() const;
	
	double x1 () const;
	double y1 () const;
	double x2 () const;
	double y2 () const;

	double gx1 () const;
	double gy1 () const;
	double gx2 () const;
	double gy2 () const;

	int index() const;

	void ListGrids() const;

	bool IsGridPresent(int n) const;

	//Mutators
	void power(double pow);
	void temp(double temp_);
	void x1 (double xcord1);
	void y1 (double ycord1);
	void x2 (double xcord2);
	void y2 (double ycord2);

	void add_index (int n);

	void updateGridcoord (int row, int column, double chip_length, double chip_height);

	void AddGrid(int n, double gridarea);

	void PartPower(int col, vector<Grids> &G);

	double CalculateTemp (vector<double> &grid_t, int offset, int mode, int g_col);

	void CalculateGridTemp(vector<Grids> &G);

	void ResetTemp();

	void SetGridUpdate(bool state);

//private:

	int index_;

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

	double area_;
	double garea_;

	struct GridDetails {
		
		int number;
		double area;		
	};

	vector<GridDetails> grids;

	bool GridsUpdated;

};

#endif

