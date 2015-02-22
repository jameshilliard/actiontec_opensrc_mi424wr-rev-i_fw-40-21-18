RGSRC=../../..
include $(RGSRC)/envir.mak

SUBDIRS+=abi/libsupc
L_OBJS+=$(wildcard  abi/libsupc/*.o)

ifneq ($(shell $(CC) -print-file-name=libgcc_eh.a),libgcc_eh.a)
SUBDIRS+=abi/libgcc_eh
L_OBJS+=$(wildcard  abi/libgcc_eh/*.o)
endif

ifdef CONFIG_DYN_LINK
SO_TARGET+=libuClibc++.so
EXPORT_LIBS+=libuClibc++.so
RAMDISK_LIB_FILES+=libuClibc++.so
else
A_TARGET+=libuClibc++.a
EXPORT_LIBS+=libuClibc++.a
RAMDISK_LIB_FILES+=libuClibc++.a
endif

O_OBJS+=algorithm.o del_opv.o iomanip.o locale.o numeric.o stdexcept.o \
  bitset.o del_opvnt.o ios.o map.o ostream.o streambuf.o char_traits.o \
  deque.o iostream.o new_op.o queue.o string.o complex.o exception.o istream.o \
  new_opnt.o set.o utility.o del_op.o fstream.o iterator.o new_opv.o sstream.o \
  valarray.o del_opnt.o func_exception.o list.o new_opvnt.o stack.o vector.o \
  new_handler.o abi/abi.o


include $(RGMK)
