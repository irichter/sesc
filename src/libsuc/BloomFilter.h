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
#include "GStats.h"

class BloomFilter {
 private:
  int *vSize;
  int *vBits;
  unsigned *vMask;
  int *rShift;
  int **countVec;
  int nVectors;
  int *nonZeroCount;
  char *desc;
  int nElements;

  bool BFBuild;

  GStatsTimingHist **timingHistVec;
  void updateHistogram(int vec);

  void initMasks();
  int getIndex(unsigned val, int chunkPos);
  
 public:
  ~BloomFilter();

  //the chunk parameters are from the least significant to 
  //the most significant portion of the address
  BloomFilter(int nv, ...);
  BloomFilter(): BFBuild(false) {}

  BloomFilter(const BloomFilter& bf);

  BloomFilter& operator=(const BloomFilter &bf);

  void init(bool build, int nv, ...);

  void insert(unsigned e);
  void remove(unsigned e);

  void clear();

  bool mayExist(unsigned e);
  bool mayIntersect(BloomFilter &otherbf);

  void intersectionWith(BloomFilter &otherbf, BloomFilter &inter);

  void mergeWith(BloomFilter &otherbf);
  void subtract(BloomFilter &otherbf);

  bool isSubsetOf(BloomFilter &otherbf);

  int countAlias(unsigned e);

  void initHistogram(char *name);

  void dump(const char *msg);
  const char *getDesc() { return desc; }

  int size() {  //# of elements encoded
    return nElements;
  }

  int getSize(); // size of the vectors in bits
  int getSizeRLE(int base = 0, int runBits = 7);


  FILE *dumpPtr;
  static int  numDumps;
  void begin_dump_pychart(const char *bname = "bf");
  void end_dump_pychart();
  void add_dump_line(unsigned e);
};

class BitSelection {
 private:
  int nBits;
  int bits[32];
  unsigned mask[32];

 public:

  BitSelection() {
    nBits = 0;
    for(int i = 0; i < 32; i++) {
      bits[i] = 0;
    }
  }

  BitSelection(int *bitPos, int n) {
    nBits = 0;
    for(int i = 0; i < n; i++) 
      addBit(bitPos[i]);
  }

  ~BitSelection() {}

  int getNBits() { return nBits; }

  void addBit(int b) {
    bits[nBits] = b;
    mask[nBits] = 1 << b;
    nBits++;
  }

  unsigned permute(unsigned val) {
    unsigned res = 0;
    for(int i = 0; i < nBits; i++) {
      unsigned bit = (val & mask[i]) ? 1 : 0;
      res = res | (bit << i);
    }
    return res;
  }
  
  void dump(const char *msg) {
    printf("%s:", msg);
    for(int i = 0; i < nBits; i++) {
      printf(" %d", bits[i]);
    }
    printf("\n");
  }
};


#endif //BLOOMFILTER_H
