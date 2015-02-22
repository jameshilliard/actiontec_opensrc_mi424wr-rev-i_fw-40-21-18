/*
 * hostapd / UNIX domain socket -based control interface
 * Copyright (c) 2004, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include <sys/un.h>
#include <sys/stat.h>

#include "hostapd.h"
#include "eloop.h"
#include "base64.h"
#include "config.h"
#include "eapol_sm.h"
#include "ieee802_1x.h"
#include "wpa.h"
#include "radius_client.h"
#include "ieee802_11.h"
#include "ctrl_iface.h"
#include "sta_info.h"
#include "accounting.h"

// #ifdef MODIFIED_BY_SONY
#include "driver.h"
#include "config.h"
// #endif /* MODIFIED_BY_SONY */

#ifdef EAP_WPS
#include "driver.h"
#include "wps_config.h"
#include "eap_wps.h"
#include <unistd.h>
#include <fcntl.h>
#ifdef WPS_OPT_UPNP
#include "wps_opt_upnp.h"
#endif /* WPS_OPT_UPNP */
#ifdef WPS_OPT_NFC
#include "wps_opt_nfc.h"
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */


struct wpa_ctrl_dst {
	struct wpa_ctrl_dst *next;
	struct sockaddr_un addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


static int hostapd_ctrl_iface_attach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dst = wpa_zalloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	memcpy(&dst->addr, from, sizeof(struct sockaddr_un));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dst->next = hapd->ctrl_dst;
	hapd->ctrl_dst = dst;
	wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor attached",
		    (u8 *) from->sun_path, fromlen);
	return 0;
}


static int hostapd_ctrl_iface_detach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst, *prev = NULL;

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    memcmp(from->sun_path, dst->addr.sun_path, fromlen) == 0) {
			if (prev == NULL)
				hapd->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			free(dst);
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor detached",
				    (u8 *) from->sun_path, fromlen);
			return 0;
		}
		prev = dst;
		dst = dst->next;
	}
	return -1;
}


static int hostapd_ctrl_iface_level(struct hostapd_data *hapd,
				    struct sockaddr_un *from,
				    socklen_t fromlen,
				    char *level)
{
	struct wpa_ctrl_dst *dst;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE LEVEL %s", level);

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    memcmp(from->sun_path, dst->addr.sun_path, fromlen) == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE changed monitor "
				    "level", (u8 *) from->sun_path, fromlen);
			dst->debug_level = atoi(level);
			return 0;
		}
		dst = dst->next;
	}

	return -1;
}


