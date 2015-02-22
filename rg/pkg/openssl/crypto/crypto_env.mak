OPENSSL_DIR=$(RGSRC)/pkg/openssl
include $(OPENSSL_DIR)/openssl_env.mak

INCLUDES=-I$(OPENSSL_DIR) -I$(OPENSSL_DIR)/crypto
CFLAGS+=$(INCLUDES)
LOCAL_CFLAGS+=$(INCLUDES)

# libcrypto.so is generated from libcrypto.a - this bypasses the standard
# SO_TARGET rules, so the -fpic flag must be added manually 
CFLAGS+=-fpic

ifneq ($(CONFIG_RG_GPL)-$(CONFIG_RG_CRYPTO),y-)
 A_TARGET+=$(BUILDDIR)/pkg/openssl/crypto/libcrypto.a
endif

CREATE_LOCAL=$(A_TARGET)

