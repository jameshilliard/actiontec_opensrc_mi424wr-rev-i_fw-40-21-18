RGSRC=../../../..
include $(RGSRC)/envir.mak

SUBDIRS+=gsm

A_TARGET=libst.a
O_OBJS_$(A_TARGET)=adpcm.o au.o auto.o g711.o g721.o g723_24.o \
  g723_40.o g72x.o gsm.o ima_rw.o mp3.o raw.o wav.o avg.o trim.o \
  getopt.o handlers.o misc.o resample.o stio.o util.o gsm/target.o
  
TARGET=sox
LDFLAGS_$(TARGET)=-L$(BUILDDIR)/pkg/voip/sox/src -lst -lm

ifdef CONFIG_RG_LIBMAD
  CFLAGS+=-DHAVE_LIBMAD
  LDFLAGS_$(TARGET)+=-L$(BUILDDIR)/pkg/av/libmad -lmad
endif

O_OBJS_$(TARGET)=sox.o
RAMDISK_BIN_FILES+=$(TARGET)

include $(RGMK)
