RGSRC := ../..
include $(RGSRC)/envir.mak
include $(NET_TOOLS_MAKE_FILE)

INTERNAL_HEADERS+= \
    ./lib/net-features.h \
    ./lib/net-support.h \
    ./lib/pathnames.h \
    ./lib/proc.h \
    ./lib/util.h \
    ./intl.h \
    ./include/interface.h \
    ./include/ipx.h \
    ./include/sockets.h \
    ./include/util-ank.h

RELEASE	= 1.54
H_FILES = config.h version.h
CONFIG_FILE = $(BUILDDIR)/pkg/include/net-tools-config.h
MKCONFIG = $(BUILDDIR)/pkg/util/mkconfig

CFLAGS	+= -D_GNU_SOURCE -I. -idirafter include -Ilib
LDFLAGS	+= -Llib
LDLIBS:=-lnet-tools $(LDLIBS)

SUBDIRS	= lib

ifdef CONFIG_RG_NETTOOLS_ARP
TARGET+=arp
RAMDISK_BIN_FILES+=arp
endif

ifdef NOT_DEFINED
TARGET+=netstat ifconfig hostname route rarp
endif

ifeq ($(HAVE_IP_TOOLS),1)
TARGET+=iptunnel ipmaddr
endif

#create for each target in TARGET a O_OBJS_target=target.o
COMMAND_FILE=variable.mak
FOR_EACH=$(TARGET)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(INDEX:%=%.o)
include $(LOOP)
O_OBJS_netstat+=statistics.o

ARCHCONFIG_FIRST_TASKS+=$(H_FILES)

include $(RGMK)
	
config.h: $(CONFIG_FILE)
	@ln -sf $< $@

#TODO change version to be OPENRG version
version.h:
	@echo "#define RELEASE \"net-tools $(RELEASE)\"" > version.h
