RGSRC=../..
include $(RGSRC)/envir.mak

ifdef CONFIG_RG_VOIP

ifdef CONFIG_HW_DSP
  SUBDIRS+=dsp
endif

SUBDIRS+=common main

A_TARGET=libvoip.a
L_OBJS_$(A_TARGET)+=main/voip_main.o

ifneq ($(CONFIG_RG_WBM),)
  SUBDIRS+=wbm
  JPKG_EXPORTED_DIR+=wbm
  L_OBJS_$(A_TARGET)+=wbm/wbm.o
endif
endif

ifdef CONFIG_RG_TR_104
  SUBDIRS+=tr_104
endif

ifdef CONFIG_RG_VOIP_RV
  SUBDIRS+=jata_rv
endif

ifdef CONFIG_RG_VOIP_OSIP
  SUBDIRS+=osip exosip jata_osip
  L_OBJS_$(A_TARGET)+=jata_osip/jata_osip.o
endif

ifdef CONFIG_RG_VOIP_HW_EMULATION
  SUBDIRS+=hwemu
endif

ifdef CONFIG_IXJ
  SUBDIRS+=ixj
endif

ifdef CONFIG_ZAPTEL
  SUBDIRS+=zaptel
endif

ifdef CONFIG_RG_VOIP_ASTERISK
  ifdef CONFIG_RG_VOIP_ASTERISK_H323
    SUBDIRS+=pwlib openh323
    JPKG_EXPORTED_DIR_SRC+=pwlib openh323
    LINK_DIRS+=pwlib openh323
  endif
  SUBDIRS+=asterisk jasterisk jtimer
  L_OBJS_$(A_TARGET)+=jasterisk/target.o
  JPKG_EXPORTED_DIR_SRC+=asterisk
  LINK_DIRS+=asterisk
  ifdef CONFIG_RG_PBX
    SUBDIRS+=sox
    JPKG_EXPORTED_DIR_SRC+=sox
    LINK_DIRS+=sox
  endif
endif

ifdef CONFIG_RG_JRTP
  SUBDIRS+=jrtp
endif

ifdef CONFIG_RG_GPL
   RAMDISK_BIN_FILES=
   A_TARGET=
   FIRST_TASKS=
   TARGET:=$(filter josip,$(TARGET))
   SUBDIRS:=$(filter %osip callctrl arch asterisk openh323 pwlib dsp jrtp \
       jtimer common,$(SUBDIRS))
endif

include $(RGMK)
