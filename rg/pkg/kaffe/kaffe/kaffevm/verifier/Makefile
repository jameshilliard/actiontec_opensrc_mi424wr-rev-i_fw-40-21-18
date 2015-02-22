RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libkaffeverifier.so
O_OBJS=verify.o verify2.o verify3a.o \
	verify3b.o verify-block.o verify-debug.o verify-sigstack.o \
	verify-type.o verify-uninit.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES+=$(SO_TARGET)

include $(RGMK)
