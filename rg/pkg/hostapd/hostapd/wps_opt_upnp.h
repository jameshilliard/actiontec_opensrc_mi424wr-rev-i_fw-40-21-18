/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wps_opt_upnp.h
//  Description: EAP-WPS UPnP option source header
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

#ifndef WPS_OPT_UPNP_H
#define WPS_OPT_UPNP_H

struct wps_opt_upnp_sm;
struct eap_wps_data;

struct wps_opt_upnp_sm_ctx {
	void *ctx;		/* pointer to arbitrary upper level context */
	void *msg_ctx;

	struct hostapd_bss_config *(*get_conf)(void *ctx);
};

struct wps_opt_upnp_sm *wps_opt_upnp_sm_init(struct wps_opt_upnp_sm_ctx *ctx);
void wps_opt_upnp_sm_deinit(struct wps_opt_upnp_sm *sm);

struct eap_sm;
void wps_opt_upnp_wait_eap_sm(
        struct wps_opt_upnp_sm *sm,
        struct eap_sm *eapsm,
        int wps_state);

void wps_opt_upnp_deinit_data(struct wps_opt_upnp_sm *sm);

u8 *wps_opt_upnp_received_message(struct wps_opt_upnp_sm *sm,
								  struct eap_wps_data *data,
								  size_t *req_len);
struct eap_sm;
int wps_opt_upnp_send_wlan_eap_event(struct wps_opt_upnp_sm *sm,
        struct eap_sm *from_eap_sm,
	struct eap_wps_data *data);
int wps_opt_upnp_send_wlan_probe_event(struct wps_opt_upnp_sm *sm,
        const u8 from_mac_addr[6],
        const u8 *data,
        size_t len);

#ifdef WPS_OPT_TINYUPNP
void wps_opt_upnp_readvertise(struct wps_opt_upnp_sm *sm);
#endif  // WPS_OPT_TINYUPNP

#endif /* WPS_OPT_UPNP_H */
