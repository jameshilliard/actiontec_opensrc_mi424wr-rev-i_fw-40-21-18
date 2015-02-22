# Just to be sure that all will be the first rule...
# the 'real' all is in general_dep.mak
# Don't you ever use all: in any makefile you ever write or else...
all:

__DBGFR_STUB__=$(strip $1)

# DBGFR: print a function arguments, and return value
# $1 - function body 
# $2 - function name
# $3 - original arg 1
# $4 - original arg 2
# $5 - original arg 3
DBGFR=$(if $(MAKEDEBUG),$(warning $2: ENTER:$$1:$3 $$2:$4 $$3:$5))$(strip $1)$(if $(MAKEDEBUG),$(warning $2 : RETURN $(strip $1)))

#PWD_BUILD = /home/wzuo/projects/bhr/rg/build.MC524WR/pkg/ctl_layer 
#BUILDDIR=/pkg/ctl_layer = /home/wzuo/projects/bhr/rg/build.MC524WR/pkg/ctl_layer      
IS_CTL_LAYER=$(if $(findstring $(PWD_BUILD),$(BUILDDIR)/pkg/ctl_layer),$(if $(findstring $(BUILDDIR)/pkg/ctl_layer,$(PWD_BUILD)),ctl_layer))
CP_CTL_LAYER=cp -srf $(CURDIR) $(BUILDDIR)/pkg/
ifeq ($(CONFIG_BHR_REV_G),y)
MK_CTL_LAYER=$(if $(findstring clean,$(MAKECMDGOALS)), \
       make -C $(PWD_BUILD) bhr2clean TARGET_CTLLAYER=bhr2, \
       $(CP_CTL_LAYER) && make -C $(PWD_BUILD) bhr2 TARGET_CTLLAYER=bhr2)
else ifeq ($(CONFIG_BHR_REV_E),y)
MK_CTL_LAYER=$(if $(findstring clean,$(MAKECMDGOALS)), \
       make -C $(PWD_BUILD) bhr2clean TARGET_CTLLAYER=bhr2, \
       $(CP_CTL_LAYER) && make -C $(PWD_BUILD) bhr2 TARGET_CTLLAYER=bhr2)
else ifeq ($(CONFIG_BHR_REV_F),y)
MK_CTL_LAYER=$(if $(findstring clean,$(MAKECMDGOALS)), \
       make -C $(PWD_BUILD) bhr2clean TARGET_CTLLAYER=bhr2, \
       $(CP_CTL_LAYER) && make -C $(PWD_BUILD) bhr2 TARGET_CTLLAYER=bhr2)
else ifeq ($(CONFIG_BHR_REV_I),y)
MK_CTL_LAYER=$(if $(findstring clean,$(MAKECMDGOALS)), \
       make -C $(PWD_BUILD) bhr2clean TARGET_CTLLAYER=bhr2, \
       $(CP_CTL_LAYER) && make -C $(PWD_BUILD) bhr2_refi TARGET_CTLLAYER=bhr2)
endif

# In the source dir we only want to change to BUILDDIR
cd_to_builddir: $(CHECK_CONFIGURE)
ifdef MAKEDEBUG
	@echo "PWD_JPKG: $(PWD_JPKG)"
	@echo "PWD_BUILD: $(PWD_BUILD)"
	@echo "MAKECMDGOALS: $(MAKECMDGOALS)"
endif
	$(if $(filter /%,$(RGSRC)),,$(error Tree is not configured))
	umask 022 && \
	mkdir -p $(PWD_BUILD) $(if $(CONFIG_RG_JPKG),$(PWD_JPKG)) && \
	$(if $(IS_CTL_LAYER), $(MK_CTL_LAYER),\
	$(MAKE) -C $(PWD_BUILD) -f $(CURDIR)/Makefile \
	 $(MAKECMDGOALS) RGSRC=$(RGSRC))

distclean:
	rm -rf $(BUILDDIR) $(BUILDDIR_LINK)

check_configure:
	$(if $(wildcard $(CONFIGURATION_FILE)),,$(error Tree is not configured. Run make config))
	$(if $(wildcard $(CONFIG_STARTED_FILE)),$(error Tree did not complete configuration successfully. Run make config),)

export WARN2ERR

ifeq ($(WARN2ERR),)
  WARN2ERR=y
endif

ifeq ($(WARN2ERR),y)
  WARN2ERR_MARK=y
endif

# MOD_CFLAGS is used both in _mod_[24|26].o and module.o
ifdef WARN2ERR_MARK
  CFLAGS+=-Werror
  LOCAL_CFLAGS+=-Werror
  MOD_CFLAGS+=-Werror
endif

export DIST_LOG_ENTITY_EXCLUDE

ifeq ($(DIST_LOG_ENTITY_EXCLUDE),)
  DIST_LOG_ENTITY_EXCLUDE=n
endif

# this works around many places in rg tree which contain this line:
# #include <file.h> 
# where file.h is in the current directory (the correct way is to use "", 
# and then there's no need for this CFLAGS fix)
CFLAGS+=-I.
LOCAL_CFLAGS+=-I.
MOD_CFLAGS+=-I.

# allow archconfig targets to compile before headers are exported using
# the export_headers target (the alternative is to divide archconfig into 
# two passes over the whole tree; first for export_headers and second for
# archconfig targets)
ifdef DOING_MAKE_CONFIG
  LOCAL_CFLAGS+=-I$(RGSRC)/pkg/include
endif

ifdef CONFIG_RG_OS_LINUX
  MODFLAGS+=-DMODULE
  ifdef CONFIG_RG_OS_LINUX_24
    MOD_24_CFLAGS+=-DKBUILD_BASENAME=$(@:%.o=%) 
  endif
  ifdef CONFIG_RG_OS_LINUX_26
    MOD_26_CFLAGS+=-D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR($(@:%.o=%))"
  endif
endif

MOD_TARGET+=$(MOD_O_TARGET)
RG_LINUX_CFLAGS+=$(LINUX_CFLAGS)
IS_LOCAL_OBJ=$(strip $(foreach t,$(LOCAL_TARGETS),$(if $(filter $(notdir $1),$(notdir $(_O_OBJS_$t))),$t,)))
IS_SO_OBJ=$(strip $(foreach t,$(SO_TARGET),$(if $(filter $(notdir $1),$(notdir $(_O_OBJS_$t))),$t,)))
FIX_OBJ_NAME=$(if $(call IS_SO_OBJ,$1),$($(notdir 1):%_pic.o=%.o),$(patsubst %_mod_24.o,%.o,$(patsubst %_mod_26.o,%.o,$(notdir $1))))

GET_MODULE=$(strip $(foreach m,$(MOD_TARGET),$(if $(filter $1,$(_O_OBJS_$m) $(_OX_OBJS_$m)),$m)))

IS_XOBJ=$(strip $(foreach m,$(MOD_TARGET),$(if $(filter $1,$(_OX_OBJS_$m)),$m)))

