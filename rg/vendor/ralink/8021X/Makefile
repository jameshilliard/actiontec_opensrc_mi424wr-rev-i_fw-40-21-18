RGSRC=../../..

include $(RGSRC)/envir.mak

INTERNAL_HEADERS+=rt2500apd.h eloop.h ieee802_1x.h eapol_sm.h ap.h sta_info.h \
  radius_client.h config.h common.h radius.h md5.h

TARGET=rt2500apd

O_OBJS_$(TARGET) = rt2500apd.o eloop.o eapol_sm.o radius.o md5.o config.o \
                   ieee802_1x.o sta_info.o radius_client.o

RAMDISK_BIN_FILES += $(TARGET)

ifdef CONFIG_RALINK_8021X_DEBUG
  CFLAGS_$(TARGET) += -DDBG
endif

include $(RGMK)
