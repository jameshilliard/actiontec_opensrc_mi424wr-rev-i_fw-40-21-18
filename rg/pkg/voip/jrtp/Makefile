RGSRC=../../../
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=rtp
EXPORT_HEADERS+=rtp.h

ifndef CONFIG_RG_GPL
INTERNAL_HEADERS+=rtp_p.h
MOD_TARGET=rtp.o
RAMDISK_MODULES_FILES+=$(MOD_TARGET)
OX_OBJS_rtp.o=rtp_dev.o
O_OBJS_rtp.o=net.o ioctl.o rtp_prot.o rtcp_prot.o write_buf.o

ifeq ($(CONFIG_RG_JRTP),y)
  MOD_2_STAT+=$(MOD_TARGET)
endif
endif

include $(RGMK)
