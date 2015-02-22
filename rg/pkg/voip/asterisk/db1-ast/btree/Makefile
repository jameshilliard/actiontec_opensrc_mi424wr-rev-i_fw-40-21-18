RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/db1-ast/common.mak

O_TARGET=btree_target.o
O_OBJS=bt_close.o bt_conv.o bt_debug.o bt_delete.o bt_get.o bt_open.o \
  bt_overflow.o bt_page.o bt_put.o bt_search.o bt_seq.o bt_split.o bt_utils.o

CFLAGS+=-I$(RGSRC)/pkg/voip/asterisk/db1-ast/include

include $(RGMK)
