RGSRC=../../
include $(RGSRC)/envir.mak

SUBDIRS=pppd kernel
ifndef CONFIG_RG_GPL
A_TARGET=libppp.a
L_OBJS_$(A_TARGET)+=pppd/ppptask.o
L_OBJS_$(A_TARGET)+=pppd/plugins/plugins.o

ifneq ($(CONFIG_RG_PPTPS),)
  SUBDIRS+=pptp-server
  L_OBJS_$(A_TARGET)+=pptp-server/pptps_task.o
endif

ifneq ($(CONFIG_RG_L2TPS),)
  SUBDIRS+=l2tp-server
  L_OBJS_$(A_TARGET)+=l2tp-server/l2tps_task.o
endif

ifdef CONFIG_RG_PPPOES
  SUBDIRS+=pppoes
  L_OBJS_$(A_TARGET)+=pppoes/pppoes_task.o
endif

ifdef CONFIG_RG_PPPOS
  SUBDIRS+=pppos
  L_OBJS_$(A_TARGET)+=pppos/pppos_task.o
endif

ifdef CONFIG_RG_PPPOE
  SUBDIRS+=pppoe
  L_OBJS_$(A_TARGET)+=pppoe/pppoe.o
endif

ifdef CONFIG_RG_PPPOA
  SUBDIRS+=pppoa
  L_OBJS_$(A_TARGET)+=pppoa/pppoa.o
endif

ifdef CONFIG_RG_PPTPC
  SUBDIRS+=pptpc
  L_OBJS_$(A_TARGET)+=pptpc/pptpc.o
endif

ifdef CONFIG_RG_L2TPC
  SUBDIRS+=l2tpc
  L_OBJS_$(A_TARGET)+=l2tpc/l2tpc.o
endif

SUBDIRS+=mgt main

ifneq ($(CONFIG_RG_WBM),)
  SUBDIRS+=wbm
  JPKG_EXPORTED_DIR+=wbm
  L_OBJS_$(A_TARGET)+=wbm/wbm.o
endif

L_OBJS_$(A_TARGET)+=main/ppp_main.o
L_OBJS_$(A_TARGET)+=mgt/ppp_dev.o

endif #CONFIG_RG_GPL

include $(RGMK)
