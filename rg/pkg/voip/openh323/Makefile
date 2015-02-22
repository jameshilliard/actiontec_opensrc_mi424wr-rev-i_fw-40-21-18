RGSRC=../../..
include $(RGSRC)/envir.mak

SO_TARGET=libh323_linux_r.so
RAMDISK_LIB_FILES+=$(SO_TARGET)
EXPORT_LIBS+=$(SO_TARGET)

SUBDIRS+=src

L_OBJS_$(SO_TARGET)+=src/target.o src/vic/target.o

include $(RGMK)
