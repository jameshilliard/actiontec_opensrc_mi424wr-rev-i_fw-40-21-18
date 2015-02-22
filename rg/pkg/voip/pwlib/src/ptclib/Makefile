RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/pwlib/pwlib_common.mak

O_TARGET=target.o

O_OBJS_$(O_TARGET)+=ipacl.o qchannel.o delaychan.o memfile.o cypher.o random.o \
  inetprot.o asner.o http.o pstun.o dtmf.o enum.o pasn.o pdns.o ptts.o \
  httpclnt.o pwavfile.o pnat.o

include $(RGMK)
