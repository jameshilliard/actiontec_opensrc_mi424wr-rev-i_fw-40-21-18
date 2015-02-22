/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wps_opt_upnp.c
//  Description: EAP-WPS UPnP option source
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

#include "includes.h"

#include "defs.h"
#include "common.h"
#include "hostapd.h"
#include "eloop.h"
#include "config.h"
#include "wps_config.h"
#include "wpa_ctrl.h"
#include "state_machine.h"
#include "wps_parser.h"
#include "wps_opt_upnp.h"
#include "eap_wps.h"
#include "upnp_wps_device.h"
#ifdef WPS_OPT_TINYUPNP
#include "eap_i.h"
#endif  /* WPS_OPT_TINYUPNP */


#define STATE_MACHINE_DATA struct wps_opt_upnp_sm
#define STATE_MACHINE_DEBUG_PREFIX "OPT_UPNP"
#define UPNP_TIMEOUT_SECONDS (5*60)     /* longer timeout w/ external registrar */

/**
 * struct wps_opt_upnp_sm - Internal data for UPNP state machines
 */

#ifdef WPS_OPT_TINYUPNP
/* Queue multiple messages as can occur if we get a mix of M2D
 * and M2 messages.
 */
struct wps_opt_upnp_sm_rcvd_msg {
    struct wps_opt_upnp_sm_rcvd_msg *next;
    size_t msg_len;
    u8 msg[1];  /* nominal size, actually bigger */
};
#endif

struct wps_opt_upnp_sm {
	struct wps_opt_upnp_sm_ctx *ctx;
	struct upnp_wps_device_sm *upnp_device_sm;
	struct eap_wps_data *proc_data; /* associated WPS session */
        #ifdef WPS_OPT_TINYUPNP
        struct wps_opt_upnp_sm_rcvd_msg *rcvUpnpMsgs;
        int nmsgs;
        int wps_state;  /* filter out illegal packets */
        #else   /* original */
	u8 *rcvUpnpMsg;
	size_t rcvUpnpMsgLen;
        #endif
        #if 0   /* was, from Sony */
	struct os_time end_selreg_time;
        #endif
        #ifdef WPS_OPT_TINYUPNP
        struct eap_sm *waiting_eapsm;   /* state machine to wake up */
        #endif  /* WPS_OPT_TINYUPNP */
};

#ifdef WPS_OPT_TINYUPNP
/* queue a message for retrieval */
static void wps_opt_upnp_add_rcvd_msg(
        struct wps_opt_upnp_sm *sm,
        u8 *msg,
        size_t msg_len)
{
    struct wps_opt_upnp_sm_rcvd_msg *rmsg = wpa_zalloc(sizeof(*rmsg)+msg_len);
    struct wps_opt_upnp_sm_rcvd_msg *rmsg1 = sm->rcvUpnpMsgs;
    if (!rmsg) {
        return;
    }
    rmsg->msg_len = msg_len;
    memcpy(rmsg->msg, msg, msg_len);
    if (!rmsg1) {
        sm->rcvUpnpMsgs = rmsg;
    } else {
        while (rmsg1->next) 
            rmsg1 = rmsg1->next;
        rmsg1->next = rmsg;
    }
    sm->nmsgs++;
    wpa_printf(MSG_INFO, "wps_opt_upnp_add_rcvd_msg done: %d msg queued", sm->nmsgs);
}
/* delete first message */
static void wps_opt_upnp_pop_rcvd_msg(
        struct wps_opt_upnp_sm *sm)
{
    struct wps_opt_upnp_sm_rcvd_msg *rmsg1 = sm->rcvUpnpMsgs;
    if (rmsg1) {
        sm->rcvUpnpMsgs = rmsg1->next;
        free(rmsg1);
    }
    sm->nmsgs--;
    wpa_printf(MSG_INFO, "wps_opt_upnp_pop_rcvd_msg done: %d msg queued", sm->nmsgs);
}
/* delete queued messages */
static void wps_opt_upnp_clean_rcvd_msgs(
        struct wps_opt_upnp_sm *sm)
{
        struct wps_opt_upnp_sm_rcvd_msg *rmsg;
        struct wps_opt_upnp_sm_rcvd_msg *rmsg1 = sm->rcvUpnpMsgs;
        int nmsgs = 0;
        sm->rcvUpnpMsgs = NULL;
        while (rmsg1) {
                nmsgs++;
                rmsg = rmsg1->next;
                free(rmsg1);
                rmsg1 = rmsg;
        }
        if (nmsgs != sm->nmsgs) 
                wpa_printf(MSG_ERROR, "wps_opt_upnp_clean_rcvd_msgs: MISMATCH!");
        if (nmsgs != 0 )
            wpa_printf(MSG_INFO, "wps_opt_upnp_clean_rcvd_msg cleaned %d/%d",
                nmsgs, sm->nmsgs);
        sm->nmsgs = 0;
}
#endif  /* WPS_OPT_TINYUPNP */


/* 
 * wps_opt_upnp_deinit_data cancels any ongoing session.
 */
