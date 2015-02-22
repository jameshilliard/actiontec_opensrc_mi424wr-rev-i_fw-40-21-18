/****************************************************************************
 *
 * rg/pkg/freeswan/pluto/block_ip.h
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

#ifndef _BLOCK_IP_H_
#define _BLOCK_IP_H_

#include "constants.h"

typedef struct block_peer {
    struct block_peer *next;
    ip_address ip;
    int reject_num;
    time_t start_time;
    time_t last_reject_time;
} block_peer;

void block_peer_log(const char *msg, block_peer *peer);
int block_peer_is_block_state(enum state_kind state);
int block_peer_is_blocked(block_peer *peer);
block_peer *block_peer_get_by_ip(ip_address *ip);
void block_peer_check_expired(void);
/* If ip is not specified - delete all. */
void block_peer_del(ip_address *ip);
void block_peer_add(ip_address *ip);
void block_peer_set(int reject_num, int block_period);

#endif
