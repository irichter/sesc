
#include <stdlib.h>
#include <unistd.h>

#include "SescConf.h"
#include "ReportGen.h"

#include "wattch/misc.h"
#include "wattch/wattch_setup.h"
#include "orion/orion_setup.h"

int main(int argc, char **argv)
{
  if (argc<3) {
    fprintf(stderr,"Usage:\n\t%s [-v] <sesc.conf> <tmp.conf>\n",argv[0]);
    exit(-1);
  }

  if (argv[1][0] == '-')
    verbose = 1;
  else
    verbose = 0;
  
  unlink(argv[2+verbose]);
  Report::openFile(argv[2+verbose]);

  SescConf = new SConfig(argv[1+verbose]);

  wattch_setup();
  orion_setup(SescConf);

  // dump the wattchified configuration
  SescConf->dump(true);

  Report::close();
}
