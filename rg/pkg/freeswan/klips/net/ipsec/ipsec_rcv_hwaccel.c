/*
 * receive code
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

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */

#define IPSEC_KLIPS1_COMPAT 1
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
#include <freeswan.h>
#ifdef SPINLOCK
# ifdef SPINLOCK_23
#  include <linux/spinlock.h> /* *lock* */
# else /* SPINLOCK_23 */
#  include <asm/spinlock.h> /* *lock* */
# endif /* SPINLOCK_23 */
#endif /* SPINLOCK */
#ifdef NET_21
# include <asm/uaccess.h>
# include <linux/in6.h>
# define proto_priv cb
#endif /* NET21 */
#include <asm/checksum.h>
#include <net/ip.h>

#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_sa.h"

#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_xform.h"
#include "ipsec_tunnel.h"
#include "ipsec_rcv.h"
# include "ipsec_ah.h"
# include "ipsec_esp.h"
# include "ipcomp.h"

#include <pfkeyv2.h>
#include <pfkey.h>

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
#include <linux/udp.h>
#endif

#include "ipsec_proto.h"

/* IXP425 cryptoAcc Glue Code */
#include "IxCryptoAcc.h"
#include "IxOsBuffMgt.h"
#include "ipsec_glue_mbuf.h"
#include "ipsec_glue.h"
#include "ipsec_glue_desc.h"
#include "ipsec_hwaccel.h"
#include <linux/tqueue.h>
#include "ipsec_log.h"
#include "ipsec_rcv_common.h"
#include "ipsec_tunnel_common.h"

#define PROTO	9   /* Protocol field offset in IP Header */
#define MAX_RCV_TASK_IN_SOFTIRQ (MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL)

#ifdef SPINLOCK
spinlock_t rcv_lock = SPIN_LOCK_UNLOCKED;
#else /* SPINLOCK */
spinlock_t rcv_lock = 0;
#endif /* SPINLOCK */

static void ipsec_rcv_next_transform (void *data);
static struct tq_struct rcv_task[MAX_RCV_TASK_IN_SOFTIRQ];
static __u32 rcvProducer = 0;
static __u32 rcvConsumer = 0;

#ifdef CONFIG_IPSEC_DEBUG
#include "ipsec_reject_debug.h"

int debug_ah = 0;
int debug_esp = 0;
int debug_rcv = 0;
#endif

int sysctl_ipsec_inbound_policy_check = 1;

/*
 * Check-replay-window routine, adapted from the original
 * by J. Hughes, from draft-ietf-ipsec-esp-des-md5-03.txt
 *
 *  This is a routine that implements a 64 packet window. This is intend-
 *  ed on being an implementation sample.
 */

DEBUG_NO_STATIC int
ipsec_checkreplaywindow(struct ipsec_sa*tdbp, __u32 seq, char *ipaddr_txt,
    char *buf, int buf_size)
{
	__u32 diff;

	if (tdbp->tdb_replaywin == 0)	/* replay shut off */
		return 1;
	if (seq == 0) {
		snprintf(buf, buf_size,
			    "klips_debug:ipsec_checkreplaywindow: "
			    "first or wrapped frame from %s, packet dropped\n",
			    ipaddr_txt);
		return 0;		/* first == 0 or wrapped */
	}

	/* new larger sequence number */
	if (seq > tdbp->tdb_replaywin_lastseq) {
		return 1;		/* larger is good */
	}
	diff = tdbp->tdb_replaywin_lastseq - seq;

	/* too old or wrapped */ /* if wrapped, kill off SA? */
	if (diff >= tdbp->tdb_replaywin) {
		snprintf(buf, buf_size,
			    "klips_debug:ipsec_checkreplaywindow: "
			    "too old frame from %s, packet dropped\n",
			    ipaddr_txt);
		return 0;
	}
	/* this packet already seen */
	if (tdbp->tdb_replaywin_bitmap & (1 << diff)) {
		snprintf(buf, buf_size,
			    "klips_debug:ipsec_checkreplaywindow: "
			    "duplicate frame from %s (sequence %d), packet dropped\n",
			    ipaddr_txt, seq);
		/* This will be always printed (ICSA). */
		KLIPS_PRINT(1, "%s", buf);
		return 0;
	}
	return 1;			/* out of order but good */
}

DEBUG_NO_STATIC int
ipsec_updatereplaywindow(struct ipsec_sa*tdbp, __u32 seq)
{
	__u32 diff;

	if (tdbp->tdb_replaywin == 0)	/* replay shut off */
		return 1;
	if (seq == 0)
		return 0;		/* first == 0 or wrapped */

	/* new larger sequence number */
	if (seq > tdbp->tdb_replaywin_lastseq) {
		diff = seq - tdbp->tdb_replaywin_lastseq;

		/* In win, set bit for this pkt */
		if (diff < tdbp->tdb_replaywin)
			tdbp->tdb_replaywin_bitmap =
				(tdbp->tdb_replaywin_bitmap << diff) | 1;
		else
			/* This packet has way larger seq num */
			tdbp->tdb_replaywin_bitmap = 1;

		if(seq - tdbp->tdb_replaywin_lastseq - 1 > tdbp->tdb_replaywin_maxdiff) {
			tdbp->tdb_replaywin_maxdiff = seq - tdbp->tdb_replaywin_lastseq - 1;
		}
		tdbp->tdb_replaywin_lastseq = seq;
		return 1;		/* larger is good */
	}
	diff = tdbp->tdb_replaywin_lastseq - seq;

	/* too old or wrapped */ /* if wrapped, kill off SA? */
	if (diff >= tdbp->tdb_replaywin) {
/*
		if(seq < 0.25*max && tdbp->tdb_replaywin_lastseq > 0.75*max) {
			deltdbchain(tdbp);
		}
*/
		return 0;
	}
	/* this packet already seen */
	if (tdbp->tdb_replaywin_bitmap & (1 << diff))
		return 0;
	tdbp->tdb_replaywin_bitmap |= (1 << diff);	/* mark as seen */
	return 1;			/* out of order but good */
}

#define IS_MATCH(ip, t_ip, t_mask, t_to) \
    ((t_to) ? (ip) >= (t_ip) && (ip) <= (t_to) : !(((ip) & (t_mask)) ^ (t_ip)))

/* IXP425 cryptoAcc Glue Code : ipsec_rcv_cb */
void ipsec_rcv_cb(
    UINT32 cryptoCtxId,
    IX_MBUF *pSrcMbuf,
    IX_MBUF *pDestMbuf,
    IxCryptoAccStatus status)
{
    struct sk_buff *skb = NULL;
    IpsecRcvDesc *pRcvDesc = NULL;
#ifdef CONFIG_IPSEC_DEBUG
    struct ipsec_pkt_info_t pkt_info;
#endif

    if (pSrcMbuf == NULL)
    {
        KLIPS_PRINT(debug_rcv,
                "klips_debug:ipsec_rcv: "
                "skb is NULL\n");
        return;
    }

    switch (status)
    {
        case IX_CRYPTO_ACC_STATUS_SUCCESS:
            KLIPS_PRINT(debug_rcv,
                    "klips_debug:ipsec_rcv: "
                    "transform successful.\n");

            spin_lock(&rcv_lock);

            if ((rcvProducer - rcvConsumer) != MAX_RCV_TASK_IN_SOFTIRQ)
            {
                rcvProducer = rcvProducer % MAX_RCV_TASK_IN_SOFTIRQ;
                INIT_LIST_HEAD(&rcv_task[rcvProducer].list);
                rcv_task[rcvProducer].sync = 0;
                rcv_task[rcvProducer].routine = ipsec_rcv_next_transform;
                rcv_task[rcvProducer].data = (void *) pSrcMbuf;
                queue_task(&rcv_task[rcvProducer], &tq_immediate);
                rcvProducer++;
                mark_bh(IMMEDIATE_BH);
            }
            else
            {
                KLIPS_PRINT(debug_rcv,
                    "klips_debug:ipsec_rcv: "
                    "soft IRQ task queue full.\n");

                /* Detach skb from mbuf */
                skb = mbuf_swap_skb(pSrcMbuf, NULL);
                /* get rcv desc from mbuf */
                pRcvDesc = (IpsecRcvDesc *) IX_MBUF_NEXT_PKT_IN_CHAIN_PTR (pSrcMbuf);
                ipsec_glue_mbuf_header_rel (pSrcMbuf);

                if (pRcvDesc)
                {
                    if(pRcvDesc->stats) {
                        (pRcvDesc->stats)->rx_dropped++;
                    }

                    if (pRcvDesc->tdbp)
                    {
                        spin_lock(&tdb_lock);
                        (pRcvDesc->tdbp)->ips_req_done_count++;
                        spin_unlock(&tdb_lock);
                    }

                    /* release desc */
                    ipsec_glue_rcv_desc_release (pRcvDesc);
                }

                if(skb) {
                    ipsec_kfree_skb(skb);
                }

		KLIPS_DEC_USE;
            }

            spin_unlock(&rcv_lock);
            break;

        case IX_CRYPTO_ACC_STATUS_AUTH_FAIL:
            /* Detach skb from mbuf */
            skb = mbuf_swap_skb(pSrcMbuf, NULL);
	    MAKE_REJECT_INFO(&pkt_info, skb);
            /* get rcv desc from mbuf */
            pRcvDesc = (IpsecRcvDesc *) IX_MBUF_NEXT_PKT_IN_CHAIN_PTR (pSrcMbuf);
            ipsec_glue_mbuf_header_rel (pSrcMbuf);
            KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
                    "klips_debug:ipsec_rcv: "
                    "auth failed on incoming packet, dropped\n");
            
            if (pRcvDesc)
            {
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                if (pRcvDesc->tdbp)
                {
                    spin_lock(&tdb_lock);
                    (pRcvDesc->tdbp)->tdb_auth_errs += 1;
                    (pRcvDesc->tdbp)->ips_req_done_count++;
                    spin_unlock(&tdb_lock);
                }
                /* release desc */
                ipsec_glue_rcv_desc_release (pRcvDesc);
            }

            if(skb) {
                ipsec_kfree_skb(skb);
            }

            KLIPS_DEC_USE;
            break;
        default:
            if(pRcvDesc->stats) {
                (pRcvDesc->stats)->rx_dropped++;
            }
            /* Detach skb from mbuf */
            skb = mbuf_swap_skb(pSrcMbuf, NULL);
	    MAKE_REJECT_INFO(&pkt_info, skb);
            KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                    "klips_debug:ipsec_rcv: "
                    "decapsulation on incoming packet failed, dropped\n");
            /* get rcv desc from mbuf */
            pRcvDesc = (IpsecRcvDesc *) IX_MBUF_NEXT_PKT_IN_CHAIN_PTR (pSrcMbuf);
            ipsec_glue_mbuf_header_rel (pSrcMbuf);

            if (pRcvDesc)
            {
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }

                if (pRcvDesc->tdbp)
                {
                    spin_lock(&tdb_lock);
                    (pRcvDesc->tdbp)->ips_req_done_count++;
                    spin_unlock(&tdb_lock);
                }
                /* release desc */
                ipsec_glue_rcv_desc_release (pRcvDesc);
            }

            if(skb) {
                ipsec_kfree_skb(skb);
            }

            KLIPS_DEC_USE;
            break;
    } /* end of switch (status) */
} /* end of ipsec_rcv_cb () */

