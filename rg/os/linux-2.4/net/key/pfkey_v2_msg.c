/* $USAGI: pfkey_v2_msg.c,v 1.18.2.3 2002/10/05 20:15:33 sekiya Exp $ */
/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * pfkey_v2_msg.c is a program for PF_KEY version2 parsing routine, and utilities.
 */
#ifdef MODULE
#include <linux/module.h>
#  ifdef MODVERSIONS
#  include <linux/modversions.h>
#  endif
#endif

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/random.h>
#include <linux/net.h>
#include <linux/crypto.h>
#include <linux/ipsec.h>

#include <net/sock.h>
#include <net/pfkeyv2.h>
#include <net/pfkey.h>
#include <net/sadb.h>
#include <net/spd.h>

#include "pfkey_v2_msg.h"
#include "sadb_utils.h"
#include "spd_utils.h"
#define BUF_SIZE 64

int sadb_msg_sanity_check(struct sadb_msg* msg)
{
	int error = 0;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	if (!(msg->sadb_msg_version == PF_KEY_V2 &&
	      msg->sadb_msg_type > 0 && msg->sadb_msg_type <= SADB_MAX &&
	      /* msg->sadb_msg_satype >= 0 && */ msg->sadb_msg_satype <= SADB_SATYPE_MAX &&
	      msg->sadb_msg_reserved == 0))
		error = -EINVAL;

	PFKEY_DEBUG("sadb_msg_version=%d\n", msg->sadb_msg_version);
	PFKEY_DEBUG("sadb_msg_type=%d\n", msg->sadb_msg_type);
	PFKEY_DEBUG("sadb_msg_satype=%d\n", msg->sadb_msg_satype);
	PFKEY_DEBUG("sadb_msg_len=%d\n", msg->sadb_msg_len);
	PFKEY_DEBUG("sadb_msg_reserved=%d\n", msg->sadb_msg_reserved);

err:
	PFKEY_DEBUG("error=%d\n", error);
	return error;
}

int sadb_address_to_sockaddr(const struct sadb_address *ext_msg, struct sockaddr* addr)
{
	int error = 0, len = 0;
	struct sockaddr* tmp_addr = NULL;
#ifdef CONFIG_IPSEC_DEBUG
	char buf[BUF_SIZE];
#endif

	if (!ext_msg || !addr) {
		PFKEY_DEBUG("msg or addr is null\n");
		error = -EINVAL;
		goto err;
	}

	len = ext_msg->sadb_address_len - sizeof(struct sadb_address);
	if (len < sizeof(struct sockaddr)) {
		PFKEY_DEBUG("sadb_address_len is small len=%d\n", len);
		error = -EINVAL;
		goto err;
	}

	tmp_addr = (struct sockaddr*)((char*)ext_msg + sizeof(struct sadb_address));
	if (!tmp_addr) {
		PFKEY_DEBUG("address==null\n");
		error = -EINVAL;
		goto err;
	}

	switch (tmp_addr->sa_family) {

	case AF_INET:
#ifdef CONFIG_IPSEC_DEBUG
		PFKEY_DEBUG("address family is AF_INET\n");
		sockaddrtoa((struct sockaddr*)tmp_addr, buf, BUF_SIZE);
		PFKEY_DEBUG("address=%s\n", buf);
#endif
		memcpy(addr, tmp_addr, sizeof(struct sockaddr_in)); 
		break;

	case AF_INET6:
#ifdef CONFIG_IPSEC_DEBUG
		PFKEY_DEBUG("address family is AF_INET6\n");
		sockaddrtoa((struct sockaddr*)tmp_addr, buf, BUF_SIZE);
		PFKEY_DEBUG("address=%s\n", buf);
#endif
		memcpy(addr, tmp_addr, sizeof(struct sockaddr_in6)); 
		break;

	default:
		PFKEY_DEBUG("address family is unknown\n");
		error = -EINVAL;
		goto err;
	}

err:
#ifdef CONFIG_IPSEC_DEBUG
	if (!error)
		PFKEY_DEBUG("error=%d\n", error);
#endif
	return error;
}

