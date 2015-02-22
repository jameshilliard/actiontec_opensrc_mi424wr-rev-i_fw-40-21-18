export RGSRC:=$(shell pwd)
IN_RGSRC_ROOT=y

ifneq ($(MAKECMDGOALS),test_jpkg)
  include $(RGSRC)/envir.mak
endif

# XXX: the whole following section should be replaced by a simple rule:
# SUBDIRS+=os pkg vendor
# this will enable removing the ARCHCONFIG_SUBDIRS, CLEAN_SUBDIRS and
# and RAMDISK_SUBDIRS below.
SUBDIRS+=vendor pkg
RAMDISK_SUBDIRS=os vendor pkg # pkg must be the last dir in RAMDISK_SUBDIRS

ifdef CONFIG_RG_JPKG_SRC

  # liblocal_jutil.a sometimes uses openssl (for dists other than JPKG_XXX), 
  # so we need to export openssl's include files (though the openssl lib 
  # itself is not built)
  ARCHCONFIG_SUBDIRS=pkg/util pkg/jpkg pkg/include pkg/openssl pkg/license \
    pkg/build
  LINK_DIRS=pkg/openssl # because openssl's Makefile include other makefiles

  SUBDIRS+=os
else
  # XXX This is a hack: libc should move to pkg/Makefile
  PKG_LIBC+=$(if $(CONFIG_ULIBC),$(if $(ULIBC_IN_TOOLCHAIN),,pkg/ulibc))
  PKG_LIBC+=$(if $(CONFIG_GLIBC),$(if $(GLIBC_IN_TOOLCHAIN),,pkg/glibc))
  ifneq ($(strip $(PKG_LIBC)),)
    ifeq ($(CONFIG_RG_JPKG_BUILD_LIBC),y)
      FIRST_TASKS+=build_libc
    else
      ifeq ($(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),)
        FIRST_TASKS+=build_libc
      endif
    endif
  endif
  ARCHCONFIG_SUBDIRS=pkg/util $(if $(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),,os $(PKG_LIBC)) $(SUBDIRS) # pkg/util first for $(MKCONFIG)
endif
CLEAN_SUBDIRS=$(SUBDIRS) os

ARCHCONFIG_FIRST_TASKS+=\
  $(if $(CONFIG_ARCH_SL2312),sl2312_links) \
  $(if $(CONFIG_ARCH_CX8620X),cx8620x_links)

ifndef IS_BUILDDIR
  ifdef DOING_MAKE_CONFIG
# This is the begining of "make config":
# Create the files: config rg_config.mk rg_config.h
cd_to_builddir: start_make_config config_files
  endif
endif
ARCHCONFIG_FIRST_TASKS+=.rgsrc
ifeq ($(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),)
  ARCHCONFIG_FIRST_TASKS+=compiler
endif
ARCHCONFIG_FIRST_TASKS+=pkg/lib pkg/lib/rg
ifeq ($(CONFIG_RG_OS_VXWORKS)$(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),)
  ARCHCONFIG_FIRST_TASKS+=create_includes
endif

RAMDISK_FIRST_TASKS=create_ramdisk_dirs

ifdef CONFIG_RG_RMT_UPDATE
  OTHER_TASKS=$(OPENRG_RMT_UPDATE_IMG)
else
  OTHER_TASKS=$(OPENRG_IMG)
endif

CLEAN+=$(OPENRG_RMT_UPDATE_IMG) $(OPENRG_IMG)
ifdef CONFIG_ARCH_BCM947_CYBERTAN
  CLEAN+=code.bin
endif

CD_EXPORTED_FILES+=rg.mak envir.mak copy_db.mak loop.mak \
  loop_do.mak dep.mak copy_db_envir.mak general_dep.mak variable.mak \
  docs.mak rg_config.sh create_includes.mak target_cflags.mak \
  archive.mak util.mak archconfig.mak jpkg_target.mak target_binfmt_elf.mak \
  path_up_recursive.mak target_binfmt_flat.mak mk_funcs.sh LICENSE \
  target_log_entity.mak

