#ifndef DARWIN

#include "sescapi.h"
#include <g2c.h>

integer sesc_f77_vfork(void)
{ 
  return ((integer) sesc_spawn(0, 0, 0));
}

void sesc_f77_exit(integer *err){
  sesc_exit((int) *err);
}

void sesc_f77_simulation_mark_id(integer *id) {
  sesc_simulation_mark_id((int) *id);
} 

void sesc_f77_simulation_mark_id_(integer *id) {
  sesc_simulation_mark_id((int) *id);
} 

#endif
