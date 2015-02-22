IPROUTE2SRC=..
include $(IPROUTE2SRC)/envir.mak

# These are needed only for DECnet and IPX
EXTRA_OBJS=dnet_ntop.o dnet_pton.o ipx_ntop.o ipx_pton.o

A_TARGET=libutil.a libnetlink.a

O_OBJS_libutil.a=utils.o rt_names.o ll_types.o ll_proto.o ll_addr.o \
  inet_proto.o $(EXTRA_OBJS)

O_OBJS_libnetlink.a=ll_map.o libnetlink.o

include $(RGMK)