ifeq ($(DIST_TYPE),BOOTLDR)
  include $(RGSRC)/bootldr.mak
endif

DIST_LAST_TASK:=cp.configure $(if $(GPL),gpl_extra_cp) remove_cvsignore

ARCHCONFIG_LAST_TASKS+=\
  $(if $(CONFIG_RG_JPKG_SRC),,$(if $(CONFIG_RG_UML),$(if $(CONFIG_RG_OS_LINUX_24),__setup)))
ARCHCONFIG_LAST_TASKS+=config_ended_successfully

FIRST_TASKS+=build_util build_libmtd

### make oldconfig
ifeq ($(MAKECMDGOALS),oldconfig)
# Do not check that the tree is configured
CHECK_CONFIGURE:=
endif
oldconfig:
ifndef IS_BUILDDIR
ifneq ($(wildcard $(CONFIGURATION_FILE)),)
	@echo "running: make config `cat $(CONFIGURATION_FILE)`"
	make config `cat $(CONFIGURATION_FILE)`
else
	$(there is no old configuration. Run make config.)
endif
endif

remove_cvsignore:
	find ${CD_RGSRC_DIR} -name .cvsignore | xargs rm -f

CD_DISTRIBUTION_NAME_FILE=$(DISTRIBUTION_NAME_FILE:$(RGSRC)%=$(JPKG_DIR)%)

gpl_extra_cp: $(CD_DISTRIBUTION_NAME_FILE)
	$(RGSRC)/gpl_extra.sh

cp.configure:
	cp -f $(CONFIGURATION_FILE) $(CD_RGSRC_DIR)/.archconfig

MAKEFLAGS_FIX=$(filter-out w -- RGSRC=$(RGSRC),$(MAKEFLAGS))

ifeq ($(wildcard $(RGSRC)/archconfig.mak),$(RGSRC)/archconfig.mak)
  # include archconfig.mak to get the create_config rules.
  include $(RGSRC)/archconfig.mak
endif

UPD_UTIL=pkg/rmt-update/upd_file_util

_EXT_VER=$(shell pkg/build/rg_version_info -e)
EXT_VER=$(if $(_EXT_VER),-e $(_EXT_VER),)

# To create the image, after we did all subdirs, we need to create modules, 
# make ramdisk and create the os.
$(OPENRG_RMT_UPDATE_IMG): $(OPENRG_IMG) $(if $(CONFIG_RG_JPKG),,$(UPD_UTIL))
ifndef CONFIG_RG_JPKG
ifneq ($(CONFIG_RG_NETWORK_BOOT_IMAGE),y)
	@$(UPD_UTIL) -C -f $@ -i $(OPENRG_IMG)
	@$(UPD_UTIL) -A -f $@ -h $(CONFIG_RG_HW) -d $(CONFIG_RG_DIST) \
	   -n $(ACTION_TEC_VENDOR_NAME) -v `pkg/build/rg_version_info -c` $(EXT_VER)
	@ln -fs $(OPENRG_RMT_UPDATE_IMG) \
            openrg-`pkg/build/rg_version_info -f`-$(CONFIG_RG_DIST).rmt
	@echo Remote-update file: $@
endif
endif

modules:
	$(MAKE) -C os modules

create_ramdisk_dirs:
	@rm -rf $(DISK_IMAGE_DIR)
	@$(MKDIR) $(DISK_IMAGE_DIR)
	@$(MKDIR) $(RAMDISK_MOUNT_POINT)
	@$(MKDIR) $(RAMDISK_DEV_DIR)
	@rm -rf $(BOOTLDR_DISK_IMAGE_DIR)
	@$(MKDIR) $(BOOTLDR_DISK_IMAGE_DIR)
	@$(MKDIR) $(BOOTLDR_RAMDISK_MOUNT_POINT)
	@$(MKDIR) $(BOOTLDR_RAMDISK_DEV_DIR)

