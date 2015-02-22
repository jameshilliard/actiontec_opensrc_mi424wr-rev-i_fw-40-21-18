RGSRC=../../../../../../../
include $(RGSRC)/envir.mak

MOD_O_TARGET=libmd5.o

CFLAGS_$(MOD_O_TARGET)+=-I../../libcrypto/include -DNO_ASM
O_OBJS_$(MOD_O_TARGET)=md5_dgst.o md5_one.o hmac_md5.o

include $(RGMK)
