RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libformats.a
O_OBJS=format_au.o
AST_MODULES_NO_RELOAD+=format_au

include $(RGSRC)/pkg/voip/asterisk/modules.mak

SO_RAMDISK_SUBDIR=asterisk/

include $(RGMK)
