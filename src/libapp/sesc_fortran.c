
#if 0 /* not tested code */
#ifndef DARWIN
#include <g2c.h>

#include "sescapi.h"

integer sesc_f77_vfork(void)
{
  return ((integer) sesc_spawn(0, 0, 0));
}

void sesc_f77_exit(integer *err){
  sesc_exit((int) *err);
}
#endif
#endif
