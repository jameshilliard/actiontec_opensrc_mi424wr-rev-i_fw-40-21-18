RGSRC=../../../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/pwlib/pwlib_common.mak

O_TARGET=target.o

O_OBJS_$(O_TARGET)+=uicmp.o socket.o udll.o channel.o osutil.o tlib.o switch.o \
  assert.o

include $(RGMK)
