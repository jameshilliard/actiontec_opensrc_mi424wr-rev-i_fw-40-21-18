RGSRC=../../../../..
include $(RGSRC)/envir.mak

O_TARGET=np.o
O_OBJS=fgetln.o strlcat.o strlcpy.o unvis.o vis.o
CFLAGS+=-I.. '-D_DIAGASSERT(x)=' '-D__RENAME(x)='

include $(RGMK)