static int hostapd_ctrl_iface_sta_mib(struct hostapd_data *hapd,
				      struct sta_info *sta,
				      char *buf, size_t buflen)
{
	int len, res, ret;

	if (sta == NULL) {
		ret = snprintf(buf, buflen, "FAIL\n");
		if (ret < 0 || (size_t) ret >= buflen)
			return 0;
		return ret;
	}

	len = 0;
	ret = snprintf(buf + len, buflen - len, MACSTR "\n",
		       MAC2STR(sta->addr));
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	res = ieee802_11_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = wpa_get_mib_sta(sta->wpa_sm, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = ieee802_1x_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

#ifdef EAP_WPS
	res = wps_get_wps_ie_txt(hapd, sta->wps_ie, sta->wps_ie_len, buf + len, buflen - len);
	if (res >= 0)
		len += res;
#endif /* EAP_WPS */

	return len;
}


static int hostapd_ctrl_iface_sta_first(struct hostapd_data *hapd,
					char *buf, size_t buflen)
{
	return hostapd_ctrl_iface_sta_mib(hapd, hapd->sta_list, buf, buflen);
}


static int hostapd_ctrl_iface_sta(struct hostapd_data *hapd,
				  const char *txtaddr,
				  char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	int ret;

	if (hwaddr_aton(txtaddr, addr)) {
		ret = snprintf(buf, buflen, "FAIL\n");
		if (ret < 0 || (size_t) ret >= buflen)
			return 0;
		return ret;
	}
	return hostapd_ctrl_iface_sta_mib(hapd, ap_get_sta(hapd, addr),
					  buf, buflen);
}


static int hostapd_ctrl_iface_sta_next(struct hostapd_data *hapd,
				       const char *txtaddr,
				       char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	int ret;

	if (hwaddr_aton(txtaddr, addr) ||
	    (sta = ap_get_sta(hapd, addr)) == NULL) {
		ret = snprintf(buf, buflen, "FAIL\n");
		if (ret < 0 || (size_t) ret >= buflen)
			return 0;
		return ret;
	}		
	return hostapd_ctrl_iface_sta_mib(hapd, sta->next, buf, buflen);
}


static int hostapd_ctrl_iface_new_sta(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE NEW_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return 0;

	wpa_printf(MSG_DEBUG, "Add new STA " MACSTR " based on ctrl_iface "
		   "notification", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta == NULL)
		return -1;

	hostapd_new_assoc_sta(hapd, sta, 0);
	accounting_sta_get_id(hapd, sta);
	return 0;
}


#ifdef MODIFIED_BY_SONY
static int hostapd_ctrl_iface_status(struct hostapd_data *hapd,
				      char *buf, size_t buflen)
{
	char *pos, *end, tmp[150];
	int res, ret;
#ifdef EAP_WPS
	struct wps_config *wps = hapd->conf->wps;
#endif /* EAP_WPS */

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "bssid=" MACSTR "\n",
			   MAC2STR(hapd->own_addr));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;
	if (hapd->conf) {
		struct hostapd_bss_config *bss = hapd->conf;
		struct hostapd_ssid *ssid = &bss->ssid;
		u8 *_ssid = (u8 *)ssid->ssid;
		size_t ssid_len = ssid->ssid_len;
		u8 ssid_buf[HOSTAPD_MAX_SSID_LEN];
		if (ssid_len == 0) {
			res = hostapd_get_ssid(hapd, ssid_buf, sizeof(ssid_buf));
			if (res < 0)
				ssid_len = 0;
			else
				ssid_len = res;
			_ssid = ssid_buf;
		} else {
			memcpy(ssid_buf, _ssid, ssid_len);
		}
		ssid_buf[ssid_len] = 0;
		ret = os_snprintf(pos, end - pos, "ssid=%s\n", ssid_buf);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "encription=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		if (bss->wpa) {
			if (bss->wpa_pairwise & WPA_CIPHER_CCMP) {
				ret = os_snprintf(pos, end - pos, "CCMP ");
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			}
			if (bss->wpa_pairwise & WPA_CIPHER_TKIP) {
				ret = os_snprintf(pos, end - pos, "TKIP ");
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			}
		} else {
			if (ssid->wep.keys_set) {
				ret = os_snprintf(pos, end - pos, "WEP ");
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			} else {
				ret = os_snprintf(pos, end - pos, "NONE ");
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			}
		}
		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "key_mgmt=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
		if (bss->wpa) {
			if (bss->wpa & HOSTAPD_WPA_VERSION_WPA2) {
				ret = os_snprintf(pos, end - pos, "WPA2 ");
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			}
			if (bss->wpa & HOSTAPD_WPA_VERSION_WPA) {
				ret = os_snprintf(pos, end - pos, "WPA ");
				if (ret < 0 || ret >= end - pos)
					return pos - buf;
				pos += ret;
			}
		} else if (bss->auth_algs & HOSTAPD_AUTH_SHARED_KEY) {
			ret = os_snprintf(pos, end - pos, "SHARED ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		} else {
			ret = os_snprintf(pos, end - pos, "OPEN ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		#ifdef CONFIG_CTRL_IFACE_SHOW_KEY
                if (bss == NULL || bss->eap_user == NULL) {
                        sprintf(tmp, "No eap user");
                } else
		if (bss->eap_user->password_len > 64 ||
		    bss->eap_user->password_len <= 0 ) {
			sprintf(tmp, "invalid password_len %zu",
				bss->eap_user->password_len);
		} else {
			int i;
			for (i = 0; i < bss->eap_user->password_len; i++ ) {
				sprintf(tmp+i*2, "%02X", 
					bss->eap_user->password[i]);
			}
		}
		ret = os_snprintf(pos, end - pos, "password=%s\n", tmp);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		if (bss->ssid.wpa_psk) {
			int i;
			for (i = 0; i < PMK_LEN; i++ ) {
				sprintf(tmp+i*2, "%02X", 
					bss->ssid.wpa_psk->psk[i]);
			}
			ret = os_snprintf(pos, end - pos, "psk=%s\n", tmp);
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		#endif 	/* CONFIG_CTRL_IFACE_SHOW_KEY */

		ret = os_snprintf(pos, end - pos, "ip_address=%s\n",
					   hostapd_ip_txt(&bss->own_ip_addr, tmp, sizeof(tmp)));
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

#ifdef EAP_WPS
	if (wps) {
		if (WPS_WPSSTATE_UNCONFIGURED == wps->wps_state) {
			ret = os_snprintf(pos, end - pos, "wps_configured=Unconfigured\n");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		} else if (WPS_WPSSTATE_CONFIGURED == wps->wps_state) {
			ret = os_snprintf(pos, end - pos, "wps_configured=Configured\n");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		if (!wps->selreg) {
			ret = os_snprintf(pos, end - pos, "selected_registrar=FALSE\n");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		} else {
			ret = os_snprintf(pos, end - pos, "selected_registrar=TRUE\n");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;

			tmp[0] = 0;
			switch (wps->dev_pwd_id) {
			case WPS_DEVICEPWDID_DEFAULT:
				os_snprintf(tmp, sizeof(tmp), "Default");
				break;
			case WPS_DEVICEPWDID_USER_SPEC:
				os_snprintf(tmp, sizeof(tmp), "User-specified");
				break;
			case WPS_DEVICEPWDID_MACHINE_SPEC:
				os_snprintf(tmp, sizeof(tmp), "Machine-specified");
				break;
			case WPS_DEVICEPWDID_PUSH_BTN:
				os_snprintf(tmp, sizeof(tmp), "PushButton");
				break;
			case WPS_DEVICEPWDID_REG_SPEC:
				os_snprintf(tmp, sizeof(tmp), "Registrar-specified");
				break;
			default:
				os_snprintf(tmp, sizeof(tmp), "%04X", wps->dev_pwd_id);
				break;
			}
			ret = os_snprintf(pos, end - pos, "dev_pwd_id=%s\n", tmp);
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
	}
#endif /* EAP_WPS */

        /* Track memory usage... useful to check for leaks (unix only) */
	ret = os_snprintf(pos, end - pos, "sbrk(0)=%p\n", sbrk(0));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	return pos - buf;
}
#endif	/* MODIFIED_BY_SONY */



#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGSTOP   -- stop WPS configuration if any
 */
static int hostap_ctrl_iface_configstop(
        struct hostapd_data *hapd)
{
	int ret = -1;
	struct wps_config *wps = hapd->conf->wps;
        wpa_printf(MSG_INFO, "CONFIGSTOP -- stopping WPS operation if any");
        if (wps) ret = eap_wps_disable(hapd, wps, 
                eap_wps_disable_reason_user_stop);
        else wpa_printf(MSG_ERROR, "CONFIGSTOP -- NO wps struct found");
        return ret;
}
#endif
#endif


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGME [<tag>=<value>]...
 * Tags are:
 * pin=<digits>         -- specify password
 * NOT IMPLEMENTED: ssid=<text>          -- restrict search to given SSID
 * NOT IMPLEMENTED: ssid="<text>["]
 * NOT IMPLEMENTED: ssid=0x<hex>
 * NOT IMPLEMENTED: bssid=<hex>:<hex>:<hex>:<hex>:<hex>:<hex>  -- restrict search to given BSSID
 * nosave[={0|1}]       -- empty or 1 to do NOT save result back to config file
 * timeout=<seconds>    -- specify session timeout
 *                      (-1 for no timeout, 0 for default)
 *
 * If no PIN given, push button mode is assumed
 * (however, push button mode for configuring an AP may be disallowed
 * by hostapd for security reasons).
 * If nosave is not given, configuration is saved back to file,
 * but if it is given without a value then it is one (NOT saved).
 * ssid and/or bssid can be give as a filter,
 * after which there must be one PB-ready AP for PB mode;
 * for PIN mode there can be either one PIN-ready AP or else one
 * WPS-capable AP or else one open AP.
 *
 * NOTE: some registars (e.g. Windows Vista) just won't
 * configure (or even show) the access point UNLESS it
 * is marked as unconfigured (wps_state) which should be done by
 * modifying the per-bss config file (e.g. SETBSS wps_configured=0)
 * followed by restarting hostapd (e.g. RECONFIGURE).
 */
static int hostap_ctrl_iface_configme(
        struct hostapd_data *hapd, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = hapd->conf->wps;
	char *password = "";
	size_t pwd_len = 0;
        int filter_bssid_flag = 0;
        u8  filter_bssid[6];
        int filter_ssid_length = 0;
        u8  *filter_ssid = NULL;
        int nosave = 0;
        char *tag;
        char *value;
        int seconds_timeout = 0;        /* 0 for default timeout */

        for (;;) {
                /* Parse args */
                value = "";
                while (*cmd && !isgraph(*cmd)) cmd++;
                if (! *cmd) break;
                tag = cmd;
                while (isgraph(*cmd) && *cmd != '=') cmd++;
                if (*cmd == '=') {
                        *cmd++ = 0;
                        value = cmd;
                        while (isgraph(*cmd)) cmd++;
                }
                if (*cmd) *cmd++ = 0;
                if (!strcmp(tag, "timeout")) {
                        if (*value) seconds_timeout = atol(value);
                        else seconds_timeout = 0;
                        continue;
                }
                if (!strcmp(tag, "nosave")) {
                        if (*value) nosave = atol(value);
                        else nosave = 1;
                        continue;
                }
                if (!strcmp(tag, "pin")) {
                        password = value;
                        pwd_len = os_strlen(password);
                        continue;
                }
                if (!strcmp(tag, "ssid")) {
                        if (*value == 0) {
                                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGME: missing ssid "
	                                "'%s'", value);
                                return -1;
                        } 
                        filter_ssid = (void *)value;
                        filter_ssid_length = os_strlen(value);
                        if (*filter_ssid == '"') {
                                filter_ssid++;
                                filter_ssid_length--;
                                if (filter_ssid_length > 0 &&
                                        filter_ssid[filter_ssid_length-1] == '"') {
                                    filter_ssid[--filter_ssid_length] = 0;
                                }
                        } else
                        if (filter_ssid[0] == '0' && filter_ssid[1] == 'x') {
                                filter_ssid += 2;
                                filter_ssid_length -= 2;
                                filter_ssid_length /= 2;
                                if (hexstr2bin((void *)filter_ssid,
                                        filter_ssid, filter_ssid_length)) {
	                                wpa_printf(MSG_ERROR, 
                                                "CTRL_IFACE CONFIGME: invalid ssid "
		                                "'%s'", value);
	                                return -1;
                                }
                        }
                        continue;
                }
                if (!strcmp(tag, "bssid")) {
	                if (hwaddr_aton(value, filter_bssid)) {
		                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGME: invalid bssid "
			                "'%s'", value);
		                return -1;
	                }
                        filter_bssid_flag = 1;
                        continue;
                }
                wpa_printf(MSG_ERROR, "Unknown tag for CONFIGME: %s", tag);
                return -1;
        }

	if (wps) do {
                struct eap_wps_enable_params params = {};
                params.dev_pwd = (u8 *)password;
                params.dev_pwd_len = pwd_len,
                params.filter_bssid_flag = filter_bssid_flag;
                params.filter_bssid = filter_bssid;
                params.filter_ssid_length = filter_ssid_length;
                params.filter_ssid = filter_ssid;
                params.seconds_timeout = seconds_timeout;
                params.config_who = WPS_CONFIG_WHO_ME;
                params.do_save = !nosave;
                if (eap_wps_enable(hapd, wps, &params)) {
                        break;
                }

		ret = 0;
	} while(0);

        if (ret) {
                wpa_printf(MSG_ERROR, "WPS failed from CONFIGME.");
        }

	return ret;
}
#endif
#endif


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGTHEM [<tag>=<value>]...
 * Tags are:
 * pin=<digits>         -- specify password
 * NOT IMPLEMENTED: ssid=<text>          -- restrict search to given SSID
 * NOT IMPLEMENTED: ssid="<text>["]
 * NOT IMPLEMENTED: ssid=0x<hex>
 * NOT IMPLEMENTED: bssid=<hex>:<hex>:<hex>:<hex>:<hex>:<hex>  -- restrict search to given BSSID
 * timeout=<seconds>    -- specify session timeout 
 *                      (-1 for no timeout, 0 for default)
 *
 * If no PIN given, push button mode is assumed.
 * ssid and/or bssid can be give as a filter,
 * after which there must be one PB-ready AP for PB mode;
 * for PIN mode there can be either one PIN-ready AP or else one
 * WPS-capable AP or else one open AP.
 */
static int hostap_ctrl_iface_configthem(
        struct hostapd_data *hapd, char *cmd)
{
	int ret = -1;
	struct wps_config *wps = hapd->conf->wps;
	char *password = "";
	size_t pwd_len = 0;
        int filter_bssid_flag = 0;
        u8  filter_bssid[6];
        int filter_ssid_length = 0;
        u8  *filter_ssid = NULL;
        char *tag;
        char *value;
        int seconds_timeout = 0;        /* 0 for default timeout */

        for (;;) {
                /* Parse args */
                value = "";
                while (*cmd && !isgraph(*cmd)) cmd++;
                if (! *cmd) break;
                tag = cmd;
                while (isgraph(*cmd) && *cmd != '=') cmd++;
                if (*cmd == '=') {
                        *cmd++ = 0;
                        value = cmd;
                        while (isgraph(*cmd)) cmd++;
                }
                if (*cmd) *cmd++ = 0;
                if (!strcmp(tag, "pin")) {
                        password = value;
                        pwd_len = os_strlen(password);
                        continue;
                }
                if (!strcmp(tag, "timeout")) {
                        if (*value) seconds_timeout = atol(value);
                        else seconds_timeout = 0;
                        continue;
                }
                if (!strcmp(tag, "ssid")) {
                        if (*value == 0) {
                                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGTHEM: missing ssid "
	                                "'%s'", value);
                                return -1;
                        } 
                        filter_ssid = (void *)value;
                        filter_ssid_length = os_strlen(value);
                        if (*filter_ssid == '"') {
                                filter_ssid++;
                                filter_ssid_length--;
                                if (filter_ssid_length > 0 &&
                                        filter_ssid[filter_ssid_length-1] == '"') {
                                    filter_ssid[--filter_ssid_length] = 0;
                                }
                        } else
                        if (filter_ssid[0] == '0' && filter_ssid[1] == 'x') {
                                filter_ssid += 2;
                                filter_ssid_length -= 2;
                                filter_ssid_length /= 2;
                                if (hexstr2bin((void *)filter_ssid,
                                        filter_ssid, filter_ssid_length)) {
	                                wpa_printf(MSG_ERROR, 
                                                "CTRL_IFACE CONFIGTHEM: invalid ssid "
		                                "'%s'", value);
	                                return -1;
                                }
                        }
                        continue;
                }
                if (!strcmp(tag, "bssid")) {
	                if (hwaddr_aton(value, filter_bssid)) {
		                wpa_printf(MSG_ERROR, 
                                        "CTRL_IFACE CONFIGTHEM: invalid bssid "
			                "'%s'", value);
		                return -1;
	                }
                        filter_bssid_flag = 1;
                        continue;
                }
                wpa_printf(MSG_ERROR, "Unknown tag for CONFIGTHEM: %s", tag);
                return -1;
        }

	if (wps) do {
                struct eap_wps_enable_params params = {};
                params.dev_pwd = (u8 *)password;
                params.dev_pwd_len = pwd_len,
                params.filter_bssid_flag = filter_bssid_flag;
                params.filter_bssid = filter_bssid;
                params.filter_ssid_length = filter_ssid_length;
                params.filter_ssid = filter_ssid;
                params.seconds_timeout = seconds_timeout;
                params.config_who = WPS_CONFIG_WHO_THEM;
                params.do_save = 0;
                if (eap_wps_enable(hapd, wps, &params)) {
                        break;
                }
		ret = 0;
	} while(0);

        if (ret) {
                wpa_printf(MSG_ERROR, "WPS failed from CONFIGTHEM.");
        }

	return ret;
}
#endif
#endif



#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
/* CONFIGIE [<option>]... {<filepath>|<base64_encoding>}
 * Reads raw WPS settings information elements from given file
 * and applies to the configuration for this BSS.
 * Options: 
 * -base64 -- base64 encoding follows instead of filepath
 * -nosave -- do not save data to original config file (and do not restart)
 * -norestart -- do not restart after saving to original configuration file
 *  If data is not saved to configuration file, the working configuration
 *  in memory is still modified; however, not all affects are propogated
 *  properly, so a restart is really required.
 */
static int hostapd_ctrl_iface_configie(
        struct hostapd_data *hapd, 
        char *cmd)      /* NOTE: cmd[] is scribbled on */
{
        int ret = -1;   /* return value */
        int nosave = 0;
        int norestart = 0;
        char *value;
        int fd = -1;
        unsigned char *buf = NULL;
        size_t len;
        int is_base64 = 0;

        for (;;) {
                /* Parse args */
                value = "";
                while (*cmd && !isgraph(*cmd)) 
                        cmd++;
                if (! *cmd) 
                        break;
                value = cmd;
                while (isgraph(*cmd) && *cmd != '=')
                        cmd++;
                /* Modify buffer in place for string null termination */
                if (*cmd) 
                        *cmd++ = 0;
                if (*value == '-') {
                        if (!strcmp(value, "-nosave"))
                                nosave = 1;
                        else
                        if (!strcmp(value, "-base64"))
                                is_base64 = 1;
                        else 
                        if (!strcmp(value, "-norestart"))
                                norestart = 1;
                        else {
                                wpa_printf(MSG_ERROR, 
                                        "Unknown option for CONFIGIE: %s", 
                                        value);
                            goto Clean;
                        }
                } else {
                        /* got filepath or encoding */
                        break;
                }
        }
        if (is_base64) {
                /* Encoding in base64 follows */
                int enc_len;
                for (enc_len = 0; isgraph(value[enc_len]); enc_len++) 
                        {;}
                if (enc_len == 0) {
                        wpa_printf(MSG_ERROR, 
                                "Missing base64 value for CONFIGIE");
                        goto Clean;
                }
                buf = base64_decode(value, enc_len, &len);
                if (!buf) {
                        wpa_printf(MSG_ERROR, 
                                "CONFIGIE: Failed to decode base64");
                        goto Clean;
                }
        } else {
                /* Filepath of binary file follows */
                char *filepath;
                int filesize;

                filepath = value;
                if (!*filepath) {
                        wpa_printf(MSG_ERROR, "Missing filepath for CONFIGIE");
                        goto Clean;
                }
                fd = open(filepath, O_RDONLY);
                if (fd < 0) {
                        wpa_printf(MSG_ERROR, "CONFIGIE failed to open file %s", filepath);
                        goto Clean;
                }
                filesize = lseek(fd, 0, SEEK_END);
                if (filesize <= 0) {
                        wpa_printf(MSG_ERROR, 
                                "CONFIGIE bad file %s", filepath);
                        goto Clean;
                }
                lseek(fd, 0, SEEK_SET);
                buf = malloc(filesize);
                if (!buf)
                        goto Clean;
                len = read(fd, buf, filesize);
                if (len != filesize) {
                        wpa_printf(MSG_ERROR, 
                                "CONFIGIE failed to read file %s", filepath);
                        goto Clean;
                }
        }

        if (wps_set_ssid_configuration(hapd, buf, len, !nosave, !norestart)) {
                    goto Clean;
        }
        ret = 0;        /* success */

        Clean:
        if (buf)
                free(buf);
        if (fd >= 0)
                close(fd);
        if (ret)
                wpa_printf(MSG_ERROR, "CONFIGIE failed");
	return ret;
}
#endif
#endif




/* 
 * RECONFIGURE causes a delayed "reboot" of the hostapd program
 * (re-execs with same process id and same argv)
 * This forces a re-read of configuration files, which is it's most
 * likely use.
 * There is (dead) code to re-read configuration files without doing
 * such a "reboot" but it has not been maintained and should not be
 * relied on.
 */
static int hostapd_ctrl_iface_reconfigure(struct hostapd_data *hapd)
{
	if (hostapd_reload_configuration(hapd))
		return -1;

	return 0;
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
static int hostapd_ctrl_iface_readvertise(struct hostapd_data *hapd)
{
        // would like to do:
        // advertisement_state_machine_start(hapd->wps_opt_upnp->upnp_device_sm);
        wps_opt_upnp_readvertise(hapd->wps_opt_upnp);
	return 0;
}
#endif // WPS_OPT_TINYUPNP



/* 
 * QUIT causes a controlled exit of the hostapd program.
 */
static int hostapd_ctrl_iface_quit(struct hostapd_data *hapd)
{
        eloop_terminate();
	return 0;
}





#ifdef EAP_WPS

#ifdef WPS_OPT_NFC
static int hostapd_ctrl_iface_cancel_nfc_command(
	struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: CANCEL_NFC_COMMAND");

	if (wps_opt_nfc_cancel_nfc_comand(hapd->wps_opt_nfc))
		return -1;
	return 0;
}


static int hostapd_ctrl_iface_read_password_token(
	struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: READ_PASSWORD_TOKEN");

	if (wps_opt_nfc_read_password_token(hapd->wps_opt_nfc))
		return -1;
	return 0;
}


static int hostapd_ctrl_iface_write_password_token(
	struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: WRITE_PASSWORD_TOKEN");

	if (wps_opt_nfc_write_password_token(hapd->wps_opt_nfc))
		return -1;
	return 0;
}


static int hostapd_ctrl_iface_read_config_token(
	struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: READ_CONFIG_TOKEN");

	if (wps_opt_nfc_read_config_token(hapd->wps_opt_nfc))
		return -1;
	return 0;
}


static int hostapd_ctrl_iface_write_config_token(
	struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: WRITE_CONFIG_TOKEN");

	if (wps_opt_nfc_write_config_token(hapd->wps_opt_nfc))
		return -1;
	return 0;
}
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */


static void hostapd_ctrl_iface_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	const int reply_size = 4096;
	int reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
#ifdef MODIFIED_BY_SONY
	if ((0 == strncmp(buf, "PING", 4)) ||
		(0 == strncmp(buf, "STATUS", 6)))
		wpa_hexdump_ascii(MSG_MSGDUMP, "RX ctrl_iface", (u8 *) buf, res);
	else
#endif /* MODIFIED_BY_SONY */
	wpa_hexdump_ascii(MSG_DEBUG, "RX ctrl_iface", (u8 *) buf, res);

	reply = malloc(reply_size);
	if (reply == NULL) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
		return;
	}

	memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (strcmp(buf, "PING") == 0) {
		memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (strcmp(buf, "MIB") == 0) {
		reply_len = ieee802_11_get_mib(hapd, reply, reply_size);
		if (reply_len >= 0) {
			res = wpa_get_mib(hapd->wpa_auth, reply + reply_len,
					  reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = ieee802_1x_get_mib(hapd, reply + reply_len,
						 reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = radius_client_get_mib(hapd->radius,
						    reply + reply_len,
						    reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
	} else if (strcmp(buf, "STA-FIRST") == 0) {
		reply_len = hostapd_ctrl_iface_sta_first(hapd, reply,
							 reply_size);
	} else if (strncmp(buf, "STA ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_sta(hapd, buf + 4, reply,
						   reply_size);
	} else if (strncmp(buf, "STA-NEXT ", 9) == 0) {
		reply_len = hostapd_ctrl_iface_sta_next(hapd, buf + 9, reply,
							reply_size);
	} else if (strcmp(buf, "ATTACH") == 0) {
		if (hostapd_ctrl_iface_attach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (strcmp(buf, "DETACH") == 0) {
		if (hostapd_ctrl_iface_detach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (strncmp(buf, "LEVEL ", 6) == 0) {
		if (hostapd_ctrl_iface_level(hapd, &from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else if (strncmp(buf, "NEW_STA ", 8) == 0) {
		if (hostapd_ctrl_iface_new_sta(hapd, buf + 8))
			reply_len = -1;
#ifdef MODIFIED_BY_SONY
	} else if (os_strncmp(buf, "STATUS", 6) == 0) {
		reply_len = hostapd_ctrl_iface_status(hapd, reply, reply_size);
#endif	/* MODIFIED_BY_SONY */
        } else if (os_strncmp(buf, "SETBSS ", 7) == 0) {
		if (hostapd_config_bss_set(hapd->conf, buf+7, 0/*not internal*/))
                        reply_len = -1;
        } else if (os_strncmp(buf, "SETRADIO ", 9) == 0) {
		if (hostapd_config_radio_set(hapd->iconf, buf+9))
                        reply_len = -1;
	} else if (os_strncmp(buf, "RECONFIGURE", 11) == 0) {
		if (hostapd_ctrl_iface_reconfigure(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "QUIT", 4) == 0) {
		if (hostapd_ctrl_iface_quit(hapd))
			reply_len = -1;
#ifdef EAP_WPS
#ifdef WPS_OPT_TINYUPNP
	} else if (os_strncmp(buf, "READVERTISE", 11) == 0) {
		if (hostapd_ctrl_iface_readvertise(hapd))
			reply_len = -1;
#endif  // WPS_OPT_TINYUPNP
	} else if (os_strncmp(buf, "CONFIGME", 8) == 0) {
		if (hostap_ctrl_iface_configme(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "CONFIGTHEM", 10) == 0) {
		if (hostap_ctrl_iface_configthem(hapd, buf + 10))
			reply_len = -1;
	} else if (os_strncmp(buf, "CONFIGSTOP", 10) == 0) {
		if (hostap_ctrl_iface_configstop(hapd))
			reply_len = -1;
        } else if (os_strncmp(buf, "CONFIGIE", 8) == 0) {
                if (hostapd_ctrl_iface_configie(hapd, buf+8))
                        reply_len = -1;
        #if 0   /* WAS from Sony */
	} else if (os_strncmp(buf, "SET_WPS_STATE ", 14) == 0) {
		if (hostapd_ctrl_iface_set_wps_state(hapd, buf + 14))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_SET_PASSWORD ", 17) == 0) {
		if (hostapd_ctrl_iface_wps_set_password(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_CLEAR_PASSWORD ", 18) == 0) {
		if (hostapd_ctrl_iface_wps_clear_password(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_SET_REGMODE ", 16) == 0) {
		if (hostapd_ctrl_iface_wps_set_regmode(hapd, buf + 16))
			reply_len = -1;
        #endif  /* WAS from Sony */
#ifdef WPS_OPT_NFC
	} else if (os_strncmp(buf, "CANCEL_NFC_COMMAND", 18) == 0) {
		if (hostapd_ctrl_iface_cancel_nfc_command(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "READ_PASSWORD_TOKEN", 19) == 0) {
		if (hostapd_ctrl_iface_read_password_token(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "WRITE_PASSWORD_TOKEN", 20) == 0) {
		if (hostapd_ctrl_iface_write_password_token(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "READ_CONFIG_TOKEN", 17) == 0) {
		if (hostapd_ctrl_iface_read_config_token(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "WRITE_CONFIG_TOKEN", 18) == 0) {
		if (hostapd_ctrl_iface_write_config_token(hapd))
			reply_len = -1;
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */
	} else {
		memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}
	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
	free(reply);
}


static char * hostapd_ctrl_iface_path(struct hostapd_data *hapd)
{
	char *buf;
	size_t len;

	if (hapd->conf->ctrl_interface == NULL)
		return NULL;

	len = strlen(hapd->conf->ctrl_interface) + strlen(hapd->conf->iface) +
		2;
	buf = malloc(len);
	if (buf == NULL)
		return NULL;

	snprintf(buf, len, "%s/%s",
		 hapd->conf->ctrl_interface, hapd->conf->iface);
	buf[len - 1] = '\0';
        
	return buf;
}


static void hostapd_create_directory_path(char *filepath)
{
        /* create leading directories needed for file. Ignore errors */
        /* caution: filepath must be temporarily writeable */
        char *slash = filepath;
        for (;;) {
                slash = strchr(slash+1, '/');
                if (slash == NULL) break;
                *slash = 0;     /* temporarily terminate */
                (void) mkdir(filepath, 0777);
                *slash = '/';   /* restore */
        }
        return;
}

int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	hapd->ctrl_sock = -1;

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	if (mkdir(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    chown(hapd->conf->ctrl_interface, 0,
		  hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface]");
		return -1;
	}

	if (strlen(hapd->conf->ctrl_interface) + 1 + strlen(hapd->conf->iface)
	    >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	fname = hostapd_ctrl_iface_path(hapd);
	if (fname == NULL)
		goto fail;
        /* An existing socket could indicate that we are trying to run
         * program twice, but more likely it is left behind when a previous
         * instance crashed... remove it to be safe.
         */
        (void) unlink(fname);
        hostapd_create_directory_path(fname);
	strncpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(PF_UNIX)");
		goto fail;
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    chown(fname, 0, hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	free(fname);

	hapd->ctrl_sock = s;
	eloop_register_read_sock(s, hostapd_ctrl_iface_receive, hapd,
				 NULL);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		free(fname);
	}
	return -1;
}


void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (hapd->ctrl_sock > -1) {
		char *fname;
		eloop_unregister_read_sock(hapd->ctrl_sock);
		close(hapd->ctrl_sock);
		hapd->ctrl_sock = -1;
		fname = hostapd_ctrl_iface_path(hapd);
		if (fname)
			unlink(fname);
		free(fname);

		if (hapd->conf->ctrl_interface &&
		    rmdir(hapd->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				perror("rmdir[ctrl_interface]");
			}
		}
	}

	dst = hapd->ctrl_dst;
	while (dst) {
		prev = dst;
		dst = dst->next;
		free(prev);
	}
}


void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
			     char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	struct msghdr msg;
	int idx;
	struct iovec io[2];
	char levelstr[10];

	dst = hapd->ctrl_dst;
	if (hapd->ctrl_sock < 0 || dst == NULL)
		return;

	snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	io[0].iov_base = levelstr;
	io[0].iov_len = strlen(levelstr);
	io[1].iov_base = buf;
	io[1].iov_len = len;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = 2;

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor send",
				    (u8 *) dst->addr.sun_path, dst->addrlen);
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(hapd->ctrl_sock, &msg, 0) < 0) {
				fprintf(stderr, "CTRL_IFACE monitor[%d]: ",
					idx);
				perror("sendmsg");
				dst->errors++;
				if (dst->errors > 10) {
					hostapd_ctrl_iface_detach(
						hapd, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
}

#endif /* CONFIG_NATIVE_WINDOWS */