ifdef CONFIG_RG_OS_LINUX
  GET_MODFLAGS=$(call DBGFR,\
    $(RG_LINUX_CFLAGS) $(MOD_CFLAGS) \
    $(if $(filter $(call GET_MODULE,$1),$(MOD_2_STAT)),\
      ,\
      $(MODFLAGS) \
    ) \
    $(if $(filter %_mod_24.o,$1),$(MOD_24_CFLAGS))\
    $(if $(filter %_mod_26.o,$1),$(MOD_26_CFLAGS))\
    $(if $(call IS_XOBJ,$1),-DEXPORT_SYMTAB)\
  ,GET_MODFLAGS,$1)
else
  GET_MODFLAGS=-D__KERNEL__ $(CFLAGS) $(MOD_CFLAGS)
endif

MOD_CFLAGS+=-I$(RGSRC)/pkg/include

_ULIBC_FLAG=--rg-use-ulibc
_GLIBC_FLAG=--rg-use-glibc

# $1 - existing flags
ifdef CONFIG_RG_JPKG_BIN
  _IS_COMPILING_ULIBC=$(if $(filter $(_ULIBC_FLAG),$1),yes)
else
  _IS_COMPILING_ULIBC=$(CONFIG_ULIBC)
endif

# $1 - existing flags
# $2 - obj name
# $3 - src
# - add the libc flag only if needed
# - RG_ULIBC_CFLAGS & RG_GLIBC_CFLAGS are already added in rg_gcc for c files, 
#   therefore they are added here only for cpp files.
_GET_CFLAGS_ULIBC=$(call DBGFR,\
    $(if $(call _IS_CPP_FILE,$3),\
      $(RG_ULIBC_CFLAGS),\
    $(if $(filter $(_ULIBC_FLAG),$1),\
      ,\
      $(_ULIBC_FLAG)\
    )\
  )\
  $(CFLAGS) \
,_GET_CFLAGS_ULIBC,$1,$2,$3)

_GET_CFLAGS_GLIBC=$(call DBGFR,\
    $(if $(call _IS_CPP_FILE,$3),\
      $(RG_GLIBC_CFLAGS),\
    $(if $(filter $(_GLIBC_FLAG),$1),\
      ,\
      $(_GLIBC_FLAG)\
    )\
  )\
  $(CFLAGS) \
,_GET_CFLAGS_GLIBC,$1,$2,$3)

_GET_OBJ_LOG_ENTITY=$(call DBGFR,$(TARGET_LOG_ENTITY_$(call \
  FIX_OBJ_NAME,$2))$(LOG_ENTITY_$(call \
  FIX_OBJ_NAME,$2)),_GET_OBJ_LOG_ENTITY,$1,$2,$3)

# Remove "local" prefix from local objects, so they will use the same entity
# ID as the target objects
FIX_OBJ_NAME_FOR_ENTITY_ID=$(if $(call IS_LOCAL_OBJ,$1),$(patsubst \
  local_%,%,$1),$1)
ENTITY_ID=$(if $(call _GET_OBJ_LOG_ENTITY,$1,$(call \
  FIX_OBJ_NAME_FOR_ENTITY_ID, $2),$3),$(call _GET_OBJ_LOG_ENTITY,$1,$(call \
  FIX_OBJ_NAME_FOR_ENTITY_ID, $2),$3),$(LOG_ENTITY))

# $1 - existing flags
# $2 - obj name
# $3 - src
# - locals don't pass through rg_gcc, so we fix the include path here.
_GET_OBJ_CFLAGS=$(call DBGFR,\
  $(if $(call _IS_CPP_FILE,$3),\
    $(CXXFLAGS) $(filter-out $(_ULIBC_FLAG) $(_GLIBC_FLAG),$1),\
    $1\
  )\
  $(if $(call IS_LOCAL_OBJ,$2),\
    $(LOCAL_CFLAGS) -I$(RG_BUILD)/local/include,\
    $(if $(call GET_MODULE,$2),\
      $(call GET_MODFLAGS,$2),\
      $(if $(call _IS_COMPILING_ULIBC,$1),\
        $(call _GET_CFLAGS_ULIBC,$1,$2,$3),\
        $(call _GET_CFLAGS_GLIBC,$1,$2,$3)\
      )\
    )\
  )\
  -DENTITY_ID=$(ENTITY_ID) $(TARGET_CFLAGS_$(call \
  FIX_OBJ_NAME,$2)) $(CFLAGS_$(call \
  FIX_OBJ_NAME,$2)),_GET_OBJ_CFLAGS,$1,$2,$3)

# $1 - existing flags
# $2 - obj name
# $3 - src
# linux 2.6 includes are already fixed in config_host.c, don't fix them again
GET_OBJ_CFLAGS=$(call DBGFR,\
  $(call FIX_VPATH_CFLAGS_EXCEPT_FOR,$(filter-out $(CFLAGS_REMOVE_$2),\
  $(if $(filter %_mod_24.o,$2),\
  $(filter-out $(RG_LINUX_26_CFLAGS),$(call _GET_OBJ_CFLAGS,$1,$2,$3)),\
  $(if $(filter %_mod_26.o,$2),\
  $(filter-out $(RG_LINUX_24_CFLAGS),$(call _GET_OBJ_CFLAGS,$1,$2,$3)),\
  $(call _GET_OBJ_CFLAGS,$1,$2,$3)))),\
  $(if $(filter %_mod_26.o,$2),$(RG_LINUX_26_CFLAGS)))\
,GET_OBJ_CFLAGS,$1,$2,$3)

# $1 - src
_IS_CPP_FILE=$(call DBGFR,\
  $(filter %.cpp %.cxx %.cc,$1)\
,_IS_CPP_FILE,$1)

# $1 - existing flags
# $2 - obj name
# $3 - src
# - For .c files, the target compiler is always rg_gcc, but for .cpp files we 
#   have to decide ourselves
# - In glibc, both CXX and TARGET_CXX point to the toolchain compiler. If
#   ulibc is also set, then CXX is set to rg_gcc but TARGET_CXX keeps its value
_GET_COMPILER=$(call DBGFR,\
  $(if $(call IS_LOCAL_OBJ,$2),\
    $(CC_FOR_BUILD),\
    $(if $(call GET_MODULE,$2),\
      $(TARGET_CC),\
      $(if $(call _IS_CPP_FILE,$3),\
        $(if $(call _IS_COMPILING_ULIBC,$1),\
          $(CXX),\
  	  $(TARGET_CXX)\
        ),\
        $(CC)\
      )\
    )\
  )\
,_GET_COMPILER,$1,$2,$3)

