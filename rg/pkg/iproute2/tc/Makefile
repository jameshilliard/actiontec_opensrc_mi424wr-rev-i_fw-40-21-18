IPROUTE2SRC=..
include $(IPROUTE2SRC)/envir.mak

CFLAGS+=-DCONFIG_GACT -DCONFIG_GACT_PROB
LDFLAGS+=-L. -ltc -lm -ldl -Wl,-export-dynamic

LOCAL_CFLAGS+=-I$(IPROUTE2SRC)/include
LOCAL_LDFLAGS+=-lm

TARGET=tc
O_OBJS_tc=tc.o tc_qdisc.o tc_class.o tc_filter.o tc_util.o m_police.o \
  m_estimator.o m_action.o q_fifo.o q_sfq.o q_red.o q_prio.o q_tbf.o \
  q_cbq.o f_rsvp.o f_u32.o f_route.o f_fw.o q_dsmark.o q_gred.o f_tcindex.o \
  q_ingress.o q_hfsc.o q_htb.o q_wrr.o m_gact.o

INTERNAL_HEADERS+= \
    ./tc_core.h  \
    ./tc_common.h  \
    ./tc_cbq.h  \
    ./tc_red.h  \
    ./tc_util.h  \

A_TARGET=libtc.a
O_OBJS_libtc.a=tc_core.o tc_red.o tc_cbq.o tc_estimator.o

LOCAL_TARGET=normal pareto paretonormal
O_OBJS_normal=normal.o
O_OBJS_pareto=pareto.o
O_OBJS_paretonormal=paretonormal.o

NETEM_DIST=normal.dist pareto.dist paretonormal.dist
OTHER_TASKS=$(NETEM_DIST)
DISTCLEAN+=$(NETEM_DIST)

RAMDISK_BIN_FILES+=$(TARGET)
include $(RGMK)

%.dist: %
	./$* >$@

