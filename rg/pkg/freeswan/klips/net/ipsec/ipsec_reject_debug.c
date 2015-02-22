/****************************************************************************
 *
 * rg/pkg/freeswan/klips/net/ipsec/ipsec_reject_debug.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/kernel.h>

#include "ipsec_param.h"
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <freeswan.h>
#include "ipsec_ah.h"
#include "ipsec_esp.h"
#include "ipsec_reject_debug.h"
#ifdef CONFIG_RG_OS_LINUX_22
/* Kernel 2.2 has no vsnprintf.
 * Use define like printk.
 */
#define vsnprintf(buf, size, fmt, va) vsprintf(buf, fmt, va)
#endif

int debug_reject = 0;
 
void make_reject_info(struct ipsec_pkt_info_t *pkt_info,
    struct sk_buff *skb)
{
    struct iphdr *iph;
    int iphlen;

    if (!pkt_info)
	return;
    memset(pkt_info, 0, sizeof(pkt_info));
    if (!skb)
	return;
    iph = (struct iphdr *)skb->data;
    iphlen = iph->ihl << 2;
    pkt_info->proto = iph->protocol;
    pkt_info->src.s_addr = iph->saddr;
    pkt_info->dst.s_addr = iph->daddr;
    if (pkt_info->proto!=IPPROTO_ESP && pkt_info->proto!=IPPROTO_AH)
	return;
    pkt_info->has_spi_seq = 1;
    if (pkt_info->proto==IPPROTO_ESP)
    {
	struct esp *esph = (struct esp *)(skb->data + iphlen);

	pkt_info->spi = esph->esp_spi;
	pkt_info->seq = esph->esp_rpl;
    }
    else
    {
	struct ah *ahh = (struct ah *)(skb->data + iphlen);

	pkt_info->spi = ahh->ah_spi;
	pkt_info->seq = ahh->ah_rpl;
    }
}

void ipsec_reject_dump(struct ipsec_pkt_info_t *pkt_info,
    char *reason_format, ...)
{
    char src_ipaddr_txt[ADDRTOA_BUF], dst_ipaddr_txt[ADDRTOA_BUF];
    char reason[1024];
    va_list args;

    if (!ipsec_rate_limit())
	return;

    addrtoa(pkt_info->src, 0, src_ipaddr_txt, sizeof(src_ipaddr_txt));
    addrtoa(pkt_info->dst, 0, dst_ipaddr_txt, sizeof(dst_ipaddr_txt));
    printk("Packet from %s to %s was rejected:\n",
	src_ipaddr_txt, dst_ipaddr_txt);
    va_start(args, reason_format);
    vsnprintf(reason, sizeof(reason), reason_format, args);
    va_end(args);
    printk("%s", reason);
    if (pkt_info->has_spi_seq)
	printk("spi number: %x, sequence: %u\n", pkt_info->spi, pkt_info->seq);
    else
	printk("spi and sequence numbers are unaccessible (invalid header)\n");
}

