RAMDISK_MODULES_FILES+=$(filter-out $(RAMDISK_MODULES_PERMANENT_FILES),$(ALL_MOBJS))

ifndef RAMDISK_SUBDIRS
RAMDISK_SUBDIRS=$(SUBDIRS) $(MOD_SUB_DIRS)
endif

# XXX: Remove this when the kernel makefiles are fixed. The kernel makefiles
# currently do not include envir.mak, where these defs are originally from
IS_BUILDDIR:=$(if $(findstring $(BUILDDIR),$(shell /bin/pwd)),y)
MKDIR=mkdir -p

export PWD_SRC:=$(if $(IS_BUILDDIR),$(CURDIR:$(BUILDDIR)%=$(RGSRC)%),$(CURDIR))
export PWD_BUILD:=$(if $(IS_BUILDDIR),$(CURDIR),$(CURDIR:$(RGSRC)%=$(BUILDDIR)%))
export PWD_JPKG:=$(CURDIR:$(if $(IS_BUILDDIR),$(BUILDDIR),$(RGSRC))%=$(JPKG_DIR)%)

export PWD_REL:=$(if $(call EQ,$(CURDIR),$(BUILDDIR)),./,$(subst $(BUILDDIR)/,,$(CURDIR)))
export JPKG_CUR_DIR=$(JPKG_DIR)/$(PWD_REL)

include $(COPY_DB)
