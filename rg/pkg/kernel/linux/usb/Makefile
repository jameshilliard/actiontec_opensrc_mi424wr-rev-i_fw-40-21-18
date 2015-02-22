RGSRC=../../../..
include $(RGSRC)/envir.mak

TARGET_$(CONFIG_RG_USB)+=usbdrv.o
MOD_2_STAT_PREFIX_usbdrv.o=1

ifeq ($(CONFIG_USB_DEVICEFS),y)
  O_OBJS_usbdrv.o+=devio.o inode.o drivers.o devices.o
endif

ifdef CONFIG_USB_EHCI_HCD
  OX_OBJS_usbdrv.o+=hcd.o
endif

O_OBJS_usbdrv.o+=usb-debug.o
OX_OBJS_usbdrv.o+=usb.o hub.o usbled.o

TARGET_$(CONFIG_USB_OV511)+=ov511_mod.o
MOD_2_STAT_PREFIX_ov511_mod.o=2
OX_OBJS_ov511_mod.o+=ov511.o

TARGET_$(CONFIG_USB_PWC)+=pwc_mod.o
MOD_2_STAT_PREFIX_pwc_mod.o=2
OX_OBJS_pwc_mod.o+=pwc-if.o pwc-misc.o pwc-ctrl.o pwc-uncompress.o

ifdef CONFIG_USB_SPCA5XX
  SUBDIRS+=spca5xx
endif

TARGET_$(CONFIG_USB_PRINTER)+=printer_mod.o
MOD_2_STAT_PREFIX_printer_mod.o=2
OX_OBJS_printer_mod.o+=printer.o
ifdef CONFIG_CX8620X_EHCI
  CFLAGS_printer.o+=-DUSBLP_NO_BIDIR
endif

MOD_TARGET=$(TARGET_m) $(TARGET_y)
MOD_2_STAT=$(TARGET_y)

ifdef CONFIG_USB_STORAGE
  SUBDIRS+=storage
endif

SUBDIRS+=host

MOD_CFLAGS+=-I./include

include $(RGMK)
