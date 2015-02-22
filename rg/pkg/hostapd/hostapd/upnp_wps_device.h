/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: upnp_wps_device.h
//  Description: EAP-WPS UPnP device source header
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Sony Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************/

#ifndef UPNP_WPS_CTRLPT_H
#define UPNP_WPS_CTRLPT_H

#include "wps_config.h"

struct upnp_wps_device_sm;

struct upnp_wps_device_ctx {
	int (*received_req_get_device_info)(void *priv,
										u8 **rsp, size_t *rsp_len);
	int (*received_req_put_message)(void *priv,
									u8 *msg, size_t msg_len,
									u8 **rsp, size_t *rsp_len);
	int (*received_req_get_ap_settings)(void *priv,
										u8 *msg, size_t msg_len,
										u8 **rsp, size_t *rsp_len);
	int (*received_req_set_ap_settings)(void *priv,
										u8 *msg, size_t msg_len);
	int (*received_req_del_ap_settings)(void *priv,
										u8 *msg, size_t msg_len);
	int (*received_req_get_sta_settings)(void *priv,
										 u8 *msg, size_t msg_len,
										 u8 **rsp, size_t *rsp_len);
	int (*received_req_set_sta_settings)(void *priv,
										 u8 *msg, size_t msg_len);
	int (*received_req_del_sta_settings)(void *priv,
										 u8 *msg, size_t msg_len);
	int (*received_req_put_wlan_event_response)(void *priv,
												int ev_type,
												u8 *msg, size_t msg_len);
	int (*received_req_set_selected_registrar)(void *priv,
											   u8 *msg, size_t msg_len);
	int (*received_req_reboot_ap)(void *priv,
								  u8 *msg, size_t msg_len);
	int (*received_req_reset_ap)(void *priv,
								 u8 *msg, size_t msg_len);
	int (*received_req_reboot_sta)(void *priv,
								   u8 *msg, size_t msg_len);
	int (*received_req_reset_sta)(void *priv,
								  u8 *msg, size_t msg_len);
};

struct upnp_wps_device_sm *
upnp_wps_device_init(struct upnp_wps_device_ctx *ctx,
                                         const struct wps_config *conf,
					 void *priv);
void upnp_wps_device_deinit(struct upnp_wps_device_sm *sm);

int upnp_wps_device_start(struct upnp_wps_device_sm *sm, char *net_if);
int upnp_wps_device_stop(struct upnp_wps_device_sm *sm);

#ifndef NAME_SIZE
#define NAME_SIZE 256
#endif /* NAME_SIZE */


#define UPNP_WPS_WLANEVENT_TYPE_PROBE	1
#define UPNP_WPS_WLANEVENT_TYPE_EAP		2
struct eap_sm;
int upnp_wps_device_send_wlan_event(struct upnp_wps_device_sm *sm,
        const u8 from_mac_addr[6],
	int ev_type,
	const u8 *msg, size_t msg_len);

#ifdef WPS_OPT_TINYUPNP
void upnp_device_readvertise(
        struct upnp_wps_device_sm *sm);
#endif // WPS_OPT_TINYUPNP

#endif /* UPNP_WPS_CTRLPT_H */
