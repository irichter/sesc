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

#include "DMemorySystem.h"
#include "DSMCache.h"
#include <math.h>

#include "DSMDebug.h" // debugging defines

DMemorySystem::StrToNetObjMapper DMemorySystem::netObjContainer;

DMemorySystem::DMemorySystem(int processorId)
  : MemorySystem(processorId)
{
  // nothing to do
}

MemObj *DMemorySystem::buildMemoryObj(const char *type, 
				      const char *section, 
				      const char *name)
{
  MemObj *obj;

  if (!strcasecmp(type, "dsmcache")) {
    obj = new DSMCache(this, section, name);
    
    // check if there are networks to build
    unsigned int nNetworks  = SescConf->getRecordSize(section, "network");
    
    GLOG(DSMDBG_CONSTR, "DMemorySystem: networks = %d", nNetworks);
    
    for(unsigned int i = 0; i < nNetworks; i++) {
      InterConnection *net = findCreateNetwork(section, i);
      Protocol_t protocol  = getProtocol(section, i);

      if(protocol < 0 || !net) {
	return NULL; // Known error mode
      }

      unsigned int routerNumber = net->getNextFreeRouter(section);

      if (routerNumber >= net->getMaxRouters()) {
	MSG("You are trying to add more that one object of the same"\
	    "section to the same router. This is not permitted in the"\
	    "current infrastructure. Please change your configuration"\
	    "file or write this support in the simulator");
	abort();
      }

      PortID_t portNumber = net->getPort(section);

      // attach cache to network through protocol
      // ugly cast
      static_cast<DSMCache *>(obj)->attachNetwork(protocol, 
						  net, 
						  routerNumber, 
						  portNumber);
      GLOG(DSMDBG_CONSTR, 
	   "Cache %s connected to router %d, port %d on network %d", 
	   section, routerNumber, portNumber, i);
    }
 
  } else {
    obj = MemorySystem::buildMemoryObj(type, section, name);
  }

  return obj;
}

InterConnection *DMemorySystem::findCreateNetwork(const char *section, 
						  unsigned int i)
{
  // retrieving network section
  const char *netSection = SescConf->getCharPtr(section, "network", i);
  
  if(!netSection) {
    return NULL; // Known error mode
  } 
  
  GLOG(DSMDBG_CONSTR, "DMemorySystem: reading section %s", netSection);
  
  // find out how many of these objects connect 
  // to that interconnection point
  char fieldName[128];
  
  sprintf(fieldName, "%sConn", section);
  SescConf->isLong(netSection, fieldName);
  SescConf->isGT(netSection, fieldName, 0);

  long numConn = SescConf->getLong(netSection, fieldName);
  GLOG(DSMDBG_CONSTR, "DMemorySystem: %s = %ld", fieldName, numConn);
  
  char *deviceName = (char *)malloc(strlen(netSection+1));
  strcpy(deviceName, netSection);

  unsigned int netId = Id / (numConn * procsPerNode);
  deviceName = privatizeNetName(deviceName, i, netId);

  GLOG(DSMDBG_CONSTR, "DMemorySystem: privatized name: %s", deviceName);

  // build or find network
  InterConnection *net = searchNetwork(netSection, deviceName);
  
  if (!net) {
    net = new InterConnection(netSection);
    addNetwork(deviceName, net);
    StrToNetObjMapper::iterator it = netObjContainer.find(deviceName);
    if(it != netObjContainer.end())
      GLOG(DSMDBG_CONSTR, "addition confirmed");
  }

  return net;
}

void DMemorySystem::addNetwork(const char *deviceName, InterConnection *net)
{
  GLOG(DSMDBG_CONSTR, "adding %s to network list", deviceName);
  netObjContainer[deviceName] = net;
}

InterConnection *DMemorySystem::searchNetwork(const char *section,
					      const char *deviceName)
{
  I(section);
  I(deviceName);

  if (netObjContainer.count(deviceName) != 1) {
    GLOG(DSMDBG_CONSTR, 
	 "DMemorySystem: didn't find network in list; creating new network");
    return NULL; // not found
  }

  GLOG(DSMDBG_CONSTR, "descrSection for %s being searched: %s", 
       deviceName, section);

  StrToNetObjMapper::iterator it = netObjContainer.find(deviceName);
    
  if (it != netObjContainer.end()) {
    const char *descrSection = (*it).second->getDescrSection();

    GLOG(DSMDBG_CONSTR, "descrSection for %s already in structure: %s", 
	deviceName, 
	descrSection);

    if (strcasecmp(descrSection, section)) {
      MSG("Two versions of Network Object [%s], definitions [%s] and [%s]", 
	  deviceName, descrSection, section);
      abort();
    }

    GLOG(DSMDBG_CONSTR, "DMemorySystem: found network in list");
    return (*it).second;
  }

  return NULL;
}

char *DMemorySystem::privatizeNetName(char *givenName, 
				      unsigned int i, 
				      unsigned int netId)
{
  char *ret = (char *) malloc(strlen(givenName) + 8 + 
			      (int)log10((float)i + 10) +
			      (int)log10((float)netId + 10));

  sprintf(ret, "%i-%s(%i)", i, givenName, netId);

  free(givenName);

  return ret;
}


Protocol_t DMemorySystem::getProtocol(const char *section, unsigned int i)
{
  // retrieving network section
  const char *netSection = SescConf->getCharPtr(section, "network", i);

  if(!netSection) {
    return (Protocol_t) -1; // Known error mode
  } 

  const char *protocol = SescConf->getCharPtr(netSection, "protocol");
  if (!strcasecmp(protocol, "control"))
    return DSMControl;
  else if (!strcasecmp(protocol, "data"))
    return DSMData;
  else return (Protocol_t) -1;
}

