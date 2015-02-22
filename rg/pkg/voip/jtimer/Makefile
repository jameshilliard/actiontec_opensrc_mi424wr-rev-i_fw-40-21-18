RGSRC=../../../
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=jtimer
EXPORT_HEADERS+=jtimer.h

ifndef CONFIG_RG_GPL
MOD_TARGET=jtimer.o
RAMDISK_MODULES_FILES+=$(MOD_TARGET)
O_OBJS_jtimer.o=jtimer.o
endif

include $(RGMK)
