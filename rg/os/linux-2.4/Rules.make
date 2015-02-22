#
# This file contains rules which are shared between multiple Makefiles.
#

#
# False targets.
#
.PHONY: dummy

#
# Special variables which should not be exported
#
unexport EXTRA_AFLAGS
unexport EXTRA_CFLAGS
unexport EXTRA_LDFLAGS
unexport EXTRA_ARFLAGS
unexport SUBDIRS
unexport SUB_DIRS
unexport ALL_SUB_DIRS
unexport MOD_SUB_DIRS
unexport O_TARGET
unexport ALL_MOBJS
unexport PREMOBJS

unexport obj-y
unexport obj-m
unexport obj-n
unexport obj-
unexport export-objs
unexport subdir-y
unexport subdir-m
unexport subdir-n
unexport subdir-

comma	:= ,
EXTRA_CFLAGS+=-D__OS_LINUX__
EXTRA_CFLAGS_nostdinc := $(EXTRA_CFLAGS) $(kbuild_2_4_nostdinc)

RGSRC=$(shell cat $(TOPDIR)/../../.rgsrc)
include $(RGSRC)/copy_db_envir.mak

#
# Get things started.
#
first_rule: sub_dirs
	$(MAKE) all_targets

both-m          := $(filter $(mod-subdirs), $(subdir-y))
SUB_DIRS	:= $(subdir-y)
MOD_SUB_DIRS	:= $(sort $(subdir-m) $(both-m))
ALL_SUB_DIRS	:= $(sort $(subdir-y) $(subdir-m) $(subdir-n))

#
# Common rules
#

%.s: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$(*F)) $(CFLAGS_$@) -S $< -o $@

%.i: %.c
	$(CPP) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$(*F)) $(CFLAGS_$@) $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$(*F)) $(CFLAGS_$@) -c -o $@ $<

%.o: %.s
	$(AS) $(AFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# Old makefiles define their own rules for compiling .S files,
# but these standard rules are available for any Makefile that
# wants to use them.  Our plan is to incrementally convert all
# the Makefiles to these standard rules.  -- rmk, mec
ifdef USE_STANDARD_AS_RULE

%.s: %.S
	$(CPP) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) $< > $@

%.o: %.S
	$(CC) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<

endif

%.lst: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) $(CFLAGS_$@) -g -c -o $*.o $<
	$(TOPDIR)/scripts/makelst $* $(TOPDIR) $(OBJDUMP)
#
#
#
all_targets: $(O_TARGET) $(L_TARGET)

#
# Rule to compile a set of .o files into one .o file
#
ifdef O_TARGET
$(O_TARGET): $(obj-y)
	rm -f $@
    ifneq "$(strip $(obj-y))" ""
	$(LD) $(EXTRA_LDFLAGS) -r -o $@ $(filter $(obj-y), $^)
    else
	$(AR) rcs $@
    endif
endif # O_TARGET

#
# Rule to compile a set of .o files into one .a file
#
ifdef L_TARGET
$(L_TARGET): $(obj-y)
	rm -f $@
	$(AR) $(EXTRA_ARFLAGS) rcs $@ $(obj-y)
endif


#
# A rule to make subdirectories
#
subdir-list = $(sort $(patsubst %,_subdir_%,$(SUB_DIRS)))
sub_dirs: dummy $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	$(MAKE) -C $(patsubst _subdir_%,%,$@)
endif

#
# A rule to make modules
#
ALL_MOBJS = $(filter-out $(obj-y), $(obj-m))
ifneq "$(strip $(ALL_MOBJS))" ""
MOD_DESTDIR := $(shell $(CONFIG_SHELL) $(TOPDIR)/scripts/pathdown.sh)
endif

unexport MOD_DIRS
MOD_DIRS := $(MOD_SUB_DIRS) $(MOD_IN_SUB_DIRS)
ifneq "$(strip $(MOD_DIRS))" ""
.PHONY: $(patsubst %,_modsubdir_%,$(MOD_DIRS))
$(patsubst %,_modsubdir_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modsubdir_%,%,$@) modules

.PHONY: $(patsubst %,_modinst_%,$(MOD_DIRS))
$(patsubst %,_modinst_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modinst_%,%,$@) modules_install
endif

.PHONY: modules
modules: dummy $(ALL_MOBJS) $(PREMOBJS) \
	 $(patsubst %,_modsubdir_%,$(MOD_DIRS))
         

.PHONY: _modinst__
_modinst__: dummy
ifneq "$(strip $(ALL_MOBJS))" ""
	mkdir -p $(MODLIB)/kernel/$(MOD_DESTDIR)
	cp $(sort $(ALL_MOBJS)) $(MODLIB)/kernel/$(MOD_DESTDIR)
endif

.PHONY: modules_install
modules_install: _modinst__ \
	 $(patsubst %,_modinst_%,$(MOD_DIRS))

