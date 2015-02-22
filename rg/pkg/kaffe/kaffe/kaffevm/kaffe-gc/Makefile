RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libkaffegc.so
O_OBJS=gc-incremental.o gc-mem.o gc-refs.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES=$(SO_TARGET)

include $(RGMK)
