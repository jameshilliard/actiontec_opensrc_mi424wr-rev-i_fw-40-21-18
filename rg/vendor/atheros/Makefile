RGSRC=../..
include $(RGSRC)/envir.mak

ifeq ($(CONFIG_BHR_REV_I),y)
SUBDIRS+=$(if $(CONFIG_RG_ATHEROS_HW_AR5416),carrier)
endif
ifeq ($(CONFIG_MC524WR_REV_G),y)
SUBDIRS+=$(if $(CONFIG_RG_ATHEROS_HW_AR5416),pb42_kiwi_rc)
endif

SUBDIRS+=$(if $(CONFIG_RG_ATHEROS_HW_AR5212),madwifi)
SUBDIRS+=$(if $(CONFIG_RG_ATHEROS_ART),art)

JPKG_EXPORTED_DIR+=madwifi pb42_kiwi_rc carrier art

include $(RGMK)
