/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2004 University of Illinois.

   Contributed by Luis Ceze

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

#include "BloomFilter.h"
#include <stdarg.h>
#include <string.h>

BloomFilter::BloomFilter()
{
  I(0);
}

BloomFilter::BloomFilter(int nv, ...)
{
  va_list argL;
  va_start(argL, nv);

  nVectors = nv;
  vSize = new int[nVectors];
  vBits = new int[nVectors];
  vMask = new unsigned[nVectors];
  rShift = new int[nVectors];
  countVec = new int*[nVectors];

  for(int i = 0; i < nVectors; i++) {
    vBits[i] = va_arg(argL, int); // # of bits from the address
    vSize[i] = va_arg(argL, int); // # of entries in the corresponding vector
    countVec[i] = new int[vSize[i]];
  }

  for(int i = 0; i < nVectors; i++) {
    for(int j = 0; j < vSize[i]; j++) {
      countVec[i][j] = 0;
    }
  }

  va_end(argL);

  initMasks();
}

void BloomFilter::initMasks()
{
  // now preparing masks, bit shifts etc...
  int totShift = 0;
  for(int i = 0; i < nVectors; i++) {
    unsigned mask = 0;
    for(int m = 0; m < vBits[i]; m++)
      mask = mask | 1 << m;

    mask = mask << totShift;
    vMask[i] = mask;
    rShift[i] = totShift;
    
    totShift += vBits[i];
  }
}

int BloomFilter::getIndex(unsigned val, int chunkPos)
{
  int idx;
  idx = val & vMask[chunkPos];
  idx = idx >> rShift[chunkPos];

  I(idx < vSize[chunkPos]);
  return idx;
}

void BloomFilter::insert(unsigned e)
{
  for(int i = 0; i < nVectors; i++) {
    int idx = getIndex(e, i);
    countVec[i][idx]++;
  }
}

void BloomFilter::remove(unsigned e)
{
  for(int i = 0; i < nVectors; i++) {
    int idx = getIndex(e, i);
    countVec[i][idx]--;
    I(countVec[i][idx] >= 0);
  }
}

bool BloomFilter::mayExist(unsigned e)
{
  for(int i = 0; i < nVectors; i++) {
    int idx = getIndex(e, i);
    if(countVec[i][idx] == 0)
      return false;
  }
  return true;
}
