RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_shell.a+=$(if $(CONFIG_ASH),ash.o)
O_OBJS_shell.a+=$(if $(CONFIG_HUSH),hush.o)
O_OBJS_shell.a+=$(if $(CONFIG_LASH),lash.o)
O_OBJS_shell.a+=$(if $(CONFIG_MSH),msh.o)
O_OBJS_shell.a+=$(if $(CONFIG_FEATURE_COMMAND_EDITING),cmdedit.o)

ifneq ($(strip $(O_OBJS_shell.a)),)
  A_TARGET+=shell.a
endif

include $(RGMK)
