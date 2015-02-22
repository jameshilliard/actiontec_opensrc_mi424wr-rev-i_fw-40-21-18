RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libcodecs.a
O_OBJS=codec_ulaw.o
AST_MODULES_WITH_RELOAD+=codec_ulaw

include $(RGSRC)/pkg/voip/asterisk/modules.mak

SO_RAMDISK_SUBDIR=asterisk/

include $(RGMK)
