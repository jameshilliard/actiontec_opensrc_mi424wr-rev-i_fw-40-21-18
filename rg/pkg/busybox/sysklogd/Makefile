RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_sysklogd.a+=$(if $(CONFIG_KLOGD),klogd.o)
O_OBJS_sysklogd.a+=$(if $(CONFIG_LOGGER),logger.o)
O_OBJS_sysklogd.a+=$(if $(CONFIG_LOGREAD),logread.o)
O_OBJS_sysklogd.a+=$(if $(CONFIG_SYSLOGD),syslogd.o)

ifneq ($(strip $(O_OBJS_sysklogd.a)),)
  A_TARGET+=sysklogd.a
endif

include $(RGMK)
