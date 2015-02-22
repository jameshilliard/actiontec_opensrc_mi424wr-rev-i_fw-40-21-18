/****************************************************************************
 *
 * rg/pkg/voip/jrtp/rtp.h
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

#ifndef _RTP_H_
#define _RTP_H_

#ifdef __KERNEL__
#include <kos/knet.h>
#else
#include <rg_types.h>
#include <netinet/in.h>
#include <asm/byteorder.h>
#endif

/* RTP header */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    u16 csrc_count:4,
	extention:1,
	padding:1,
	version:2,
        payload:7,
        marker:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
    u16	version:2,
	padding:1,
	extention:1,
        csrc_count:4,
        marker:1,
        payload:7;
#endif
    u16 seq_number;
    u32 timestamp;
    u32 ssrc;
    u32 csrc[0];
} rtphdr_t;

/* RTP DTMF payload */
typedef struct {
    u8 event;
#if defined(__LITTLE_ENDIAN_BITFIELD)
    u16 volume:6,
	res:1,
	end:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
    u16	end:1,
	res:1,
	volume:6;
#endif
    u16 duration;
} dtmf_payload_t;

/* identifiers of the media devices */
typedef enum {
    RTP_MEDIA_TYPE_PHONE = 0,
    RTP_MEDIA_TYPES_COUNT /* must be last in the list - this is count of the
			   * khown types */
} rtp_media_type_t;

/* IOCTL values */
#define RTP_NEW _IOW(RG_IOCTL_PREFIX_RTP, 0, rtp_ioctl_new_t)
#define RTP_CONNECT _IOW(RG_IOCTL_PREFIX_RTP, 1, rtp_ioctl_connect_t)
#define RTP_ID_GET _IOR(RG_IOCTL_PREFIX_RTP, 2, u32)
#define RTP_MODE _IOW(RG_IOCTL_PREFIX_RTP, 3, u32)
#define RTP_PAYLOAD _IOW(RG_IOCTL_PREFIX_RTP, 4, rtp_ioctl_payload_t)
#define RTP_DTMF _IOW(RG_IOCTL_PREFIX_RTP, 5, rtp_ioctl_dtmf_t)
#define RTP_RTCP_SET _IOW(RG_IOCTL_PREFIX_RTP, 6, rtp_ioctl_rtcp_param_t)
#define RTP_STATS_GET _IOR(RG_IOCTL_PREFIX_RTP, 7, rtp_stats_t)
#define RTP_TOS_SET _IOW(RG_IOCTL_PREFIX_RTP, 8, int)
#define RTP_TIMESTAMP_GET _IOW(RG_IOCTL_PREFIX_RTP, 9, u32)

#ifdef __KERNEL__
/* data types for bind RTP module with Media module */

typedef void (*media_write_free_cb_t)(void *ptr);

typedef struct {
    media_write_free_cb_t free_cb;
    void *ptr;
} media_write_params_t;

typedef int (*media_write_cb_t)(void *ctxt, u8 *buf, int len,
    media_write_params_t *params);

/* exported by RTP module and used by each media module for binding */
void rtp_media_bind(void *rtp_context, void *media_context,
    media_write_cb_t media_write_cb, media_write_cb_t *rtp_write_cb);

void rtp_media_unbind(void *rtp_context);
#endif

typedef struct {
    u16 port;
    in_addr_t addr;
    u16 mss_clamping_limit; /* The MSS value to be used when enabling MSS
                             * clamping */
    u8 tos; /* Type Of Service value - will be put on similar field of the IP
	     * header of the RTP packets */
    int rtcp_enable;
} rtp_ioctl_new_t;

typedef struct {
    u16 port;
    in_addr_t addr;
} rtp_ioctl_connect_t;

typedef struct {
    char media_payload_type;
    char rtp_payload_type;
} rtp_ioctl_payload_t;

typedef struct {
    int code; /* DTMF code according to RFC 2833 */
    int duration; /* in milliseconds */
    int stop_flag; /* used to stop previously started DTMF event with undefined
		    * "duration" value */
    char payload; /* payload for out-of-band DTMF */
} rtp_ioctl_dtmf_t;

#define RTP_SEND 	0x01 /* all data, received from media or user, must be
			      * sent to network */
#define RTP_RECEIVE 	0x02 /* all data. received from network, must be
			      * forwarded to media or user */
#define RTP_SENDRECEIVE (RTP_SEND | RTP_RECEIVE)

#define RTCP_CNAME_MAX_SIZE 256

typedef struct {
    char cname[RTCP_CNAME_MAX_SIZE];
} rtp_ioctl_rtcp_param_t;

typedef struct {
    u32 rx_packets;
    u32 tx_packets;
    u32 rx_octets;
    u32 tx_octets;
    u32 rx_packets_lost;
    u32 reported_packets_lost;
    u8 rx_loss_rate;
    u8 reported_loss_rate;
    u32 rx_jitter; /* in timestamp units */
    u32 reported_jitter; /* in timestamp units */
    u32 round_trip_delay; /* in 1/65536 seconds units */
} rtp_stats_t;

#endif

