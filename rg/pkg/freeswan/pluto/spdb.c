/* Security Policy Data Base (such as it is)
 * Copyright (C) 1998-2001  D. Hugh Redelmeier.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freeswan.h>

#include "constants.h"
#include "defs.h"
#include "id.h"
#include "x509.h"
#include "connections.h"	/* needs id.h */
#include "state.h"
#include "packet.h"
#include "keys.h"
#include "kernel.h"
#include "log.h"
#include "spdb.h"
#include "whack.h"	/* for RC_LOG_SERIOUS */
#include "rg_utils.h"

#include "sha1.h"
#include "md5.h"
#include "crypto.h" /* requires sha1.h and md5.h */

#ifndef NO_IKE_ALG
#include "alg_info.h"
#include "kernel_alg.h"
#include "ike_alg.h"
#include "db_ops.h"
#endif

#ifdef NAT_TRAVERSAL
#include "nat_traversal.h"
#endif

#define AD(x) x, elemsof(x)	/* Array Description */
#define AD_NULL NULL, 0

static void prop_free(struct db_prop *p)
{
    while (p->trans_cnt--)
	free(p->trans[p->trans_cnt].attrs);
    free(p->trans);
}

static void conj_free(struct db_prop_conj *p)
{
    while (p->prop_cnt--)
	prop_free(p->props+p->prop_cnt);
    free(p->props);
}

void sadb_free(struct db_sa **p)
{
    struct db_sa *tmp = *p;
    if (tmp && tmp->prop_conjs)
    {
	while (tmp->prop_conj_cnt--)
	    conj_free(tmp->prop_conjs+tmp->prop_conj_cnt);
	free(tmp->prop_conjs);
    }
    free(tmp);
    *p = NULL;
}

/* array_property_add return added pointer */
static void *array_property_add(void **p, int *count, int size)
{
    void *tmp = realloc(*((char **)p), size*(*count+1));

    if (!tmp)
	return NULL;
    *(char **)p = tmp;
    memset(*(char **)p+size*(*count), 0, size);
    (*count)++;
    return (void *) (*(char **)p+size*(*count-1));
}

static int attr_add(struct db_trans *trans, u_int16_t type, u_int16_t val)
{
    int res = -1;
    struct db_attr *new_attr;
    
    new_attr = (struct db_attr *) array_property_add(
	(void **) &trans->attrs, &trans->attr_cnt,
	sizeof(struct db_attr));
    if (!new_attr)
	goto Exit;
    new_attr->type = type;
    new_attr->val = val;
    res = 0;

Exit:
    return res;
}

/**************** Oakley (main mode) SA database ****************/

typedef struct oakley_trans_t {
    union {
        lset_t openrg_flag;
	lset_t policy;
    } flag;
    u_int16_t value;
    u_int16_t keylen;
} oakley_trans_t;

static oakley_trans_t rg_oakley_hash[] = {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
    {{OPENRG_AUTH_MD5}, OAKLEY_MD5, 0},
#endif
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
    {{OPENRG_AUTH_SHA}, OAKLEY_SHA, 0},
#endif
    {{0}, 0, 0}
};

static oakley_trans_t rg_oakley_encrypt[] = {
#ifdef CONFIG_IPSEC_ENC_1DES
    {{OPENRG_ENC_1DES}, OAKLEY_DES_CBC, 0},
#endif
#ifdef CONFIG_IPSEC_ENC_3DES
    {{OPENRG_ENC_3DES}, OAKLEY_3DES_CBC, 0},
#endif
#ifdef CONFIG_IPSEC_ENC_AES
    {{OPENRG_ENC_AES128}, OAKLEY_AES_CBC, 128},
    {{OPENRG_ENC_AES192}, OAKLEY_AES_CBC, 192},
    {{OPENRG_ENC_AES256}, OAKLEY_AES_CBC, 256},
#endif
    {{0}, 0, 0}
};

oakley_trans_t rg_oakley_group[] = {
    {{OPENRG_MODP_768}, OAKLEY_GROUP_MODP768, 0},
    {{OPENRG_MODP_1024}, OAKLEY_GROUP_MODP1024, 0},
    {{OPENRG_MODP_1536}, OAKLEY_GROUP_MODP1536, 0},
    {{0}, 0, 0}
};

static oakley_trans_t rg_oakley_auth[] = {
    {{POLICY_PSK}, OAKLEY_PRESHARED_KEY, 0},
    {{POLICY_RSASIG}, OAKLEY_RSA_SIG, 0},
    {{0}, 0, 0}
};

/* In aggressive mode openrg_policy have only one policy for each
 * policy list.
 */
u_int16_t aggr_get_oakley_policy(oakley_trans_t *policy_list,
    lset_t openrg_policy)
{
    for (; policy_list->flag.openrg_flag; policy_list++)
    {
	if (policy_list->flag.openrg_flag&openrg_policy)
	    break;
    }
    return policy_list->value;
}

static int oakley_attr_add(struct db_trans *trans, u_int16_t auth,
    u_int16_t hash, u_int16_t group, u_int16_t encrypt, u_int16_t keylen)
{
    if (attr_add(trans, OAKLEY_ENCRYPTION_ALGORITHM, encrypt)<0 ||
        attr_add(trans, OAKLEY_HASH_ALGORITHM, hash)<0 ||
        (keylen && attr_add(trans, OAKLEY_KEY_LENGTH, keylen)<0) ||
        attr_add(trans, OAKLEY_AUTHENTICATION_METHOD, auth)<0 ||
        attr_add(trans, OAKLEY_GROUP_DESCRIPTION, group)<0)
    {
	return -1;
    }
    return 0;
}

static int oakley_trans_add(struct db_prop *prop, u_int16_t hash,
    u_int16_t encrypt, u_int16_t keylen, u_int16_t group, u_int16_t auth)
{
    struct db_trans *new_trans;
    int res = -1;

    new_trans = (struct db_trans *) array_property_add((void **) &prop->trans,
	&prop->trans_cnt, sizeof(struct db_trans));
    if (!new_trans)
	goto Exit;
    new_trans->transid = KEY_IKE;
    if (oakley_attr_add(new_trans, auth, hash, group, encrypt, keylen)<0)
	goto Exit;
    res = 0;
    
Exit:
    return res;
}

struct db_sa *oakley_sadb_alloc(lset_t openrg_flags, lset_t policy)
{
    struct db_prop_conj *new_conj;
    struct db_prop *new_prop;
    int hash, enc, auth, group;
    struct db_sa *res;
    
    res = (struct db_sa *) calloc(1, sizeof(struct db_sa));
    if (!res)
	goto Exit;
    if (!openrg_flags)
	goto Exit;
    
    new_conj = (struct db_prop_conj *) array_property_add(
	(void **) &res->prop_conjs, &res->prop_conj_cnt,
	sizeof(struct db_prop_conj));
    if (!new_conj)
	goto Error;
    new_prop = (struct db_prop *) array_property_add(
	(void **) &new_conj->props, &new_conj->prop_cnt,
	sizeof(struct db_prop));
    if (!new_prop)
	goto Error;
    new_prop->protoid = PROTO_ISAKMP;

    for (hash = 0; rg_oakley_hash[hash].flag.openrg_flag; hash++)
    {
	for (enc = 0; rg_oakley_encrypt[enc].flag.openrg_flag; enc++)
	{
	    for (group = 0; rg_oakley_group[group].flag.openrg_flag; group++)
	    {
		for (auth = 0; rg_oakley_auth[auth].flag.policy; auth++)
		{
		    if (rg_oakley_hash[hash].flag.openrg_flag&openrg_flags &&
			rg_oakley_encrypt[enc].flag.openrg_flag&openrg_flags &&
			rg_oakley_group[group].flag.openrg_flag&openrg_flags &&
			rg_oakley_auth[auth].flag.policy&policy &&
			oakley_trans_add(new_prop, rg_oakley_hash[hash].value,
			rg_oakley_encrypt[enc].value,
			rg_oakley_encrypt[enc].keylen,
			rg_oakley_group[group].value,
			rg_oakley_auth[auth].value)<0)
		    {
			goto Error;
		    }
		}
	    }
	}
    }
    goto Exit;
    
Error:
    sadb_free(&res);
    res = NULL;
Exit:
    return res;
}

/**************** IPsec (quick mode) SA database ****************/

static int compress_add(struct db_prop_conj *p)
{
    int res = -1;
    struct db_prop *prop;
    struct db_trans *trans;

    prop = (struct db_prop *) array_property_add((void **) &p->props,
	&p->prop_cnt, sizeof(struct db_prop));
    if (!prop)
	goto Exit;
    prop->protoid = PROTO_IPCOMP;
    trans = (struct db_trans *) array_property_add((void **) &prop->trans,
	&prop->trans_cnt, sizeof(struct db_trans));
    if (!trans)
	goto Exit;
    trans->transid = IPCOMP_DEFLATE;
    res = 0;

Exit:
    return res;
}

static int ipsec_prop_add_transid(struct db_prop *prop, u_int8_t transid,
    u_int16_t attr_type, u_int16_t attr, u_int16_t transid_attr_type,
    u_int16_t transid_attr)
{
    struct db_trans *new_trans = (struct db_trans *) array_property_add(
	(void **) &prop->trans, &prop->trans_cnt, sizeof(struct db_trans));

    if (!new_trans)
	return -1;
    new_trans->transid = transid;
    if (attr_add(new_trans, attr_type, attr))
	return -1;
    if (transid_attr_type &&
	attr_add(new_trans, transid_attr_type, transid_attr))
    {
	return -1;
    }
    return 0;
}

/* If prop==NULL function only checks if trans_list have propositions
 * signed in openrg_flags.
 * Return value:
 * < 0  - fail
 * == 0 - success
 * > 0  - propositions form trans_list can be added (when prop==NULL only).
 */
static int ipsec_prop_add_proto(rg_ipsec_prop_t *trans_list,
    rg_ipsec_prop_t *attr_list, lset_t openrg_flags, struct db_prop *prop)
{
    int i, j;

    for (i = 0; trans_list[i].openrg_policy; i++)
    {
	if (!(trans_list[i].openrg_policy & openrg_flags))
	    continue;
	if (!attr_list)
	{
	    if (!prop)
		return 1;
	    if (ipsec_prop_add_transid(prop, trans_list[i].transid,
		trans_list[i].attr_type, trans_list[i].attr, 0, 0))
	    {
		return -1;
	    }
	    continue;
	}
	for (j=0; attr_list[j].openrg_policy; j++)
	{
	    if (!(attr_list[j].openrg_policy & openrg_flags))
		continue;
	    if (!prop)
		return 1;
	    if (ipsec_prop_add_transid(prop, trans_list[i].transid,
		attr_list[j].attr_type, attr_list[j].attr,
		trans_list[i].attr_type, trans_list[i].attr))
	    {
		return -1;
	    }
	}
    }
    return 0;
}

