RGSRC=../../
include $(RGSRC)/envir.mak

#ifdef CONFIG_IPV6
  SUBDIRS+=client
#endif

#ifdef CONFIG_IPV6
  SUBDIRS+=server
#endif

#ifdef CONFIG_IPV6
  SUBDIRS+=relay
#endif

#ifdef CONFIG_IPV6
  SUBDIRS+=tools
#endif

#ifdef CONFIG_IPV6
  SUBDIRS+=common
#endif

#ifdef CONFIG_IPV6
  SUBDIRS+=missing
#endif

#ifdef CONFIG_IPV6
  SUBDIRS+=ctrl
#endif


INC_INTERL_H+=
INTERNAL_HEADERS+=./missing/config_ifaddrs.h \
./missing/ifaddrs.h \
./common/control.h \
./common/base64.h \
./common/common.h \
./common/lease.h \
./common/auth.h \
./common/timer.h \
./common/dhcp6_ctl.h \
./common/y.tab.h \
./common/config.h \
./common/dhcp6.h \
./client/dhcp6c.h \
./client/addrconf.h \
./client/prefixconf.h \
./client/dhcp6c_ia.h \
./client/netlink_dad.h

EXPORT_HEADERS+=
EXPORT_HEADERS_DIR=
WARN2ERR=n

include $(RGMK)