static void ipsec_rcv_next_transform(void *data)
{
        struct sk_buff *skb = NULL;
        IpsecRcvDesc *pRcvDesc = NULL;
        IX_MBUF *pRetSrcMbuf = NULL;

        struct iphdr *ipp;
        int authlen;

#ifdef CONFIG_IPSEC_ESP
        struct esp *espp = NULL;
        int esphlen = 0;
        char iv[ESP_IV_MAXSZ];
        int pad = 0, padlen;
#endif /* !CONFIG_IPSEC_ESP */
#ifdef CONFIG_IPSEC_AH
	struct ah *ahp = NULL;
	int ahhlen = 0;
#endif /* CONFIG_IPSEC_AH */

#ifdef CONFIG_IPSEC_IPCOMP
	struct ipcomphdr*compp = NULL;
#endif /* CONFIG_IPSEC_IPCOMP */

#ifdef CONFIG_IPSEC_DEBUG
	struct ipsec_pkt_info_t pkt_info;
#endif
	char err_msg[128];

	int iphlen;
	unsigned char *dat;
	struct ipsec_sa *tdbp = NULL;
	struct sa_id said;

	char sa[SATOA_BUF];
	size_t sa_len;
	char ipaddr_txt[ADDRTOA_BUF];
	int i;
	struct in_addr ipaddr;
	__u8 next_header = 0;
	__u8 proto;
	int lifename_idx = 0;
	char *lifename[] = {
	    "bytes",
	    "addtime",
	    "usetime",
	    "packets"
	};

	int len;	/* packet length */
	int replay = 0;	/* replay value in AH or ESP packet */

	struct ipsec_sa* tdbprev = NULL;	/* previous SA from outside of packet */
	struct ipsec_sa* tdbnext = NULL;	/* next SA towards inside of packet */

    __u32 auth_start_offset = 0;
    __u32 auth_data_len = 0;
    __u32 crypt_start_offset = 0;
    __u32 crypt_data_len = 0;
    __u32 icv_offset = 0;
    IX_MBUF *src_mbuf;

    pRetSrcMbuf = (IX_MBUF *) data;

    spin_lock(&rcv_lock);
    rcvConsumer++;
    spin_unlock(&rcv_lock);

    if (pRetSrcMbuf == NULL)
    {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "NULL mbuf passed in.\n");
		return;
    }

    /* Detach skb from mbuf */
    skb = mbuf_swap_skb(pRetSrcMbuf, NULL);

    /* get rcv desc from mbuf */
    pRcvDesc = (IpsecRcvDesc *) IX_MBUF_NEXT_PKT_IN_CHAIN_PTR (pRetSrcMbuf);

    /* release src mbuf */
    ipsec_glue_mbuf_header_rel (pRetSrcMbuf);

    if (pRcvDesc == NULL) {
        KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "NULL Rcv Descriptor passed in.\n");
		goto rcvleave_cb;
	}

    if (skb == NULL) {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "NULL skb passed in.\n");
		goto rcvleave_cb;
	}

	if (skb->data == NULL) {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "NULL skb->data passed in, packet is bogus, dropping.\n");
		goto rcvleave_cb;
	}

    /* Restore tdbp from desc */
    tdbp = pRcvDesc->tdbp;

    if (tdbp == NULL)
    {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "Corrupted descriptor, dropping.\n");
		goto rcvleave_cb;
    }

    /* get ip header from skb */
    ipp = (struct iphdr *)skb->data;
    iphlen = ipp->ihl << 2;
    len  = skb->len;

#ifdef CONFIG_IPSEC_DEBUG
    pkt_info.has_spi_seq = 0;
    pkt_info.proto = ipp->protocol;
    pkt_info.src.s_addr = ipp->saddr;
    pkt_info.dst.s_addr = ipp->daddr;
#endif

    switch(ipp->protocol) {
        case IPPROTO_ESP:
                next_header = skb->data[pRcvDesc->icv_offset - 1];
                padlen = skb->data[pRcvDesc->icv_offset - 2];

		switch(tdbp->tdb_encalg) {
		    case ESP_3DES:
#ifdef USE_SINGLE_DES
		    case ESP_DES:
#endif /* USE_SINGLE_DES */
			esphlen = ESP_HEADER_LEN + EMT_ESPDES_IV_SZ;
			break;
		    case ESP_AES:
			esphlen = ESP_HEADER_LEN + EMT_ESPAES_IV_SZ;
			break;
		    case ESP_NULL:
			esphlen = offsetof(struct esp, esp_iv);
			break;
		    default:
			spin_lock(&tdb_lock);
			tdbp->ips_req_done_count++;
			tdbp->tdb_alg_errs += 1;
			spin_unlock(&tdb_lock);
			KLIPS_REJECT_INFO(&pkt_info,
			    "unsupported encryption protocol");
			if(pRcvDesc->stats) {
			    (pRcvDesc->stats)->rx_errors++;
			}
			goto rcvleave_cb;
		}

                pad = padlen + 2 + (len - pRcvDesc->icv_offset);
                {
                    int badpad = 0;

                    KLIPS_PRINT(debug_rcv & DB_RX_IPAD,
                            "klips_debug:ipsec_rcv: "
                            "padlen=%d, contents: 0x<offset>: 0x<value> 0x<value> ...\n",
                            padlen);

                    for (i = 1; i <= padlen; i++) {
                        if((i % 16) == 1) {
                            KLIPS_PRINTMORE_START(debug_rcv & DB_RX_IPAD,
                                    "klips_debug:           %02x:",
                                    i - 1);
                        }
                        KLIPS_PRINTMORE(debug_rcv & DB_RX_IPAD,
                                " %02x",
                                skb->data[pRcvDesc->icv_offset - 2 - padlen + i -1]);

                        if(i != skb->data[pRcvDesc->icv_offset - 2 - padlen + i -1]) {
                                badpad = 1;
                        }
                        if((i % 16) == 0) {
                            KLIPS_PRINTMORE_FINISH(debug_rcv & DB_RX_IPAD);
                        }
                    }
                    if((i % 16) != 1) {
                        KLIPS_PRINTMORE_FINISH(debug_rcv & DB_RX_IPAD);
                    }
                    if(badpad) {
                        KLIPS_PRINT(debug_rcv & DB_RX_IPAD,
                                "klips_debug:ipsec_rcv: "
                                "warning, decrypted packet from %s has bad padding\n",
                                ipaddr_txt);
                        KLIPS_PRINT(debug_rcv & DB_RX_IPAD,
                                "klips_debug:ipsec_rcv: "
                                "...may be bad decryption -- not dropped\n");
                        spin_lock(&tdb_lock);
                        (pRcvDesc->tdbp)->tdb_encpad_errs += 1;
                        spin_unlock(&tdb_lock);
                    }

                    KLIPS_PRINT(debug_rcv & DB_RX_IPAD,
                            "klips_debug:ipsec_rcv: "
                            "packet decrypted: next_header = %d, padding = %d\n",
                            next_header,
                            pad - 2 - (len - pRcvDesc->icv_offset));
                }

                /* Discard ESP header */
                ipp->tot_len = htons(ntohs(ipp->tot_len) - (esphlen + pad));
                memmove((void *)(skb->data + esphlen),
                    (void *)(skb->data), iphlen);
                if(skb->len < esphlen) {
                    PRINTK_REJECT(&pkt_info, KERN_WARNING,
                        "klips_error: "
                        "tried to skb_pull esphlen=%d, %d available.  This should never happen, please report.\n",
                        esphlen, (int)(skb->len));
                    spin_lock (&tdb_lock);
					tdbp->ips_req_done_count++;
					spin_unlock (&tdb_lock);
                    goto rcvleave_cb;
                }
                skb_pull(skb, esphlen);

                KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
                        "klips_debug:ipsec_rcv: "
                        "trimming to %d.\n",
                        len - esphlen - pad);
                if(pad + esphlen <= len) {
                        skb_trim(skb, len - esphlen - pad);
                } else {
                    KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
                            "klips_debug:ipsec_rcv: "
                            "bogus packet, size is zero or negative, dropping.\n");
                    spin_lock (&tdb_lock);
					tdbp->ips_req_done_count++;
					spin_unlock (&tdb_lock);
                    goto rcvleave_cb;
                }
                break;
            case IPPROTO_AH:
                /* Restore original IP header */
                ipp->frag_off = pRcvDesc->ip_frag_off;
                ipp->ttl = pRcvDesc->ip_ttl;

                ahp = (struct ah *) (skb->data + iphlen);
                /* get AH header len */
                ahhlen = (ahp->ah_hl << 2) +
				    ((caddr_t)&(ahp->ah_rpl) - (caddr_t)ahp);
                next_header = ahp->ah_nh;

                /* DIscard AH header */
                ipp->tot_len = htons(ntohs(ipp->tot_len) - ahhlen);
                memmove((void *)(skb->data + ahhlen),
                    (void *)(skb->data), iphlen);
                if(skb->len < ahhlen) {
                    ipsec_log(KERN_WARNING
                        "klips_error:ipsec_rcv: "
                        "tried to skb_pull ahhlen=%d, %d available.  This should never happen, please report.\n",
                        ahhlen,
                        (int)(skb->len));
                    spin_lock (&tdb_lock);
					tdbp->ips_req_done_count++;
					spin_unlock (&tdb_lock);
                    goto rcvleave_cb;
                }
                skb_pull(skb, ahhlen);
                break;
    }

    /* set next header */
    skb->data[PROTO] = next_header; /* Update next header protocol into IP header */

    /*
    *	Adjust pointers
    */

    len = skb->len;
    dat = skb->data;

#ifdef NET_21
/*		skb->h.ipiph=(struct iphdr *)skb->data; */
    skb->nh.raw = skb->data;
    skb->h.raw = skb->nh.raw + (skb->nh.iph->ihl << 2);

    memset(&(IPCB(skb)->opt), 0, sizeof(struct ip_options));
#else /* NET_21 */
    skb->h.iph=(struct iphdr *)skb->data;
    skb->ip_hdr=(struct iphdr *)skb->data;
    memset(skb->proto_priv, 0, sizeof(struct options));
#endif /* NET_21 */

    ipp = (struct iphdr *)dat;
    ipp->check = 0;
    ipp->check = ip_fast_csum((unsigned char *)dat, iphlen >> 2);

    KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

    skb->protocol = htons(ETH_P_IP);
    skb->ip_summed = 0;

    tdbprev = tdbp;
	tdbnext = tdbp->tdb_inext;
	
    if(sysctl_ipsec_inbound_policy_check) {
	    if(tdbnext) {
            if(tdbnext->tdb_onext != tdbp) {
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                spin_lock (&tdb_lock);
				tdbp->ips_req_done_count++;
				spin_unlock (&tdb_lock);
                goto rcvleave_cb;
            }

            if( ipp->protocol != IPPROTO_AH
                && ipp->protocol != IPPROTO_ESP
                && ipp->protocol != IPPROTO_COMP
                && (tdbnext->tdb_said.proto != IPPROTO_COMP
                    || (tdbnext->tdb_said.proto == IPPROTO_COMP
                    && tdbnext->tdb_inext))
                && ipp->protocol != IPPROTO_IPIP
                ) {
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                spin_lock (&tdb_lock);
				tdbp->ips_req_done_count++;
				spin_unlock (&tdb_lock);
                goto rcvleave_cb;
            }
        }
    }

    /* lock TDB lock */
    spin_lock(&tdb_lock);

    /* update ipcomp ratio counters, even if no ipcomp packet is present */
    if (tdbnext
    && tdbnext->tdb_said.proto == IPPROTO_COMP
    && ipp->protocol != IPPROTO_COMP) {
        tdbnext->tdb_comp_ratio_cbytes += ntohs(ipp->tot_len);
        tdbnext->tdb_comp_ratio_dbytes += ntohs(ipp->tot_len);
    }

    tdbp->ips_life.ipl_bytes.ipl_count += len;
    tdbp->ips_life.ipl_bytes.ipl_last   = len;

    if(!tdbp->ips_life.ipl_usetime.ipl_count) {
        tdbp->ips_life.ipl_usetime.ipl_count = jiffies / HZ;
    }
    tdbp->ips_life.ipl_usetime.ipl_last = jiffies / HZ;
    tdbp->ips_life.ipl_packets.ipl_count += 1;

    tdbp->ips_req_done_count++;
    spin_unlock(&tdb_lock);

    /* begin decapsulating loop here */
    while(   (ipp->protocol == IPPROTO_ESP )
		|| (ipp->protocol == IPPROTO_AH  )
		|| (ipp->protocol == IPPROTO_COMP)
		)
    {
		authlen = 0;
		espp = NULL;
		esphlen = 0;
		ahp = NULL;
		ahhlen = 0;
		compp = NULL;
		auth_data_len = 0;

		len = skb->len;
		dat = skb->data;
		ipp = (struct iphdr *)skb->data;
		proto = ipp->protocol;
		ipaddr.s_addr = ipp->saddr;
		addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));
		iphlen = ipp->ihl << 2;
		ipp->check = 0;			/* we know the sum is good */

#ifdef CONFIG_IPSEC_DEBUG
		pkt_info.has_spi_seq = 0;
		pkt_info.proto = ipp->protocol;
		pkt_info.src.s_addr = ipp->saddr;
		pkt_info.dst.s_addr = ipp->daddr;
#endif

		/*
		 * Find tunnel control block and (indirectly) call the
		 * appropriate tranform routine. The resulting sk_buf
		 * is a valid IP packet ready to go through input processing.
		 */

		said.dst.s_addr = ipp->daddr;
		switch(proto) {
		case IPPROTO_ESP:
    		/* XXX this will need to be 8 for IPv6 */
    		if ((len - iphlen) % 4) {
    			PRINTK_REJECT(&pkt_info, "", "klips_error: "
    			       "got packet with content length = %d from %s -- should be on 4 octet boundary, packet dropped\n",
    			       len - iphlen,
    			       ipaddr_txt);
    			if(pRcvDesc->stats) {
    				(pRcvDesc->stats)->rx_errors++;
    			}
    			goto rcvleave_cb;
    		}

			if(skb->len < (pRcvDesc->hard_header_len + sizeof(struct iphdr) + sizeof(struct esp))) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "runt esp packet of skb->len=%d received from %s, dropped.\n",
					    skb->len,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave_cb;
			}

			espp = (struct esp *)(skb->data + iphlen);
#ifdef CONFIG_IPSEC_DEBUG
			pkt_info.has_spi_seq = 1;
			pkt_info.spi = espp->esp_spi;
			pkt_info.seq = espp->esp_rpl;
