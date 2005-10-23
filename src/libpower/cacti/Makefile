#the following line has to be before the include
SIGNATURE=libcacti
include	../../Makefile.defs
##############################################################################
.PHONY: all clean

all	: $(DOTDEPEND) $(OBJ)/libcacti.a

##############################################################################
#                Objects
##############################################################################
OBJS	:= xcacti_area.o xcacti_io.o cacti_setup.o xcacti_time.o

##############################################################################
#                             Change Rules                                   # 
##############################################################################
# eclean is called by clean
eclean:
	-@rm -f $(OBJ)/libcacti.a
	-@rm -f $(patsubst %.o,$(OBJ)/%.o,$(OBJS))

_MAKEFILE_COMMON_SUBSHELL=1
include $(SRC_DIR)/Makefile.common
ifneq ($(MAKECMDGOALS),distclean)
ifneq ($(MAKECMDGOALS),clean)
-include $(DOTDEPEND) 
endif
endif

# DO NOT DELETE
