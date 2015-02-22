RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libengine.so
O_OBJS=machine.o native-wrapper.o methodcalls.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES=$(SO_TARGET)

include $(RGMK)
