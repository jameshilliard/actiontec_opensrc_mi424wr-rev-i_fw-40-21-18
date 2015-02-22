IPROUTE2SRC=..
include $(IPROUTE2SRC)/envir.mak

LDFLAGS+=$(LIBNETLINK) -lm
CLFAGS+=-I../include
TARGET=nstat ifstat rtacct lnstat
RAMDISK_BIN_FILES+=$(TARGET)

INTERNAL_HEADERS+= ./ssfilter.h ./lnstat.h  

O_OBJS_ss=ss.o ssfilter.o
O_OBJS_lnstat=lnstat.o lnstat_util.o
O_OBJS_nstat=nstat.o
O_OBJS_ifstat=ifstat.o
O_OBJS_rtacct=rtacct.o
# Currently not supported since requires Berkeley DB with 1.85 compatibility
#O_OBJS_arpd=arpd.o
#TARGET+=arpd

DISTCLEAN+=ssfilter.c

include $(RGMK)

ifndef IS_DISTRIBUTION
ssfilter.c: ssfilter.y
	$(YACC) ssfilter.y -o ssfilter.c
endif

