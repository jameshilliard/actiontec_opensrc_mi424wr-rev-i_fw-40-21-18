RGSRC=../../../
include $(RGSRC)/envir.mak

ifdef CONFIG_DYN_LINK
  SO_TARGET = libfreeswan.so
  LIB = $(SO_TARGET)
  RAMDISK_LIB_FILES+=$(SO_TARGET)
else
  A_TARGET = libfreeswan.a
  LIB = $(A_TARGET)
endif

OBJS=ultoa.o addrtoa.o subnettoa.o subnetof.o goodmask.o datatot.o \
  prng.o rangetoa.o satoa.o pfkey_v2_parse.o pfkey_v2_build.o \
  pfkey_v2_ext_bits.o pfkey_v2_debug.o 

O_OBJS_$(LIB)=$(OBJS) addrtot.o addrtypeof.o anyaddr.o atoaddr.o \
  atoasr.o atosa.o atosubnet.o atoul.o initaddr.o initsaid.o initsubnet.o \
  keyblobtoid.o optionsfrom.o portof.o rangetosubnet.o sameaddr.o satot.o \
  subnettot.o subnettypeof.o ttoaddr.o ttodata.o ttosa.o ttosubnet.o ttoul.o \
  ultot.o ttoprotoport.o version.o copyright.o

CFLAGS+=-O3 -I. -I../libdes -I../../gmp -Wbad-function-cast -Wpointer-arith -Wcast-qual -Wstrict-prototypes
ifeq ($(CONFIG_IPSEC_NAT_TRAVERSAL),y)
    CFLAGS+=-DNAT_TRAVERSAL
endif
LDFLAGS+=-L../libdes/
LDLIBS:=-ldes $(LDLIBS)

override CFLAGS+=-I. -Wall -Wconversion -Wmissing-prototypes -Wpointer-arith \
		 -Wcast-qual -Wmissing-declarations -Wstrict-prototypes \
		 -Wbad-function-cast 

EXPORT_HEADERS+=freeswan.h
#header files are included with <> so the automatic dependancy will not get 
#them
CD_EXPORTED_FILES=Makefile.kernel $(call VPATH_WILDCARD,*.[ch])

include $(RGMK)

