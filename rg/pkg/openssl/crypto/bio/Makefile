RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=bio.h
INTERNAL_HEADERS=bio_lcl.h

O_OBJS= bio_lib.o bio_cb.o bio_err.o \
	bss_mem.o bss_null.o bss_fd.o \
	bss_file.o bss_sock.o bss_conn.o \
	bf_null.o bf_buff.o b_print.o b_dump.o \
	b_sock.o bss_acpt.o bf_nbio.o bss_log.o bss_bio.o bss_dgram.o

include $(RGMK)

