#the following line has to be before the include
SIGNATURE=libsescspot
include	../Makefile.defs
##############################################################################
.PHONY: all clean

all	: $(OBJ)/libsuc.a

##############################################################################
#                Objects
##############################################################################
OBJS	:= sescspot.o temperature.o RCutil.o flp.o util.o

##############################################################################
#                             Change Rules                                   # 
##############################################################################
eclean:
	-@rm -f $(OBJ)/libsescspot.a
	-@rm -f $(TESTS) $(patsubst %,$(OBJ)/%.o,$(TESTS))

size:
	wc *.cpp *.h *.l *.y

# DO NOT DELETE
_MAKEFILE_COMMON_SUBSHELL=1
include $(SRC_DIR)/Makefile.common

ifneq ($(MAKECMDGOALS),distclean)
ifneq ($(MAKECMDGOALS),clean)
-include $(DOTDEPEND)
endif
endif
