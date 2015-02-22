RGSRC=../../..
include $(RGSRC)/envir.mak
include $(NET_TOOLS_MAKE_FILE)

HWOBJS = hw.o loopback.o slip.o ether.o ax25.o ppp.o arcnet.o tr.o tunnel.o \
    frame.o sit.o rose.o ash.o fddi.o hippi.o hdlclapb.o irda.o ec_hw.o

AFOBJS = unix.o inet.o inet6.o ax25.o ipx.o ddp.o ipx.o netrom.o af.o rose.o \
    econet.o

AFGROBJS = inet_gr.o inet6_gr.o ipx_gr.o ddp_gr.o netrom_gr.o ax25_gr.o \
    rose_gr.o getroute.o

AFSROBJS = inet_sr.o inet6_sr.o netrom_sr.o ipx_sr.o setroute.o

ACTOBJS  = slip_ac.o ppp_ac.o activate.o

VARIA = getargs.o masq_info.o proc.o util.o nstrcmp.o interface.o sockets.o

ifeq ($(HAVE_IP_TOOLS),1)
VARIA += util-ank.o
endif

O_OBJS = $(VARIA) $(AFOBJS) $(HWOBJS) $(AFGROBJS) $(AFSROBJS) $(ACTOBJS)
CLEAN += $(L_OBJS)
A_TARGET=libnet-tools.a

CFLAGS += -I$(RGSRC)/pkg/net-tools -idirafter $(RGSRC)/pkg/net-tools/include \
  -idirafter ../include -I$(RGSRC)/pkg/include

include $(RGMK)

