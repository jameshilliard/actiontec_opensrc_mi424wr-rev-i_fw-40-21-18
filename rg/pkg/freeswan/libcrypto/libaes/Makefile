RGSRC=../../../../../
include $(RGSRC)/envir.mak

A_TARGET=libaes.a
CFLAGS_$(A_TARGET)+=-I$(RGSRC)/pkg/freeswan/libcrypto/include
O_OBJS_$(A_TARGET)=aes_cbc.o aes.o

include $(RGMK)

