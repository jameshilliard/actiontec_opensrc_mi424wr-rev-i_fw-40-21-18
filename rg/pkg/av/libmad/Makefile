RGSRC=../../..
include $(RGSRC)/envir.mak

EXPORT_HEADERS+=mad.h

A_TARGET=libmad.a

O_OBJS_$(A_TARGET)+=fixed.o bit.o timer.o stream.o frame.o synth.o decoder.o \
  layer12.o layer3.o huffman.o
ifdef CONFIG_ARM
  O_OBJS_$(A_TARGET)+=imdct_l_arm.o
endif

CFLAGS+=-DHAVE_CONFIG_H

ifdef CONFIG_PPC
  CFLAGS+=-DFPM_PPC
endif
ifdef CONFIG_ARM
  CFLAGS+=-DFPM_ARM
endif
ifdef CONFIG_MIPS
  CFLAGS+=-DFPM_MIPS
endif
ifdef CONFIG_UM
  CFLAGS+=-DFPM_INTEL
endif

include $(RGMK)
