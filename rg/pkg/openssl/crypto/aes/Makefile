RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=aes.h
INTERNAL_HEADERS=aes_locl.h

AES_ASM_OBJ=aes_core.o aes_cbc.o
O_OBJS=aes_misc.o aes_ecb.o aes_cfb.o aes_ofb.o aes_ctr.o $(AES_ASM_OBJ)

include $(RGMK)

