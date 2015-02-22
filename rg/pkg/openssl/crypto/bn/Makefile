RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=bn.h
INTERNAL_HEADERS=bn_lcl.h

BN_ASM=	bn_asm.o
O_OBJS=	bn_add.o bn_div.o bn_exp.o bn_lib.o bn_ctx.o bn_mul.o bn_mod.o \
	bn_print.o bn_rand.o bn_shift.o bn_word.o bn_blind.o \
	bn_kron.o bn_sqrt.o bn_gcd.o bn_prime.o bn_err.o bn_sqr.o $(BN_ASM) \
	bn_recp.o bn_mont.o bn_mpi.o bn_exp2.o bn_gf2m.o bn_nist.o \
	bn_depr.o bn_const.o

CD_EXPORTED_FILES+=bn_prime.pl

include $(RGMK)

archconfig:: bn_prime.h

bn_prime.h: bn_prime.pl
	$(PERL) $< > $@

