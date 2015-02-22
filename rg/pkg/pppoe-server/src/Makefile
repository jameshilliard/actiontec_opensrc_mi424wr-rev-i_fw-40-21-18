RGSRC=../../../
include $(RGSRC)/envir.mak

ifdef CONFIG_PPPOES
SUBDIRS+=libevent

TARGET=pppoe pppoe-server

O_OBJS_pppoe-server=pppoe-server.o if.o debug.o common.o md5.o
O_OBJS_pppoe=pppoe.o if.o debug.o common.o ppp.o discovery.o

RAMDISK_ETC_FILES=/ppp/pppoe-server-options

# Configuration file path
PPPOESERVER_PPPD_OPTIONS=/etc/ppp/pppoe-server-options

# Program paths
PPPOE_PATH=/bin/pppoe
PPPD_PATH=/bin/pppd
PLUGIN_PATH=/etc/ppp/plugin

PATHS='-DPPPOE_PATH="$(PPPOE_PATH)"' '-DPPPD_PATH="$(PPPD_PATH)"' '-DPLUGIN_PATH="$(PLUGIN_PATH)"' '-DPPPOE_SERVER_OPTIONS="$(PPPOESERVER_PPPD_OPTIONS)"'

VERSION=3.5
CFLAGS+= $(PATHS) -DVERSION=$(VERSION)
LDFLAGS+=-Llibevent
LDLIBS:=-levent $(LDLIBS)

CD_EXPORTED_FILES+=pppoe-server-options
endif

ifdef CONFIG_RG_PPPOE_RELAY
  MOD_TARGET=pppoe_relay.o
  RAMDISK_MODULES_FILES+=$(MOD_TARGET)
  OX_OBJS_pppoe_relay.o=relay.o
  INTERNAL_HEADERS+=relay.h pppoe.h

  ifeq ($(CONFIG_RG_PPPOE_RELAY),y)
    MOD_2_STAT+=$(MOD_TARGET)
  endif
endif

include $(RGMK)
