RGSRC=../../../
include $(RGSRC)/envir.mak

A_TARGET=libatm.a libatmd.a
O_OBJS_libatm.a=text2atm.o atm2text.o atmequal.o sdu2cell.o text2qos.o \
  qos2text.o qosequal.o sap2text.o text2sap.o sapequal.o misc.o

ifeq ($(CONFIG_GLIBC)$(GLIBC_HAVE_LIBRESOLV),yy)
  ATMLIB_NEEDS_ANS_L=y
  JPKG_BIN_LIBCS_ans_l.o+=GLIBC
endif
ifeq ($(CONFIG_ULIBC)$(ULIBC_HAVE_LIBRESOLV),yy)
  ATMLIB_NEEDS_ANS_L=y
  JPKG_BIN_LIBCS_ans_l.o+=ULIBC
endif

ifdef ATMLIB_NEEDS_ANS_L
  O_TARGET+=ans_l.o
  O_OBJS_ans_l.o=ans_l.o ans.o
  LDFLAGS_ans_l.o=-L$(RG_LIB) -lresolv
  O_OBJS_libatm.a+=ans_l.o
else
  O_OBJS_libatm.a+=ans.o
endif

O_OBJS_libatmd.a=common.o diag.o kptr.o text2ip.o timer.o unix.o
CFLAGS+=-I../lib

include $(RGMK)