static int ipsec_prop_add(struct db_sa *sa, u_int8_t protoid,
    rg_ipsec_prop_t *trans_list, rg_ipsec_prop_t *attr_list,
    int compress, lset_t openrg_flags)
{
    struct db_prop_conj *new_conj;
    struct db_prop *new_prop;
    int res = 0;
    
    if (!openrg_flags)
	return -1;
    /* Check if we can add propositions. */
    res = ipsec_prop_add_proto(trans_list, attr_list, openrg_flags, NULL);
    if (res<=0)
	return res;
    res = -1;
    new_conj = (struct db_prop_conj *) array_property_add(
	(void **) &sa->prop_conjs, &sa->prop_conj_cnt,
	sizeof(struct db_prop_conj));
    if (!new_conj)
	return -1;
    new_prop = (struct db_prop *) array_property_add(
	(void **) &new_conj->props, &new_conj->prop_cnt,
	sizeof(struct db_prop));
    if (!new_prop)
	return -1;
    new_prop->protoid = protoid;
    if (ipsec_prop_add_proto(trans_list, attr_list, openrg_flags, new_prop)<0)
	return -1;
    if (compress && compress_add(new_conj)<0)
	return -1;

    return 0;
}

struct db_sa *ipsec_sadb_alloc(lset_t openrg_flags, int is_compress)
{
    struct db_sa *res;
    
    if (!openrg_flags)
	return NULL;
    res = (struct db_sa *) calloc(1, sizeof(struct db_sa));
    if (!res)
	return NULL;
    if (ipsec_prop_add(res, PROTO_IPSEC_ESP, esp_enc, esp_auth, is_compress,
	openrg_flags)<0)
    {
	goto Error;
    }
    if (ipsec_prop_add(res, PROTO_IPSEC_AH, ah_auth, NULL, is_compress,
	openrg_flags)<0)
    {
	goto Error;
    }

    return res;
Error:
    sadb_free(&res);
    return NULL;
}

#undef AD
#undef AD_NULL

/* output an attribute (within an SA) */
static bool
out_attr(int type
, unsigned long val
, struct_desc *attr_desc
, enum_names **attr_val_descs USED_BY_DEBUG
, pb_stream *pbs)
{
    struct isakmp_attribute attr;

    if (val >> 16 == 0)
    {
	/* short value: use TV form */
	attr.isaat_af_type = type | ISAKMP_ATTR_AF_TV;
	attr.isaat_lv = val;
	if (!out_struct(&attr, attr_desc, pbs, NULL))
	    return FALSE;
    }
    else
    {
	/* This is a real fudge!  Since we rarely use long attributes
	 * and since this is the only place where we can cause an
	 * ISAKMP message length to be other than a multiple of 4 octets,
	 * we force the length of the value to be a multiple of 4 octets.
	 * Furthermore, we only handle values up to 4 octets in length.
	 * Voila: a fixed format!
	 */
	pb_stream val_pbs;
	u_int32_t nval = htonl(val);

	attr.isaat_af_type = type | ISAKMP_ATTR_AF_TLV;
	if (!out_struct(&attr, attr_desc, pbs, &val_pbs)
	|| !out_raw(&nval, sizeof(nval), &val_pbs, "long attribute value"))
	    return FALSE;
	close_output_pbs(&val_pbs);
    }
    DBG(DBG_EMITTING,
	enum_names *d = attr_val_descs[type];

	if (d != NULL)
	    DBG_log("    [%lu is %s]"
		, val, enum_show(d, val)));
    return TRUE;
}
#define return_on(var, val) do { var=val;goto return_out; } while(0);
/* Output an SA, as described by a db_sa.
 * This has the side-effect of allocating SPIs for us.
 */
bool
out_sa(pb_stream *outs
, struct db_sa *sadb
, struct state *st
, bool oakley_mode
, bool aggressive_mode
, u_int8_t np)
{
    pb_stream sa_pbs;
    int pcn;
    bool ret = FALSE;
    bool ah_spi_generated = FALSE
	, esp_spi_generated = FALSE
	, ipcomp_cpi_generated = FALSE;
#if !defined NO_KERNEL_ALG || !defined NO_IKE_ALG
    struct db_context *db_ctx = NULL;
#endif

    /* SA header out */
    {
	struct isakmp_sa sa;

	sa.isasa_np = np;
	st->st_doi = sa.isasa_doi = ISAKMP_DOI_IPSEC;	/* all we know */
	if (!out_struct(&sa, &isakmp_sa_desc, outs, &sa_pbs))
	    return_on(ret, FALSE);
    }

    /* within SA: situation out */
    st->st_situation = SIT_IDENTITY_ONLY;
    if (!out_struct(&st->st_situation, &ipsec_sit_desc, &sa_pbs, NULL))
	return_on(ret, FALSE);

    /* within SA: Proposal Payloads
     *
     * Multiple Proposals with the same number are simultaneous
     * (conjuncts) and must deal with different protocols (AH or ESP).
     * Proposals with different numbers are alternatives (disjuncts),
     * in preference order.
     * Proposal numbers must be monotonic.
     * See draft-ietf-ipsec-isakmp-09.txt 4.2
     */

    for (pcn = 0; pcn != sadb->prop_conj_cnt; pcn++)
    {
	struct db_prop_conj *pc = &sadb->prop_conjs[pcn];
	int pn;

	for (pn = 0; pn != pc->prop_cnt; pn++)
	{
	    struct db_prop *p = &pc->props[pn];
	    pb_stream proposal_pbs;
	    struct isakmp_proposal proposal;
	    struct_desc *trans_desc;
	    struct_desc *attr_desc;
	    enum_names **attr_val_descs;
	    int tn;

	    /* Proposal header */
	    proposal.isap_np = pcn == sadb->prop_conj_cnt-1 && pn == pc->prop_cnt-1
		? ISAKMP_NEXT_NONE : ISAKMP_NEXT_P;
	    proposal.isap_proposal = pcn;
	    proposal.isap_protoid = p->protoid;
	    proposal.isap_spisize = oakley_mode ? 0
		: p->protoid == PROTO_IPCOMP ? IPCOMP_CPI_SIZE
		: IPSEC_DOI_SPI_SIZE;
#ifndef NO_KERNEL_ALG
	    /*	
	     *	In quick mode ONLY, create proposal for
	     *	runtime kernel algos
	     *
	     *  replace ESP proposals
	     *  with runtime created one
	     */
	    if (!oakley_mode && p->protoid==PROTO_IPSEC_ESP) {
		    DBG(DBG_CONTROL | DBG_CRYPT, 
			if (st->st_connection->alg_info_esp) {
			    static char buf[256]="";
			    alg_info_snprint(buf, sizeof (buf), 
				    (struct alg_info *)st->st_connection->alg_info_esp);
			    DBG_log(buf);
			 }
		    );
		    db_ctx=kernel_alg_db_new(st->st_connection->alg_info_esp, st->st_policy);
		    p = db_prop_get(db_ctx);

		    if (!p || p->trans_cnt==0) {
			loglog(RC_LOG_SERIOUS, 
				"empty IPSEC SA proposal to send "
				"(no kernel algorithms for esp selection)");
			return_on(ret, FALSE);
		    }
	    }
#endif
#ifndef NO_IKE_ALG
	    if (oakley_mode && p->protoid==PROTO_ISAKMP) {
		    DBG(DBG_CONTROL | DBG_CRYPT, 
			if (st->st_connection->alg_info_ike) {
			    static char buf[256]="";
			    alg_info_snprint(buf, sizeof (buf), 
				    (struct alg_info *)st->st_connection->alg_info_ike);
			    DBG_log(buf);
			 }
		    );
		    db_ctx=ike_alg_db_new(st->st_connection->alg_info_ike,
				st->st_policy);
		    p = db_prop_get(db_ctx);
		    if (!p || p->trans_cnt==0) {
			loglog(RC_LOG_SERIOUS, 
					"empty ISAKMP SA proposal to send "
					"(no algorithms for ike selection?)");
			return_on(ret, FALSE);
		    }
	    }
#endif
	    proposal.isap_notrans = p->trans_cnt;
	    if (!out_struct(&proposal, &isakmp_proposal_desc, &sa_pbs, &proposal_pbs))
		return_on(ret, FALSE);

	    /* Per-protocols stuff:
	     * Set trans_desc.
	     * Set attr_desc.
	     * Set attr_val_descs.
	     * If not oakley_mode, emit SPI.
	     * We allocate SPIs on demand.
	     * All ESPs in an SA will share a single SPI.
	     * All AHs in an SAwill share a single SPI.
	     * AHs' SPI will be distinct from ESPs'.
	     * This latter is needed because KLIPS doesn't
	     * use the protocol when looking up a (dest, protocol, spi).
	     * ??? If multiple ESPs are composed, how should their SPIs
	     * be allocated?
	     */
	    {
		ipsec_spi_t *spi_ptr = NULL;
		bool *spi_generated;

		switch (p->protoid)
		{
		case PROTO_ISAKMP:
		    passert(oakley_mode);
		    trans_desc = &isakmp_isakmp_transform_desc;
		    attr_desc = &isakmp_oakley_attribute_desc;
		    attr_val_descs = oakley_attr_val_descs;
		    /* no SPI needed */
		    break;
		case PROTO_IPSEC_AH:
		    passert(!oakley_mode);
		    trans_desc = &isakmp_ah_transform_desc;
		    attr_desc = &isakmp_ipsec_attribute_desc;
		    attr_val_descs = ipsec_attr_val_descs;
		    spi_ptr = &st->st_ah.our_spi;
		    spi_generated = &ah_spi_generated;
		    break;
		case PROTO_IPSEC_ESP:
		    passert(!oakley_mode);
		    trans_desc = &isakmp_esp_transform_desc;
		    attr_desc = &isakmp_ipsec_attribute_desc;
		    attr_val_descs = ipsec_attr_val_descs;
		    spi_ptr = &st->st_esp.our_spi;
		    spi_generated = &esp_spi_generated;
		    break;
		case PROTO_IPCOMP:
		    passert(!oakley_mode);
		    trans_desc = &isakmp_ipcomp_transform_desc;
		    attr_desc = &isakmp_ipsec_attribute_desc;
		    attr_val_descs = ipsec_attr_val_descs;

		    /* a CPI isn't quite the same as an SPI
		     * so we use specialized code to emit it.
		     */
		    if (!ipcomp_cpi_generated)
		    {
			st->st_ipcomp.our_spi = get_my_cpi();
			if (st->st_ipcomp.our_spi == 0)
			    return_on(ret, FALSE);	/* problem generating CPI */

			ipcomp_cpi_generated = TRUE;
		    }
		    /* CPI is stored in network low order end of an
		     * ipsec_spi_t.  So we start a couple of bytes in.
		     */
		    if (!out_raw((u_char *)&st->st_ipcomp.our_spi
		     + IPSEC_DOI_SPI_SIZE - IPCOMP_CPI_SIZE
		    , IPCOMP_CPI_SIZE
		    , &proposal_pbs, "CPI"))
			return_on(ret, FALSE);
		    break;
		default:
		    impossible();
		}
		if (spi_ptr != NULL)
		{
		    if (!*spi_generated)
		    {
			*spi_ptr = get_ipsec_spi(0);
			*spi_generated = TRUE;
		    }
		    if (!out_raw((u_char *)spi_ptr, IPSEC_DOI_SPI_SIZE
		    , &proposal_pbs, "SPI"))
			return_on(ret, FALSE);
		}
	    }

	    /* within proposal: Transform Payloads */
	    for (tn = 0; tn != p->trans_cnt; tn++)
	    {
		struct db_trans *t = &p->trans[tn];
		pb_stream trans_pbs;
		struct isakmp_transform trans;
		int an;

		trans.isat_np = (tn == p->trans_cnt - 1)
		    ? ISAKMP_NEXT_NONE : ISAKMP_NEXT_T;
		trans.isat_transnum = tn;
		trans.isat_transid = t->transid;
		if (!out_struct(&trans, trans_desc, &proposal_pbs, &trans_pbs))
		    return_on(ret, FALSE);

		/* Within tranform: Attributes. */

		/* For Phase 2 / Quick Mode, GROUP_DESCRIPTION is
		 * automatically generated because it must be the same
		 * in every transform.  Except IPCOMP.
		 */
		if (p->protoid != PROTO_IPCOMP
		&& st->st_pfs_group != NULL)
		{
		    passert(!oakley_mode);
		    passert(st->st_pfs_group != &unset_group);
		    out_attr(GROUP_DESCRIPTION, st->st_pfs_group->group
			, attr_desc, attr_val_descs
			, &trans_pbs);
		}

		/* automatically generate duration
		 * and, for Phase 2 / Quick Mode, encapsulation.
		 */
		if (oakley_mode)
		{
		    out_attr(OAKLEY_LIFE_TYPE, OAKLEY_LIFE_SECONDS
			, attr_desc, attr_val_descs
			, &trans_pbs);
		    out_attr(OAKLEY_LIFE_DURATION
			, st->st_connection->sa_ike_life_seconds
			, attr_desc, attr_val_descs
			, &trans_pbs);
		}
		else
		{
		    /* RFC 2407 (IPSEC DOI) 4.5 specifies that
		     * the default is "unspecified (host-dependent)".
		     * This makes little sense, so we always specify it.
		     *
		     * Unlike other IPSEC transforms, IPCOMP defaults
		     * to Transport Mode, so we can exploit the default
		     * (draft-shacham-ippcp-rfc2393bis-05.txt 4.1).
		     */
		    if (p->protoid != PROTO_IPCOMP
		    || st->st_policy & POLICY_TUNNEL)
		    {
			out_attr(ENCAPSULATION_MODE
#ifdef NAT_TRAVERSAL
			    , NAT_T_ENCAPSULATION_MODE(st, st->st_policy)
#else
			    , st->st_policy & POLICY_TUNNEL
			      ? ENCAPSULATION_MODE_TUNNEL : ENCAPSULATION_MODE_TRANSPORT
#endif
			    , attr_desc, attr_val_descs
			    , &trans_pbs);
		    }
		    out_attr(SA_LIFE_TYPE, SA_LIFE_TYPE_SECONDS
			, attr_desc, attr_val_descs
			, &trans_pbs);
		    out_attr(SA_LIFE_DURATION
			, st->st_connection->sa_ipsec_life_seconds
			, attr_desc, attr_val_descs
			, &trans_pbs);
		}

		/* spit out attributes from table */
		for (an = 0; an != t->attr_cnt; an++)
		{
		    struct db_attr *a = &t->attrs[an];

		    out_attr(a->type, a->val
			, attr_desc, attr_val_descs
			, &trans_pbs);
		}

		close_output_pbs(&trans_pbs);
	    }
	    close_output_pbs(&proposal_pbs);
	}
	/* end of a conjunction of proposals */
    }
    close_output_pbs(&sa_pbs);
    ret = TRUE;

return_out:

#if !defined NO_KERNEL_ALG || !defined NO_IKE_ALG
    if (db_ctx)
	    db_destroy(db_ctx);
#endif
    return ret;
}

