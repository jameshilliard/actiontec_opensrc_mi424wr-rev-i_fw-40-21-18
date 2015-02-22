/*
 * This file is defived from ${SRC}/sys/net/radix.c of BSD 4.4lite
 *
 * Variable and procedure names have been modified so that they don't
 * conflict with the original BSD code, as a small number of modifications
 * have been introduced and we may want to reuse this code in BSD.
 * 
 * The `j' in `radij' is pronounced as a voiceless guttural (like a Greek
 * chi or a German ch sound (as `doch', not as in `milch'), or even a 
 * spanish j as in Juan.  It is not as far back in the throat like
 * the corresponding Hebrew sound, nor is it a soft breath like the English h.
 * It has nothing to do with the Dutch ij sound.
 * 
 * Here is the appropriate copyright notice:
 */

/*
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)radix.c	8.2 (Berkeley) 1/4/94
 */

/*
 * Routines to build and maintain radix trees for routing lookups.
 */

#include <linux/config.h>
#include <linux/version.h>

#include "ipsec_param.h"

#ifdef MALLOC_SLAB
# include <linux/slab.h> /* kmalloc() */
#else /* MALLOC_SLAB */
# include <linux/malloc.h> /* kmalloc() */
#endif /* MALLOC_SLAB */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/netdevice.h>   /* struct net_device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/skbuff.h>
#ifdef NET_21
# include <asm/uaccess.h>
# include <linux/in6.h>
#endif /* NET_21 */
#include <asm/checksum.h>
#include <net/ip.h>

#include <freeswan.h>
#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_log.h"

int	maj_keylen;
/* replaced the original radix-tree code with a simple linked list in order to
 * support ranges */

#define ENC_IP(a, fld) ntohl(((struct sockaddr_encap *)(a))->fld.s_addr)
#define ENC_ADDR(a) ((struct sockaddr_encap *)(a))

struct radij_node *
rj_match(v_arg, head)
	void *v_arg; /* pointer to sockaddr_encap */
	struct radij_node_head *head;
{
    struct radij_node *t;

#define IS_MATCH_IP(v, n, fld) \
    /* host match */ \
    ((ENC_IP(v, fld) == ENC_IP((n)->rj_key, fld)) || \
    /* mask match */ \
    ((n)->rj_mask && \
    !((ENC_IP(v, fld) ^ ENC_IP((n)->rj_key, fld)) & ENC_IP((n)->rj_mask, fld))) || \
    /* range match */ \
    ((n)->rj_range_end && \
    ENC_IP(v, fld) >= ENC_IP((n)->rj_key, fld) && \
    ENC_IP(v, fld) <= ENC_IP((n)->rj_range_end, fld)))

#define IS_MATCH_PORT(key, val, fld) \
    (!ENC_ADDR(key)->fld || \
	ENC_ADDR(key)->fld == ENC_ADDR(val)->fld)

#define IS_MATCH_PROTO_PORT(key, val, proto_fld, sport_fld, dport_fld) \
    (!ENC_ADDR(key)->proto_fld || \
    (ENC_ADDR(key)->proto_fld == ENC_ADDR(val)->proto_fld && \
    IS_MATCH_PORT(key, val, sport_fld) && \
    IS_MATCH_PORT(key, val, dport_fld)))

    for (t = head->rnh_treetop; t; t = t->rj_next)
    {
	if (IS_MATCH_IP(v_arg, t, sen_ip_src) &&
	    IS_MATCH_IP(v_arg, t, sen_ip_dst) &&
	    IS_MATCH_PROTO_PORT(t->rj_key, v_arg, sen_proto, sen_sport,
	    sen_dport))
	{
	    return t;
	}
    }

#undef IS_MATCH_PROTO_PORT
#undef IS_MATCH_PORT
#undef IS_MATCH_IP

    KLIPS_PRINT(debug_radij, "klips_debug:rj_match: ***** not found.\n");
    return NULL;
};

int
rj_addroute(v_arg, n_arg, range_end, head, treenodes)
	void *v_arg, *n_arg, *range_end;
	struct radij_node_head *head;
	struct radij_node treenodes[2];
{
    treenodes->rj_key = v_arg;
    treenodes->rj_mask = n_arg;
    treenodes->rj_range_end = range_end;

    treenodes->rj_next = head->rnh_treetop;
    head->rnh_treetop = treenodes;
    return 0;
}

int
rj_delete(v_arg, netmask_arg, range_end, head, node)
	void *v_arg, *netmask_arg, *range_end;
	struct radij_node_head *head;
	struct radij_node **node;
{
    struct radij_node **tp, *t;

    for (tp = &head->rnh_treetop; (t = *tp); tp = &t->rj_next)
    {
	if (memcmp(v_arg, t->rj_key, sizeof(struct sockaddr_encap)))
	    continue;
	if (netmask_arg && (!t->rj_mask ||
	    memcmp(netmask_arg, t->rj_mask, sizeof(struct sockaddr_encap))))
	{
	    continue;
	}
	if (range_end && (!t->rj_range_end ||
	    memcmp(range_end, t->rj_range_end, sizeof(struct sockaddr_encap))))
	{
	    continue;
	}
	break;
    }

    if (!t)
	return -ENOENT;
    *tp = t->rj_next;
    *node = t;
    return 0;
}

int
rj_walktree(h, f, w)
	struct radij_node_head *h;
	register int (*f)(struct radij_node *,void *);
	void *w;
{
    struct radij_node *t, *n;
    int error;

    t = h->rnh_treetop;
    while (t)
    {
	n = t->rj_next; /* save next as t may be deleted by f */
	if (f && (error = f(t, w)))
	    return -error;
	t = n;
    }
    return 0;
}

int
rj_inithead(head, off)
	void **head;
	int off;
{
	register struct radij_node_head *rnh;

	if (*head)
		return (1);
	R_Malloc(rnh, struct radij_node_head *, sizeof (*rnh));
	if (rnh == NULL)
		return (0);
	Bzero(rnh, sizeof (*rnh));
	*head = rnh;
	return (1);
}

void
rj_init()
{
}

static int rj_print_node(struct radij_node *n, void *o)
{
    struct sockaddr_encap *addr = (struct sockaddr_encap *)n->rj_key;

        printk("%08x->%08x mask: %08x->%08x range-end: %08x->%08x "
	"proto: %d ports: %d-%d\n",
	ENC_IP(n->rj_key, sen_ip_src), ENC_IP(n->rj_key, sen_ip_dst), 
	ENC_IP(n->rj_mask, sen_ip_src), ENC_IP(n->rj_mask, sen_ip_dst), 
	ENC_IP(n->rj_range_end, sen_ip_src),
	ENC_IP(n->rj_range_end, sen_ip_dst),
	addr->sen_proto,
	ntohs(addr->sen_sport),
	ntohs(addr->sen_dport));
    return 0;
}

void
rj_dumptrees(void)
{
    rj_walktree(rnh, rj_print_node, NULL);
}

int
radijcleartree(void)
{
	return rj_walktree(rnh, ipsec_rj_walker_delete, NULL);
}

int
radijcleanup(void)
{
	int error = 0;

	error = radijcleartree();

	if(rnh) {
		kfree(rnh);
	}

	return error;
}

