RGSRC=../../../
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=syslog/glibc
EXPORT_HEADERS+=ulibc_syslog.h

A_TARGET=libulibc_syslog.a
O_OBJS_$(A_TARGET)=ulibc_syslog.o
EXPORT_LIBS+=$(A_TARGET)

include $(RGMK)
