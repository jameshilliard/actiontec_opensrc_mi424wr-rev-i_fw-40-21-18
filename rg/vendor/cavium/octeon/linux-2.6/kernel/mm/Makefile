RGSRC=../../../../../../
include $(RGSRC)/envir.mak

LINKS=$(BUILDDIR)/os/linux-2.6/arch/mips/mm/c-octeon.c \
  $(BUILDDIR)/os/linux-2.6/arch/mips/mm/hugetlbpage.c \
  $(BUILDDIR)/os/linux-2.6/arch/mips/mm/pg-octeon.c \
  $(BUILDDIR)/os/linux-2.6/arch/mips/mm/cex-oct.S \

ARCHCONFIG_FIRST_TASKS+=$(LINKS)

$(LINKS):
	$(MKDIR) $(dir $@)
	$(RG_CP_LN) $(PWD_SRC)/$(notdir $@) $@

include $(RGMK)
