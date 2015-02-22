RGSRC=../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../envir.mak

TARGET=kaffe-bin
O_OBJS=main.o version.o

LDLIBS:= \
	-lltdl \
	-lkaffevm \
	-lengine \
	-lkthread \
	-lkaffegc \
	-lkaffeverifier \
	-ljni \
	-lreplace \
	-ldl \
	-lm \
	-lpthread \
	$(LDLIBS)

RAMDISK_BIN_FILES+=kaffe-bin

include $(RGMK)
