RGSRC=../../../../../
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/route/quagga/quagga/common.mak

A_TARGET=libquagga.a

INTERNAL_HEADERS+= buffer.h checksum.h command.h distribute.h filter.h \
  getopt.h hash.h if.h if_rmap.h jhash.h keychain.h linklist.h log.h md5.h \
  memory.h memtypes.h network.h plist.h pqueue.h prefix.h privs.h regex-gnu.h \
  routemap.h route_types.h sigevent.h smux.h sockopt.h sockunion.h stream.h \
  str.h table.h thread.h vector.h version.h vty.h workqueue.h zassert.h \
  zclient.h zebra.h

O_OBJS=network.o pid_output.o getopt.o getopt1.o daemon.o \
  checksum.o vector.o linklist.o vty.o command.o sockunion.o prefix.o thread.o \
  if.o memory.o buffer.o table.o hash.o filter.o routemap.o distribute.o \
  stream.o str.o log.o plist.o zclient.o sockopt.o smux.o md5.o if_rmap.o \
  keychain.o privs.o sigevent.o pqueue.o jhash.o memtypes.o workqueue.o
	
include $(RGMK)

