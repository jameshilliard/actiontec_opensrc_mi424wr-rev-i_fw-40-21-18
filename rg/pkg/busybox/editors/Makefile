RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_editors.a+=$(if $(CONFIG_AWK),awk.o)
O_OBJS_editors.a+=$(if $(CONFIG_PATCH),patch.o)
O_OBJS_editors.a+=$(if $(CONFIG_SED),sed.o)
O_OBJS_editors.a+=$(if $(CONFIG_VI),vi.o)

ifneq ($(strip $(O_OBJS_editors.a)),)
  A_TARGET+=editors.a
endif

include $(RGMK)
