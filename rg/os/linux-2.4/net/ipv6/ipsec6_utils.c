/* derived from iabg ipv6_ipsec-main.c -mk */
/*
 * IPSECv6 code
 * Copyright (C) 2000 Stefan Schlott
 * Copyright (C) 2001 Florian Heissenhuber
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
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>

#include <linux/ipsec6.h>

#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/snmp.h>  
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/ipsec.h>
#include <net/ipsec6_utils.h>
#include <net/pfkeyv2.h>

/* Set all mutable/unpredictable fields to zero. */
static int zero_out_mutable_opts(struct ipv6_opt_hdr *opthdr)
{
	u8 *opt = (u8*)opthdr;
	int len = ipv6_optlen(opthdr);
	int off = 0;
	int optlen;

	off += 2;
	len -= 2;

	while(len > 0) {
		switch(opt[off]) {
		case IPV6_TLV_PAD0:
			optlen = 1;
			break;
		default:
			if (len < 2)
				goto bad;
			optlen = opt[off+1]+2;
			if (len < optlen)
				goto bad;
			if (opt[off] & 0x20)	/* mutable check */
				memset(&opt[off+2], 0, opt[off+1]);
			break;
		}
		off += optlen;
		len -= optlen;
	}
	if (len == 0)
		return 1;
bad:
	return 0;
}

/* Set all mutable/predictable fields to the destination state, and all
   mutable/unpredictable fields to zero. */
void zero_out_for_ah(struct inet6_skb_parm *parm, char* packet) 
{
	struct ipv6hdr *hdr = (struct ipv6hdr*)packet;

	/* Main header */
	hdr->tclass1=0;
	hdr->tclass2_flow[0]=0;
	hdr->tclass2_flow[1]=0;
	hdr->tclass2_flow[2]=0;
	hdr->hop_limit=0;
	/* Mutable/unpredictable Option headers */
	/* AH header */

	if (parm->auth) {
		struct ipv6_auth_hdr* authhdr =
			(struct ipv6_auth_hdr*)(packet + parm->auth);
		int len = ((authhdr->hdrlen - 1)  << 2);
		memset(authhdr->auth_data,0,len);
	}

	if (parm->hop) {
		struct ipv6_hopopt_hdr* hopopthdr =
			(struct ipv6_hopopt_hdr*)(packet + parm->hop);
		if (!zero_out_mutable_opts(hopopthdr))
			printk(KERN_WARNING
				"overrun when muting hopopts\n");
	}

	if (parm->dst0) {
		struct ipv6_destopt_hdr* destopthdr0 =
			(struct ipv6_destopt_hdr*)(packet + parm->dst0);
		if (!zero_out_mutable_opts(destopthdr0))
			printk(KERN_WARNING
				"overrun when muting destopt\n");
	}

	if (parm->dst1) {
		struct ipv6_destopt_hdr* destopthdr1 =
			(struct ipv6_destopt_hdr*)(packet + parm->dst1);
		if (!zero_out_mutable_opts(destopthdr1))
			printk(KERN_WARNING
				"overrun when muting destopt\n");
	}
}

int ipsec6_ah_calc(const void *data, unsigned length, inet_getfrag_t getfrag, 
		struct sk_buff *skb, struct ipv6_auth_hdr *authhdr, struct ipsec_sp *policy)
{
	struct ipsec_sa *sa = NULL;
	char* pseudo_packet;
	int packetlen;
	struct inet6_skb_parm* parm;
	struct ipv6_auth_hdr *pseudo_authhdr = NULL;
	__u8* authdata = NULL;
	
	IPSEC6_DEBUG("called.\n");
	if(!policy){
		return -EINVAL;
	}

	if (!data) length=0;
	if (!skb) {
		IPSEC6_DEBUG("skb is NULL!\n");
		return -EINVAL;
	}

	parm = (struct inet6_skb_parm*)skb->cb;

	if (!authhdr) {
		if (parm->auth) {
			authhdr = (struct ipv6_auth_hdr*)(skb->nh.raw + parm->auth);
		} else {
			return -EINVAL;
		}
	}

	read_lock_bh(&policy->lock);