int sadb_key_to_esp(const __u8 esp_algo, const struct sadb_key* ext_msg, struct ipsec_sa* sa_entry)
{
	int error = 0;
	char *algoname = NULL;
	struct cipher_implementation *ci=NULL;

	if (!sa_entry) {
		PFKEY_DEBUG("sa_entry is null\n");
		error = -EINVAL;
		goto err;
	}

	if (esp_algo != SADB_EALG_NULL && !ext_msg) {
		PFKEY_DEBUG("ext_msg is null\n");
		error = -EINVAL;
		goto err;
	}

	switch (esp_algo) {
	case SADB_EALG_DESCBC:
		algoname = "des-cbc";
		PFKEY_DEBUG("esp algorithm is DES-CBC\n");
		if(ext_msg->sadb_key_bits != ESP_DES_KEY_BITS){
			PFKEY_DEBUG("the key length is not match\n");
			error = -EINVAL;
			goto err;
		} 
		break;
	case SADB_EALG_3DESCBC:
		algoname = "des_ede3-cbc";
		PFKEY_DEBUG("esp algorithm is 3DES-CBC\n");
		if(ext_msg->sadb_key_bits != ESP_3DES_KEY_BITS){
			PFKEY_DEBUG("the key length is not match\n");
			error = -EINVAL;
			goto err;
		}
		break;
	case SADB_EALG_NULL:
		algoname = "null-algo";
		PFKEY_DEBUG("esp algorithm is NULL\n");
		break; 
	case SADB_EALG_AES:
		algoname = "aes-cbc";
		PFKEY_DEBUG("esp algorithm is AES(128-bit)\n");
		if (ext_msg->sadb_key_bits != ESP_AES_KEY_BITS) {
			PFKEY_DEBUG("the key length is no match\n");
			error = -EINVAL;
			goto err;
		}
		break;
	case SADB_EALG_NONE: /* currently not enter */
	default:
		PFKEY_DEBUG("esp_algo is NONE or not supported one\n");
		error = -EINVAL;
		goto err;
	}	

	ci = find_cipher_by_name(algoname, 1);
	if(!ci){
		PFKEY_DEBUG("algorithm %u:%s is not supported\n", esp_algo, algoname);
		error = -EINVAL;
		goto err;
	}
	ci->lock();

	sa_entry->esp_algo.algo = esp_algo;

	if (esp_algo != SADB_EALG_NULL) {
		sa_entry->esp_algo.key_len = (ext_msg->sadb_key_bits)/OCTETBITS;
	} else {
		sa_entry->esp_algo.key_len = 0;
	}

	sa_entry->esp_algo.cx = ci->realloc_context (NULL, ci, sa_entry->esp_algo.key_len);
	if (!sa_entry->esp_algo.cx) {
		ci->unlock();
		error = -EINVAL;
		goto err;
	}
	sa_entry->esp_algo.cx->ci = ci;

	if (esp_algo != SADB_EALG_NULL) {
		sa_entry->esp_algo.key = kmalloc(sa_entry->esp_algo.key_len, GFP_KERNEL);
		if (!sa_entry->esp_algo.key) {
			PFKEY_DEBUG("could not allocate memory for key\n");
			ci->wipe_context(sa_entry->esp_algo.cx);
			ci->free_context(sa_entry->esp_algo.cx);
			ci->unlock();
			error = -ENOMEM;
			goto err;
		}
		memset(sa_entry->esp_algo.key, 0, sa_entry->esp_algo.key_len);
		memcpy(sa_entry->esp_algo.key, ((char*)ext_msg)+sizeof(struct sadb_key), sa_entry->esp_algo.key_len);

		memset(sa_entry->esp_algo.cx->keyinfo, 0, ci->key_schedule_size);

		error = ci->set_key(sa_entry->esp_algo.cx, ((char*)ext_msg)+sizeof(struct sadb_key), sa_entry->esp_algo.key_len);
		if (error < 0) {
			PFKEY_DEBUG("set_key failed, key is wondered weak \n");
			ci->wipe_context(sa_entry->esp_algo.cx);
			ci->free_context(sa_entry->esp_algo.cx);
			ci->unlock();
			goto err;
		}
	}
err:
	return error;

}

