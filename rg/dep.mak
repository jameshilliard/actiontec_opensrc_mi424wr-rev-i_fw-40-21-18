# This file should never be included directly. It should be included only from 
# loop.mak (COMMAND_FILE variable).
# This file is used to define a set of rules from a list of arguments
# in a variable.

INDEX:=$(INDEX$(DEPTH))
$(INDEX): $(INDEX_DEP)
ifdef MAKE_DEBUG_DEP_MAK
$(warning "$(INDEX): $(INDEX_DEP)")
endif
