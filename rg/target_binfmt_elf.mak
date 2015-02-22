# Create the TARGET: O_OBJS and L_OBJS dependencies
COMMAND_FILE=dep.mak
FOR_EACH=$(TARGET)
INDEX_DEP=$(_O_OBJS_$(INDEX)) $(if $(JPKG_TARGET_$(INDEX)),$(_L_OBJS_$(INDEX)))
include $(LOOP)

LINKAGE_MAP_FLAGS=-Wl,-Map $@.link.map

$(TARGET): $(_O_OBJS_$@) $(if $(JPKG_TARGET_$@),$(_L_OBJS_$@))
	$(if $(JPKG_TARGET_$@),\
	  touch $@.link.map; \
	    $(CC) $^ $(call FIX_VPATH_LDFLAGS,$(filter-out $(LDFLAGS_REMOVE_$@),\
	    $(LDFLAGS)) $(LDFLAGS_$@) $(MAK_LDFLAGS)) $(LDLIBS) $(LDLIBS_$@) \
	    $(LINKAGE_MAP_FLAGS) -o $@,\
	  echo "Skipping TARGET($@:$^) since creating JPKG"\
	 )
	$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$@
