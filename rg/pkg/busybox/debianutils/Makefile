RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_debianutils.a+=$(if $(CONFIG_MKTEMP),mktemp.o)
O_OBJS_debianutils.a+=$(if $(CONFIG_PIPE_PROGRESS),pipe_progress.o)
O_OBJS_debianutils.a+=$(if $(CONFIG_READLINK),readlink.o)
O_OBJS_debianutils.a+=$(if $(CONFIG_RUN_PARTS),run_parts.o)
O_OBJS_debianutils.a+=$(if $(CONFIG_START_STOP_DAEMON),start_stop_daemon.o)
O_OBJS_debianutils.a+=$(if $(CONFIG_WHICH),which.o)

ifneq ($(strip $(O_OBJS_debianutils.a)),)
  A_TARGET+=debianutils.a
endif

include $(RGMK)