int sadb_key_to_auth(const __u8 auth_algo, const struct sadb_key* ext_msg, struct ipsec_sa* sa_entry)
{
	int error = 0;
	char *algoname = NULL;
	__u16 digest_len = 0;
	struct digest_implementation *di = NULL;

	if(!(ext_msg&&sa_entry)){
		PFKEY_DEBUG("msg or sa_entry is null\n");
		error = -EINVAL;
		goto err;
	}

	switch(auth_algo){

	case SADB_AALG_MD5HMAC:
		algoname = "md5";
		digest_len = 12; /* 96 bit length */
		PFKEY_DEBUG("auth algorithm is HMAC-MD5\n");

		if(ext_msg->sadb_key_bits != AUTH_MD5HMAC_KEY_BITS){
			PFKEY_DEBUG("the key length is %d\n", ext_msg->sadb_key_bits);
			PFKEY_DEBUG("the key length is not match\n");
			error = -EINVAL;
			goto err;
		} 
		break;

	case SADB_AALG_SHA1HMAC:
		algoname = "sha1";
		digest_len = 12; /* 96 bit length */
		PFKEY_DEBUG("auth algorithm is HMAC-SHA1\n");

		if(ext_msg->sadb_key_bits != AUTH_SHA1HMAC_KEY_BITS){
			PFKEY_DEBUG("the key length is %d\n", ext_msg->sadb_key_bits);
			PFKEY_DEBUG("the key length is not match\n");
			error = -EINVAL;
			goto err;
		} 
		break;

	case SADB_AALG_NONE:
	default:
		PFKEY_DEBUG("auth_algo is NONE or not supported one\n");
		error = -EINVAL;
		goto err;
	}

	di = find_digest_by_name(algoname, 0);
	if (!di) {
		PFKEY_DEBUG("algorithm %u:%s is not supported\n", auth_algo, algoname);
		error = -EINVAL;
		goto err;
	}
	di->lock();

	sa_entry->auth_algo.algo = auth_algo;
	sa_entry->auth_algo.key_len = (ext_msg->sadb_key_bits)/OCTETBITS;
	sa_entry->auth_algo.digest_len = digest_len;
	sa_entry->auth_algo.dx = di->realloc_context(NULL, di);
	if (!sa_entry->auth_algo.dx) {
		di->unlock();
		error = -EINVAL;
		goto err;
	}
	sa_entry->auth_algo.dx->di = di;

	sa_entry->auth_algo.key = kmalloc(sa_entry->auth_algo.key_len, GFP_KERNEL);
	if (!sa_entry->auth_algo.key) {
		PFKEY_DEBUG("cannot allocate key\n");
		di->free_context(sa_entry->auth_algo.dx);
		di->unlock();
		error = -ENOMEM;
		goto err;
	}

	memcpy(sa_entry->auth_algo.key, (char *)ext_msg+sizeof(struct sadb_key),
			sa_entry->auth_algo.key_len);
err:
	return error;
}


int sadb_lifetime_to_lifetime(const struct sadb_lifetime* ext_msg, struct sa_lifetime* lifetime)
{
	int error = 0;

	if (!ext_msg || !lifetime) {
		PFKEY_DEBUG("param is null\n");
		error = -EINVAL;
		goto err;
	}

	lifetime->allocations 	= ext_msg->sadb_lifetime_allocations;
	lifetime->bytes 	= ext_msg->sadb_lifetime_bytes;
	lifetime->addtime	= ext_msg->sadb_lifetime_addtime;
	lifetime->usetime	= ext_msg->sadb_lifetime_usetime;

err:
	return error;
}

int lifetime_to_sadb_lifetime(struct sa_lifetime *lifetime, struct sadb_lifetime *ext_msg, int type)
{
	int error = 0;
	if (!lifetime || !ext_msg) {
		PFKEY_DEBUG("param is null\n");
		error = -EINVAL;
		goto err;
	}

	error = pfkey_lifetime_build((struct sadb_ext**)&ext_msg, type,
					lifetime->allocations,
					lifetime->bytes,
					lifetime->addtime,
					lifetime->usetime);
err:
	return error;
}

