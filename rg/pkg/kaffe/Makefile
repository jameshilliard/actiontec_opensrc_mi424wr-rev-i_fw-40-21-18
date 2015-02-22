RGSRC=../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/envir.mak

# Order is important, due to library dependencies.
ifeq ($(CONFIG_RG_JPKG),)
SUBDIRS+=replace libltdl kaffe libraries
endif

include $(RGMK)

