ifndef COPY_DB_ENVIR_MAK
COPY_DB_ENVIR_MAK=1
ifneq ($(wildcard $(BUILDDIR)/.rgsrc),)
  export RGSRC:=$(shell cat $(BUILDDIR)/.rgsrc)
endif

ifdef VPATH
  VPATH_WILDCARD=$(strip $(wildcard $1) $(foreach f,$(strip $1),\
    $(patsubst ./%,%,$(addprefix $(dir $f),$(notdir $(wildcard $(VPATH)/$f))))))
  VPATH_WILDCARD_ABS=$(strip $(wildcard $(addprefix $(CURDIR)/,$1)) \
    $(wildcard $(addprefix $(VPATH)/,$1)))
else
  VPATH_WILDCARD=$(strip $(wildcard $1))
  VPATH_WILDCARD_ABS=$(strip $(wildcard $1))
endif

# Definitions of logical functions
# Usage: VAR=$(call OR,$(call EQ,$(A),1),$(CALL EQ,$(B),2)) is equal to:
#      if (a==1 || b==2) then VAR=y
OR=$(if $(1),y,$(if $(2),y))
AND=$(if $(1),$(if $2,y))
# XXX filter is not equal...
EQ=$(if $(filter $(2),$(1)),y)
NOT_EQ=$(if $(call EQ,$(1),$(2)),,y)
# add words in $(2) to list in $(1) only once
ADD_ONCE=$(1) $(foreach w,$(2),$(if $(filter $(w),$(1)),,$(w)))

PWD_REL:=$(if $(call EQ,$(CURDIR),$(BUILDDIR)),./,$(subst $(BUILDDIR)/,,$(CURDIR)))

LOOP:=$(RGSRC)/loop.mak

export PKG_DIR=$(RGSRC)/pkg/
export LICENSES_DIR=$(PKG_DIR)/build/licenses
export DISK_IMAGE_DIR=$(BUILDDIR)/pkg/build/disk_image
export CRAMFS_DIR=$(DISK_IMAGE_DIR)/cramfs_dir
export MODFS_DIR=$(DISK_IMAGE_DIR)/modfs_dir
export RAMDISK_RO_DIR=$(CRAMFS_DIR)
export RAMDISK_RW_DIR=$(DISK_IMAGE_DIR)/ramdisk_dir
export RAMDISK_MOUNT_POINT=$(DISK_IMAGE_DIR)/ramdisk_mount_point
export RAMDISK_IMAGE_FILE=$(DISK_IMAGE_DIR)/ramdisk_image_file
export RAMDISK_RO_BIN_DIR=$(RAMDISK_RO_DIR)/bin
export RAMDISK_RO_ETC_DIR=$(RAMDISK_RO_DIR)/etc
export RAMDISK_RO_LIB_DIR=$(RAMDISK_RO_DIR)/lib
export RAMDISK_RO_MODULES_DIR=$(RAMDISK_RO_DIR)/lib/modules
export RAMDISK_RW_BIN_DIR=$(RAMDISK_RW_DIR)/bin
export RAMDISK_RW_ETC_DIR=$(RAMDISK_RW_DIR)/etc
export RAMDISK_RW_LIB_DIR=$(RAMDISK_RW_DIR)/lib
export RAMDISK_RW_MODULES_DIR=$(RAMDISK_RW_DIR)/lib/modules
export RAMDISK_DEV_DIR=$(RAMDISK_RW_DIR)/dev
export CRAMFS_RUNTIME_MOUNT_POINT=/mnt/cramfs
export MODFS_RUNTIME_MOUNT_POINT=/mnt/modfs

export RG_CP=cp -avf
export RG_CP_LN=cp -srf
export RG_LN=ln -sfn
export RG_SHELL_FUNCS=. $(RGSRC)/mk_funcs.sh
export RG_LN_NOFAIL=$(RG_SHELL_FUNCS) && rg_lnf
export RG_VPATH_CP=$(RG_SHELL_FUNCS) && rg_vpath_cp

