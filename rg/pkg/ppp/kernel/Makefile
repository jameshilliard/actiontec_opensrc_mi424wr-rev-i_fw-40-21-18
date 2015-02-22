RGSRC=../../..
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=ppp/kernel
EXPORT_HEADERS+=be_api.h be_pppoe.h be_pppoh.h be_chardev.h be_pptp.h if_ppp.h
EXPORT_HEADERS+=rg_ppp_defs.h ppp-comp.h 
INTERNAL_HEADERS+=ppp_mppe.h if_pppvar.h
EXPORT_AS_SRC+=kernel.c ppp_compress.c

LOG_ENTITY_be_pptp.o=LOG_ENTITY_PPTP

CRYPTO_PATH=$(RGSRC)/pkg/openssl/crypto

CRYPTO_FILES=md32_common.h rc4/rc4.h rc4/rc4_locl.h
ifdef CONFIG_RG_OS_VXWORKS
  ifndef CONFIG_RG_OPENSSL
    CRYPTO_CODE_FILES+=rc4/rc4_enc.c rc4/rc4_skey.c
  endif
else
  CRYPTO_CODE_FILES+=rc4/rc4_enc.c rc4/rc4_skey.c
endif
CRYPTO_FILES+=$(CRYPTO_CODE_FILES)
CRYPTO_LINKS=$(notdir $(CRYPTO_FILES))

ifeq ($(CONFIG_RG_ONE_MODULE)$(CONFIG_RG_PPP_COMMON),mm)
  MOD_O_TARGET=ppp.o
else
  MOD_TARGET=ppp.o
  RAMDISK_MODULES_FILES+=ppp.o
endif

O_OBJS_ppp.o=be_chardev.o kernel.o ppp_compress.o
OX_OBJS_ppp.o+=if_ppp.o if_ppp_pppoa.o
EXPORT_AS_SRC+=if_ppp_pppoa.c

ifdef CONFIG_RG_PPPOE
  O_OBJS_ppp.o+=be_pppoe.o
endif

ifdef CONFIG_RG_PPTPC
  O_OBJS_ppp.o+=be_pptp.o
endif

ifdef CONFIG_RG_PPPOA
  MOD_TARGET+=be_pppoa_mod.o
  RAMDISK_MODULES_FILES+=be_pppoa_mod.o
  OX_OBJS_be_pppoa_mod.o=be_pppoa.o
endif

ifeq ($(CONFIG_RG_PPP_COMMON),y)
  MOD_2_STAT+=ppp.o
endif

ifeq ($(CONFIG_RG_PPPOA),y)
  MOD_2_STAT+=be_pppoa_mod.o
endif

# PPP compressors
ifneq ($(CONFIG_RG_PPP_DEFLATE)$(CONFIG_RG_PPP_BSDCOMP)$(CONFIG_RG_PPP_MPPE),)
  MOD_CFLAGS+=-DPPP_COMPRESS
endif

ifneq ($(CONFIG_RG_PPP_DEFLATE)$(CONFIG_RG_PPP_BSDCOMP),)
  ifdef CONFIG_RG_PPP_DEFLATE
    MOD_CFLAGS+=-DPPP_COMPRESS_DEFLATE
    O_OBJS_ppp.o+=ppp-deflate.o
  endif
  ifdef CONFIG_RG_PPP_BSDCOMP
    MOD_CFLAGS+=-DPPP_COMPRESS_BSDCOMP
    O_OBJS_ppp.o+=bsd-comp.o
  endif
endif

ifdef CONFIG_RG_PPP_MPPE
  O_OBJS_ppp.o+=ppp_mppe.o
  ifneq ($(CRYPTO_CODE_FILES),)
    O_OBJS_ppp.o+=$(notdir $(CRYPTO_CODE_FILES:%.c=%.o))
  endif
  MOD_CFLAGS+=-DPPP_COMPRESS_MPPE -I$(CRYPTO_PATH)
  ARCHCONFIG_FIRST_TASKS+=crypto_links
endif

ifdef CONFIG_RG_HSS
  O_OBJS_ppp.o+=be_pppoh.o
  MOD_CFLAGS+=-I$(RGSRC)/vendor/intel/ixp425/csr/include \
	-I$(RGSRC)/vendor/intel/ixp425/modules
endif

ifdef CONFIG_RG_GPL
MOD_TARGET:=$(filter be_pppoa_mod.o,$(MOD_TARGET))
MOD_O_TARGET=
MOD_2_STAT=
endif

include $(RGMK)

crypto_links: $(addprefix $(CRYPTO_PATH)/, $(CRYPTO_FILES))
	@echo "Generating MPPE openssl/crypto links:"
	@for f in $^ ; do \
	  $(RG_LN) $$f ./ ; \
	  echo $$f ; \
	done

ctags:
	ctags -R $(RGSRC)/os/linux $(RGSRC)/pkg/kos $(RGSRC)/pkg/include .
