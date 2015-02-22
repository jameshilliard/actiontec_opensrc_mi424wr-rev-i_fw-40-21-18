RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../envir.mak

SUBDIRS=jni $(KAFFE_ENGINE) systems $(KAFFE_GC) verifier

SO_TARGET=libkaffevm.so
O_OBJS=access.o \
       baseClasses.o classMethod.o \
       classPool.o methodCache.o \
       code-analyse.o code.o \
       constants.o debug.o \
       exception.o external.o \
       file.o findInJar.o \
       fp.o gc.o \
       hashtab.o inflate.o \
       itypes.o jar.o \
       ksem.o locks.o \
       lookup.o object.o \
       readClass.o soft.o \
       stackTrace.o stats.o \
       string.o stringParsing.o \
       support.o javacall.o \
       thread.o utf8const.o \
       gcFuncs.o reflect.o \
       reference.o \
       md.o

ifneq ($(wildcard $(RGSRC)/pkg/kaffe/config/$(LIBC_ARCH)/linux/md.c),)
md.c:
	$(RG_LN_NOFAIL) $(RGSRC)/pkg/kaffe/config/$(LIBC_ARCH)/linux/$@
endif

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES=$(SO_TARGET)

include $(RGMK)
