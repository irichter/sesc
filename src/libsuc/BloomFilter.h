/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

   Contributed by Luis Ceze
                  Jose Renau

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.21

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include "nanassert.h"

class BloomFilter {
 private:
  int *vSize;
  int *vBits;
  unsigned *vMask;
  int *rShift;
  int **countVec;
  int nVectors;

  void initMasks();
  int getIndex(unsigned val, int chunkPos);
  
  BloomFilter() {
  };
 public:
  ~BloomFilter() { }
  BloomFilter(int nv, ...);

  void insert(unsigned e);
  void remove(unsigned e);
  bool mayExist(unsigned e);
};

#endif //BLOOMFILTER_H
