RGSRC=../../
include $(RGSRC)/envir.mak

termcapfile=/etc/termcap
A_TARGET=libtermcap.a
O_OBJS=termcap.o tparam.o version.o

CFLAGS+=-DHAVE_STRING_H=1 -DHAVE_UNISTD_H=1 -DSTDC_HEADERS=1 \
	-DTERMCAP_FILE=\"$(termcapfile)\"

EXPORT_LIBS+=$(A_TARGET)
EXPORT_HEADERS+=termcap.h

include $(RGMK)
