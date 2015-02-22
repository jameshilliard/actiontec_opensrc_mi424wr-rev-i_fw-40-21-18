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

#ifndef _IPSEC_TUNNEL_COMMON_H_
#define _IPSEC_TUNNEL_COMMON_H_

extern ipsec_dev_list *ipsec_dev_head;
void ipsec_tunnel_correct_tcp_udp_csum(struct sk_buff *skb,
    struct iphdr *iph, struct ipsec_sa *tdbp);
int ipsec_tunnel_udp_encap(struct sk_buff *skb, uint8_t natt_type,
    uint8_t natt_head, uint16_t natt_sport, uint16_t natt_dport);

#endif

