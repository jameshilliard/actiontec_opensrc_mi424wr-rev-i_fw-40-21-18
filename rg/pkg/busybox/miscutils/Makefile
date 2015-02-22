RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_miscutils.a+=$(if $(CONFIG_ADJTIMEX),adjtimex.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_CROND),crond.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_CRONTAB),crontab.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_DC),dc.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_DEVFSD),devfsd.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_HDPARM),hdparm.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_LAST),last.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_MAKEDEVS),makedevs.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_MT),mt.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_RX),rx.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_STRINGS),strings.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_TIME),time.o)
O_OBJS_miscutils.a+=$(if $(CONFIG_WATCHDOG),watchdog.o)

ifneq ($(strip $(O_OBJS_miscutils.a)),)
  A_TARGET+=miscutils.a
endif

include $(RGMK)
