RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_findutils.a+=$(if $(CONFIG_FIND),find.o)
O_OBJS_findutils.a+=$(if $(CONFIG_GREP),grep.o)
O_OBJS_findutils.a+=$(if $(CONFIG_XARGS),xargs.o)

ifneq ($(strip $(O_OBJS_findutils.a)),)
  A_TARGET+=findutils.a
endif

include $(RGMK)
