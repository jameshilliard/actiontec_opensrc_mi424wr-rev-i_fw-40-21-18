RGSRC=../../../../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/pwlib/pwlib_common.mak

#COMMON_SRC_DIR=$(RGSRC)/pkg/voip/pwlib/src/ptlib/common

getdate.tab.c: $(PWD_SRC)/getdate.y
	bison -y $(PWD_SRC)/getdate.y -o getdate.tab.c

getdate.tab.o: getdate.tab.c
	$(CC) -c $< -o $@

O_TARGET=target.o

O_OBJS_$(O_TARGET)+=notifier_ext.o getdate.tab.o pluginmgr.o sockets.o qos.o \
  ptime.o osutils.o safecoll.o collect.o contain.o sound.o videoio.o \
  vconvert.o pvidchan.o vfakeio.o tinyjpeg.o jidctflt.o object.o 
  # object.o must be last module 

CLEAN+=getdate.tab.c

include $(RGMK)
