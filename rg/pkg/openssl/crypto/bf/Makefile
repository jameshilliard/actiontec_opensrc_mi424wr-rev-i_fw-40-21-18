RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=blowfish.h
INTERNAL_HEADERS=bf_pi.h bf_locl.h

BF_ENC=bf_enc.o
O_OBJS=bf_skey.o bf_ecb.o $(BF_ENC) bf_cfb64.o bf_ofb64.o

include $(RGMK)

