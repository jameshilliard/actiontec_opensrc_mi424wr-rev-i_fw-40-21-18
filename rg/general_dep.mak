# This file creates all the dependencies between variable contents and should
# only be included from rg.mak.

# Create the $(TARGET) logic
ifndef CONFIG_RG_OS_VXWORKS
  ifdef BINFRMT_FILE
    include $(RGSRC)/$(BINFRMT_FILE)
  endif
endif


DEP_OBJS=$(_O_OBJS_$(INDEX)) $(_OX_OBJS_$(INDEX)) $(if $(JPKG_TARGET_$(INDEX)),$(_L_OBJS_$(INDEX)))

ifdef NORMAL_TARGETS
  COMMAND_FILE=dep.mak
  FOR_EACH=$(filter-out $(A_TARGET),$(NORMAL_TARGETS))
  INDEX_DEP=$(DEP_OBJS)
  include $(LOOP)
endif

ifdef MOD_TARGET
  COMMAND_FILE=dep.mak
  ifdef CONFIG_RG_OS_LINUX_24
    FOR_EACH=$(filter %_mod_24.o,$(foreach m,$(MOD_TARGET),$(_O_OBJS_$m) $(_OX_OBJS_$m)))
    INDEX_DEP=$(call GET_SRC,$(INDEX:%_mod_24.o=%))
    include $(LOOP)
  endif
  
  ifdef CONFIG_RG_OS_LINUX_26
    FOR_EACH=$(filter %_mod_26.o,$(foreach m,$(MOD_TARGET),$(_O_OBJS_$m) $(_OX_OBJS_$m)))
    INDEX_DEP=$(call GET_SRC,$(INDEX:%_mod_26.o=%))
    include $(LOOP)
  endif
endif

ifdef A_TARGETS
  COMMAND_FILE=archive.mak
  FOR_EACH=$(A_TARGETS)
  include $(LOOP)
endif

ifneq ($(LOCAL_TARGET)$(LOCAL_O_TARGET)$(LOCAL_CXX_TARGET),)
  COMMAND_FILE=dep.mak
  FOR_EACH=$(LOCAL_TARGET) $(LOCAL_O_TARGET) $(LOCAL_CXX_TARGET)
  INDEX_DEP=$(DEP_OBJS)
  include $(LOOP)
endif

# Adding the special objects for O_TARGET.
ifdef O_TARGET
  COMMAND_FILE=dep.mak
  FOR_EACH=$(O_TARGET)
  # Warning: sO_OBJS and FAST_RAM_OBJS aren't for multiple targets!
  INDEX_DEP=$(sO_OBJS) $(FAST_RAM_OBJS)
  include $(LOOP)
endif

ifdef sO_OBJS
# It seems like this could be done by '$(sO_OBJS): %.o : %.s' BUT:
# (1) We will have a problem when running in a binary distribution.
# (2) We can't just use '%.o : %.s' because usually %.s files stand
# for assembly files that should NOT go through precompilation and we 
# want to precompile only if the %.s file is in the sO_OBJS variable. 
# So, the solution is to use a recursive dependency and '$(sO_OBJS) : '
# instead (see rg.mak)
  COMMAND_FILE=dep.mak
  FOR_EACH=$(sO_OBJS)
  INDEX_DEP=$(INDEX:%.o=%.s)
  include $(LOOP)
endif

# All compiled objs depend on the makefile that creates them
ifdef ALL_OBJS
  COMMAND_FILE=dep.mak
  FOR_EACH=$(ALL_OBJS)
  INDEX_DEP=$(wildcard $(PWD_SRC)/Makefile)
  include $(LOOP)
endif

# DUMMY_VAR is used when we want to force make to calculate a $(),
# usually in case we want to $(error ....).
ALL_DEPS+=$(DUMMY_VAR) export_headers ln_internal_headers $(FIRST_TASKS) $(OTHER_DEPS) $(SUBDIRS_)

ifdef CONFIG_RG_JPKG_SRC
  ALL_DEPS+=$(ALL_OBJS) $(LOCAL_A_TARGET) $(LOCAL_O_TARGET) $(LOCAL_TARGET)
else
ifdef CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY
  ALL_DEPS+=$(ALL_LOCAL_OBJS) $(LOCAL_A_TARGET) $(LOCAL_O_TARGET) $(LOCAL_TARGET)
else
  ALL_DEPS+= $(OTHER_TARGETS) $(LOCAL_A_TARGET) $(LOCAL_O_TARGET) \
    $(LOCAL_TARGET) $(LOCAL_CXX_TARGET) $(OTHER_TASKS) $(O_TARGET) \
    $(MOD_TARGET) $(MOD_2_STAT_LINKS) $(A_TARGET) $(SO_TARGET) $(TARGET) \
    $(LAST_TASKS)
endif
endif

ifdef CONFIG_RG_JPKG
  ALL_DEPS+=$(JPKG_FIRST_TASKS) export_to_jpkg $(JPKG_LAST_TASKS)
endif

# One all rule to rule them all: Should be only in this file!
# BAD:
#     all: my_rule
# GOOD:
#     $(OTHER_TASKS)+=my_rule
ifdef IS_BUILDDIR
all: $(ALL_DEPS)
endif
