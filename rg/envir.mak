# all is the first target.
all:

# We can do either 'make archconfig' or 'make config'
DOING_MAKE_CONFIG:=$(DOING_MAKE_CONFIG)
ifeq ($(MAKECMDGOALS),archconfig)
  DOING_MAKE_CONFIG:=y
endif
ifeq ($(MAKECMDGOALS),config)
  DOING_MAKE_CONFIG:=y
endif

# check whether makeflags for config were defined
ifeq ($(DOING_MAKE_CONFIG)-$(IN_RGSRC_ROOT),y-y)
  ifeq ($(DIST),)
    # fetch makeflags from license file, if possible
    ifneq ($(LIC),)
      $(shell rm -f /tmp/rg_config.mak)
      $(shell $(RGSRC)/pkg/license/lic_rg makeflags-lines "$(LIC)" \
        2>/dev/null 1>/tmp/rg_config.mak)
      include /tmp/rg_config.mak
      $(shell rm -f /tmp/rg_config.mak)
      EXTRA_MAKEFLAGS:=$(shell $(RGSRC)/pkg/license/lic_rg makeflags "$(LIC)" \
      2>/dev/null)
    endif
  endif
  ifeq ($(DIST),)
    $(error DIST not defined)
  endif
endif
.PHONY: /tmp/rg_config.mak

# handle user pre-defined BUILD directory: set BUILDDIR accordingly
ifdef BUILD
  export BUILD
  BUILDDIR:=$(if $(filter /%,$(BUILD)),,$(shell cd $(RGSRC); pwd)/)$(BUILD)
endif

# handle BUILDDIR
ifdef BUILDDIR
  # we are already in BUILDDIR, or the user specified a specific BUILD dir
  IS_BUILDDIR:=$(if $(findstring $(BUILDDIR),$(CURDIR)),y)
  ifndef IS_BUILDDIR
    CREATE_BUILDDIR_LINK:=y
  endif
else
  # we are in the initial make source directory, we need to move
  # to BUILDDIR and respawn make
  IS_BUILDDIR:=$(if $(findstring $(RGSRC)/build,$(CURDIR)),y)
  ifndef IS_BUILDDIR
    ifdef DOING_MAKE_CONFIG
      ifeq ($(CURDIR),$(RGSRC))
       CREATE_BUILDDIR_LINK:=y
       BUILDDIR:=$(RGSRC)/build.$(DIST)
      endif
    endif
  endif
endif

BUILDDIR_LINK:=$(RGSRC)/build

CREATE_BUILDDIR_LINK_=$(shell mkdir -p $1; ln -sfn $1 $(BUILDDIR_LINK))
ifdef CREATE_BUILDDIR_LINK
    $(call CREATE_BUILDDIR_LINK_,$(BUILDDIR))
else
  ifndef BUILDDIR
    ifneq ($(wildcard $(BUILDDIR_LINK)),)
      BUILDDIR:=$(shell readlink $(BUILDDIR_LINK))
    else
      ifeq ($(DIST),)
        $(error DIST undefined (Cannot find $(BUILDDIR_LINK)). Run make config)
      else
        $(error Cannot find $(BUILDDIR_LINK). Run make config)
      endif
    endif
  endif
endif

export RGSRC:=$(if $(filter /%,$(RGSRC)),$(RGSRC),$(shell cd $(RGSRC) && pwd))

JPKG_EXPORTED_FILES+=Makefile
JPKG_EXPORTED_IF_EXIST+=envir.subdirs.mak

JPKG_DIR=$(BUILDDIR)/jpkg/rg

export PWD_SRC:=$(if $(IS_BUILDDIR),$(CURDIR:$(BUILDDIR)%=$(RGSRC)%),$(CURDIR))
export PWD_BUILD:=$(if $(IS_BUILDDIR),$(CURDIR),$(CURDIR:$(RGSRC)%=$(BUILDDIR)%))
export PWD_JPKG:=$(CURDIR:$(if $(IS_BUILDDIR),$(BUILDDIR),$(RGSRC))%=$(JPKG_DIR)%)

export PWD_REL:=$(if $(filter $(CURDIR),$(BUILDDIR)),.,$(subst $(BUILDDIR)/,,$(CURDIR)))
export JPKG_CUR_DIR=$(JPKG_DIR)/$(PWD_REL)