export RG_MKDIR=mkdir -p
export EXEC_AS_ROOT=$(BUILDDIR)/pkg/build/exec_as_root
export RG_MKNOD=$(EXEC_AS_ROOT) mknod
export RG_CHMOD=$(EXEC_AS_ROOT) chmod
RAMDISK_REPORT_FILE=$(DISK_IMAGE_DIR)/report.txt
export CRAMFS_FILES_FILE=$(BUILDDIR)/pkg/build/.cramfs_files.txt
export CRAMFS_FOOTPRINT_FILE=$(BUILDDIR)/pkg/build/.cramfs_footprint.txt
export DISK_IMG_FILE=$(BUILDDIR)/pkg/build/disk_image_file.txt

export CD_RGSRC_DIR=$(BUILDDIR)/pkg/build/cd_dir
export CD_GPL_DIR_REL=pkg/build/cd_dir_gpl
export CD_GPL_DIR=$(RGSRC)/$(CD_GPL_DIR_REL)
export GPL_TAG=gpl-src

export BOOTLDR_DISK_IMAGE_DIR=$(BUILDDIR)/bootldr_disk_image
export BOOTLDR_RAMDISK_MOUNT_POINT=$(BOOTLDR_DISK_IMAGE_DIR)/ramdisk_mount_point
export BOOTLDR_RAMDISK_RW_DIR=$(BOOTLDR_DISK_IMAGE_DIR)/ramdisk_dir
export BOOTLDR_RAMDISK_DEV_DIR=$(BOOTLDR_RAMDISK_RW_DIR)/dev

ifdef BOOTLDR_RAMDISK
  DISK_IMAGE_DIR=$(BOOTLDR_DISK_IMAGE_DIR)
  CRAMFS_DIR=$(DISK_IMAGE_DIR)/cramfs_dir
  RAMDISK_RO_DIR=$(CRAMFS_DIR)
  RAMDISK_RW_DIR=$(BOOTLDR_RAMDISK_RW_DIR)
  RAMDISK_MOUNT_POINT=$(BOOTLDR_RAMDISK_MOUNT_POINT)
  RAMDISK_IMAGE_FILE=$(DISK_IMAGE_DIR)/ramdisk_image_file
  RAMDISK_RO_BIN_DIR=$(RAMDISK_RO_DIR)/bin
  RAMDISK_RO_ETC_DIR=$(RAMDISK_RO_DIR)/etc
  RAMDISK_RO_LIB_DIR=$(RAMDISK_RO_DIR)/lib
  RAMDISK_RO_MODULES_DIR=$(RAMDISK_RO_DIR)/lib/modules
  RAMDISK_RW_BIN_DIR=$(RAMDISK_RW_DIR)/bin
  RAMDISK_RW_ETC_DIR=$(RAMDISK_RW_DIR)/etc
  RAMDISK_RW_LIB_DIR=$(RAMDISK_RW_DIR)/lib
  RAMDISK_RW_MODULES_DIR=$(RAMDISK_RW_DIR)/lib/modules
  RAMDISK_DEV_DIR=$(BOOTLDR_RAMDISK_DEV_DIR)
endif

# This file exists only on a distribution tree
DISTRIBUTION_NAME_FILE=$(RGSRC)/.distribution

export IS_DISTRIBUTION=$(if $(wildcard $(DISTRIBUTION_NAME_FILE)),y)

DIST?=$(CONFIG_RG_DIST)

# If we are in a distribution the distribution name is in the file
# DISTRIBUTION, otherwise it is a variable that the user should give in
# the make dist DIST=...
DISTRIBUTION_NAME=$(strip $(if $(call EQ,$(IS_DISTRIBUTION),y),$(shell cat $(DISTRIBUTION_NAME_FILE)),$(DIST)))

# Ramdisk function:
# When a non standard copy is needed (src name is different from dst) use
# RAMDISK_CP_RO_FUNC: 
# usage: 
# ramdisk::
# 	$(call RAMDISK_CP_RO_FUNC,src,dst,STRIP/STRIP_DEBUG_SYM/DONT_STRIP)
# 	$(call RAMDISK_CP_RW_FUNC,src,dst,STRIP/STRIP_DEBUG_SYM/DONT_STRIP)

