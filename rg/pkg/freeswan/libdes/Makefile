RGSRC=../../../
include $(RGSRC)/envir.mak

A_TARGET=libdes.a

CFLAGS+=-O3 -fomit-frame-pointer -DLIBDES_LIT -DRAND -DTERMIO

DES_ENC=des_enc.o fcrypt_b.o
O_OBJS=cbc_enc.o ecb_enc.o $(DES_ENC) fcrypt.o set_key.o

EXPORT_HEADERS_DIR=freeswan/libdes
EXPORT_HEADERS+=des.h des_locl.h des_ver.h podd.h sk.h spr.h

CD_EXPORTED_FILES=$(call VPATH_WILDCARD,*.[ch])

include $(RGMK)
