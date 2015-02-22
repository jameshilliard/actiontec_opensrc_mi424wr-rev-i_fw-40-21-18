RGSRC=../../
include $(RGSRC)/envir.mak

SUBDIRS=lib ip misc etc tc

INTERNAL_HEADERS+= \
    ./include/linux/tc_act/tc_gact.h \
    ./include/linux/netlink.h \
    ./include/linux/pkt_cls.h \
    ./include/linux/pkt_sched.h \
    ./include/linux/rtnetlink.h \
    ./include/linux/tcp.h \
    ./include/linux/tcp_diag.h \
    ./include/linux/xfrm.h \
    ./include/SNAPSHOT.h \
    ./include/libnetlink.h \
    ./include/ll_map.h \
    ./include/rt_names.h \
    ./include/rtm_map.h \
    ./include/utils.h \
    ./include/linux/pkt_sched.h \
    ./include/linux/pkt_cls.h

CD_EXPORTED_FILES+=envir.mak

include $(RGMK)