int sadb_msg_detect_ext(struct sadb_msg* msg, struct sadb_ext **ext_msgs)
{
	int error = 0, len = 0, msg_size = 0;
	struct sadb_ext *ext_ptr = NULL;

	if (!msg || !ext_msgs) {
		PFKEY_DEBUG("msg or ext_msgs is null.\n");
		error = -EINVAL;
		goto err;
	}

	error = sadb_msg_sanity_check(msg);
	if (error) {
		error = -EINVAL;
		goto err;
	}

	ext_ptr = (struct sadb_ext*)((u8*)msg + sizeof(struct sadb_msg));
	len = (msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) - sizeof(struct sadb_msg);

	while (len >= sizeof(*ext_ptr)) { 
		PFKEY_DEBUG("ext type %d\n", ext_ptr->sadb_ext_type);
		PFKEY_DEBUG("ext length %d\n", ext_ptr->sadb_ext_len);
		msg_size = (ext_ptr->sadb_ext_len) * IPSEC_PFKEYv2_ALIGN;
		if (len < msg_size)
			break;	/* error will be set later */
		ext_msgs[ext_ptr->sadb_ext_type] = ext_ptr;
		ext_ptr = (struct sadb_ext*)((u8*)ext_ptr + msg_size); 
		len -= msg_size;
	}

	if (len)
		error = -EINVAL;

err:
	PFKEY_DEBUG("error=%d\n", error);
	return error;
}		

int sadb_msg_acquire_parse(struct sock *sk, struct sadb_msg *msg, struct sadb_msg **reply)
{
	int error = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1];

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

err:
	return error;
}

