# The real work is in the loop_do.mak. This level increments the DEPTH 
# variable which makes the loop logic recusive (see target_cflags.mak for
# example).

FOR_EACH:=$(strip $(FOR_EACH))
ifneq ($(FOR_EACH),)
  PREDEPTH:=$(DEPTH)
  DEPTH:=$(DEPTH)6
  FOR_EACH$(DEPTH):=$(FOR_EACH)
  COMMAND_FILE$(DEPTH):=$(COMMAND_FILE)
  include $(RGSRC)/loop_do.mak
  DEPTH:=$(PREDEPTH)
endif