/* Handle long form of duration attribute.
 * The code is can only handle values that can fit in unsigned long.
 * "Clamping" is probably an acceptable way to impose this limitation.
 */
static u_int32_t
decode_long_duration(pb_stream *pbs)
{
    u_int32_t val = 0;

    /* ignore leading zeros */
    while (pbs_left(pbs) != 0 && *pbs->cur == '\0')
	pbs->cur++;

    if (pbs_left(pbs) > sizeof(val))
    {
	/* "clamp" too large value to max representable value */
	val -= 1;	/* portable way to get to maximum value */
	DBG(DBG_PARSING, DBG_log("   too large duration clamped to: %lu"
	    , (unsigned long)val));
    }
    else
    {
	/* decode number */
	while (pbs_left(pbs) != 0)
	    val = (val << BITS_PER_BYTE) | *pbs->cur++;
	DBG(DBG_PARSING, DBG_log("   long duration: %lu", (unsigned long)val));
    }
    return val;
}

/* Parse the body of an ISAKMP SA Payload (i.e. Phase 1 / Main Mode).
 * Various shortcuts are taken.  In particular, the policy, such as
 * it is, is hardwired.
 *
 * If r_sa is non-NULL, the body of an SA representing the selected
 * proposal is emitted.
 *
 * If "selection" is true, the SA is supposed to represent the
 * single tranform that the peer has accepted.
 * ??? We only check that it is acceptable, not that it is one that we offered!
 *
 * Only IPsec DOI is accepted (what is the ISAKMP DOI?).
 * Error response is rudimentary.
 *
 * This routine is used by main_inI1_outR1() and main_inR1_outI2().
 */
notification_t
parse_isakmp_sa_body(
    pb_stream *sa_pbs,	/* body of input SA Payload */
    const struct isakmp_sa *sa,	/* header of input SA Payload */
    pb_stream *r_sa_pbs,	/* if non-NULL, where to emit winning SA */
    bool selection,	/* if this SA is a selection, only one tranform can appear */
    struct state *st)	/* current state object */
{
#ifndef NO_IKE_ALG
    struct connection *c = st->st_connection;
#endif
    u_int32_t ipsecdoisit;
    pb_stream proposal_pbs;
    struct isakmp_proposal proposal;
    unsigned no_trans_left;
    int last_transnum;

    /* DOI */
    if (sa->isasa_doi != ISAKMP_DOI_IPSEC)
    {
	DBG_reject_loglog(RC_LOG_SERIOUS, "Unknown/unsupported DOI %s",
	    enum_show(&doi_names, sa->isasa_doi));
	/* XXX Could send notification back */
	return DOI_NOT_SUPPORTED;
    }

    /* Situation */
    if (!in_struct(&ipsecdoisit, &ipsec_sit_desc, sa_pbs, NULL))
    {
	DBG(DBG_IKE_REJECT, log_reject("situation not supported"));
	return SITUATION_NOT_SUPPORTED;
    }

    if (ipsecdoisit != SIT_IDENTITY_ONLY)
    {
	DBG_reject_loglog(RC_LOG_SERIOUS, "unsupported IPsec DOI situation (%s)"
	    , bitnamesof(sit_bit_names, ipsecdoisit));
	/* XXX Could send notification back */
	return SITUATION_NOT_SUPPORTED;
    }

    /* The rules for ISAKMP SAs are scattered.
     * draft-ietf-ipsec-isakmp-oakley-07.txt section 5 says that there
     * can only be one SA, and it can have only one proposal in it.
     * There may well be multiple transforms.
     */
    if (!in_struct(&proposal, &isakmp_proposal_desc, sa_pbs, &proposal_pbs))
    {
	DBG(DBG_IKE_REJECT, log_reject("payload malformed"));
	return PAYLOAD_MALFORMED;
    }

    if (proposal.isap_np != ISAKMP_NEXT_NONE)
    {
	DBG_reject_loglog(RC_LOG_SERIOUS,
	    "Proposal Payload must be alone in Oakley SA; found %s following Proposal"
	    , enum_show(&payload_names, proposal.isap_np));
	return PAYLOAD_MALFORMED;
    }

    if (proposal.isap_protoid != PROTO_ISAKMP)
    {
	DBG_reject_loglog(RC_LOG_SERIOUS,
	    "unexpected Protocol ID (%s) found in Oakley Proposal"
	    , enum_show(&protocol_names, proposal.isap_protoid));
	return INVALID_PROTOCOL_ID;
    }

    /* Just what should we accept for the SPI field?
     * The RFC is sort of contradictory.  We will ignore the SPI
     * as long as it is of the proper size.
     *
     * From RFC2408 2.4 Identifying Security Associations:
     *   During phase 1 negotiations, the initiator and responder cookies
     *   determine the ISAKMP SA. Therefore, the SPI field in the Proposal
     *   payload is redundant and MAY be set to 0 or it MAY contain the
     *   transmitting entity's cookie.
     *
     * From RFC2408 3.5 Proposal Payload:
     *    o  SPI Size (1 octet) - Length in octets of the SPI as defined by
     *       the Protocol-Id.  In the case of ISAKMP, the Initiator and
     *       Responder cookie pair from the ISAKMP Header is the ISAKMP SPI,
     *       therefore, the SPI Size is irrelevant and MAY be from zero (0) to
     *       sixteen (16).  If the SPI Size is non-zero, the content of the
     *       SPI field MUST be ignored.  If the SPI Size is not a multiple of
     *       4 octets it will have some impact on the SPI field and the
     *       alignment of all payloads in the message.  The Domain of
     *       Interpretation (DOI) will dictate the SPI Size for other
     *       protocols.
     */
    if (proposal.isap_spisize == 0)
    {
	/* empty (0) SPI -- fine */
    }
    else if (proposal.isap_spisize <= MAX_ISAKMP_SPI_SIZE)
    {
	u_char junk_spi[MAX_ISAKMP_SPI_SIZE];

	if (!in_raw(junk_spi, proposal.isap_spisize, &proposal_pbs, "Oakley SPI"))
	{
	    DBG(DBG_IKE_REJECT, log_reject("payload malformed"));
	    return PAYLOAD_MALFORMED;
	}
    }
    else
    {
	DBG_reject_loglog(RC_LOG_SERIOUS, "invalid SPI size (%u) in Oakley Proposal"
	    , (unsigned)proposal.isap_spisize);
	return INVALID_SPI;
    }

    if (selection && proposal.isap_notrans != 1)
    {
	DBG_reject_loglog(RC_LOG_SERIOUS,
	    "a single Transform is required in a selecting Oakley Proposal; found %u"
	    , (unsigned)proposal.isap_notrans);
	return BAD_PROPOSAL_SYNTAX;
    }

    /* for each transform payload... */

