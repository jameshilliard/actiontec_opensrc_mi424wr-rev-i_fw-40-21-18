RGSRC=../../../../../../
include $(RGSRC)/envir.mak

LINKS=$(BUILDDIR)/os/linux-2.6/arch/mips/oprofile/op_model_cavium_octeon.c

ARCHCONFIG_FIRST_TASKS+=$(LINKS)

$(LINKS):
	$(MKDIR) $(dir $@)
	$(RG_CP_LN) $(PWD_SRC)/$(notdir $@) $@

include $(RGMK)
