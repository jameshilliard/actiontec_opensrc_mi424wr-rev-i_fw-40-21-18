RGSRC=../..
include $(RGSRC)/envir.mak

CD_EXPORTED_FILES+=rt256x_to_openrg.h

SUBDIRS+=$(if $(CONFIG_RALINK_RT2560), rt2560 8021X)
SUBDIRS+=$(if $(CONFIG_RALINK_RT2561), rt2561/802.1X rt2561/Module)

include $(RGMK)