    last_transnum = -1;
    no_trans_left = proposal.isap_notrans;
    for (;;)
    {
	pb_stream trans_pbs;
	u_char *attr_start;
	size_t attr_len;
	struct isakmp_transform trans;
	lset_t seen_attrs = 0
	    , seen_durations = 0;
	u_int16_t life_type;
	struct oakley_trans_attrs ta;
	u_int16_t ta_enckey_len = 0;
	err_t ugh = NULL;	/* set to diagnostic when problem detected */

	/* initialize only optional field in ta */
	ta.life_seconds = OAKLEY_ISAKMP_SA_LIFETIME_DEFAULT;	/* When this SA expires (seconds) */

	if (no_trans_left == 0)
	{
	    DBG_reject_loglog(RC_LOG_SERIOUS,
		"number of Transform Payloads disagrees with Oakley Proposal Payload");
	    return BAD_PROPOSAL_SYNTAX;
	}

	if (!in_struct(&trans, &isakmp_isakmp_transform_desc, &proposal_pbs, &trans_pbs))
	{
	    DBG(DBG_IKE_REJECT, log_reject("bad proposal syntax"));
	    return BAD_PROPOSAL_SYNTAX;
	}

	if (trans.isat_transnum <= last_transnum)
	{
	    /* picky, picky, picky */
	    DBG_reject_loglog(RC_LOG_SERIOUS,
		"Transform Numbers are not monotonically increasing"
		" in Oakley Proposal");
	    return BAD_PROPOSAL_SYNTAX;
	}
	last_transnum = trans.isat_transnum;

	if (trans.isat_transid != KEY_IKE)
	{
	    DBG_reject_loglog(RC_LOG_SERIOUS,
		"expected KEY_IKE but found %s in Oakley Transform"
		, enum_show(&isakmp_transformid_names, trans.isat_transid));
	    return INVALID_TRANSFORM_ID;
	}

	attr_start = trans_pbs.cur;
	attr_len = pbs_left(&trans_pbs);

	/* process all the attributes that make up the transform */

	while (pbs_left(&trans_pbs) != 0)
	{
	    struct isakmp_attribute a;
	    pb_stream attr_pbs;
	    u_int32_t val;	/* room for larger values */

	    if (!in_struct(&a, &isakmp_oakley_attribute_desc, &trans_pbs, &attr_pbs))
	    {
		DBG(DBG_IKE_REJECT, log_reject("bad proposal syntax"));
		return BAD_PROPOSAL_SYNTAX;
	    }

	    passert((a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK) < 32);

	    if (seen_attrs & LELEM(a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK))
	    {
		DBG_reject_loglog(RC_LOG_SERIOUS,
		    "repeated %s attribute in Oakley Transform %u"
		    , enum_show(&oakley_attr_names, a.isaat_af_type)
		    , trans.isat_transnum);
		return BAD_PROPOSAL_SYNTAX;
	    }

	    seen_attrs |= LELEM(a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK);

	    val = a.isaat_lv;

	    DBG(DBG_PARSING,
	    {
		enum_names *vdesc = oakley_attr_val_descs
		    [a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK];

		if (vdesc != NULL)
		{
		    const char *nm = enum_name(vdesc, val);

		    if (nm != NULL)
			DBG_log("   [%u is %s]", (unsigned)val, nm);
		}
	    });

	    switch (a.isaat_af_type)
	    {
		case OAKLEY_ENCRYPTION_ALGORITHM | ISAKMP_ATTR_AF_TV:
#ifndef NO_IKE_ALG
		    if (ike_alg_enc_ok(val, ta_enckey_len, c->alg_info_ike, &ugh)) {
		    /* if (ike_alg_enc_present(val)) { */
			ta.encrypt = val;
			ta.encrypter = crypto_get_encrypter(val);
			ta.enckeylen = ta_enckey_len ? ta_enckey_len : ta.encrypter->keydeflen;
		    } else 
#endif
		    {
			if ((val==OAKLEY_3DES_CBC &&
			    st->st_connection->openrg_main_policy &
			    OPENRG_ENC_3DES)
#ifdef USE_SINGLE_DES
			    || (val==OAKLEY_DES_CBC &&
			    st->st_connection->openrg_main_policy &
			    OPENRG_ENC_1DES)
#endif
			   )
			{
			    ta.encrypt = val;
			    ta.encrypter = crypto_get_encrypter(val);
			    break;
			}
			else if (val==OAKLEY_3DES_CBC
#ifdef USE_SINGLE_DES
			    || val==OAKLEY_DES_CBC
#endif
			    )
			{
			    ugh = builddiag("%s is disabled",
				enum_show(&oakley_enc_names, val));
			}
			else
			{
			    ugh = builddiag("%s is not supported",
				enum_show(&oakley_enc_names, val));
			}
		    }
		    break;

		case OAKLEY_HASH_ALGORITHM | ISAKMP_ATTR_AF_TV:
#ifndef NO_IKE_ALG
		    if (ike_alg_hash_present(val)) {
			ta.hash = val;
			ta.hasher = crypto_get_hasher(val);
		    } else 
#endif
/* #else */
		    {
			if ((val==OAKLEY_MD5 &&
			    st->st_connection->openrg_main_policy&OPENRG_AUTH_MD5)
			    || (val==OAKLEY_SHA &&
			    st->st_connection->openrg_main_policy &
			    OPENRG_AUTH_SHA))
			{
			    ta.hash = val;
			    ta.hasher = crypto_get_hasher(val);
			    break;
			}
			else if (val==OAKLEY_MD5 || val==OAKLEY_SHA)
			{
			    ugh = builddiag("%s is disabled",
				enum_show(&oakley_hash_names, val));
			}
			else
			{
			    ugh = builddiag("%s is not supported",
				enum_show(&oakley_hash_names, val));
			}
		    }
		    break;

		case OAKLEY_AUTHENTICATION_METHOD | ISAKMP_ATTR_AF_TV:
		    {
		    /* check that authentication method is acceptable */
		    lset_t iap = st->st_policy & POLICY_ID_AUTH_MASK;

		    switch (val)
		    {
		    case OAKLEY_PRESHARED_KEY:
			if ((iap & POLICY_PSK) == LEMPTY)
			{
			    ugh = "policy does not allow OAKLEY_PRESHARED_KEY authentication";
			}
			else
			{
			    /* check that we can find a preshared secret */
			    struct connection *c = st->st_connection;

			    if (get_preshared_secret(c) == NULL)
			    {
				char mid[IDTOA_BUF]
				    , hid[IDTOA_BUF];

				idtoa(&c->this.id, mid, sizeof(mid));
				if (his_id_was_instantiated(c))
				    strcpy(hid, "%any");
				else
				    idtoa(&c->that.id, hid, sizeof(hid));
				ugh = builddiag("Can't authenticate: no preshared key found for `%s' and `%s'"
				    , mid, hid);
			    }
			    ta.auth = val;
			}
			break;
		    case OAKLEY_RSA_SIG:
			/* Accept if policy specifies RSASIG or is default */
			if ((iap & POLICY_RSASIG) == LEMPTY)
			{
			    ugh = "policy does not allow OAKLEY_RSA_SIG authentication";
			}
			else
			{
			    /* We'd like to check that we can find a public
			     * key for him and a private key for us that is
			     * suitable, but we don't yet have his
			     * Id Payload, so it seems futile to try.
			     * We can assume that if he proposes it, he
			     * thinks we've got it.  If we proposed it,
			     * perhaps we know what we're doing.
			     */
			    ta.auth = val;
			}
			break;

		    default:
			ugh = builddiag("Pluto does not support %s authentication"
			    , enum_show(&oakley_auth_names, val));
			break;
		    }
		    }
		    break;

		case OAKLEY_GROUP_DESCRIPTION | ISAKMP_ATTR_AF_TV:
		    ta.group = lookup_group(val);
		    if (ta.group == NULL)
		    {
			ugh = "only OAKLEY_GROUP_MODP1024,1536,2048,3072,4096,6144,8192 supported";
			break;
		    }
		    if ((val==OAKLEY_GROUP_MODP768 &&
			st->st_connection->openrg_main_policy &
			OPENRG_MODP_768) ||
			(val==OAKLEY_GROUP_MODP1024 &&
			st->st_connection->openrg_main_policy &
			OPENRG_MODP_1024) ||
			(val==OAKLEY_GROUP_MODP1536 &&
			st->st_connection->openrg_main_policy &
			OPENRG_MODP_1536))
		    {
			break;
		    }
		    ugh = builddiag("%s is disabled",
			enum_show(&oakley_group_names, val));
		    break;

		case OAKLEY_LIFE_TYPE | ISAKMP_ATTR_AF_TV:
		    switch (val)
		    {
		    case OAKLEY_LIFE_SECONDS:
		    case OAKLEY_LIFE_KILOBYTES:
			if (seen_durations & LELEM(val))
			{
			    DBG_reject_loglog(RC_LOG_SERIOUS
				, "attribute OAKLEY_LIFE_TYPE value %s repeated"
				, enum_show(&oakley_lifetime_names, val));
			    return BAD_PROPOSAL_SYNTAX;
			}
			seen_durations |= LELEM(val);
			life_type = val;
			break;
		    default:
			ugh = builddiag("unknown value %s"
			    , enum_show(&oakley_lifetime_names, val));
			break;
		    }
		    break;

		case OAKLEY_LIFE_DURATION | ISAKMP_ATTR_AF_TLV:
		    val = decode_long_duration(&attr_pbs);
		    /* fall through */
		case OAKLEY_LIFE_DURATION | ISAKMP_ATTR_AF_TV:
		    if ((seen_attrs & LELEM(OAKLEY_LIFE_TYPE)) == 0)
		    {
			ugh = "OAKLEY_LIFE_DURATION attribute not preceded by OAKLEY_LIFE_TYPE attribute";
			break;
		    }
		    seen_attrs &= ~(LELEM(OAKLEY_LIFE_DURATION) | LELEM(OAKLEY_LIFE_TYPE));

		    switch (life_type)
		    {
			case OAKLEY_LIFE_SECONDS:
			    if (val > OAKLEY_ISAKMP_SA_LIFETIME_MAXIMUM)
				ugh = builddiag("peer requested %lu seconds"
				    " which exceeds our limit %d seconds"
				    , (long) val
				    , OAKLEY_ISAKMP_SA_LIFETIME_MAXIMUM);
			    ta.life_seconds = val;
			    break;
			case OAKLEY_LIFE_KILOBYTES:
			    ta.life_kilobytes = val;
			    break;
			default:
			    impossible();
		    }
		    break;

#ifndef NO_IKE_ALG
		case OAKLEY_KEY_LENGTH | ISAKMP_ATTR_AF_TV:
		    if ((seen_attrs & LELEM(OAKLEY_ENCRYPTION_ALGORITHM)) == 0)
		    {
			ugh = "OAKLEY_KEY_LENGTH attribute not preceded by OAKLEY_ENCRYPTION_ALGORITHM attribute";
			break;
		    }
		    if (ta.encrypter == NULL)
		    {
			ta_enckey_len = val;
		        break;
		    }
		    /*
		     * check if this keylen is compatible with 
		     * specified alg_info_ike
		     */
		    if (!ike_alg_enc_ok(ta.encrypt, val, c->alg_info_ike, &ugh)) {
			ugh = "peer proposed key_len not valid for encrypt algo setup specified";
		    }
		    ta.enckeylen=val;
		    break;
#else
		case OAKLEY_KEY_LENGTH | ISAKMP_ATTR_AF_TV:
#endif
#if 0 /* not yet supported */
		case OAKLEY_GROUP_TYPE | ISAKMP_ATTR_AF_TV:
		case OAKLEY_PRF | ISAKMP_ATTR_AF_TV:
		case OAKLEY_FIELD_SIZE | ISAKMP_ATTR_AF_TV:

		case OAKLEY_GROUP_PRIME | ISAKMP_ATTR_AF_TV:
		case OAKLEY_GROUP_PRIME | ISAKMP_ATTR_AF_TLV:
		case OAKLEY_GROUP_GENERATOR_ONE | ISAKMP_ATTR_AF_TV:
		case OAKLEY_GROUP_GENERATOR_ONE | ISAKMP_ATTR_AF_TLV:
		case OAKLEY_GROUP_GENERATOR_TWO | ISAKMP_ATTR_AF_TV:
		case OAKLEY_GROUP_GENERATOR_TWO | ISAKMP_ATTR_AF_TLV:
		case OAKLEY_GROUP_CURVE_A | ISAKMP_ATTR_AF_TV:
		case OAKLEY_GROUP_CURVE_A | ISAKMP_ATTR_AF_TLV:
		case OAKLEY_GROUP_CURVE_B | ISAKMP_ATTR_AF_TV:
		case OAKLEY_GROUP_CURVE_B | ISAKMP_ATTR_AF_TLV:
		case OAKLEY_GROUP_ORDER | ISAKMP_ATTR_AF_TV:
		case OAKLEY_GROUP_ORDER | ISAKMP_ATTR_AF_TLV:
#endif
		default:
		    ugh = "unsupported OAKLEY attribute";
		    break;
	    }

	    if (ugh != NULL)
	    {
		loglog(RC_LOG_SERIOUS, "%s.  Attribute %s"
		    , ugh, enum_show(&oakley_attr_names, a.isaat_af_type));
		break;
	    }
	}

#ifndef NO_IKE_ALG
	/* 
	 * ML: at last check for allowed transforms in alg_info_ike 
	 *     (ALG_INFO_F_STRICT flag)
	 */
	if (ugh == NULL)
	{
	        if (!ta.encrypt && ta_enckey_len)
			ugh = "NULL encrypter with seen OAKLEY_ENCRYPTION_ALGORITHM";
		else if (!ike_alg_ok_final(ta.encrypt, ta.enckeylen, ta.hash,
			ta.group ? ta.group->group : -1, c->alg_info_ike)) {
			ugh = "OAKLEY proposal refused";
		}
	}
#endif

	if (ugh == NULL)
	{
	    /* a little more checking is in order */
	    {
		unsigned long missing
		    = ~seen_attrs
		    & (LELEM(OAKLEY_ENCRYPTION_ALGORITHM)
		      | LELEM(OAKLEY_HASH_ALGORITHM)
		      | LELEM(OAKLEY_AUTHENTICATION_METHOD)
		      | LELEM(OAKLEY_GROUP_DESCRIPTION));

		if (missing)
		{
		    DBG_reject_loglog(RC_LOG_SERIOUS,
			"missing mandatory attribute(s) %s in Oakley Transform %u"
			, bitnamesof(oakley_attr_bit_names, missing)
			, trans.isat_transnum);
		    return BAD_PROPOSAL_SYNTAX;
		}
	    }
	    /* We must have liked this transform.
	     * Lets finish early and leave.
	     */

	    DBG(DBG_PARSING | DBG_CRYPT
		, DBG_log("Oakley Transform %u accepted", trans.isat_transnum));

	    if (r_sa_pbs != NULL)
	    {
		struct isakmp_proposal r_proposal = proposal;
		pb_stream r_proposal_pbs;
		struct isakmp_transform r_trans = trans;
		pb_stream r_trans_pbs;

		/* Situation */
		if (!out_struct(&ipsecdoisit, &ipsec_sit_desc, r_sa_pbs, NULL))
		    impossible();

		/* Proposal */
#ifdef EMIT_ISAKMP_SPI
		r_proposal.isap_spisize = COOKIE_SIZE;
#else
		r_proposal.isap_spisize = 0;
#endif
		r_proposal.isap_notrans = 1;
		if (!out_struct(&r_proposal, &isakmp_proposal_desc, r_sa_pbs, &r_proposal_pbs))
		    impossible();

		/* SPI */
#ifdef EMIT_ISAKMP_SPI
		if (!out_raw(my_cookie, COOKIE_SIZE, &r_proposal_pbs, "SPI"))
		    impossible();
		r_proposal.isap_spisize = COOKIE_SIZE;
#else
		/* none (0) */
#endif

		/* Transform */
		r_trans.isat_np = ISAKMP_NEXT_NONE;
		if (!out_struct(&r_trans, &isakmp_isakmp_transform_desc, &r_proposal_pbs, &r_trans_pbs))
		    impossible();

		if (!out_raw(attr_start, attr_len, &r_trans_pbs, "attributes"))
		    impossible();
		close_output_pbs(&r_trans_pbs);
		close_output_pbs(&r_proposal_pbs);
		close_output_pbs(r_sa_pbs);
	    }

	    /* ??? If selection, we used to save the proposal in state.
	     * We never used it.  From proposal_pbs.start,
	     * length pbs_room(&proposal_pbs)
	     */

	    /* copy over the results */
	    st->st_oakley = ta;
	    return NOTHING_WRONG;
	}

	/* on to next transform */
	no_trans_left--;

	if (trans.isat_np == ISAKMP_NEXT_NONE)
	{
	    if (no_trans_left != 0)
	    {
		DBG_reject_loglog(RC_LOG_SERIOUS,
		    "number of Transform Payloads disagrees with Oakley Proposal Payload");
		return BAD_PROPOSAL_SYNTAX;
	    }
	    break;
	}
	if (trans.isat_np != ISAKMP_NEXT_T)
	{
	    DBG_reject_loglog(RC_LOG_SERIOUS,
		"unexpected %s payload in Oakley Proposal"
		, enum_show(&payload_names, proposal.isap_np));
	    return BAD_PROPOSAL_SYNTAX;
	}
    }
    DBG_reject_loglog(RC_LOG_SERIOUS, "no acceptable Oakley Transform");
    return NO_PROPOSAL_CHOSEN;
}

