# This files is used to create a 2 depth 'for' using the loop.mak logic.
# Create a JPKG_TARGET_obj.o=y variable for each target which
# JPKG_TARGET_target=y.

COMMAND_FILE=variable.mak
FOR_EACH=$(_O_OBJS_$(INDEX$(DEPTH))) $(_OX_OBJS_$(INDEX$(DEPTH)))
INDEX_VARIABLE=JPKG_TARGET_$(INDEX$(DEPTH))
INDEX_VAL:=y

include $(LOOP)