#endif
			said.spi = espp->esp_spi;
			replay = ntohl(espp->esp_rpl);

			break;
		case IPPROTO_AH:
			if((skb->len
			    < (pRcvDesc->hard_header_len + sizeof(struct iphdr) + sizeof(struct ah)))
			   || (skb->len
			       < (pRcvDesc->hard_header_len + sizeof(struct iphdr)
				  + ((ahp = (struct ah *) (skb->data + iphlen))->ah_hl << 2)))) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "runt ah packet of skb->len=%d received from %s, dropped.\n",
					    skb->len,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave_cb;
			}
#ifdef CONFIG_IPSEC_DEBUG
			pkt_info.has_spi_seq = 1;
			pkt_info.spi = ahp->ah_spi;
			pkt_info.seq = ahp->ah_rpl;
#endif
			said.spi = ahp->ah_spi;
			replay = ntohl(ahp->ah_rpl);
			ahhlen = (ahp->ah_hl << 2) +
				((caddr_t)&(ahp->ah_rpl) - (caddr_t)ahp);
			next_header = ahp->ah_nh;
			if (ahhlen != sizeof(struct ah)) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "bad authenticator length %d, expected %d from %s.\n",
					    ahhlen - ((caddr_t)(ahp->ah_data) - (caddr_t)ahp),
					    AHHMAC_HASHLEN,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave_cb;
			}
			break;
		case IPPROTO_COMP:
			if(skb->len < (pRcvDesc->hard_header_len + sizeof(struct iphdr) + sizeof(struct ipcomphdr))) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "runt comp packet of skb->len=%d received from %s, dropped.\n",
					    skb->len,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave_cb;
			}

			compp = (struct ipcomphdr *)(skb->data + iphlen);
			said.spi = htonl((__u32)ntohs(compp->ipcomp_cpi));
			break;
		default:
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_errors++;
			}
			KLIPS_REJECT_INFO(&pkt_info, "unknown protocol");
			goto rcvleave_cb;
		}
		said.proto = proto;

		sa_len = satoa(said, 0, sa, SATOA_BUF);
		if(sa_len == 0) {
		  strcpy(sa, "(error)");
		}

		if (proto == IPPROTO_COMP) {
			unsigned int flags = 0;

			if (tdbp == NULL) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
					    "klips_debug: "
					    "Incoming packet with outer IPCOMP header SA:%s: not yet supported by KLIPS, dropped\n",
					    sa_len ? sa : " (error)");
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_dropped++;
				}

				goto rcvleave_cb;
			}

			tdbprev = tdbp;
			spin_lock(&tdb_lock);
			tdbp->ips_req_done_count++;
			tdbp = tdbnext;

			/* store current tdbp into rcv descriptor */
			pRcvDesc->tdbp = tdbp;

			if(sysctl_ipsec_inbound_policy_check
			   && ((tdbp == NULL)
			       || (((ntohl(tdbp->tdb_said.spi) & 0x0000ffff)
				    != ntohl(said.spi))
				/* next line is a workaround for peer
				   non-compliance with rfc2393 */
				   && (tdbp->tdb_encalg != ntohl(said.spi))
				       )))
			{

				char sa2[SATOA_BUF];
				size_t sa_len2 = 0;

				spin_unlock(&tdb_lock);

				if(tdbp) {
				    sa_len2 = satoa(tdbp->tdb_said, 0, sa2, SATOA_BUF);
				}

				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
					    "klips_debug: "
					    "Incoming packet with SA(IPCA):%s does not match policy SA(IPCA):%s cpi=%04x cpi->spi=%08x spi=%08x, spi->cpi=%04x for SA grouping, dropped.\n",
					    sa_len ? sa : " (error)",
					    tdbp ? (sa_len2 ? sa2 : " (error)") : "NULL",
					    ntohs(compp->ipcomp_cpi),
					    (__u32)ntohl(said.spi),
					    tdbp ? (__u32)ntohl((tdbp->tdb_said.spi)) : 0,
					    tdbp ? (__u16)(ntohl(tdbp->tdb_said.spi) & 0x0000ffff) : 0);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_dropped++;
				}
				goto rcvleave_cb;
			}

			next_header = compp->ipcomp_nh;

			if (tdbp) {
			    tdbp->ips_req_count++;
				tdbp->tdb_comp_ratio_cbytes += ntohs(ipp->tot_len);
				tdbnext = tdbp->tdb_inext;
			}

			skb = skb_decompress(skb, tdbp, &flags);
			if (!skb || flags) {
			    tdbp->ips_req_done_count++;
				spin_unlock(&tdb_lock);
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
					    "klips_debug: "
					    "skb_decompress() returned error flags=%x, dropped.\n",
					    flags);
				if (pRcvDesc->stats) {
				    if (flags)
					    (pRcvDesc->stats)->rx_errors++;
				    else
					    (pRcvDesc->stats)->rx_dropped++;
				}
				goto rcvleave_cb;
			}
#ifdef NET_21
			ipp = skb->nh.iph;
#else /* NET_21 */
			ipp = skb->ip_hdr;
#endif /* NET_21 */

			if (tdbp) {
				tdbp->tdb_comp_ratio_dbytes += ntohs(ipp->tot_len);
			}

			KLIPS_PRINT(debug_rcv,
				    "klips_debug: "
				    "packet decompressed SA(IPCA):%s cpi->spi=%08x spi=%08x, spi->cpi=%04x, nh=%d.\n",
				    sa_len ? sa : " (error)",
				    (__u32)ntohl(said.spi),
				    tdbp ? (__u32)ntohl((tdbp->tdb_said.spi)) : 0,
				    tdbp ? (__u16)(ntohl(tdbp->tdb_said.spi) & 0x0000ffff) : 0,
				    next_header);
			KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

			spin_unlock(&tdb_lock);

			continue;
			/* Skip rest of stuff and decapsulate next inner
			   packet, if any */
		}

		tdbp = ipsec_sa_getbyid(&said);
		pRcvDesc->tdbp = tdbp;

		if (tdbp == NULL) {
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "no Tunnel Descriptor Block for SA:%s: incoming packet with no SA dropped\n",
				    sa_len ? sa : " (error)");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		spin_lock(&tdb_lock);
		tdbp->ips_req_count++;
		if(sysctl_ipsec_inbound_policy_check) {
			if(ipp->saddr != ((struct sockaddr_in*)(tdbp->tdb_addr_s))->sin_addr.s_addr) {
				tdbp->ips_req_done_count++;
				spin_unlock(&tdb_lock);
				ipaddr.s_addr = ipp->saddr;
				addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));

				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
					    "klips_debug: "
					    "SA:%s, src=%s of pkt does not agree with expected SA source address policy.\n",
					    sa_len ? sa : " (error)",
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_dropped++;
				}
				goto rcvleave_cb;
			}

			ipaddr.s_addr = ipp->saddr;
			addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));
			KLIPS_PRINT(debug_rcv,
				    "klips_debug: "
				    "SA:%s, src=%s of pkt agrees with expected SA source address policy.\n",
				    sa_len ? sa : " (error)",
				    ipaddr_txt);
			if(tdbnext) {
				if(tdbnext != tdbp) {
					tdbp->ips_req_done_count++;
					spin_unlock(&tdb_lock);
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "unexpected SA:%s: does not agree with tdb->inext policy, dropped\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave_cb;
				}
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s grouping from previous SA is OK.\n",
					    sa_len ? sa : " (error)");
			} else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s First SA in group.\n",
					    sa_len ? sa : " (error)");
			}

			if(tdbp->tdb_onext) {
				if(tdbprev != tdbp->tdb_onext) {
				    tdbp->ips_req_done_count++;
					spin_unlock(&tdb_lock);
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "unexpected SA:%s: does not agree with tdb->onext policy, dropped.\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave_cb;
				} else {
					KLIPS_PRINT(debug_rcv,
						    "klips_debug:ipsec_rcv: "
						    "SA:%s grouping to previous SA is OK.\n",
						    sa_len ? sa : " (error)");
				}
			} else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s No previous backlink in group.\n",
					    sa_len ? sa : " (error)");
			}
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
			KLIPS_PRINT(debug_rcv,
				"klips_debug:ipsec_rcv: "
				"natt_type=%u tdbp->ips_natt_type=%u : %s\n",
				pRcvDesc->natt_type, tdbp->ips_natt_type,
				(pRcvDesc->natt_type==tdbp->ips_natt_type)?"ok":"bad");
			if (pRcvDesc->natt_type != tdbp->ips_natt_type) {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s does not agree with expected NAT-T policy.\n",
					    sa_len ? sa : " (error)");
				if(pRcvDesc->stats) {
					 pRcvDesc->stats->rx_dropped++;
				}
				goto rcvleave_cb;
			}
#endif		 
		}

		/* If it is in larval state, drop the packet, we cannot process yet. */
		if(tdbp->tdb_state == SADB_SASTATE_LARVAL) {
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "TDB in larval state, cannot be used yet, dropping packet.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		if(tdbp->tdb_state == SADB_SASTATE_DEAD) {
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "TDB in dead state, cannot be used any more, dropping packet.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		if(ipsec_lifetime_check(&tdbp->ips_life.ipl_bytes,   lifename[lifename_idx], sa,
					ipsec_life_countbased, ipsec_incoming, tdbp) == ipsec_life_harddied ||
		   ipsec_lifetime_check(&tdbp->ips_life.ipl_addtime, lifename[++lifename_idx],sa,
					ipsec_life_timebased,  ipsec_incoming, tdbp) == ipsec_life_harddied ||
		   ipsec_lifetime_check(&tdbp->ips_life.ipl_addtime, lifename[++lifename_idx],sa,
					ipsec_life_timebased,  ipsec_incoming, tdbp) == ipsec_life_harddied ||
		   ipsec_lifetime_check(&tdbp->ips_life.ipl_packets, lifename[++lifename_idx],sa,
					ipsec_life_countbased, ipsec_incoming, tdbp) == ipsec_life_harddied)
		{
			tdbp->ips_req_done_count++;
			ipsec_sa_delchain(tdbp);
			spin_unlock(&tdb_lock);
			KLIPS_REJECT_INFO(&pkt_info,
			    "hard %s lifetime of SA:<%s%s%s> %s has been reached, SA expired",
			    lifename[lifename_idx], IPS_XFORM_NAME(tdbp), sa);
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		if (!ipsec_checkreplaywindow(tdbp, replay, ipaddr_txt,
		    err_msg, sizeof(err_msg)))
		{
			tdbp->tdb_replaywin_errs += 1;
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_REJECT_INFO(&pkt_info, "%s", err_msg);
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "encalg = %d, authalg = %d.\n",
			    tdbp->tdb_encalg,
			    tdbp->tdb_authalg);

		/* If the sequence number == 0, expire SA, it had rolled */
		if(tdbp->tdb_replaywin && !replay /* !tdbp->tdb_replaywin_lastseq */) {
			tdbp->ips_req_done_count++;
			ipsec_sa_delchain(tdbp);
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "replay window counter rolled, expiring SA.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		if (!ipsec_updatereplaywindow(tdbp, replay)) {
			tdbp->tdb_replaywin_errs += 1;
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_REPLAY,
				    "klips_debug: "
				    "duplicate frame from %s, packet dropped\n",
				    ipaddr_txt);
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

		spin_unlock(&tdb_lock);


		switch(tdbp->tdb_authalg) {
		case AH_MD5:
			authlen = AHHMAC_HASHLEN;
			break;
		case AH_SHA:
			authlen = AHHMAC_HASHLEN;
			break;
		case AH_NONE:
			authlen = 0;
			break;
		default:
		    spin_lock(&tdb_lock);
		    tdbp->ips_req_done_count++;
			tdbp->tdb_alg_errs += 1;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "runt AH packet with no data, dropping.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_errors++;
			}
			goto rcvleave_cb;
		}

		KLIPS_PRINT(proto == IPPROTO_ESP && debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "packet from %s received with seq=%d (iv)=0x%08x%08x iplen=%d esplen=%d sa=%s\n",
			    ipaddr_txt,
			    (__u32)ntohl(espp->esp_rpl),
			    (__u32)ntohl(*((__u32 *)(espp->esp_iv)    )),
			    (__u32)ntohl(*((__u32 *)(espp->esp_iv) + 1)),
			    len,
			    auth_data_len,
			    sa_len ? sa : " (error)");

		switch(proto) {
		case IPPROTO_ESP:
/*
                 AFTER APPLYING ESP
            -------------------------------------------------
      IPv4  |orig IP hdr  | ESP |     |      |   ESP   | ESP|
            |(any options)| Hdr | TCP | Data | Trailer |Auth|
            -------------------------------------------------
                                |<----- encrypted ---->|
                          |<------ authenticated ----->|
*/

            switch(tdbp->tdb_encalg) {
                case ESP_3DES:
#ifdef USE_SINGLE_DES
                case ESP_DES:
#endif /* USE_SINGLE_DES */
                    memcpy (iv, espp->esp_iv, EMT_ESPDES_IV_SZ);
		    esphlen = ESP_HEADER_LEN + EMT_ESPDES_IV_SZ;
                    break;
		case ESP_AES:
		    memcpy(iv, espp->esp_iv, EMT_ESPAES_IV_SZ);
		    esphlen = ESP_HEADER_LEN + EMT_ESPAES_IV_SZ;
		    break;
                case ESP_NULL:
		    esphlen = offsetof(struct esp, esp_iv);
		    break;
                default:
                    spin_lock(&tdb_lock);
                    tdbp->ips_req_done_count++;
                    tdbp->tdb_alg_errs += 1;
                    spin_unlock(&tdb_lock);
		    KLIPS_REJECT_INFO(&pkt_info,
			"unsupported encryption protocol");
                    if(pRcvDesc->stats) {
                        (pRcvDesc->stats)->rx_errors++;
                    }
                    goto rcvleave_cb;
            }

            auth_start_offset = iphlen;
            auth_data_len = len - iphlen - authlen;
            icv_offset = len - authlen;
            crypt_start_offset = iphlen + esphlen;
            crypt_data_len = len - iphlen - authlen - esphlen;

            if ((crypt_data_len) % 8) {
                    spin_lock(&tdb_lock);
                    tdbp->ips_req_done_count++;
					tdbp->tdb_encsize_errs += 1;
					spin_unlock(&tdb_lock);
					KLIPS_REJECT_INFO(&pkt_info, "klips_error: "
					    "got packet with esplen = %d from %s "
					    "-- should be on 8 octet boundary, packet dropped\n",
					    crypt_data_len, ipaddr_txt);
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_errors++;
					}
					goto rcvleave_cb;
	    }
            break;
		case IPPROTO_AH:
/*
                  AFTER APPLYING AH
            ---------------------------------
      IPv4  |orig IP hdr  |    |     |      |
            |(any options)| AH | TCP | Data |
            ---------------------------------
            |<------- authenticated ------->|
                 except for mutable fields
*/

            auth_start_offset = 0; /* start at the beginning */
            auth_data_len = len;
            icv_offset = iphlen + AUTH_DATA_IN_AH_OFFSET;

            /* IXP425 glue code : mutable field, need to keep a copy of original IP header and
               restore the original IP header after callback received.
               Modify the mutable fields in header*/
            pRcvDesc->ip_frag_off = ipp->frag_off;
            pRcvDesc->ip_ttl = ipp->ttl;
            ipp->frag_off = 0;
            ipp->ttl = 0;
            ipp->check = 0;
			break;
		}

		if(auth_data_len <= 0) {
		    spin_lock (&tdb_lock);
			tdbp->ips_req_done_count++;
			spin_unlock (&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "runt AH packet with no data, dropping.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave_cb;
		}

        /* IXP425 glue code */
        if ((proto == IPPROTO_AH) || (proto == IPPROTO_ESP))
        {
            /* store ICV_offset */
            pRcvDesc->icv_offset = icv_offset;

            /* get mbuf */
            if(0 != ipsec_glue_mbuf_header_get(&src_mbuf))
            {
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                        "klips_debug: "
                        "running out of mbufs, dropped\n");
                spin_lock (&tdb_lock);
    			tdbp->ips_req_done_count++;
    			spin_unlock (&tdb_lock);
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                goto rcvleave_cb;
            }

            /* attach mbuf to sk_buff */
            mbuf_swap_skb(src_mbuf, skb);

            /* store rcv desc in mbuf */
            (IpsecRcvDesc *) IX_MBUF_NEXT_PKT_IN_CHAIN_PTR (src_mbuf) = pRcvDesc;

            /* call crypto perform */
            if (IX_CRYPTO_ACC_STATUS_SUCCESS != ipsec_hwaccel_perform (
                            tdbp->ips_crypto_context_id,
                            src_mbuf,
                            NULL,
                            auth_start_offset,
                            auth_data_len,
                            crypt_start_offset,
                            crypt_data_len,
                            icv_offset,
                            iv))
            {
                spin_lock(&tdb_lock);
                tdbp->ips_req_done_count++;
                spin_unlock(&tdb_lock);
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                        "klips_debug: "
                        "warning, decrapsulation packet from %s cannot be started\n",
                        ipaddr_txt);

                ipsec_glue_mbuf_header_rel(src_mbuf);

                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                goto rcvleave_cb;
            }
            return;
        } /* end of if ((proto == IPPROTO_AH) || (proto == IPPROTO_ESP))*/

	/* set next header */
	skb->data[PROTO] = next_header;

		/*
		 *	Adjust pointers
		 */

		len = skb->len;
		dat = skb->data;

#ifdef NET_21
/*		skb->h.ipiph=(struct iphdr *)skb->data; */
		skb->nh.raw = skb->data;
		skb->h.raw = skb->nh.raw + (skb->nh.iph->ihl << 2);

		memset(&(IPCB(skb)->opt), 0, sizeof(struct ip_options));
#else /* NET_21 */
		skb->h.iph=(struct iphdr *)skb->data;
		skb->ip_hdr=(struct iphdr *)skb->data;
		memset(skb->proto_priv, 0, sizeof(struct options));
#endif /* NET_21 */

		ipp = (struct iphdr *)dat;
		ipp->check = 0;
		ipp->check = ip_fast_csum((unsigned char *)dat, iphlen >> 2);

		KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
			    "klips_debug:ipsec_rcv: "
			    "after <%s%s%s>, SA:%s:\n",
			    IPS_XFORM_NAME(tdbp),
			    sa_len ? sa : " (error)");
		KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

		skb->protocol = htons(ETH_P_IP);
		skb->ip_summed = 0;

		tdbprev = tdbp;
		tdbnext = tdbp->tdb_inext;

		spin_lock (&tdb_lock);

		if(sysctl_ipsec_inbound_policy_check) {
			if(tdbnext) {
				if(tdbnext->tdb_onext != tdbp) {
					tdbp->ips_req_done_count++;
					spin_unlock(&tdb_lock);
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "SA:%s, backpolicy does not agree with fwdpolicy.\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave_cb;
				}
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s, backpolicy agrees with fwdpolicy.\n",
					    sa_len ? sa : " (error)");
				if(
					ipp->protocol != IPPROTO_COMP
					&& (tdbnext->tdb_said.proto != IPPROTO_COMP
					    || (tdbnext->tdb_said.proto == IPPROTO_COMP
						&& tdbnext->tdb_inext))
					&& ipp->protocol != IPPROTO_IPIP
					) {
					tdbp->ips_req_done_count++;
					spin_unlock(&tdb_lock);
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "packet with incomplete policy dropped, last successful SA:%s.\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave_cb;
				}
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s, Another IPSEC header to process.\n",
					    sa_len ? sa : " (error)");
			} else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "No tdb_inext from this SA:%s.\n",
					    sa_len ? sa : " (error)");
			} /* end of if(tdbnext)*/
		} /* end of if(sysctl_ipsec_inbound_policy_check) */

		/* update ipcomp ratio counters, even if no ipcomp packet is present */
		if (tdbnext
		  && tdbnext->tdb_said.proto == IPPROTO_COMP
		  && ipp->protocol != IPPROTO_COMP) {
			tdbnext->tdb_comp_ratio_cbytes += ntohs(ipp->tot_len);
			tdbnext->tdb_comp_ratio_dbytes += ntohs(ipp->tot_len);
		}

		tdbp->ips_life.ipl_bytes.ipl_count += len;
		tdbp->ips_life.ipl_bytes.ipl_last   = len;

		if(!tdbp->ips_life.ipl_usetime.ipl_count) {
			tdbp->ips_life.ipl_usetime.ipl_count = jiffies / HZ;
		}
		tdbp->ips_life.ipl_usetime.ipl_last = jiffies / HZ;
		tdbp->ips_life.ipl_packets.ipl_count += 1;
		tdbp->ips_req_done_count++;
		spin_unlock(&tdb_lock);

	} /* end decapsulation loop here */

        spin_lock(&tdb_lock);
        tdbp->ips_req_count++;

        if(tdbnext && tdbnext->tdb_said.proto == IPPROTO_COMP) {

            tdbprev = tdbp;
            tdbp->ips_req_done_count++;
            tdbp = tdbnext;
            pRcvDesc->tdbp = tdbp;
            tdbp->ips_req_count++;
            tdbnext = tdbp->tdb_inext;
        }

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
	if ((pRcvDesc->natt_type) && (ipp->protocol != IPPROTO_IPIP)) {
	    ipsec_rcv_natt_correct_tcp_udp_csum(skb, ipp, tdbp);
	}
#endif

        /*
        * XXX this needs to be locked from when it was first looked
        * up in the decapsulation loop.  Perhaps it is better to put
        * the IPIP decap inside the loop.
        */
        if(tdbnext) {
            tdbp->ips_req_done_count++;
            tdbp = tdbnext;
            tdbp->ips_req_count++;
            pRcvDesc->tdbp = tdbp;


            sa_len = satoa(tdbp->tdb_said, 0, sa, SATOA_BUF);
            if(ipp->protocol != IPPROTO_IPIP) {
                tdbp->ips_req_done_count++;
                spin_unlock(&tdb_lock);
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                        "klips_debug: "
                        "SA:%s, Hey!  How did this get through?  Dropped.\n",
                        sa_len ? sa : " (error)");
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                goto rcvleave_cb;
            }
            if(sysctl_ipsec_inbound_policy_check) {
                tdbnext = tdbp->tdb_inext;
                if(tdbnext) {
                    char sa2[SATOA_BUF];
                    size_t sa_len2;
                    sa_len2 = satoa(tdbnext->tdb_said, 0, sa2, SATOA_BUF);
                    tdbp->ips_req_done_count++;
                    spin_unlock(&tdb_lock);
                    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                            "klips_debug: "
                            "unexpected SA:%s after IPIP SA:%s\n",
                            sa_len2 ? sa2 : " (error)",
                            sa_len ? sa : " (error)");
                    if(pRcvDesc->stats) {
                        (pRcvDesc->stats)->rx_dropped++;
                    }
                    goto rcvleave_cb;
                }
                if(ipp->saddr != ((struct sockaddr_in*)(tdbp->tdb_addr_s))->sin_addr.s_addr) {
                    tdbp->ips_req_done_count++; 
                    spin_unlock(&tdb_lock); 
                    ipaddr.s_addr = ipp->saddr;
                    addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));
                    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                            "klips_debug: "
                            "SA:%s, src=%s of pkt does not agree with expected SA source address policy.\n",
                            sa_len ? sa : " (error)",
                            ipaddr_txt);
                    if(pRcvDesc->stats) {
                        (pRcvDesc->stats)->rx_dropped++;
                    }
                    goto rcvleave_cb;
                }
            } /* end of if(sysctl_ipsec_inbound_policy_check) */

            /*
            * XXX this needs to be locked from when it was first looked
            * up in the decapsulation loop.  Perhaps it is better to put
            * the IPIP decap inside the loop.
            */
            tdbp->ips_life.ipl_bytes.ipl_count += len;
            tdbp->ips_life.ipl_bytes.ipl_last   = len;

            if(!tdbp->ips_life.ipl_usetime.ipl_count) {
                tdbp->ips_life.ipl_usetime.ipl_count = jiffies / HZ;
            }
            tdbp->ips_life.ipl_usetime.ipl_last = jiffies / HZ;
            tdbp->ips_life.ipl_packets.ipl_count += 1;

            if(skb->len < iphlen) {
                PRINTK_REJECT(&pkt_info, KERN_WARNING, "klips_debug: "
                    "tried to skb_pull iphlen=%d, %d available.  This should never happen, please report.\n",
                    iphlen,
                    (int)(skb->len));

                tdbp->ips_req_done_count++;
                spin_unlock (&tdb_lock);
                goto rcvleave_cb;
            }
            skb_pull(skb, iphlen);

#ifdef NET_21
            ipp = (struct iphdr *)skb->nh.raw = skb->data;
            skb->h.raw = skb->nh.raw + (skb->nh.iph->ihl << 2);

            memset(&(IPCB(skb)->opt), 0, sizeof(struct ip_options));
#else /* NET_21 */
            ipp = skb->ip_hdr = skb->h.iph = (struct iphdr *)skb->data;

            memset(skb->proto_priv, 0, sizeof(struct options));
