RGSRC=../../../../../../../
include $(RGSRC)/envir.mak

MOD_O_TARGET=libsha1.o
CFLAGS_$(MOD_O_TARGET)+=-I../../libcrypto/include -DNO_ASM
O_OBJS_$(MOD_O_TARGET)=sha_dgst.o sha1_one.o hmac_sha1.o

include $(RGMK)