int sadb_msg_register_parse(struct sock *sk, struct sadb_msg *msg, struct sadb_msg **reply)
{
	int error = 0;
	uint  alg_num = 0;
	struct sadb_alg algs[5];
	struct digest_implementation *di = NULL;
	struct cipher_implementation *ci = NULL;
	struct sadb_ext *reply_ext_msgs[SADB_EXT_MAX+1];

	if (!sk || !msg) {
		PFKEY_DEBUG("msg or sk is null\n");
		error = -EINVAL;
		PFKEY_DEBUG("msg or *reply is null\n");
		goto err; 	
	}

	error = sadb_msg_sanity_check(msg);
	if (error)
		goto err;

	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));
	memset(algs, 0, sizeof(algs));

	switch (msg->sadb_msg_satype) {

	case SADB_SATYPE_AH:
	case SADB_SATYPE_ESP:
	case SADB_X_SATYPE_COMP:
		break;
	case SADB_SATYPE_RSVP:
	case SADB_SATYPE_OSPFV2:
	case SADB_SATYPE_RIPV2:
	case SADB_SATYPE_MIP:
	case SADB_X_SATYPE_IPIP:
	case SADB_X_SATYPE_INT:

	default:
		error = -EINVAL;
		goto err;
	}

	write_lock_bh(&pfkey_sk_lock);
	/*
	 register a socket with the proper SA type's socket list
	 it will be release in pfkey_release process (file:pfkey_v2.c).
	*/
	error = pfkey_list_insert_socket(sk->socket, &pfkey_registered_sockets[msg->sadb_msg_satype]);
	if (error) {
		goto err;
	}
	write_unlock_bh(&pfkey_sk_lock);

	PFKEY_DEBUG("socket=%p registered, SA type is %d\n", sk->socket, msg->sadb_msg_satype);

	reply_ext_msgs[0] = (struct sadb_ext*)msg;

	if (msg->sadb_msg_satype == SADB_SATYPE_AH || msg->sadb_msg_satype == SADB_SATYPE_ESP) {
		di = find_digest_by_name("md5", 0 /* atomic */);
		if (di) {
			algs[alg_num].sadb_alg_id = SADB_AALG_MD5HMAC;
			algs[alg_num].sadb_alg_ivlen = 0;
			algs[alg_num].sadb_alg_minbits = AUTH_MD5HMAC_KEY_BITS;
			algs[alg_num].sadb_alg_maxbits = AUTH_MD5HMAC_KEY_BITS;
			algs[alg_num].sadb_alg_reserved = 0;
			alg_num++;
			di = NULL;	
		}
	
		di = find_digest_by_name("sha1", 0 /* atomic */);
		if (di) {
			algs[alg_num].sadb_alg_id = SADB_AALG_SHA1HMAC;
			algs[alg_num].sadb_alg_ivlen = 0;
			algs[alg_num].sadb_alg_minbits = AUTH_SHA1HMAC_KEY_BITS;
			algs[alg_num].sadb_alg_maxbits = AUTH_SHA1HMAC_KEY_BITS;
			algs[alg_num].sadb_alg_reserved = 0;
			alg_num++;
			di = NULL;
		}

		if (msg->sadb_msg_satype == SADB_SATYPE_AH) {
			error = pfkey_supported_build(&reply_ext_msgs[SADB_EXT_SUPPORTED_AUTH],
						SADB_EXT_SUPPORTED_AUTH,
						alg_num,
						algs);
			if (error) goto free_ext_finish;
		}

	}

	if (msg->sadb_msg_satype == SADB_SATYPE_ESP) {
		ci = find_cipher_by_name("des-cbc", 1 /* atomic */);
		if (ci) {
			ci->lock();
			algs[alg_num].sadb_alg_id = SADB_EALG_DESCBC;
			algs[alg_num].sadb_alg_ivlen = ci->ivsize;
			algs[alg_num].sadb_alg_minbits = ESP_DES_KEY_BITS;
			algs[alg_num].sadb_alg_maxbits = ESP_DES_KEY_BITS;
			algs[alg_num].sadb_alg_reserved = 0;
			alg_num++;
			ci->unlock();
			ci = NULL;
		}

		ci = find_cipher_by_name("des_ede3-cbc", 1 /* atomic */);
		if (ci) {
			ci->lock();
			algs[alg_num].sadb_alg_id = SADB_EALG_3DESCBC;
			algs[alg_num].sadb_alg_ivlen = ci->ivsize;
			algs[alg_num].sadb_alg_minbits = ESP_3DES_KEY_BITS;
			algs[alg_num].sadb_alg_maxbits = ESP_3DES_KEY_BITS;
			algs[alg_num].sadb_alg_reserved = 0;
			alg_num++;
			ci->unlock();
			ci = NULL;
		}
		ci = find_cipher_by_name("aes-cbc", 1 /* atomic */);
		if (ci) {
			ci->lock();
			algs[alg_num].sadb_alg_id = SADB_EALG_AES;
			algs[alg_num].sadb_alg_ivlen = ci->ivsize;
			algs[alg_num].sadb_alg_minbits = ESP_AES_KEY_BITS;
			algs[alg_num].sadb_alg_maxbits = ESP_AES_KEY_BITS;
			algs[alg_num].sadb_alg_reserved = 0;
			alg_num++;
			ci->unlock();
			ci = NULL;
		}

		error = pfkey_supported_build(&reply_ext_msgs[SADB_EXT_SUPPORTED_ENCRYPT],
						SADB_EXT_SUPPORTED_ENCRYPT,
						alg_num,
						algs);
		if (error) goto free_ext_finish;

	}

	pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);

free_ext_finish:

	if (reply_ext_msgs[SADB_EXT_SUPPORTED_AUTH])
		kfree(reply_ext_msgs[SADB_EXT_SUPPORTED_AUTH]);

	if (reply_ext_msgs[SADB_EXT_SUPPORTED_ENCRYPT])
		kfree(reply_ext_msgs[SADB_EXT_SUPPORTED_ENCRYPT]);


err:

	return error;
}

int sadb_msg_expire_parse(struct sock *sk, struct sadb_msg *msg, struct sadb_msg **reply)
{
	int error = -EINVAL;

#if 0 /* kernel never receive SADB_EXPIRE message */

	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1];

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

err:
#endif /* kernel never receive SADB_EXPIRE message */

	return error;
}

int sadb_msg_flush_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply)
{
	int error = 0;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	error = sadb_msg_sanity_check(msg);

	if (error) {
		PFKEY_DEBUG("message is invalid\n");
		goto err;
	}

	switch(msg->sadb_msg_satype) {
	case SADB_SATYPE_AH:
	case SADB_SATYPE_ESP:
		sadb_flush_sa(msg->sadb_msg_satype);
		break;
	default:
		sadb_clear_db(); 
	}

	*reply = kmalloc(sizeof(struct sadb_msg), GFP_KERNEL);
	memcpy(*reply, msg, sizeof(struct sadb_msg));
err:
	return error;
}

