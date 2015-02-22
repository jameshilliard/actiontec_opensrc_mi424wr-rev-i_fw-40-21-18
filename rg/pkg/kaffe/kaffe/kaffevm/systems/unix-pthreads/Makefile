RGSRC=../../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../../envir.mak

SO_TARGET=libkthread.so
O_OBJS=lock-impl.o signal.o thread-impl.o syscalls.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES=$(SO_TARGET)

include $(RGMK)
