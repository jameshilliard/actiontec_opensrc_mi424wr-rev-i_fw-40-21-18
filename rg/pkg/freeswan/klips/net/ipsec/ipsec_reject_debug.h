/****************************************************************************
 *
 * rg/pkg/freeswan/klips/net/ipsec/ipsec_reject_debug.h
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

#ifndef _IPSEC_REJECT_DEBUG_H_
#define _IPSEC_REJECT_DEBUG_H_

#include "ipsec_log.h"

#ifdef CONFIG_IPSEC_DEBUG

#define PRINTK_REJECT(info, level, params...) \
do { \
    if (debug_reject) \
	ipsec_reject_dump(info, params); \
    else \
	ipsec_log(level params); \
} while (0)

#define KLIPS_PRINT_REJECT(info, flag, params...) \
do { \
    if (debug_reject) \
	ipsec_reject_dump(info, params); \
    else \
	KLIPS_PRINT(flag, params); \
} while (0)

#define KLIPS_REJECT_INFO(info, params...) \
do { \
    if (debug_reject) \
	ipsec_reject_dump(info, params); \
} while (0)

#define MAKE_REJECT_INFO(info, skb) make_reject_info(info, skb)
extern int debug_reject;

struct ipsec_pkt_info_t {
    __u8 proto;
    struct in_addr src;
    struct in_addr dst;
    int has_spi_seq;
    ipsec_spi_t spi;
    __u32 seq;
};

void make_reject_info(struct ipsec_pkt_info_t *pkt_info,
    struct sk_buff *skb);
void ipsec_reject_dump(struct ipsec_pkt_info_t *pkt_info,
    char *reason_format, ...);

#else

#define PRINTK_REJECT(info, params...) ipsec_log(params);

#define KLIPS_PRINT_REJECT(info, flag, params...) do ; while(0)

#define KLIPS_REJECT_INFO(info, params...) do ; while (0)

#define MAKE_REJECT_INFO(info, skb) do ; while (0)

#endif

#endif
