RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

CFLAGS+= \
	-I$(RGSRC)/pkg/zlib

SO_TARGET=libzip.so
O_OBJS=Adler32.o \
       CRC32.o \
       Deflater.o \
       Inflater.o

RAMDISK_LIB_FILES+=libzip.so

include $(RGMK)
