RGSRC=../../../..
include $(RGSRC)/envir.mak

LINKS_SRC=include arch drivers

DST_PATH=$(BUILDDIR)/os/linux-2.6/
LINKS_DST=$(addprefix $(DST_PATH), $(LINKS_SRC))
ARCHCONFIG_FIRST_TASKS+=$(LINKS_DST)

mv_copy_link_phony:
#	echo ">>>>>>>>> phony target >>>>>>>>>>>>"

$(LINKS_DST): mv_copy_link_phony
	$(MKDIR) $(dir $@)
	$(RG_CP_LN) $(PWD_SRC)/$(notdir $@) $(dir $@)

include $(RGMK)

