# This file should never be included directly. It should be included only form 
# loop.mak (COMMAND_FILE).
# This file is used to define a set of new variables from a list of arguments
# in  an already existing variable.

INDEX:=$(INDEX$(DEPTH))
ifdef MAKE_DEBUG_VAR_MAK
$(warning "INDEX:=$(INDEX)")
endif
$(INDEX_VARIABLE):=$(INDEX_VAL)
ifdef MAKE_DEBUG_VAR_MAK
$(warning "INDEX_VARIABLE:=$(INDEX_VAL)")
endif