	if (!policy->auth_sa_idx || !policy->auth_sa_idx->sa) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(ah) SA missing.\n", __FUNCTION__);
		read_unlock_bh(&policy->lock);
		return -EINVAL;
	}

	ipsec_sa_hold(policy->auth_sa_idx->sa);
	sa = policy->auth_sa_idx->sa;

	read_unlock_bh(&policy->lock);

	packetlen = ntohs(skb->nh.ipv6h->payload_len) + sizeof(struct ipv6hdr);

	pseudo_packet = kmalloc(packetlen,GFP_ATOMIC);
	if (!pseudo_packet) {
		ipsec_sa_put(sa);
		return -ENOMEM;
	}

	if (length>0) {
		struct in6_addr *addr;
		addr=&skb->nh.ipv6h->saddr;
		getfrag(data,addr,&pseudo_packet[skb->len],0,length);
	}

	pseudo_authhdr = (struct ipv6_auth_hdr*)(pseudo_packet + parm->auth);

	authdata=kmalloc((sa->auth_algo.dx)->di->blocksize, GFP_ATOMIC);
	if (!authdata) {
		kfree(pseudo_packet);
		ipsec_sa_put(sa);
		return -ENOMEM;
	}

	write_lock_bh(&sa->lock);

	/* authhdr->spi = htonl(sa->spi); */
	IPSEC6_DEBUG("spi is 0x%x\n", ntohl(sa->spi));
	authhdr->spi = sa->spi; /* -mk */
	authhdr->seq_no = htonl(++sa->replay_window.seq_num);

	memcpy(pseudo_packet,skb->nh.ipv6h,skb->len);

	pseudo_authhdr->spi = authhdr->spi;
	pseudo_authhdr->seq_no = authhdr->seq_no;

	zero_out_for_ah(parm, pseudo_packet);

	sa->auth_algo.dx->di->hmac_atomic(sa->auth_algo.dx,
			sa->auth_algo.key,
			sa->auth_algo.key_len,
			pseudo_packet, packetlen, authdata);

	memcpy(authhdr->auth_data, authdata, sa->auth_algo.digest_len);
	
	if (!sa->fuse_time) {
		sa->fuse_time = jiffies;
		sa->lifetime_c.usetime = (sa->fuse_time)/HZ;
		ipsec_sa_mod_timer(sa);
		IPSEC6_DEBUG("set fuse_time = %lu\n", sa->fuse_time);
	}

	sa->lifetime_c.bytes += packetlen;
	IPSEC6_DEBUG("sa->lifetime_c.bytes=%-9u %-9u\n",	/* XXX: %-18Lu */
			(__u32)((sa->lifetime_c.bytes) >> 32), (__u32)(sa->lifetime_c.bytes));

	if (sa->lifetime_c.bytes >= sa->lifetime_s.bytes && sa->lifetime_s.bytes) {
		IPSEC6_DEBUG("change sa state DYING\n");
		sa->state = SADB_SASTATE_DYING;
	} 
	if (sa->lifetime_c.bytes >= sa->lifetime_h.bytes && sa->lifetime_h.bytes) {
		sa->state = SADB_SASTATE_DEAD;
		IPSEC6_DEBUG("change sa state DEAD\n");
	}

	write_unlock_bh(&sa->lock);
	ipsec_sa_put(sa);

	kfree(authdata);
	kfree(pseudo_packet); 
	return 0;

}

int ipsec6_out_get_ahsize(struct ipsec_sp *policy)
{
	int result = 0;
	struct ipsec_sa *sa_ah = NULL;

	IPSEC6_DEBUG("called.\n");

	if (!policy) return 0;

	write_lock_bh(&policy->lock);
	if (policy->auth_sa_idx && policy->auth_sa_idx->sa) {
		ipsec_sa_hold(policy->auth_sa_idx->sa);
		sa_ah = policy->auth_sa_idx->sa;
	}

	write_unlock_bh(&policy->lock);

	if (sa_ah) {
		read_lock_bh(&sa_ah->lock);
		if ( sa_ah->auth_algo.algo != SADB_AALG_NONE) {
			result += (offsetof(struct ipv6_auth_hdr, auth_data) + 
					sa_ah->auth_algo.digest_len + 7) & ~7;	/* 64 bit alignment */
		}
		read_unlock_bh(&sa_ah->lock);
		ipsec_sa_put(sa_ah);
	}

	IPSEC6_DEBUG("Calculated size is %d.\n", result);
	return result;
}

