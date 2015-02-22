ifndef CONFIG_BINFMT_FLAT
  $(error CONFIG_BINFMT_FLAT is not defined, but using its logic!)
endif

COMMAND_FILE=dep.mak
FOR_EACH=$(TARGET)
# Create the TARGET: TARGET.elf dependencies
INDEX_DEP=$(INDEX).elf.o
include $(LOOP)

# Create the TARGET.elf: O_OBJS_TARGET dependencies
FOR_EACH=$(TARGET:%=%.elf.o)
INDEX_DEP=$(_O_OBJS_$(INDEX:%.elf.o=%)) $(_L_OBJS_$(INDEX:%.elf.o=%))
include $(LOOP)

# TODO implement support for zipped files
#LDFLAT_FLAGS+=-z

# Initialize executable stack to default of 4KB
FLAT_STACK_SIZE=4096

FLTFLAGS=$(LDFLAT_FLAGS)

# ld running elf2flt uses FLTFLAGS so they need to be exported
export FLTFLAGS

$(TARGET):
	$(if $(JPKG_TARGET_$@),\
	$(CC) $(_O_OBJS_$@) $(_L_OBJS_$@) $(filter-out $(LDFLAGS_REMOVE_$@),\
	$(LDFLAGS)) $(LDFLAGS_$@) $(MAK_LDFLAGS) $(LDLIBS) $(LDLIBS_$@) -o \
	$@ && $(FLTHDR) -s \
	$(if $(FLAT_STACK_SIZE_$@),$(FLAT_STACK_SIZE_$@),$(FLAT_STACK_SIZE))\
	$@,\
	echo "Skipping TARGET($@:$^) Since JPKG is on")
	@$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$@
