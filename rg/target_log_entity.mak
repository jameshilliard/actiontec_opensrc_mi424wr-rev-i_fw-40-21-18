# This files is used to create a 2 depth 'for' using the loop.mak logic.
# Create a TARGET_LOG_ENTITY_obj.o variable which contains the LOG_ENTITY_target
# created for each target.

COMMAND_FILE=variable.mak
FOR_EACH=$(O_OBJS_$(INDEX$(DEPTH))) $(OX_OBJS_$(INDEX$(DEPTH)))
INDEX_VARIABLE=TARGET_LOG_ENTITY_$(INDEX$(DEPTH))
INDEX_VAL:=$(LOG_ENTITY_$(INDEX$(DEPTH)))

include $(LOOP)