RAMDISK_CP_FUNC_TEMP=$(RG_CP) -L $(if $(wildcard $(PWD_SRC)/$1),$(PWD_SRC)/$1,$1) $(2)
ifndef CONFIG_RG_FILES_ON_HOST
  RAMDISK_CP_FUNC=$(RAMDISK_CP_FUNC_TEMP)
else
  RAMDISK_CP_FUNC=$(if $(filter %/bin/init,$2),$(RAMDISK_CP_FUNC_TEMP),$(RG_LN) $(if $(wildcard $(PWD_SRC)/$1),$(subst $(RGSRC),/mnt/tree,$(PWD_SRC))/$1,$(subst $(RGSRC),/mnt/tree,$(CURDIR))/$1) $(2))
endif

RAMDISK_LN_FUNC=$(RG_MKDIR) $(dir $2) && $(RG_LN) $(1) $(2) && echo '- $(patsubst $(RGSRC)/%,%,$2) is linked to $1' >> $(RAMDISK_REPORT_FILE)
CRAMFS_FILES_FUNC=echo `pwd`/$(1) >> $(CRAMFS_FILES_FILE)
DISK_IMAGE_FUNC=echo $(subst $(BUILDDIR),'rg',$(CURDIR))/$(1) " -> " $(2) >> $(DISK_IMG_FILE)

RAMDISK_MKDIR_FUNC=$(RG_MKDIR) $(1)

#By default strip does not remove all useless (for us) sections. If you found 
# such one - add it into the list below
STRIPABLE_SECTIONS=.comment .note .pdr .gnu.warning.get .gnu.warning.tmpnam \
	.gnu.warning.siggetmask .gnu.warning.mktemp .gnu.warning.gets

STRIP_LIST=$(foreach s, $(STRIPABLE_SECTIONS), -R $s)

ifdef CONFIG_RG_NO_OPT
  RAMDISK_STRIP_FUNC=true
else
RAMDISK_STRIP_FUNC=$(STRIP) $(STRIP_LIST)  $(1) && echo '- stripping $1' >> \
		       $(RAMDISK_REPORT_FILE)
endif
RAMDISK_STRIP_DEBUG_SYM_FUNC=$(STRIP) -g $(STRIP_LIST) $(1) \
			     && echo '- stripping debug symbols $1' >> \
			     $(RAMDISK_REPORT_FILE)
ifdef CONFIG_RG_FILES_ON_HOST
  RAMDISK_STRIP_DEBUG_SYM_FUNC=true
  RAMDISK_STRIP_FUNC=true
endif

# Do nothing
NO_STRIP_FUNC=

RAMDISK_CP_RO_FUNC=$(call RAMDISK_MKDIR_FUNC,$(RAMDISK_RO_DIR)/$(dir $2)) && \
	      $(call RAMDISK_LN_FUNC,$(CRAMFS_RUNTIME_MOUNT_POINT)/$2,$(RAMDISK_RW_DIR)/$2) && \
	      $(call RAMDISK_CP_FUNC,$1,$(RAMDISK_RO_DIR)/$2) && \
	      echo '- $(patsubst $(RGSRC)/%,%,$(CURDIR))/$1 -> $(CRAMFS_RUNTIME_MOUNT_POINT)/$2' >> $(RAMDISK_REPORT_FILE) && \
	      $(if $(call EQ,$3,DONT_STRIP),true,$(call $(if $(call EQ,$3,STRIP),RAMDISK_STRIP_FUNC,RAMDISK_STRIP_DEBUG_SYM_FUNC),$(RAMDISK_RO_DIR)/$2)) && \
	      $(call CRAMFS_FILES_FUNC,$1) && \
	      $(call DISK_IMAGE_FUNC,$1,disk_image/cramfs_dir$2)
	      
