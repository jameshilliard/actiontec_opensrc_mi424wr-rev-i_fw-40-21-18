
RGSRC=../../
include $(RGSRC)/envir.mak

SUBDIRS=lib

EXPORT_HEADERS_DIR=mtd-utils
EXPORT_HEADERS+=include/common.h include/crc32.h include/libmtd.h include/xalloc.h
EXPORT_HEADERS+=lib/libmtd_int.h

TARGET=nandwrite flash_erase
#RAMDISK_BIN_FILES+=$(TARGET)

O_OBJS_nandwrite=nandwrite.o
L_OBJS_nandwrite=$(BUILDDIR)/pkg/mtd-utils/lib/libmtd.a

O_OBJS_flash_erase=flash_erase.o
L_OBJS_flash_erase=$(BUILDDIR)/pkg/mtd-utils/lib/libmtd.a

CFLAGS+=-I$(BUILDDIR)/pkg/mtd-utils/include -I$(BUILDDIR)/pkg/mtd-utils/lib

include $(RGMK)

