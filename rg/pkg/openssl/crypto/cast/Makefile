RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=cast.h
INTERNAL_HEADERS=cast_s.h cast_lcl.h

CAST_ENC=c_enc.o
O_OBJS=c_skey.o c_ecb.o $(CAST_ENC) c_cfb64.o c_ofb64.o

include $(RGMK)

