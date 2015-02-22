RGSRC=../../../../../..
include $(RGSRC)/envir.mak

COMMON_CFLAGS+=-I.. -I../libfreeswan -I../libcrypto/include -I../libcrypto \
	-Wall -Wpointer-arith -Wstrict-prototypes

ifndef CONFIG_RG_GPL
ifeq ($(CONFIG_ADI_6843),y)
  ifneq ($(CONFIG_RG_OS_LINUX_26),y)
    VENDOR_SUFFIX_ENC=_adi
  endif
endif
ifeq ($(CONFIG_IPSEC_USE_MPC_CRYPTO),y)
  VENDOR_SUFFIX_ENC=_mpc
endif

ifeq ($(CONFIG_IPSEC_USE_DANUBE_CRYPTO),y)
  VENDOR_SUFFIX_ENC=_danube
# as soon as B45763 is resolved. Need to compare performance before setting.
# VENDOR_SUFFIX_AUTH=_danube
endif

ifeq ($(CONFIG_CADENCE_IPSEC2),y)
  VENDOR_SUFFIX_ENC=_cadence
  VENDOR_SUFFIX_AUTH=_cadence
  MOD_TARGET+=cadence_ipsec2.o
  O_OBJS_cadence_ipsec2.o=ipsec_alg_cadence.o cadence_ipsec2.o \
    cadence_ipsec2_itf.o cadence_ipsec2_reverse.o
endif
endif #CONFIG_RG_GPL

MOD_O_TARGET=ipsec_alg_static.o
O_OBJS_$(MOD_O_TARGET)=ipsec_alg_static_init.o

ifeq ($(CONFIG_IPSEC_ALG_MD5),m)
  MOD_TARGET+=ipsec_md5.o
  O_OBJS_ipsec_md5.o=ipsec_alg_md5$(VENDOR_SUFFIX_AUTH).o
  ifeq ($(VENDOR_SUFFIX_AUTH),)
    L_OBJS_ipsec_md5.o=libmd5/libmd5.o
  endif
  SUBDIRS+=libmd5
endif

ifeq ($(CONFIG_IPSEC_ALG_SHA1),m)
  MOD_TARGET+=ipsec_sha1.o
  O_OBJS_ipsec_sha1.o=ipsec_alg_sha1$(VENDOR_SUFFIX_AUTH).o
  ifeq ($(VENDOR_SUFFIX_AUTH),)
    L_OBJS_ipsec_sha1.o=libsha1/libsha1.o
  endif
  SUBDIRS+=libsha1
endif

ifeq ($(CONFIG_IPSEC_ALG_1DES),m)
  MOD_TARGET+=ipsec_1des.o
  O_OBJS_ipsec_1des.o=ipsec_alg_1des$(VENDOR_SUFFIX_ENC).o
  ifeq ($(VENDOR_SUFFIX_ENC),)
    L_OBJS_ipsec_1des.o=../libdes/libdes.o
  endif
endif

ifeq ($(CONFIG_IPSEC_ALG_3DES),m)
  MOD_TARGET+=ipsec_3des.o
  O_OBJS_ipsec_3des.o=ipsec_alg_3des$(VENDOR_SUFFIX_ENC).o
  ifeq ($(VENDOR_SUFFIX_ENC),)
    L_OBJS_ipsec_3des.o=../libdes/libdes.o
  endif
endif

ifeq ($(CONFIG_IPSEC_ALG_AES),m)
  MOD_TARGET+=ipsec_aes.o
# AES HW ENC for ADI does not work. When works - remove this if
  ifeq ($(CONFIG_ADI_6843),y)
    O_OBJS_ipsec_aes.o=ipsec_alg_aes.o
    L_OBJS_ipsec_aes.o=libaes/libaes.o
  else
    O_OBJS_ipsec_aes.o=ipsec_alg_aes$(VENDOR_SUFFIX_ENC).o
  endif
  ifeq ($(VENDOR_SUFFIX_ENC),)
    L_OBJS_ipsec_aes.o=libaes/libaes.o
  endif
  SUBDIRS+=libaes
endif

ifeq ($(CONFIG_IPSEC_ALG_NULL),m)
  MOD_TARGET+=ipsec_null.o
  O_OBJS_ipsec_null.o=ipsec_alg_null.o
endif

CFLAGS_$(MOD_O_TARGET)+=$(COMMON_CFLAGS)
MOD_CFLAGS+=$(COMMON_CFLAGS)

RAMDISK_MODULES_FILES+=$(MOD_TARGET)

include $(RGMK)