void wps_opt_upnp_deinit_data(struct wps_opt_upnp_sm *sm)
{
        if (sm->proc_data) {
                wpa_printf(MSG_INFO, "wps_opt_upnp: Release wps session");
                sm->proc_data = NULL;
                /* the eap_wps_data (proc_data) will be freed later */
        } else {
                wpa_printf(MSG_INFO, "wps_opt_upnp: No wps session!");
        }
        sm->waiting_eapsm = NULL;
        wps_opt_upnp_clean_rcvd_msgs(sm);
        return;
}


static int wps_opt_upnp_init_data(struct wps_opt_upnp_sm *sm,
								  struct eap_wps_data **data)
{
	int ret = -1;
	struct wps_config *conf;

        wpa_printf(MSG_INFO, "ENTER wps_opt_upnp_init_data");
	do {
		if (!sm || !sm->ctx || !data)
			break;
		*data = 0;

		if (!sm->ctx->get_conf)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?sm->ctx->get_conf(sm->ctx->ctx)->wps:0;
		if (!conf)
			break;

		*data = (struct eap_wps_data *)wpa_zalloc(sizeof(**data));
		if (!*data)
			break;
		if (eap_wps_config_init_data((struct hostapd_data *)sm->ctx->ctx, conf, *data, NULL))
			break;
		ret = 0;
	} while (0);

	if (ret) {
                wpa_printf(MSG_ERROR, "FAILED wps_opt_upnp_init_data");
		if (data && *data) {
			os_free(*data);
			*data = 0;
		}
	} else {
                wpa_printf(MSG_INFO, "SUCCESS wps_opt_upnp_init_data");
        }

	return ret;
}


static u8 *
wps_opt_upnp_build_req_enrollee(
        struct wps_opt_upnp_sm *sm,
	struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *req_len)
{
#define EAP_WPS_COMP_FILENAME "eap_wps_upnp_cmp.conf"
	u8 *req = 0;
	struct eap_wps_target_info *target;
        char *filepath = NULL;
	struct hostapd_data *hapd = (struct hostapd_data *)sm->ctx->ctx;

	do {
		if (!sm || !conf || !data || !data->target || !req_len)
			break;
		target = data->target;

		switch (data->state) {
		case START:
		{
			/* Should be received Start Message */
			/* Build M1 message */
			if (!(req = eap_wps_config_build_message_M1(hapd, conf, data, req_len)))
				break;
			data->state = M2;
			break;
		}
		case M3:
		{
			/* Build M3 message */
			if (!(req = eap_wps_config_build_message_M3(hapd, conf, data, req_len)))
				break;
			data->state = M4;
			break;
		}
		case M5:
		{
			/* Build M5 message */
			if (!(req = eap_wps_config_build_message_M5(hapd, conf, data, req_len)))
				break;
			data->state = M6;
			break;
		}
		case M7:
		{
			/* Build M7 message */
			if (!(req = eap_wps_config_build_message_M7(hapd, conf, data, req_len)))
				break;
			data->state = M8;
			break;
		}
		case NACK:
		{
			if (!(req = eap_wps_config_build_message_special(hapd, conf, data, WPS_MSGTYPE_NACK,
															 data->nonce, target->nonce,
															 req_len)))
				break;
			data->state = NACK;
			break;
		}
		case DONE:
		{
			// struct hostapd_data *hapd = (struct hostapd_data *)sm->ctx->ctx;
			/* Build Done */
                        wpa_printf(MSG_INFO, "WPS-UPnP build DONE");
			if (!(req = eap_wps_config_build_message_special(hapd, conf, data, WPS_MSGTYPE_DONE,
															 data->nonce, target->nonce,
															 req_len)))
				break;

                        /* Make temporary file in same directory as final one */
                        filepath = wps_config_temp_filepath_make(
                                hapd->conf->config_fname,
                                EAP_WPS_COMP_FILENAME);

                        if (target->config == NULL) {
                                wpa_printf(MSG_INFO, 
"WPS-UPnP build DONE not configuring due to no config!");
				eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "No config!");
                        } else
                        if ((conf->wps_job_busy && conf->do_save) ||
                            (!conf->wps_job_busy && 
                                    conf->wps_state == 
                                        WPS_WPSSTATE_UNCONFIGURED)) {
				/* Set Network Configuration */
                                wpa_printf(MSG_INFO, 
"WPS-UPnP build DONE configuring ourselves!");
				if (eap_wps_config_set_ssid_configuration(
                                                hapd, 
                                                conf, 
					        target->config,
					        target->config_len,
					        filepath)) {
                                        wpa_printf(MSG_ERROR, __FILE__ 
                                            ": failed to save configuration");
                                        break;
                                }
                                wpa_printf(MSG_INFO, 
                                        "Saving config %s to %s",
                                        filepath, 
                                        hapd->conf->config_fname);
                                if (rename(
                                        filepath, 
                                        hapd->conf->config_fname)) {
                                        wpa_printf(MSG_ERROR, "Failed to rename %s to %s",
                                                filepath,
                                                hapd->conf->config_fname);
                                        break;
                                }
                                os_free(filepath);
                                filepath = strdup(hapd->conf->config_fname);
                                /* Load the configuration we saved, after a delay.
                                * Note: Currently this is almost an entire "reboot"
                                * of the entire program.
                                */
                                hostapd_reload_configuration(hapd);
                                             
			        if (conf->wps_job_busy) {
                                        #ifdef WPS_OPT_TINYUPNP
                                        /* Throw away any old messages */
                                        wps_opt_upnp_clean_rcvd_msgs(sm);
                                        #endif

				        (void)eap_wps_disable(hapd, conf,
                                                eap_wps_disable_reason_success);
                                }
                        } else {
                                wpa_printf(MSG_INFO, 
"WPS-UPnP build DONE self-configuring disallowed!");
				eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Self-configuration disallowed!");
			        if (conf->wps_job_busy) {
                                        #ifdef WPS_OPT_TINYUPNP
                                        /* Throw away any old messages */
                                        wps_opt_upnp_clean_rcvd_msgs(sm);
                                        #endif

				        (void)eap_wps_disable(hapd, conf,
                                                eap_wps_disable_reason_misc_failure);
                                }
                        }

			if (conf->dev_pwd_len) {
				conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
				os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
				conf->dev_pwd_len = 0;
			}

			if (conf->set_pub_key) {
				if (conf->dh_secret)
					eap_wps_free_dh(&conf->dh_secret);
				os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
				conf->set_pub_key = 0;
			}

			data->state = FAILURE;
			break;
		}
		default:
		{
			/* Build NACK */
			if (!(req = eap_wps_config_build_message_special(hapd, conf, data, WPS_MSGTYPE_NACK,
															 data->nonce, target->nonce,
															 req_len)))
				break;
			data->state = NACK;
			break;
		}
		}
	} while (0);

        os_free(filepath);
	return req;
