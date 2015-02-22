/*
 * Common receive code.
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
#include "ipsec_rcv.h"
#include "ipsec_log.h"
#ifdef CONFIG_IPSEC_ESP
#include "ipsec_esp.h"
#endif

#if !defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)

/* Decapsulate a UDP encapsulated ESP packet */
struct sk_buff *ipsec_rcv_natt_decap(struct sk_buff *skb
				     , struct sock *sk
				     , struct ipsec_rcv_state *irs
				     , int *udp_decap_ret_p)
{
	*udp_decap_ret_p = 0;
	if (sk && skb->nh.iph && skb->nh.iph->protocol==IPPROTO_UDP) {
		/**
		 * Packet comes from udp_queue_rcv_skb so it is already defrag,
		 * checksum verified, ... (ie safe to use)
		 *
		 * If the packet is not for us, return -1 and udp_queue_rcv_skb
		 * will continue to handle it (do not kfree skb !!).
		 */

#ifndef UDP_OPT_IN_SOCK
		struct udp_opt {
			__u32 esp_in_udp;
		};
		struct udp_opt *tp =  (struct udp_opt *)&(sk->tp_pinfo.af_tcp);
#else
		struct udp_opt *tp =  &(sk->tp_pinfo.af_udp);
#endif

		struct iphdr *ip = (struct iphdr *)skb->nh.iph;
		struct udphdr *udp = (struct udphdr *)((__u32 *)ip+ip->ihl);
		__u8 *udpdata = (__u8 *)udp + sizeof(struct udphdr);
		__u32 *udpdata32 = (__u32 *)udpdata;
		
		irs->natt_sport = ntohs(udp->source);
		irs->natt_dport = ntohs(udp->dest);
	  
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "suspected ESPinUDP packet (NAT-Traversal) [%d].\n",
			    tp->esp_in_udp);
		KLIPS_IP_PRINT(debug_rcv, ip);
	  
		if (udpdata < skb->tail) {
			unsigned int len = skb->tail - udpdata;
			if ((len==1) && (udpdata[0]==0xff)) {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    /* not IPv6 compliant message */
					    "NAT-keepalive from %d.%d.%d.%d.\n", NIPQUAD(ip->saddr));
				*udp_decap_ret_p = 0;
				return NULL;
			}
			else if ( (tp->esp_in_udp == ESPINUDP_WITH_NON_IKE) &&
				  (len > (2*sizeof(__u32) + sizeof(struct esp))) &&
				  (udpdata32[0]==0) && (udpdata32[1]==0) ) {
				/* ESP Packet with Non-IKE header */
				KLIPS_PRINT(debug_rcv, 
					    "klips_debug:ipsec_rcv: "
					    "ESPinUDP pkt with Non-IKE - spi=0x%x\n",
					    ntohl(udpdata32[2]));
				irs->natt_type = ESPINUDP_WITH_NON_IKE;
				irs->natt_len = sizeof(struct udphdr)+(2*sizeof(__u32));
			}
			else if ( (tp->esp_in_udp == ESPINUDP_WITH_NON_ESP) &&
				  (len > sizeof(struct esp)) &&
				  (udpdata32[0]!=0) ) {
				/* ESP Packet without Non-ESP header */
				irs->natt_type = ESPINUDP_WITH_NON_ESP;
				irs->natt_len = sizeof(struct udphdr);
				KLIPS_PRINT(debug_rcv, 
					    "klips_debug:ipsec_rcv: "
					    "ESPinUDP pkt without Non-ESP - spi=0x%x\n",
					    ntohl(udpdata32[0]));
			}
			else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "IKE packet - not handled here\n");
				*udp_decap_ret_p = -1;
				return NULL;
			}
		}
		else {
			return NULL;
		}
	}
	return skb;
}

int ipsec_espinudp_encap(struct sk_buff *skb, struct sock *sk)
{
    return _ipsec_rcv(skb, sk);
}
#endif

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
void ipsec_rcv_natt_correct_tcp_udp_csum(struct sk_buff *skb,
    struct iphdr *ipp, struct ipsec_sa *tdbp)
{
    /**
     * NAT-Traversal and Transport Mode:
     *   we need to correct TCP/UDP checksum
     *
     * If we've got NAT-OA, we can fix checksum without recalculation.
     */
    __u32 natt_oa = tdbp->ips_natt_oa ?
      ((struct sockaddr_in*)(tdbp->ips_natt_oa))->sin_addr.s_addr : 0;
    __u16 pkt_len = skb->tail - (unsigned char *)ipp;
    __u16 data_len = pkt_len - (ipp->ihl << 2);
    
