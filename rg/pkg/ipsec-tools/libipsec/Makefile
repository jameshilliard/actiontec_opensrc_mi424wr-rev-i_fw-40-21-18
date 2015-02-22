RGSRC= ../../../
include $(RGSRC)/envir.mak

CFLAGS+=-I../include-glibc/ -I../ -DHAVE_CONFIG_H -I./

SO_TARGET=libipsec.so
TARGET=test-policy

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES=$(SO_TARGET)

L_OBJS_$(SO_TARGET) =ipsec_dump_policy.o  ipsec_get_policylen.o  ipsec_strerror.o  key_debug.o  pfkey_dump.o  pfkey.o  policy_parse.o policy_token.o
O_OBJS_$(TARGET) =test-policy.o
LDFLAGS_$(TARGET) += -lipsec
CFLAGS_$(TARGET) +=-DIP_IPSEC_POLICY=16


include $(RGMK)
