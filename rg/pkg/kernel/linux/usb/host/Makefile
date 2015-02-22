RGSRC=../../../../..
include $(RGSRC)/envir.mak

TARGET_$(CONFIG_USB_EHCI_HCD)+=ehci-hcd_mod.o
MOD_2_STAT_PREFIX_ehci-hcd_mod.o=3
O_OBJS_ehci-hcd_mod.o+=ehci-hcd.o
ifdef CONFIG_CX8620X_EHCI
  CFLAGS_ehci-hcd.o+=-DNONPCI_EHCI -DQTD_STATUS_WA
  ARCHCONFIG_FIRST_TASKS+=cx8620x_ehci.c
cx8620x_ehci.c:
	$(RG_LN) $(RG_INCLUDE_DIR)/vendor/c/$@
endif

TARGET_$(CONFIG_USB_UHCI)+=usb-uhci_mod.o
MOD_2_STAT_PREFIX_usb-uhci_mod.o=3
O_OBJS_usb-uhci_mod.o+=usb-uhci.o

TARGET_$(CONFIG_USB_OHCI)+=usb-ohci_mod.o
MOD_2_STAT_PREFIX_usb-ohci_mod.o=3
O_OBJS_usb-ohci_mod.o+=usb-ohci.o

MOD_TARGET=$(TARGET_y) $(TARGET_m)
MOD_2_STAT+=$(TARGET_y)

RAMDISK_MODULES_FILES+=$(TARGET_m)

MOD_CFLAGS+=-I../include

include $(RGMK)
