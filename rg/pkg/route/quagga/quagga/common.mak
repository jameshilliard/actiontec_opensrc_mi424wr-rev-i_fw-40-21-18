DEFINES+=-DSYSCONFDIR=\"'/etc/'\"

CFLAGS+=-I$(BUILDDIR)/pkg/route/quagga/quagga \
 -I$(BUILDDIR)/pkg/route/quagga/quagga/lib
CFLAGS+= $(DEFINES) -DMULTIPATH_NUM=1 -DHAVE_CONFIG_H


LDFLAGS+=-L$(BUILDDIR)/pkg/route/quagga/quagga/lib
LDFLAGS+=-L$(BUILDDIR)/pkg/route/quagga/quagga