int ipsec6_out_get_espsize(struct ipsec_sp *policy)
{
	int result = 0;
	struct ipsec_sa *sa_esp = NULL;

	IPSEC6_DEBUG("called.\n");

	if (!policy) return 0;

	write_lock_bh(&policy->lock);

	if (policy->esp_sa_idx && policy->esp_sa_idx->sa) {
		ipsec_sa_hold(policy->esp_sa_idx->sa);
		sa_esp = policy->esp_sa_idx->sa;
	}
	write_unlock_bh(&policy->lock);

	if (sa_esp) {
		read_lock_bh(&sa_esp->lock);
		if ( sa_esp->esp_algo.algo != SADB_EALG_NONE){
			result += sizeof(struct ipv6_esp_hdr) - 8;
			result += sa_esp->esp_algo.cx->ci->ivsize;
			result += (sa_esp->esp_algo.cx->ci->blocksize + 3) & ~3;
			result += 4;	/* included pad_len and next_hdr  32 bit align */
		}else{
			read_unlock_bh(&sa_esp->lock);
			ipsec_sa_put(sa_esp);
			return 0;
		}
		if ( sa_esp->auth_algo.algo != SADB_AALG_NONE) {
			result += (sa_esp->auth_algo.digest_len + 3) & ~3;	/* 32 bit alignment */
		}
		read_unlock_bh(&sa_esp->lock);
		ipsec_sa_put(sa_esp);
	}
	IPSEC6_DEBUG("Calculated size is %d.\n", result);
	return result;
}

struct ipv6_txoptions *ipsec6_out_get_newopt(struct ipv6_txoptions *opt, struct ipsec_sp *policy)
{
	struct ipv6_txoptions *newopt = NULL;
	struct ipsec_sa *sa = NULL;
	int ah_len = 0;

	IPSEC6_DEBUG("called.\n");

	
	if (!policy) {
		if (net_ratelimit())
			printk(KERN_INFO "ipsec6_out_get_newopt: ipsec6_ptr/policy is NULL.\n");
		return NULL;
	}

	read_lock_bh(&policy->lock);

	if (policy->auth_sa_idx && policy->auth_sa_idx->sa) {

		ipsec_sa_hold(policy->auth_sa_idx->sa);
		sa = policy->auth_sa_idx->sa;
		read_unlock_bh(&policy->lock);
	
		read_lock_bh(&sa->lock);
	
		IPSEC6_DEBUG("use kerneli version\n");
		if ( sa->auth_algo.algo == SADB_AALG_NONE ) {
			if (net_ratelimit())
				printk(KERN_INFO "ipsec6_out_get_newopt: Hash algorithm %d not present.\n",
					sa->auth_algo.algo);
			read_unlock_bh(&sa->lock);
			ipsec_sa_put(sa);
			return NULL;
		}
	
		ah_len = (offsetof(struct ipv6_auth_hdr, auth_data) +
				sa->auth_algo.digest_len + 7) & ~7;
	
		IPSEC6_DEBUG("used kerneli version\n");
		IPSEC6_DEBUG("ah_len=%d hash_size=%d\n", ah_len, sa->auth_algo.digest_len);
		read_unlock_bh(&sa->lock);
		ipsec_sa_put(sa);
	
	} else {
		read_unlock_bh(&policy->lock);
	}

	if (opt) {
		IPSEC6_DEBUG("There have already been opt.\n");
		newopt = (struct ipv6_txoptions*)kmalloc(opt->tot_len + ah_len, GFP_ATOMIC);
		if (!newopt) {
			if (net_ratelimit())
				printk(KERN_WARNING "Couldn't allocate newopt - out of memory.\n");
			return NULL;
		}
		memset(newopt, 0, opt->tot_len + ah_len);
		memcpy(newopt, opt, sizeof(struct ipv6_txoptions));

		if (ah_len)
			newopt->auth = (struct ipv6_opt_hdr*)((char*)newopt + opt->tot_len);


	} else if (ah_len) {

		IPSEC6_DEBUG("There is not opt.\n");
		newopt = (struct ipv6_txoptions*) kmalloc(sizeof(struct ipv6_txoptions) + ah_len, GFP_ATOMIC);
		if (!newopt) {
			if (net_ratelimit()) {
				printk(KERN_INFO "ipsec6_out_get_newopt: could not allocate newopt.\n");
				return NULL;
			}
		}
		memset(newopt, 0, sizeof(struct ipv6_txoptions) + ah_len);
		newopt->auth = (struct ipv6_opt_hdr*)((char*)newopt + sizeof(struct ipv6_txoptions));
		newopt->tot_len = sizeof(struct ipv6_txoptions);
	}

