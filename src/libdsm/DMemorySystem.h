/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Karin Strauss

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

#ifndef DMEMORY_SYSTEM_H
#define DMEMORY_SYSTEM_H

#include "estl.h"
#include "MemorySystem.h"
#include "InterConn.h"
#include "DSMProtocol.h"

// Class for comparison to be used in hashes of char * where the
// content is to be compared
class NetObjComp {
public:
  inline bool operator()(const char* s1, const char* s2) const {
    return (strcasecmp(s1, s2) == 0);
  }
};  

class DMemorySystem : public MemorySystem {
private:
  typedef HASH_MAP<const char *, InterConnection *, HASH<const char*>, 
    NetObjComp> StrToNetObjMapper;
  static StrToNetObjMapper netObjContainer;

protected:

  MemObj *buildMemoryObj(const char *type, 
			 const char *section, 
			 const char *name);  

  void addNetwork(const char *deviceName, InterConnection *net);

  InterConnection *searchNetwork(const char *section, const char *deviceName);
  InterConnection *findCreateNetwork(const char *section, unsigned int i);

  char *privatizeNetName(char *givenName, unsigned int i, unsigned int netId);

  Protocol_t getProtocol(const char *section, unsigned int i);

public:
  DMemorySystem(int processorId);
  virtual ~DMemorySystem() {
  }
};

#endif // DMEMORY_SYSTEM_H