/* Initialize st_oakley field of state for use when initiating in
 * aggressive mode.
 *
 * This should probably get more of its parameters, like what group to use,
 * from the connection specification, but it's not there yet.
 * This should ideally be done by passing them via whack.
 * 
 */
bool
init_st_oakley(struct state *st, lset_t openrg_policy)
{
    struct oakley_trans_attrs ta;
    lset_t policy;

    policy &= POLICY_ISAKMP_MASK;

    /* When this SA expires (seconds) */
    ta.life_seconds = st->st_connection->sa_ike_life_seconds;
    ta.life_kilobytes = 1000000;

    policy = aggr_get_oakley_policy(rg_oakley_encrypt, openrg_policy);
    if (!policy)
	return FALSE;
    ta.encrypt = policy;
    ta.encrypter = crypto_get_encrypter(policy);
    policy = aggr_get_oakley_policy(rg_oakley_hash, openrg_policy);
    if (!policy)
	return FALSE;
    ta.hash = policy;
    ta.hasher = crypto_get_hasher(policy);

    /* XXX. Now preshared key supported for aggressive mode.
     * policy&POLICY_PSK or policy&POLICY_RSASIG must be added.
     */
    ta.auth = OAKLEY_PRESHARED_KEY;

    policy = aggr_get_oakley_policy(rg_oakley_group, openrg_policy);
    if (!policy)
	return FALSE;
    ta.group = lookup_group(policy);

    st->st_oakley = ta;

    return TRUE;
}


/* Parse the body of an IPsec SA Payload (i.e. Phase 2 / Quick Mode).
 *
 * The main routine is parse_ipsec_sa_body; other functions defined
 * between here and there are just helpers.
 *
 * Various shortcuts are taken.  In particular, the policy, such as
 * it is, is hardwired.
 *
 * If r_sa is non-NULL, the body of an SA representing the selected
 * proposal is emitted into it.
 *
 * If "selection" is true, the SA is supposed to represent the
 * single tranform that the peer has accepted.
 * ??? We only check that it is acceptable, not that it is one that we offered!
 *
 * Only IPsec DOI is accepted (what is the ISAKMP DOI?).
 * Error response is rudimentary.
 *
 * Since all ISAKMP groups in all SA Payloads must match, st->st_pfs_group
 * holds this across multiple payloads.
 * &unset_group signifies not yet "set"; NULL signifies NONE.
 *
 * This routine is used by quick_inI1_outR1() and quick_inR1_outI2().
 */

static const struct ipsec_trans_attrs null_ipsec_trans_attrs = {
    0,					/* transid (NULL, for now) */
    0,					/* spi */
    SA_LIFE_DURATION_DEFAULT,		/* life_seconds */
    SA_LIFE_DURATION_K_DEFAULT,		/* life_kilobytes */
    ENCAPSULATION_MODE_UNSPECIFIED,	/* encapsulation */
    AUTH_ALGORITHM_NONE,		/* auth */
    0,					/* key_len */
    0,					/* key_rounds */
};

