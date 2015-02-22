RGSRC=../../../../../../
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libgsm.a

O_OBJS=add.o code.o debug.o decode.o gsm_create.o gsm_decode.o gsm_destroy.o \
  gsm_encode.o gsm_explode.o gsm_implode.o gsm_option.o gsm_print.o \
  long_term.o lpc.o preprocess.o rpe.o short_term.o table.o

CFLAGS+=-I../inc

include $(RGMK)