#
# A rule to do nothing
#
dummy:

#
# This is useful for testing
#
script:
	$(SCRIPT)

#
# This sets version suffixes on exported symbols
# Separate the object into "normal" objects and "exporting" objects
# Exporting objects are: all objects that define symbol tables
#
ifdef CONFIG_MODULES

multi-used	:= $(filter $(list-multi), $(obj-y) $(obj-m))
multi-objs	:= $(foreach m, $(multi-used), $($(basename $(m))-objs))
active-objs	:= $(sort $(multi-objs) $(obj-y) $(obj-m))

# XXX: We probebly don't need the modversion logic at all
ifdef CONFIG_MODVERSIONS
ifneq "$(strip $(export-objs))" ""

MODINCL = $(TOPDIR)/include/linux/modules

ifeq ($(CONFIG_DEBUG_INFO),y)
	EXTRA_CFLAGS+=-g
endif

# The -w option (enable warnings) for genksyms will return here in 2.1
# So where has it gone?
#
# Added the SMP separator to stop module accidents between uniprocessor
# and SMP Intel boxes - AC - from bits by Michael Chastain
#

ifdef CONFIG_SMP
	genksyms_smp_prefix := -p smp_
else
	genksyms_smp_prefix := 
endif

$(MODINCL)/%.ver: %.c
	@if [ ! -r $(MODINCL)/$*.stamp -o $(MODINCL)/$*.stamp -ot $< ]; then \
		echo '$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -E -D__GENKSYMS__ $<'; \
		echo '| $(GENKSYMS) $(genksyms_smp_prefix) -k $(VERSION).$(PATCHLEVEL).$(SUBLEVEL) > $@.tmp'; \
		$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -E -D__GENKSYMS__ $< \
		| $(GENKSYMS) $(genksyms_smp_prefix) -k $(VERSION).$(PATCHLEVEL).$(SUBLEVEL) > $@.tmp; \
		if [ -r $@ ] && cmp -s $@ $@.tmp; then echo $@ is unchanged; rm -f $@.tmp; \
		else echo mv $@.tmp $@; mv -f $@.tmp $@; fi; \
	fi; touch $(MODINCL)/$*.stamp
	
$(addprefix $(MODINCL)/,$(export-objs:.o=.ver)): $(TOPDIR)/include/linux/autoconf.h

# updates .ver files but not modversions.h
fastdep: $(addprefix $(MODINCL)/,$(export-objs:.o=.ver))

endif # export-objs 

endif # CONFIG_MODVERSIONS

ifneq "$(strip $(export-objs))" ""

COMMAND_FILE=dep.mak
FOR_EACH=$(export-objs)
INDEX_DEP=$(INDEX:%.o=%.c)
include $(LOOP)

$(export-objs):
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) -DEXPORT_SYMTAB -c $(@:.o=.c)
endif

endif # CONFIG_MODULES


#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

ifneq ($(wildcard $(TOPDIR)/.hdepend),)
include $(TOPDIR)/.hdepend
endif

$(USER_OBJS) : %.o: %.c
	$(CC_FOR_BUILD) $(CFLAGS_$@) $(USER_CFLAGS) -c -o $@ $<

unexport CLEAN_TARGETS
unexport CLEAN
unexport CLEAN_EXTRA

CLEAN_EXTRA=$(filter-out $(NOT_FOR_CLEAN),$(shell find . -follow -name '*.[oas]' ! \( -regex '.*lxdialog/.*' -o -regex '.*ksymoops/.*' \) -maxdepth 1 -print))

clean: $(filter-out $(NOT_FOR_CLEAN),$(CLEAN_TARGETS) $(patsubst %,_clean_%,$(SUB_DIRS)) $(patsubst %,_clean_%,$(SUBDIRS)) $(patsubst %,_clean_%,$(MOD_DIRS)))
ifdef CLEAN
	rm -rf $(filter-out $(NOT_FOR_CLEAN),$(CLEAN))
endif
	@echo "standard clean: `pwd`"
	@rm -f __dummy $(CLEAN_EXTRA)
	@rm -f __dummy `find . -follow -type f -name 'core' -maxdepth 1 ! \( -type d \) -print`
	@rm -f __dummy `find . -follow -name '.*.flags' -maxdepth 1 -print`

$(sort $(patsubst %,_clean_%,$(SUB_DIRS)) $(patsubst %,_clean_%,$(SUBDIRS)) $(patsubst %,_clean_%,$(MOD_DIRS))) : dummy
	@if [ -e $(patsubst _clean_%,%,$@)/Makefile ]; then \
	echo "$(MAKE) -C $(patsubst _clean_%,%,$@) clean" ; \
	$(MAKE) -C $(patsubst _clean_%,%,$@) clean ; \
	fi

RG_ALL_OBJS=$(obj-y) $(obj-m)
include $(RGSRC)/os/Rules_common.make
