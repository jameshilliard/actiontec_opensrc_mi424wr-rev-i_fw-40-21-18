# Usage:
#   PATH_UP_START:=a/b/c/d
#   include path_up_recursive.mak
# Result:
#   PATH_UP_LIST=a a/b a/b/c a/b/c/d
ifneq ($(strip $(PATH_UP_START)),)
  PATH_UP_LIST:=$(PATH_UP_START) $(PATH_UP_LIST)
  PATH_UP_START:=$(call PATH_UP,$(PATH_UP_START))
  include $(RGSRC)/path_up_recursive.mak
endif

# the following only works on make 3.80 and above:
# 
# The definition:
# PATH_UP_RECURSIVE=$(strip $(if $1,$(call PATH_UP_RECURSIVE,$(call PATH_UP,$1)),) $1)
#
# The usage:
# $(call PATH_UP_RECURSIVE,./$(PWD_REL))
#
# The reason is that earlier versions of make complain about recursive 
# function calls.

