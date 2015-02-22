RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../envir.mak

RAMDISK_JAVALIB_FILES+=$(if $(CONFIG_RG_KAFFE_JAVALIB_RT),rt.jar,mini_rt.jar)
RAMDISK_JAVALIB_FILES+=$(if $(CONFIG_RG_KAFFE_JAVALIB_TOOLS),tools.jar)

# TODO: possibly place this in /jre/lib? => modify CRAM scripts/makefiles.
RAMDISK_FILES=$(RAMDISK_JAVALIB_FILES:%=/home/kaffe/jre/lib/%)

include $(RGMK)
