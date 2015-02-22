
RGSRC=../../
include $(RGSRC)/envir.mak

A_TARGET=libmtd.a

O_OBJS+=libcrc32.o libmtd.o libmtd_legacy.o libfec.o

CFLAGS+=-I$(BUILDDIR)/pkg/mtd-utils/include -I$(BUILDDIR)/pkg/mtd-utils/lib

include $(RGMK)

