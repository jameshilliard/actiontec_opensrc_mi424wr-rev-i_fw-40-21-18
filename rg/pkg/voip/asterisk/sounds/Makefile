RGSRC=../../../..
include $(RGSRC)/envir.mak

RAMDISK_VAR_FILES=$(addprefix lib/asterisk/sounds/, \
  $(notdir $(wildcard $(PWD_SRC)/vm-*.gsm) \
  $(wildcard $(PWD_SRC)/queue-*.gsm) beep.gsm auth-thankyou.gsm invalid.gsm \
  pbx-invalid.gsm))

SUBDIRS+=digits

include $(RGMK)
