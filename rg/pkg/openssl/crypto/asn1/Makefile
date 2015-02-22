RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=asn1.h asn1_mac.h asn1t.h
INTERNAL_HEADERS=charmap.h

O_OBJS=	a_object.o a_bitstr.o a_utctm.o a_gentm.o a_time.o a_int.o a_octet.o \
	a_print.o a_type.o a_set.o a_dup.o a_d2i_fp.o a_i2d_fp.o \
	a_enum.o a_utf8.o a_sign.o a_digest.o a_verify.o a_mbstr.o a_strex.o \
	x_algor.o x_val.o x_pubkey.o x_sig.o x_req.o x_attrib.o x_bignum.o \
	x_long.o x_name.o x_x509.o x_x509a.o x_crl.o x_info.o x_spki.o nsseq.o \
	d2i_pu.o d2i_pr.o i2d_pu.o i2d_pr.o \
	t_req.o t_x509.o t_x509a.o t_crl.o t_pkey.o t_spki.o t_bitst.o \
	tasn_new.o tasn_fre.o tasn_enc.o tasn_dec.o tasn_utl.o tasn_typ.o \
	f_int.o f_string.o n_pkey.o \
	f_enum.o a_hdr.o x_pkey.o a_bool.o x_exten.o \
	asn1_gen.o asn1_par.o asn1_lib.o asn1_err.o a_meth.o a_bytes.o \
	a_strnid.o \
	evp_asn1.o asn_pack.o p5_pbe.o p5_pbev2.o p8_pkey.o asn_moid.o

include $(RGMK)