# Compilation functions for internal use only
# Usage: $(call RG_COMPILE_FUNC,EXTRA_FLAGS,output,src)
_RG_COMPILE_FUNC=$(strip \
  $(RG_LN_NOFAIL) $(call BUILD2SRC,$3) $(call SRC2BUILD,$3) && \
  $(call _GET_COMPILER,$1,$2,$3)\
  $(call GET_OBJ_CFLAGS,$1,$2,$3) -c -o $2 \
  $(filter-out $(PWD_SRC)/Makefile,\
    $(subst $(PWD_BUILD)/,,$(call SRC2BUILD_ALLOW_RELATIVE,$3))) \
  $(if $(STRIP_OBJS),&& $(STRIP) $(STRIP_FLAGS) $2) \
)

# Autogenerated sources and links should not be exported nor their objects
DONT_EXPORT+=$(foreach f,$(LINKS) $(AUTOGEN_SRC),$f $(f:%.c=%.o) \
  $(f:%.c=local_%.o) $(f:%.c=%_pic.o) $(f:%.S=%.o) $(f:%.S=local_%.o) \
  $(f:%.S=%_pic.o))

# Distributin source files.
# This is the place to do keyword expansion, strip_ifdef, anything else?
RG_COMPILE_FUNC_JPKG_SRC=$(if $(filter $(DONT_EXPORT),$(notdir $1) $1),\
  echo "Not exporting SRC $1",$(RG_VPATH_CP) $1 \
  $(JPKG_CUR_DIR)/$(1:$(PWD_SRC)/%=%))

# this is the place to strip debug information
JPKG_SOURCES=$(EXPORT_AS_SRC) \
  $(filter %.c,$(call GET_FILE_FROM,$(EXPORT_HEADERS)))
ifndef CONFIG_RG_JPKG
  RG_COMPILE_FUNC=$(call _RG_COMPILE_FUNC,$1,$2,$3)
endif
ifdef CONFIG_RG_JPKG_SRC
  RG_COMPILE_FUNC=$(if $(JPKG_TARGET_$2),$(call _RG_COMPILE_FUNC,$1,$2,$3) &&) $(call RG_COMPILE_FUNC_JPKG_SRC,$3)
endif

ifdef CONFIG_RG_JPKG_BIN

  # $1 - GLIBC or ULIBC
  # $2 - obj name
  # - if (JPKG_BIN_LIBCS_obj is set), then compile just for the specified libc.
  #   Otherwise, compile both ulibc and glibc.
  _NEEDS_TO_COMPILE_FOR=$(call DBGFR,\
    $(if $(CONFIG_$1),\
      $(if $(JPKG_BIN_LIBCS),\
        $(filter $1,$(JPKG_BIN_LIBCS)),\
        $(if $(JPKG_BIN_LIBCS_$2),\
          $(filter $1,$(JPKG_BIN_LIBCS_$2)),\
          yes_please\
        )\
      )\
    )\
  ,_NEEDS_TO_COMPILE_FOR,$1,$2)

  # _RG_COMPILE_AND_EXPORT: compiles and export using the default libc
  #
  # - if JPKG_TARGET_ is set, then the obj was already compiled in 
  #   RG_COMPILE_FUNC, so no need to compile again
  _RG_COMPILE_AND_EXPORT=$(call DBGFR,\
    $(if $(JPKG_TARGET_$2),\
      ,\
      $(call _RG_COMPILE_FUNC,$1,$2,$3) && \
    ) \
    $(RG_VPATH_CP) $2 $(JPKG_CUR_DIR)/$2.$(TARGET_MACHINE)\
  ,_RG_COMPILE_AND_EXPORT,$1,$2,$3)

  _RG_COMPILE_AND_EXPORT_BOTH_LIBCS=$(call DBGFR,\
    $(if $(call _NEEDS_TO_COMPILE_FOR,ULIBC,$2),\
      $(call _RG_COMPILE_FUNC,$(_ULIBC_FLAG) $1,$2,$3) && \
      $(RG_CP) $2 $2.ulibc && \
      $(RG_VPATH_CP) $2.ulibc $(JPKG_CUR_DIR)/$2.$(TARGET_MACHINE).ulibc &&\
    )\
    $(if $(call _NEEDS_TO_COMPILE_FOR,GLIBC,$2),\
      $(call _RG_COMPILE_FUNC,$(_GLIBC_FLAG) $1,$2,$3) && \
      $(RG_CP) $2 $2.glibc && \
      $(RG_VPATH_CP) $2.glibc $(JPKG_CUR_DIR)/$2.$(TARGET_MACHINE).glibc &&\
    )\
    true\
  ,_RG_COMPILE_AND_EXPORT_BOTH_LIBCS,$1,$2,$3)

  RG_COMPILE_FUNC=$(call DBGFR,\
    $(if $(JPKG_TARGET_$2),\
      $(call _RG_COMPILE_FUNC,$1,$2,$3) &&\
    )\
    $(if $(filter $(JPKG_SOURCES),$(notdir $3)),\
      $(call RG_COMPILE_FUNC_JPKG_SRC,$3),\
      $(if $(filter $(DONT_EXPORT),$2 $(2:local_%.o=%.o) $(2:%_pic.o=%.o)),\
        echo "Not exporting BIN $2",\
	$(if $(call IS_LOCAL_OBJ,$2),\
	  $(call _RG_COMPILE_AND_EXPORT,$1,$2,$3),\
	  $(if $(call GET_MODULE,$2),\
	    $(call _RG_COMPILE_AND_EXPORT,$1,$2,$3),\
	    $(call _RG_COMPILE_AND_EXPORT_BOTH_LIBCS,$1,$2,$3)\
	  )\
	)\
      )\
    )\
  ,RG_COMPILE_FUNC,$1,$2,$3)
endif

FILTER_SRC=$(filter %.c %.S %.s %.cpp %.cxx,$1)

ifdef VPATH
  GET_SRC=$(firstword $(wildcard $1.S) $(wildcard $(VPATH)/$1.S) \
    $(wildcard $1.cpp) $(wildcard $(VPATH)/$1.cpp) $(wildcard $1.cc) \
    $(wildcard $(VPATH)/$1.cc) $(wildcard $(VPATH)/$1.c) $(wildcard $1.c))
else
  GET_SRC=$(firstword $(wildcard $1.S) $(wildcard $1.cpp) $(wildcard $1.cc) \
    $(wildcard $1.c))
endif

SUBDIRS+=$(SUBDIRS_m)
SUBDIRS+=$(SUBDIRS_y)

ifdef CONFIG_RG_OS_VXWORKS
# For VxWorks we want MOD_TARGET to be compiled as O_TARGET
  O_TARGET+=$(MOD_TARGET)
endif

# *_SUBDIRS variables are using the mechanism in copy_db.mak
CLEAN_SUBDIRS?=$(SUBDIRS)
ARCHCONFIG_SUBDIRS?=$(SUBDIRS)
DOCS_SUBDIRS?=$(SUBDIRS)
RUN_UNITTESTS_SUBDIRS?=$(SUBDIRS)

