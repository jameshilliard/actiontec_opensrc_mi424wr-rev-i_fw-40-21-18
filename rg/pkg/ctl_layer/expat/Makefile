include ../global.def

CFLAGS+=-I. 
#expat.h

LIB_A=libexpat.a
LIB_SO=libexpat.so
OBJS=xmlparse.o xmlrole.o xmltok.o

#config.h xmltok.h xmlrole.h ascii.h nametab.h xmltok_impl.h \
#  xmltok_impl.c asciitab.h utf8tab.h iasciitab.h latin1tab.h xmltok_ns.c

all: $(LIB_A)

ifeq ($(TARGET_PLATFORM),BHR2)
$(LIB_A): $(OBJS)
	$(AR) crv $@ $^ $(LDFLAGS)
.c.o:
	$(RCC) -c -o $@ $(CFLAGS) $(IFLAGS) $<

else
$(LIB_A): $(OBJS)
	$(AR) -rcs $@ $^
endif

clean:
	rm $(OBJS) $(LIB_A)

#include $(BUILD_DIR)/make.deprules

#-include $(OBJS:.o=.d)
