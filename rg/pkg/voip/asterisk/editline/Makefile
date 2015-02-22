RGSRC=../../../..
include $(RGSRC)/envir.mak

A_TARGET=editline.a
O_OBJS_editline.a=editline.o history.o tokenizer.o readline.o np/np.o
CFLAGS+='-D__RCSID(x)=' '-D__RENAME(x)='

SUBDIRS+=np

include $(RGMK)
