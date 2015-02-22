RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=des.h des_old.h
INTERNAL_HEADERS=des_locl.h rpc_des.h spr.h des_ver.h ncbc_enc.c

DES_ENC=des_enc.o fcrypt_b.o
O_OBJS= set_key.o  ecb_enc.o  cbc_enc.o \
	ecb3_enc.o cfb64enc.o cfb64ede.o cfb_enc.o  ofb64ede.o \
	enc_read.o enc_writ.o ofb64enc.o \
	ofb_enc.o  str2key.o  pcbc_enc.o qud_cksm.o rand_key.o \
	${DES_ENC} read2pwd.o \
	fcrypt.o xcbc_enc.o rpc_enc.o  cbc_cksm.o \
	ede_cbcm_enc.o des_old.o des_old2.o

include $(RGMK)

