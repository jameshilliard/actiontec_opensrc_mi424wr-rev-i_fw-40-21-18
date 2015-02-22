/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: upnp_wps_device.c
//  Description: EAP-WPS UPnP device source
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

/* ENTIRE FILE IS COMPILED ONLY IF WPS_OPT_TINYUPNP IS >>NOT<< DEFINED */
#ifndef WPS_OPT_TINYUPNP

#include <upnp/ithread.h>
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include "common.h"
#include "upnp_wps_common.h"
#include "upnp_wps_device.h"
#include "base64.h"
#include <stdlib.h>

#include <sys/ioctl.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/route.h>
#include <errno.h>
#include <stdio.h>

#define WPS_MAXVARS		11
#define WPS_MAX_VAL_LEN	255

#define WPS_MAXACTIONS	14

#define DEFAULT_TIMEOUT	1801


struct wps_device_vars {
	int cnt;
	char *name[WPS_MAXVARS];
	char *val[WPS_MAXVARS];
};


struct wps_device_actions {
	int cnt;
	char *name[WPS_MAXACTIONS];
	int (*action[WPS_MAXACTIONS])(struct upnp_wps_device_sm *sm,
				  IXML_Document *req,
				  IXML_Document **rsp,
				  char **err_string);
};


struct wps_device_service {
	struct wps_device_vars var;
	struct wps_device_actions act;
};


struct upnp_wps_device_sm {
	struct upnp_wps_device_ctx *ctx;
	void *priv;
	char *root_dir;
	char *desc_url;
	int initialized;
	ithread_mutex_t mutex_device;
	int mutex_initialized;
	UpnpDevice_Handle device_handle;
	char udn[NAME_SIZE];
	char service_id[NAME_SIZE];
	char service_type[NAME_SIZE];
	struct wps_device_service service;
};


/*
static const char *wps_device_type = "urn:schemas-wifialliance-org:device:WFADevice:1";
*/
static const char *wps_service_type = "urn:schemas-wifialliance-org:service:WFAWLANConfig:1";

enum WPS_VAR {
	WPS_VAR_FIRST = 0,
	WPS_VAR_MESSAGE = WPS_VAR_FIRST,
	WPS_VAR_INMESSAGE,
	WPS_VAR_OUTMESSAGE,
	WPS_VAR_DEVICEINFO,
	WPS_VAR_APSETTINGS,
	WPS_VAR_APSTATUS,
	WPS_VAR_STASETTINGS,
	WPS_VAR_STASTATUS,
	WPS_VAR_WLANEVENT,
	WPS_VAR_WLANEVENTTYPE,
	WPS_VAR_WLANEVENTMAC,
	WPS_VAR_LAST = WPS_VAR_WLANEVENTMAC
};


static const char *wps_service_var_name[WPS_MAXVARS] = {
	"Message",
	"InMessage",
	"OutMessage",
	"DeviceInfo",
	"APSettings",
	"APStatus",
	"STASettings",
	"STAStatus",
	"WLANEvent",
	"WLANEventType",
	"WLANEventMAC"
};


static const char *wps_service_var_default_val[WPS_MAXVARS] = {
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	""
};


enum WPS_ACTION {
	WPS_ACTION_GETDEVICEINFO = 0,
	WPS_ACTION_PUTMESSAGE,
	WPS_ACTION_GETAPSETTINGS,
	WPS_ACTION_SETAPSETTINGS,
	WPS_ACTION_DELAPSETTINGS,
	WPS_ACTION_GETSTASETTINGS,
	WPS_ACTION_SETSTASETTINGS,
	WPS_ACTION_DELSTASETTINGS,
	WPS_ACTION_PUTWLANRESPONSE,
	WPS_ACTION_SETSELECTEDREGISTRAR,
	WPS_ACTION_REBOOTAP,
	WPS_ACTION_RESETAP,
	WPS_ACTION_REBOOTSTA,
	WPS_ACTION_RESETSTA,
};


static const char *wps_service_action_name[WPS_MAXACTIONS] = {
	"GetDeviceInfo",
	"PutMessage",
	"GetAPSettings",
	"SetAPSettings",
	"DelAPSettings",
	"GetSTASettings",
	"SetSTASettings",
	"DelSTASettings",
	"PutWLANResponse",
	"SetSelectedRegistrar",
	"RebootAP",
	"ResetAP",
	"RebootSTA",
	"ResetSTA"
};