int sadb_msg_flush_sp_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply)
{
	int error = 0;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	error = sadb_msg_sanity_check(msg);

	if (error) {
		PFKEY_DEBUG("message is invalid\n");
		goto err;
	}

	spd_clear_db();

	*reply = kmalloc(sizeof(struct sadb_msg), GFP_KERNEL);
	memcpy(*reply, msg, sizeof(struct sadb_msg));
err:
	return error;
}

int sadb_msg_dump_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply)
{
	int error = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1];

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

err:
	return error;
}

int sadb_msg_send_expire(struct ipsec_sa *sa)
{
	int i=0, error = 0;
	struct sadb_msg *msg = NULL;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1];
	struct socket_list *pfkey_socketsp = NULL;

	if (!sa) {
		PFKEY_DEBUG("sa is null\n");
		return -EINVAL;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));

	error = pfkey_msg_hdr_build(&ext_msgs[0],
				    SADB_EXPIRE,
				    sa->ipsec_proto,
				    0,
				    0,
				    0);
	if (error) {
		PFKEY_DEBUG("pfkey_msg_hdr_build is failed\n");
		goto free_ext_finish;
	}

	error = pfkey_sa_build(&ext_msgs[SADB_EXT_SA],
				   SADB_EXT_SA,
				   sa->spi,
				   64,
				   sa->state,
				   sa->auth_algo.algo,
				   sa->esp_algo.algo,
				   0); /* TODO: add pfs flag to struct ipsec_sa */
	if (error) {
		PFKEY_DEBUG("pfkey_sa_build is failed\n");
		goto free_ext_finish;
	}

	error = pfkey_lifetime_build(&ext_msgs[SADB_EXT_LIFETIME_CURRENT],
					 SADB_EXT_LIFETIME_CURRENT,
					 sa->lifetime_c.allocations,
					 sa->lifetime_c.bytes,
					 sa->lifetime_c.addtime,
					 sa->lifetime_c.usetime);
	if (error) {
		PFKEY_DEBUG("pfkey_lifetime_build is failed\n");
		goto free_ext_finish;
	}

	switch(sa->state) {
	case SADB_SASTATE_DEAD:
		error = pfkey_lifetime_build(&ext_msgs[SADB_EXT_LIFETIME_HARD],
						 SADB_EXT_LIFETIME_HARD,
						 sa->lifetime_h.allocations,
						 sa->lifetime_h.bytes,
						 sa->lifetime_h.addtime,
						 sa->lifetime_h.usetime);
		if (error) {
			PFKEY_DEBUG("pfkey_liftime_build(hard) is failed\n");
			goto free_ext_finish;
		}
		break;

	case SADB_SASTATE_DYING:
		error = pfkey_lifetime_build(&ext_msgs[SADB_EXT_LIFETIME_SOFT],
						 SADB_EXT_LIFETIME_SOFT,
						 sa->lifetime_s.allocations,
						 sa->lifetime_s.bytes,
						 sa->lifetime_s.addtime,
						 sa->lifetime_s.usetime);
		if (error) {
			PFKEY_DEBUG("pfkey_lifetime_build(soft) is failed\n");
			goto free_ext_finish;
		}
		break;

	case SADB_SASTATE_LARVAL:
	case SADB_SASTATE_MATURE:
	default:
		error = -EINVAL;
		goto free_ext_finish;

	}
	
	error = pfkey_address_build(&ext_msgs[SADB_EXT_ADDRESS_SRC],
					SADB_EXT_ADDRESS_SRC,
					sa->proto,
					sa->prefixlen_s,
					(struct sockaddr*)&sa->src);
	if (error) goto free_ext_finish;

	error = pfkey_address_build(&ext_msgs[SADB_EXT_ADDRESS_DST],
					SADB_EXT_ADDRESS_DST,
					sa->proto,
					sa->prefixlen_d,
					(struct sockaddr*)&sa->dst);
	if (error) goto free_ext_finish;

	error = pfkey_msg_build(&msg, ext_msgs, EXT_BITS_OUT);

	write_lock_bh(&pfkey_sk_lock);
	for (pfkey_socketsp = pfkey_open_sockets;
		pfkey_socketsp;
			pfkey_socketsp = pfkey_socketsp->next)
	{
		pfkey_upmsg(pfkey_socketsp->socketp, msg);
	}
	write_unlock_bh(&pfkey_sk_lock);

	kfree(msg);

