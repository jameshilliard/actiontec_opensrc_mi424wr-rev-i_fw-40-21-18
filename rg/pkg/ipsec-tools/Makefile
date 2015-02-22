RGSRC=../..
include $(RGSRC)/envir.mak

CFLAGS+=-DHAVE_CONFIG_H
SUBDIRS+=libipsec setkey racoon
INTERNAL_HEADERS+= \
./include-glibc/sys/queue.h \
./include-glibc/net/pfkeyv2.h \
./include-glibc/netinet/ipsec.h \
./include-glibc/glibc-bugs.h \
./setkey/vchar.h \
./setkey/extern.h \
./setkey/parse.h \
./libipsec/policy_parse.h \
./libipsec/ipsec_strerror.h \
./libipsec/libpfkey.h \
./config.h \
./package_version.h


include $(RGMK)