static bool
parse_ipsec_transform(struct isakmp_transform *trans
, struct ipsec_trans_attrs *attrs
, pb_stream *prop_pbs
, pb_stream *trans_pbs
, struct_desc *trans_desc
, int previous_transnum	/* or -1 if none */
, bool selection
, bool is_last
, bool is_ipcomp
, struct state *st)	/* current state object */
{
    lset_t seen_attrs = 0
	, seen_durations = 0;
    u_int16_t life_type;
    const struct oakley_group_desc *pfs_group = NULL;

    if (!in_struct(trans, trans_desc, prop_pbs, trans_pbs))
	return FALSE;

    if (trans->isat_transnum <= previous_transnum)
    {
	loglog(RC_LOG_SERIOUS, "Transform Numbers in Proposal are not monotonically increasing");
	return FALSE;
    }

    switch (trans->isat_np)
    {
	case ISAKMP_NEXT_T:
	    if (is_last)
	    {
		loglog(RC_LOG_SERIOUS, "Proposal Payload has more Transforms than specified");
		return FALSE;
	    }
	    break;
	case ISAKMP_NEXT_NONE:
	    if (!is_last)
	    {
		loglog(RC_LOG_SERIOUS, "Proposal Payload has fewer Transforms than specified");
		return FALSE;
	    }
	    break;
	default:
	    loglog(RC_LOG_SERIOUS, "expecting Transform Payload, but found %s in Proposal"
		, enum_show(&payload_names, trans->isat_np));
	    return FALSE;
    }

    *attrs = null_ipsec_trans_attrs;
    attrs->transid = trans->isat_transid;

    while (pbs_left(trans_pbs) != 0)
    {
	struct isakmp_attribute a;
	pb_stream attr_pbs;
	enum_names *vdesc;
	u_int32_t val;	/* room for larger value */
	bool ipcomp_inappropriate = is_ipcomp;	/* will get reset if OK */

	if (!in_struct(&a, &isakmp_ipsec_attribute_desc, trans_pbs, &attr_pbs))
	    return FALSE;

	passert((a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK) < 32);

	if (seen_attrs & LELEM(a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK))
	{
	    loglog(RC_LOG_SERIOUS, "repeated %s attribute in IPsec Transform %u"
		, enum_show(&ipsec_attr_names, a.isaat_af_type)
		, trans->isat_transnum);
	    return FALSE;
	}

	seen_attrs |= LELEM(a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK);

	val = a.isaat_lv;

	vdesc  = ipsec_attr_val_descs[a.isaat_af_type & ISAKMP_ATTR_RTYPE_MASK];
	if (vdesc != NULL)
	{
	    if (enum_name(vdesc, val) == NULL)
	    {
		loglog(RC_LOG_SERIOUS, "invalid value %u for attribute %s in IPsec Transform"
		    , (unsigned)val, enum_show(&ipsec_attr_names, a.isaat_af_type));
		return FALSE;
	    }
	    DBG(DBG_PARSING
		, if ((a.isaat_af_type & ISAKMP_ATTR_AF_MASK) == ISAKMP_ATTR_AF_TV)
		    DBG_log("   [%u is %s]"
			, (unsigned)val, enum_show(vdesc, val)));
	}

	switch (a.isaat_af_type)
	{
	    case SA_LIFE_TYPE | ISAKMP_ATTR_AF_TV:
		ipcomp_inappropriate = FALSE;
		if (seen_durations & LELEM(val))
		{
		    loglog(RC_LOG_SERIOUS, "attribute SA_LIFE_TYPE value %s repeated in message"
			, enum_show(&sa_lifetime_names, val));
		    return FALSE;
		}
		seen_durations |= LELEM(val);
		life_type = val;
		break;
	    case SA_LIFE_DURATION | ISAKMP_ATTR_AF_TLV:
		val = decode_long_duration(&attr_pbs);
		/* fall through */
	    case SA_LIFE_DURATION | ISAKMP_ATTR_AF_TV:
		ipcomp_inappropriate = FALSE;
		if ((seen_attrs & LELEM(SA_LIFE_DURATION)) == 0)
		{
		    loglog(RC_LOG_SERIOUS, "SA_LIFE_DURATION IPsec attribute not preceded by SA_LIFE_TYPE attribute");
		    return FALSE;
		}
		seen_attrs &= ~(LELEM(SA_LIFE_DURATION) | LELEM(SA_LIFE_TYPE));

		switch (life_type)
		{
		    case SA_LIFE_TYPE_SECONDS:
			/* silently limit duration to our maximum */
			attrs->life_seconds = val <= SA_LIFE_DURATION_MAXIMUM
			    ? val : SA_LIFE_DURATION_MAXIMUM;
			break;
		    case SA_LIFE_TYPE_KBYTES:
			attrs->life_kilobytes = val;
			break;
		    default:
			impossible();
		}
		break;
	    case GROUP_DESCRIPTION | ISAKMP_ATTR_AF_TV:
		if (is_ipcomp)
		{
		    /* Accept reluctantly.  Should not happen, according to
		     * draft-shacham-ippcp-rfc2393bis-05.txt 4.1.
		     */
		    ipcomp_inappropriate = FALSE;
		    loglog(RC_COMMENT
			, "IPCA (IPcomp SA) contains GROUP_DESCRIPTION."
			"  Ignoring inapproprate attribute.");
		}
		pfs_group = lookup_group(val);
		if (pfs_group == NULL)
		{
		    loglog(RC_LOG_SERIOUS, "only OAKLEY_GROUP_MODP1024,1536,2048,3072,4096,6144,8192 supported for PFS");
		    return FALSE;
		}
		break;
	    case ENCAPSULATION_MODE | ISAKMP_ATTR_AF_TV:
		ipcomp_inappropriate = FALSE;
#ifdef NAT_TRAVERSAL
		switch (val) {
			case ENCAPSULATION_MODE_TUNNEL:
			case ENCAPSULATION_MODE_TRANSPORT:
				if (st->hidden_variables.st_nat_traversal & NAT_T_DETECTED) {
					loglog(RC_LOG_SERIOUS,
						"%s must only be used if "
						"NAT-Traversal is not detected",
						enum_name(&enc_mode_names, val));
#ifdef I_CARE_OF_SSH_SENTINEL
					/*
					 * Accept it anyway because SSH-Sentinel does not
					 * use UDP_TUNNEL or UDP_TRANSPORT for the diagnostic.
					 *
					 * remove when SSH-Sentinel is fixed
					 */
					return FALSE;
#endif
				}
				attrs->encapsulation = val;
				break;

			case ENCAPSULATION_MODE_UDP_TRANSPORT_DRAFTS:
			case ENCAPSULATION_MODE_UDP_TUNNEL_DRAFTS:
				if (st->hidden_variables.st_nat_traversal & NAT_T_WITH_ENCAPSULATION_RFC_VALUES) {
					loglog(RC_LOG_SERIOUS,
						"%s must only be used with old IETF drafts",
						enum_name(&enc_mode_names, val));
					return FALSE;
				}
				else if (st->hidden_variables.st_nat_traversal & NAT_T_DETECTED) {
					attrs->encapsulation = val - ENCAPSULATION_MODE_UDP_TUNNEL_DRAFTS + ENCAPSULATION_MODE_TUNNEL;
				}
				else {
					loglog(RC_LOG_SERIOUS,
						"%s must only be used if "
						"NAT-Traversal is detected",
						enum_name(&enc_mode_names, val));
					return FALSE;
				}
				break;

			case ENCAPSULATION_MODE_UDP_TRANSPORT_RFC:
			case ENCAPSULATION_MODE_UDP_TUNNEL_RFC:
				if ((st->hidden_variables.st_nat_traversal & NAT_T_DETECTED) &&
					(st->hidden_variables.st_nat_traversal & NAT_T_WITH_ENCAPSULATION_RFC_VALUES)) {
					attrs->encapsulation = val - ENCAPSULATION_MODE_UDP_TUNNEL_RFC + ENCAPSULATION_MODE_TUNNEL;
				}
				else if (st->hidden_variables.st_nat_traversal & NAT_T_DETECTED) {
					loglog(RC_LOG_SERIOUS,
						"%s must only be used with NAT-T RFC",
						enum_name(&enc_mode_names, val));
					return FALSE;
				}
				else {
					loglog(RC_LOG_SERIOUS,
						"%s must only be used if "
						"NAT-Traversal is detected",
						enum_name(&enc_mode_names, val));
					return FALSE;
				}
				break;
			default:
				loglog(RC_LOG_SERIOUS,
					"unknown ENCAPSULATION_MODE %d in IPSec SA", val);
				return FALSE;
				break;
		}
#else
		attrs->encapsulation = val;
#endif
		break;
	    case AUTH_ALGORITHM | ISAKMP_ATTR_AF_TV:
		attrs->auth = val;
		break;
	    case KEY_LENGTH | ISAKMP_ATTR_AF_TV:
		attrs->key_len = val;
		break;
	    case KEY_ROUNDS | ISAKMP_ATTR_AF_TV:
		attrs->key_rounds = val;
		break;
#if 0 /* not yet implemented */
	    case COMPRESS_DICT_SIZE | ISAKMP_ATTR_AF_TV:
		break;
	    case COMPRESS_PRIVATE_ALG | ISAKMP_ATTR_AF_TV:
		break;

	    case SA_LIFE_DURATION | ISAKMP_ATTR_AF_TLV:
		break;
	    case COMPRESS_PRIVATE_ALG | ISAKMP_ATTR_AF_TLV:
		break;
#endif
	    default:
		loglog(RC_LOG_SERIOUS, "unsupported IPsec attribute %s"
		    , enum_show(&ipsec_attr_names, a.isaat_af_type));
		return FALSE;
	}
	if (ipcomp_inappropriate)
	{
	    loglog(RC_LOG_SERIOUS, "IPsec attribute %s inappropriate for IPCOMP"
		, enum_show(&ipsec_attr_names, a.isaat_af_type));
	    return FALSE;
	}
    }

    /* Although an IPCOMP SA (IPCA) ought not to have a pfs_group,
     * if it does, demand that it be consistent.
     * See draft-shacham-ippcp-rfc2393bis-05.txt 4.1.
     */
    if (!is_ipcomp || pfs_group != NULL)
    {
	if (st->st_pfs_group == &unset_group)
	    st->st_pfs_group = pfs_group;

	if (st->st_pfs_group != pfs_group)
	{
	    loglog(RC_LOG_SERIOUS, "GROUP_DESCRIPTION inconsistent with that of %s in IPsec SA"
		, selection? "the Proposal" : "a previous Transform");
	    return FALSE;
	}
    }
    else
    {
	if (st->st_pfs_group == &unset_group)
	{
	    loglog(RC_LOG_SERIOUS, "GROUP_DESCRIPTION inconsistent with that of %s "
		"in IPsec SA [line=%d]", selection? "the Proposal" :
		"a previous Transform", __LINE__);
	    return FALSE;
	}
    }

    if ((seen_attrs & LELEM(SA_LIFE_DURATION)) != 0)
    {
	loglog(RC_LOG_SERIOUS, "SA_LIFE_TYPE IPsec attribute not followed by SA_LIFE_DURATION attribute in message");
	return FALSE;
    }

    if ((seen_attrs & LELEM(ENCAPSULATION_MODE)) == 0)
    {
	/* ??? Technically, RFC 2407 (IPSEC DOI) 4.5 specifies that
	 * the default is "unspecified (host-dependent)".
	 * This makes little sense, so we demand that it be specified.
	 */
	loglog(RC_LOG_SERIOUS, "IPsec Transform must specify ENCAPSULATION_MODE");
	return FALSE;
    }
    else if ((attrs->encapsulation == ENCAPSULATION_MODE_TRANSPORT &&
	st->st_policy & POLICY_TUNNEL) ||
	(attrs->encapsulation == ENCAPSULATION_MODE_TUNNEL &&
	!(st->st_policy & POLICY_TUNNEL)))
    {
	loglog(RC_LOG_SERIOUS, "Peer's encapsulation mode doesn't match local "
	    "encapsulation mode");
	return FALSE;
    }

    /* ??? should check for key_len and/or key_rounds if required */

    return TRUE;
}

static void
echo_proposal(
    struct isakmp_proposal r_proposal,	/* proposal to emit */
    struct isakmp_transform r_trans,	/* winning transformation within it */
    u_int8_t np,			/* Next Payload for proposal */
    pb_stream *r_sa_pbs,		/* SA PBS into which to emit */
    struct ipsec_proto_info *pi,	/* info about this protocol instance */
    struct_desc *trans_desc,		/* descriptor for this transformation */
    pb_stream *trans_pbs)		/* PBS for incoming transform */
{
    pb_stream r_proposal_pbs;
    pb_stream r_trans_pbs;

    /* Proposal */
    r_proposal.isap_np = np;
    r_proposal.isap_notrans = 1;
    if (!out_struct(&r_proposal, &isakmp_proposal_desc, r_sa_pbs, &r_proposal_pbs))
	impossible();

    /* allocate and emit our CPI/SPI */
    if (r_proposal.isap_protoid == PROTO_IPCOMP)
    {
	/* CPI is stored in network low order end of an
	 * ipsec_spi_t.  So we start a couple of bytes in.
	 * Note: we may fail to generate a satisfactory CPI,
	 * but we'll ignore that.
	 */
	pi->our_spi = get_my_cpi();
	out_raw((u_char *) &pi->our_spi
	     + IPSEC_DOI_SPI_SIZE - IPCOMP_CPI_SIZE
	    , IPCOMP_CPI_SIZE
	    , &r_proposal_pbs, "CPI");
    }
    else
    {
	pi->our_spi = get_ipsec_spi(pi->attrs.spi);
	out_raw((u_char *) &pi->our_spi, IPSEC_DOI_SPI_SIZE
	    , &r_proposal_pbs, "SPI");
    }

    /* Transform */
    r_trans.isat_np = ISAKMP_NEXT_NONE;
    if (!out_struct(&r_trans, trans_desc, &r_proposal_pbs, &r_trans_pbs))
	impossible();

    /* Transform Attributes: pure echo */
    trans_pbs->cur = trans_pbs->start + sizeof(struct isakmp_transform);
    if (!out_raw(trans_pbs->cur, pbs_left(trans_pbs)
    , &r_trans_pbs, "attributes"))
	impossible();

    close_output_pbs(&r_trans_pbs);
    close_output_pbs(&r_proposal_pbs);
}

