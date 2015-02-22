RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=ui.h ui_compat.h
INTERNAL_HEADERS=ui_locl.h

COMPATSRC=ui_compat.o
O_OBJS=ui_err.o ui_lib.o ui_openssl.o ui_util.o $(COMPATSRC)

include $(RGMK)