# ACTION_TEC - Changes for GPL distribution
ifeq ($(CONFIG_RG_OS_LINUX)-$(CONFIG_RG_JPKG_SRC),y-)
  OPENRG_IMG_DEPS+=modules
endif
ifndef CONFIG_RG_JPKG
  OPENRG_IMG_DEPS+=ramdisk
endif

OPENRG_IMG_DEPS+=dummy

$(OPENRG_IMG): $(OPENRG_IMG_DEPS)
	$(MAKE) -C os
ifeq ($(CONFIG_I386_BOCHS),y)
	pkg/external/bochs.sh update $(CONFIG_RG_OS)
endif
ifdef CONFIG_RG_GPL
	@echo "Successfully completed compilation of BHR GPL source"
endif

ifdef CONFIG_RG_FACTORY_SETTINGS
  FACTORY_SECTION=-f $(RGSRC)/pkg/main/rg_factory
else
  FACTORY_SECTION=
endif

# XXX This is a hack: util and libc should be built from pkg/Makefile
build_libc:
ifdef IS_BUILDDIR
	make -C $(PKG_LIBC) VERBOSE=1 
endif

build_util:
	make -C $(RGSRC)/pkg/util rg_config_lib
	make -C $(RGSRC)/pkg/util mgt_client_lib

build_libmtd:
	make -C $(RGSRC)/pkg/mtd-utils/lib

__setup:
ifdef CONFIG_RG_OS_LINUX_24
	$(MAKE) -C $(BUILDDIR)/os/linux/ setup
else
	$(MAKE) -C $(RGSRC)/os/linux-2.6/ O=$(BUILDDIR)/os/linux-2.6/ setup
endif

compiler:
ifneq ($(strip $(PKG_LIBC)),)
	# Compiler wrapper will be built during LIBC compilation
	$(RG_CP_LN) $(PKG_LIBC:%=$(RGSRC)/%) $(BUILDDIR)/pkg
endif
	# Compiler wrapper for toolchains with LIBC
	$(MKDIR) $(BUILDDIR)/pkg/build
	$(MAKE) -C $(BUILDDIR)/pkg/build gcc \
	  RGSRC=$(RGSRC) -f $(RGSRC)/pkg/build/Makefile

JPKG_LAST_TASKS+=$(CD_DISTRIBUTION_NAME_FILE)

$(CD_DISTRIBUTION_NAME_FILE):
	@$(MKDIR) $(dir $(CD_DISTRIBUTION_NAME_FILE))
	@echo $(DIST) > $(CD_DISTRIBUTION_NAME_FILE)

ifdef JPKG_DISTS
  JPKG_LAST_TASKS+=build_jpkg_target
build_jpkg_target:
	for d in $(subst :, ,$(JPKG_DISTS)) ; do \
	  echo "Build JPKG_DIST: $$d" && cd $(JPKG_DIR) && \
	  make config DIST=$$d BUILD=build.$$d RGSRC=$(JPKG_DIR) && \
	  make BUILD=build.$$d RGSRC=$(JPKG_DIR); \
	done
endif

.rgsrc:
	@echo $(RGSRC) > $@

include $(RGSRC)/create_includes.mak

ifneq ($(wildcard $(RGSRC)/make_cd.mak),)
  include $(RGSRC)/make_cd.mak
endif

pkg/lib pkg/lib/rg:
	$(MKDIR) $(BUILDDIR)/$@

.PHONY: start_make_config modules create_ramdisk_dir config_ended_successfully

# ACTION_TEC - Changes for GPL distribution
ifdef CONFIG_RG_GPL
OTHER_TASKS=$(OPENRG_IMG)
endif

ifneq ($(MAKECMDGOALS),test_jpkg)
  include $(RGMK)
endif

sl2312_links:
	$(MAKE) -C $(RGSRC)/vendor/storlink/sl2312 create_links

cx8620x_links:
	$(MAKE) -C $(RGSRC)/vendor/conexant/cx8620x create_links


make_install_uml:
ifndef IS_BUILDDIR
	make -C pkg/tools install_uml_tools
endif

echovar:
	@echo $($(ECHOVAR))
