RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

A_TARGET=libchannels.a

ifdef CONFIG_ZAPTEL
  O_OBJS+=chan_zap.o
  AST_MODULES_WITH_RELOAD+=chan_zap
else
ifdef CONFIG_IXJ
  O_OBJS+=chan_phone.o
  AST_MODULES_NO_RELOAD+=chan_phone
else
ifdef CONFIG_HW_DSP
  O_OBJS+=chan_jdsp.o
  AST_MODULES_WITH_RELOAD+=chan_jdsp
endif
endif
endif

ifdef CONFIG_HW_FXO
  O_OBJS+=chan_jfxo.o
  AST_MODULES_WITH_RELOAD+=chan_jfxo
endif

ifdef CONFIG_RG_VOIP_ASTERISK_H323
  SUBDIRS+=h323
  O_OBJS+=chan_h323.o h323/target.o
  AST_MODULES_WITH_RELOAD+=chan_h323
endif

ifdef CONFIG_RG_VOIP_ASTERISK_SIP
  O_OBJS+=chan_sip.o
  AST_MODULES_WITH_RELOAD+=chan_sip
endif

ifdef CONFIG_RG_VOIP_ASTERISK_MGCP_GW
  O_OBJS+=chan_mgcp_gw.o
  AST_MODULES_WITH_RELOAD+=chan_mgcp_gw
endif

ifdef CONFIG_RG_VOIP_ASTERISK_MGCP_CALL_AGENT
  O_OBJS+=chan_mgcp.o
  AST_MODULES_WITH_RELOAD+=chan_mgcp
endif

include $(RGSRC)/pkg/voip/asterisk/modules.mak

COMMAND_FILE=variable.mak
FOR_EACH=$(SO_TARGET)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(INDEX:%.so=%.o)
include $(LOOP)

CFLAGS+=-DT38_SUPPORT

SO_RAMDISK_SUBDIR=asterisk
RAMDISK_LIB_FILES+=$(addprefix $(SO_RAMDISK_SUBDIR)/,$(notdir $(SO_TARGET)))

include $(RGMK)
