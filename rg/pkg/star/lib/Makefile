RGSRC=../../..
include $(RGSRC)/envir.mak

SUBDIRS=stdio

include $(RGSRC)/pkg/star/rules.mk

include $(RGSRC)/pkg/star/lib/Targets

A_TARGET=libshlschily.a
O_OBJS=$(CFILES:%.c=%.o)
CD_EXPORTED_FILES=Targets $(wildcard stdio/*.c) stdio/io.h strtod.c cvt.c

include $(RGMK)

