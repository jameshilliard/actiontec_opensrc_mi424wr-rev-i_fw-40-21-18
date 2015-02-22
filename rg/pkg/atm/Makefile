RGSRC=../../
include $(RGSRC)/envir.mak

SUBDIRS=lib atmt
ifneq ($(CONFIG_LINUX_OAM),)
SUBDIR+=oam
endif

ifdef CONFIG_ATM_PVC_SCAN
SUBDIRS+=pvc_scan
endif

ifdef CONFIG_RG_OAM_PING
SUBDIRS+=oam_ping
endif

include $(RGMK)
