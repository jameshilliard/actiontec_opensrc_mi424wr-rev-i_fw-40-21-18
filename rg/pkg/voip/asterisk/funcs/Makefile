RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libfuncs.a
O_OBJS=pbx_functions.o
AST_MODULES_NO_RELOAD+=pbx_functions

include $(RGSRC)/pkg/voip/asterisk/modules.mak

CFLAGS+=-DBUILTIN_FUNC

SO_RAMDISK_SUBDIR=asterisk
RAMDISK_LIB_FILES+=$(addprefix $(SO_RAMDISK_SUBDIR)/,$(notdir $(SO_TARGET)))

COMMAND_FILE=variable.mak
FOR_EACH=$(SO_TARGET)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(INDEX:%.so=%.o)
include $(LOOP)

include $(RGMK)