	if (ah_len) {
		newopt->tot_len += ah_len;
		newopt->opt_flen += ah_len;
		newopt->auth->hdrlen = (ah_len >> 2) - 2;
	}

	return newopt;
}

/*** ESP ***/

void ipsec6_enc(const void *data, unsigned length, u8 proto, struct ipv6_txoptions *opt,
		void **newdata, unsigned *newlength, struct ipsec_sp *policy)
{
	struct ipsec_sa *sa = NULL;
	struct ipv6_esp_hdr *esphdr = NULL;
	u8* srcdata = NULL;
	u8* authdata = NULL;
	int encblocksize = 0;
	int encsize = 0, hashsize = 0, totalsize = 0;
	int dstoptlen = 0;
	int i;

	IPSEC6_DEBUG("called.\nData ptr is %p, data length is %d.\n",data,length);

	if (!policy) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s; ipsec policy is NULL\n", __FUNCTION__);
		return;
	}

	read_lock_bh(&policy->lock);
	if (!policy->esp_sa_idx || !policy->esp_sa_idx->sa) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(esp) SA missing.\n", __FUNCTION__);
		read_unlock_bh(&policy->lock);
		return;
	}
	ipsec_sa_hold(policy->esp_sa_idx->sa);
	sa = policy->esp_sa_idx->sa;
	read_unlock_bh(&policy->lock);

	write_lock_bh(&sa->lock);
	/* Get algorithms */
	if (sa->esp_algo.algo == SADB_EALG_NONE) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(esp) encryption algorithm not present.\n", __FUNCTION__);
		goto unlock_finish;
		return;
	}


	if (!(sa->esp_algo.cx->ci)){
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(esp) cipher_implementation not present.\n", __FUNCTION__);
		goto unlock_finish;
		return;
	}

	/* Calculate size */

	if (opt && opt->dst1opt) 
		dstoptlen = ipv6_optlen(opt->dst1opt);

	encblocksize = (sa->esp_algo.cx->ci->blocksize + 3) & ~3;
	encsize = dstoptlen + length + 2 + (encblocksize - 1);
	encsize -= encsize % encblocksize;

	/* The tail of payload does not have to be aligned with a multiple number of 64 bit.	*/
	/* 64 bit alignment is adapted to the position of top of header. 			*/

	if (sa->auth_algo.algo != SADB_AALG_NONE)
		hashsize = sa->auth_algo.digest_len;


	totalsize = sizeof(struct ipv6_esp_hdr) - 8 + sa->esp_algo.cx->ci->ivsize + encsize + hashsize;
	IPSEC6_DEBUG("IV size=%d, enc size=%d hash size=%d, total size=%d\n",
			 sa->esp_algo.cx->ci->ivsize, encsize, hashsize, totalsize);
	
	/* Get memory */
	esphdr = kmalloc(totalsize, GFP_ATOMIC);
	srcdata = kmalloc(encsize, GFP_ATOMIC);
	if (!esphdr || !srcdata) {
		if (net_ratelimit())
			printk(KERN_WARNING "ipsec6_enc: Out of memory.\n");
		if (esphdr) kfree(esphdr);
		if (srcdata) kfree(srcdata);
		goto unlock_finish;
		return;
	}

	memset(esphdr, 0, totalsize);
	memset(srcdata, 0, encsize);
	/* Handle sequence number and fill in header fields */
	esphdr->spi = sa->spi;
	esphdr->seq_no = htonl(++sa->replay_window.seq_num);

	/* Get source data, fill in padding and trailing fields */
	if (opt && opt->dst1opt) 
		memcpy(srcdata, opt->dst1opt, dstoptlen);

	memcpy(srcdata + dstoptlen, data, length);
	for (i = length + dstoptlen; i < encsize-2; i++) 
		srcdata[i] = (u8)(i-length+dstoptlen+1);
	srcdata[encsize-2] = (encsize-2)-length-dstoptlen;
	IPSEC6_DEBUG("length=%d, encsize=%d\n", length+dstoptlen, encsize);
	IPSEC6_DEBUG("encsize-2=%d\n", srcdata[encsize-2]);

	if (opt && opt->dst1opt) {
		/* ((struct ipv6_opt_hdr*)srcdata)->nexthdr = proto; */
		srcdata[0] = proto;
		opt->dst1opt = NULL;
		opt->opt_flen -= dstoptlen;
		srcdata[encsize-1] = NEXTHDR_DEST;
	} else {
		srcdata[encsize-1] = proto;
	}

	/* Do encryption */

	if (!(sa->esp_algo.iv)) { /* first packet */
		sa->esp_algo.iv = kmalloc(sa->esp_algo.cx->ci->ivsize, GFP_ATOMIC); /* kfree at SA removed */
		get_random_bytes(sa->esp_algo.iv, sa->esp_algo.cx->ci->ivsize);
		IPSEC6_DEBUG("IV initilized.\n");
	}  /* else, had inserted a stored iv (last packet block) */

