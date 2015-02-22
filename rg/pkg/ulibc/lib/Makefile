RGSRC=../../../
include $(RGSRC)/envir.mak

ifdef CONFIG_DYN_LINK

RAMDISK_LIB_FILES=$(wildcard *0.9.29.so)
RAMDISK_LINKS=$(foreach f,$(wildcard *so*),$(if $(shell readlink $f),$f))
RAMDISK_LAST_TASKS+=ramdisk_links
ramdisk_links:
	$(foreach l,$(RAMDISK_LINKS),$(call RAMDISK_LN_FUNC,$(CRAMFS_RUNTIME_MOUNT_POINT)/lib/$(shell readlink $l),$(RAMDISK_RW_LIB_DIR)/$l))

endif

include $(RGMK)

