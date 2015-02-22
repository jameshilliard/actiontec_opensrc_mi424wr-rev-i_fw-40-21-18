RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=rc4.h
INTERNAL_HEADERS=rc4_locl.h

RC4_ENC=rc4_enc.o
O_OBJS=rc4_skey.o $(RC4_ENC)

ifdef CONFIG_RG_PPP_MPPE
  SRC=rc4_enc.c rc4_skey.c rc4_locl.h
  CD_EXPORTED_FILES+=$(SRC)
endif

include $(RGMK)

