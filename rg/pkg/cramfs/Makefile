RGSRC=../..
include $(RGSRC)/envir.mak

LOCAL_CXX_TARGET=mkcramfs
O_OBJS_mkcramfs=mkcramfs.o

LOCAL_CFLAGS=-Wall -O2 -I$(RGSRC)/pkg/lzma/SRC
ifdef CONFIG_ULIBC
  LOCAL_CFLAGS+=-I$(RG_BUILD)/ulibc
endif
ifdef CONFIG_GLIBC
  LOCAL_CFLAGS+=-I$(RG_BUILD)/glibc
endif
LOCAL_LDLIBS:=-llocal_z -llocal_lzma $(LOCAL_LDLIBS)

include $(RGMK)
