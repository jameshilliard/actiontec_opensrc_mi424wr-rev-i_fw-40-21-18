IPROUTE2SRC=..
include $(IPROUTE2SRC)/envir.mak

LDFLAGS+=$(LIBNETLINK) -lm

TARGET=ip rtmon
RAMDISK_BIN_FILES+=$(TARGET)

INTERNAL_HEADERS+= ./xfrm.h ./ip_common.h ./tunnel.h

O_OBJS_ip=ip.o ipaddress.o iproute.o iprule.o rtm_map.o iptunnel.o \
  ipneigh.o iplink.o ipmaddr.o ipmonitor.o ipmroute.o ipxfrm.o \
  xfrm_state.o xfrm_policy.o tunnel.o ip6tunnel.o

O_OBJS_rtmon=rtmon.o

include $(RGMK)

