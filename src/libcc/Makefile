#the following line has to be before the include
SIGNATURE=libcc
include	../Makefile.defs
##############################################################################
.PHONY: all clean

default : all

all: $(OBJ)/nkcc1.o $(OBJ)/libcc.a

##############################################################################
#                Objects
##############################################################################
OBJS	:= MPCache.o GDebugCC.o CacheCoherence.o SlaveMPCoh.o  \
	   MasterMPCoh.o MPCacheCoherence.o CMemorySystem.o
##############################################################################
#                             Change Rules                                   # 
##############################################################################

# eclean is called by clean
eclean:
	-@rm -f $(OBJ)/libcc.a
	-@rm -f $(OBJ)/nkcc1.o

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
