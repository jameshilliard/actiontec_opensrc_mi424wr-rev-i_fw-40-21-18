RGSRC=../../../../../
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/route/quagga/quagga/common.mak

TARGET=ospfd
RAMDISK_BIN_FILES+=$(TARGET)

INTERNAL_HEADERS= ospf_interface.h ospf_apiserver.h ospf_packet.h ospf_asbr.h \
  ospf_dump.h ospf_lsdb.h ospf_snmp.h ospf_route.h ospf_zebra.h \
  ospf_neighbor.h ospf_flood.h ospf_opaque.h ospf_ia.h ospf_te.h ospfd.h \
  ospf_network.h ospf_abr.h ospf_api.h ospf_ase.h ospf_ism.h ospf_lsa.h \
  ospf_nsm.h ospf_spf.h ospf_vty.h

O_OBJS = ospfd.o ospf_zebra.o ospf_interface.o ospf_ism.o ospf_neighbor.o \
  ospf_nsm.o ospf_dump.o ospf_network.o ospf_packet.o ospf_lsa.o ospf_spf.o \
  ospf_route.o ospf_ase.o ospf_abr.o ospf_ia.o ospf_flood.o ospf_lsdb.o \
  ospf_asbr.o ospf_routemap.o ospf_snmp.o ospf_opaque.o ospf_te.o ospf_vty.o \
  ospf_api.o ospf_apiserver.o ospf_main.o

LDLIBS+=-lquagga -lcrypt $(OPENRG_GPL_LDLIBS)

include $(RGMK)

