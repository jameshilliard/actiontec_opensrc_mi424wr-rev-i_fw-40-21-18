/****************************************************************************
 *
 * rg/pkg/include/voip_types.h
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

#ifndef _VOIP_TYPES_H_
#define _VOIP_TYPES_H_

/* The following enum values are the payload-type values defined in RFC 3551 */
typedef enum {
    JRTP_PAYLOAD_PCMU = 0,
    JRTP_PAYLOAD_G726_32 = 2,
    JRTP_PAYLOAD_G723 = 4,
    JRTP_PAYLOAD_PCMA = 8,
    JRTP_PAYLOAD_G722 = 9,
    JRTP_PAYLOAD_G728 = 15,
    JRTP_PAYLOAD_G729 = 18,
    JRTP_PAYLOAD_CN = 13,
    JRTP_PAYLOAD_DTMF = 101,
} rtp_payload_type_t;

#endif
