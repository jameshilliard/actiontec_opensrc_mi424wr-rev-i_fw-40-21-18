/*
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

#ifndef _IPSEC_RCV_COMMON_H_
#define _IPSEC_RCV_COMMON_H_

struct sk_buff *ipsec_rcv_unclone(struct sk_buff *skb, int hard_header_len);
void ipsec_rcv_natt_correct_tcp_udp_csum(struct sk_buff *skb,
    struct iphdr *ipp, struct ipsec_sa *tdbp);
struct sk_buff *ipsec_rcv_natt_decap(struct sk_buff *skb, struct sock *sk,
    struct ipsec_rcv_state *irs, int *udp_decap_ret_p);
int ipsec_espinudp_encap(struct sk_buff *skb, struct sock *sk);

#endif