#ifdef CONFIG_IPSEC_DEBUG
	{
		int i;
		IPSEC6_DEBUG("IV is 0x");
		if (sysctl_ipsec_debug_ipv6) {
			for (i=0; i < sa->esp_algo.cx->ci->ivsize ; i++) {
				printk(KERN_DEBUG "%x", (u8)(sa->esp_algo.iv[i]));
			}
		}
	}
#endif /* CONFIG_IPSEC_DEBUG */
	sa->esp_algo.cx->ci->encrypt_atomic_iv(sa->esp_algo.cx, srcdata,
					(u8 *)&esphdr->enc_data + sa->esp_algo.cx->ci->ivsize, encsize, sa->esp_algo.iv);
	memcpy(esphdr->enc_data, sa->esp_algo.iv, sa->esp_algo.cx->ci->ivsize);
	kfree(srcdata);
	srcdata=NULL;
	/* copy last block for next IV (src: enc_data + ivsize + encsize - ivsize) */
	memcpy(sa->esp_algo.iv, esphdr->enc_data + encsize, sa->esp_algo.cx->ci->ivsize);
	/* if CONFIG_IPSEC_DEBUG isn't defined here is finish of encryption process */

	if(sa->auth_algo.algo){
		authdata = kmalloc(sa->auth_algo.dx->di->blocksize, GFP_ATOMIC);
		if (!authdata) {
			if (net_ratelimit())
				printk(KERN_WARNING "ipsec6_enc: Out of memory.\n");
			kfree(esphdr);
			goto unlock_finish;
			return;
		}
		memset(authdata, 0, sa->auth_algo.dx->di->blocksize);
		sa->auth_algo.dx->di->hmac_atomic(sa->auth_algo.dx,
				sa->auth_algo.key,
				sa->auth_algo.key_len,
				(char*)esphdr, totalsize-hashsize, authdata);
		memcpy(&((char*)esphdr)[8 + sa->esp_algo.cx->ci->ivsize + encsize],
			authdata, sa->auth_algo.digest_len);

		kfree(authdata);
	}	

	if (!sa->fuse_time) {
		sa->fuse_time = jiffies;
		sa->lifetime_c.usetime = (sa->fuse_time)/HZ;
		ipsec_sa_mod_timer(sa);
		IPSEC6_DEBUG("set fuse_time = %lu\n", sa->fuse_time);
	}
	sa->lifetime_c.bytes += totalsize;
	IPSEC6_DEBUG("sa->lifetime_c.bytes=%-9u %-9u\n",	/* XXX: %-18Lu */
			(__u32)((sa->lifetime_c.bytes) >> 32), (__u32)(sa->lifetime_c.bytes));
	if (sa->lifetime_c.bytes >= sa->lifetime_s.bytes && sa->lifetime_s.bytes) {
		sa->state = SADB_SASTATE_DYING;
		IPSEC6_DEBUG("change sa state DYING\n");
	} 
	if (sa->lifetime_c.bytes >= sa->lifetime_h.bytes && sa->lifetime_h.bytes) {
		sa->state = SADB_SASTATE_DEAD;
		IPSEC6_DEBUG("change sa state DEAD\n");
	}

	write_unlock_bh(&sa->lock);
	ipsec_sa_put(sa);

	authdata = NULL;
	/* Set return values */
	*newdata = esphdr;
	*newlength = totalsize;
	return;

unlock_finish:
	write_unlock_bh(&sa->lock);
	ipsec_sa_put(sa);
	return;
}

void ipsec6_out_finish(struct ipv6_txoptions *opt, struct ipsec_sp *policy)
{

	if (opt) {
		kfree(opt);
	}

	if (policy) {
		ipsec_sp_put(policy);
	}
}