RAMDISK_CP_MOD_FUNC=$(call RAMDISK_MKDIR_FUNC,$(MODFS_DIR)/$(dir $2)) && \
	      $(call RAMDISK_LN_FUNC,$(MODFS_RUNTIME_MOUNT_POINT)/$2,$(RAMDISK_RW_DIR)/$2) && \
	      $(call RAMDISK_CP_FUNC,$1,$(MODFS_DIR)/$2) && \
	      echo '- $(patsubst $(RGSRC)/%,%,$(CURDIR))/$1 -> $(MODFS_RUNTIME_MOUNT_POINT)/$2' >> $(RAMDISK_REPORT_FILE) && \
	      $(if $(call EQ,$3,DONT_STRIP),true,$(call $(if $(call EQ,$3,STRIP),RAMDISK_STRIP_FUNC,RAMDISK_STRIP_DEBUG_SYM_FUNC),$(MODFS_DIR)/$2)) && \
	      $(call CRAMFS_FILES_FUNC,$1) && \
	      $(call DISK_IMAGE_FUNC,$1,disk_image/modfs_dir$2)

RAMDISK_CP_RW_FUNC=$(call RAMDISK_MKDIR_FUNC,$(RAMDISK_RW_DIR)/$(dir $2)) && \
	      $(call RAMDISK_CP_FUNC,$1,$(RAMDISK_RW_DIR)/$2) && \
	      echo '* $(patsubst $(RGSRC)/%,%,$(CURDIR))/$1 -> /$2' >> $(RAMDISK_REPORT_FILE) && \
	      $(if $(call EQ,$3,STRIP),$(call RAMDISK_STRIP_FUNC,$(RAMDISK_RO_DIR)/$2),true) && \
	      $(call CRAMFS_FILES_FUNC,$1) && \
	      $(call DISK_IMAGE_FUNC,$1,disk_image/ramdisk_dir$2)

# Add a CURDIR prefix relative to root of rg tree. The logic:
# if (there is a / in the name (i.e absolute path))
#   if (there is BUILDDIR in the name)
#     replace BUILDDIR with nothing
#   else
#     replace RGSRC with nothing
# else
#   add CURDIR to name and remove BUILDDIR from name
# 
# Example:
# ======== 
# In directory pkg we export the file xxx.c
# 
# input                     output
# -----------------------------------
# xxx.c			    pkg/xxx.c
# /rg/pkg/xxx.c             pkg/xxx.c
# /rg/build.DIST/pkg/xxx.c  pkg/xxx.c
#
ADD_REL_PWD_PREFIX_=$(if $(filter /%,$1),$(subst $(if $(filter $(BUILDDIR)/%,$1),$(BUILDDIR)/,$(RGSRC)/),,$1),$(subst $(BUILDDIR)/,,$(addprefix $(CURDIR)/,$1)))
ADD_REL_PWD_PREFIX=$(foreach f,$1,$(call ADD_REL_PWD_PREFIX_,$f))

export STRIP_IFDEF=$(BUILDDIR)/pkg/build/make_cd/strip_ifdef
export STRIP_IFDEF_CONFIG=$(BUILDDIR)/pkg/build/make_cd/strip_ifdef_config.c
export DISTRIBUTION_SRC_LIST=$(CURDIR)/.dist_src_files

# All local and directory dependant variables should never be exported (linux
# makefiles export all variables unless unexported explicitly)
unexport ALL_OBJS
unexport ALL_TARGETS
unexport ALL_PRODS
unexport ALL_LOCAL_OBJS
unexport RG_ALL_OBJS
unexport CLEAN

unexport FOR_EACH
unexport COMMAND_FILE
unexport INDEX_DEP

unexport EXCLUDE_FROM_DEPS

unexport RAMDISK_STRIPT_FILES_
unexport RAMDISK_MODULES_FILES
unexport RAMDISK_FILES_
unexport RAMDISK_SUBDIRS
unexport RAMDISK_SUBDIRS_
unexport ALL_RAMDISK_FILES
unexport BOOTLDR_MODULES
unexport BOOTLDR_BIN_FILES

unexport COPY_DB_ENVIR_MAK
unexport NEED_ALL

endif
