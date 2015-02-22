RGSRC=../../
include $(RGSRC)/envir.mak
TARGET+=ping6

INTERNAL_HEADERS+=ping_common.h SNAPSHOT.h 

CFLAGS+=-I. -D_GNU_SOURCE
LDFLAGS +=-lresolv

O_OBJS_ping6+=ping6.o ping_common.o



RAMDISK_BIN_FILES+=$(TARGET)
include $(RGMK)

