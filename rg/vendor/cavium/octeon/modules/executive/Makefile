RGSRC=../../../../..
include $(RGSRC)/envir.mak

INTERNAL_HEADERS+=$(notdir $(wildcard $(PWD_SRC)/*.*)) \
  cvmx-malloc/arena.c cvmx-malloc/malloc.c cvmx-malloc/malloc.h \
  cvmx-malloc/thread-m.h

include $(RGMK)
