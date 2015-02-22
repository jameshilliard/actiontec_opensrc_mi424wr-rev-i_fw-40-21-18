RGSRC=../../
include $(RGSRC)/envir.mak
TARGET+=totd
INTERNAL_HEADERS+=acconfig.h  config.h  macros.h  protos.h  tot_constants.h  totd.h  tot_types.h
TOT_CONFIG_FILE=/etc/totd.conf
CFLAGS+=-I.  -DHAVE_CONFIG_H  -Werror -Wall -DTOTCONF=\"$(TOT_CONFIG_FILE)\" -DSCOPED_REWRITE -UUSE_INET4 -DUSE_INET6

O_OBJS_totd+=\
context.o      daemon.o     ev_timeout.o  inet_aton.o  queue.o        res_record.o   tcp_response.o \
conv_scoped.o  ev_dup.o     ev_udp_in.o   list.o       read_config.o  strlcat.o      totd.o \
conv_stf.o     ev_signal.o  forward.o     ne_io.o      request.o      strlcpy.o      udp_request.o \
conv_trick.o   ev_tcp.o     html.o        ne_mesg.o    response.o     tcp_request.o  udp_response.o 



RAMDISK_BIN_FILES+=$(TARGET)
include $(RGMK)

