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

#include "SMPMemRequest.h"
#include "MESICacheState.h"

pool<SMPMemRequest> SMPMemRequest::rPool(256);

SMPMemRequest::SMPMemRequest()
  : MemRequest()
{
}

SMPMemRequest *SMPMemRequest::create(MemRequest *mreq, 
				     MemObj *reqCache,
				     bool sendData)
{
  SMPMemRequest *sreq = rPool.out();

  I(mreq);
  sreq->oreq = mreq;

  sreq->pAddr = mreq->getPAddr();
  sreq->memOp = mreq->getMemOperation();

  sreq->state = MESI_INVALID;

  I(reqCache);
  sreq->requestor = reqCache;
  sreq->currentMemObj = reqCache;

  sreq->needData = sendData;

  sreq->newReq = true;

  return sreq;
}

void SMPMemRequest::destroy()
{
  rPool.in(this);
}

VAddr SMPMemRequest::getVaddr() const
{
  return oreq->getVaddr();
}

PAddr SMPMemRequest::getPAddr() const
{
  return pAddr;
}

void SMPMemRequest::ack(TimeDelta_t lat)
{
  I(0);
}

void SMPMemRequest::setState(unsigned int st)
{
  state = st;
}

MemRequest *SMPMemRequest::getOriginalRequest()
{
  return oreq;
}

MemOperation SMPMemRequest::getMemOperation()
{
  return oreq->getMemOperation();
}

unsigned int SMPMemRequest::getState()
{
  return state;
}

MemObj *SMPMemRequest::getRequestor()
{
  return requestor;
}

bool SMPMemRequest::needsData() 
{
  return needData;
}
