RGSRC= ../
include $(RGSRC)/envir.mak

ifndef CONFIG_RG_OS
ifdef IS_BUILDDIR
  $(error CONFIG_RG_OS not defined!)
endif
endif

ifdef CONFIG_RG_OS_LINUX
  ARCHCONFIG_FIRST_TASKS+=cp_kernel
endif

ifdef CONFIG_RG_OS_LINUX_24
  LINUX_AUTOCONF_H=$(BUILDDIR)/os/linux-2.4/$(AUTOCONF_H)
endif
ifdef CONFIG_RG_OS_LINUX_26
  LINUX_AUTOCONF_H+=$(BUILDDIR)/os/linux-2.6/$(AUTOCONF_H)
endif

ARCHCONFIG_FIRST_TASKS+=create_links
ARCHCONFIG_FIRST_TASKS+=$(if $(CONFIG_RG_OS_LINUX), prepare_linux)

_create_includes_os_local:
	$(MKDIR) $(RG_BUILD)/local/include

ARCHCONFIG_FIRST_TASKS+=_create_includes_os_local
ifdef CONFIG_GLIBC
  ARCHCONFIG_FIRST_TASKS+=create_includes_os_glibc
endif
ifdef CONFIG_ULIBC
  ARCHCONFIG_FIRST_TASKS+=create_includes_os_ulibc
endif

KERNEL_DIR=$(CURDIR)/kernel



ifdef CONFIG_RG_OS_LINUX_24
  ifdef CONFIG_RG_JPKG
    ARCHCONFIG_SUBDIRS+=linux-2.4
  else
    SUBDIRS+=linux-2.4
  endif
endif

ifdef CONFIG_RG_OS_LINUX_26
  # kernel 2.6 does not have an archconfig target
  RAMDISK_SUBDIRS+=linux-2.6
endif

