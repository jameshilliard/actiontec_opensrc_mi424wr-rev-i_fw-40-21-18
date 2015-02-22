RGSRC=../../
include $(RGSRC)/envir.mak
TARGET+=radvd

INTERNAL_HEADERS+=config.h  defaults.h  gram.h  includes.h  pathnames.h  radvd.h

CFLAGS+=-DPATH_RADVD_CONF=\"/etc/radvd.conf\" -DPATH_RADVD_LOG=\"/var/log/radvd.log\" -DLOG_FACILITY=LOG_DAEMON -DPATH_RADVD_PID=\"/var/run/radvd.pid\" -DVERSION=\"1.6\" -DINET6=1 -DYY_SKIP_YYWRAP



O_OBJS_radvd+=device-common.o  gram.o       log.o      process.o  scanner.o  socket.o  util.o \
device.o         interface.o  privsep.o  radvd.o    recv.o      send.o     timer.o

O_OBJS_radvdump+=device-common.o  gram.o       log.o      process.o  radvdump.o  scanner.o  socket.o  util.o \
device.o         interface.o  privsep.o  recv.o      send.o     timer.o

RAMDISK_BIN_FILES+=$(TARGET)
include $(RGMK)

