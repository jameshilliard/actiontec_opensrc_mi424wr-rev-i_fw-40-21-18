RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=dso.h

O_OBJS=	dso_dl.o dso_dlfcn.o dso_err.o dso_lib.o dso_null.o \
	dso_openssl.o dso_win32.o dso_vms.o

include $(RGMK)