notification_t
parse_ipsec_sa_body(
    pb_stream *sa_pbs,		/* body of input SA Payload */
    const struct isakmp_sa *sa,	/* header of input SA Payload */
    pb_stream *r_sa_pbs,	/* if non-NULL, where to emit body of winning SA */
    bool selection,		/* if this SA is a selection, only one transform may appear */
    struct state *st)		/* current state object */
{
    const struct connection *c = st->st_connection;
    u_int32_t ipsecdoisit;
    pb_stream next_proposal_pbs;

    struct isakmp_proposal next_proposal;
    ipsec_spi_t next_spi;

    bool next_full = TRUE;

    /* DOI */
    if (sa->isasa_doi != ISAKMP_DOI_IPSEC)
    {
	loglog(RC_LOG_SERIOUS, "Unknown or unsupported DOI %s", enum_show(&doi_names, sa->isasa_doi));
	/* XXX Could send notification back */
	return DOI_NOT_SUPPORTED;
    }

    /* Situation */
    if (!in_struct(&ipsecdoisit, &ipsec_sit_desc, sa_pbs, NULL))
	return SITUATION_NOT_SUPPORTED;

    if (ipsecdoisit != SIT_IDENTITY_ONLY)
    {
	loglog(RC_LOG_SERIOUS, "unsupported IPsec DOI situation (%s)"
	    , bitnamesof(sit_bit_names, ipsecdoisit));
	/* XXX Could send notification back */
	return SITUATION_NOT_SUPPORTED;
    }

    /* The rules for IPsec SAs are scattered.
     * draft-ietf-ipsec-isakmp-09.txt section 4.2 gives some info.
     * There may be multiple proposals.  Those with identical proposal
     * numbers must be considered as conjuncts.  Those with different
     * numbers are disjuncts.
     * Each proposal may have several transforms, each considered
     * an alternative.
     * Each transform may have several attributes, all applying.
     *
     * To handle the way proposals are combined, we need to do a
     * look-ahead.
     */

    if (!in_struct(&next_proposal, &isakmp_proposal_desc, sa_pbs, &next_proposal_pbs))
	return BAD_PROPOSAL_SYNTAX;

