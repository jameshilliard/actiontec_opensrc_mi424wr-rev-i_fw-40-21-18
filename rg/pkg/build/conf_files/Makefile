RGSRC=../../../
include $(RGSRC)/envir.mak

RAMDISK_ETC_FILES=nsswitch.conf protocols services resolv.conf hosts

ifdef CONFIG_LSP_DIST
  RAMDISK_ETC_FILES+=passwd
endif

# Check if we have ld.so.cache for this arch
ifdef CONFIG_ULIBC
  ifdef CONFIG_DYN_LINK
    LD_SO_FILE=ld.so.cache.$(ARCH)$(ENDIANESS_SUFFIX)
    ifeq ($(LD_SO_FILE),$(wildcard $(LD_SO_FILE)))
      LD_SO_CACHE=$(LD_SO_FILE)
      CD_EXPORTED_FILES+=$(LD_SO_CACHE)
    endif
  endif
endif

CD_EXPORTED_FILES+=passwd fstab $(RAMDISK_ETC_FILES)

RAMDISK_LAST_TASKS+=ramdisk_conf_files
ramdisk_conf_files:
	$(call RAMDISK_CP_RW_FUNC,fstab,fstab)
# Can't use RAMDISK_CP_RO_FUNC because we shouldn't try stripping ld.so.cache 
ifneq ($(LD_SO_CACHE),)
	$(call RAMDISK_MKDIR_FUNC,$(RAMDISK_RO_DIR))
	$(call RAMDISK_LN_FUNC,$(CRAMFS_RUNTIME_MOUNT_POINT)/etc/ld.so.cache,$(RAMDISK_RW_ETC_DIR)/ld.so.cache)
	$(call RAMDISK_CP_FUNC,$(LD_SO_CACHE),$(RAMDISK_RO_ETC_DIR)/ld.so.cache)
endif

include $(RGMK)
