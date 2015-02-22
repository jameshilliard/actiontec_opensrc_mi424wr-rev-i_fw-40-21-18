RGSRC=../../../../
include $(RGSRC)/envir.mak

LOCAL_CXX_TARGET=mkcramfs
O_OBJS=mkcramfs.o

LOCAL_CFLAGS=-Wall -O2 -I../../include -I$(RGSRC)/pkg/lzma/SRC
LOCAL_LDLIBS:=-llocal_z -llocal_lzma $(LOCAL_LDLIBS)

include $(RGMK)
