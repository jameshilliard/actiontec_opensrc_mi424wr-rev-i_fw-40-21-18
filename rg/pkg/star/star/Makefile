RGSRC=../../..
include $(RGSRC)/envir.mak

include $(RGSRC)/pkg/star/rules.mk

TARGET=star
RAMDISK_BIN_FILES+=$(TARGET)
O_OBJS=star.o header.o xheader.o \
		list.o extract.o create.o append.o diff.o \
		remove.o star_unix.o acl_unix.o fflags.o \
		buffer.o dirtime.o lhash.o \
		hole.o longnames.o names.o remote.o \
		movearch.o table.o props.o \
		unicode.o \
		fifo.o device.o

LDFLAGS+=-L../lib -lshlschily

include $(RGMK)
