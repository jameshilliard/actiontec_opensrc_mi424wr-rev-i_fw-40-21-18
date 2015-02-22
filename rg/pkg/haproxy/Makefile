RGSRC=../../
include $(RGSRC)/envir.mak
SUBDIRS=ebtree include
SUBDIRS+=src
TARGET+=haproxy

INTERNAL_HEADERS+=

CFLAGS+=-I./include -I./include/common -I./ebtree  -I./include/types -I./include/proto -I./include/import
CFLAGS+=-DENABLE_POLL -DENABLE_EPOLL -DENABLE_SEPOLL -DNETFILTER -DUSE_GETSOCKNAME  -DCONFIG_HAPROXY_VERSION=\"1.4.8\" -DCONFIG_HAPROXY_DATE=\"2010/06/16\" \
              -DBUILD_TARGET='"linux26"' \
              -DBUILD_CFLAGS='"-O2"' \
              -DBUILD_OPTIONS='""' 


O_OBJS_haproxy+=./ebtree/eb64tree.o\
./ebtree/ebimtree.o\
./ebtree/eb32tree.o\
./ebtree/ebsttree.o\
./ebtree/ebtree.o\
./ebtree/ebmbtree.o\
./ebtree/ebistree.o\
./src/freq_ctr.o\
./src/proto_tcp.o\
./src/base64.o\
./src/ev_epoll.o\
./src/proto_http.o\
./src/standard.o\
./src/ev_poll.o\
./src/buffers.o\
./src/ev_select.o\
./src/appsession.o\
./src/proto_uxst.o\
./src/session.o\
./src/stream_sock.o\
./src/queue.o\
./src/auth.o\
./src/proxy.o\
./src/server.o\
./src/checks.o\
./src/task.o\
./src/ev_sepoll.o\
./src/stick_table.o\
./src/lb_chash.o\
./src/cfgparse.o\
./src/acl.o\
./src/dumpstats.o\
./src/protocols.o\
./src/hdr_idx.o\
./src/sessionhash.o\
./src/lb_fwrr.o\
./src/stream_interface.o\
./src/time.o\
./src/haproxy.o\
./src/signal.o\
./src/uri_auth.o\
./src/log.o\
./src/client.o\
./src/backend.o\
./src/pipe.o\
./src/lb_map.o\
./src/lb_fwlc.o\
./src/regex.o\
./src/fd.o\
./src/pattern.o \
./src/memory.o


RAMDISK_BIN_FILES+=$(TARGET)
include $(RGMK)

