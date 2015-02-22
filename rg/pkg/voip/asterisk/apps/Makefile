RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libapps.a
O_OBJS=app_dial.o app_openrg_cmd.o app_transfer.o app_voicemail.o
AST_MODULES_NO_RELOAD+=app_dial app_openrg_cmd app_transfer
AST_MODULES_WITH_RELOAD+=app_voicemail

ifdef CONFIG_RG_PBX
  O_OBJS+=app_record.o app_playback.o app_macro.o \
    app_queue.o app_parkandannounce.o
  AST_MODULES_NO_RELOAD+=app_record app_playback app_macro app_parkandannounce
  AST_MODULES_WITH_RELOAD+=app_queue
endif

include $(RGSRC)/pkg/voip/asterisk/modules.mak

SO_RAMDISK_SUBDIR=asterisk
RAMDISK_LIB_FILES+=$(addprefix $(SO_RAMDISK_SUBDIR)/,$(notdir $(SO_TARGET)))

COMMAND_FILE=variable.mak
FOR_EACH=$(SO_TARGET)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(INDEX:%.so=%.o)
include $(LOOP)

include $(RGMK)
