RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libnio.so
O_OBJS=java_nio_MappedByteBufferImpl.o \
	java_nio_VMDirectByteBuffer.o \
	gnu_java_nio_charset_iconv_IconvDecoder.o \
	gnu_java_nio_charset_iconv_IconvEncoder.o \
	gnu_java_nio_VMSelector.o \
	gnu_java_nio_channels_FileChannelImpl.o

RAMDISK_LIB_FILES+=libnio.so

include $(RGMK)
