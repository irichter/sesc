#include <stdio.h>
#include <stdlib.h>

#include "sescapi.h"

static void notifyEvent(const char *ev, long vaddr, long type, const void *sptr)
{
#ifdef DEBUG
  fprintf(stderr, "%s(0x%x,%d,0x%x) invoked\n", ev, (unsigned)vaddr, (int)type,
	  (unsigned)sptr);
#endif
}

/***********************************/
void sesc_preevent_(long vaddr, long type, void *sptr)
{
  sesc_preevent(vaddr, type, sptr);
}

void sesc_preevent(long vaddr, long type, void *sptr)
{
  notifyEvent("sesc_preevent", vaddr, type, sptr);
}

/***********************************/
void sesc_postevent_(long vaddr, long type, const void *sptr)
{
  sesc_postevent(vaddr, type, sptr);
}

void sesc_postevent(long vaddr, long type, const void *sptr)
{
  notifyEvent("sesc_postevent", vaddr, type, sptr);
}

/***********************************/
void sesc_memfence_(long vaddr)
{
    sesc_memfence(vaddr);
}

void sesc_memfence(long vaddr)
{
  notifyEvent("sesc_memfence", vaddr, 0, 0);
}

void sesc_acquire_(long vaddr)
{
  sesc_acquire(vaddr);
}

void sesc_acquire(long vaddr)
{
  notifyEvent("sesc_acquire", vaddr, 0, 0);
}

void sesc_release_(long vaddr)
{
    sesc_release(vaddr);
}

void sesc_release(long vaddr)
{
  notifyEvent("sesc_release", vaddr, 0, 0);
}

/***********************************/

void sesc_simulation_mark_()
{
  sesc_simulation_mark();
}

void sesc_simulation_mark()
{
  static int mark=0;
  fprintf(stderr,"sesc_simulation_mark %d (native)", mark);
  mark++;
}

void sesc_simulation_mark_id_(int id)
{
  sesc_simulation_mark_id(id);
}

void sesc_simulation_mark_id(int id)
{
  static int marks[256];
  static int first=1;
  int i;

  if(first) {
    for(i=0;i<256;i++)
      marks[i]=0;
    first=0;
  }

  fprintf(stderr,"sesc_simulation_mark(%d) %d (native)", id, marks[id%256]);
  marks[id%256]++;
}

void sesc_finish()
{
  fprintf(stderr,"sesc_finish called (end of simulation in native)");
  exit(0);
}

void sesc_fast_sim_begin_()
{
  sesc_fast_sim_begin();
}

void sesc_fast_sim_begin()
{
  notifyEvent("sesc_fast_sim_begin", 0, 0, 0);
}

void sesc_fast_sim_end_()
{
  sesc_fast_sim_end();
}

void sesc_fast_sim_end()
{
  notifyEvent("sesc_fast_sim_end", 0, 0, 0);
}

void sesc_sysconf(int tid, long flags)
{
  notifyEvent("sesc_sysconf", 0, 0, 0);
}

/*************************************/

#ifdef VALUEPRED
int  sesc_get_last_value(int id)
{
  return 0;
}

void sesc_put_last_value(int id, int val)
{
}

int  sesc_get_stride_value(int id)
{
  return 0;
}

void sesc_put_stride_value(int id, int val)
{
}

int  sesc_get_incr_value(int id, int lval)
{
  return 0;
}

void sesc_put_incr_value(int id, int incr)
{
}

void sesc_verify_value(int rval, int pval)
{
}
#endif

#ifdef VALUEPROF
void sesc_delinquent_load_begin (int id)
{

}

void sesc_delinquent_load_end ()
{

}
#endif
