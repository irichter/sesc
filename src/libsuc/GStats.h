/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Smruti Sarangi
		  Luis Ceze

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

#ifndef GSTATSD_H
#define GSTATSD_H

#include <list>
#include <stdarg.h>
#include <vector>

#include "estl.h" // hash_map
#include "callback.h"
#include "nanassert.h"

class GStats {
private:
  typedef std::list < GStats * >Container;
  typedef std::list < GStats * >::iterator ContainerIter;
  static Container *store;
protected:
  char *name;
  char *getText(const char *format,
		va_list ap);
  void subscribe();
  void unsubscribe();
public:
  static void report(const char *str);
  static GStats *getRef(const char *str);

  GStats() {
  }
  virtual ~GStats() {
  }

  virtual void reportValue() const =0;
  virtual double getDouble() const {
    MSG("getDouble Not supported by this class %s",name);
    return 0;
  }

  const char *getName() const { return name; }
  virtual long long getSamples() const { return 1; }
};

class GStatsCntr : public GStats {
private:
  long long data;
protected:
public:
  GStatsCntr(const char *format,...);

  ~GStatsCntr();

  GStatsCntr & operator += (const long v) {
    data += v;
    return *this;
  }

  void add(const long v) {
    data += v;
  }
  // TODO? (Volunteer wanted!)
  // change that for operator++()
  void inc() {
    data++;
  }

  void dec() {
    data--;
  }

  long long getValue() const {
    return data;
  }

  double getDouble() const;
  void reportValue() const;
};

class GStatsAvg : public GStats {
private:
  long long data;
  long long nData;
protected:
public:
  GStatsAvg(const char *format,...);
  ~GStatsAvg();

  void sample(const long v) {
    data += v;
    nData++;
  }

  void msamples(const long long v, long long n) {
    data  += v;
    nData += n;
  }

  double getDouble() const;

  long long getSamples() const {
    return nData;
  }

  void reportValue() const;
};

class GStatsProfiler : public GStats {
private:
protected:

  typedef HASH_MAP<ulong, int> ProfHash;

  ProfHash p;

public:
  GStatsProfiler(const char *format,...);

  ~GStatsProfiler();

  void reportValue() const;

  void sample(ulong key);  
};

class GStatsMax : public GStats {
 private:
  long maxValue;
  long long nData;
 protected:
 public:
  GStatsMax(const char *format,...);
  ~GStatsMax();

  void sample(const long v) {
    maxValue = v > maxValue ? v : maxValue;
    nData++;
  }

  void reportValue() const;
};

#endif   // GSTATSD_H