struct upnp_wps_device_sm *
upnp_wps_device_init(struct upnp_wps_device_ctx *ctx,
                                         const struct wps_config *conf,
					 void *priv)
{
	struct upnp_wps_device_sm *sm = 0;
	do {
		if (!root_dir || !desc_url)
			break;

		sm = wpa_zalloc(sizeof(*sm));
		if (!sm)
			break;
		sm->ctx = ctx;
		sm->priv = priv;
		sm->root_dir = os_strdup(conf->upnp_root_dir);
		sm->desc_url = os_strdup(conf->upnp_desc_url);
		sm->device_handle = -1;
	} while (0);

	return sm;
}


void
upnp_wps_device_deinit(struct upnp_wps_device_sm *sm)
{
	do {
		if (!sm)
			break;
		upnp_wps_device_stop(sm);

		if (sm->root_dir)
			os_free(sm->root_dir);
		if (sm->desc_url)
			os_free(sm->desc_url);

		os_free(sm->ctx);
		os_free(sm);
	} while (0);
}


int add_ssdp_network(char *net_if)
{
#define SSDP_TARGET		"239.0.0.0"
#define SSDP_NETMASK	"255.0.0.0"
	int ret = -1;
	SOCKET sock = -1;
	struct rtentry rt;
	struct sockaddr_in *sin;

	do {
		if (!net_if)
			break;

		os_memset(&rt, 0, sizeof(rt));
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (-1 == sock)
			break;

		rt.rt_dev = net_if;
		sin = (struct sockaddr_in *)&rt.rt_dst;
		sin->sin_family = AF_INET;
		sin->sin_port = 0;
		sin->sin_addr.s_addr = inet_addr(SSDP_TARGET);
		sin = (struct sockaddr_in *)&rt.rt_genmask;
		sin->sin_family = AF_INET;
		sin->sin_port = 0;
		sin->sin_addr.s_addr = inet_addr(SSDP_NETMASK);
		rt.rt_flags = RTF_UP;
		if (ioctl(sock, SIOCADDRT, &rt) < 0) {
			if (EEXIST != errno) {
                                perror("add_ssdp_network() ioctl error");
				break;
                        }
		}

		ret = 0;
	} while (0);

	if (-1 != sock)
		close(sock);

	return ret;
#undef SSDP_TARGET
#undef SSDP_NETMASK
}


int get_ip_address(char *net_if, char **ipaddr)
{
#define MAX_INTERFACES 256
	int ret = -1;
	char buf[MAX_INTERFACES * sizeof(struct ifreq)];
	struct ifconf conf;
	struct ifreq *req;
	struct sockaddr_in sock_addr;
	int sock = -1;
	int i;

	do {
		if (!ipaddr)
			break;
		*ipaddr = 0;

		if (!net_if)
			break;

		if(0 > (sock = socket(AF_INET, SOCK_DGRAM, 0)))
			break;

		conf.ifc_len = sizeof(buf);
		conf.ifc_ifcu.ifcu_buf = (caddr_t)buf;
		if (0 > ioctl(sock, SIOCGIFCONF, &conf))
			break;

		for( i = 0; i < conf.ifc_len; ) {
			req = (struct ifreq *)((caddr_t)conf.ifc_req + i);
			i += sizeof(*req);

			if (AF_INET == req->ifr_addr.sa_family) {
				if (!os_strcmp(net_if, req->ifr_name)) {
					size_t len;
					os_memcpy(&sock_addr, &req->ifr_addr, sizeof(req->ifr_addr));
					len = os_strlen(inet_ntoa(sock_addr.sin_addr)) + 1;
					*ipaddr = wpa_zalloc(len);
					if (!*ipaddr)
						break;
					os_snprintf(*ipaddr, len, "%s", inet_ntoa(sock_addr.sin_addr));
					ret = 0;
					break;
				}
			}
		}
	} while (0);

	if (0 <= sock)
		close(sock);

	return ret;
#undef MAX_INTERFACES
}


