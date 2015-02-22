RGSRC= ../../../
include $(RGSRC)/envir.mak

LDFLAGS+=-L../libipsec -lcrypto -lresolv -lipsec #../libipsec/libipsec.a
CFLAGS+=-I../include-glibc/ -I../ -I../libipsec -I. -DHAVE_CONFIG_H

TARGET=setkey test-pfkey
RAMDISK_BIN_FILES+=$(TARGET)

O_OBJS_setkey =parse.o  setkey.o  token.o
O_OBJS_test-pfkey =test-pfkey.o

include $(RGMK)
