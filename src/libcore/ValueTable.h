/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Luis Ceze
                  Liu Wei

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef VALUETABLE_H
#define VALUETABLE_H

#include "estl.h"
#include "nanassert.h"
#include "GStats.h"

class ValueTable {
 private:
  static GStatsCntr *putCntr;
  static GStatsCntr *getCntr;

  static GStatsCntr *goodCntr;
  static GStatsCntr *badCntr;

  static GStatsCntr *verifyGoodCntr;
  static GStatsCntr *verifyBadCntr;

  //TODO: move it to configuration file
  class PredEntry {
  protected:
    const int id;

    GStatsCntr goodCntr;
    GStatsCntr badCntr;

    // Use a hash_map because the compiler does not provide the
    // maximum prediction ID generated.
    typedef HASH_MAP<int, PredEntry*> VTable;
    static VTable table;

    PredEntry(int i);

  public:
    static const int MaxValue = (1<<2)-1;
    int curr;
    int pred;
    int stride;
    int confidence;

    static PredEntry *getEntry(int i);

    void good() {
      goodCntr.inc();
      if (confidence < MaxValue) {
	confidence++;
      }
    }
    void bad() {
      badCntr.inc();
      if (confidence > 0)
	confidence--;
    }
    bool isBad() const {
      return confidence < MaxValue;
    }
  };

  ValueTable();
 public:
  static void boot();

  /* Last Value Predictor:
   *   1 entry used: curr, confidence
   */
  static int readLVPredictor(int id);
  static void updateLVPredictor(int id, int value);

  /* Stride Value Predictor:
   *   3 entries used: curr, stride, confidence
   */
  static int readSVPredictor(int id);
  static void updateSVPredictor(int id, int value);

  /* Increment Predictor:
   *   2 entries used: stride, confidence
   */
  static int readIncrPredictor(int id, int lvalue);
  static void updateIncrPredictor(int id, int value);

  static void verifyValue(int rval, int pval);
};

#endif