#endif /* NET_21 */

            skb->protocol = htons(ETH_P_IP);
            skb->ip_summed = 0;
            KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
                    "klips_debug:ipsec_rcv: "
                    "IPIP tunnel stripped.\n");
            KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

            if(sysctl_ipsec_inbound_policy_check)
	    {
		u32 s_addr, d_addr, tdb_s_ip, tdb_d_ip, tdb_s_to, tdb_d_to;

		s_addr = ntohl(ipp->saddr);
		tdb_s_ip = ntohl(tdbp->tdb_flow_s.u.v4.sin_addr.s_addr);
		tdb_s_to = ntohl(tdbp->tdb_to_s.u.v4.sin_addr.s_addr);
		d_addr = ntohl(ipp->daddr);
		tdb_d_ip = ntohl(tdbp->tdb_flow_d.u.v4.sin_addr.s_addr);
		tdb_d_to = ntohl(tdbp->tdb_to_d.u.v4.sin_addr.s_addr);
		if (!IS_MATCH(s_addr, tdb_s_ip,
		    ntohl(tdbp->tdb_mask_s.u.v4.sin_addr.s_addr),
		    tdb_s_to) ||
		    !IS_MATCH(d_addr, tdb_d_ip,
		    ntohl(tdbp->tdb_mask_d.u.v4.sin_addr.s_addr), tdb_d_to))
		{
		    struct in_addr daddr, saddr;
		    char saddr_txt[ADDRTOA_BUF], daddr_txt[ADDRTOA_BUF];
		    char sflow_txt[SUBNETTOA_BUF], dflow_txt[SUBNETTOA_BUF];

		    subnettoa(tdbp->tdb_flow_s.u.v4.sin_addr,
			tdbp->tdb_mask_s.u.v4.sin_addr,
			0, sflow_txt, sizeof(sflow_txt));
		    subnettoa(tdbp->tdb_flow_d.u.v4.sin_addr,
			tdbp->tdb_mask_d.u.v4.sin_addr,
			0, dflow_txt, sizeof(dflow_txt));

		    saddr.s_addr = ipp->saddr;
		    daddr.s_addr = ipp->daddr;
		    addrtoa(saddr, 0, saddr_txt, sizeof(saddr_txt));
		    addrtoa(daddr, 0, daddr_txt, sizeof(daddr_txt));
		    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
			    "klips_debug: "
			    "SA:%s, inner tunnel policy [%s -> %s] does not agree with pkt contents [%s -> %s].\n",
			    sa_len ? sa : " (error)",
			    sflow_txt,
			    dflow_txt,
			    saddr_txt,
			    daddr_txt);
		    if(pRcvDesc->stats) {
			(pRcvDesc->stats)->rx_dropped++;
		    }
		    tdbp->ips_req_done_count++;
		    spin_unlock (&tdb_lock);
		    goto rcvleave_cb;
		}
	    }
        } /* end of if(tdbnext) */
        
        tdbp->ips_req_done_count++;
        spin_unlock(&tdb_lock);

#ifdef NET_21
        if(pRcvDesc->stats) {
            (pRcvDesc->stats)->rx_bytes += skb->len;
        }
        if(skb->dst) {
            dst_release(skb->dst);
            skb->dst = NULL;
        }
        skb->pkt_type = PACKET_HOST;
        if(pRcvDesc->hard_header_len &&
        (skb->mac.raw != (skb->data - pRcvDesc->hard_header_len)) &&
        (pRcvDesc->hard_header_len <= skb_headroom(skb))) {
            /* copy back original MAC header */
            memmove(skb->data - pRcvDesc->hard_header_len, skb->mac.raw, pRcvDesc->hard_header_len);
            skb->mac.raw = skb->data - pRcvDesc->hard_header_len;
        }
#endif /* NET_21 */

        if(ipp->protocol == IPPROTO_COMP) {
            unsigned int flags = 0;

            if(sysctl_ipsec_inbound_policy_check) {
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_PKTRX,
                    "klips_debug: "
                    "inbound policy checking enabled, IPCOMP follows IPIP, dropped.\n");
                if (pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_errors++;
                }
                goto rcvleave_cb;
            }
            /*
            XXX need a TDB for updating ratio counters but it is not
            following policy anyways so it is not a priority
            */
            skb = skb_decompress(skb, NULL, &flags);
            if (!skb || flags) {
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_PKTRX,
                    "klips_debug: "
                    "skb_decompress() returned error flags: %d, dropped.\n",
                    flags);
                if (pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_errors++;
                }
                goto rcvleave_cb;
            }
        }

#ifdef SKB_RESET_NFCT
            nf_conntrack_put(skb->nfct);
            skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	    skb->nf_debug = 0;
#endif /* CONFIG_NETFILTER_DEBUG */
#endif /* SKB_RESET_NFCT */

        KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
                "klips_debug:ipsec_rcv: "
                "netif_rx() called.\n");
        netif_rx(skb);

        /* release desc */
        if (pRcvDesc)
            ipsec_glue_rcv_desc_release (pRcvDesc);

        KLIPS_DEC_USE;
        return;

rcvleave_cb:
	/* release desc */
	if (pRcvDesc)
	    ipsec_glue_rcv_desc_release (pRcvDesc);

 	if(skb) {
	    ipsec_kfree_skb(skb);
	}

	KLIPS_DEC_USE;
	return;
}

static int ipsec_rcv_decap(struct ipsec_rcv_state *irs, IpsecRcvDesc *pRcvDesc)
{
	struct sk_buff *skb = irs->skb;
	struct iphdr *ipp;
	int authlen = 0;
	struct esp *espp = NULL;
	int esphlen = 0;
	__u32 iv[ESP_IV_MAXSZ_INT];
	struct ah *ahp = NULL;
	int ahhlen = 0;
	struct ipcomphdr*compp = NULL;

	int iphlen;
	unsigned char *dat;
	struct ipsec_sa *tdbp = NULL;
	struct sa_id said;
	char sa[SATOA_BUF];
	size_t sa_len;
	char ipaddr_txt[ADDRTOA_BUF];
	struct in_addr ipaddr;
	__u8 next_header = 0;
	__u8 proto;
#ifdef CONFIG_IPSEC_DEBUG
	struct ipsec_pkt_info_t pkt_info;
#endif
	char err_msg[128];
	int lifename_idx = 0;
	char *lifename[] = {
	    "bytes",
	    "addtime",
	    "usetime",
	    "packets"
	};

	int len;
	int replay = 0;	/* replay value in AH or ESP packet */
	struct ipsec_sa* tdbprev = NULL;	/* previous SA from outside of packet */
	struct ipsec_sa* tdbnext = NULL;	/* next SA towards inside of packet */
	__u32 auth_start_offset;
	__u32 auth_data_len;
	__u32 crypt_start_offset = 0;
	__u32 crypt_data_len = 0;
	__u32 icv_offset = 0;
	IX_MBUF *src_mbuf;

	/* begin decapsulating loop here */
        do
        {
		authlen = 0;
		espp = NULL;
		esphlen = 0;
		ahp = NULL;
		ahhlen = 0;
		compp = NULL;
		auth_data_len = 0;

		len = skb->len;
		dat = skb->data;
		ipp = (struct iphdr *)skb->data;
		proto = ipp->protocol;
		ipaddr.s_addr = ipp->saddr;
		addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));
		iphlen = ipp->ihl << 2;
		ipp->check = 0;			/* we know the sum is good */

#ifdef CONFIG_IPSEC_DEBUG
		pkt_info.has_spi_seq = 0;
		pkt_info.proto = proto;
		pkt_info.src.s_addr = ipp->saddr;
		pkt_info.dst.s_addr = ipp->daddr;
#endif

		/*
		 * Find tunnel control block and (indirectly) call the
		 * appropriate tranform routine. The resulting sk_buf
		 * is a valid IP packet ready to go through input processing.
		 */

		said.dst.s_addr = ipp->daddr;
		switch(proto) {
		case IPPROTO_ESP:
    		/* XXX this will need to be 8 for IPv6 */
    		if ((len - iphlen) % 4) {
    			PRINTK_REJECT(&pkt_info, "", "klips_error: "
    			       "got packet with content length = %d from %s -- should be on 4 octet boundary, packet dropped\n",
    			       len - iphlen,
    			       ipaddr_txt);
    			if(pRcvDesc->stats) {
    				(pRcvDesc->stats)->rx_errors++;
    			}
    			goto rcvleave;
    		}

			if(skb->len < (pRcvDesc->hard_header_len + sizeof(struct iphdr) + sizeof(struct esp))) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "runt esp packet of skb->len=%d received from %s, dropped.\n",
					    skb->len,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave;
			}

			espp = (struct esp *)(skb->data + iphlen);
#ifdef CONFIG_IPSEC_DEBUG
			pkt_info.has_spi_seq = 1;
			pkt_info.spi = espp->esp_spi;
			pkt_info.seq = espp->esp_rpl;
#endif
			said.spi = espp->esp_spi;
			replay = ntohl(espp->esp_rpl);

			break;
		case IPPROTO_AH:
			if((skb->len
			    < (pRcvDesc->hard_header_len + sizeof(struct iphdr) + sizeof(struct ah)))
			   || (skb->len
			       < (pRcvDesc->hard_header_len + sizeof(struct iphdr)
				  + ((ahp = (struct ah *) (skb->data + iphlen))->ah_hl << 2)))) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "runt ah packet of skb->len=%d received from %s, dropped.\n",
					    skb->len,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave;
			}
#ifdef CONFIG_IPSEC_DEBUG
			pkt_info.has_spi_seq = 1;
			pkt_info.spi = ahp->ah_spi;
			pkt_info.seq = ahp->ah_rpl;
#endif
			said.spi = ahp->ah_spi;
			replay = ntohl(ahp->ah_rpl);
			ahhlen = (ahp->ah_hl << 2) +
				((caddr_t)&(ahp->ah_rpl) - (caddr_t)ahp);
			next_header = ahp->ah_nh;
			if (ahhlen != sizeof(struct ah)) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "bad authenticator length %d, expected %d from %s.\n",
					    ahhlen - ((caddr_t)(ahp->ah_data) - (caddr_t)ahp),
					    AHHMAC_HASHLEN,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave;
			}
			break;
		case IPPROTO_COMP:
			if(skb->len < (pRcvDesc->hard_header_len + sizeof(struct iphdr) + sizeof(struct ipcomphdr))) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_INAU,
					    "klips_debug: "
					    "runt comp packet of skb->len=%d received from %s, dropped.\n",
					    skb->len,
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_errors++;
				}
				goto rcvleave;
			}

			compp = (struct ipcomphdr *)(skb->data + iphlen);
			said.spi = htonl((__u32)ntohs(compp->ipcomp_cpi));
			break;
		default:
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_errors++;
			}
			KLIPS_REJECT_INFO(&pkt_info, "unknown protocol");
			goto rcvleave;
		}
		said.proto = proto;

		sa_len = satoa(said, 0, sa, SATOA_BUF);
		if(sa_len == 0) {
		  strcpy(sa, "(error)");
		}
		
		if (proto == IPPROTO_COMP) {
			unsigned int flags = 0;

			if (tdbp == NULL) {
				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
					    "klips_debug: "
					    "Incoming packet with outer IPCOMP header SA:%s: not yet supported by KLIPS, dropped\n",
					    sa_len ? sa : " (error)");
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_dropped++;
				}

				goto rcvleave;
			}

			tdbprev = tdbp;
			spin_lock(&tdb_lock);
			tdbp->ips_req_done_count++;
			tdbp = tdbnext;

			/* store current tdbp into rcv descriptor */
			pRcvDesc->tdbp = tdbp;

			if(sysctl_ipsec_inbound_policy_check
			    && ((tdbp == NULL)
			    || (((ntohl(tdbp->tdb_said.spi) & 0x0000ffff)
			    != ntohl(said.spi))
			    /* next line is a workaround for peer
			       non-compliance with rfc2393 */
			    && (tdbp->tdb_encalg != ntohl(said.spi))
			    )))
			{

			    char sa2[SATOA_BUF];
			    size_t sa_len2 = 0;

			    spin_unlock(&tdb_lock);

			    if(tdbp) {
				sa_len2 = satoa(tdbp->tdb_said, 0, sa2, SATOA_BUF);
			    }

			    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				"klips_debug: "
				"Incoming packet with SA(IPCA):%s does not match policy SA(IPCA):%s cpi=%04x cpi->spi=%08x spi=%08x, spi->cpi=%04x for SA grouping, dropped.\n",
				sa_len ? sa : " (error)",
				tdbp ? (sa_len2 ? sa2 : " (error)") : "NULL",
				ntohs(compp->ipcomp_cpi),
				(__u32)ntohl(said.spi),
				tdbp ? (__u32)ntohl((tdbp->tdb_said.spi)) : 0,
				tdbp ? (__u16)(ntohl(tdbp->tdb_said.spi) & 0x0000ffff) : 0);
			    if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			    }
			    goto rcvleave;
			}

			next_header = compp->ipcomp_nh;

			if (tdbp) {
			    tdbp->ips_req_count++;
			    tdbp->tdb_comp_ratio_cbytes += ntohs(ipp->tot_len);
			    tdbnext = tdbp->tdb_inext;
			}

			skb = skb_decompress(skb, tdbp, &flags);
			if (!skb || flags) {
			    tdbp->ips_req_done_count++;
			    spin_unlock(&tdb_lock);
			    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				"klips_debug: "
				"skb_decompress() returned error flags=%x, dropped.\n",
				flags);
			    if (pRcvDesc->stats) {
				if (flags)
				    (pRcvDesc->stats)->rx_errors++;
				else
				    (pRcvDesc->stats)->rx_dropped++;
			    }
			    goto rcvleave;
			}