free_ext_finish:
	for(i=0; i<SADB_MAX+1; i++) {
		if (ext_msgs[i]) {
			kfree(ext_msgs[i]);
		}
	}

	return 0;

}

int sadb_msg_send_acquire(struct ipsec_sa *sa)
{
	int i=0, error = 0;
	uint  comb_num = 0;
	struct sadb_comb combs[5];
	struct digest_implementation *di = NULL;
	struct cipher_implementation *ci = NULL;
	struct sadb_msg *msg = NULL;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1];
	struct socket_list *pfkey_socketsp = NULL;

	memset(ext_msgs, 0, sizeof(ext_msgs));
	memset(combs, 0, sizeof(combs));

	if (sa->ipsec_proto == SADB_SATYPE_AH || sa->ipsec_proto == SADB_SATYPE_ESP) {
		di = find_digest_by_name("md5", 0 /* atomic */);
		if (di) {
			combs[comb_num].sadb_comb_auth = SADB_AALG_MD5HMAC;
			combs[comb_num].sadb_comb_auth_minbits = AUTH_MD5HMAC_KEY_BITS;
			combs[comb_num].sadb_comb_auth_maxbits = AUTH_MD5HMAC_KEY_BITS;
			combs[comb_num].sadb_comb_reserved = 0;
			combs[comb_num].sadb_comb_soft_allocations = 0;
			combs[comb_num].sadb_comb_hard_allocations = 0;
			combs[comb_num].sadb_comb_soft_bytes = 0;
			combs[comb_num].sadb_comb_hard_bytes = 0;
			combs[comb_num].sadb_comb_soft_addtime = 57600;
			combs[comb_num].sadb_comb_hard_addtime = 86400;
			combs[comb_num].sadb_comb_soft_usetime = 57600;
			combs[comb_num].sadb_comb_hard_usetime = 86400;
			comb_num++;
			di = NULL;	
		}
	
		di = find_digest_by_name("sha1", 0 /* atomic */);
		if (di) {
			combs[comb_num].sadb_comb_auth = SADB_AALG_SHA1HMAC;
			combs[comb_num].sadb_comb_auth_minbits = AUTH_SHA1HMAC_KEY_BITS;
			combs[comb_num].sadb_comb_auth_maxbits = AUTH_SHA1HMAC_KEY_BITS;
			combs[comb_num].sadb_comb_reserved = 0;
			combs[comb_num].sadb_comb_soft_allocations = 0;
			combs[comb_num].sadb_comb_hard_allocations = 0;
			combs[comb_num].sadb_comb_soft_bytes = 0;
			combs[comb_num].sadb_comb_hard_bytes = 0;
			combs[comb_num].sadb_comb_soft_addtime = 57600;
			combs[comb_num].sadb_comb_hard_addtime = 86400;
			combs[comb_num].sadb_comb_soft_usetime = 57600;
			combs[comb_num].sadb_comb_hard_usetime = 86400;
			comb_num++;
			di = NULL;
		}
	}

	if (sa->ipsec_proto == SADB_SATYPE_ESP) {
		ci = find_cipher_by_name("des-cbc", 1 /* atomic */);
		if (ci) {
			combs[comb_num].sadb_comb_encrypt = SADB_EALG_DESCBC;
			combs[comb_num].sadb_comb_encrypt_minbits = ESP_DES_KEY_BITS;
			combs[comb_num].sadb_comb_encrypt_maxbits = ESP_DES_KEY_BITS;
			combs[comb_num].sadb_comb_reserved = 0;
			combs[comb_num].sadb_comb_soft_allocations = 0;
			combs[comb_num].sadb_comb_hard_allocations = 0;
			combs[comb_num].sadb_comb_soft_bytes = 0;
			combs[comb_num].sadb_comb_hard_bytes = 0;
			combs[comb_num].sadb_comb_soft_addtime = 57600;
			combs[comb_num].sadb_comb_hard_addtime = 86400;
			combs[comb_num].sadb_comb_soft_usetime = 57600;
			combs[comb_num].sadb_comb_hard_usetime = 86400;
			comb_num++;
			ci = NULL;
		}

		ci = find_cipher_by_name("des_ede3-cbc", 1 /* atomic */);
		if (ci) {
			combs[comb_num].sadb_comb_encrypt = SADB_EALG_3DESCBC;
			combs[comb_num].sadb_comb_encrypt_minbits = ESP_3DES_KEY_BITS;
			combs[comb_num].sadb_comb_encrypt_maxbits = ESP_3DES_KEY_BITS;
			combs[comb_num].sadb_comb_reserved = 0;
			combs[comb_num].sadb_comb_soft_allocations = 0;
			combs[comb_num].sadb_comb_hard_allocations = 0;
			combs[comb_num].sadb_comb_soft_bytes = 0;
			combs[comb_num].sadb_comb_hard_bytes = 0;
			combs[comb_num].sadb_comb_soft_addtime = 57600;
			combs[comb_num].sadb_comb_hard_addtime = 86400;
			combs[comb_num].sadb_comb_soft_usetime = 57600;
			combs[comb_num].sadb_comb_hard_usetime = 86400;
			comb_num++;
			ci = NULL;
		}

		ci = find_cipher_by_name("aes-cbc", 1 /* atomic */);
		if (ci) {
			combs[comb_num].sadb_comb_encrypt = SADB_EALG_AES;
			combs[comb_num].sadb_comb_encrypt_minbits = ESP_AES_KEY_BITS;
			combs[comb_num].sadb_comb_encrypt_maxbits = ESP_AES_KEY_BITS;
			combs[comb_num].sadb_comb_reserved = 0;
			combs[comb_num].sadb_comb_soft_allocations = 0;
			combs[comb_num].sadb_comb_hard_allocations = 0;
			combs[comb_num].sadb_comb_soft_bytes = 0;
			combs[comb_num].sadb_comb_hard_bytes = 0;
			combs[comb_num].sadb_comb_soft_addtime = 57600;
			combs[comb_num].sadb_comb_hard_addtime = 86400;
			combs[comb_num].sadb_comb_soft_usetime = 57600;
			combs[comb_num].sadb_comb_hard_usetime = 86400;
			comb_num++;
			ci = NULL;
		}
	}

	error = pfkey_msg_hdr_build(&ext_msgs[0],
				    SADB_ACQUIRE,
				    sa->ipsec_proto,
				    0,
				    0,
				    0);
	if (error) {
		PFKEY_DEBUG("pfkey_msg_hdr_build is failed\n");
		goto free_ext_finish;
	}

	error = pfkey_address_build(&ext_msgs[SADB_EXT_ADDRESS_SRC],
					SADB_EXT_ADDRESS_SRC,
					sa->proto,
					sa->prefixlen_s,
					(struct sockaddr*)&sa->src);
	if (error) goto free_ext_finish;

	error = pfkey_address_build(&ext_msgs[SADB_EXT_ADDRESS_DST],
					SADB_EXT_ADDRESS_DST,
					sa->proto,
					sa->prefixlen_d,
					(struct sockaddr*)&sa->dst);
	if (error) goto free_ext_finish;

	error = pfkey_prop_build(&ext_msgs[SADB_EXT_PROPOSAL],
					64,
					comb_num,
					combs);
	if (error) goto free_ext_finish;


	error = pfkey_msg_build(&msg, ext_msgs, EXT_BITS_OUT);

	write_lock_bh(&pfkey_sk_lock);
	for (pfkey_socketsp = pfkey_registered_sockets[sa->ipsec_proto];
		pfkey_socketsp;
			pfkey_socketsp = pfkey_socketsp->next)
	{
		pfkey_upmsg(pfkey_socketsp->socketp, msg);
	}
	write_unlock_bh(&pfkey_sk_lock);

	kfree(msg);
free_ext_finish:

	for(i=0; i<SADB_MAX+1; i++) {
		if (ext_msgs[i]) {
			kfree(ext_msgs[i]);
		}
	}

	return 0;
}


