#the following line has to be before the include
SIGNATURE=libdsm
include	../Makefile.defs
##############################################################################
.PHONY: all clean

default : all

all: $(OBJ)/dsm.o $(OBJ)/libdsm.a

##############################################################################
#                Objects
##############################################################################
OBJS	:= DSMCache.o DMemorySystem.o DSMProtocol.o

##############################################################################
#                             Change Rules                                   # 
##############################################################################
# eclean is called by clean
eclean:
	-@rm -f $(OBJ)/libdsm.a
	-@rm -f $(OBJ)/dsm.o

size:
	-@wc *.cpp *.h

# DO NOT DELETE
_MAKEFILE_COMMON_SUBSHELL=1
include $(SRC_DIR)/Makefile.common

ifneq ($(MAKECMDGOALS),distclean)
ifneq ($(MAKECMDGOALS),clean)
-include $(DOTDEPEND)
endif
endif
