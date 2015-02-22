RGSRC=../../../../../
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/route/quagga/quagga/common.mak

TARGET=zebra
RAMDISK_BIN_FILES+=$(TARGET)

INTERNAL_HEADERS= rib.h zserv.h router-id.h redistribute.h debug.h ipforward.h \
  irdp.h rtadv.h interface.h ioctl.h connected.h rt.h

O_OBJS = zserv.o main.o interface.o connected.o zebra_rib.o redistribute.o \
  debug.o rtadv.o zebra_snmp.o zebra_vty.o irdp_main.o irdp_interface.o \
  irdp_packet.o router-id.o ipforward_proc.o rt_ioctl.o if_proc.o \
  rtread_proc.o if_ioctl.o ioctl.o

LDLIBS+=-lquagga -lcrypt $(OPENRG_GPL_LDLIBS)

include $(RGMK)

