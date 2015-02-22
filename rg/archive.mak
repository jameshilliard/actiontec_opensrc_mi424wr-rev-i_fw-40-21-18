# This file should never be included directly. It should be included only from 
# loop.mak (COMMAND_FILE variable).
# This file is used to define a set of dependencies for libraries.

INDEX:=$(INDEX$(DEPTH))
__create_lib_$(INDEX): $(_O_OBJS_$(INDEX)) $(if $(JPKG_TARGET_$(INDEX)),$(_L_OBJS_$(INDEX)))

$(INDEX): __create_lib_$(INDEX)