int get_mac_from_ip(char *ipaddr, char mac[18])
{
#define MAX_INTERFACES 256
	int ret = -1;
	char buf[MAX_INTERFACES * sizeof(struct ifreq)];
	struct ifconf conf;
	struct ifreq *req;
	struct sockaddr_in sock_addr;
	int sock = -1;
	int i;

	do {
		if(0 > (sock = socket(AF_INET, SOCK_DGRAM, 0)))
			break;

		conf.ifc_len = sizeof(buf);
		conf.ifc_ifcu.ifcu_buf = (caddr_t)buf;
		if (0 > ioctl(sock, SIOCGIFCONF, &conf))
			break;

		for( i = 0; i < conf.ifc_len; ) {
			req = (struct ifreq *)((caddr_t)conf.ifc_req + i);
			i += sizeof(*req);

			if (AF_INET == req->ifr_addr.sa_family) {
				os_memcpy(&sock_addr, &req->ifr_addr, sizeof(req->ifr_addr));
				if (!os_strcmp(ipaddr, inet_ntoa(sock_addr.sin_addr))) {
					os_snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
					(u8)(req->ifr_hwaddr.sa_data[0]),
					(u8)(req->ifr_hwaddr.sa_data[1]),
					(u8)(req->ifr_hwaddr.sa_data[2]),
					(u8)(req->ifr_hwaddr.sa_data[3]),
					(u8)(req->ifr_hwaddr.sa_data[4]),
					(u8)(req->ifr_hwaddr.sa_data[5]));
					ret = 0;
					break;
				}
			}
		}
	} while (0);

	if (0 <= sock)
		close(sock);

	return ret;
#undef MAX_INTERFACES
}


static int
upnp_wps_device_encode_base64(u8 *data, size_t data_len,
							  char **encoded, size_t *encoded_len)
{
	int ret = -1;

	do {
		if (!data || !encoded || !encoded_len)
			break;
		*encoded = 0;
		*encoded_len = 0;

		*encoded = base64_encode(data, data_len, encoded_len, 72);
		if (!*encoded)
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (encoded && *encoded) {
			os_free(*encoded);
			*encoded = 0;
		}
		if (encoded_len)
			*encoded_len = 0;
	}

	return ret;
}


static int
upnp_wps_device_decode_base64(char *data, size_t data_len,
								  u8 **decoded, size_t *decoded_len)
{
	int ret = -1;

	do {
		if (!data || !decoded || !decoded_len)
			break;
		*decoded = 0;
		*decoded_len = 0;

		*decoded = base64_decode((u8 *)data,
								 data_len, decoded_len);
		if (!*decoded)
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (decoded && *decoded) {
			os_free(*decoded);
			*decoded = 0;
		}
		if (decoded_len)
			*decoded_len = 0;
	}

	return ret;
}


static int
upnp_wps_device_handle_subscription_request(struct upnp_wps_device_sm *sm,
											struct Upnp_Subscription_Request *event)
{
	int ret = -1;
	do {
		if (!sm || !event)
			break;

		ithread_mutex_lock(&sm->mutex_device);

		if (!os_strcmp(event->UDN, sm->udn) &&
			!os_strcmp(event->ServiceId, sm->service_id)) {
			UpnpAcceptSubscription(sm->device_handle,
								   event->UDN,
								   event->ServiceId,
								   (const char **)sm->service.var.name,
								   (const char **)sm->service.var.val,
								   sm->service.var.cnt,
								   event->Sid);
			ret = 0;
		}

		ithread_mutex_unlock(&sm->mutex_device);
	} while (0);

	return ret;
}


static int
upnp_wps_device_handle_get_var_request(struct upnp_wps_device_sm *sm,
									   struct Upnp_State_Var_Request *event)
{
	int ret = -1, i;

	do {
		if (!sm || !event)
			break;

		ithread_mutex_lock(&sm->mutex_device);

		if (!os_strcmp(event->DevUDN, sm->udn) &&
			!os_strcmp(event->ServiceID, sm->service_id)) {
			for (i = 0; i < sm->service.var.cnt; i++) {
				if (!os_strcmp(event->StateVarName, sm->service.var.name[i])) {
					event->CurrentVal =
						ixmlCloneDOMString(sm->service.var.val[i]);
					ret = 0;
					break;
				}
			}
		}

		if (!ret)
			event->ErrCode = UPNP_E_SUCCESS;
		else
			event->ErrCode = 404;

		ithread_mutex_unlock(&sm->mutex_device);
	} while (0);

	return ret;
}


static int
upnp_wps_device_handle_action_request(struct upnp_wps_device_sm *sm,
									  struct Upnp_Action_Request *event)
{
	int ret = -1, i;
	char *str_error = 0;

