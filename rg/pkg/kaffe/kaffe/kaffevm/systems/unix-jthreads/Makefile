RGSRC=../../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../../envir.mak

SO_TARGET=libkthread.so
O_OBJS=jthread.o syscalls.o signal.o jqueue.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES=$(SO_TARGET)

include $(RGMK)
