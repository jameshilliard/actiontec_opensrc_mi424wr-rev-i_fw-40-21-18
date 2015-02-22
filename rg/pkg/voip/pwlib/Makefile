RGSRC=../../..
include $(RGSRC)/envir.mak

SO_TARGET=libpt_linux_r.so
RAMDISK_LIB_FILES+=$(SO_TARGET)
EXPORT_LIBS+=$(SO_TARGET)

SUBDIRS+=src

L_OBJS_$(SO_TARGET)+=src/ptclib/target.o src/ptlib/unix/target.o \
  src/ptlib/common/target.o

include $(RGMK)