RGMK=$(RGSRC)/rg.mak
CONFIG_FILE=$(BUILDDIR)/rg_config.mk
RG_CONFIG_H=$(BUILDDIR)/rg_config.h
RG_CONFIG_C=$(BUILDDIR)/pkg/util/rg_c_config.c

LOOP:=$(RGSRC)/loop.mak
export COPY_DB=$(RGSRC)/copy_db.mak
export COPY_DB_ENVIR=$(RGSRC)/copy_db_envir.mak
export CONFIGURATION_FILE=$(BUILDDIR)/config

export BUILDDIR DOING_MAKE_CONFIG

-include $(CONFIG_FILE)

# internal functions for implementing PATH_UP
PWD_REL_LIST=$(subst /,! ,$1)
PWD_REL_NUM=$(words $(call PWD_REL_LIST,$1))
PWD_REL_MINUS_ONE=$(words $(wordlist 2,$(call PWD_REL_NUM,$1),$(call PWD_REL_LIST,$1)))
PATH_UP_REMOVE_LAST=$(wordlist 1,$(call PWD_REL_MINUS_ONE,$1),$(call PWD_REL_LIST,$1))

# usage: "$(call PATH_UP,a/b/c/d)" gives "a/b/c"
PATH_UP=$(subst !,,$(subst ! ,/,$(call PATH_UP_REMOVE_LAST,$1)))

ifdef IS_BUILDDIR

# PWD_REL=pkg/util --> PATH_UP_START=./pkg/util --> ./pkg/util ./pkg .
# PWD_REL=. --> PATH_UP_START=./ --> .
PATH_UP_START:=$(if $(filter .,$(PWD_REL)),./,./$(PWD_REL))
include $(RGSRC)/path_up_recursive.mak

# Include envir.subdirs.mak in all the parent directories up to RGSRC
-include $(addsuffix /envir.subdirs.mak,$(addprefix $(RGSRC)/,$(PATH_UP_LIST)))

VPATH:=$(if $(NEED_VPATH),$(PWD_SRC))

else  # IS_BUILDDIR
# we are in the source directory. we need to move to the build directory
all $(filter-out echovar distclean, $(MAKECMDGOALS)): cd_to_builddir

# This is the catch-all target for the SRCDIR
# we need it since the real target can only be built in BUILDDIR and we still
# need to do something in SRCDIR
%:;

endif # IS_BUILDDIR

CHECK_CONFIGURE:=$(if $(DOING_MAKE_CONFIG),,check_configure)

CONFIG_STARTED_FILE=$(BUILDDIR)/.make_config_running

BUILD2SRC=$(if $(findstring $(BUILDDIR),$1), \
  $(subst $(BUILDDIR),$(RGSRC),$1), \
  $(if $(filter /%,$1)$(findstring $(RGSRC),$1), \
    $1, \
    $(PWD_SRC)/$1))

# begin by searching for BUILDDIR, since it usually contains RGSRC as substring
SRC2BUILD_ALLOW_RELATIVE=$(strip \
  $(if $(findstring $(BUILDDIR),$1),\
    $1,\
    $(if $(findstring $(RGSRC),$1),\
      $(subst $(RGSRC),$(BUILDDIR),$1),\
      $1\
    )\
  )\
)

SRC2BUILD=$(strip \
  $(if $(filter /%,$(call SRC2BUILD_ALLOW_RELATIVE,$1)),\
    $(call SRC2BUILD_ALLOW_RELATIVE,$1),\
    $(PWD_BUILD)/$(call SRC2BUILD_ALLOW_RELATIVE,$1)\
  )\
)

# $1 - cflags
# $2 - parts of cflags we don't want to fix
FIX_VPATH_CFLAGS_EXCEPT_FOR=$(strip \
  $(foreach f,$1, \
    $(if $(filter-out -I -I- $2,$(filter -I%,$f)), \
      -I$(call SRC2BUILD_ALLOW_RELATIVE,$(f:-I%=%)) $(if $(DOING_MAKE_CONFIG),$f,), \
      $f) \
    ) \
)
    
FIX_VPATH_CFLAGS=$(call FIX_VPATH_CFLAGS_EXCEPT_FOR,$1,)

