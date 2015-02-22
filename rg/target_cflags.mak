# This files is used to create a 2 depth 'for' using the loop.mak logic.
# Create a TARGET_CFLAGS_obj.o variable which contains the CFLAGS_target
# created for each target.

COMMAND_FILE=variable.mak
FOR_EACH=$(O_OBJS_$(INDEX$(DEPTH))) $(OX_OBJS_$(INDEX$(DEPTH)))
INDEX_VARIABLE=TARGET_CFLAGS_$(INDEX$(DEPTH))
INDEX_VAL:=$(CFLAGS_$(INDEX$(DEPTH)))

include $(LOOP)