	do {
		if (!sm || !event)
			break;

		ithread_mutex_lock(&sm->mutex_device);

		if (!os_strcmp(event->DevUDN, sm->udn) &&
			!os_strcmp(event->ServiceID, sm->service_id)) {
			for(i = 0; i < sm->service.act.cnt; i++) {
				if (!os_strcmp(event->ActionName, sm->service.act.name[i])) {
					if (sm->service.act.action[i])
						event->ErrCode = sm->service.act.action[i](
														sm,
														event->ActionRequest,
														&event->ActionResult,
														&str_error);
					else {
						str_error = os_strdup("Function not found");
						event->ErrCode = 501;
					}
					ret = 0;
				}
			}
		}

		if (!ret && str_error)
			os_strncpy(event->ErrStr, str_error, sizeof(event->ErrStr));
		else if (ret) {
			event->ActionResult = 0;
			os_strncpy(event->ErrStr, "Invalid Action", sizeof(event->ErrStr));
			event->ErrCode = 401;
		}

		ithread_mutex_unlock(&sm->mutex_device);
	} while (0);

	if (str_error)
		os_free(str_error);

	return ret;
}


static int
upnp_wps_device_callback_event_handler(Upnp_EventType event_type,
									   void *event, void *cookie)
{
	switch (event_type) {
	/* GENA */
	case UPNP_EVENT_SUBSCRIPTION_REQUEST:
	{
		struct upnp_wps_device_sm *sm = (struct upnp_wps_device_sm *)cookie;
		struct Upnp_Subscription_Request *sub_event = event;
		upnp_wps_device_handle_subscription_request(sm, sub_event);
		break;
	}
	/* SOAP */
	case UPNP_CONTROL_GET_VAR_REQUEST:
	{
		struct upnp_wps_device_sm *sm = (struct upnp_wps_device_sm *)cookie;
		struct Upnp_State_Var_Request *var_event = event;
		upnp_wps_device_handle_get_var_request(sm, var_event);
		break;
	}
	case UPNP_CONTROL_ACTION_REQUEST:
	{
		struct upnp_wps_device_sm *sm = (struct upnp_wps_device_sm *)cookie;
		struct Upnp_Action_Request *action_event = event;
		upnp_wps_device_handle_action_request(sm, action_event);
		break;
	}
	default:
		/* ignore */
		break;
	}

	return 0;
}


static int
upnp_wps_device_set_var(struct upnp_wps_device_sm *sm,
						int var, char *val)
{
	int ret = -1;

	do {
		if (!sm || ((WPS_VAR_FIRST > var) || (WPS_VAR_LAST < var)))
			break;

		if (sm->service.var.val[var]) {
			os_free(sm->service.var.val[var]);
			sm->service.var.val[var] = 0;
		}

		if (val)
			sm->service.var.val[var] = os_strdup(val);
		else
			sm->service.var.val[var] = os_strdup("");

		UpnpNotify(sm->device_handle,
				   sm->udn,
				   sm->service_id,
				   (const char **)&sm->service.var.name[var],
				   (const char **)&sm->service.var.val[var], 1);

		ret = 0;
	} while (0);

	return ret;
}


int
upnp_wps_device_send_wlan_event(struct upnp_wps_device_sm *sm,
        const u8 from_mac_addr[6],
	int ev_type,
	const u8 *msg, size_t msg_len)
{
	int ret = -1;
	char type[2];
	char mac[32];
	u8 *raw = 0;
	size_t raw_len;
	char *val = 0;
	size_t val_len;
	int pos = 0;

	do {
		if (!sm)
			break;

		os_snprintf(type, sizeof(type), "%1u", ev_type);

                %%%%%%% This is WRONG, should use from_mac_addr
                %%%%%%% instread (and format as xx:xx:xx:xx:xx:xx)
		if (get_mac_from_ip(UpnpGetServerIpAddress(), mac))
			break;

		raw_len = 1 + 17 + ((msg && msg_len)?msg_len:0);
		raw = (u8 *)wpa_zalloc(raw_len);
		if (!raw)
			break;

		*(raw + pos) = (u8)ev_type;
		pos += 1;
		os_memcpy(raw + pos, mac, 17);
		pos += 17;
		if (msg && msg_len) {
			os_memcpy(raw + pos, msg, msg_len);
			pos += msg_len;
		}

		if (upnp_wps_device_encode_base64(raw, raw_len, &val, &val_len))
			break;

		upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTTYPE, type);
		upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTMAC, mac);
		upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENT, val);

		ret = 0;
	} while (0);

	if (raw) os_free(raw);
	if (val) os_free(val);

	return ret;
}


