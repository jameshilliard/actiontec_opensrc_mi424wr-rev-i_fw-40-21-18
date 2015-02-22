RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

SUBDIRS=btree db hash mpool recno

A_TARGET=libdb1.a
O_OBJS_libdb1.a=btree/btree_target.o db/db_target.o hash/hash_target.o \
  mpool/mpool_target.o recno/recno_target.o

include $(RGMK)
