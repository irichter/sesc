
#include <stdlib.h>
#include <unistd.h>

#include "SescConf.h"
#include "ReportGen.h"

#include "wattch/wattch_setup.h"
#include "cacti/cacti_setup.h"
#include "orion/orion_setup.h"

int main(int argc, char **argv)
{
  // Note: This is not integrated with powermain.cpp in a single pass because
  // cacti and wattch have same function names. Therefore, they do not link
  // together.
  if (argc<3) {
    fprintf(stderr,"Usage:\n\t%s <tmp.conf> <power.conf>\n",argv[0]);
    exit(-1);
  }

  unlink(argv[2]);
  Report::openFile(argv[2]);

  SescConf = new SConfig(argv[1]);

  cacti_setup();
  printf("CACTI Setup done\n");

  // dump the cactify configuration
  SescConf->dump(true);

  Report::close();
}