static int
upnp_wps_device_get_device_info(struct upnp_wps_device_sm *sm,
								IXML_Document *req,
								IXML_Document **rsp,
								char **err_string)
{
	int ret = 501;
	u8 *raw_msg = 0;
	size_t raw_msg_len;
	char *device_info = 0;
	size_t device_info_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string)
			break;
		*rsp = 0;
		*err_string = 0;

		if (!sm->ctx->received_req_get_device_info)
			break;

		if (sm->ctx->received_req_get_device_info(sm->priv, &raw_msg, &raw_msg_len))
			break;

		(void)upnp_wps_device_encode_base64(raw_msg, raw_msg_len, &device_info, &device_info_len);

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "GetDeviceInfo",
									sm->service_type,
									"NewDeviceInfo", device_info?device_info:""))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_DEVICEINFO, device_info);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (raw_msg) os_free(raw_msg);
	if (device_info) os_free(device_info);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_put_message(struct upnp_wps_device_sm *sm,
							IXML_Document *req,
							IXML_Document **rsp,
							char **err_string)
{
	int ret = 501;
	char *in_msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;
	u8 *raw_msg = 0;
	size_t raw_msg_len;
	char *out_msg = 0;
	size_t out_msg_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewInMessage", &in_msg))
			break;

		if (upnp_wps_device_decode_base64(in_msg, os_strlen(in_msg),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_put_message)
			break;

		if (sm->ctx->received_req_put_message(sm->priv, decoded, decoded_len,
											  &raw_msg, &raw_msg_len))
			break;

		(void)upnp_wps_device_encode_base64(raw_msg, raw_msg_len, &out_msg, &out_msg_len);

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "PutMessage",
									sm->service_type,
									"NewOutMessage", out_msg?out_msg:""))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_INMESSAGE, in_msg);
		(void)upnp_wps_device_set_var(sm, WPS_VAR_OUTMESSAGE, out_msg);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (in_msg) os_free(in_msg);
	if (decoded) os_free(decoded);
	if (raw_msg) os_free(raw_msg);
	if (out_msg) os_free(out_msg);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_get_ap_settings(struct upnp_wps_device_sm *sm,
								IXML_Document *req,
								IXML_Document **rsp,
								char **err_string)
{
	int ret = 501;
	char *msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;
	u8 *raw_msg = 0;
	size_t raw_msg_len;
	char *ap_settings = 0;
	size_t ap_settings_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewMessage", &msg))
			break;

		if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_get_ap_settings)
			break;

		if (sm->ctx->received_req_get_ap_settings(sm->priv, decoded, decoded_len,
											  &raw_msg, &raw_msg_len))
			break;

		if (upnp_wps_device_encode_base64(raw_msg, raw_msg_len, &ap_settings, &ap_settings_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "GetAPSettings",
									sm->service_type,
									"NewAPSettings", ap_settings?ap_settings:""))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
		(void)upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (msg) os_free(msg);
	if (decoded) os_free(decoded);
	if (raw_msg) os_free(raw_msg);
	if (ap_settings) os_free(ap_settings);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_set_ap_settings(struct upnp_wps_device_sm *sm,
								IXML_Document *req,
								IXML_Document **rsp,
								char **err_string)
{
	int ret = 501;
	char *ap_settings = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewAPSettings", &ap_settings))
			break;

		if (upnp_wps_device_decode_base64(ap_settings, os_strlen(ap_settings),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_set_ap_settings)
			break;

		if (sm->ctx->received_req_set_ap_settings(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "SetAPSettings", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (ap_settings) os_free(ap_settings);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_del_ap_settings(struct upnp_wps_device_sm *sm,
								IXML_Document *req,
								IXML_Document **rsp,
								char **err_string)
{
	int ret = 501;
	char *ap_settings = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewAPSettings", &ap_settings))
			break;

		if (upnp_wps_device_decode_base64(ap_settings, os_strlen(ap_settings),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_del_ap_settings)
			break;

		if (sm->ctx->received_req_del_ap_settings(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "DelAPSettings", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (ap_settings) os_free(ap_settings);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_get_sta_settings(struct upnp_wps_device_sm *sm,
								 IXML_Document *req,
								 IXML_Document **rsp,
								 char **err_string)
{
	int ret = 501;
	char *msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;
	u8 *raw_msg = 0;
	size_t raw_msg_len;
	char *sta_settings = 0;
	size_t sta_settings_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewMessage", &msg))
			break;

		if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_get_sta_settings)
			break;

		if (sm->ctx->received_req_get_sta_settings(sm->priv, decoded, decoded_len,
											  &raw_msg, &raw_msg_len))
			break;

		if (upnp_wps_device_encode_base64(raw_msg, raw_msg_len, &sta_settings, &sta_settings_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "GetSTASettings",
									sm->service_type,
									"NewSTASettings", sta_settings?sta_settings:""))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
		(void)upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (msg) os_free(msg);
	if (decoded) os_free(decoded);
	if (raw_msg) os_free(raw_msg);
	if (sta_settings) os_free(sta_settings);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_set_sta_settings(struct upnp_wps_device_sm *sm,
								 IXML_Document *req,
								 IXML_Document **rsp,
								 char **err_string)
{
	int ret = 501;
	char *sta_settings = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewSTASettings", &sta_settings))
			break;

		if (upnp_wps_device_decode_base64(sta_settings, os_strlen(sta_settings),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_set_sta_settings)
			break;

		if (sm->ctx->received_req_set_sta_settings(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "SetSTASettings", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (sta_settings) os_free(sta_settings);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_del_sta_settings(struct upnp_wps_device_sm *sm,
								 IXML_Document *req,
								 IXML_Document **rsp,
								 char **err_string)
{
	int ret = 501;
	char *sta_settings = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewSTASettings", &sta_settings))
			break;

		if (upnp_wps_device_decode_base64(sta_settings, os_strlen(sta_settings),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_del_sta_settings)
			break;

		if (sm->ctx->received_req_del_sta_settings(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "DelSTASettings", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (sta_settings) os_free(sta_settings);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_put_wlan_response(struct upnp_wps_device_sm *sm,
								  IXML_Document *req,
								  IXML_Document **rsp,
								  char **err_string)
{
	int ret = 501;
	char *msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;
	char *type = 0;
	int ev_type;
	char *mac = 0;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string)
			break;
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewMessage", &msg))
			break;

		if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
										  &decoded, &decoded_len))
			break;

		if (upnp_get_first_document_item(req, "NewWLANEventType", &type))
			break;
		ev_type = atoi(type);

		if (upnp_get_first_document_item(req, "NewWLANEventMAC", &mac))
			break;

		if (!sm->ctx->received_req_put_wlan_event_response)
			break;

		if (sm->ctx->received_req_put_wlan_event_response(sm->priv, ev_type,
														  decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "PutWLANResponse", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
		(void)upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTTYPE, type);
		(void)upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTMAC, mac);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (msg) os_free(msg);
	if (decoded) os_free(decoded);
	if (type) os_free(type);
	if (mac) os_free(mac);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_set_selected_registrar(struct upnp_wps_device_sm *sm,
									   IXML_Document *req,
									   IXML_Document **rsp,
									   char **err_string)
{
	int ret = 501;
	char *msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewMessage", &msg))
			break;

		if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_set_selected_registrar)
			break;

		if (sm->ctx->received_req_set_selected_registrar(sm->priv,
														 decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "SetSelectedRegistrar", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (msg) os_free(msg);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_reboot_ap(struct upnp_wps_device_sm *sm,
						  IXML_Document *req,
						  IXML_Document **rsp,
						  char **err_string)
{
	int ret = 501;
	char *ap_settings = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewAPSettings", &ap_settings))
			break;

		if (upnp_wps_device_decode_base64(ap_settings, os_strlen(ap_settings),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_reboot_ap)
			break;

		if (sm->ctx->received_req_reboot_ap(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "RebootAP", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (ap_settings) os_free(ap_settings);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_reset_ap(struct upnp_wps_device_sm *sm,
						 IXML_Document *req,
						 IXML_Document **rsp,
						 char **err_string)
{
	int ret = 501;
	char *msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewMessage", &msg))
			break;

		if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_reset_ap)
			break;

		if (sm->ctx->received_req_reset_ap(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "ResetAP", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (msg) os_free(msg);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_reboot_sta(struct upnp_wps_device_sm *sm,
						   IXML_Document *req,
						   IXML_Document **rsp,
						   char **err_string)
{
	int ret = 501;
	char *sta_settings = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewSTASettings", &sta_settings))
			break;

		if (upnp_wps_device_decode_base64(sta_settings, os_strlen(sta_settings),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_reboot_sta)
			break;

		if (sm->ctx->received_req_reboot_sta(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "RebootSTA", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (sta_settings) os_free(sta_settings);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_reset_sta(struct upnp_wps_device_sm *sm,
						  IXML_Document *req,
						  IXML_Document **rsp,
						  char **err_string)
{
	int ret = 501;
	char *msg = 0;
	u8 *decoded = 0;
	size_t decoded_len;

	do {
		if (!sm || !sm->ctx || !req || !rsp || !err_string) {
			break;
		}
		*rsp = 0;
		*err_string = 0;

		if (upnp_get_first_document_item(req, "NewMessage", &msg))
			break;

		if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
										  &decoded, &decoded_len))
			break;

		if (!sm->ctx->received_req_reset_sta)
			break;

		if (sm->ctx->received_req_reset_sta(sm->priv, decoded, decoded_len))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpAddToActionResponse(rsp, "ResetSTA", sm->service_type, 0, 0))
			break;

		(void)upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);

		ret = UPNP_E_SUCCESS;
	} while (0);

	if (msg) os_free(msg);
	if (decoded) os_free(decoded);

	if (UPNP_E_SUCCESS != ret) {
		if (rsp && *rsp) {
			os_free(*rsp);
			*rsp = 0;
		}
		if (err_string)
			*err_string = os_strdup("Internal Error");
	}

	return ret;
}


static int
upnp_wps_device_deinit_service(struct upnp_wps_device_sm *sm)
{
	int ret = -1, i;

	do {
		if (!sm)
			break;

		for (i = 0; i < sm->service.var.cnt; i++) {
			if (sm->service.var.name[i]) {
				os_free(sm->service.var.name[i]);
				sm->service.var.name[i] = 0;
			}
			if (sm->service.var.val[i]) {
				os_free(sm->service.var.val[i]);
				sm->service.var.val[i] = 0;
			}
		}

		for (i = 0; i < sm->service.act.cnt; i++) {
			if (sm->service.act.name[i]) {
				os_free(sm->service.act.name[i]);
				sm->service.act.name[i] = 0;
			}
		}

		ret = 0;
	} while (0);

	return ret;
}


static int
upnp_wps_device_init_service(struct upnp_wps_device_sm *sm,
							 char *desc_url)
{
	int ret = -1, i;
	IXML_Document *desc_doc = 0;
	char *udn = 0, *service_id = 0, *scpd_url = 0,
		 *control_url = 0, *event_url = 0;

	do {
		if (!sm || !desc_url)
			break;

		if (UPNP_E_SUCCESS !=
			UpnpDownloadXmlDoc(desc_url, &desc_doc))
			break;

		if (upnp_get_first_document_item(desc_doc, "UDN", &udn))
			break;

		if (!upnp_find_service(desc_doc, desc_url, (char *)wps_service_type,
							   &service_id, &scpd_url, &control_url, &event_url))
			break;

		os_strncpy(sm->udn, udn, sizeof(sm->udn));
		os_strncpy(sm->service_id, service_id, sizeof(sm->service_id));
		os_strncpy(sm->service_type, wps_service_type, sizeof(sm->service_type));

		sm->service.var.cnt = WPS_MAXVARS;
		for (i = 0; i < sm->service.var.cnt; i++) {
			sm->service.var.name[i] = os_strdup(wps_service_var_name[i]);
			sm->service.var.val[i] = os_strdup(wps_service_var_default_val[i]);
		}

		sm->service.act.cnt = WPS_MAXACTIONS;
		for (i = 0; i < sm->service.act.cnt; i++) {
			sm->service.act.name[i] = os_strdup(wps_service_action_name[i]);
			switch (i) {
			case WPS_ACTION_GETDEVICEINFO:
				sm->service.act.action[i] = upnp_wps_device_get_device_info;
				break;
			case WPS_ACTION_PUTMESSAGE:
				sm->service.act.action[i] = upnp_wps_device_put_message;
				break;
			case WPS_ACTION_GETAPSETTINGS:
				sm->service.act.action[i] = upnp_wps_device_get_ap_settings;
				break;
			case WPS_ACTION_SETAPSETTINGS:
				sm->service.act.action[i] = upnp_wps_device_set_ap_settings;
				break;
			case WPS_ACTION_DELAPSETTINGS:
				sm->service.act.action[i] = upnp_wps_device_del_ap_settings;
				break;
			case WPS_ACTION_GETSTASETTINGS:
				sm->service.act.action[i] = upnp_wps_device_get_sta_settings;
				break;
			case WPS_ACTION_SETSTASETTINGS:
				sm->service.act.action[i] = upnp_wps_device_set_sta_settings;
				break;
			case WPS_ACTION_DELSTASETTINGS:
				sm->service.act.action[i] = upnp_wps_device_del_sta_settings;
				break;
			case WPS_ACTION_PUTWLANRESPONSE:
				sm->service.act.action[i] = upnp_wps_device_put_wlan_response;
				break;
			case WPS_ACTION_SETSELECTEDREGISTRAR:
				sm->service.act.action[i] = upnp_wps_device_set_selected_registrar;
				break;
			case WPS_ACTION_REBOOTAP:
				sm->service.act.action[i] = upnp_wps_device_reboot_ap;
				break;
			case WPS_ACTION_RESETAP:
				sm->service.act.action[i] = upnp_wps_device_reset_ap;
				break;
			case WPS_ACTION_REBOOTSTA:
				sm->service.act.action[i] = upnp_wps_device_reboot_sta;
				break;
			case WPS_ACTION_RESETSTA:
				sm->service.act.action[i] = upnp_wps_device_reset_sta;
				break;
			default:
				break;
			}
		}

		ret = 0;
	} while (0);

	if (ret)
		(void)upnp_wps_device_deinit_service(sm);

	if (desc_doc) ixmlDocument_free(desc_doc);
	if (udn) os_free(udn);
	if (service_id) os_free(service_id);
	if (scpd_url) os_free(scpd_url);
	if (control_url) os_free(control_url);
	if (event_url) os_free(event_url);

	return ret;
}


int
upnp_wps_device_start(struct upnp_wps_device_sm *sm, char *net_if)
{
	int ret = -1;
	char *ip_address = 0;
	u16 port;
	char desc_url[BUFSIZ];

	do {
		if (!sm || !net_if)
			break;

		if (sm->initialized)
			upnp_wps_device_stop(sm);

		ithread_mutex_init(&sm->mutex_device, 0);
		sm->mutex_initialized = 1;

		if (add_ssdp_network(net_if))
			break;

		if (get_ip_address(net_if, &ip_address))
			break;

		if (UPNP_E_SUCCESS != UpnpInit(ip_address, 0))
			break;
		sm->initialized++;

		if (os_strcmp(UpnpGetServerIpAddress(), ip_address))
			break;

		port = UpnpGetServerPort();

		if (UPNP_E_SUCCESS !=
			UpnpSetWebServerRootDir(sm->root_dir))
			break;

		os_snprintf(desc_url, sizeof(desc_url), "http://%s:%u/%s", ip_address, port,
				 sm->desc_url);
		if (UPNP_E_SUCCESS !=
			UpnpRegisterRootDevice(desc_url,
								   upnp_wps_device_callback_event_handler,
								   (void *)sm, &sm->device_handle))
			break;
		sm->initialized++;

		if (upnp_wps_device_init_service(sm, desc_url))
			break;

		if (UPNP_E_SUCCESS !=
			UpnpSendAdvertisement(sm->device_handle, DEFAULT_TIMEOUT))
				break;

		ret = 0;
	} while (0);

	if (ip_address)
		os_free(ip_address);

	return ret;
}


int
upnp_wps_device_stop(struct upnp_wps_device_sm *sm)
{
	do {
		if (!sm)
			break;

		if (!sm->initialized)
			break;

		if (sm->mutex_initialized)
			ithread_mutex_lock(&sm->mutex_device);
		if (0 <= (int)sm->device_handle) {
			UpnpUnRegisterRootDevice(sm->device_handle);
			sm->device_handle = -1;
			sm->initialized--;
		}

		(void)upnp_wps_device_deinit_service(sm);

		if (sm->initialized) {
			UpnpFinish();
			sm->initialized--;
		}

		if (sm->mutex_initialized) {
			ithread_mutex_unlock(&sm->mutex_device);
			ithread_mutex_destroy(&sm->mutex_device);
			sm->mutex_initialized = 0;
		}
	} while (0);

	if (sm)
		sm->initialized = 0;
	return 0;
}


#endif  /* WPS_OPT_TINYUPNP */