#undef EAP_WPS_COMP_FILENAME
}


/* Called with message from external upnp registrar (when we are enrollee) */
static int
wps_opt_upnp_process_enrollee(
        struct wps_opt_upnp_sm *sm,
	struct wps_config *conf,
	struct eap_wps_data *data)
{
#define EAP_WPS_COMP_FILENAME "eap_wps_upnp_cmp.conf"
	int ret = -1;
	struct eap_wps_target_info *target;
	int prev_state;
        char *filepath = NULL;
	struct hostapd_data *hapd = (struct hostapd_data *)sm->ctx->ctx;

	do {
		if (!sm || !conf || !data || !data->target)
			break;
		target = data->target;

		prev_state = data->state;
		switch (data->state) {
		case M2:
		{
			Boolean with_config;
			/* Should be received M2/M2D message */
			if (!eap_wps_config_process_message_M2(conf, data, &with_config)) {
				/* Received M2 */
				if (with_config) {
					/* Build Done message */
					data->state = DONE;
				} else {
					/* Build M3 message */
					data->state = M3;
				}
			} else if (!eap_wps_config_process_message_M2D(conf, data)) {
				/* Received M2D */
				/* Build NACK message */
				data->state = NACK;
			}
			break;
		}
		case M4:
		{
			/* Should be received M4 message */
			if (!eap_wps_config_process_message_M4(conf, data)) {
				/* Build M5 message */
				data->state = M5;
			}
			break;
		}
		case M6:
		{
			/* Should be received M6 message */
			if (!eap_wps_config_process_message_M6(conf, data)) {
				/* Build M7 message */
				data->state = M7;
			}
			break;
		}
		case M8:
		{
			/* Should be received M8 message */
			if (!eap_wps_config_process_message_M8(conf, data)) {
				data->state = DONE;
			}
			break;
		}
		case ACK:
		{
                        wpa_printf(MSG_ERROR,
                                "UNEXPECTED AT %s %d", __FILE__, __LINE__);
			break;
		}
		case NACK:
		{
			/* Should be received NACK message */
			if (!eap_wps_config_process_message_special(
                                        hapd, conf, data, WPS_MSGTYPE_NACK, 
                                        data->nonce, target->nonce)) {
				data->state = FAILURE;
			}
			break;
		}
		default:
		{
			break;
		}
		}
		if (prev_state == data->state)
			break;
		ret = 0;
	} while (0);

        os_free(filepath);
	return ret;
#undef EAP_WPS_COMP_FILENAME
}

#ifdef WPS_OPT_TINYUPNP
/* wps_opt_upnp_wait_eap_sm -- make eap s.m. pend data we will get.
 */
void wps_opt_upnp_wait_eap_sm(
        struct wps_opt_upnp_sm *sm,
        struct eap_sm *eapsm,
        int wps_state) 
{
        wpa_printf(MSG_INFO, "wps_opt_upnp_wait_eap_sm wps_state = %d", wps_state);
        if (sm->wps_state != wps_state) {
                wps_opt_upnp_clean_rcvd_msgs(sm);
                sm->wps_state = wps_state;
        }
        /* Wait for new stuff */
        if (! sm->rcvUpnpMsgs) {
                sm->waiting_eapsm = eapsm;
                if (eapsm) {
                        wpa_printf(MSG_INFO, "wps_opt_upnp_wait_eap_sm WAIT");
                        eapsm->method_pending = METHOD_PENDING_WAIT;
                } else {
                        wpa_printf(MSG_INFO, "wps_opt_upnp_wait_eap_sm no eapsm");
                }
        } else {
                wpa_printf(MSG_INFO, "wps_opt_upnp_wait_eap_sm NO WAIT");
                sm->waiting_eapsm = NULL;
                if (eapsm && eapsm->method_pending == METHOD_PENDING_WAIT) {
                        eapsm->method_pending = METHOD_PENDING_NONE;
                }
        }
}
#endif  /* WPS_OPT_TINYUPNP */


