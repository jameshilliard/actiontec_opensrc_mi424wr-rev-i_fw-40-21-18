RGSRC=../../..
include $(RGSRC)/envir.mak

TARGET=excepttest algotest bitsettest dequetest fstreamtest  \
  listtest maptest mmaptest newdel numerictest settest sstreamtest \
  stacktest streambuftest streamitertest stringtest vectortest io

COMMAND_FILE=variable.mak
FOR_EACH=$(TARGET)
INDEX_VARIABLE=O_OBJS_$(INDEX)
INDEX_VAL=$(INDEX:%=%.o)
include $(LOOP)

LDFLAGS+=-L$(BUILDDIR)/pkg/uclibc++/src -luClibc++ -ldl

CFLAGS=-I../include -I$(BUILDDIR) -g

include $(RGMK)
