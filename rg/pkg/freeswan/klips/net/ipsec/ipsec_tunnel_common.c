/*
 * Common IPSEC Tunneling code.
 * Copyright (C) 1996, 1997  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs.
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

#include <linux/config.h>
#include <linux/version.h>
#include "ipsec_kversion.h"

#define __NO_VERSION__

#include <linux/skbuff.h>
#include <net/protocol.h>
#ifdef NET_21
#include <asm/uaccess.h>
#endif
#include <asm/checksum.h>
#include <freeswan.h>
#include <net/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include "ipsec_sa.h"
#include "ipsec_tunnel.h"
#include "ipsec_log.h"
#ifdef CONFIG_IPSEC_ESP
# include "ipsec_esp.h"
#endif

ipsec_dev_list *ipsec_dev_head;

struct net_device *ipsec_get_first_device(void)
{
    if (ipsec_dev_head)
	return ipsec_dev_head->ipsec_dev;
    return NULL;
}

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
void ipsec_tunnel_correct_tcp_udp_csum(struct sk_buff *skb,
    struct iphdr *iph, struct ipsec_sa *tdbp)
{
    /**
     * NAT-Traversal and Transport Mode:
     *   we need to correct TCP/UDP checksum
     *
     * If we've got NAT-OA, we can fix checksum without recalculation.
     * If we don't we can zero udp checksum.
     */
    __u32 natt_oa = tdbp->ips_natt_oa ?
	((struct sockaddr_in*)(tdbp->ips_natt_oa))->sin_addr.s_addr : 0;
    __u16 pkt_len = skb->tail - (unsigned char *)iph;
    __u16 data_len = pkt_len - (iph->ihl << 2);
    switch (iph->protocol) {
    case IPPROTO_TCP:
	if (data_len >= sizeof(struct tcphdr)) {
	    struct tcphdr *tcp = (struct tcphdr *)((__u32 *)iph+iph->ihl);
	    if (natt_oa) {
		__u32 buff[2] = { ~iph->daddr, natt_oa };
		KLIPS_PRINT(debug_tunnel,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "NAT-T & TRANSPORT: "
		    "fix TCP checksum using NAT-OA\n");
		tcp->check = csum_fold(
		    csum_partial((unsigned char *)buff, sizeof(buff),
		    tcp->check^0xffff));
	    }
	    else {
		KLIPS_PRINT(debug_tunnel,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "NAT-T & TRANSPORT: do not recalc TCP checksum\n");
	    }
	}
	else {
	    KLIPS_PRINT(debug_tunnel,
		"klips_debug:ipsec_tunnel_start_xmit: "
		"NAT-T & TRANSPORT: can't fix TCP checksum\n");
	}
	break;
    case IPPROTO_UDP:
	if (data_len >= sizeof(struct udphdr)) {
	    struct udphdr *udp = (struct udphdr *)((__u32 *)iph+iph->ihl);
	    if (udp->check == 0) {
		KLIPS_PRINT(debug_tunnel,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "NAT-T & TRANSPORT: UDP checksum already 0\n");
	    }
	    else if (natt_oa) {
		__u32 buff[2] = { ~iph->daddr, natt_oa };
		KLIPS_PRINT(debug_tunnel,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "NAT-T & TRANSPORT: "
		    "fix UDP checksum using NAT-OA\n");
		udp->check = csum_fold(
		    csum_partial((unsigned char *)buff, sizeof(buff),
		    udp->check^0xffff));
	    }
	    else {
		KLIPS_PRINT(debug_tunnel,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "NAT-T & TRANSPORT: zero UDP checksum\n");
		udp->check = 0;
	    }
	}
	else {
	    KLIPS_PRINT(debug_tunnel,
		"klips_debug:ipsec_tunnel_start_xmit: "
		"NAT-T & TRANSPORT: can't fix UDP checksum\n");
	}
	break;
    default:
	KLIPS_PRINT(debug_tunnel,
	    "klips_debug:ipsec_tunnel_start_xmit: "
	    "NAT-T & TRANSPORT: non TCP/UDP packet -- do nothing\n");
	break;
    }
}

int ipsec_tunnel_udp_encap(struct sk_buff *skb, uint8_t natt_type,
    uint8_t natt_head, uint16_t natt_sport, uint16_t natt_dport)
{
    struct iphdr *ipp = skb->nh.iph;
    struct udphdr *udp;
    int iphlen;

    KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
	"klips_debug:ipsec_tunnel_udp_encap: "
	"encapsuling packet into UDP (NAT-Traversal) (%d %d)\n",
	natt_type, natt_head);

    iphlen = ipp->ihl << 2;
    ipp->tot_len = htons(ntohs(ipp->tot_len) + natt_head);
    if(skb_tailroom(skb) < natt_head) {
	printk(KERN_WARNING "klips_error:ipsec_tunnel_udp_encap: "
	    "tried to skb_put %d, %d available. "
	    "This should never happen, please report.\n",
	    natt_head,
	    skb_tailroom(skb));
	return -1;
    }
    skb_put(skb, natt_head);

    udp = (struct udphdr *)((char *)ipp + iphlen);

    /* move ESP hdr after UDP hdr */
    memmove((void *)((char *)udp + natt_head),
	(void *)(udp),
	ntohs(ipp->tot_len) - iphlen - natt_head);

    /* clear UDP & Non-IKE Markers (if any) */
    memset(udp, 0, natt_head);

    /* fill UDP with usefull informations ;-) */
    udp->source = htons(natt_sport);
    udp->dest = htons(natt_dport);
    udp->len = htons(ntohs(ipp->tot_len) - iphlen);

    /* set protocol */
    ipp->protocol = IPPROTO_UDP;

    /* fix IP checksum */
    ipp->check = 0;
    ipp->check = ip_fast_csum((unsigned char *)ipp, ipp->ihl);

    return 0;
}

#endif

