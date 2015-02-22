/****************************************************************************
 *
 * rg/pkg/include/ipsec_types.h
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

#ifndef _IPSEC_TYPES_H_
#define _IPSEC_TYPES_H_

#ifndef __KERNEL__
#include <netinet/in.h>
#else
#include <linux/in.h>
#endif

/* IPSEC types */
typedef enum {
    RANGE_SEL_SUBNET = 0,
    RANGE_SEL_SINGLE = 1,
    RANGE_SEL_RANGE = 2,
    RANGE_SEL_ANY = 3,
    RANGE_SEL_OTHER = 4,
    RANGE_SEL_NONE = 5,
} range_sel_type_t;

typedef struct {
    range_sel_type_t type;
    union {
	struct {
	    struct in_addr net, mask;
	} net;
	struct {
	    struct in_addr start, end;
	} range;
    } u;
} rg_ip_range_t;

#endif