/* Retrieve a message from registrar that we are to proxy over to
 * the WPS client.
 * 
 * Note: original Sony code waited syncronously up to 2 seconds for
 * registrar to send the expected message.  New tiny upnp implementation
 * instead manipulates the EAP state machine so that this function will
 * not be called until the message has already been received by
 * us (and stashed in rcvUpnpMsgs).
 *
 * This function pulls of of the rcv message queue the next message
 * and places it as the wps session "sndMsg"... oddly it then makes a copy
 * of that and returns the copy as return value.
 */
u8 *
wps_opt_upnp_received_message(struct wps_opt_upnp_sm *sm,
							  struct eap_wps_data *data,
							  size_t *req_len)
{
	u8 *req = 0;
	int ret = -1;
        #ifndef WPS_OPT_TINYUPNP
	int timeout = 0;
	struct os_time end, now;
        #endif

	do {
		if (!sm || !data || !req_len)
			break;
		*req_len = 0;

                #ifdef WPS_OPT_TINYUPNP
                if (sm->rcvUpnpMsgs == NULL) {
                        wpa_printf(MSG_ERROR, 
                                "wps_opt_upnp_received_message: No message!");
                        break;
                }
                wpa_printf(MSG_INFO, 
                        "wps_opt_upnp_received_message: got message");
                #else   /* old ugly libupnp stuff from Sony... ugh! */
                /* Note old Sony code here would stall hostapd for up to
                 * 2 seconds waiting for response to registrar
                 */
		os_get_time(&end);
		end.sec += 2;
		while (!timeout) {
			if (sm->rcvUpnpMsg && sm->rcvUpnpMsgLen)
				break;

			os_sleep(0, 100000);
			os_get_time(&now);
			if((now.sec > end.sec) ||
			   ((now.sec == end.sec) &&
				(now.usec >= end.usec)))
				timeout = 1;
		}
                /* Note bug in sony code... it does not take any particularl
                 * action if the registrar fails to respond in 2 minutes 
                 */
                #endif

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}

                #ifdef WPS_OPT_TINYUPNP
		data->sndMsg = (u8 *)wpa_zalloc(sm->rcvUpnpMsgs->msg_len);
		if (!data->sndMsg)
			break;
		os_memcpy(data->sndMsg, sm->rcvUpnpMsgs->msg, sm->rcvUpnpMsgs->msg_len);
		data->sndMsgLen = sm->rcvUpnpMsgs->msg_len;
                #else   /* original from Sony */
		data->sndMsg = (u8 *)wpa_zalloc(sm->rcvUpnpMsgLen);
		if (!data->sndMsg)
			break;
		os_memcpy(data->sndMsg, sm->rcvUpnpMsg, sm->rcvUpnpMsgLen);
		data->sndMsgLen = sm->rcvUpnpMsgLen;
                #endif

		req = (u8 *)wpa_zalloc(data->sndMsgLen);
		if (!req)
			break;
		os_memcpy(req, data->sndMsg, data->sndMsgLen);
		*req_len = data->sndMsgLen;

		ret = 0;
	} while (0);

        #ifdef WPS_OPT_TINYUPNP
        wps_opt_upnp_pop_rcvd_msg(sm);
        #else   /* original from Sony */
	if (sm->rcvUpnpMsg) {
		os_free(sm->rcvUpnpMsg);
		sm->rcvUpnpMsg = 0;
		sm->rcvUpnpMsgLen = 0;
	}
        #endif

	if (ret) {
                wpa_printf(MSG_INFO, "FAIL wps_opt_upnp_received_message");
		if (sm->proc_data)
                        wps_opt_upnp_deinit_data(sm);
		if (req) {
			os_free(req);
			req = 0;
		}
		if (req_len)
			*req_len = 0;
	}

	return req;
}


/*
 * Called to proxy a WPS EAP message from wifi-based client over to
 * external upnp-based registrar.
 */
int
wps_opt_upnp_send_wlan_eap_event(struct wps_opt_upnp_sm *sm,
        struct eap_sm *from_eap_sm,
	struct eap_wps_data *data)
{
	int ret = -1;

	do {
		if (!sm)
			break;

                #ifdef WPS_OPT_TINYUPNP
                #else
		if (sm->rcvUpnpMsg) {
			os_free(sm->rcvUpnpMsg);
			sm->rcvUpnpMsg = 0;
			sm->rcvUpnpMsgLen = 0;
		}
                #endif

		if (upnp_wps_device_send_wlan_event(
                                sm->upnp_device_sm, from_eap_sm->addr, UPNP_WPS_WLANEVENT_TYPE_EAP,
			        data->rcvMsg, data->rcvMsgLen))
			break;

		sm->proc_data = data;
                wpa_printf(MSG_INFO, "sm %p links wps session %p",
                    sm, sm->proc_data);

		ret = 0;
	} while (0);

	return ret;
}


/*
 * Called to proxy a WPS probe request from wifi-based client over to
 * external upnp-based registrar.
 */
