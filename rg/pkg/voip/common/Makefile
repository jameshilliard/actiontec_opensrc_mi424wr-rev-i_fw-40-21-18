RGSRC=../../..
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=voip/common
EXPORT_HEADERS+=voip_common_dsp.h

ifndef CONFIG_RG_GPL
  EXPORT_HEADERS+=voip_common_const.h hw_config.h voip_ipc_cmd.h 
endif

include $(RGMK)
