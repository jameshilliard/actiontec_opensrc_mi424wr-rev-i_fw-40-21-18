RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libnet.so
O_OBJS=InetAddressImpl.o \
	NetworkInterface.o \
	PlainDatagramSocketImpl.o \
	PlainSocketImpl.o

RAMDISK_LIB_FILES+=libnet.so

include $(RGMK)
