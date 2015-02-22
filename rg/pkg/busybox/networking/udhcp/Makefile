RGSRC=../../../../
include $(RGSRC)/envir.mak

CFLAGS+=-DIN_BUSYBOX

O_OBJS_udhcp.a+=$(if $(CONFIG_UDHCPC)$(CONFIG_UDHCPD),\
  common.o options.o packet.o pidfile.o signalpipe.o socket.o)

O_OBJS_udhcp.a+=$(if $(CONFIG_UDHCPC),\
  dhcpc.o clientpacket.o clientsocket.o script.o)

O_OBJS_udhcp.a+=$(if $(CONFIG_UDHCPD),\
  dhcpd.o arpping.o files.o leases.o serverpacket.o static_leases.o)

O_OBJS_udhcp.a+=$(if $(CONFIG_DUMPLEASES),dumpleases.o)

ifneq ($(strip $(O_OBJS_udhcp.a)),)
  A_TARGET+=udhcp.a
endif

include $(RGMK)
