
#include "GLVID.h"
#include "TaskContext.h"


GLVIDDummy::GLVIDDummy() 
{
}

SubLVIDType GLVIDDummy::getSubLVID() const 
{
  return 0;
}

void GLVIDDummy::garbageCollect() 
{
  TaskContext::tryPropagateSafeToken();
}

bool GLVIDDummy::isKilled() const
{ 
  return false; 
}
