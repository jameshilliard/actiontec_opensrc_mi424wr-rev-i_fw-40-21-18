RGSRC=../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../envir.mak

SO_TARGET=libreplace.so
O_OBJS=dummyin6.o getaddrinfo.o getifaddrs.o \
       inet_pton.o inet_ntop.o memset.o semaphores.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES+=$(SO_TARGET)

include $(RGMK)
