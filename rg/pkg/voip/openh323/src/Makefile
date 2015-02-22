RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/pwlib/pwlib_common.mak

O_TARGET=target.o

CXXFLAGS+=-c -fPIC -I$(RGSRC)/pkg/voip/openh323/include -DPASN_NOPRINTON \
  -DPASN_LEANANDMEAN 
  
# Disable optimization by overriding previously set -O flags. With optimization
# enabled, build time is 5 times longer.
CFLAGS+=-O0

O_OBJS_$(O_TARGET)+=h225_1.o h225_2.o h245_1.o h245_2.o h245_3.o h235.o \
  h323.o h323ep.o h323neg.o h323pdu.o h323rtp.o h323caps.o q931.o x224.o \
  codecs.o channels.o transports.o rtp.o gkclient.o gkserver.o mediafmt.o \
  guid.o h225ras.o h323trans.o h235auth.o rfc2833.o gccpdu.o jitter.o \
  h323pluginmgr.o g711.o lid.o rtp2wav.o h235auth1.o h261codec.o

SUBDIRS+=vic
	
include $(RGMK)