DOCS_SUBDIRS_=$(DOCS_SUBDIRS:%=%/xmldocs.subdir__)
CLEAN_SUBDIRS_=$(CLEAN_SUBDIRS:%=%/clean.subdir__)
ARCHCONFIG_SUBDIRS_=$(ARCHCONFIG_SUBDIRS:%=%/archconfig.subdir__)
RUN_UNITTESTS_SUBDIRS_=$(RUN_UNITTESTS_SUBDIRS:%=%/run_unittests.subdir__)
SUBDIRS_=$(SUBDIRS:%=%/all.subdir__)

ifdef CREATE_LOCAL
  LOCAL_TARGET+=$(foreach t,$(filter $(CREATE_LOCAL),$(TARGET)), \
    $(filter-out ./,$(dir $t))local_$(notdir $t))
  LOCAL_A_TARGET+=$(foreach t,$(filter $(CREATE_LOCAL),$(A_TARGET)), \
    $(filter-out ./,$(dir $t))$(patsubst lib%.a,liblocal_%.a,$(notdir $t)))
  LOCAL_O_TARGET+=$(foreach t,$(filter $(CREATE_LOCAL),$(O_TARGET)), \
    $(filter-out ./,$(dir $t))$(patsubst %.o,local_%.o,$(notdir $t)))
  COMMAND_FILE=variable.mak
  FOR_EACH=$(CREATE_LOCAL)
  INDEX_VARIABLE=$(if $(filter $(INDEX),$(TARGET)),O_OBJS_local_$(INDEX),O_OBJS_$(INDEX:lib%.a=liblocal_%.a))
  INDEX_VAL=$(if $(O_OBJS_$(INDEX)),$(O_OBJS_$(INDEX)),$(O_OBJS))
  include $(LOOP)
  FOR_EACH=$(CREATE_LOCAL)
  INDEX_VARIABLE=$(if $(filter $(INDEX),$(TARGET)),L_OBJS_local_$(INDEX),L_OBJS_$(INDEX:lib%.a=liblocal_%.a))
  INDEX_VAL=$(if $(L_OBJS_$(INDEX)),$(L_OBJS_$(INDEX)),$(L_OBJS))
  include $(LOOP)
endif

# Using _[OL]_OBJS_% as the internal variable of all objs for each target.
# This variable contain the real object names for compilation including local_
# prefix for local targets.
# User shouldn't use _O_OBJS or _L_OBJS directly!

FIXLOCALA=$(1:lib%=liblocal_%)
ISA=$(call EQ,.a,$(1:%.a=.a))
# Create _O_OBJS_% for any target either from the O_OBJS_% or the default
# O_OBJS

ADD_LOCAL_PREFIX=$(foreach o,$1,$(dir $o)$(if $(call ISA,$o),$(call FIXLOCALA,$(notdir $o)),local_$(notdir $o)))

ifdef COMPILE_MULTIPLE_CONFIGS

# Checking that at least one of the configs in the list is set 
IS_ANY_MULTIPLE_CONFIG_SET=$(strip \
  $(foreach conf,$(filter-out  $(COMPILE_MULTIPLE_CONFIGS_SKIP),\
	$(COMPILE_MULTIPLE_CONFIGS)),\
    $(if $($(conf)),\
      y\
    )\
  )\
)

ifeq ($(IS_ANY_MULTIPLE_CONFIG_SET),) 

# None of the configs in COMPILE_MULTIPLE_CONFIGS is set - add to O_OBJS the 
# multiple objs as they are

# add to O_OBJS_ the multiple objs
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(O_OBJS_$(INDEX)) $(O_OBJS_MULTIPLE_$(INDEX)) $(O_OBJS_MULTIPLE)
include $(LOOP)

# add to OX_OBJS_ the multiple objs
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=OX_OBJS_$(INDEX)
INDEX_VAL=$(OX_OBJS_$(INDEX)) $(OX_OBJS_MULTIPLE_$(INDEX)) $(OX_OBJS_MULTIPLE)
include $(LOOP)

else #ifeq (IS_ANY_MULTIPLE_CONFIG_SET,)

# helper function for loops
# $1 - empty for O_OBJS , or
#      X     for OX_OBJS
_O_OR_OX_MULTIPLE_INDEX=$(call DBGFR, \
    $(if $(O$1_OBJS_MULTIPLE_$(INDEX))$(O$1_OBJS_MULTIPLE),\
        $(foreach conf,$(filter-out  $(COMPILE_MULTIPLE_CONFIGS_SKIP),\
	               $(COMPILE_MULTIPLE_CONFIGS)),\
	    $(if $($(conf)),\
	        $(O$1_OBJS_MULTIPLE_$(INDEX):%.o=%.$(conf:CONFIG_%=%).o)\
	        $(O$1_OBJS_MULTIPLE:%.o=%.$(conf:CONFIG_%=%).o)\
	    )\
        ),\
    )\
,_O_OR_OX_MULTIPLE_INDEX, $1)

# create renamed objs for all configs
# e.g. for CONFIG_ABC - 
# a.o => a.ABC.o
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=_O_OBJS_MULTIPLE_RENAMED_$(INDEX)
INDEX_VAL=$(call _O_OR_OX_MULTIPLE_INDEX)
include $(LOOP)

# create renamed X_objs for all configs
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=_OX_OBJS_MULTIPLE_RENAMED_$(INDEX)
INDEX_VAL=$(call _O_OR_OX_MULTIPLE_INDEX,X)
include $(LOOP)

# add to O_OBJS_ the renamed objs
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(O_OBJS_$(INDEX)) $(_O_OBJS_MULTIPLE_RENAMED_$(INDEX))
include $(LOOP)

# add to OX_OBJS_ the renamed objs
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=OX_OBJS_$(INDEX)
INDEX_VAL=$(OX_OBJS_$(INDEX)) $(_OX_OBJS_MULTIPLE_RENAMED_$(INDEX))
include $(LOOP)

# list of all of the multiple objects we use
_ALL_MULTIPLE_OBJS=$(foreach target,$(ALL_TARGETS),\
  $(_O_OBJS_MULTIPLE_RENAMED_$(target))\
  $(_OX_OBJS_MULTIPLE_RENAMED_$(target))\
)

ALL_CLEAN_FILES+=$(_ALL_MULTIPLE_OBJS)

# the wildcard is used for binary jpkg trees, where there're no sources
_ALL_MULTIPLE_OBJS_RENAMED_SRCS_C=$(foreach obj,$(_ALL_MULTIPLE_OBJS),\
    $(if $(wildcard $(PWD_SRC)/$(basename $(basename $(obj))).c),\
      $(obj:%.o=%.c),\
    )\
)
_ALL_MULTIPLE_OBJS_RENAMED_SRCS_CPP=$(foreach obj,$(_ALL_MULTIPLE_OBJS),\
    $(if $(wildcard $(PWD_SRC)/$(basename $(basename $(obj))).cpp),\
      $(obj:%.o=%.cpp),\
    )\
)
_ALL_MULTIPLE_OBJS_RENAMED_SRCS=$(_ALL_MULTIPLE_OBJS_RENAMED_SRCS_C) \
  $(_ALL_MULTIPLE_OBJS_RENAMED_SRCS_CPP)

