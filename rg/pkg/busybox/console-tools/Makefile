RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_console-tools.a+=$(if $(CONFIG_CHVT),chvt.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_CLEAR),clear.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_DEALLOCVT),deallocvt.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_DUMPKMAP),dumpkmap.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_LOADFONT),loadfont.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_LOADKMAP),loadkmap.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_OPENVT),openvt.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_RESET),reset.o)
O_OBJS_console-tools.a+=$(if $(CONFIG_SETKEYCODES),setkeycodes.o)

ifneq ($(strip $(O_OBJS_console-tools.a)),)
  A_TARGET+=console-tools.a
endif

include $(RGMK)
