RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libjni.so
O_OBJS=jni.o jni-base.o jni-callmethod.o \
     jni-fields.o jni-arrays.o jni-string.o jni-helpers.o \
     jni-refs.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES+=$(SO_TARGET)

include $(RGMK)
