# Note: This file included from rg/Makefile AFTER rg.mak

CREATE_CONFIG=$(BUILDDIR)/pkg/build/create_config

# create_config uses these environment variables
export DEV_IF_CONF_FILE=$(BUILDDIR)/pkg/main/device_conf.set
export MAC_TYPE_DEVS_FILE=$(BUILDDIR)/pkg/main/dev_if_mac_types.c
export MAJOR_FEATURES_FILE=$(BUILDDIR)/openrg_features.txt
export MAJOR_FEATURES_CFILE=$(BUILDDIR)/pkg/build/features.c
export OSTYPE:=$(shell echo $$OSTYPE)
export CONFIG_LOG=$(BUILDDIR)/config.log

ifdef DIST
CREATE_CONFIG_FLAGS=-d "$(DIST)"
endif

ifdef OS
CREATE_CONFIG_FLAGS+=-o "$(OS)"
endif

ifdef HW
CREATE_CONFIG_FLAGS+=-h "$(HW)"
endif

# Cygwin adds additional parameters to $MAKEFLAGS, which we don't need
export MAKEFLAGS_CYGWIN_FIX=$(filter-out " --unix -- ", $(MAKEFLAGS) $(EXTRA_MAKEFLAGS))

# TODO:
# in 'make config <config flags...>' we actually run at the end
# 'create_config <config flags...>', with some modifications:
# RGSRC=xxx VPATH=xxx: not passed
# DIST=xxx: passed as -d xxx
# OS=xxx: passed as -o xxx
# HW=xxx: passed as -h xxx
# XXX=yyy: passwd as -f XXX=yyy
# -d -s and all other flags without '=': not passed
#
# We need to change 'create_config' to accept the flags in the same format
# as 'make config', thus - simplifing the Makefile (below), and handling
# or ignoring the arguments in the C code of create_config.
#
# Remove MAKEFLAGS that are not of the type CONFIG_SOMETHING=value
CONFIG_FLAGS_FIX=$(foreach o,$(filter-out RGSRC=$(RGSRC) VPATH=$(VPATH),$1),$(if $(findstring =,$(o)),$(o),))
# Also filter out debug (make -d) and YUVAL (make --)
UNSPACED_MAKEFLAGS=$(subst \ ,^^^,$(MAKEFLAGS_CYGWIN_FIX))
UNSPACED_MAKEFLAGS_FIX=$(call CONFIG_FLAGS_FIX,$(UNSPACED_MAKEFLAGS))
CONFIG_OPTIONS=$(filter-out BUILD=$(BUILD) DIST=$(DIST) OS=$(OS) HW=$(HW),$(UNSPACED_MAKEFLAGS_FIX))
UNSPACED_FLAGS=$(foreach o,$(CONFIG_OPTIONS),-f $(o))
CREATE_CONFIG_FLAGS+=$(subst ^^^,\ ,$(UNSPACED_FLAGS))
MAKEFLAGS_FIX=$(subst ^^^,\ ,$(UNSPACED_MAKEFLAGS_FIX))

ifdef DOING_MAKE_CONFIG

host_config:
	pkg/build/detect_host.sh \
	  $(BUILDDIR)/host_config.h \
	  $(BUILDDIR)/host_config.mk \
	  $(BUILDDIR)/host_config.sh

ifneq ($(CONFIG_RG_GPL),y)
 ifeq ($(shell ls pkg/license/lic_rg.c 2> /dev/null),)
   LIC_RG_APP=$(RGSRC)/pkg/license/lic_rg
   ifeq ($(shell ls pkg/license/lic_rg 2> /dev/null),)
     $(LIC_RG_APP):
	 @echo $(LIC_RG_APP) " is missing. compilation can't continue."
	 @false
   endif
 else
   LIC_RG_APP=$(BUILDDIR)/pkg/license/lic_rg
   $(LIC_RG_APP): host_config
   include $(RGSRC)/pkg/license/Makefile.lic_rg
 endif
else
 LIC_RG_APP=
endif

$(CREATE_CONFIG):: host_config $(LIC_RG_APP)

config_files: $(CREATE_CONFIG)
	$(MKDIR) $(dir $(DEV_IF_CONF_FILE))
	$(MKDIR) $(dir $(RG_CONFIG_C))
	BUILDDIR=$(BUILDDIR) RGSRC=$(RGSRC) CONFIG_LOG=$(CONFIG_LOG) \
	  $(CREATE_CONFIG) \
	    $(CREATE_CONFIG_FLAGS) -m $(CONFIG_FILE) \
	    -e $(DEV_IF_CONF_FILE) \
	    -M $(MAJOR_FEATURES_FILE) \
	    -F $(MAJOR_FEATURES_CFILE) \
	    -i $(RG_CONFIG_H) -c $(RG_CONFIG_C) \
	    >$(CONFIG_LOG) \
	    2>&1 ; RET=$$? ; cat $(CONFIG_LOG) ; exit $$RET
	$(RG_LN) $(CONFIG_FILE) $(BUILDDIR)/rg_configure
	$(MKDIR) $(BUILDDIR)/pkg/include
	$(RG_LN) $(RG_CONFIG_H) $(BUILDDIR)/pkg/include

  # create_config should be compiled at the begining of config stage,
  # before we can use our rules. Compile it using a special Makefile.
  include $(RGSRC)/pkg/build/Makefile.create_config

endif

config_ended_successfully:
	@rm -f $(CONFIG_STARTED_FILE)
	@cat $(CONFIG_LOG)
	@echo Configuration ended successfully. You can now run make.

start_make_config:
	@echo $(MAKEFLAGS_FIX) > $(CONFIGURATION_FILE)
	@$(RG_LN) $(CONFIGURATION_FILE) $(BUILDDIR)/.configure
	@touch $(CONFIG_STARTED_FILE)