#ifdef NET_21
			ipp = skb->nh.iph;
#else /* NET_21 */
			ipp = skb->ip_hdr;
#endif /* NET_21 */

			if (tdbp) {
				tdbp->tdb_comp_ratio_dbytes += ntohs(ipp->tot_len);
			}

			KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "packet decompressed SA(IPCA):%s cpi->spi=%08x spi=%08x, spi->cpi=%04x, nh=%d.\n",
			    sa_len ? sa : " (error)",
			    (__u32)ntohl(said.spi),
			    tdbp ? (__u32)ntohl((tdbp->tdb_said.spi)) : 0,
			    tdbp ? (__u16)(ntohl(tdbp->tdb_said.spi) & 0x0000ffff) : 0,
			    next_header);
			KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

			spin_unlock(&tdb_lock);

			continue;
			/* Skip rest of stuff and decapsulate next inner
			   packet, if any */
		}

		tdbp = ipsec_sa_getbyid(&said);
		pRcvDesc->tdbp = tdbp;

		if (tdbp == NULL) {
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "no Tunnel Descriptor Block for SA:%s: incoming packet with no SA dropped\n",
				    sa_len ? sa : " (error)");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave;
		}

		spin_lock(&tdb_lock);
		tdbp->ips_req_count++;
		if(sysctl_ipsec_inbound_policy_check) {
			if(ipp->saddr != ((struct sockaddr_in*)(tdbp->tdb_addr_s))->sin_addr.s_addr) {
			    tdbp->ips_req_done_count++;
				spin_unlock(&tdb_lock); 
				ipaddr.s_addr = ipp->saddr;
				addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));

				KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
					    "klips_debug: "
					    "SA:%s, src=%s of pkt does not agree with expected SA source address policy.\n",
					    sa_len ? sa : " (error)",
					    ipaddr_txt);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_dropped++;
				}
				goto rcvleave;
			}

			ipaddr.s_addr = ipp->saddr;
			addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));
			KLIPS_PRINT(debug_rcv,
				    "klips_debug:ipsec_rcv: "
				    "SA:%s, src=%s of pkt agrees with expected SA source address policy.\n",
				    sa_len ? sa : " (error)",
				    ipaddr_txt);
			if(tdbnext) {
				if(tdbnext != tdbp) {
				    tdbp->ips_req_done_count++;
					spin_unlock(&tdb_lock); 
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "unexpected SA:%s: does not agree with tdb->inext policy, dropped\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave;
				}
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s grouping from previous SA is OK.\n",
					    sa_len ? sa : " (error)");
			} else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s First SA in group.\n",
					    sa_len ? sa : " (error)");
			}

			if(tdbp->tdb_onext) {
				if(tdbprev != tdbp->tdb_onext) {
				    tdbp->ips_req_done_count++;
					spin_unlock(&tdb_lock); 
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "unexpected SA:%s: does not agree with tdb->onext policy, dropped.\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave;
				} else {
					KLIPS_PRINT(debug_rcv,
						    "klips_debug:ipsec_rcv: "
						    "SA:%s grouping to previous SA is OK.\n",
						    sa_len ? sa : " (error)");
				}
			} else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s No previous backlink in group.\n",
					    sa_len ? sa : " (error)");
			}
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
			KLIPS_PRINT(debug_rcv,
				"klips_debug:ipsec_rcv: "
				"natt_type=%u tdbp->ips_natt_type=%u : %s\n",
				irs->natt_type, tdbp->ips_natt_type,
				(irs->natt_type==tdbp->ips_natt_type)?"ok":"bad");
			if (irs->natt_type != tdbp->ips_natt_type) {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s does not agree with expected NAT-T policy.\n",
					    sa_len ? sa : " (error)");
				if(pRcvDesc->stats) {
					 pRcvDesc->stats->rx_dropped++;
				}
				goto rcvleave;
			}
#endif		 
		}

		/* If it is in larval state, drop the packet, we cannot process yet. */
		if(tdbp->tdb_state == SADB_SASTATE_LARVAL) {
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "TDB in larval state, cannot be used yet, dropping packet.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave;
		}

		if(tdbp->tdb_state == SADB_SASTATE_DEAD) {
		        tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "TDB in dead state, cannot be used any more, dropping packet.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave;
		}

		if(ipsec_lifetime_check(&tdbp->ips_life.ipl_bytes,   lifename[lifename_idx], sa,
					ipsec_life_countbased, ipsec_incoming, tdbp) == ipsec_life_harddied ||
		   ipsec_lifetime_check(&tdbp->ips_life.ipl_addtime, lifename[++lifename_idx],sa,
					ipsec_life_timebased,  ipsec_incoming, tdbp) == ipsec_life_harddied ||
		   ipsec_lifetime_check(&tdbp->ips_life.ipl_addtime, lifename[++lifename_idx], sa,
					ipsec_life_timebased,  ipsec_incoming, tdbp) == ipsec_life_harddied ||
		   ipsec_lifetime_check(&tdbp->ips_life.ipl_packets, lifename[++lifename_idx],sa,
					ipsec_life_countbased, ipsec_incoming, tdbp) == ipsec_life_harddied)
		{
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			ipsec_sa_delchain(tdbp);
			KLIPS_REJECT_INFO(&pkt_info,
			    "hard %s lifetime of SA:<%s%s%s> %s has been reached, SA expired",
			    lifename[lifename_idx], IPS_XFORM_NAME(tdbp), sa);
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave;
		}

		if (!ipsec_checkreplaywindow(tdbp, replay, ipaddr_txt, err_msg, sizeof(err_msg))) {
		    tdbp->tdb_replaywin_errs += 1;
		    tdbp->ips_req_done_count++;
		    spin_unlock(&tdb_lock);
		    KLIPS_REJECT_INFO(&pkt_info, "%s", err_msg);
		    if(pRcvDesc->stats) {
			(pRcvDesc->stats)->rx_dropped++;
		    }
		    goto rcvleave;
		}

		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "encalg = %d, authalg = %d.\n",
			    tdbp->tdb_encalg,
			    tdbp->tdb_authalg);

		/* If the sequence number == 0, expire SA, it had rolled */
		if(tdbp->tdb_replaywin && !replay /* !tdbp->tdb_replaywin_lastseq */) {
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			ipsec_sa_delchain(tdbp);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
				    "klips_debug: "
				    "replay window counter rolled, expiring SA.\n");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave;
		}

		if (!ipsec_updatereplaywindow(tdbp, replay)) {
			tdbp->tdb_replaywin_errs += 1;
			tdbp->ips_req_done_count++;
			spin_unlock(&tdb_lock);
			KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_REPLAY,
				    "klips_debug: "
				    "duplicate frame from %s, packet dropped\n",
				    ipaddr_txt);
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_dropped++;
			}
			goto rcvleave;
		}

		spin_unlock(&tdb_lock);

		switch(tdbp->tdb_authalg) {
		case AH_MD5:
			authlen = AHHMAC_HASHLEN;
			break;
		case AH_SHA:
			authlen = AHHMAC_HASHLEN;
			break;
		case AH_NONE:
			authlen = 0;
			break;
		default:
		    spin_lock(&tdb_lock);
		    tdbp->ips_req_done_count++; 
			tdbp->tdb_alg_errs += 1;
			spin_unlock(&tdb_lock);
			KLIPS_REJECT_INFO(&pkt_info,
			    "unknown authentication type");
			if(pRcvDesc->stats) {
				(pRcvDesc->stats)->rx_errors++;
			}
			goto rcvleave;
		}

		KLIPS_PRINT(proto == IPPROTO_ESP && debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "packet from %s received with seq=%d (iv)=0x%08x%08x iplen=%d esplen=%d sa=%s\n",
			    ipaddr_txt,
			    (__u32)ntohl(espp->esp_rpl),
			    (__u32)ntohl(*((__u32 *)(espp->esp_iv)    )),
			    (__u32)ntohl(*((__u32 *)(espp->esp_iv) + 1)),
			    len,
			    auth_data_len,
			    sa_len ? sa : " (error)");

		switch(proto) {
		case IPPROTO_ESP:
/*
                 AFTER APPLYING ESP
            -------------------------------------------------
      IPv4  |orig IP hdr  | ESP |     |      |   ESP   | ESP|
            |(any options)| Hdr | TCP | Data | Trailer |Auth|
            -------------------------------------------------
                                |<----- encrypted ---->|
                          |<------ authenticated ----->|
*/

		    switch(tdbp->tdb_encalg) {

		    case ESP_3DES:
#ifdef USE_SINGLE_DES
		    case ESP_DES:
#endif /* USE_SINGLE_DES */
			memcpy (iv, espp->esp_iv, EMT_ESPDES_IV_SZ);
			esphlen = ESP_HEADER_LEN + EMT_ESPDES_IV_SZ;
			break;
		    case ESP_NULL:
			esphlen = offsetof(struct esp, esp_iv);
			break;
		    case ESP_AES:
			memcpy (iv, espp->esp_iv, EMT_ESPAES_IV_SZ);
			esphlen = ESP_HEADER_LEN + EMT_ESPAES_IV_SZ;
			break;
		    default:
			spin_lock(&tdb_lock);
			tdbp->ips_req_done_count++;
			tdbp->tdb_alg_errs += 1;
			spin_unlock(&tdb_lock);
			KLIPS_REJECT_INFO(&pkt_info,
			    "unsupported encryption protocol");
			if(pRcvDesc->stats) {
			    (pRcvDesc->stats)->rx_errors++;
			}
			goto rcvleave;
		    }

		    auth_start_offset = iphlen;
		    auth_data_len = len - iphlen - authlen;
		    icv_offset = len - authlen;
		    crypt_start_offset = iphlen + esphlen;
		    crypt_data_len = len - iphlen - authlen - esphlen;

		    if ((crypt_data_len) % 8) {
			spin_lock(&tdb_lock);
			tdbp->ips_req_done_count++; 
			tdbp->tdb_encsize_errs += 1;
			spin_unlock(&tdb_lock);
			KLIPS_REJECT_INFO(&pkt_info, "klips_error: "
			    "got packet with esplen = %d from %s "
			    "-- should be on 8 octet boundary, packet dropped\n",
			    crypt_data_len, ipaddr_txt);
			if(pRcvDesc->stats) {
			    (pRcvDesc->stats)->rx_errors++;
			}
			goto rcvleave;
		    }
		    break;
		case IPPROTO_AH:
/*
                  AFTER APPLYING AH
            ---------------------------------
      IPv4  |orig IP hdr  |    |     |      |
            |(any options)| AH | TCP | Data |
            ---------------------------------
            |<------- authenticated ------->|
                 except for mutable fields
*/

		    auth_start_offset = 0; /* start at the beginning */
		    auth_data_len = len;
		    icv_offset = iphlen + AUTH_DATA_IN_AH_OFFSET;

		    /* IXP425 glue code : mutable field, need to keep a copy of original IP header and
		       restore the original IP header after callback received.
		       Modify the mutable fields in header*/
		    pRcvDesc->ip_frag_off = ipp->frag_off;
		    pRcvDesc->ip_ttl = ipp->ttl;
		    ipp->frag_off = 0;
		    ipp->ttl = 0;
		    ipp->check = 0;
		    break;
		}

		if(auth_data_len <= 0) {
		    spin_lock (&tdb_lock);
		    tdbp->ips_req_done_count++; 
		    spin_unlock (&tdb_lock);
		    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
			"klips_debug: "
			"runt AH packet with no data, dropping.\n");
		    if(pRcvDesc->stats) {
			(pRcvDesc->stats)->rx_dropped++;
		    }
		    goto rcvleave;
		}

        /* IXP425 glue code */

        if ((proto == IPPROTO_AH) || (proto == IPPROTO_ESP))
        {
            /* store ICV_offset */
            pRcvDesc->icv_offset = icv_offset;

            /* get mbuf */
            if(0 != ipsec_glue_mbuf_header_get(&src_mbuf))
            {
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                        "klips_debug: "
                        "running out of mbufs, dropped\n");
                spin_lock (&tdb_lock);
    			tdbp->ips_req_done_count++; 
    			spin_unlock (&tdb_lock);
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                goto rcvleave;
            }

            /* attach mbuf to sk_buff */
            mbuf_swap_skb(src_mbuf, skb);

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
	    pRcvDesc->natt_type = irs->natt_type;