int
wps_opt_upnp_send_wlan_probe_event(struct wps_opt_upnp_sm *sm,
        const u8 from_mac_addr[6],
        const u8 *data,
        size_t len)
{
	int ret = -1;

	do {
		if (!sm)
			break;

		if (upnp_wps_device_send_wlan_event(
                                sm->upnp_device_sm, from_mac_addr, UPNP_WPS_WLANEVENT_TYPE_PROBE,
			        data, len))
			break;

		ret = 0;
	} while (0);

	return ret;
}


/* The upnp GetDeviceInfo request "retrieves the device's M1 data"...
 * and thus effectively sets up a WPS session w/ the AP as enrollee?
 * although it is commonly requested when there is no intention
 * to configure the AP... we should perhaps just ignore it if
 * there is a "selected registrar" currently.
 */
static int
wps_opt_upnp_received_req_get_device_info(void *priv,
										  u8 **rsp, size_t *rsp_len)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm = priv;
	struct wps_config *conf;
	struct eap_wps_data *data = 0;

	do {
		if (!sm || !sm->ctx || !rsp || !rsp_len)
			break;
		*rsp = 0;
		*rsp_len = 0;

		if (!sm->ctx->get_conf)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   sm->ctx->get_conf(sm->ctx->ctx)->wps:0;
		if (!conf)
			break;
                if (conf->upnp_enabled) {
                        /* Avoid conflict with "selected registrar" */
                        wpa_printf(MSG_INFO, 
"wps_opt_upnp_received_req_get_device_info: ignoring when selected registrar");
                        break;
                }

		if(wps_opt_upnp_init_data(sm, &data))
			break;

		*rsp = wps_opt_upnp_build_req_enrollee(sm, conf, data, rsp_len);
		if (!rsp)
			break;

		sm->proc_data = data;
                wpa_printf(MSG_INFO, "wps upnp sm %p links wps session %p",
                    sm, sm->proc_data);
		ret = 0;
	} while (0);

	if (ret) {
		os_free(data);
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (rsp_len)
			*rsp_len = 0;
                wps_opt_upnp_deinit_data(sm);
	}

	return ret;
}


/* Called with message from external upnp registrar */
static int
wps_opt_upnp_received_req_put_message(void *priv,
									  u8 *msg, size_t msg_len,
									  u8 **rsp, size_t *rsp_len)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm = priv;
	struct wps_config *conf;
	struct eap_wps_data *data;

        wpa_printf(MSG_INFO, "ENTER wps_opt_upnp_received_req_put_message sm=%p", sm);
	do {
		if (!sm || !sm->ctx || !msg || !rsp || !rsp_len) {
			break;
                }
		*rsp = 0;
		*rsp_len = 0;

		if (!sm->ctx->get_conf) {
			break;
                }
		conf = sm->ctx->get_conf(sm->ctx->ctx)?
			   sm->ctx->get_conf(sm->ctx->ctx)->wps:0;
		if (!conf) {
			break;
                }

		if (!sm->proc_data) {
			break;
                }

		data = sm->proc_data;
		if (data->rcvMsg) {
			os_free(data->rcvMsg);
			data->rcvMsg = 0;
			data->rcvMsgLen = 0;
		}
		data->rcvMsg = (u8 *)wpa_zalloc(msg_len);
		if (!data->rcvMsg) {
			break;
                }
		os_memcpy(data->rcvMsg, msg, msg_len);
		data->rcvMsgLen = msg_len;

		if (wps_opt_upnp_process_enrollee(sm, conf, data)) {
			break;
                }

		if (FAILURE == data->state) {
			wps_opt_upnp_deinit_data(sm);
			ret = 0;
			break;
		}

		*rsp = wps_opt_upnp_build_req_enrollee(sm, conf, data, rsp_len);
		if (!rsp) {
			break;
                }
		if (FAILURE == data->state) {
			wps_opt_upnp_deinit_data(sm);
			ret = 0;
			break;
		}

		ret = 0;
	} while (0);

	if (ret) {
                wpa_printf(MSG_INFO, 
                    "wps_opt_upnp_received_req_put_message failed sm=%p", sm);
		wps_opt_upnp_deinit_data(sm);
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
	} else {
                wpa_printf(MSG_INFO, 
                    "wps_opt_upnp_received_req_put_message success sm=%p", sm);
        }

	return ret;
}


