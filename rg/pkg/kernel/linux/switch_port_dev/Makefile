RGSRC=../../../../
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=kernel/linux/switch_port_dev
EXPORT_HEADERS=switch_port_dev.h
CD_EXPORTED_FILES+=port_dev_glue.c

MOD_TARGET=switch_port_dev_glue.o
ifdef CONFIG_RG_KOS
ifndef CONFIG_RG_GPL
  MOD_TARGET+=switch_port_dev.o
  OX_OBJS_switch_port_dev.o=port_dev.o
endif
endif
MOD_2_STAT=switch_port_dev_glue.o
RAMDISK_MODULES_FILES=$(MOD_TARGET)
OX_OBJS_switch_port_dev_glue.o=port_dev_glue.o

include $(RGMK)
