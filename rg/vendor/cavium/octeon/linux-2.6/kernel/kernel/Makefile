RGSRC=../../../../../../
include $(RGSRC)/envir.mak

LINKS=$(BUILDDIR)/os/linux-2.6/arch/mips/kernel/irq-octeon.c \
  $(BUILDDIR)/os/linux-2.6/arch/mips/kernel/octeon_switch.S

ARCHCONFIG_FIRST_TASKS+=$(LINKS)

$(LINKS):
	$(MKDIR) $(dir $@)
	$(RG_CP_LN) $(PWD_SRC)/$(notdir $@) $@

include $(RGMK)
