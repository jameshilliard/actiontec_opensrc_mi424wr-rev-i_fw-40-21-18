RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libpbx.a
O_OBJS=pbx_config.o
AST_MODULES_WITH_RELOAD+=pbx_config

include $(RGSRC)/pkg/voip/asterisk/modules.mak

SO_RAMDISK_SUBDIR=asterisk
RAMDISK_LIB_FILES+=$(addprefix $(SO_RAMDISK_SUBDIR)/,$(notdir $(SO_TARGET)))

include $(RGMK)
