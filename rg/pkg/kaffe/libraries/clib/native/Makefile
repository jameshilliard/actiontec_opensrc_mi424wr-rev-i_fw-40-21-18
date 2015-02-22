RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libnative.so
O_OBJS=gnu_classpath_VMStackWalker.o \
	gnu_classpath_VMSystemProperties.o \
	java_lang_Math.o \
	java_lang_VMFloat.o \
	java_lang_VMDouble.o \
	java_lang_VMProcess.o \
	java_lang_VMSystem.o Class.o \
	ClassLoader.o \
	Object.o \
	Runtime.o \
	VMRuntime.o \
	String.o \
	System.o \
	Thread.o \
	Throwable.o \
	ZipFile.o \
	Array.o \
	Constructor.o \
	Field.o \
	Method.o \
	AccessController.o \
	TestNative.o \
	TimeZone.o \
	java_lang_ref_Reference.o

SO_LDFLAGS+=-L$(BUILDDIR)/pkg/lib -lmath -lkaffevm -lfdlibm -lclasspath 

RAMDISK_LIB_FILES=$(SO_TARGET)

include $(RGMK)
