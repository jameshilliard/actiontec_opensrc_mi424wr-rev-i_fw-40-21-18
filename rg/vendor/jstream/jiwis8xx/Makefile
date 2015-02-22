RGSRC=../../..
include $(RGSRC)/envir.mak

SUBDIRS+=modules

FLASH_LAYOUT=flash_layout_jiwis800.c

EXPORT_HEADERS_DIR=vendor

ifndef CONFIG_RG_GPL
EXPORT_HEADERS+=$(if $(CONFIG_RG_PERM_STORAGE), $(FLASH_LAYOUT)__c/flash_layout.c)
endif

EXPORT_HEADERS+=$(if $(CONFIG_RG_UIEVENTS), uievents.c__c/uievents.c)

include $(RGMK)
