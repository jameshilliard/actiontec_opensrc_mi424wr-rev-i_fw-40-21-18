RGSRC=../../../../..
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../../../envir.mak

SO_TARGET=libfdlibm.so
O_OBJS=dtoa.o e_acos.o e_asin.o e_atan2.o \
	e_exp.o e_fmod.o e_log.o e_pow.o e_remainder.o \
	e_rem_pio2.o e_scalb.o e_sqrt.o k_cos.o k_rem_pio2.o \
	k_sin.o k_tan.o mprec.o s_atan.o s_ceil.o s_copysign.o \
	s_cos.o s_fabs.o s_finite.o sf_fabs.o s_floor.o \
	sf_rint.o s_rint.o s_scalbn.o s_sin.o s_tan.o strtod.o \
	w_acos.o w_asin.o w_atan2.o w_exp.o w_fmod.o w_log.o \
	w_pow.o w_remainder.o w_sqrt.o

EXPORT_LIBS=$(SO_TARGET)
RAMDISK_LIB_FILES+=$(SO_TARGET)

include $(RGMK)