#_ALL_MULTIPLE_OBJS_RENAMED_SRCS=$(foreach obj,$(_ALL_MULTIPLE_OBJS),\
    $(if $(wildcard $(PWD_SRC)/$(basename $(basename $(obj))).c),\
      $(obj:%.o=%.c),\
      $(if $(wildcard $(PWD_SRC)/$(basename $(basename $(obj))).cpp),\
        $(obj:%.o=%.cpp),\
        $(if $(wildcard $(PWD_SRC)/$(basename $(basename $(obj))).s),\
          $(obj:%.o=%.s),\
	)\
      )\
    )\
)

ifdef CONFIG_RG_JPKG_SRC

# We want to export the original src files in the src packages, and not the 
# renamed ones.

# extract the orig source file names from the renamed objects
_ALL_MULTIPLE_OBJS_ORIG_SRCS=$(foreach obj,$(_ALL_MULTIPLE_OBJS),\
    $(if $(wildcard $(PWD_SRC)/$(basename $(obj:%.o=%)).c),\
      $(basename $(obj:%.o=%)).c,\
      $(if $(wildcard $(PWD_SRC)/$(basename $(obj:%.o=%)).cpp),\
        $(basename $(obj:%.o=%)).cpp,\
        $(error Internal error: missing source file for $(obj).)\
      )\
    )\
)

JPKG_EXPORTED_FILES+=$(_ALL_MULTIPLE_OBJS_ORIG_SRCS)
DONT_EXPORT+=$(_ALL_MULTIPLE_OBJS_RENAMED_SRCS)

else # CONFIG_RG_JPKG_SRC

ARCHCONFIG_LAST_TASKS+=$(_ALL_MULTIPLE_OBJS_RENAMED_SRCS)
endif

endif # ifeq (IS_ANY_MULTIPLE_CONFIG_SET,)

endif # COMPILE_MULTIPLE_CONFIGS

COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=_O_OBJS_$(INDEX)
_TARGET_O_OBJS=$(if $(O_OBJS_$(INDEX)),$(O_OBJS_$(INDEX)),$(O_OBJS))
_TARGET_O_OBJS_WITH_LOCAL=$(strip \
  $(if $(filter $(INDEX),$(LOCAL_TARGETS)), \
    $(call ADD_LOCAL_PREFIX,$(_TARGET_O_OBJS)), \
    $(_TARGET_O_OBJS)\
  )\
)
# Fix problem of .// in the obj name. (x.c, ./x.c, .//x.c are different names)
# TODO: this operates only on the first obj, check if needed for others
_TARGET_O_OBJS_FIXED1=$(_TARGET_O_OBJS_WITH_LOCAL:.//%=%)
_TARGET_O_OBJS_FIXED2=$(_TARGET_O_OBJS_FIXED1:./%=%)

INDEX_VAL=$(_TARGET_O_OBJS_FIXED2)
include $(LOOP)


# Change module objects from x.o to x_mod_24.o and/or x_mod_26.o
COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_MOD_O_OBJS_$(INDEX)
MY_MOD_OBJS=$(if $(O_OBJS_$(INDEX)),$(O_OBJS_$(INDEX)),$(O_OBJS))
INDEX_VAL=$(if $(CONFIG_RG_OS_LINUX_24),$(MY_MOD_OBJS:%.o=%_mod_24.o)) $(if $(CONFIG_RG_OS_LINUX_26),$(MY_MOD_OBJS:%.o=%_mod_26.o))
include $(LOOP)

ifdef CONFIG_RG_OS_LINUX_24
COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_MOD_24_O_OBJS_$(INDEX)
MY_MOD_24_OBJS=$(if $(MOD_24_O_OBJS_$(INDEX)),$(MOD_24_O_OBJS_$(INDEX)),$(MOD_24_O_OBJS))
INDEX_VAL=$(MY_MOD_24_OBJS:%.o=%_mod_24.o)
include $(LOOP)
endif

ifdef CONFIG_RG_OS_LINUX_26
COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_MOD_26_O_OBJS_$(INDEX)
MY_MOD_26_OBJS=$(if $(MOD_26_O_OBJS_$(INDEX)),$(MOD_26_O_OBJS_$(INDEX)),$(MOD_26_O_OBJS))
INDEX_VAL=$(MY_MOD_26_OBJS:%.o=%_mod_26.o)
include $(LOOP)
endif

COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_O_OBJS_$(INDEX)
INDEX_VAL=$(_MOD_O_OBJS_$(INDEX)) $(_MOD_24_O_OBJS_$(INDEX)) $(_MOD_26_O_OBJS_$(INDEX))
include $(LOOP)

# Create _O_OBJS_%_pic
COMMAND_FILE=variable.mak
FOR_EACH=$(SO_TARGET) $(if $(A_TARGET_PIC), $(A_TARGET))
INDEX_VARIABLE=_O_OBJS_$(INDEX)
MY_PIC_OBJS=$(if $(O_OBJS_$(INDEX)),$(O_OBJS_$(INDEX)),$(O_OBJS))
INDEX_VAL=$(MY_PIC_OBJS:%.o=%_pic.o)
include $(LOOP)
FPIC_FLAG=-fpic

# Create _L_OBJS_%
COMMAND_FILE=variable.mak
FOR_EACH=$(ALL_TARGETS)
INDEX_VARIABLE=_L_OBJS_$(INDEX)
MY_L_OBJS=$(if $(L_OBJS_$(INDEX)),$(L_OBJS_$(INDEX)),$(L_OBJS))
INDEX_VAL= $(if $(filter $(INDEX),$(LOCAL_TARGETS)),\
	     $(call ADD_LOCAL_PREFIX,$(MY_L_OBJS)),\
	     $(MY_L_OBJS)\
	   )
include $(LOOP)

# Create _OX_OBJS_% (for MOD_TARGET)
COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_MOD_OX_OBJS_$(INDEX)
MY_MOD_OXBJS=$(if $(OX_OBJS_$(INDEX)),$(OX_OBJS_$(INDEX)),$(OX_OBJS))
INDEX_VAL=$(if $(CONFIG_RG_OS_LINUX_24),$(MY_MOD_OXBJS:%.o=%_mod_24.o)) $(if $(CONFIG_RG_OS_LINUX_26),$(MY_MOD_OXBJS:%.o=%_mod_26.o))
include $(LOOP)

ifdef CONFIG_RG_OS_LINUX_24
COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_MOD_24_OX_OBJS_$(INDEX)
MY_MOD_24_OXBJS=$(if $(MOD_24_OX_OBJS_$(INDEX)),$(MOD_24_OX_OBJS_$(INDEX)),$(MOD_24_OX_OBJS))
INDEX_VAL=$(MY_MOD_24_OXBJS:%.o=%_mod_24.o)
include $(LOOP)
endif

ifdef CONFIG_RG_OS_LINUX_26
COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_MOD_26_OX_OBJS_$(INDEX)
MY_MOD_26_OXBJS=$(if $(MOD_26_OX_OBJS_$(INDEX)),$(MOD_26_OX_OBJS_$(INDEX)),$(MOD_26_OX_OBJS))
INDEX_VAL=$(MY_MOD_26_OXBJS:%.o=%_mod_26.o)
include $(LOOP)
endif

COMMAND_FILE=variable.mak
FOR_EACH=$(MOD_TARGET)
INDEX_VARIABLE=_OX_OBJS_$(INDEX)
INDEX_VAL=$(_MOD_OX_OBJS_$(INDEX)) $(_MOD_24_OX_OBJS_$(INDEX)) $(_MOD_26_OX_OBJS_$(INDEX))
include $(LOOP)

_OTHER_OBJS=$(OTHER_OBJS)

# Create the TARGET_CFLAGS_% cflags. This variable is the CFLAGS_target of the
# target that %.o is connected to.
# Note that if the same object is used for more than one target the last 
# target CFLAGS will overrun the previes cflags.
COMMAND_FILE=target_cflags.mak
FOR_EACH=$(ALL_TARGETS)
include $(LOOP)

# Create LOG_ENTITY_%
COMMAND_FILE=target_log_entity.mak
FOR_EACH=$(ALL_TARGETS)
include $(LOOP)

ifdef CONFIG_RG_OS_LINUX
  MOD_2_STAT_LINKS:=$(addprefix $(STATIC_MOD_DIR),$(filter-out $(MOD_O_TARGET),$(MOD_2_STAT)))
endif

JPKG_TARGETS_ALL:=$(strip $(DOING_MAKE_CONFIG) $(JPKG_TARGETS_ALL) \
  $(if $(CONFIG_RG_JPKG_BIN),$(JPKG_BIN_TARGETS_ALL)))
ifndef CONFIG_RG_JPKG
  JPKG_TARGETS_ALL:=y
endif

ifdef JPKG_TARGETS_ALL
  JPKG_TARGETS:=$(ALL_TARGETS) $(foreach t,$(A_TARGETS),__create_lib_$t)
else
  JPKG_TARGETS:=$(foreach t,$(ALL_TARGETS),$(if $(JPKG_TARGET_$t),$t)) \
    $(foreach t,$(A_TARGETS),$(if $(JPKG_TARGET_$t),__create_lib_$t))
  ifdef CONFIG_RG_JPKG_BIN
    JPKG_TARGETS:=$(foreach t,$(ALL_TARGETS),$(if $(JPKG_TARGET_BIN_$t),$t)) \
      $(foreach t,$(A_TARGETS),$(if $(JPKG_TARGET_BIN_$t),__create_lib_$t)) \
      $(JPKG_TARGETS)
  endif
endif

ifdef JPKG_TARGETS
  COMMAND_FILE=variable.mak
  FOR_EACH=$(JPKG_TARGETS)
  INDEX_VARIABLE=JPKG_TARGET_$(INDEX)
  INDEX_VAL=y
  include $(LOOP)
  COMMAND_FILE=jpkg_target.mak
  FOR_EACH=$(JPKG_TARGETS)
  INDEX_VAR_PREFIX=JPKG_TARGET_
  include $(LOOP)
endif

ifdef IS_BUILDDIR

# C source code compilation
%.o: %.c
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_24.o: %.c
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_26.o: %.c
	$(call RG_COMPILE_FUNC,,$@,$<)
	
# C++ source code compilation
%.o: %.cpp
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_24.o: %.cpp
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_26.o: %.cpp
	$(call RG_COMPILE_FUNC,,$@,$<)
%.o: %.cxx
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_24.o: %.cxx
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_26.o: %.cxx
	$(call RG_COMPILE_FUNC,,$@,$<)
%.o: %.cc
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_24.o: %.cc
	$(call RG_COMPILE_FUNC,,$@,$<)
%_mod_26.o: %.cc
	$(call RG_COMPILE_FUNC,,$@,$<)

# PIC files
%_pic.o: %.c
	$(call RG_COMPILE_FUNC,$(FPIC_FLAG),$@,$<)

%_pic.o: %.cpp
	$(call RG_COMPILE_FUNC,$(FPIC_FLAG),$@,$<)

%_pic.o: %.cxx
	$(call RG_COMPILE_FUNC,$(FPIC_FLAG),$@,$<)

# For now assembly code compiles the same as C code
%.o: %.S
	$(call RG_COMPILE_FUNC,-D__ASSEMBLY__,$@,$<)
%_mod_24.o: %.S
	$(call RG_COMPILE_FUNC,-D__ASSEMBLY__,$@,$<)
%_mod_26.o: %.S
	$(call RG_COMPILE_FUNC,-D__ASSEMBLY__,$@,$<)

local_%.o : %.c
	$(call RG_COMPILE_FUNC,,$@,$<)

$(sO_OBJS):
	$(call RG_COMPILE_FUNC,$(sOFLAGS) -x assembler-with-cpp,$@,$<)

# This is a default rule to make an object out of its dependencies.
$(_OTHER_OBJS):
	$(call RG_COMPILE_FUNC,,$@,$(call FILTER_SRC,$^))

%.c: %.y
	$(YACC) $(YFLAGS) $< -o $@

ifdef CONFIG_RG_LANG
%_lang.c: $(PWD_SRC)/%_lang.csv
	$(LANG_COMPILER) --require-pos fmtcheck $^
	$(LANG_COMPILER) $(LANG_COMPILER_OPT) csv2c $(notdir $(patsubst %_lang.csv,%,$^)) $^ $@

$(PWD_BUILD)/%_lang.h %_lang.h: $(PWD_SRC)/%_lang.csv
	$(LANG_COMPILER) $(LANG_COMPILER_OPT) csv2h $(notdir $(patsubst %_lang.csv,%,$^)) $^ $@
endif

$(O_TARGET):
	$(if $(JPKG_TARGET_$@),$(LD) $(ENDIAN_LDFLAGS) -r -o $@ $(foreach o,$(filter-out $(PWD_SRC)/Makefile,$^), $(call OS_PATH,$(o))) $(LDFLAGS_$@))
	@$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$@

$(LOCAL_O_TARGET):
	$(if $(JPKG_TARGET_$@),$(HOST_LD) -r -o $@ $(foreach o,$(filter-out $(PWD_SRC)/Makefile,$^), $(call OS_PATH,$(o))) $(LDFLAGS_$@))

ifdef CONFIG_RG_OS_LINUX

$(MOD_TARGET):
ifdef CONFIG_RG_OS_LINUX_26
	@# compile kos_26_mod.o for all modules (override RG_COMPILE_FUNC with exact cflags)
	$(if $(JPKG_TARGET_$@),\
	$(if $(filter $@,$(MOD_2_STAT) $(MOD_O_TARGET)),,\
	$(RG_LN) $(RGSRC)/pkg/util/kos_26_mod.c $(*F).mod.c && \
	$(TARGET_CC) $(RG_LINUX_CFLAGS) $(MOD_CFLAGS) $(MOD_26_CFLAGS) -DKBUILD_MODNAME=$(*F) \
	  -DMODULE -c -o $(*F).mod.o $(*F).mod.c))
endif
	$(if $(JPKG_TARGET_$@), \
	  $(LD) $(ENDIAN_LDFLAGS) -r -o $@ \
	    -Map $@.link.map \
	    $(filter-out $(*F).mod.c,\
	      $(filter-out %.h,\
	        $(filter-out $(PWD_SRC)/Makefile,$^)\
	       )\
	     )\
	    $(if $(CONFIG_RG_OS_LINUX_26),\
	      $(if $(filter $@,$(MOD_2_STAT) $(MOD_O_TARGET)),,$(*F).mod.o)\
	     ) \
	    $(LDFLAGS_$@)\
	 )
	@$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$@

$(MOD_2_STAT_LINKS):
	$(RG_LN) $(CURDIR)/$(notdir $@) $(dir $@)$(MOD_2_STAT_PREFIX_$(notdir $@))$(notdir $@)
endif

# TODO: Remove the __create_lib_ logic and replace it with a touch on the 
# changed object (make sure it is newer than the archive).
GET_AR=$(if $(filter $1,$(A_TARGET)),$(AR),$(if $(filter $1,$(LOCAL_A_TARGET)),$(HOST_AR),$(error RG_MAKEFILE internal error $1)))

$(foreach t,$(A_TARGETS),__create_lib_$t):
	$(if $(JPKG_TARGET_$@),\
	  $(if $?,$(call GET_AR,$(@:__create_lib_%=%)) crv \
	  $(@:__create_lib_%=%) $?))

$(A_TARGET):
	$(if $(JPKG_TARGET_$@),$(RANLIB) $@)
	@$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$(@F)

$(LOCAL_A_TARGET):
	$(if $(JPKG_TARGET_$@),$(HOST_RANLIB) $@)

# Note: the -Wl,--no-whole-archive must be the last argument in order to make
# sure that when a so_target add -Wl,--whole-archive, it will not add all libgcc
# and libc into the sheared library

SO_LAST_FLAGS=-Wl,--no-whole-archive -Wl,-Map $@.link.map

$(SO_TARGET):
	$(if $(JPKG_TARGET_$@),\
	  touch $@.link.map; \
	    $(CC) -shared -o $@ $(FPIC_FLAG) $(SO_CFLAGS) $(SO_CFLAGS_$@) $^ \
	      $(call FIX_VPATH_LDFLAGS,$(SO_LDFLAGS) $(SO_LDFLAGS_$@)) \
	      $(SO_LAST_FLAGS), \
	  @echo "SO_TARGET: Not building ($@:$)"\
	) 
	@$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$@ 

# We have to have static on a local target so that the target will work in a 
# distribution tree on a different computer with different libc version.
$(LOCAL_TARGET):
	$(if $(JPKG_TARGET_$@),\
	$(CC_FOR_BUILD) -o $@ $^ $(call FIX_VPATH_LDFLAGS,\
	$(filter-out $(LDFLAGS_REMOVE_$@),\
	$(LOCAL_LDFLAGS)) $(LDFLAGS_$@)) $(filter-out $(LDLIBS_REMOVE_$@),\
	$(LOCAL_LDLIBS)) $(LDLIBS_$@),\
	@echo "LOCAL_TARGET: Not building ($@:$^)")

$(LOCAL_CXX_TARGET):
	$(if $(JPKG_TARGET_$@),\
	$(CXX_FOR_BUILD) -o $@ $^ $(call FIX_VPATH_LDFLAGS,\
	$(filter-out $(LDFLAGS_REMOVE_$@),$(LOCAL_LDFLAGS)) $(LDFLAGS_$@))\
	$(filter-out $(LDLIBS_REMOVE_$@),$(LOCAL_LDLIBS)) $(LDLIBS_$@),\
	@echo "LOCAL_CXX_TARGET: Not building ($@:$^)")

# This is the default binary distribution rule.
ifdef CONFIG_ULIBC
%.o: %.o.$(TARGET_MACHINE).ulibc
	$(MKDIR) $(dir $@)
	$(RG_LN) $(if $(filter /%,$<),,$(CURDIR)/)$< $@
endif

ifdef CONFIG_GLIBC
%.o: %.o.$(TARGET_MACHINE).glibc
	$(MKDIR) $(dir $@)
	$(RG_LN) $(if $(filter /%,$<),,$(CURDIR)/)$< $@
endif

# for locals and modules (no libc distinction)
%.o: %.o.$(TARGET_MACHINE)
	$(MKDIR) $(dir $@)
	$(RG_LN) $(if $(filter /%,$<),,$(CURDIR)/)$< $@

ifneq ($(IS_ANY_MULTIPLE_CONFIG_SET),)

$(_ALL_MULTIPLE_OBJS_RENAMED_SRCS_C): %.c:
	$(RG_LN_NOFAIL) $(PWD_SRC)/$(basename $*).c 
	@echo "#include <linux/config.h>" > $@
	@for config in $(COMPILE_MULTIPLE_CONFIGS) ; do \
	    echo "#undef $$config" >> $@ ; \
	done
	@echo "#define CONFIG_$(subst .,,$(suffix $*))" >> $@
	@echo "#include \"$(basename $*).c\"" >> $@

$(_ALL_MULTIPLE_OBJS_RENAMED_SRCS_CPP): %.cpp:
	$(RG_LN_NOFAIL) $(PWD_SRC)/$(basename $*).cpp
	@echo "#include <linux/config.h>" > $@
	@for config in $(COMPILE_MULTIPLE_CONFIGS) ; do \
	    echo "#undef $$config" >> $@ ; \
	done
	@echo "#define CONFIG_$(subst .,,$(suffix $*))" >> $@
	@echo "#include \"$(basename $*).cpp\"" >> $@

endif

include $(COPY_DB)

include $(RGSRC)/docs.mak

JPKG_EXPORTED_FILES+=$(call GET_FILE_FROM,$(EXPORT_HEADERS)) \
  $(CD_EXPORTED_FILES) $(foreach f,$(JPKG_EXPORTED_IF_EXIST),\
  $(if $(call VPATH_WILDCARD,$f),$f))
ifdef CONFIG_RG_JPKG_SRC
  JPKG_EXPORTED_FILES+=$(INTERNAL_HEADERS)
  JPKG_EXPORTED_DIR+=$(JPKG_EXPORTED_DIR_SRC)
endif

export_to_jpkg:
	@$(RG_SHELL_FUNCS) && \
	  $(foreach f,\
	  $(strip $(filter-out $(DONT_EXPORT),$(JPKG_EXPORTED_FILES))),\
	  rg_vpath_cp $f $(JPKG_CUR_DIR)/$f &&) true
ifneq ($(strip $(JPKG_EXPORTED_DIR)),)
	rm -rf $(addprefix $(PWD_JPKG)/,$(filter-out ., \
	    $(strip $(JPKG_EXPORTED_DIR))))
	cd $(PWD_SRC) && (echo $(RG_SHELL_FUNCS) && \
	    find $(JPKG_EXPORTED_DIR) -not -regex \
	    '^\(\|.*/\)CVS/.*$$' -and -type f \
	    $(foreach f,$(DONT_EXPORT),-and -not -path "$f") | \
	    awk '{printf "rg_vpath_cp %s $(PWD_JPKG)/%s\n", $$1, $$1}') | \
	    bash -s$(if $(MAKEDEBUG),x)
endif

ifndef CONFIG_RG_GPL
ifdef CONFIG_RG_LANG
RG_LANG_FILES=$(filter-out $(NOT_RG_LANG_FILES),$(CSV_LANG_FILES))

ARCHCONFIG_LAST_TASKS+=$(H_LANG_FILES) $(C_LANG_FILES) 
endif
endif

ARCHCONFIG_LAST_TASKS+=make_dir_debug

# This functions handles files in the format of <src>__<target>

# $1 - files in the <src>__<target> format
# $2 - 1 - return the src, 2 return the target
IS_FROM_TO=$(findstring __,$1)
GET_FILE_FROM_TO=$(foreach f,$1,$(if $(call IS_FROM_TO,$f),$(word $2,$(subst __, ,$f)),$f))
GET_FILE_FROM=$(call GET_FILE_FROM_TO,$1,1)
GET_FILE_TO=$(call GET_FILE_FROM_TO,$1,2)

# We create the target directory because it might not exist yet.
# We check wether to create the link to the SRC or the BUILD dir, usualy the
# target of the link is in the source directory, but autogenarated files are in
# BUILD directory.
EXPORT_FILE=$(MKDIR) $(dir $2) && \
  $(RG_LN) $(if $(wildcard $(PWD_SRC)/$1),$(PWD_SRC),$(CURDIR))/$1 $2

export_headers:
	$(foreach f,$(EXPORT_HEADERS),\
	$(call EXPORT_FILE,$(call GET_FILE_FROM,$f),$(RG_INCLUDE_DIR)/$(EXPORT_HEADERS_DIR)/$(call GET_FILE_TO,$f)) &&) true
	
export_libs:
ifdef EXPORT_LIBS
	$(foreach f,$(EXPORT_LIBS),$(call EXPORT_FILE,$f,$(BUILDDIR)/pkg/lib/$f) &&) true
endif

ln_internal_headers:
	$(RG_SHELL_FUNCS) && \
	  $(foreach f,\
	  $(strip $(INTERNAL_HEADERS) $(CD_EXPORTED_FILES) \
	     $(foreach eh,$(EXPORT_HEADERS),$(call GET_FILE_FROM,$(eh)))),\
	  $(if $(MAKEDEBUG),echo ln_internal_headers: copying $f &&,)\
	  DO_LINK=1 rg_vpath_cp $f $(PWD_BUILD)/$f &&) \
	true
	
dist_log_entity:
ifeq ($(DIST_LOG_ENTITY_EXCLUDE),n)
#\054 is for comma
	@rm -f $(BUILDDIR)/pkg/util/dist_log_entity.tmp
#Every subdirectory adds it entity to the list of entity
	@echo -e '$(LOG_ENTITY)\054' >> $(BUILDDIR)/pkg/util/dist_log_entity.tmp
#Also, every file that compiles add it entity to the list of entities
	@$(foreach o,$(ALL_OBJS),$(if $(LOG_ENTITY_$(o)),echo -e '$(LOG_ENTITY_$(o))\054' >> $(BUILDDIR)/pkg/util/dist_log_entity.tmp;,))
#Also, add the option to add more entities manually to the list of entities
	@$(foreach o,$(DIST_LOG_ENTITY),echo -e '$(o)\054' >> $(BUILDDIR)/pkg/util/dist_log_entity.tmp;)
	@cat $(BUILDDIR)/pkg/util/dist_log_entity.tmp >> $(BUILDDIR)/pkg/util/dist_log_entity.h
	@sort $(BUILDDIR)/pkg/util/dist_log_entity.h | uniq > $(BUILDDIR)/pkg/util/dist_log_entity.tmp
	@mv $(BUILDDIR)/pkg/util/dist_log_entity.tmp $(BUILDDIR)/pkg/util/dist_log_entity.h
endif

archconfig:: $(ARCHCONFIG_FIRST_TASKS) \
    export_headers $(if $(CONFIG_RG_JPKG_SRC),,export_libs) ln_internal_headers\
    $(ARCHCONFIG_SUBDIRS_) $(ARCHCONFIG_LAST_TASKS) dist_log_entity

make_dir_debug:
	$(MKDIR) $(DEBUG_PATH)

# One day we'll remove archconfig, until then...
config: archconfig

ALL_CLEAN_FILES=$(CLEAN) $(ALL_OBJS) $(ALL_PRODS) *.d
ALL_CLEAN_FILES+=$(foreach t,$(TARGET) $(SO_TARGET) $(MOD_TARGET),$(DEBUG_PATH)/$t $t.link.map)

ifeq ($(CONFIG_BINFMT_FLAT),y)
  ALL_CLEAN_FILES+=$(foreach t,$(TARGET),$t.elf.o) $(foreach t,$(TARGET),$t.gdb.o) $(FAST_RAM_OBJS) $(FAST_RAM_OBJS:%.fr=%.o)
endif

ALL_CLEAN_FILES:=$(filter-out $(NOT_FOR_CLEAN),$(ALL_CLEAN_FILES))

clean:: $(FIRST_CLEAN) $(CLEAN_SUBDIRS_) do_clean

do_clean:
	rm -rf $(ALL_CLEAN_FILES)

UNITTEST_OPT=$(if $(CONFIG_RG_VALGRIND_LOCAL_TARGET),$(VALGRIND_CMD))

RUN_UNITTEST_CMD=$(foreach t,$(RUN_UNITTEST),$(UNITTEST_SPAWNER) $(UNITTEST_OPT) ./local_$(t) &&) true

run_unittests: $(RUN_UNITTESTS_SUBDIRS_)
	$(if $(RUN_UNITTEST_DATA),$(RG_LN) $(addprefix $(PWD_SRC)/,$(RUN_UNITTEST_DATA)) .)
	$(if $(RUN_UNITTEST),$(RUN_UNITTEST_CMD))

run_tests: run_unittests

# All dependencies including all: Are in general_dep.mak
include $(RGSRC)/general_dep.mak

else # IS_BUILDDIR
# do not use default rules, so that we do not generate by mistake any output
# into the source directory (the non BUILDDIR directory).
# like running "make --no-builtin-rules" 
.SUFFIXES:

endif # IS_BUILDDIR

.PHONY: do_clean cd_to_builddir