    switch (ipp->protocol) {
    case IPPROTO_TCP:
      if (data_len >= sizeof(struct tcphdr)) {
        struct tcphdr *tcp = skb->h.th;
        if (natt_oa) {
          __u32 buff[2] = { ~natt_oa, ipp->saddr };
          KLIPS_PRINT(debug_rcv,
      		"klips_debug:ipsec_rcv: "
      		"NAT-T & TRANSPORT: "
      		"fix TCP checksum using NAT-OA\n");
          tcp->check = csum_fold(
      			   csum_partial((unsigned char *)buff, sizeof(buff),
      					tcp->check^0xffff));
        }
        else {
          KLIPS_PRINT(debug_rcv,
      		"klips_debug:ipsec_rcv: "
      		"NAT-T & TRANSPORT: recalc TCP checksum\n");
          if (pkt_len > (ntohs(ipp->tot_len)))
            data_len -= (pkt_len - ntohs(ipp->tot_len));
          tcp->check = 0;
          tcp->check = csum_tcpudp_magic(ipp->saddr, ipp->daddr,
      				   data_len, IPPROTO_TCP,
      				   csum_partial((unsigned char *)tcp, data_len, 0));
        }
      }
      else {
        KLIPS_PRINT(debug_rcv,
      	      "klips_debug:ipsec_rcv: "
      	      "NAT-T & TRANSPORT: can't fix TCP checksum\n");
      }
      break;
    case IPPROTO_UDP:
      if (data_len >= sizeof(struct udphdr)) {
        struct udphdr *udp = skb->h.uh;
        if (udp->check == 0) {
          KLIPS_PRINT(debug_rcv,
      		"klips_debug:ipsec_rcv: "
      		"NAT-T & TRANSPORT: UDP checksum already 0\n");
        }
        else if (natt_oa) {
          __u32 buff[2] = { ~natt_oa, ipp->saddr };
          KLIPS_PRINT(debug_rcv,
      		"klips_debug:ipsec_rcv: "
      		"NAT-T & TRANSPORT: "
      		"fix UDP checksum using NAT-OA\n");
          udp->check = csum_fold(
      			   csum_partial((unsigned char *)buff, sizeof(buff),
      					udp->check^0xffff));
        }
        else {
          KLIPS_PRINT(debug_rcv,
      		"klips_debug:ipsec_rcv: "
      		"NAT-T & TRANSPORT: zero UDP checksum\n");
          udp->check = 0;
        }
      }
      else {
        KLIPS_PRINT(debug_rcv,
      	      "klips_debug:ipsec_rcv: "
      	      "NAT-T & TRANSPORT: can't fix UDP checksum\n");
      }
      break;
    default:
      KLIPS_PRINT(debug_rcv,
      	    "klips_debug:ipsec_rcv: "
      	    "NAT-T & TRANSPORT: non TCP/UDP packet -- do nothing\n");
      break;
    }
}
#endif

struct sk_buff *ipsec_rcv_unclone(struct sk_buff *skb,
                                  int hard_header_len)
{
	/* if skb was cloned (most likely due to a packet sniffer such as
	   tcpdump being momentarily attached to the interface), make
	   a copy of our own to modify */
	if(skb_cloned(skb)) {
		/* include any mac header while copying.. */
		if(skb_headroom(skb) < hard_header_len) {
			ipsec_log(KERN_WARNING "klips_error:ipsec_rcv: "
			       "tried to skb_push hhlen=%d, %d available.  This should never happen, please report.\n",
			       hard_header_len,
			       skb_headroom(skb));
			goto rcvleave;
		}
		skb_push(skb, hard_header_len);
		if
#ifdef SKB_COW_NEW
                  (skb_cow(skb, skb_headroom(skb)) != 0)
#else /* SKB_COW_NEW */
                  ((skb = skb_cow(skb, skb_headroom(skb))) == NULL)
#endif /* SKB_COW_NEW */
		{
			goto rcvleave;
		}
		if(skb->len < hard_header_len) {
			ipsec_log(KERN_WARNING "klips_error:ipsec_rcv: "
			       "tried to skb_pull hhlen=%d, %d available.  This should never happen, please report.\n",
			       hard_header_len,
			       skb->len);
			goto rcvleave;
		}
		skb_pull(skb, hard_header_len);
	}
	return skb;

rcvleave:
	ipsec_kfree_skb(skb);
	return NULL;
}