CLEAN+=$(wildcard $(STATIC_MOD_DIR)/*) $(BUILDDIR)/$(notdir $(OPENRG_IMG)) $(BUILDDIR)/$(notdir $(OPENRG_PROD)) layout.bin layout.sec

ifndef CONFIG_RG_JPKG
  OTHER_TASKS+=$(OPENRG_IMG) $(BUILDDIR)/$(notdir $(OPENRG_IMG))
endif

ifndef CONFIG_RG_GPL
  ifndef CONFIG_RG_JPKG
    ifeq ($(CONFIG_RG_PROD_IMG),y)
      OTHER_TASKS+=$(OPENRG_PROD) $(BUILDDIR)/$(notdir $(OPENRG_PROD))
    else  
      ifneq ($(CONFIG_RG_NETWORK_BOOT_IMAGE),y)
        OTHER_TASKS+=$(if $(CONFIG_OPENRG)$(CONFIG_RG_RGLOADER),image_size_verify)
      endif
    endif
    ifeq ($(CONFIG_RG_NETWORK_BOOT_IMAGE),y)
      OTHER_TASKS+=network_boot_message
    else
      OTHER_TASKS+=end_message
    endif
  endif
else
  $(shell touch $(BUILDDIR)/os/linux/arch/$(ARCH)/ramdisk/mod.img)
  $(shell touch $(BUILDDIR)/os/linux/arch/$(ARCH)/ramdisk/ramdisk.gz)
  $(shell touch $(BUILDDIR)/os/linux/arch/$(ARCH)/cramdisk/cramfs.img)
  $(shell touch $(BUILDDIR)/os/linux/arch/$(ARCH)/cramdisk/cramfs_init.img)
  $(shell touch $(BUILDDIR)/os/linux/needed_symbols.lst)
endif
  
ifdef CONFIG_RG_OS_LINUX
  CD_EXPORTED_FILES+=Rules_common.make Makefile.vmlinuz
endif
ifdef CONFIG_RG_OS_LINUX_24
  JPKG_EXPORTED_DIR_SRC+=linux-2.4
endif
ifdef CONFIG_RG_OS_LINUX_26
  JPKG_EXPORTED_DIR_SRC+=linux-2.6
endif

ifeq ($(CONFIG_RG_OS),ECOS)
  SUBDIRS=ecos
  ARCHCONFIG_SUBDIRS=$(SUBDIRS)
endif

ifdef CONFIG_RG_FACTORY_SETTINGS
  FACTORY_SETTINGS=$(BUILDDIR)/pkg/main/rg_factory
endif

ifndef CONFIG_RG_JPKG
ifneq ($(CONFIG_RG_OS),VXWORKS)
  ifneq ($(MAKECMDGOALS),archconfig)
# Adding explicit all: I have to include the boot Makefile before rg.mak for
# distribution purposes but I want the all: the be the first rule. (Ugly? yes,
# working? don't touch)
all:

    include Makefile.vmlinuz
  endif
endif
endif

ifeq ($(CONFIG_RG_RGLOADER),y)
    SECTION_TYPE="\<BOOT\>"
else
    SECTION_TYPE="\<IMAGE\>"
endif

include $(RGMK)
include $(RGSRC)/create_includes.mak

$(OPENRG_IMG): dummy

end_message: dummy
	@echo
	@echo "OpenRG rg_factory is ready at $(FACTORY_SETTINGS)"
	@echo "OpenRG image is ready at $(OPENRG_IMG)"
ifneq ($(CONFIG_RG_UML),)
	@echo "To run this image: "
	@echo "$$ jrguml burn    # 'burn' (load) the new image"
	@echo "$$ jnetwork start # connect uml0 (WAN) and uml1 (LAN) interfaces to UML"
	@echo "$$ jrguml start --std # start running OpenRG/UML"
	@echo "To later stop OpenRG/UML, run 'jrguml stop' from a DIFFERENT terminal"
	@echo "For further assistance see pkg/tools/README"
endif

network_boot_message: dummy
ifeq ($(findstring $(DIST), BCM96358), $(DIST))
	@echo
	@echo "Copy the following file to /tftpboot in order to allow network-booting:"
	@echo $$ cp $(BUILDDIR)/vendor/broadcom/bcm963xx/linux-2.6/boot/bootimg.elf /tftpboot
	@echo
endif

$(BUILDDIR)/$(notdir $(OPENRG_IMG)): $(notdir $(OPENRG_IMG))
	@$(RG_LN) $(OPENRG_IMG) $@

ifeq ($(CONFIG_RG_PROD_IMG),y)
  ifeq ($(findstring $(DIST), RGLOADER_RTA770W RGLOADER_WADB100G), $(DIST))
    # Create RGLoader image: 'jump boot' and 'boot' sections only
    BOOTSTRAP_CMD_PARAM=-s '0 ../vendor/broadcom/bcm963xx/boot/jmp_boot.img'
    BOOTSTRAP=$(OPENRG_IMG)
    # No factory settings or OpenRG image when creating RGLoader
    FACTORY_SETTINGS=
    PROD_SKIP_IMAGE=y
  endif
  ifdef BOOTSTRAP
    BOOTSTRAP_CMD_PARAM+=-b $(BOOTSTRAP)
  endif
  ifdef FACTORY_SETTINGS
    FACTORY_SETTINGS_CMD_PARAM=-f $(FACTORY_SETTINGS)
  endif
  ifneq ($(PROD_SKIP_IMAGE),y)
    IMAGE_PARAM=-i $(OPENRG_IMG)
  endif
$(OPENRG_PROD): $(OPENRG_IMG) $(BOOTSTRAP) $(FACTORY_SETTINGS)
	$(BUILDDIR)/pkg/perm_storage/build_flash -v $(BOOTSTRAP_CMD_PARAM) \
	  $(FACTORY_SETTINGS_CMD_PARAM) $(FLASH_LAYOUT_CMD_PARAM) \
	  $(IMAGE_PARAM) -o $@ -p $(FLASH_IMG_OFFSET)

$(BUILDDIR)/$(notdir $(OPENRG_PROD)): $(notdir $(OPENRG_PROD))
	@$(RG_LN) $(OPENRG_PROD) $@
else	
image_size_verify: $(OPENRG_IMG)
ifdef IS_BUILDDIR
ifdef ACTION_TEC_SMALL_IMG
	$(BUILDDIR)/pkg/perm_storage/build_flash
	@section_size=`$(BUILDDIR)/pkg/perm_storage/build_flash  | grep $(SECTION_TYPE) | head -n 1 | awk 'BEGIN { RS="\r\n"} { print $$8}'`; \
	image_size=`du -b -D $(OPENRG_IMG) | awk '{print $$1}'`; \
	echo "section size = $$((section_size)) , image size = $$image_size"; \
	if ((image_size>section_size)); then \
	    echo "Image too big (exceeds section size by "\
	        $$((image_size - section_size)) " bytes)."; \
	    exit 1; \
	fi; 
else
	$(BUILDDIR)/pkg/perm_storage/build_flash
	@section_size=`$(BUILDDIR)/pkg/perm_storage/build_flash  | grep $(SECTION_TYPE) | tail -n 1 | awk 'BEGIN { RS="\r\n"} { print $$8}'`; \
	image_size=`du -b -D $(OPENRG_IMG) | awk '{print $$1}'`; \
	echo "section size = $$((section_size)) , image size = $$image_size"; \
	if ((image_size>section_size)); then \
	    echo "Image too big (exceeds section size by "\
	        $$((image_size - section_size)) " bytes)."; \
	    exit 1; \
	fi; 
endif
endif
endif

modules:
ifdef IS_BUILDDIR
	make -C $(KERNEL_DIR) $@
endif

cp_kernel:
	$(MKDIR) $(BUILDDIR)/os/
ifdef CONFIG_RG_OS_LINUX_24
	$(RG_CP_LN) $(RGSRC)/os/linux-2.4 $(BUILDDIR)/os/
endif
ifdef CONFIG_RG_OS_LINUX_26
	$(MKDIR) $(BUILDDIR)/os/linux-2.6
endif
	ln -sfn $(RGSRC)/os/Makefile.vmlinuz $(BUILDDIR)/os
	ln -sfn $(RGSRC)/os/Makefile $(BUILDDIR)/os
	ln -sfn $(RGSRC)/os/Rules_common.make $(BUILDDIR)/os

create_links:
ifdef CONFIG_RG_OS_LINUX
  ifeq ($(CONFIG_RG_OS),LINUX_22)
	$(RG_LN) $(RGSRC)/linux $(CURDIR)/linux-2.2
	$(RG_LN) $(CURDIR)/linux-2.2 $(CURDIR)/linux
  endif
  ifeq ($(CONFIG_RG_OS),LINUX_24)
	$(RG_LN) $(CURDIR)/linux-2.4 $(CURDIR)/linux
  endif
  ifeq ($(CONFIG_RG_OS),LINUX_26)
	$(RG_LN) $(CURDIR)/linux-2.6 $(CURDIR)/linux
  endif
	@$(RG_LN) $(CURDIR)/linux $(KERNEL_DIR)
	@$(MKDIR) $(STATIC_MOD_DIR)
else
ifeq ($(CONFIG_RG_OS),VXWORKS)
	@$(RG_LN) $(PWD_SRC)/vx $(KERNEL_DIR)
endif
endif
LINUX_DOT_CONFIG_FILE=$(BUILDDIR)/os/linux-2.6/.config
$(LINUX_AUTOCONF_H):
	$(MKDIR) $(@D)
	@cat $(RG_CONFIG_H) | grep -v "extern " | grep -vw _RG_CONFIG_H_ | \
	  grep -v "^#endif" > $@
#        Replace the CC compiler from rg_gcc (as in rg_config.mk) with the 
#        correct one, since only CC is used by the kernel.
ifdef CONFIG_RG_OS_LINUX_26
	@echo "# Jungo auto-generated .config file" > $(LINUX_DOT_CONFIG_FILE)
	@echo "include $(CONFIG_FILE)" >> $(LINUX_DOT_CONFIG_FILE) 
	@echo "CC=$(if $(CONFIG_RG_UML),$(HOSTCC),$(TARGET_CC))" \
	  >> $(LINUX_DOT_CONFIG_FILE)     
	@echo "CPP=$(if $(CONFIG_RG_UML),$(HOSTCPP),$(TARGET_CPP))" \
	  >> $(LINUX_DOT_CONFIG_FILE)     
endif

prepare_linux: $(LINUX_AUTOCONF_H)
ifdef CONFIG_RG_OS_LINUX_26
	$(MAKE) -C $(RGSRC)/os/linux-2.6/ O=$(BUILDDIR)/os/linux-2.6/ scripts
	$(MAKE) -C $(RGSRC)/os/linux-2.6/ O=$(BUILDDIR)/os/linux-2.6/ prepare-all
endif

ifdef CONFIG_RG_OS_LINUX
del_subdirs:
	rm -rf $(STATIC_MOD_DIR)
endif

.PHONY: cp_kernel create_links modules del_subdirs prepare_linux
