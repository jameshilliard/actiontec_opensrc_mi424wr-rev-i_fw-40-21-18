RGSRC=../../../..
include $(RGSRC)/envir.mak

# generic board support (must be statically linked to kernel)
MOD_TARGET=bsp.o
MOD_2_STAT=$(MOD_TARGET)
O_OBJS_bsp.o+=arch.o latch.o
O_OBJS_bsp.o+=$(if $(CONFIG_PCI), pci.o)

EXPORT_HEADERS_DIR=vendor
EXPORT_HEADERS+=jiwis8xx.h

ifndef CONFIG_RG_GPL
EXPORT_HEADERS+=$(if $(CONFIG_RG_KLEDS),plf_leds.h plf_leds.c__c/plf_leds.c)
endif

include $(RGMK)
