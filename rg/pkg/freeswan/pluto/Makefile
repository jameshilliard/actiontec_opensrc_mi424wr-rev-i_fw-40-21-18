RGSRC=../../../
include $(RGSRC)/envir.mak

TARGET=pluto _pluto_adns
RAMDISK_BIN_FILES+=$(TARGET)

EXPORT_HEADERS_DIR=freeswan/pluto
EXPORT_HEADERS+=rg_utils.h constants.h defs.h whack.h packet.h ipsec_ipc.h

O_OBJS_pluto=asn1.o connections.o constants.o cookie.o crypto.o defs.o log.o \
	     state.o main.o server.o timer.o pem.o pkcs.o x509.o id.o \
	     ipsec_doi.o kernel.o kernel_comm.o demux.o packet.o preshared.o \
	     dnskey.o rnd.o spdb.o sha1.o md5.o md2.o rg_utils.o block_ip.o \
	     db_ops.o vendor.o dpd.o

# Current version of ARM compiler have some optimization bugs. Following
# ifeq will be removed after compiler update.
ifeq ($(ARCH),arm)
CFLAGS_demux.o+=-O1
endif

ifeq ($(CONFIG_RG_IPSEC_IKE_ALG),y)
SUBDIRS=alg
O_OBJS_pluto+=kernel_alg.o ike_alg.o alg_info.o alg/libalg.o
endif

O_OBJS_whack=whack.o
O_OBJS__pluto_adns=adns.o

KLIPSD=../klips/net/ipsec
# -O on Linux makes gcc coredump when compiling sha1.c
# -Wundef is nice but RHL5.2 compiler doesn't support it
CFLAGS+=-g -Wall -W -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast \
	-Wcast-qual -Wmissing-declarations -Wwrite-strings -Wstrict-prototypes \
	-I$(KLIPSD) -I../lib -I$(BUILDDIR)/pkg/gmp -I../libdes \
	-DPLUTO -DKLIPS -DDODGE_DH_MISSING_ZERO_BUG -DDEBUG -DGCC_LINT \
	-fno-builtin-log

ifeq ($(CONFIG_IPSEC_NAT_TRAVERSAL),y)
    O_OBJS_pluto+=nat_traversal.o
    CFLAGS+=-DNAT_TRAVERSAL
endif

# libefence is a free memory allocation debugger
LDFLAGS+=-L$(BUILDDIR)/pkg/gmp -L../libdes -L../lib
LDLIBS:=$(OPENRG_GPL_LDLIBS) -lgmp -ldes -lfreeswan $(LDLIBS)

OTHER_DEPS += $(OPENRG_GPL_LIB)

ifeq ($(CONFIG_RG_SYSLOG_GLIBC),y)
    CFLAGS += -include $(RGSRC)/pkg/syslog/glibc/ulibc_syslog.h
    LDFLAGS += -L$(RGSRC)/pkg/syslog/glibc/
    LDLIBS:=-lresolv -lulibc_syslog $(LDLIBS)
endif

include $(RGMK)

ifeq ($(CONFIG_RG_SYSLOG_GLIBC),y)
$(TARGET): $(BUILDDIR)/pkg/syslog/glibc/libulibc_syslog.a
endif

ifeq ($(CONFIG_RG_IPSEC_NO_SECRET_FILE),y)
CFLAGS+=-DNO_SECRET_FILE
endif
