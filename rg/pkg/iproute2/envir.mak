RGSRC=$(IPROUTE2SRC)/../..
include $(RGSRC)/envir.mak

CFLAGS+=-D_GNU_SOURCE -I$(IPROUTE2SRC)/include -DRESOLVE_HOSTNAMES
LDFLAGS+=-L$(IPROUTE2SRC)/lib -lnetlink -lutil

LIBNETLINK=$(IPROUTE2SRC)/lib/libnetlink.a $(IPROUTE2SRC)/lib/libutil.a

