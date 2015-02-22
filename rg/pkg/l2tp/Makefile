RGSRC=../../
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=l2tp
EXPORT_HEADERS+=rg_ipc.h l2tp.h

VERSION=0.3

LIB_EVENT=libevent
SUBDIRS=$(LIB_EVENT)

TARGET=l2tpd
RAMDISK_BIN_FILES+=$(TARGET)
O_OBJS_l2tpd=auth.o debug.o dgram.o main.o md5.o network.o options.o peer.o session.o tunnel.o utils.o rg.o 

CFLAGS_l2tpd=-DVERSION=\"$(VERSION)\"
LDFLAGS_l2tpd=-L$(LIB_EVENT) 
LDLIBS_l2tpd:=-levent -lutil $(OPENRG_GPL_LDLIBS) $(LDLIBS_l2tpd)

INTERNAL_HEADERS+=md5.h

include $(RGMK)

