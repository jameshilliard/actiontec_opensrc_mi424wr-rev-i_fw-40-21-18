RGSRC=../../../../../../
include $(RGSRC)/envir.mak

LINKS=$(BUILDDIR)/os/linux-2.6/arch/mips/lib/memcpy_octeon.S \
  $(BUILDDIR)/os/linux-2.6/arch/mips/lib/iomap_cavium.c

ARCHCONFIG_FIRST_TASKS+=$(LINKS)

$(LINKS):
	$(MKDIR) $(dir $@)
	$(RG_CP_LN) $(PWD_SRC)/$(notdir $@) $@

include $(RGMK)
