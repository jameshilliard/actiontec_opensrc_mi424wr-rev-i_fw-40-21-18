RGSRC=../../../
include $(RGSRC)/envir.mak

SUBDIRS=$(if $(CONFIG_RG_OS_LINUX_26),linux-2.6)
SUBDIRS+=modules

EXPORT_HEADERS_DIR=vendor
EXPORT_HEADERS+=flash_layout.c__c/flash_layout.c

SUBDIRS+=$(if $(CONFIG_CAVIUM_FASTPATH)$(CONFIG_CAVIUM_FASTPATH_AEI), fastpath)

include $(RGMK)
