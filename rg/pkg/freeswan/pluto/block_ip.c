/****************************************************************************
 *
 * rg/pkg/freeswan/pluto/block_ip.c
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

#include <stdlib.h>
#include <freeswan.h>
#include "constants.h"
#include "defs.h"
#include "cookie.h"
#include "id.h"
#include "x509.h"
#include "connections.h"
#include "whack.h"
#include "log.h"
#include "timer.h"
#include "block_ip.h"

block_peer *block_peer_head;

/* When block_ip_reject_num is 0 block unauthenticated IP is disabled. */
int block_ip_reject_num;
int block_ip_period;

void block_peer_log(const char *msg, block_peer *peer)
{
    char peer_ip_str[IDTOA_BUF];
    struct id id;

    iptoid(&peer->ip, &id);
    idtoa(&id, peer_ip_str, IDTOA_BUF);
    loglog(RC_LOG_SERIOUS, msg, peer_ip_str);
}

int block_peer_is_block_state(enum state_kind state)
{
    return (state==STATE_MAIN_R1 || state==STATE_MAIN_R2 ||
	state==STATE_MAIN_R3 || state==STATE_AGGR_R1 ||
	state==STATE_AGGR_R2);
}

int block_peer_is_blocked(block_peer *peer)
{
    return peer && peer->reject_num>=block_ip_reject_num &&
	peer->start_time;
}

block_peer *block_peer_get_by_ip(ip_address *ip)
{
    block_peer *peer = block_peer_head;

    for (; peer; peer=peer->next)
    {
	if (sameaddr(ip, &peer->ip))
	    return peer;
    }
    return NULL;
}

static void block_peer_free(block_peer *peer)
{
    free(peer);
}

void block_peer_check_expired(void)
{
    block_peer **cur = &block_peer_head;
    time_t cur_time = now();

    while (*cur)
    {
	block_peer *tmp = NULL;
	int is_blocked = block_peer_is_blocked(*cur);

	if ((is_blocked && cur_time-(*cur)->start_time >= block_ip_period) ||
	    (!is_blocked && cur_time-(*cur)->last_reject_time >=
	    block_ip_period))
	{
	    tmp = *cur;
	    *cur = (*cur)->next;
	    block_peer_log("unblock IP address %s", tmp);
	    block_peer_free(tmp);
	}
	else
	    cur = &(*cur)->next;
    }
}

static void block_peer_start_block(block_peer *peer)
{
    if (block_peer_is_blocked(peer))
	return;
    if (peer->reject_num<block_ip_reject_num)
	return;
    peer->start_time = now();
    block_peer_log("start block the IP address %s", peer);
}

void block_peer_del(ip_address *ip)
{
    block_peer **peer = &block_peer_head, *tmp;

    while (*peer)
    {
	if (ip && !sameaddr(ip, &(*peer)->ip))
	{
	    peer=&(*peer)->next;
	    continue;
	}
	tmp = *peer;
	*peer = tmp->next;
	block_peer_log("IP address %s was removed from block-IP list", tmp);
	block_peer_free(tmp);
	if (ip)
	    return;
    }
}

void block_peer_add(ip_address *ip)
{
    block_peer **cur = &block_peer_head, *tmp;

    if (!block_ip_reject_num)
	return;
    for (; *cur; cur = &(*cur)->next)
    {
	if (!sameaddr(&(*cur)->ip, ip))
	    continue;
	(*cur)->reject_num++;
	(*cur)->last_reject_time = now();
	goto Exit;
    }
    tmp = (block_peer *)malloc(sizeof(block_peer));
    if (!tmp)
	return;
    tmp->reject_num = 1;
    tmp->ip = *ip;
    tmp->start_time = 0;
    tmp->last_reject_time = now();
    tmp->next = NULL;
    *cur = tmp;
    block_peer_log("IP address %s was added to block-IP list", tmp);

Exit:
    block_peer_start_block(*cur);
}

void block_peer_set(int reject_num, int block_period)
{
    block_ip_reject_num = reject_num;
    block_ip_period = block_period;

    if (!block_ip_reject_num)
	block_peer_del(NULL);
}

