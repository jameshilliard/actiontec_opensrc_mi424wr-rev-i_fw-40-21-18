RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=evp.h
INTERNAL_HEADERS=evp_locl.h

O_OBJS=	encode.o digest.o evp_enc.o evp_key.o evp_acnf.o \
	e_des.o e_bf.o e_idea.o e_des3.o \
	e_rc4.o e_aes.o names.o \
	e_xcbc_d.o e_rc2.o e_cast.o e_rc5.o \
	m_null.o m_md2.o m_md4.o m_md5.o m_sha.o m_sha1.o \
	m_dss.o m_dss1.o m_mdc2.o m_ripemd.o m_ecdsa.o\
	p_open.o p_seal.o p_sign.o p_verify.o p_lib.o p_enc.o p_dec.o \
	bio_md.o bio_b64.o bio_enc.o evp_err.o e_null.o \
	c_all.o c_allc.o c_alld.o evp_lib.o bio_ok.o \
	evp_pkey.o evp_pbe.o p5_crpt.o p5_crpt2.o \
	e_old.o

include $(RGMK)

