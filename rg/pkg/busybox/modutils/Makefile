RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_modutils.a+=$(if $(CONFIG_INSMOD),insmod.o)
O_OBJS_modutils.a+=$(if $(CONFIG_LSMOD),lsmod.o)
O_OBJS_modutils.a+=$(if $(CONFIG_MODPROBE),modprobe.o)
O_OBJS_modutils.a+=$(if $(CONFIG_RMMOD),rmmod.o)

ifneq ($(strip $(O_OBJS_modutils.a)),)
  A_TARGET+=modutils.a
endif

include $(RGMK)
