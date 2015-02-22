RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_init.a+=$(if $(CONFIG_HALT),halt.o)
O_OBJS_init.a+=$(if $(CONFIG_INIT),init.o)
O_OBJS_init.a+=$(if $(CONFIG_MESG),mesg.o)
O_OBJS_init.a+=$(if $(CONFIG_POWEROFF),poweroff.o)
O_OBJS_init.a+=$(if $(CONFIG_REBOOT),reboot.o)

O_OBJS_init.a+=$(if $(CONFIG_HALT)$(CONFIG_INIT)$(CONFIG_POWEROFF)$(CONFIG_REBOOT),init_shared.o)

ifneq ($(strip $(O_OBJS_init.a)),)
  A_TARGET+=init.a
endif

include $(RGMK)