#endif
            /* store rcv desc in mbuf */
            (IpsecRcvDesc *) IX_MBUF_NEXT_PKT_IN_CHAIN_PTR (src_mbuf) = pRcvDesc;

            /* call crypto perform */
            if (IX_CRYPTO_ACC_STATUS_SUCCESS != ipsec_hwaccel_perform (
                            tdbp->ips_crypto_context_id,
                            src_mbuf,
                            NULL,
                            auth_start_offset,
                            auth_data_len,
                            crypt_start_offset,
                            crypt_data_len,
                            icv_offset,
                            iv))
            {
                spin_lock(&tdb_lock);
                tdbp->ips_req_done_count++; 
                spin_unlock(&tdb_lock); 
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                        "klips_debug: "
                        "warning, decrapsulation packet from %s cannot be started\n",
                        ipaddr_txt);

		        ipsec_glue_mbuf_header_rel(src_mbuf);

                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                goto rcvleave;
            }
            return 0;
        } /* end of if ((proto == IPPROTO_AH) || (proto == IPPROTO_ESP))*/

	        /* set next header */
	        skb->data[PROTO] = next_header;

		/*
		 *	Adjust pointers
		 */

		len = skb->len;
		dat = skb->data;

#ifdef NET_21
/*		skb->h.ipiph=(struct iphdr *)skb->data; */
		skb->nh.raw = skb->data;
		skb->h.raw = skb->nh.raw + (skb->nh.iph->ihl << 2);

		memset(&(IPCB(skb)->opt), 0, sizeof(struct ip_options));
#else /* NET_21 */
		skb->h.iph=(struct iphdr *)skb->data;
		skb->ip_hdr=(struct iphdr *)skb->data;
		memset(skb->proto_priv, 0, sizeof(struct options));
#endif /* NET_21 */

		ipp = (struct iphdr *)dat;
		ipp->check = 0;
		ipp->check = ip_fast_csum((unsigned char *)dat, iphlen >> 2);

		KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
			    "klips_debug:ipsec_rcv: "
			    "after <%s%s%s>, SA:%s:\n",
			    IPS_XFORM_NAME(tdbp),
			    sa_len ? sa : " (error)");
		KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

		skb->protocol = htons(ETH_P_IP);
		skb->ip_summed = 0;

		tdbprev = tdbp;
		tdbnext = tdbp->tdb_inext;

		spin_lock(&tdb_lock);

		if(sysctl_ipsec_inbound_policy_check) {
			if(tdbnext) {
				if(tdbnext->tdb_onext != tdbp) {
					tdbp->ips_req_done_count++; 
					spin_unlock(&tdb_lock); 
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "SA:%s, backpolicy does not agree with fwdpolicy.\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave;
				}
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s, backpolicy agrees with fwdpolicy.\n",
					    sa_len ? sa : " (error)");
				if(
					ipp->protocol != IPPROTO_COMP
					&& (tdbnext->tdb_said.proto != IPPROTO_COMP
					    || (tdbnext->tdb_said.proto == IPPROTO_COMP
						&& tdbnext->tdb_inext))
					&& ipp->protocol != IPPROTO_IPIP
					) {
					tdbp->ips_req_done_count++; 
					spin_unlock(&tdb_lock);
					KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
						    "klips_debug: "
						    "packet with incomplete policy dropped, last successful SA:%s.\n",
						    sa_len ? sa : " (error)");
					if(pRcvDesc->stats) {
						(pRcvDesc->stats)->rx_dropped++;
					}
					goto rcvleave;
				}
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "SA:%s, Another IPSEC header to process.\n",
					    sa_len ? sa : " (error)");
			} else {
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "No tdb_inext from this SA:%s.\n",
					    sa_len ? sa : " (error)");
			} /* end of if(tdbnext)*/
		} /* end of if(sysctl_ipsec_inbound_policy_check) */

		/* update ipcomp ratio counters, even if no ipcomp packet is present */
		if (tdbnext
		  && tdbnext->tdb_said.proto == IPPROTO_COMP
		  && ipp->protocol != IPPROTO_COMP) {
			tdbnext->tdb_comp_ratio_cbytes += ntohs(ipp->tot_len);
			tdbnext->tdb_comp_ratio_dbytes += ntohs(ipp->tot_len);
		}

		tdbp->ips_life.ipl_bytes.ipl_count += len;
		tdbp->ips_life.ipl_bytes.ipl_last   = len;

		if(!tdbp->ips_life.ipl_usetime.ipl_count) {
			tdbp->ips_life.ipl_usetime.ipl_count = jiffies / HZ;
		}
		tdbp->ips_life.ipl_usetime.ipl_last = jiffies / HZ;
		tdbp->ips_life.ipl_packets.ipl_count += 1;
        tdbp->ips_req_done_count++;
        spin_unlock(&tdb_lock);

	} while(   (ipp->protocol == IPPROTO_ESP )
		|| (ipp->protocol == IPPROTO_AH  )
		|| (ipp->protocol == IPPROTO_COMP)
		);
	/* end decapsulation loop here */

        spin_lock(&tdb_lock);
        tdbp->ips_req_count++; 

        if(tdbnext && tdbnext->tdb_said.proto == IPPROTO_COMP) {

            tdbprev = tdbp;
            tdbp->ips_req_done_count++; 
            tdbp = tdbnext;
            pRcvDesc->tdbp = tdbp;
            tdbp->ips_req_count++; 
            tdbnext = tdbp->tdb_inext;
        }

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
	if ((irs->natt_type) && (ipp->protocol != IPPROTO_IPIP)) {
	    ipsec_rcv_natt_correct_tcp_udp_csum(skb, ipp, tdbp);
	}
#endif

        /*
        * XXX this needs to be locked from when it was first looked
        * up in the decapsulation loop.  Perhaps it is better to put
        * the IPIP decap inside the loop.
        */
        if(tdbnext) {
            tdbp->ips_req_done_count++;
            tdbp = tdbnext;
            tdbp->ips_req_count++; 
            pRcvDesc->tdbp = tdbp;


            sa_len = satoa(tdbp->tdb_said, 0, sa, SATOA_BUF);
            if(ipp->protocol != IPPROTO_IPIP) {
                tdbp->ips_req_done_count++; 
                spin_unlock(&tdb_lock); 
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                        "klips_debug: "
                        "SA:%s, Hey!  How did this get through?  Dropped.\n",
                        sa_len ? sa : " (error)");
                if(pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_dropped++;
                }
                goto rcvleave;
            }
            if(sysctl_ipsec_inbound_policy_check) {
                tdbnext = tdbp->tdb_inext;
                if(tdbnext) {
                    char sa2[SATOA_BUF];
                    size_t sa_len2;
                    sa_len2 = satoa(tdbnext->tdb_said, 0, sa2, SATOA_BUF);
                    tdbp->ips_req_done_count++; 
                    spin_unlock(&tdb_lock); 
                    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                            "klips_debug: "
                            "unexpected SA:%s after IPIP SA:%s\n",
                            sa_len2 ? sa2 : " (error)",
                            sa_len ? sa : " (error)");
                    if(pRcvDesc->stats) {
                        (pRcvDesc->stats)->rx_dropped++;
                    }
                    goto rcvleave;
                }
                if(ipp->saddr != ((struct sockaddr_in*)(tdbp->tdb_addr_s))->sin_addr.s_addr) {
                    tdbp->ips_req_done_count++; 
                    spin_unlock(&tdb_lock); 
                    ipaddr.s_addr = ipp->saddr;
                    addrtoa(ipaddr, 0, ipaddr_txt, sizeof(ipaddr_txt));
                    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
                            "klips_debug: "
                            "SA:%s, src=%s of pkt does not agree with expected SA source address policy.\n",
                            sa_len ? sa : " (error)",
                            ipaddr_txt);
                    if(pRcvDesc->stats) {
                        (pRcvDesc->stats)->rx_dropped++;
                    }
                    goto rcvleave;
                }
            } /* end of if(sysctl_ipsec_inbound_policy_check) */

            /*
            * XXX this needs to be locked from when it was first looked
            * up in the decapsulation loop.  Perhaps it is better to put
            * the IPIP decap inside the loop.
            */
            tdbp->ips_life.ipl_bytes.ipl_count += len;
            tdbp->ips_life.ipl_bytes.ipl_last   = len;

            if(!tdbp->ips_life.ipl_usetime.ipl_count) {
                tdbp->ips_life.ipl_usetime.ipl_count = jiffies / HZ;
            }
            tdbp->ips_life.ipl_usetime.ipl_last = jiffies / HZ;
            tdbp->ips_life.ipl_packets.ipl_count += 1;

            if(skb->len < iphlen) {
                PRINTK_REJECT(&pkt_info, KERN_WARNING, "klips_debug: "
                    "tried to skb_pull iphlen=%d, %d available.  This should never happen, please report.\n",
                    iphlen,
                    (int)(skb->len));

                tdbp->ips_req_done_count++; 
                spin_unlock (&tdb_lock);
                goto rcvleave;
            }
            skb_pull(skb, iphlen);

#ifdef NET_21
            ipp = (struct iphdr *)skb->nh.raw = skb->data;
            skb->h.raw = skb->nh.raw + (skb->nh.iph->ihl << 2);

            memset(&(IPCB(skb)->opt), 0, sizeof(struct ip_options));
#else /* NET_21 */
            ipp = skb->ip_hdr = skb->h.iph = (struct iphdr *)skb->data;

            memset(skb->proto_priv, 0, sizeof(struct options));
#endif /* NET_21 */

            skb->protocol = htons(ETH_P_IP);
            skb->ip_summed = 0;
            KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
                    "klips_debug:ipsec_rcv: "
                    "IPIP tunnel stripped.\n");
            KLIPS_IP_PRINT(debug_rcv & DB_RX_PKTRX, ipp);

            if(sysctl_ipsec_inbound_policy_check)
            {
		u32 s_addr, d_addr, tdb_s_ip, tdb_d_ip, tdb_s_to, tdb_d_to;

		s_addr = ntohl(ipp->saddr);
		tdb_s_ip = ntohl(tdbp->tdb_flow_s.u.v4.sin_addr.s_addr);
		tdb_s_to = ntohl(tdbp->tdb_to_s.u.v4.sin_addr.s_addr);
		d_addr = ntohl(ipp->daddr);
		tdb_d_ip = ntohl(tdbp->tdb_flow_d.u.v4.sin_addr.s_addr);
		tdb_d_to = ntohl(tdbp->tdb_to_d.u.v4.sin_addr.s_addr);
		if (!IS_MATCH(s_addr, tdb_s_ip,
		    ntohl(tdbp->tdb_mask_s.u.v4.sin_addr.s_addr),
		    tdb_s_to) ||
		    !IS_MATCH(d_addr, tdb_d_ip,
		    ntohl(tdbp->tdb_mask_d.u.v4.sin_addr.s_addr), tdb_d_to))
		{
		    struct in_addr daddr, saddr;
		    char saddr_txt[ADDRTOA_BUF], daddr_txt[ADDRTOA_BUF];
		    char sflow_txt[SUBNETTOA_BUF], dflow_txt[SUBNETTOA_BUF];

		    subnettoa(tdbp->tdb_flow_s.u.v4.sin_addr,
			tdbp->tdb_mask_s.u.v4.sin_addr,
			0, sflow_txt, sizeof(sflow_txt));
		    subnettoa(tdbp->tdb_flow_d.u.v4.sin_addr,
			tdbp->tdb_mask_d.u.v4.sin_addr,
			0, dflow_txt, sizeof(dflow_txt));
		    saddr.s_addr = ipp->saddr;
		    daddr.s_addr = ipp->daddr;
		    addrtoa(saddr, 0, saddr_txt, sizeof(saddr_txt));
		    addrtoa(daddr, 0, daddr_txt, sizeof(daddr_txt));
		    KLIPS_PRINT_REJECT(&pkt_info, debug_rcv,
			    "klips_debug: "
			    "SA:%s, inner tunnel policy [%s -> %s] does not agree with pkt contents [%s -> %s].\n",
			    sa_len ? sa : " (error)",
			    sflow_txt,
			    dflow_txt,
			    saddr_txt,
			    daddr_txt);
		    if(pRcvDesc->stats) {
			(pRcvDesc->stats)->rx_dropped++;
		    }
		    tdbp->ips_req_done_count++; 
		    spin_unlock (&tdb_lock);
		    goto rcvleave;
		}
	    }
        } /* end of if(tdbnext) */
        
        tdbp->ips_req_done_count++;
        spin_unlock(&tdb_lock);

#ifdef NET_21
        if(pRcvDesc->stats) {
            (pRcvDesc->stats)->rx_bytes += skb->len;
        }
        if(skb->dst) {
            dst_release(skb->dst);
            skb->dst = NULL;
        }
        skb->pkt_type = PACKET_HOST;
        if(pRcvDesc->hard_header_len &&
        (skb->mac.raw != (skb->data - pRcvDesc->hard_header_len)) &&
        (pRcvDesc->hard_header_len <= skb_headroom(skb))) {
            /* copy back original MAC header */
            memmove(skb->data - pRcvDesc->hard_header_len, skb->mac.raw, pRcvDesc->hard_header_len);
            skb->mac.raw = skb->data - pRcvDesc->hard_header_len;
        }
