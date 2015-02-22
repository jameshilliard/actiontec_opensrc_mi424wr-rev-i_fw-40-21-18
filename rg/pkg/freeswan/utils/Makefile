RGSRC=../../../
include $(RGSRC)/envir.mak

GMPSRC=$(RGSRC)/pkg/gmp

TARGET=ranbits rsasigkey
RAMDISK_BIN_FILES+=$(TARGET)

CFLAGS+=-I../lib
LDFLAGS+=-L../lib
LDLIBS:=-lfreeswan $(LDLIBS)

O_OBJS_ranbits=ranbits.o
O_OBJS_rsasigkey=rsasigkey.o
CFLAGS+=-I$(GMPSRC)
LDFLAGS_rsasigkey=$(LDFLAGS) -L$(GMPSRC)
LDLIBS_rsasigkey:=-lgmp $(OPENRG_GPL_LDLIBS) $(LDLIBS_rsasigkey)

include $(RGMK)
