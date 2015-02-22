RGSRC=../../../../
include $(RGSRC)/envir.mak

TARGET=spi eroute spigrp tncfg klipsdebug
RAMDISK_BIN_FILES+=$(TARGET)

PLUTODIR=$(RGSRC)/pkg/freeswan/pluto

O_OBJS_spi=spi.o
ifeq ($(CONFIG_RG_IPSEC_ESP_ALG),y)
  O_OBJS_spi+=constants.o alg_info.o kernel_alg.o
  CFLAGS_spi+=-I$(PLUTODIR)
  CFLAGS_constants.o+=-UDEBUG
  CFLAGS_alg_info.o+=-DNO_PLUTO
  CFLAGS_kernel_alg.o+=-DNO_PLUTO
  LINKS=constants.c alg_info.c kernel_alg.c
$(LINKS):
	$(RG_LN) $(RGSRC)/pkg/freeswan/pluto/$@
endif
O_OBJS_eroute=eroute.o
O_OBJS_spigrp=spigrp.o
O_OBJS_tncfg=tncfg.o
O_OBJS_pf_key=pf_key.o
O_OBJS_klipsdebug=klipsdebug.o

CFLAGS+=-I$(RGSRC)/pkg/freeswan/klips/net/ipsec -I$(RGSRC)/pkg/freeswan/lib -I$(RGSRC)/pkg/gmp
LDFLAGS+=-L$(RGSRC)/pkg/freeswan/lib/
LDLIBS:=-lfreeswan $(LDLIBS)

include $(RGMK)