    /* for each conjunction of proposals... */
    while (next_full)
    {
	int propno = next_proposal.isap_proposal;
	pb_stream ah_prop_pbs, esp_prop_pbs, ipcomp_prop_pbs;
	struct isakmp_proposal ah_proposal, esp_proposal, ipcomp_proposal;
	ipsec_spi_t ah_spi, esp_spi, ipcomp_cpi;
	bool ah_seen = FALSE, esp_seen = FALSE, ipcomp_seen = FALSE;
	u_int16_t well_known_cpi = 0;

	pb_stream ah_trans_pbs, esp_trans_pbs, ipcomp_trans_pbs;
	struct isakmp_transform ah_trans, esp_trans, ipcomp_trans;
	struct ipsec_trans_attrs ah_attrs, esp_attrs, ipcomp_attrs;

	/* for each proposal in the conjunction */
	do {

	    if (next_proposal.isap_protoid == PROTO_IPCOMP)
	    {
		/* IPCOMP CPI */
		if (next_proposal.isap_spisize == IPSEC_DOI_SPI_SIZE)
		{
		    /* This code is to accommodate those peculiar
		     * implementations that send a CPI in the bottom of an
		     * SPI-sized field.
		     * See draft-shacham-ippcp-rfc2393bis-05.txt 4.1
		     */
		    u_int8_t filler[IPSEC_DOI_SPI_SIZE - IPCOMP_CPI_SIZE];

		    if (!in_raw(filler, sizeof(filler)
		     , &next_proposal_pbs, "CPI filler")
		    || !all_zero(filler, sizeof(filler)))
			return INVALID_SPI;
		}
		else if (next_proposal.isap_spisize != IPCOMP_CPI_SIZE)
		{
		    loglog(RC_LOG_SERIOUS, "IPsec Proposal with improper CPI size (%u)"
			, next_proposal.isap_spisize);
		    return INVALID_SPI;
		}

		/* We store CPI in the low order of a network order
		 * ipsec_spi_t.  So we start a couple of bytes in.
		 */
		zero(&next_spi);
		if (!in_raw((u_char *)&next_spi
		  + IPSEC_DOI_SPI_SIZE - IPCOMP_CPI_SIZE
		, IPCOMP_CPI_SIZE, &next_proposal_pbs, "CPI"))
		    return INVALID_SPI;

		/* If sanity ruled, CPIs would have to be such that
		 * the SAID (the triple (CPI, IPCOM, destination IP))
		 * would be unique, just like for SPIs.  But there is a
		 * perversion where CPIs can be well-known and consequently
		 * the triple is not unique.  We hide this fact from
		 * ourselves by fudging the top 16 bits to make
		 * the property true internally!
		 */
		switch (ntohl(next_spi))
		{
		case IPCOMP_DEFLATE:
		    well_known_cpi = ntohl(next_spi);
		    next_spi = uniquify_his_cpi(next_spi, st);
		    if (next_spi == 0)
		    {
			loglog(RC_LOG_SERIOUS
			    , "IPsec Proposal contains well-known CPI that I cannot uniquify");
			return INVALID_SPI;
		    }
		    break;
		default:
		    if (ntohl(next_spi) < IPCOMP_FIRST_NEGOTIATED
		    || ntohl(next_spi) > IPCOMP_LAST_NEGOTIATED)
		    {
			loglog(RC_LOG_SERIOUS, "IPsec Proposal contains CPI from non-negotiated range (0x%lx)"
			    , (unsigned long) ntohl(next_spi));
			return INVALID_SPI;
		    }
		    break;
		}
	    }
	    else
	    {
		/* AH or ESP SPI */
		if (next_proposal.isap_spisize != IPSEC_DOI_SPI_SIZE)
		{
		    loglog(RC_LOG_SERIOUS, "IPsec Proposal with improper SPI size (%u)"
			, next_proposal.isap_spisize);
		    return INVALID_SPI;
		}

		if (!in_raw((u_char *)&next_spi, sizeof(next_spi), &next_proposal_pbs, "SPI"))
		    return INVALID_SPI;

		/* SPI value 0 is invalid and values 1-255 are reserved to IANA.
		 * RFC 2402 (ESP) 2.4, RFC 2406 (AH) 2.1
		 * IPCOMP???
		 */
		if (ntohl(next_spi) < IPSEC_DOI_SPI_MIN)
		{
		    loglog(RC_LOG_SERIOUS, "IPsec Proposal contains invalid SPI (0x%lx)"
			, (unsigned long) ntohl(next_spi));
		    return INVALID_SPI;
		}
	    }

	    if (next_proposal.isap_notrans == 0)
	    {
		loglog(RC_LOG_SERIOUS, "IPsec Proposal contains no Transforms");
		return BAD_PROPOSAL_SYNTAX;
	    }

	    switch (next_proposal.isap_protoid)
	    {
	    case PROTO_IPSEC_AH:
		if (ah_seen)
		{
		    loglog(RC_LOG_SERIOUS, "IPsec SA contains two simultaneous AH Proposals");
		    return BAD_PROPOSAL_SYNTAX;
		}
		ah_seen = TRUE;
		ah_prop_pbs = next_proposal_pbs;
		ah_proposal = next_proposal;
		ah_spi = next_spi;
		break;

	    case PROTO_IPSEC_ESP:
		if (esp_seen)
		{
		    loglog(RC_LOG_SERIOUS, "IPsec SA contains two simultaneous ESP Proposals");
		    return BAD_PROPOSAL_SYNTAX;
		}
		esp_seen = TRUE;
		esp_prop_pbs = next_proposal_pbs;
		esp_proposal = next_proposal;
		esp_spi = next_spi;
		break;

	    case PROTO_IPCOMP:
		if (ipcomp_seen)
		{
		    loglog(RC_LOG_SERIOUS, "IPsec SA contains two simultaneous IPCOMP Proposals");
		    return BAD_PROPOSAL_SYNTAX;
		}
		ipcomp_seen = TRUE;
		ipcomp_prop_pbs = next_proposal_pbs;
		ipcomp_proposal = next_proposal;
		ipcomp_cpi = next_spi;
		break;

	    default:
		loglog(RC_LOG_SERIOUS, "unexpected Protocol ID (%s) in IPsec Proposal"
		    , enum_show(&protocol_names, next_proposal.isap_protoid));
		return INVALID_PROTOCOL_ID;
	    }

	    /* refill next_proposal */
	    if (next_proposal.isap_np == ISAKMP_NEXT_NONE)
	    {
		next_full = FALSE;
		break;
	    }
	    else if (next_proposal.isap_np != ISAKMP_NEXT_P)
	    {
		loglog(RC_LOG_SERIOUS, "unexpected in Proposal: %s"
		    , enum_show(&payload_names, next_proposal.isap_np));
		return BAD_PROPOSAL_SYNTAX;
	    }

	    if (!in_struct(&next_proposal, &isakmp_proposal_desc, sa_pbs, &next_proposal_pbs))
		return BAD_PROPOSAL_SYNTAX;
	} while (next_proposal.isap_proposal == propno);

	/* Now that we have all conjuncts, we should try
	 * the Cartesian product of eachs tranforms!
	 * At the moment, we take short-cuts on account of
	 * our rudimentary hard-wired policy.
	 * For now, we find an acceptable AH (if any)
	 * and then an acceptable ESP.  The only interaction
	 * is that the ESP acceptance can know whether there
	 * was an acceptable AH and hence not require an AUTH.
	 */

	if (ah_seen)
	{
	    int previous_transnum = -1;
	    int tn;

	    for (tn = 0; tn != ah_proposal.isap_notrans; tn++)
	    {
		int ok_transid = 0;
		bool ok_auth = FALSE;

		if (!parse_ipsec_transform(&ah_trans
		, &ah_attrs
		, &ah_prop_pbs
		, &ah_trans_pbs
		, &isakmp_ah_transform_desc
		, previous_transnum
		, selection
		, tn == ah_proposal.isap_notrans - 1
		, FALSE
		, st))
		    return BAD_PROPOSAL_SYNTAX;

		previous_transnum = ah_trans.isat_transnum;

		/* we must understand ah_attrs.transid
		 * COMBINED with ah_attrs.auth.
		 * See draft-ietf-ipsec-ipsec-doi-08.txt 4.4.3
		 * The following combinations are legal,
		 * but we don't implement all of them:
		 * It seems as if each auth algorithm
		 * only applies to one ah transid.
		 * AH_MD5, AUTH_ALGORITHM_HMAC_MD5
		 * AH_MD5, AUTH_ALGORITHM_KPDK (unimplemented)
		 * AH_SHA, AUTH_ALGORITHM_HMAC_SHA1
		 * AH_DES, AUTH_ALGORITHM_DES_MAC (unimplemented)
		 */
		switch (ah_attrs.auth)
		{
		    case AUTH_ALGORITHM_NONE:
			loglog(RC_LOG_SERIOUS, "AUTH_ALGORITHM attribute missing in AH Transform");
			return BAD_PROPOSAL_SYNTAX;

		    case AUTH_ALGORITHM_HMAC_MD5:
			ok_auth = TRUE;
			/* fall through */
		    case AUTH_ALGORITHM_KPDK:
			ok_transid = AH_MD5;
			break;

		    case AUTH_ALGORITHM_HMAC_SHA1:
			ok_auth = TRUE;
			ok_transid = AH_SHA;
			break;

		    case AUTH_ALGORITHM_DES_MAC:
			ok_transid = AH_DES;
			break;
		}
		if (ah_attrs.transid != ok_transid)
		{
		    loglog(RC_LOG_SERIOUS, "%s attribute inappropriate in %s Transform"
			, enum_name(&auth_alg_names, ah_attrs.auth)
			, enum_show(&ah_transformid_names, ah_attrs.transid));
		    return BAD_PROPOSAL_SYNTAX;
		}
		if (!ok_auth)
		{
		    DBG(DBG_CONTROL | DBG_CRYPT
			, DBG_log("%s attribute unsupported"
			    " in %s Transform from %s"
			    , enum_name(&auth_alg_names, ah_attrs.auth)
			    , enum_show(&ah_transformid_names, ah_attrs.transid)
			    , ip_str(&c->that.host_addr)));
		    continue;   /* try another */
		}
		if (check_openrg_ah_policy(ah_attrs.auth,
		    st->st_connection->openrg_quick_policy))
		{
		    break;
		}
	    }
	    if (tn == ah_proposal.isap_notrans)
		continue;	/* we didn't find a nice one */
	    ah_attrs.spi = ah_spi;
	}

	if (esp_seen)
	{
	    int previous_transnum = -1;
	    int tn;

	    for (tn = 0; tn != esp_proposal.isap_notrans; tn++)
	    {
		if (!parse_ipsec_transform(&esp_trans
		, &esp_attrs
		, &esp_prop_pbs
		, &esp_trans_pbs
		, &isakmp_esp_transform_desc
		, previous_transnum
		, selection
		, tn == esp_proposal.isap_notrans - 1
		, FALSE
		, st))
		    return BAD_PROPOSAL_SYNTAX;

		previous_transnum = esp_trans.isat_transnum;

#ifndef NO_KERNEL_ALG
		if (!kernel_alg_esp_enc_ok(esp_attrs.transid, esp_attrs.key_len,
			c->alg_info_esp))
#endif
		switch (esp_attrs.transid)
		{
#ifdef NO_KERNEL_ALG	/* strictly use runtime information */
#ifdef USE_SINGLE_DES
		    case ESP_DES:
#endif
		    case ESP_3DES:
			break;
#endif
#ifdef CONFIG_IPSEC_ENC_AES
		    case ESP_AES:
			break;
#endif

#ifdef SUPPORT_ESP_NULL	/* should be about as secure as AH-only */
		    case ESP_NULL:
			if (esp_attrs.auth == AUTH_ALGORITHM_NONE)
			{
			    loglog(RC_LOG_SERIOUS, "ESP_NULL requires auth algorithm");
			    return BAD_PROPOSAL_SYNTAX;
			}
			break;
#endif

		    default:
			DBG(DBG_CONTROL | DBG_CRYPT
			    , DBG_log("unsupported ESP Transform %s from %s"
				, enum_show(&esp_transformid_names, esp_attrs.transid)
				, ip_str(&c->that.host_addr)));
			continue;   /* try another */
		}
#ifndef NO_KERNEL_ALG
		if (!kernel_alg_esp_auth_ok(esp_attrs.auth, c->alg_info_esp))
#endif
		switch (esp_attrs.auth)
		{
		    case AUTH_ALGORITHM_NONE:
			if (!ah_seen)
			{
			    DBG(DBG_CONTROL | DBG_CRYPT
				, DBG_log("ESP from %s must either have AUTH or be combined with AH"
				    , ip_str(&c->that.host_addr)));
			    continue;   /* try another */
			}
			break;
#ifdef NO_KERNEL_ALG	/* strictly use runtime information */
		    case AUTH_ALGORITHM_HMAC_MD5:
		    case AUTH_ALGORITHM_HMAC_SHA1:
			break;
#endif
		    default:
			DBG(DBG_CONTROL | DBG_CRYPT
			    , DBG_log("unsupported ESP auth alg %s from %s"
				, enum_show(&auth_alg_names, esp_attrs.auth)
				, ip_str(&c->that.host_addr)));
			continue;   /* try another */
		}

		if (ah_seen && ah_attrs.encapsulation != esp_attrs.encapsulation)
		{
		    /* ??? This should be an error, but is it? */
		    DBG(DBG_CONTROL | DBG_CRYPT
			, DBG_log("AH and ESP transforms disagree about encapsulation; TUNNEL presumed"));
		}

		if (check_openrg_esp_policy(esp_attrs.transid, esp_attrs.key_len,
		    esp_attrs.auth, st->st_connection->openrg_quick_policy))
		{
		    break;
		}
	    }
	    if (tn == esp_proposal.isap_notrans)
		continue;	/* we didn't find a nice one */
#ifndef NO_KERNEL_ALG
	    /* 
	     * ML: at last check for allowed transforms in alg_info_esp 
	     *     (ALG_INFO_F_STRICT flag)
	     *
	     */
	    if (!kernel_alg_esp_ok_final(esp_attrs.transid, esp_attrs.key_len,
				    esp_attrs.auth, c->alg_info_esp))
		    continue;
#endif
	    esp_attrs.spi = esp_spi;
	}
	else if ((st->st_policy & POLICY_AUTHENTICATE) && !ah_seen)
	{
	    DBG(DBG_CONTROL | DBG_CRYPT
		, DBG_log("policy for \"%s\" requires authentication"
		    " but none in Proposal from %s"
		    , c->name, ip_str(&c->that.host_addr)));
	    continue;	/* we need authentication, but we found neither ESP nor AH */
	}

	if (ipcomp_seen)
	{
	    int previous_transnum = -1;
	    int tn;

	    /**** FUDGE TO PREVENT UNREQUESTED IPCOMP:
	     **** NEEDED BECAUSE OUR IPCOMP IS EXPERIMENTAL (UNSTABLE).
	     ****/
	    if (!(st->st_policy & POLICY_COMPRESS))
	    {
		log("compression proposed by %s, but policy for \"%s\" forbids it"
		    , ip_str(&c->that.host_addr), c->name);
		continue;	/* unwanted compression proposal */
	    }

	    if (!can_do_IPcomp)
	    {
		log("compression proposed by %s, but KLIPS is not configured with IPCOMP"
		    , ip_str(&c->that.host_addr));
		continue;
	    }

	    if (well_known_cpi != 0 && !ah_seen && !esp_seen)
	    {
		log("illegal proposal: bare IPCOMP used with well-known CPI");
		return BAD_PROPOSAL_SYNTAX;
	    }

	    for (tn = 0; tn != ipcomp_proposal.isap_notrans; tn++)
	    {
		if (!parse_ipsec_transform(&ipcomp_trans
		, &ipcomp_attrs
		, &ipcomp_prop_pbs
		, &ipcomp_trans_pbs
		, &isakmp_ipcomp_transform_desc
		, previous_transnum
		, selection
		, tn == ipcomp_proposal.isap_notrans - 1
		, TRUE
		, st))
		    return BAD_PROPOSAL_SYNTAX;

		previous_transnum = ipcomp_trans.isat_transnum;

		if (well_known_cpi != 0 && ipcomp_attrs.transid != well_known_cpi)
		{
		    log("illegal proposal: IPCOMP well-known CPI disagrees with transform");
		    return BAD_PROPOSAL_SYNTAX;
		}

		switch (ipcomp_attrs.transid)
		{
		    case IPCOMP_DEFLATE:    /* all we can handle! */
			break;

		    default:
			DBG(DBG_CONTROL | DBG_CRYPT
			    , DBG_log("unsupported IPCOMP Transform %s from %s"
				, enum_show(&ipcomp_transformid_names, ipcomp_attrs.transid)
				, ip_str(&c->that.host_addr)));
			continue;   /* try another */
		}

		if (ah_seen && ah_attrs.encapsulation != ipcomp_attrs.encapsulation)
		{
		    /* ??? This should be an error, but is it? */
		    DBG(DBG_CONTROL | DBG_CRYPT
			, DBG_log("AH and IPCOMP transforms disagree about encapsulation; TUNNEL presumed"));
		} else if (esp_seen && esp_attrs.encapsulation != ipcomp_attrs.encapsulation)
		{
		    /* ??? This should be an error, but is it? */
		    DBG(DBG_CONTROL | DBG_CRYPT
			, DBG_log("ESP and IPCOMP transforms disagree about encapsulation; TUNNEL presumed"));
		}

		break;	/* we seem to be happy */
	    }
	    if (tn == ipcomp_proposal.isap_notrans)
		continue;	/* we didn't find a nice one */
	    ipcomp_attrs.spi = ipcomp_cpi;
	}

	/* Eureka: we liked what we saw -- accept it. */

	if (r_sa_pbs != NULL)
	{
	    /* emit what we've accepted */

	    /* Situation */
	    if (!out_struct(&ipsecdoisit, &ipsec_sit_desc, r_sa_pbs, NULL))
		impossible();

	    /* AH proposal */
	    if (ah_seen)
		echo_proposal(ah_proposal
		    , ah_trans
		    , esp_seen || ipcomp_seen? ISAKMP_NEXT_P : ISAKMP_NEXT_NONE
		    , r_sa_pbs
		    , &st->st_ah
		    , &isakmp_ah_transform_desc
		    , &ah_trans_pbs);

	    /* ESP proposal */
	    if (esp_seen)
		echo_proposal(esp_proposal
		    , esp_trans
		    , ipcomp_seen? ISAKMP_NEXT_P : ISAKMP_NEXT_NONE
		    , r_sa_pbs
		    , &st->st_esp
		    , &isakmp_esp_transform_desc
		    , &esp_trans_pbs);

	    /* IPCOMP proposal */
	    if (ipcomp_seen)
		echo_proposal(ipcomp_proposal
		    , ipcomp_trans
		    , ISAKMP_NEXT_NONE
		    , r_sa_pbs
		    , &st->st_ipcomp
		    , &isakmp_ipcomp_transform_desc
		    , &ipcomp_trans_pbs);

	    close_output_pbs(r_sa_pbs);
	}

	/* save decoded version of winning SA in state */

	st->st_ah.present = ah_seen;
	if (ah_seen)
	    st->st_ah.attrs = ah_attrs;

	st->st_esp.present = esp_seen;
	if (esp_seen)
	    st->st_esp.attrs = esp_attrs;

	st->st_ipcomp.present = ipcomp_seen;
	if (ipcomp_seen)
	    st->st_ipcomp.attrs = ipcomp_attrs;

	return NOTHING_WRONG;
    }

    loglog(RC_LOG_SERIOUS, "no acceptable Proposal in IPsec SA");
    return NO_PROPOSAL_CHOSEN;
}
