RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=ec.h
INTERNAL_HEADERS=ec_lcl.h

O_OBJS=	ec_lib.o ecp_smpl.o ecp_mont.o ecp_nist.o ec_cvt.o ec_mult.o\
	ec_err.o ec_curve.o ec_check.o ec_print.o ec_asn1.o ec_key.o\
	ec2_smpl.o ec2_mult.o

include $(RGMK)