FIX_VPATH_LDFLAGS=$(strip \
  $(foreach f,$1, \
    $(if $(filter-out -L,$(filter -L%,$f)), \
      -L$(call SRC2BUILD_ALLOW_RELATIVE,$(f:-L%=%)) $(if $(DOING_MAKE_CONFIG),$f,), \
      $f)\
    )\
)

MKDIR=mkdir -p

export LD_LIBRARY_PATH:=$(LD_LIBRARY_PATH):.

# We dont define CFLAGS in rg_config.mk because the kernel dose not like it.
# Will be fixed with B2846
# However, CFLAGS may be defined in envir.subdirs.mak, so append and don't
# override
CFLAGS+=$(CFLAGS_ENVIR)
LDFLAGS+=$(LDFLAGS_ENVIR)

# RG_GLIBC_CFLAGS & RG_ULIBC_CFLAGS can be used by Makefiles that don't 
# use rg.mak for making .o from .c (e.g. pkg/ulibc/Rules.mak, pkg/gmp/Makefile)
ifdef CONFIG_GLIBC
-include $(BUILDDIR)/pkg/build/glibc/include/libc_config.make
RG_GLIBC_CFLAGS=-I$(RG_BUILD)/glibc/include
endif
ifdef CONFIG_ULIBC
-include $(BUILDDIR)/pkg/build/ulibc/include/libc_config.make
RG_ULIBC_CFLAGS=-I$(RG_BUILD)/ulibc/include -I$(BUILDDIR)/pkg/ulibc/include
endif

NORMAL_TARGETS=$(strip $(sort $(TARGET) $(O_TARGET) $(SO_TARGET) $(A_TARGET) \
  $(MOD_TARGET)))
LOCAL_TARGETS=$(strip $(sort $(LOCAL_TARGET) $(LOCAL_CXX_TARGET) \
  $(LOCAL_A_TARGET))) $(LOCAL_O_TARGET)
ALL_TARGETS=$(LOCAL_TARGETS) $(NORMAL_TARGETS) $(OTHER_TARGETS)
# Note: I'm using _O_OBJS_% variables that are only calculated in rg.mak. 
# These calculated variables can be used only in and after including rg.mak
ALL_LOCAL_OBJS=$(sort $(foreach o,$(LOCAL_TARGETS),$(_O_OBJS_$o)))

ALL_TARGET_OBJS=$(sort $(foreach o,$(NORMAL_TARGETS),$(_O_OBJS_$o) \
  $(_OX_OBJS_$o)) $(O_OBJS) $(_OTHER_OBJS) $(sO_OBJS))

ALL_OBJS=$(ALL_LOCAL_OBJS) $(ALL_TARGET_OBJS) $(filter %.o,$(OTHER_TARGETS))
ALL_PRODS=$(ALL_OBJS) $(ALL_TARGETS)

ifdef CONFIG_BINFMT_FLAT
  ALL_PRODS+=$(foreach t,$(TARGET),$t.elf.o)
  ALL_PRODS+=$(foreach t,$(TARGET),$t.gdb.o)
endif

A_TARGETS=$(A_TARGET) $(LOCAL_A_TARGET)

include $(RGSRC)/copy_db_envir.mak
-include $(BUILDDIR)/os/kernel/envir.mak

CLEAN+=$(COMPRESSED_RAMDISK) $(COMPRESSED_DISK)

DEBUG_PATH=$(BUILDDIR)/debug

# Language files
ifdef CONFIG_RG_LANG
CSV_LANG_FILES=$(call VPATH_WILDCARD,*_lang.csv)

H_LANG_FILES=$(if $(CONFIG_RG_JPKG_SRC),,$(patsubst %.csv,%.h,$(CSV_LANG_FILES)))

# No need for *_lang.[co] files in a binary jpkg distribution
O_LANG_FILES=$(if $(CONFIG_RG_JPKG),,$(patsubst %.csv,%.o,$(CSV_LANG_FILES)))
C_LANG_FILES=$(if $(CONFIG_RG_JPKG),,$(patsubst %.csv,%.c,$(CSV_LANG_FILES)))

EXPORT_HEADERS+=$(C_LANG_FILES) $(H_LANG_FILES)

# Only the _lang.csv file needs to be exported to JPKG.
DONT_EXPORT+=$(H_LANG_FILES) $(C_LANG_FILES)

JPKG_EXPORTED_IF_EXIST+=$(CSV_LANG_FILES)
endif

