RGSRC=../../../../..
include $(RGSRC)/envir.mak

O_TARGET=target.o

CFLAGS+=-DNDEBUG -I../../include -Wmissing-prototypes -fPIC -DP_USE_PRAGMA \
  -D_REENTRANT -Wall -I$(RGSRC)/pkg/voip/pwlib/include \
  -I$(RGSRC)/pkg/voip/openh323/include -Os -pipe

O_OBJS_$(O_TARGET)+=ast_h323.o

include $(RGMK)