static int
wps_opt_upnp_received_req_get_ap_settings(void *priv,
										  u8 *msg, size_t msg_len,
										  u8 **rsp, size_t *rsp_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_set_ap_settings(void *priv,
										  u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_del_ap_settings(void *priv,
										  u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_get_sta_settings(void *priv,
										   u8 *msg, size_t msg_len,
										   u8 **rsp, size_t *rsp_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_set_sta_settings(void *priv,
										   u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_del_sta_settings(void *priv,
										   u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


/*
 * Called back from our upnp shim code when there is a message
 * from upnp-based registrar to be proxied over to a wifi-based
 * enrollee.
 * However, we don't send the message immediately, instead we
 * rememeber it in rcvUpnpMsg and trigger eap state machine
 * to look for it and then send it.
 */
static int
wps_opt_upnp_received_req_put_wlan_event_response(void *priv,
												  int ev_type,
												  u8 *msg,
												  size_t msg_len)
{
	int ret = -1;
        int multi_messages = 0;
	struct wps_opt_upnp_sm *sm = priv;
        #ifdef WPS_OPT_TINYUPNP
        struct eap_sm *eapsm;
        #endif  /* WPS_OPT_TINYUPNP */
        #ifdef WPS_OPT_TINYUPNP
        int msg_type = wps_get_message_type(msg, msg_len);
        #endif

        wpa_printf(MSG_DEBUG, 
                "ENTER wps_opt_upnp_received_req_put_wlan_event_response");

        
	do {
		if (!sm) {
                    wpa_printf(MSG_INFO, "!sm at %s %d", __FILE__, __LINE__);
		    break;
                }

		if (!sm->proc_data) {
                    wpa_printf(MSG_INFO, 
                        "!sm->proc_data at %s %d", __FILE__, __LINE__);
		    break;
                }

                #ifdef WPS_OPT_TINYUPNP
                /* Reject any messages that don't correspond to what we 
                 * expect.
                 * (Originally the code just blindly sent these to station
                 * even when clearly inappropriate).
                 */
                switch (sm->wps_state) {
	            case START:
                    break;
                    case M1:
                        if (msg_type != WPS_MSGTYPE_M1) 
                            goto Reject;
                    break;
                    case M2:
                        /* not used? */
                    break;
                    case ACK:
                        /* ACK state is when we are waiting for enrollee
                         * to send ACK back in response to an M2D.
                         * Continue to received M2[D] messages in this state.
                         */
                    case M2D1:
                        if (msg_type != WPS_MSGTYPE_M2 &&
                            msg_type != WPS_MSGTYPE_M2D) 
                            goto Reject;
                        multi_messages = 1;
                    break;
                    case M2D2:
                        /* not used? */
                    break;
                    case M3:
                        if (msg_type != WPS_MSGTYPE_M3) 
                            goto Reject;
                    break;
                    case M4:
                        if (msg_type != WPS_MSGTYPE_M4) 
                            goto Reject;
                    break;
                    case M5:
                        if (msg_type != WPS_MSGTYPE_M5) 
                            goto Reject;
                    break;
                    case M6:
                        if (msg_type != WPS_MSGTYPE_M6) 
                            goto Reject;
                    break;
                    case M7:
                        if (msg_type != WPS_MSGTYPE_M7) 
                            goto Reject;
                    break;
                    case M8:
                        if (msg_type != WPS_MSGTYPE_M8) 
                            goto Reject;
                    break;
                    case DONE:
                    break;
                    case NACK:
                    break;
                    case FAILURE:
                    break;
                    default:
                    break;
                }
                /* Also, allow multiple messages to be queued only
                 * for M2/D case.
                 */
                if (multi_messages == 0 && sm->nmsgs != 0) 
                    goto Reject;
                wpa_printf(MSG_INFO, "wps_opt_upnp: Queueing msg type %d at state %d",
                        msg_type, sm->wps_state);
                wps_opt_upnp_add_rcvd_msg(sm, msg, msg_len);
                #else
		if (sm->rcvUpnpMsg) {
			os_free(sm->rcvUpnpMsg);
			sm->rcvUpnpMsg = 0;
			sm->rcvUpnpMsgLen = 0;
		}
		sm->rcvUpnpMsg = (u8 *)wpa_zalloc(msg_len);
		if (!sm->rcvUpnpMsg)
			break;
		os_memcpy(sm->rcvUpnpMsg, msg, msg_len);
		sm->rcvUpnpMsgLen = msg_len;
                #endif

                #ifdef  WPS_OPT_TINYUPNP
                /* Note: Sony code did not force use of this data...
                 * instead it was expected to have been recevied within
                 * a 2 second timeout, ugh ugh ugh.
                 */
                /* Wake up state machine to use the received message */
                eapsm = sm->waiting_eapsm;
                if (eapsm && eapsm->method_pending == METHOD_PENDING_WAIT) {
                        wpa_printf(MSG_DEBUG, 
"wps_opt_upnp_received_req_put_wlan_event_response execute");
                        sm->waiting_eapsm = NULL;
                        eapsm->method_pending = METHOD_PENDING_NONE;
                } else {
                        wpa_printf(MSG_INFO, 
"wps_opt_upnp_received_req_put_wlan_event_response NO eapsm waiting");
                }
                #endif  /* WPS_OPT_TINYUPNP */

		ret = 0;
	} while (0);

        #if 0   /* why? */
	if (ret)
                wps_opt_upnp_deinit_data(sm);
        #endif

	return ret;

        Reject:
        wpa_printf(MSG_INFO, "Reject message type %d at state %d nmsgqd=%d",
                msg_type, sm->wps_state, sm->nmsgs);
        return -1;
}


/*
 * Called when we get a SetSelectedRegistrar UPnP message from
 * a control point which is serving as a registrar.
 * We proxy for this registrar.
 *
 * The message contains a set of information elements.
 */
static int
wps_opt_upnp_received_req_set_selected_registrar(void *priv,
												 u8 *msg,
												 size_t msg_len)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm = priv;
	struct wps_config *conf;
	struct wps_data *wps = 0;
	Boolean selreg;
	u8 u8val;
	u16 dev_pwd_id = 0;
        u16 selreg_config_methods = 0;
        struct hostapd_data *hapd = NULL;

	do {
		if (!sm) break;
                if (!sm->ctx) break;
                hapd = (struct hostapd_data *)sm->ctx->ctx;
                if (!hapd) break;
                if (!hapd->conf) break;
                conf = hapd->conf->wps;
                if (!conf) break;
                if (conf->wps_disable) {
                        wpa_printf(MSG_DEBUG, "WPS is disabled for this BSS");
                        break;
                }
                if (conf->wps_upnp_disable) {
                        wpa_printf(MSG_DEBUG, "WPS UPnP is disabled for this BSS");
                        break;
                }

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(msg, msg_len, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &u8val, 0))
			break;
		if ((WPS_VERSION != u8val) && (WPS_VERSION_EX != u8val))
			break;

		/* Selected Registrar */
		if (wps_get_value(wps, WPS_TYPE_SEL_REGISTRAR, &selreg, 0))
			break;

                #ifdef WPS_OPT_TINYUPNP
                /* Throw away any old messages */
                wps_opt_upnp_clean_rcvd_msgs(sm);
                sm->wps_state = -1;
                #endif

                if (selreg == 0) {
                        /* If external registrar is disabling itself...
                         * we hope that is what is happening, it could
                         * be an external collision.
                         */
                        wpa_printf(MSG_INFO, "WPS External Register has deslected itself.");
                        if (conf->config_who == WPS_CONFIG_WHO_EXTERNAL_REGISTRAR) {

                                eap_wps_disable(hapd, conf,
                                        eap_wps_disable_reason_registrar_stop);
                        }
                        ret = 0;
                        break;
                }



		/* Device Password ID */
		if (wps_get_value(wps, WPS_TYPE_DEVICE_PWD_ID, &dev_pwd_id, 0))
			break;

		/* Selected Registrar Config Methods */
		if (wps_get_value(wps, WPS_TYPE_SEL_REG_CFG_METHODS, &selreg_config_methods, 0))
			break;


                #if 1   /* new */
                {
                        struct eap_wps_enable_params params = {};
                        params.config_who = WPS_CONFIG_WHO_EXTERNAL_REGISTRAR;
                        params.dev_pwd_id = dev_pwd_id;
                        params.selreg_config_methods = selreg_config_methods;
                        params.seconds_timeout = UPNP_TIMEOUT_SECONDS;
                        if (eap_wps_enable(hapd, conf, &params)) {
                                break;
                        }
                }
                #else   /* old... was, from Sony */
                // this is odd... what would happen if there are
                // multiple external registrars?
                // this code seems to assume that there will
                // never be a collision...
		conf->selreg = selreg;
		conf->dev_pwd_id = conf->selreg?dev_pwd_id:WPS_DEVICEPWDID_DEFAULT;
		conf->selreg_config_methods = conf->selreg?selreg_config_methods:0;
		conf->upnp_enabled = conf->selreg?1:0;
		os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
		conf->dev_pwd_len = 0;
		if (conf->set_pub_key) {
			if (conf->dh_secret)
				eap_wps_free_dh(&conf->dh_secret);
			os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
			conf->set_pub_key = 0;
		}
		if (conf->selreg) {
			os_get_time(&sm->end_selreg_time);
			sm->end_selreg_time.sec += EAP_WPS_TIMEOUT_SEC;
			sm->end_selreg_time.usec += EAP_WPS_TIMEOUT_USEC;
		} else {
			os_memset(&sm->end_selreg_time, 0, sizeof(sm->end_selreg_time));
		}
		if (eap_wps_set_ie((struct hostapd_data *)sm->ctx->ctx, conf))
			break;
                #endif  /* was */

		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_reboot_ap(void *priv,
									u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_reset_ap(void *priv,
								   u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_reboot_sta(void *priv,
									 u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


static int
wps_opt_upnp_received_req_reset_sta(void *priv,
									u8 *msg, size_t msg_len)
{
	int ret = -1;

	do {
		ret = 0;
	} while (0);

	return ret;
}


#if 0   /* Was, from Sony. Instead we use wps_enable mechanism */
static void
wps_opt_upnp_sm_check_monitor(void *ctx, void *timeout)
{
	struct wps_opt_upnp_sm *sm = ctx;
	struct wps_config *conf = (sm && sm->ctx && sm->ctx->get_conf &&
							   sm->ctx->get_conf(sm->ctx->ctx))?
							   sm->ctx->get_conf(sm->ctx->ctx)->wps:0;

	if (conf && conf->upnp_enabled) {
		struct os_time now;
		os_get_time(&now);
		if ((now.sec > sm->end_selreg_time.sec) ||
			((now.sec == sm->end_selreg_time.sec) &&
			 (now.usec >= sm->end_selreg_time.usec))) {
			conf->upnp_enabled = 0;
			conf->selreg = 0;
			conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			conf->selreg_config_methods = 0;
			os_memset(&sm->end_selreg_time, 0, sizeof(sm->end_selreg_time));

			(void)eap_wps_set_ie((struct hostapd_data *)sm->ctx->ctx, conf);
			hostapd_msg(sm->ctx->msg_ctx, MSG_INFO, "WPS-Selected Registrar timeout");
		}
	}
}
#endif  /* was */

#if 0   /* was, from Sony */
static void 
wps_opt_upnp_sm_check_monitor_timer(void *ctx, void *timeout)
{
	wps_opt_upnp_sm_check_monitor(ctx, timeout);
	eloop_register_timeout(0, 0, wps_opt_upnp_sm_check_monitor_timer, ctx, timeout);
}
#endif  /* was */

struct wps_opt_upnp_sm *
wps_opt_upnp_sm_init(struct wps_opt_upnp_sm_ctx *ctx)
{
	int ret = -1;
	struct wps_opt_upnp_sm *sm = 0;
	struct upnp_wps_device_ctx *dev_ctx = 0;
	struct upnp_wps_device_sm *dev = 0;
	struct hostapd_bss_config *conf;
	struct wps_config *wps;

	do {
		dev_ctx = wpa_zalloc(sizeof(*dev_ctx));
		if (!dev_ctx)
			break;
		dev_ctx->received_req_get_device_info = 
			wps_opt_upnp_received_req_get_device_info;
		dev_ctx->received_req_put_message = 
			wps_opt_upnp_received_req_put_message;
		dev_ctx->received_req_get_ap_settings = 
			wps_opt_upnp_received_req_get_ap_settings;
		dev_ctx->received_req_set_ap_settings = 
			wps_opt_upnp_received_req_set_ap_settings;
		dev_ctx->received_req_del_ap_settings = 
			wps_opt_upnp_received_req_del_ap_settings;
		dev_ctx->received_req_get_sta_settings = 
			wps_opt_upnp_received_req_get_sta_settings;
		dev_ctx->received_req_set_sta_settings = 
			wps_opt_upnp_received_req_set_sta_settings;
		dev_ctx->received_req_del_sta_settings = 
			wps_opt_upnp_received_req_del_sta_settings;
		dev_ctx->received_req_put_wlan_event_response = 
			wps_opt_upnp_received_req_put_wlan_event_response;
		dev_ctx->received_req_set_selected_registrar = 
			wps_opt_upnp_received_req_set_selected_registrar;
		dev_ctx->received_req_reboot_ap = 
			wps_opt_upnp_received_req_reboot_ap;
		dev_ctx->received_req_reset_ap = 
			wps_opt_upnp_received_req_reset_ap;
		dev_ctx->received_req_reboot_sta = 
			wps_opt_upnp_received_req_reboot_sta;
		dev_ctx->received_req_reset_sta = 
			wps_opt_upnp_received_req_reset_sta;

		sm = wpa_zalloc(sizeof(*sm));
		if (!sm)
			break;
		sm->ctx = ctx;

		if (!sm->ctx || !sm->ctx->get_conf)
			break;
		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf || !conf->wps)
			break;

		wps = conf->wps;
		if (!conf->iface[0] || !conf->bridge[0])
			break;

		if (!wps->upnp_iface)
			wps->upnp_iface = os_strdup(conf->bridge);

		if (!wps->upnp_iface || !wps->upnp_iface[0])
			break;

                /* Note: dev_ctx ownereship is given to upnp_wps_device... 
                 * which must free it
                 */
		dev = upnp_wps_device_init(dev_ctx, wps, sm);
		if (!dev)
			break;
		sm->upnp_device_sm = dev;

		if (wps->upnp_iface) {
			if (upnp_wps_device_start(sm->upnp_device_sm, wps->upnp_iface))
				break;
			wps->upnp_enabled = 0;
		}

                #if 0   /* was, from Sony */
		eloop_register_timeout(0, 0, wps_opt_upnp_sm_check_monitor_timer, sm, 0);
                #endif
		ret = 0;
	} while (0);

	if (ret) {
		if (sm) {
			wps_opt_upnp_sm_deinit(sm);
			sm = 0;
		}
	}

	return sm;
}


void wps_opt_upnp_sm_deinit(struct wps_opt_upnp_sm *sm)
{
	do {
		if (!sm)
			break;
		upnp_wps_device_deinit(sm->upnp_device_sm);
                #ifdef WPS_OPT_TINYUPNP
                wps_opt_upnp_clean_rcvd_msgs(sm);
                sm->wps_state = -1;
                #else
		if (sm->rcvUpnpMsg)
			free(sm->rcvUpnpMsg);
                sm->rcvUpnpMsg = NULL;          /* aid debugging */
                sm->rcvUpnpMsgLen = 0;          /* aid debugging */
                #endif
                sm->waiting_eapsm = NULL;       /* aid debugging */
		free(sm->ctx);
                sm->ctx = 0;                    /* aid debugging */
		free(sm);
	} while (0);
}

#ifdef WPS_OPT_TINYUPNP
/* 
 * A debugging hack:
 * READVERTISE causes the tiny upnp state machine to restart it's
 * broadcast advertisements, beginning with a "byebye"... this should
 * with any luck fix any failure to subscribe issues.
 * It should also cause existing subscriptions to be abandoned by
 * clients who will hopefully get new ones... 
 * it would likely interfere with an ongoing wps upnp operation.
 */
void wps_opt_upnp_readvertise(struct wps_opt_upnp_sm *sm)
{
        upnp_device_readvertise(sm->upnp_device_sm);
}
#endif  // WPS_OPT_TINYUPNP

