RGSRC=../../../
include $(RGSRC)/envir.mak
OPENSSL_DIR=$(RGSRC)/pkg/openssl
include $(OPENSSL_DIR)/openssl_env.mak

INCLUDES=-I$(OPENSSL_DIR) -I$(OPENSSL_DIR)/crypto
CFLAGS+=$(INCLUDES)
LOCAL_CFLAGS+=$(INCLUDES)

O_OBJS=	s2_meth.o s2_srvr.o s2_clnt.o s2_lib.o s2_enc.o s2_pkt.o \
	s3_meth.o s3_srvr.o s3_clnt.o s3_lib.o s3_enc.o s3_pkt.o s3_both.o \
	s23_meth.o s23_srvr.o s23_clnt.o s23_lib.o s23_pkt.o \
	t1_meth.o t1_srvr.o t1_clnt.o t1_lib.o t1_enc.o \
	d1_meth.o d1_srvr.o d1_clnt.o d1_lib.o d1_pkt.o \
	d1_both.o d1_enc.o \
	ssl_lib.o ssl_err2.o ssl_cert.o ssl_sess.o \
	ssl_ciph.o ssl_stat.o ssl_rsa.o \
	ssl_asn1.o ssl_txt.o ssl_algs.o \
	bio_ssl.o ssl_err.o kssl.o

EXPORT_HEADERS=ssl.h ssl2.h ssl3.h ssl23.h tls1.h dtls1.h kssl.h
INTERNAL_HEADERS=ssl_locl.h kssl_lcl.h

ifdef CONFIG_DYN_LINK
  SO_TARGET=libssl.so
  EXPORT_LIBS=$(SO_TARGET)
  RAMDISK_LIB_FILES+=$(SO_TARGET)
else
  A_TARGET=libssl.a
  EXPORT_LIBS=$(A_TARGET)
endif
LOCAL_A_TARGET=liblocal_ssl.a
EXPORT_LIBS+=$(LOCAL_A_TARGET)

include $(RGMK)

