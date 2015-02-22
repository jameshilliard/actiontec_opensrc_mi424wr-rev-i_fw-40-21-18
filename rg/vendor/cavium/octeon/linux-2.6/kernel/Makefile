RGSRC=../../../../../
include $(RGSRC)/envir.mak

LINKS=$(BUILDDIR)/os/linux-2.6/arch/mips/cavium-octeon \
  $(BUILDDIR)/os/linux-2.6/include/asm-mips/mach-cavium-octeon \
  $(BUILDDIR)/os/linux-2.6/drivers/usb/host/dwc_otg

ARCHCONFIG_FIRST_TASKS+=$(LINKS)

SUBDIRS=kernel lib mm oprofile

EXPORT_HEADERS_DIR=vendor/kernel
EXPORT_HEADERS+=cavium-octeon/hal.h__/hal.h

$(LINKS):
	$(MKDIR) $(dir $@)
	$(RG_CP_LN) $(PWD_SRC)/$(notdir $@) $@

include $(RGMK)