#endif /* NET_21 */

        if(ipp->protocol == IPPROTO_COMP) {
            unsigned int flags = 0;

            if(sysctl_ipsec_inbound_policy_check) {
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_PKTRX,
                    "klips_debug: "
                    "inbound policy checking enabled, IPCOMP follows IPIP, dropped.\n");
                if (pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_errors++;
                }
                goto rcvleave;
            }
            /*
            XXX need a TDB for updating ratio counters but it is not
            following policy anyways so it is not a priority
            */
            skb = skb_decompress(skb, NULL, &flags);
            if (!skb || flags) {
                KLIPS_PRINT_REJECT(&pkt_info, debug_rcv & DB_RX_PKTRX,
                    "klips_debug: "
                    "skb_decompress() returned error flags: %d, dropped.\n",
                    flags);
                if (pRcvDesc->stats) {
                    (pRcvDesc->stats)->rx_errors++;
                }
                goto rcvleave;
            }
        }

#ifdef SKB_RESET_NFCT
	nf_conntrack_put(skb->nfct);
	skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 0;
#endif /* CONFIG_NETFILTER_DEBUG */
#endif /* SKB_RESET_NFCT */

        KLIPS_PRINT(debug_rcv & DB_RX_PKTRX,
                "klips_debug:ipsec_rcv: "
                "netif_rx() called.\n");
        netif_rx(skb);
	skb = NULL;

rcvleave:
	if(skb) {
		ipsec_kfree_skb(skb);
	}
        /* release desc */
        if (pRcvDesc) {
	        ipsec_glue_rcv_desc_release (pRcvDesc);
	}

	return 0;
}

int _ipsec_rcv(struct sk_buff *skb, struct sock *sk)
{
#ifdef NET_21
	struct net_device *dev = skb->dev;
#endif /* NET_21 */
	unsigned char protoc;
	struct iphdr *ipp;
	int iphlen;
	struct net_device *ipsecdev = NULL, *prvdev;
	struct ipsecpriv *prv;
	struct ipsec_rcv_state nirs, *irs = &nirs;
	IpsecRcvDesc *pRcvDesc;

	/* Don't unlink in the middle of a turnaround */
	KLIPS_DEC_USE;

	if (skb == NULL) {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "NULL skb passed in.\n");
		goto rcvleave;
	}

	if (skb->data == NULL) {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "NULL skb->data passed in, packet is bogus, dropping.\n");
		goto rcvleave;
	}

	memset(irs, 0, sizeof(*irs));

#if defined(CONFIG_IPSEC_NAT_TRAVERSAL) && !defined(NET_26)
	{
		/* NET_26 NAT-T is handled by seperate function */
		struct sk_buff *nskb;
		int udp_decap_ret = 0;

		nskb = ipsec_rcv_natt_decap(skb, sk, irs, &udp_decap_ret);
		if (nskb == NULL) {
			/* return with non-zero, because UDP.c code
			 * need to send it upstream.
			 */
			if (skb && udp_decap_ret == 0) {
				ipsec_kfree_skb(skb);
			}
			KLIPS_DEC_USE;
			return(udp_decap_ret);
		}
		skb = nskb;
	}
#endif /* NAT_T */

	/* Get rcv desc */
	if (ipsec_glue_rcv_desc_get(&pRcvDesc) != 0)
	{
	    KLIPS_PRINT(debug_rcv,
		"klips_debug:ipsec_rcv: "
		"run out of rcv descriptors, dropping.\n");
	    goto rcvleave;
	}

#ifdef IPH_is_SKB_PULLED
	/* In Linux 2.4.4, the IP header has been skb_pull()ed before the
	   packet is passed to us. So we'll skb_push() to get back to it. */
	if (skb->data == skb->h.raw) {
		skb_push(skb, skb->h.raw - skb->nh.raw);
	}
#endif /* IPH_is_SKB_PULLED */

	/* dev->hard_header_len is unreliable and should not be used */
	pRcvDesc->hard_header_len = skb->mac.raw ? (skb->data - skb->mac.raw) : 0;
	if((pRcvDesc->hard_header_len < 0) || (pRcvDesc->hard_header_len > skb_headroom(skb)))
		pRcvDesc->hard_header_len = 0;

	skb = ipsec_rcv_unclone(skb, pRcvDesc->hard_header_len);
	if (!skb) {
	        goto rcvleave;
	}

#if IP_FRAGMENT_LINEARIZE
	/* In Linux 2.4.4, we may have to reassemble fragments. They are
	   not assembled automatically to save TCP from having to copy
	   twice.
	*/
	if (skb_is_nonlinear(skb)) {
	    if (skb_linearize(skb, GFP_ATOMIC) != 0) {
		goto rcvleave;
	    }
	}
	ipp = (struct iphdr *)skb->nh.iph;
	iphlen = ipp->ihl << 2;
#endif

#if defined(CONFIG_IPSEC_NAT_TRAVERSAL) && !defined(NET_26)
	if (irs->natt_len) {
		/**
		 * Now, we are sure packet is ESPinUDP, and we have a private
		 * copy that has been linearized, remove natt_len bytes
		 * from packet and modify protocol to ESP.
		 */
		if (((unsigned char *)skb->data > (unsigned char *)skb->nh.iph)
		    && ((unsigned char *)skb->nh.iph > (unsigned char *)skb->head))
		{
			unsigned int _len = (unsigned char *)skb->data -
				(unsigned char *)skb->nh.iph;
			KLIPS_PRINT(debug_rcv,
				"klips_debug:ipsec_rcv: adjusting skb: skb_push(%u)\n",
				_len);
			skb_push(skb, _len);
		}
		KLIPS_PRINT(debug_rcv,
		    "klips_debug:ipsec_rcv: "
			"removing %d bytes from ESPinUDP packet\n", irs->natt_len);
		ipp = skb->nh.iph;
		iphlen = ipp->ihl << 2;
		ipp->tot_len = htons(ntohs(ipp->tot_len) - irs->natt_len);
		if (skb->len < iphlen + irs->natt_len) {
			printk(KERN_WARNING
		        "klips_error:ipsec_rcv: "
		        "ESPinUDP packet is too small (%d < %d+%d). "
			"This should never happen, please report.\n",
		        (int)(skb->len), iphlen, irs->natt_len);
			goto rcvleave;
		}

		memmove(skb->data + irs->natt_len, skb->data, iphlen);
		skb_pull(skb, irs->natt_len);
		/* update nh.iph */
		ipp = skb->nh.iph = (struct iphdr *)skb->data;

		/* modify protocol */
		ipp->protocol = IPPROTO_ESP;

		skb->sk = NULL;

		KLIPS_IP_PRINT(debug_rcv, skb->nh.iph);
	}
#endif

	ipp = skb->nh.iph;
	iphlen = ipp->ihl << 2;

	KLIPS_PRINTMORE_START(debug_rcv,
		    "klips_debug:ipsec_rcv: "
		    "<<< Info -- ");
	KLIPS_PRINTMORE(debug_rcv && skb->dev, "skb->dev=%s ",
		    skb->dev->name ? skb->dev->name : "NULL");
	KLIPS_PRINTMORE(debug_rcv && dev, "dev=%s ",
		    dev->name ? dev->name : "NULL");
	KLIPS_PRINTMORE_FINISH(debug_rcv);

	KLIPS_PRINT(debug_rcv && !(skb->dev && dev && (skb->dev == dev)),
		    "klips_debug:ipsec_rcv: "
		    "Informational -- **if this happens, find out why** skb->dev:%s is not equal to dev:%s\n",
		    skb->dev ? (skb->dev->name ? skb->dev->name : "NULL") : "NULL",
		    dev ? (dev->name ? dev->name : "NULL") : "NULL");

	protoc = ipp->protocol;
#ifndef NET_21
	if((!protocol) || (protocol->protocol != protoc)) {
		KLIPS_PRINT(debug_rcv & DB_RX_TDB,
			    "klips_debug:ipsec_rcv: "
			    "protocol arg is NULL or unequal to the packet contents, this is odd, using value in packet.\n");
	}
#endif /* !NET_21 */

	if( (protoc != IPPROTO_AH) &&
#ifdef CONFIG_IPSEC_IPCOMP_disabled_until_we_register_IPCOMP_HANDLER
	    (protoc != IPPROTO_COMP) &&
#endif /* CONFIG_IPSEC_IPCOMP */
	    (protoc != IPPROTO_ESP) ) {
		KLIPS_PRINT(debug_rcv & DB_RX_TDB,
			    "klips_debug:ipsec_rcv: Why the hell is someone "
			    "passing me a non-ipsec protocol = %d packet? -- dropped.\n",
			    protoc);
		goto rcvleave;
	}

	if(skb->dev) {
		ipsec_dev_list *cur;

		for(cur=ipsec_dev_head; cur; cur=cur->next) {
			if(!strcmp(cur->ipsec_dev->name, skb->dev->name)) {
				prv = (struct ipsecpriv *)(skb->dev->priv);
				if(prv) {
				    pRcvDesc->stats = (struct net_device_stats *) &(prv->mystats);
				}
				ipsecdev = skb->dev;
				KLIPS_PRINT(debug_rcv,
					    "klips_debug:ipsec_rcv: "
					    "Info -- pkt already proc'ed a group of ipsec headers, processing next group of ipsec headers.\n");
				break;
			}
			if((ipsecdev = ipsec_dev_get(cur->ipsec_dev->name)) == NULL) {
				KLIPS_PRINT(debug_rcv,
					    "klips_error:ipsec_rcv: "
					    "device %s does not exist\n",
					    cur->ipsec_dev->name);
			}
			prv = ipsecdev ? (struct ipsecpriv *)(ipsecdev->priv) : NULL;
			prvdev = prv ? (struct net_device *)(prv->dev) : NULL;

			if(prvdev && skb->dev &&
			   !strcmp(prvdev->name, skb->dev->name)) {
				pRcvDesc->stats = prv ? ((struct net_device_stats *) &(prv->mystats)) : NULL;

				skb->dev = ipsecdev;
				KLIPS_PRINT(debug_rcv && prvdev,
					    "klips_debug:ipsec_rcv: "
					    "assigning packet ownership to virtual device %s from physical device %s.\n",
					    cur->ipsec_dev->name, prvdev->name);
				if(pRcvDesc->stats) {
					(pRcvDesc->stats)->rx_packets++;
				}
				break;
			}
		}
	} else {
		KLIPS_PRINT(debug_rcv,
			    "klips_debug:ipsec_rcv: "
			    "device supplied with skb is NULL\n");
	}

	if(!pRcvDesc->stats) {
		ipsecdev = NULL;
	}
	KLIPS_PRINT((debug_rcv && !pRcvDesc->stats),
		    "klips_error:ipsec_rcv: "
		    "packet received from physical I/F (%s) not connected to ipsec I/F.  Cannot record stats.  May not have SA for decoding.  Is IPSEC traffic expected on this I/F?  Check routing.\n",
		    skb->dev ? (skb->dev->name ? skb->dev->name : "NULL") : "NULL");

	KLIPS_IP_PRINT(debug_rcv, ipp);

	irs->skb = skb;

	ipsec_rcv_decap(irs, pRcvDesc);
	KLIPS_DEC_USE;

        return(0);

rcvleave:
	/* release desc */
	if (pRcvDesc)
	        ipsec_glue_rcv_desc_release (pRcvDesc);

 	if(skb) {
	    ipsec_kfree_skb(skb);
	}

	KLIPS_DEC_USE;
	return(0);
}

int
#ifdef PROTO_HANDLER_SINGLE_PARM
ipsec_rcv(struct sk_buff *skb)
#else /* PROTO_HANDLER_SINGLE_PARM */
#ifdef NET_21
ipsec_rcv(struct sk_buff *skb, unsigned short xlen)
#else /* NET_21 */
ipsec_rcv(struct sk_buff *skb, struct net_device *dev, struct options *opt,
		__u32 daddr_unused, unsigned short xlen, __u32 saddr,
                                   int redo, struct inet_protocol *protocol)
#endif /* NET_21 */
#endif /* PROTO_HANDLER_SINGLE_PARM */
{
	return _ipsec_rcv(skb, NULL);
}

struct inet_protocol ah_protocol =
{
	ipsec_rcv,				/* AH handler */
	NULL,				/* TUNNEL error control */
	0,				/* next */
	IPPROTO_AH,			/* protocol ID */
	0,				/* copy */
	NULL,				/* data */
	"AH"				/* name */
};

struct inet_protocol esp_protocol =
{
	ipsec_rcv,			/* ESP handler          */
	NULL,				/* TUNNEL error control */
	0,				/* next */
	IPPROTO_ESP,			/* protocol ID */
	0,				/* copy */
	NULL,				/* data */
	"ESP"				/* name */
};

#if 0
/* We probably don't want to install a pure IPCOMP protocol handler, but
   only want to handle IPCOMP if it is encapsulated inside an ESP payload
   (which is already handled) */
struct inet_protocol comp_protocol =
{
	ipsec_rcv,			/* COMP handler		*/
	NULL,				/* COMP error control	*/
	0,				/* next */
	IPPROTO_COMP,			/* protocol ID */
	0,				/* copy */
	NULL,				/* data */
	"COMP"				/* name */
};
#endif
