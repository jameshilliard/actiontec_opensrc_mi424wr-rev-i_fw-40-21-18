RGSRC=../../../../
include $(RGSRC)/envir.mak

INTERNAL_HEADERS+=config.h

CD_EXPORTED_FILES+=common.mak

ifneq ($(CONFIG_RG_BGP)$(CONFIG_RG_OSPF),)
  SUBDIRS=lib zebra
endif

ifneq ($(CONFIG_RG_BGP),)
  SUBDIRS+=bgpd
endif

ifneq ($(CONFIG_RG_OSPF),)
  SUBDIRS+=ospfd
endif

include $(RGMK)

