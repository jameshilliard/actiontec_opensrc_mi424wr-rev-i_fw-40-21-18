RGSRC=../../../../..
include $(RGSRC)/envir.mak

TARGET_$(CONFIG_USB_STORAGE)=usb-storage.o
MOD_2_STAT_PREFIX_usb-storage.o=2

O_OBJS_usb-storage.o=scsiglue.o protocol.o transport.o usb.o initializers.o

ifdef CONFIG_USB_STORAGE_DEBUG
  O_OBJS_usb-storage.o+=debug.o
endif

MOD_TARGET=$(TARGET_y) $(TARGET_m)
MOD_2_STAT=$(TARGET_y)

MOD_CFLAGS+=-I../include -I$(RGSRC)/os/linux/drivers/scsi

include $(RGMK)
