RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libres.a
O_OBJS=res_features.o res_adsi.o res_monitor.o
AST_MODULES_NO_RELOAD=res_monitor
AST_MODULES_WITH_RELOAD=res_features res_adsi

ifdef CONFIG_RG_PBX
  O_OBJS+=res_musiconhold.o
  AST_MODULES_WITH_RELOAD+=res_musiconhold
endif

include $(RGSRC)/pkg/voip/asterisk/modules.mak

RAMDISK_LIB_FILES+=$(addprefix $(SO_RAMDISK_SUBDIR)/,$(notdir $(SO_TARGET)))

COMMAND_FILE=variable.mak
FOR_EACH=$(SO_TARGET)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(INDEX:%.so=%.o)
include $(LOOP)

include $(RGMK)
