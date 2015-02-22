RGSRC=../../../
include $(RGSRC)/envir.mak

A_TARGET+=procps.a

O_OBJS_procps.a+=$(if $(CONFIG_FREE),free.o)
O_OBJS_procps.a+=$(if $(CONFIG_KILL),kill.o)
O_OBJS_procps.a+=$(if $(CONFIG_PIDOF),pidof.o)
O_OBJS_procps.a+=$(if $(CONFIG_PS),ps.o)
O_OBJS_procps.a+=$(if $(CONFIG_RENICE),renice.o)
O_OBJS_procps.a+=$(if $(CONFIG_SYSCTL),sysctl.o)
O_OBJS_procps.a+=$(if $(CONFIG_TOP),top.o)
O_OBJS_procps.a+=$(if $(CONFIG_UPTIME),uptime.o)

include $(RGMK)
