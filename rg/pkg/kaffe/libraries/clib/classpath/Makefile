RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

include $(RGSRC)/pkg/kaffe/envir.mak

SO_TARGET=libclasspath.so
O_OBJS=jcl.o jnilink.o native_state.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES+=$(SO_TARGET)

include $(RGMK)
