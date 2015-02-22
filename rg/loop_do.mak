# This is a recursive for-each loop. It issues the command found
# in COMMAND_FILE on each element in FOR_EACH.

INDEX$(DEPTH):=$(firstword $(FOR_EACH$(DEPTH)))
WORD_COUNT$(DEPTH):=$(words $(FOR_EACH$(DEPTH)))
FOR_EACH$(DEPTH):=$(wordlist 2,$(WORD_COUNT$(DEPTH)),$(FOR_EACH$(DEPTH)))

include $(RGSRC)/$(COMMAND_FILE$(DEPTH))

ifneq ($(WORD_COUNT$(DEPTH)),1)
  include $(RGSRC)/loop_do.mak
endif
