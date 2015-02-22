RGSRC=../../../../
include $(RGSRC)/envir.mak

O_TARGET=libalg.o

FREESWANDIR=$(RGSRC)/pkg/freeswan
LIBCRYPTO=$(FREESWANDIR)/libcrypto

CFLAGS+=-I.. -I$(FREESWANDIR)/lib -I$(FREESWANDIR)/libdes -I$(LIBCRYPTO)

O_OBJS=ike_alginit.o

ifeq ($(CONFIG_IPSEC_ENC_1DES),y)
  O_OBJS+=ike_alg_1des.o
endif

ifeq ($(CONFIG_IPSEC_ENC_AES),y)
  SUBDIRS+=libaes
  O_OBJS+=ike_alg_aes.o
  LDFLAGS_$(O_TARGET)+=-Llibaes -laes
endif

include $(RGMK)
