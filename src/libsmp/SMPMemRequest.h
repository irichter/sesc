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

#ifndef SMPMEMREQUEST_H
#define SMPMEMREQUEST_H

#include "pool.h"

#include "MemRequest.h"
#include "MESICacheState.h"

class SMPMemRequest : public MemRequest {
private:

  static pool<SMPMemRequest> rPool;
  friend class pool<SMPMemRequest>;

protected:

  MemRequest *oreq;
  unsigned int state;
  MemObj *requestor;
  bool needData;
  bool newReq;

public:
  SMPMemRequest(); 
  ~SMPMemRequest(){
  }

  // BEGIN: MemRequest interface

  static SMPMemRequest *create(MemRequest *mreq, MemObj *reqCache, 
			       bool sendData);
  void destroy();

  VAddr getVaddr() const;
  PAddr getPAddr() const;
  void  ack(short unsigned int);

  // END: MemRequest interface

  void setOriginalRequest(MemRequest *mreq);
  void setState(unsigned int st);
  void setRequestor(MemObj *reqCache);

  MemRequest  *getOriginalRequest();
  MemOperation getMemOperation();
  unsigned int getState();
  MemObj      *getRequestor();
  bool         needsData();

  bool         isNew() const { return newReq; }
  void         setNotNew() { newReq = false; }
};

#endif // SMPMEMREQUEST_H
