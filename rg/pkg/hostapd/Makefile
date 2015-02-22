RGSRC=../..
include $(RGSRC)/envir.mak

ifeq ($(CONFIG_BHR_REV_I),y)
SUBDIRS=main
LINK_DIRS=main
JPKG_EXPORTED_DIR_SRC+=main
else
SUBDIRS=common hostapd hostapd-0.4.8 main
LINK_DIRS=common hostapd hostapd-0.4.8 main
JPKG_EXPORTED_DIR_SRC+=common hostapd hostapd-0.4.8 main
endif

include $(RGMK)

