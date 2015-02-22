RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/pwlib/pwlib_common.mak

O_TARGET=target.o

CXXFLAGS+=-c -fPIC -I$(RGSRC)/pkg/voip/openh323/include -DPASN_NOPRINTON \
  -DPASN_LEANANDMEAN 

# Disable optimization by overriding previously set -O flags. With optimization
# enabled, build time is 5 times longer.
CFLAGS+=-O0

O_OBJS_$(O_TARGET)+=bv.o dct.o p64.o vid_coder.o huffcode.o transmitter.o \
  encoder-h261.o p64encoder.o

include $(RGMK)
