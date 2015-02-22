/*
 * hostapd / Driver interaction with MADWIFI 802.11 driver
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, Video54 Technologies
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
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
#include "common.h"
#include <net/if.h>
#include <sys/ioctl.h>

#include <include/compat.h>
#include <net80211/ieee80211.h>
#ifdef WME_NUM_AC
/* Assume this is built against BSD branch of madwifi driver. */
#define MADWIFI_BSD
#include <net80211/_ieee80211.h>
#endif /* WME_NUM_AC */
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

#ifdef IEEE80211_IOCTL_SETWMMPARAMS
/* Assume this is built against madwifi-ng */
#define MADWIFI_NG
#endif /* IEEE80211_IOCTL_SETWMMPARAMS */

#include <net/if_arp.h>
#include "wireless_copy.h"

#include <netpacket/packet.h>

#include "hostapd.h"
#include "driver.h"
#include "ieee802_1x.h"
#include "eloop.h"
#include "priv_netlink.h"
#include "sta_info.h"
#include "l2_packet.h"

#include "eapol_sm.h"
#include "wpa.h"
#include "radius.h"
#include "ieee802_11.h"
#include "accounting.h"
#include "common.h"

#if EAP_WPS
#ifndef USE_INTEL_SDK
#include "wps_config.h"
#include "eap_wps.h"
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */


struct madwifi_driver_data {
	struct driver_ops ops;			/* base class */
	struct hostapd_data *hapd;		/* back pointer */

	char	iface[IFNAMSIZ + 1];
	int     ifindex;
	struct l2_packet_data *sock_xmit;	/* raw packet xmit socket */
	struct l2_packet_data *sock_recv;	/* raw packet recv socket */
	int	ioctl_sock;			/* socket for ioctl() use */
	int	wext_sock;			/* socket for wireless events */
	int	we_version;
	u8	acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;
};

static const struct driver_ops madwifi_driver_ops;

static void madwifi_deinit(void *priv);
static int madwifi_sta_deauth(void *priv, const u8 *addr, int reason_code);

#if EAP_WPS
static int madwifi_set_wps_beacon_ie(void *priv, u8 *iebuf, int iebuflen);
static int madwifi_set_wps_probe_resp_ie(void *priv, u8 *iebuf, int iebuflen);
static int madwifi_set_wps_assoc_resp_ie(void *priv, u8 *iebuf, int iebuflen);
static int madwifi_start_receive_prob_req(void *priv);
static void madwifi_handle_mgmt_frames(void *ctx, const unsigned char *src_addr,
        const unsigned char *buf, size_t len);
#endif /* EAP_WPS */

#ifdef MODIFIED_BY_SONY
static int wext_set_key(void *priv, int alg,
						const u8 *addr, int key_idx,
						int set_tx, const u8 *seq, size_t seq_len,
						const u8 *key, size_t key_len);
#endif /* MODIFIED_BY_SONY */

static int
set80211priv(struct madwifi_driver_data *drv, int op, void *data, int len)
{
	struct iwreq iwr;
        int do_inline = (len < IFNAMSIZ);

        /* Poorly thought out inteface -- certain ioctls MUST use
         * the non-inline method:
         */
        if (
            #ifdef IEEE80211_IOCTL_SET_APPIEBUF 
            op == IEEE80211_IOCTL_SET_APPIEBUF ||
            #endif
            #ifdef IEEE80211_IOCTL_FILTERFRAME
            op == IEEE80211_IOCTL_FILTERFRAME ||
            #endif
            0
            ) {
                do_inline = 0;
        }

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	if (do_inline) {
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data MAY BE too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->ioctl_sock, op, &iwr) < 0) {
#if 0   /* was, but who cares? */
#ifdef MADWIFI_NG
		int first = IEEE80211_IOCTL_SETPARAM;
		/* NOT NEEDED: int last = IEEE80211_IOCTL_KICKMAC; */
		static const char *opnames[] = {
			"ioctl[IEEE80211_IOCTL_SETPARAM]",
			"ioctl[IEEE80211_IOCTL_GETPARAM]",
			"ioctl[IEEE80211_IOCTL_SETMODE]",
			"ioctl[IEEE80211_IOCTL_GETMODE]",
			"ioctl[IEEE80211_IOCTL_SETWMMPARAMS]",
			"ioctl[IEEE80211_IOCTL_GETWMMPARAMS]",
			"ioctl[IEEE80211_IOCTL_SETCHANLIST]",
			"ioctl[IEEE80211_IOCTL_GETCHANLIST]",
			"ioctl[IEEE80211_IOCTL_CHANSWITCH]",
			NULL,
			NULL,
			"ioctl[IEEE80211_IOCTL_GETSCANRESULTS]",
			NULL,
			"ioctl[IEEE80211_IOCTL_GETCHANINFO]",
			"ioctl[IEEE80211_IOCTL_SETOPTIE]",
			"ioctl[IEEE80211_IOCTL_GETOPTIE]",
			"ioctl[IEEE80211_IOCTL_SETMLME]",
			NULL,
			"ioctl[IEEE80211_IOCTL_SETKEY]",
			NULL,
			"ioctl[IEEE80211_IOCTL_DELKEY]",
			NULL,
			"ioctl[IEEE80211_IOCTL_ADDMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_DELMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_WDSMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_WDSDELMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_KICMAC]",
		};
#else /* MADWIFI_NG */
		int first = IEEE80211_IOCTL_SETPARAM;
		/* NOT NEEDED: int last = IEEE80211_IOCTL_CHANLIST; */
		static const char *opnames[] = {
			"ioctl[IEEE80211_IOCTL_SETPARAM]",
			"ioctl[IEEE80211_IOCTL_GETPARAM]",
			"ioctl[IEEE80211_IOCTL_SETKEY]",
			"ioctl[SIOCIWFIRSTPRIV+3]",
			"ioctl[IEEE80211_IOCTL_DELKEY]",
			"ioctl[SIOCIWFIRSTPRIV+5]",
			"ioctl[IEEE80211_IOCTL_SETMLME]",
			"ioctl[SIOCIWFIRSTPRIV+7]",
			"ioctl[IEEE80211_IOCTL_SETOPTIE]",
			"ioctl[IEEE80211_IOCTL_GETOPTIE]",
			"ioctl[IEEE80211_IOCTL_ADDMAC]",
			"ioctl[SIOCIWFIRSTPRIV+11]",
			"ioctl[IEEE80211_IOCTL_DELMAC]",
			"ioctl[SIOCIWFIRSTPRIV+13]",
			"ioctl[IEEE80211_IOCTL_CHANLIST]",
			"ioctl[SIOCIWFIRSTPRIV+15]",
			"ioctl[IEEE80211_IOCTL_GETRSN]",
			"ioctl[SIOCIWFIRSTPRIV+17]",
			"ioctl[IEEE80211_IOCTL_GETKEY]",
		};
#endif /* MADWIFI_NG */
		int idx = op - first;
		if (first <= op && /* NOT NEEDED: op <= last && */
		    idx < (int) (sizeof(opnames) / sizeof(opnames[0])) &&
		    opnames[idx])
			perror(opnames[idx]);
		else 
#endif /* end who cares */
                {
                        int err = errno;
			perror("set80211priv ioctl failed");
                        wpa_printf(MSG_ERROR, "ioctl 0x%x failed errno=%d", 
                                op, err);
                }
		return -1;
	}
	return 0;
}

static int
set80211param(struct madwifi_driver_data *drv, int op, int arg)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.mode = op;
	memcpy(iwr.u.name+sizeof(__u32), &arg, sizeof(arg));

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		perror("ioctl[IEEE80211_IOCTL_SETPARAM]");
		wpa_printf(MSG_DEBUG, "%s: Failed to set parameter (op %d "
			   "arg %d)", __func__, op, arg);
		return -1;
	}
	return 0;
}

static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}

/*
 * Configure WPA parameters.
 */
static int
madwifi_configure_wpa(struct madwifi_driver_data *drv)
{
	struct hostapd_data *hapd = drv->hapd;
	struct hostapd_bss_config *conf = hapd->conf;
	int v;

	switch (conf->wpa_group) {
	case WPA_CIPHER_CCMP:
		v = IEEE80211_CIPHER_AES_CCM;
		break;
	case WPA_CIPHER_TKIP:
		v = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_CIPHER_WEP104:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_WEP40:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_NONE:
		v = IEEE80211_CIPHER_NONE;
		break;
	default:
		printf("Unknown group key cipher %u\n",
			conf->wpa_group);
		return -1;
	}
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: group key cipher=%d\n", __func__, v);
	if (set80211param(drv, IEEE80211_PARAM_MCASTCIPHER, v)) {
		printf("Unable to set group key cipher to %u\n", v);
		return -1;
	}
	if (v == IEEE80211_CIPHER_WEP) {
		/* key length is done only for specific ciphers */
		v = (conf->wpa_group == WPA_CIPHER_WEP104 ? 13 : 5);
		if (set80211param(drv, IEEE80211_PARAM_MCASTKEYLEN, v)) {
			printf("Unable to set group key length to %u\n", v);
			return -1;
		}
	}

	v = 0;
	if (conf->wpa_pairwise & WPA_CIPHER_CCMP)
		v |= 1<<IEEE80211_CIPHER_AES_CCM;
	if (conf->wpa_pairwise & WPA_CIPHER_TKIP)
		v |= 1<<IEEE80211_CIPHER_TKIP;
	if (conf->wpa_pairwise & WPA_CIPHER_NONE)
		v |= 1<<IEEE80211_CIPHER_NONE;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: pairwise key ciphers=0x%x\n", __func__, v);
	if (set80211param(drv, IEEE80211_PARAM_UCASTCIPHERS, v)) {
		printf("Unable to set pairwise key ciphers to 0x%x\n", v);
		return -1;
	}

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: key management algorithms=0x%x\n",
		__func__, conf->wpa_key_mgmt);
	if (set80211param(drv, IEEE80211_PARAM_KEYMGTALGS, conf->wpa_key_mgmt)) {
		printf("Unable to set key management algorithms to 0x%x\n",
			conf->wpa_key_mgmt);
		return -1;
	}

	v = 0;
	if (conf->rsn_preauth)
		v |= BIT(0);
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: rsn capabilities=0x%x\n", __func__, conf->rsn_preauth);
	if (set80211param(drv, IEEE80211_PARAM_RSNCAPS, v)) {
		printf("Unable to set RSN capabilities to 0x%x\n", v);
		return -1;
	}

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: enable WPA=0x%x\n", __func__, conf->wpa);
	if (set80211param(drv, IEEE80211_PARAM_WPA, conf->wpa)) {
		printf("Unable to set WPA to %u\n", conf->wpa);
		return -1;
	}
	return 0;
}


static int
madwifi_set_iface_flags(void *priv, int dev_up)
{
	struct madwifi_driver_data *drv = priv;
	struct ifreq ifr;

	wpa_printf(MSG_DEBUG, "%s: dev_up=%d", __func__, dev_up);

	if (drv->ioctl_sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", drv->iface);

	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		return -1;
	}

	if (dev_up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(drv->ioctl_sock, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCSIFFLAGS]");
		return -1;
	}

	if (dev_up) {
		memset(&ifr, 0, sizeof(ifr));
		snprintf(ifr.ifr_name, IFNAMSIZ, "%s", drv->iface);
		ifr.ifr_mtu = HOSTAPD_MTU;
		if (ioctl(drv->ioctl_sock, SIOCSIFMTU, &ifr) != 0) {
			perror("ioctl[SIOCSIFMTU]");
			printf("Setting MTU failed - trying to survive with "
			       "current value\n");
		}
	}

	return 0;
}

static int
madwifi_set_ieee8021x(const char *ifname, void *priv, int enabled)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct hostapd_bss_config *conf = hapd->conf;
	struct hostapd_wep_keys wep = conf->ssid.wep;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE,
		"%s: enabled=%d\n", __func__, enabled);

	if (!enabled) {
    	/* Set interface up flags after setting authentication modes,
    	   done atlast in madwifi_commit()
    	*/
#if 0 
		/* XXX restore state */
	#ifdef EAP_WPS 
		if (0 != madwifi_set_iface_flags(priv, 1))
			return -1;
	#endif /* EAP_WPS */
#endif
		if(conf->auth_algs==1 && wep.keys_set==0)
			return set80211param(priv, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_OPEN);
		else if(conf->auth_algs==1 && wep.keys_set==1){
			set80211param(priv, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_OPEN);
			/*Fix for open wep  when run using hostapd*/
			return set80211param(priv, IEEE80211_PARAM_PRIVACY, 1);			
		}		
                else if(conf->auth_algs==2)
			return set80211param(priv, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_SHARED);
		else return set80211param(priv, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_AUTO);
	}
	if (!conf->wpa && !conf->ieee802_1x) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_DRIVER,
			HOSTAPD_LEVEL_WARNING, "No 802.1X or WPA enabled!");
		return -1;
	}
	if (conf->wpa && madwifi_configure_wpa(drv) != 0) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_DRIVER,
			HOSTAPD_LEVEL_WARNING, "Error configuring WPA state!");
		return -1;
	}
	if (set80211param(priv, IEEE80211_PARAM_AUTHMODE,
		(conf->wpa ?  IEEE80211_AUTH_WPA : IEEE80211_AUTH_8021X))) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_DRIVER,
			HOSTAPD_LEVEL_WARNING, "Error enabling WPA/802.1X!");
		return -1;
	}

	return 0;
}

static int
madwifi_set_privacy(const char *ifname, void *priv, int enabled)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: enabled=%d\n", __func__, enabled);

	return set80211param(priv, IEEE80211_PARAM_PRIVACY, enabled);
}

static int
madwifi_set_sta_authorized(void *priv, const u8 *addr, int authorized)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_mlme mlme;
	int ret;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE,
		"%s: addr=%s authorized=%d\n",
		__func__, ether_sprintf(addr), authorized);

	if (authorized)
		mlme.im_op = IEEE80211_MLME_AUTHORIZE;
	else
		mlme.im_op = IEEE80211_MLME_UNAUTHORIZE;
	mlme.im_reason = 0;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(priv, IEEE80211_IOCTL_SETMLME, &mlme,
			   sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to %sauthorize STA " MACSTR,
			   __func__, authorized ? "" : "un", MAC2STR(addr));
	}

	return ret;
}

static int
madwifi_sta_set_flags(void *priv, const u8 *addr, int flags_or, int flags_and)
{
	/* For now, only support setting Authorized flag */
	if (flags_or & WLAN_STA_AUTHORIZED)
		return madwifi_set_sta_authorized(priv, addr, 1);
	if (!(flags_and & WLAN_STA_AUTHORIZED))
		return madwifi_set_sta_authorized(priv, addr, 0);
	return 0;
}

static int
madwifi_del_key(void *priv, const u8 *addr, int key_idx)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_del_key wk;
	int ret;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: addr=%s key_idx=%d\n",
		__func__, ether_sprintf(addr), key_idx);

	memset(&wk, 0, sizeof(wk));
	if (addr != NULL) {
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (u8) IEEE80211_KEYIX_NONE;
	} else {
		wk.idk_keyix = key_idx;
	}

	ret = set80211priv(priv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to delete key (addr %s"
			   " key_idx %d)", __func__, ether_sprintf(addr),
			   key_idx);
	}

	return ret;
}

static int
madwifi_set_key(const char *ifname, void *priv, const char *alg,
		const u8 *addr, int key_idx,
		const u8 *key, size_t key_len, int txkey)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_key wk;
	u_int8_t cipher;
	int ret;

	if (strcmp(alg, "none") == 0)
		return madwifi_del_key(priv, addr, key_idx);

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: alg=%s addr=%s key_idx=%d\n",
		__func__, alg, ether_sprintf(addr), key_idx);

	if (strcmp(alg, "WEP") == 0)
#ifndef MODIFIED_BY_SONY
		cipher = IEEE80211_CIPHER_WEP;
#else /* MODIFIED_BY_SONY */
		if (!hapd->conf->ieee802_1x && (addr == NULL || os_memcmp(addr, "\xff\xff\xff\xff\xff\xff",
                                              ETH_ALEN) == 0))
                        /*
                         * madwifi did not seem to like static WEP key
                         * configuration with IEEE80211_IOCTL_SETKEY, so use
                         * Linux wireless extensions ioctl for this.
                         */

			return wext_set_key(priv, WPA_ALG_WEP,
							addr, key_idx, txkey, NULL, 0, key, key_len);
		else
			cipher = IEEE80211_CIPHER_WEP;
#endif /* MODIFIED_BY_SONY */
	else if (strcmp(alg, "TKIP") == 0)
		cipher = IEEE80211_CIPHER_TKIP;
	else if (strcmp(alg, "CCMP") == 0)
		cipher = IEEE80211_CIPHER_AES_CCM;
	else {
		printf("%s: unknown/unsupported algorithm %s\n",
			__func__, alg);
		return -1;
	}

	if (key_len > sizeof(wk.ik_keydata)) {
		printf("%s: key length %lu too big\n", __func__,
		       (unsigned long) key_len);
		return -3;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT;
	if (addr == NULL) {
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_keyix = key_idx;
		wk.ik_flags |= IEEE80211_KEY_DEFAULT;
	} else {
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.ik_keyix = IEEE80211_KEYIX_NONE;
	}
	wk.ik_keylen = key_len;
	memcpy(wk.ik_keydata, key, key_len);

	ret = set80211priv(priv, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to set key (addr %s"
			   " key_idx %d alg '%s' key_len %lu txkey %d)",
			   __func__, ether_sprintf(wk.ik_macaddr), key_idx,
			   alg, (unsigned long) key_len, txkey);
	}

	return ret;
}


static int
madwifi_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx,
		   u8 *seq)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_key wk;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: addr=%s idx=%d\n", __func__, ether_sprintf(addr), idx);

	memset(&wk, 0, sizeof(wk));
	if (addr == NULL)
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
	else
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	wk.ik_keyix = idx;

	if (set80211priv(priv, IEEE80211_IOCTL_GETKEY, &wk, sizeof(wk))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get encryption data "
			   "(addr " MACSTR " key_idx %d)",
			   __func__, MAC2STR(wk.ik_macaddr), idx);
		return -1;
	}

#ifdef WORDS_BIGENDIAN
	{
		/*
		 * wk.ik_keytsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
		u8 tmp[WPA_KEY_RSC_LEN];
		memcpy(tmp, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
		for (i = 0; i < WPA_KEY_RSC_LEN; i++) {
			seq[i] = tmp[WPA_KEY_RSC_LEN - i - 1];
		}
	}
#else /* WORDS_BIGENDIAN */
	memcpy(seq, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
#endif /* WORDS_BIGENDIAN */
	return 0;
}


static int 
madwifi_flush(void *priv)
{
#ifdef MADWIFI_BSD
	u8 allsta[IEEE80211_ADDR_LEN];
	memset(allsta, 0xff, IEEE80211_ADDR_LEN);
	return madwifi_sta_deauth(priv, allsta, IEEE80211_REASON_AUTH_LEAVE);
#else /* MADWIFI_BSD */
	return 0;		/* XXX */
#endif /* MADWIFI_BSD */
}


static int
madwifi_read_sta_driver_data(void *priv, struct hostap_sta_driver_data *data,
			     const u8 *addr)
{
	struct madwifi_driver_data *drv = priv;

#ifdef MADWIFI_BSD
	struct ieee80211req_sta_stats stats;

	memset(data, 0, sizeof(*data));

	/*
	 * Fetch statistics for station from the system.
	 */
	memset(&stats, 0, sizeof(stats));
	memcpy(stats.is_u.macaddr, addr, IEEE80211_ADDR_LEN);
	if (set80211priv(drv,
#ifdef MADWIFI_NG
			 IEEE80211_IOCTL_STA_STATS,
#else /* MADWIFI_NG */
			 IEEE80211_IOCTL_GETSTASTATS,
#endif /* MADWIFI_NG */
			 &stats, sizeof(stats))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to fetch STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
		if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
			memcpy(data, &drv->acct_data, sizeof(*data));
			return 0;
		}

		printf("Failed to get station stats information element.\n");
		return -1;
	}

	data->rx_packets = stats.is_stats.ns_rx_data;
	data->rx_bytes = stats.is_stats.ns_rx_bytes;
	data->tx_packets = stats.is_stats.ns_tx_data;
	data->tx_bytes = stats.is_stats.ns_tx_bytes;
	return 0;

#else /* MADWIFI_BSD */

	char buf[1024], line[128], *pos;
	FILE *f;
	unsigned long val;

	memset(data, 0, sizeof(*data));
	snprintf(buf, sizeof(buf), "/proc/net/madwifi/%s/" MACSTR,
		 drv->iface, MAC2STR(addr));

	f = fopen(buf, "r");
	if (!f) {
		if (memcmp(addr, drv->acct_mac, ETH_ALEN) != 0)
			return -1;
		memcpy(data, &drv->acct_data, sizeof(*data));
		return 0;
	}
	/* Need to read proc file with in one piece, so use large enough
	 * buffer. */
	setbuffer(f, buf, sizeof(buf));

	while (fgets(line, sizeof(line), f)) {
		pos = strchr(line, '=');
		if (!pos)
			continue;
		*pos++ = '\0';
		val = strtoul(pos, NULL, 10);
		if (strcmp(line, "rx_packets") == 0)
			data->rx_packets = val;
		else if (strcmp(line, "tx_packets") == 0)
			data->tx_packets = val;
		else if (strcmp(line, "rx_bytes") == 0)
			data->rx_bytes = val;
		else if (strcmp(line, "tx_bytes") == 0)
			data->tx_bytes = val;
	}

	fclose(f);

	return 0;
#endif /* MADWIFI_BSD */
}


static int
madwifi_sta_clear_stats(void *priv, const u8 *addr)
{
#if defined(MADWIFI_BSD) && defined(IEEE80211_MLME_CLEAR_STATS)
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_mlme mlme;
	int ret;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "%s: addr=%s\n",
		      __func__, ether_sprintf(addr));

	mlme.im_op = IEEE80211_MLME_CLEAR_STATS;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(priv, IEEE80211_IOCTL_SETMLME, &mlme,
			   sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to clear STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
	}

	return ret;
#else /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
	return 0; /* FIX */
#endif /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
}


static int
madwifi_set_opt_ie(const char *ifname, void *priv, const u8 *ie, size_t ie_len)
{
	/*
	 * Do nothing; we setup parameters at startup that define the
	 * contents of the beacon information element.
	 */
	return 0;
}

static int
madwifi_is_ifup(struct madwifi_driver_data *drv)
{
	struct ifreq ifr;

	if (drv->ioctl_sock < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", drv->iface);

	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		return 0;
	}

	return ((ifr.ifr_flags & IFF_UP) == IFF_UP);
}

static int
madwifi_sta_deauth(void *priv, const u8 *addr, int reason_code)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_mlme mlme;
	int ret;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: addr=%s reason_code=%d\n",
		__func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	if (!madwifi_is_ifup(drv)) {
		return EINVAL;
	}
	ret = set80211priv(priv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to deauth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int
madwifi_sta_disassoc(void *priv, const u8 *addr, int reason_code)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_mlme mlme;
	int ret;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		"%s: addr=%s reason_code=%d\n",
		__func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(priv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disassoc STA (addr "
			   MACSTR " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int
madwifi_del_sta(struct madwifi_driver_data *drv, u8 addr[IEEE80211_ADDR_LEN])
{
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		HOSTAPD_LEVEL_INFO, "disassociated");

	sta = ap_get_sta(hapd, addr);
	if (sta != NULL) {
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
		wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
		sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
		ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
		ap_free_sta(hapd, sta);
	}
	return 0;
}

static int
madwifi_process_wpa_ie(struct madwifi_driver_data *drv, struct sta_info *sta)
{
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_wpaie ie;
	int ielen, res;
	u8 *iebuf = NULL;
#ifdef EAP_WPS
	struct hostapd_bss_config *conf = hapd->conf;
#endif /* EAP_WPS */

	/*
	 * Fetch negotiated WPA/RSN parameters from the system.
	 */
	memset(&ie, 0, sizeof(ie));
	memcpy(ie.wpa_macaddr, sta->addr, IEEE80211_ADDR_LEN);
	if (set80211priv(drv, IEEE80211_IOCTL_GETWPAIE, &ie, sizeof(ie))) {
		wpa_printf(MSG_ERROR, "%s: Failed to get WPA/RSN IE",
			   __func__);
		printf("Failed to get WPA/RSN information element.\n");
		return -1;		/* XXX not right */
	}
#ifndef EAP_WPS
	iebuf = ie.wpa_ie;
#ifdef MADWIFI_NG
	if (iebuf[1] == 0 && ie.rsn_ie[1] > 0) {
		/* madwifi-ng svn #1453 added rsn_ie. Use it, if wpa_ie was not
		 * set. This is needed for WPA2. */
		iebuf = ie.rsn_ie;
	}
#endif /* MADWIFI_NG */
	ielen = iebuf[1];
	if (ielen == 0) {
		printf("No WPA/RSN information element for station!?\n");
		return -1;		/* XXX not right */
	}
	ielen += 2;
#else /* EAP_WPS */
	do {
		iebuf = 0; ielen = 0;
		if (conf->wpa & HOSTAPD_WPA_VERSION_WPA) {
			iebuf = ie.wpa_ie; ielen = 0;
			if ((iebuf[0] == WLAN_EID_GENERIC) && iebuf[1]) {
				ielen = iebuf[1];
				break;
			}
		}
#ifdef MADWIFI_NG
		if (conf->wpa & HOSTAPD_WPA_VERSION_WPA2) {
			iebuf = ie.rsn_ie; ielen = 0;
			if ((iebuf[0] == WLAN_EID_RSN) && iebuf[1]) {
				ielen = iebuf[1];
				break;
			}
		}
#endif /* MADWIFI_NG */
	} while (0);

	if ((ie.wps_ie[0] == WLAN_EID_VENDOR_SPECIFIC) && ie.wps_ie[1]) {
		os_memcpy(sta->wps_ie, ie.wps_ie, ie.wps_ie[1] + 2);
		sta->wps_ie_len = ie.wps_ie[1] + 2;
                /* Per WPS spec:
                 * A client that intends to use the EAP-WSC method 
                 * with a WSC enabled AP may include a WSC IE in its
                 * 802.11 (re)association request. 
                 * If a WSC IE is present in the (re)association request, 
                 * the AP shall engage in EAP-WSC with the station 
                 * and must not attempt other security handshake.
                 */
                ielen = 0;      /* IGNORE wpa/rsn ie! */
	} else {
                os_memset(sta->wps_ie, 0, sizeof(sta->wps_ie));
                sta->wps_ie_len = 0;
        }
#endif /* EAP_WPS */

	if (sta->wpa_sm == NULL)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr);
	if (sta->wpa_sm == NULL) {
		printf("Failed to initialize WPA state machine\n");
		return -1;
	}

#ifdef EAP_WPS
	if (!ielen) {
                #if 0   /* I don't know the purpose of this code from Sony -Ted*/
                /* how do we know we want WPS?
                 * This seems to assume that the absence of valid
                 * wpa_ie and rsn_ie implies that we want WPS or
                 * what does it mean?
                 */
		(void)wpa_wps_prepare(sta->wpa_sm);
		printf("driver_madwifi: Prepared EAP-WPS\n");
                #endif
	} else {
		ielen += 2;
#endif /* EAP_WPS */
	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
				  iebuf, ielen);
	if (res != WPA_IE_OK) {
		printf("WPA/RSN information element rejected? (res %u)\n", res);
		return -1;
	}
#ifdef EAP_WPS
	}
#endif /* EAP_WPS */

	return 0;
}

static int
madwifi_new_sta(struct madwifi_driver_data *drv, u8 addr[IEEE80211_ADDR_LEN])
{
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;
	int new_assoc;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		HOSTAPD_LEVEL_INFO, "associated");

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		accounting_sta_stop(hapd, sta);
	} else {
		sta = ap_sta_add(hapd, addr);
		if (sta == NULL)
			return -1;
	}

	if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		memset(drv->acct_mac, 0, ETH_ALEN);
		memset(&drv->acct_data, 0, sizeof(drv->acct_data));
	}
	accounting_sta_get_id(hapd, sta);

	if (hapd->conf->wpa) {
		if (madwifi_process_wpa_ie(drv, sta))
			return -1;
	}

	/*
	 * Now that the internal station state is setup
	 * kick the authenticator into action.
	 */
	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
	hostapd_new_assoc_sta(hapd, sta, !new_assoc);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);
	return 0;
}

static void
madwifi_wireless_event_wireless_custom(struct madwifi_driver_data *drv,
				       char *custom)
{
	struct hostapd_data *hapd = drv->hapd;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Custom wireless event: '%s'\n",
		      custom);

	if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		u8 addr[ETH_ALEN];
		pos = strstr(custom, "addr=");
		if (pos == NULL) {
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "MLME-MICHAELMICFAILURE.indication "
				      "without sender address ignored\n");
			return;
		}
		pos += 5;
		if (hwaddr_aton(pos, addr) == 0) {
			ieee80211_michael_mic_failure(drv->hapd, addr, 1);
		} else {
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "MLME-MICHAELMICFAILURE.indication "
				      "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u32 val;
		key = custom;
		while ((key = strchr(key, '\n')) != NULL) {
			key++;
			value = strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (strcmp(key, "mac") == 0)
				hwaddr_aton(value, drv->acct_mac);
			else if (strcmp(key, "rx_packets") == 0)
				drv->acct_data.rx_packets = val;
			else if (strcmp(key, "tx_packets") == 0)
				drv->acct_data.tx_packets = val;
			else if (strcmp(key, "rx_bytes") == 0)
				drv->acct_data.rx_bytes = val;
			else if (strcmp(key, "tx_bytes") == 0)
				drv->acct_data.tx_bytes = val;
			key = value;
		}
	}
        #ifdef EAP_WPS
	else if (strncmp(custom, "PUSH-BUTTON.indication", 22) == 0) {
                /* Some atheros kernels send push button as a wireless event*/
                /* PROBLEM! this event is received for ALL BSSs ...
                 * so all are enabled for WPS... ugh.
                 */
                struct eap_wps_enable_params params = {};
                params.dev_pwd = (u8 *)"00000000"; /* for push button method */
                params.dev_pwd_len = 8;
                params.config_who = WPS_CONFIG_WHO_THEM;
                wpa_printf(MSG_INFO, "WPS push button pressed for if:%s",drv->iface);
                if (eap_wps_enable(hapd, hapd->conf->wps, &params)) {
                        wpa_printf(MSG_ERROR, "Push button failed to enable WPS");
                }
    	} 
        else if (strncmp(custom, "Manage.prob_req",15) == 0) {
                /* Atheros driver specially modified to pass probe requests
                 * this way.  The old way (using packet sniffing)
                 * didn't work when bridging!
                 */
                #define WPS_FRAM_TAG_SIZE 30    /* hardcoded in driver */
                char tag[WPS_FRAM_TAG_SIZE];
                int len;
                sscanf(custom, "%s %d", tag,&len);
                madwifi_handle_mgmt_frames(drv, NULL, 
                        (u8 *)custom+WPS_FRAM_TAG_SIZE, len);
        }
        #endif  /* EAP_WPS */
}

static void
madwifi_wireless_event_wireless(struct madwifi_driver_data *drv,
					    u8 *data, int len)
{
	struct hostapd_data *hapd = drv->hapd;
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	u8 *pos, *end, *custom, *buf;
        u8 macaddr[ETH_ALEN] = {};

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE, "Wireless event: "
			      "cmd=0x%x len=%d\n", iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (drv->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
                     iwe->cmd == SIOCGIWESSID ||
                     iwe->cmd == SIOCGIWENCODE ||
                     iwe->cmd == IWEVGENIE ||
		     iwe->cmd == IWEVASSOCREQIE ||
		     iwe->cmd == IWEVASSOCRESPIE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case SIOCGIWAP:
			os_memcpy(macaddr, iwe->u.ap_addr.sa_data, ETH_ALEN);
                        break;
		case IWEVEXPIRED:
			madwifi_del_sta(drv, (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			madwifi_new_sta(drv, (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVCUSTOM:
		case IWEVASSOCREQIE:    /* per HACK in driver, same as IWEVCUSTOM */
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			madwifi_wireless_event_wireless_custom(drv, (char *)buf);
			free(buf);
			break;
		case IWEVGENIE: {
                        /* EXPERIMENTAL -- THIS MAY NOT WORK.
                         * THE IDEA IS TO GIVE DRIVER WRITERS ANOTHER WAY
                         * TO PASS INFORMATION ELEMENTS FOR WPS
                         * (WITHOUT THE BUFFER SIZE ISSUES THAT PLAGUE
                         * IWEVCUSTOM etc.)
                         */
                        #define GENERIC_INFO_ELEM 0xdd
                        #define RSN_INFO_ELEM 0x30
	                u8 *genie, *gpos, *gend;
			gpos = genie = custom;
			gend = genie + iwe->u.data.length;
			if (gend > end) {
				wpa_printf(MSG_INFO, "IWEVGENIE overflow");
				break;
			}
			while (gpos + 1 < gend &&
			       gpos + 2 + (u8) gpos[1] <= gend) {
				u8 ie = gpos[0], ielen = gpos[1] + 2;
#ifndef EAP_WPS
				if (ielen > SSID_MAX_WPA_IE_LEN) {
					gpos += ielen;
					continue;
				}
#endif /* EAP_WPS */
				switch (ie) {
				case GENERIC_INFO_ELEM:
#ifndef EAP_WPS
					if (ielen < 2 + 4 ||
					    os_memcmp(&gpos[2],
						      "\x00\x50\xf2\x01", 4) !=
					    0)
						break;
#else /* EAP_WPS */
					if (ielen >= 2 + 4 &&
					    os_memcmp(&gpos[2],
						   "\x00\x50\xf2\x01", 4) == 0) {
#endif /* EAP_WPS */
                                                /* wpa_ie -- ignore */
#ifdef EAP_WPS
					} else if (ielen >= 2 + 4 &&
					    os_memcmp(&gpos[2],
						   "\x00\x50\xf2\x04", 4) == 0) {
                                                eap_wps_handle_mgmt_frames(
                                                        drv->hapd, macaddr, 
                                                        custom,
                                                        iwe->u.data.length,
                                                        gpos+6, ielen-6);
					}
#endif /* EAP_WPS */
					break;
				case RSN_INFO_ELEM:
					/* rsn ie -- ignore */
					break;
				}
				gpos += ielen;
			}
                }
		break;
		}

		pos += iwe->len;
	}
}


static void
madwifi_wireless_event_rtm_newlink(struct madwifi_driver_data *drv,
					       struct nlmsghdr *h, int len)
{
	struct ifinfomsg *ifi;
	int attrlen, nlmsg_len, rta_len;
	struct rtattr * attr;

	if (len < (int) sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	if (ifi->ifi_index != drv->ifindex)
		return;

	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			madwifi_wireless_event_wireless(
				drv, ((u8 *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void
madwifi_wireless_event_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	char buf[256];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	struct madwifi_driver_data *drv = eloop_ctx;

	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;
	while (left >= (int) sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			printf("Malformed netlink message: "
			       "len=%d left=%d plen=%d\n",
			       len, left, plen);
			break;
		}

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			madwifi_wireless_event_rtm_newlink(drv, h, plen);
			break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		printf("%d extra bytes in the end of netlink message\n", left);
	}
}


static int
madwifi_get_we_version(struct madwifi_driver_data *drv)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = wpa_zalloc(buflen);
	if (range == NULL)
		return -1;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}


static int
madwifi_wireless_event_init(void *priv)
{
	struct madwifi_driver_data *drv = priv;
	int s;
	struct sockaddr_nl local;

	madwifi_get_we_version(drv);

	drv->wext_sock = -1;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s < 0) {
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		return -1;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind(netlink)");
		close(s);
		return -1;
	}

	eloop_register_read_sock(s, madwifi_wireless_event_receive, drv, NULL);
	drv->wext_sock = s;

	return 0;
}


static void
madwifi_wireless_event_deinit(void *priv)
{
	struct madwifi_driver_data *drv = priv;

	if (drv != NULL) {
		if (drv->wext_sock < 0)
			return;
		eloop_unregister_read_sock(drv->wext_sock);
		close(drv->wext_sock);
	}
}


static int
madwifi_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
		   int encrypt, const u8 *own_addr)
{
	struct madwifi_driver_data *drv = priv;
	unsigned char buf[3000];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = malloc(len);
		if (bp == NULL) {
			printf("EAPOL frame discarded, cannot malloc temp "
			       "buffer of size %lu!\n", (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	memcpy(eth->h_dest, addr, ETH_ALEN);
	memcpy(eth->h_source, own_addr, ETH_ALEN);
	eth->h_proto = htons(ETH_P_EAPOL);
	memcpy(eth+1, data, data_len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", bp, len);

	status = l2_packet_send(drv->sock_xmit, addr, ETH_P_EAPOL, bp, len);

	if (bp != buf)
		free(bp);
	return status;
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct madwifi_driver_data *drv = ctx;
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, src_addr);
	if (!sta || !(sta->flags & WLAN_STA_ASSOC)) {
		printf("Data frame from not associated STA %s\n",
		       ether_sprintf(src_addr));
		/* XXX cannot happen */
		return;
	}
	ieee802_1x_receive(hapd, src_addr, buf + sizeof(struct l2_ethhdr),
			   len - sizeof(struct l2_ethhdr));
}

static int
madwifi_init(struct hostapd_data *hapd)
{
	struct madwifi_driver_data *drv;
	struct ifreq ifr;
	struct iwreq iwr;
#if EAP_WPS
#ifndef USE_INTEL_SDK
	int ret = -1;
	u8 *iebuf = 0;
	size_t iebuflen;
	struct wps_config * wps;
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */


	drv = wpa_zalloc(sizeof(struct madwifi_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for madwifi driver data\n");
		goto bad;
	}

	drv->ops = madwifi_driver_ops;
	drv->hapd = hapd;
	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		goto bad;
	}
	memcpy(drv->iface, hapd->conf->iface, sizeof(drv->iface));

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", drv->iface);
	if (ioctl(drv->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		goto bad;
	}
	drv->ifindex = ifr.ifr_ifindex;

	drv->sock_xmit = l2_packet_init(drv->iface, NULL, ETH_P_EAPOL,
					handle_read, drv, 1);
	if (drv->sock_xmit == NULL)
		goto bad;
	if (l2_packet_get_own_addr(drv->sock_xmit, hapd->own_addr))
		goto bad;
	if (hapd->conf->bridge[0] != '\0') {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			"Configure bridge %s for EAPOL traffic.\n",
			hapd->conf->bridge);
		drv->sock_recv = l2_packet_init(hapd->conf->bridge, NULL,
						ETH_P_EAPOL, handle_read, drv,
						1);
		if (drv->sock_recv == NULL)
			goto bad;
	} else
		drv->sock_recv = drv->sock_xmit;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	iwr.u.mode = IW_MODE_MASTER;

	if (ioctl(drv->ioctl_sock, SIOCSIWMODE, &iwr) < 0) {
		perror("ioctl[SIOCSIWMODE]");
		printf("Could not set interface to master mode!\n");
		goto bad;
	}

	madwifi_set_iface_flags(drv, 0);	/* mark down during setup */
	madwifi_set_privacy(drv->iface, drv, 0); /* default to no privacy */

	hapd->driver = &drv->ops;

#if EAP_WPS
#ifndef USE_INTEL_SDK
	wps = hapd->conf->wps;
        if (wps == NULL ) {
                wpa_printf(MSG_INFO, "WPS not configured on");
        } else
	do {
		memcpy(wps->mac, hapd->own_addr, sizeof(wps->mac));
		wps->mac_set = 1;

                if (wps->uuid_set == 0) {
                        /* The following invents a uuid in rough compliance
                         * with rfc4122 based on mac address.
                         */
                        memset(wps->uuid, 0, sizeof(wps->uuid));
                        wps->uuid[6] = (1<<4);  /* for mac-based address */
                        memcpy(wps->uuid+SIZE_UUID-6, wps->mac, 6);
                        wps->uuid_set = 1;
                        wpa_printf(MSG_INFO, 
"Defaulted uuid based on mac addr %02x:%02x:%02x:%02x:%02x:%02x",
                            wps->mac[0], wps->mac[1], wps->mac[2],
                            wps->mac[3], wps->mac[4], wps->mac[5]);
                }

                if (wps->wps_disable) {
		        if (madwifi_set_wps_beacon_ie(drv, (u8 *)"", 0)) {
			        perror("Clear WPS Beacon IE error");
			        break;
		        }
                } else {
		        /* Create WPS Beacon IE */
		        if (wps_config_create_beacon_ie(drv->hapd, &iebuf, &iebuflen)) {
			        perror("Create WPS Beacon IE error");
			        break;
		        }
		        /* Set WPS Beacon IE */
		        if (madwifi_set_wps_beacon_ie(drv, iebuf, iebuflen)) {
			        perror("Set WPS Beacon IE error");
			        break;
		        }
		        os_free(iebuf);
		        iebuf = 0;
                }
                if (wps->wps_disable) {
		        if (madwifi_set_wps_probe_resp_ie(drv, (u8 *)"", 0)) {
			        perror("Set WPS ProbeResp IE error");
			        break;
		        }
                } else {
		        if (wps_config_create_probe_resp_ie(drv->hapd, &iebuf, &iebuflen)) {
			        wpa_printf(MSG_ERROR, "Create WPS ProbeResp IE error");
			        /* WAS: break; */
                                /* The error is most likely due to missing config;
                                * continue on anyway without WPS functionality.
                                */
		        } else
		        if (madwifi_set_wps_probe_resp_ie(drv, iebuf, iebuflen)) {
			        perror("Set WPS ProbeResp IE error");
			        break;
		        }
		        os_free(iebuf);
		        iebuf = 0;
                }
                #if WPS_DO_ASSOC_RESP_IE
                if (wps->wps_disable) {
                #endif
		        if (madwifi_set_wps_assoc_resp_ie(drv, (u8 *)"", 0)) {
			        perror("Clear WPS AssocResp IE error");
			        break;
		        }

                #if WPS_DO_ASSOC_RESP_IE
                } else  {
                        /* This is reported to break some clients; is required?
                        * only if client uses WPS association request (uncommon?).
                        */
		        if (wps_config_create_assoc_resp_ie(drv->hapd, &iebuf, &iebuflen)) {
			        wpa_printf(MSG_ERROR, "Create WPS AssocResp IE error");
			        /* WAS: break; */
                                /* The error is most likely due to missing config;
                                * continue on anyway without WPS functionality.
                                */
		        } else
		        if (madwifi_set_wps_assoc_resp_ie(drv, iebuf, iebuflen)) {
			        perror("Set WPS AssocResp IE error");
			        break;
		        }
                }
                #endif  /* WPS_DO_ASSOC_RESP_IE */

                #if 0   /* OLD METHOD, does not work when bridging! */
                /* Capture management frames so we can detect WPS conflicts
                 * ARGH! The eth type 0x19 is obsolete and can conflict
                 * with HDLC which is officially assigned to use this.
                 * It is apparently being replaced with ETH_P_802_2 (== 4)
                 * but need more details...
                 * madwifi drivers will probably continue to be hardcoded
                 * to use 0x19 but if we switch to ath5k then that may
                 * be a differentstory.
                 */
	        if (hapd->conf->bridge[0] != '\0') {
		        HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			        "Configure bridge %s for probe req traffic.\n",
			        hapd->conf->bridge);
		        drv->probe_recv = l2_packet_init(
                                hapd->conf->bridge, NULL,
                                0x0019/* the old ETH_P_80211_RAW */,
                                madwifi_handle_mgmt_frames, drv, 1);
	        } else {
	                drv->probe_recv = l2_packet_init(drv->iface, NULL, 
                                0x0019/* the old ETH_P_80211_RAW */,
                                madwifi_handle_mgmt_frames, drv, 1);
                }
	        if (drv->probe_recv == NULL) {
		        break;
                }
                #endif
                /* This selects the management frames we receive.
                 * We handle only probe requests.
                 * The WPS spec suggests that perhaps we should handle
                 * association requests as well, but this does not seem
                 * to be mandatory.
                 */
                madwifi_start_receive_prob_req(drv);

		ret = 0;
	} while (0);

	if (iebuf)
		os_free(iebuf);

	if (ret)
		goto bad;
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

	return 0;
bad:
        wpa_printf(MSG_ERROR, "madwifi_init failed!");
#if EAP_WPS
#ifndef USE_INTEL_SDK
#endif
#endif
        #if 0   /* was */
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv != NULL)
		free(drv);
        #else
	if (drv != NULL)
                madwifi_deinit(drv);
        #endif
	return -1;
}


static void
madwifi_deinit(void *priv)
{
	struct madwifi_driver_data *drv = priv;

	drv->hapd->driver = NULL;

	(void) madwifi_set_iface_flags(drv, 0);
        #if 0   /* OLD */
	if (drv->probe_recv != NULL)
		l2_packet_deinit(drv->probe_recv);
        #endif
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv->sock_recv != NULL && drv->sock_recv != drv->sock_xmit)
		l2_packet_deinit(drv->sock_recv);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	free(drv);
}

static int
madwifi_set_ssid(const char *ifname, void *priv, const u8 *buf, int len)
{
	struct madwifi_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}
	return 0;
}

static int
madwifi_get_ssid(const char *ifname, void *priv, u8 *buf, int len)
{
	struct madwifi_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len;

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else
		ret = iwr.u.essid.length;

	return ret;
}

static int
madwifi_set_countermeasures(void *priv, int enabled)
{
	struct madwifi_driver_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_COUNTERMEASURES, enabled);
}

static int
madwifi_commit(void *priv)
{
	return madwifi_set_iface_flags(priv, 1);
}

#ifdef EAP_WPS
static int
madwifi_set_wps_ie(void *priv, u8 *iebuf, int iebuflen, u32 frametype)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	u8 buf[256];
	struct ieee80211req_getset_appiebuf * ie;
	// int i;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "%s buflen = %d\n", 
			__func__, iebuflen);

	ie = (struct ieee80211req_getset_appiebuf *) buf;
	ie->app_frmtype = frametype;
	ie->app_buflen = iebuflen;
        if (iebuflen > 0)
	        os_memcpy(&(ie->app_buf[0]), iebuf, iebuflen);
	
	return set80211priv(priv, IEEE80211_IOCTL_SET_APPIEBUF, ie,
			sizeof(struct ieee80211req_getset_appiebuf) + iebuflen);
}

static int
madwifi_set_wps_beacon_ie(void *priv, u8 *iebuf, int iebuflen)
{
	return madwifi_set_wps_ie(priv, iebuf, iebuflen, 
			IEEE80211_APPIE_FRAME_BEACON);
}

static int
madwifi_set_wps_probe_resp_ie(void *priv, u8 *iebuf, int iebuflen)
{
	return madwifi_set_wps_ie(priv, iebuf, iebuflen, 
			IEEE80211_APPIE_FRAME_PROBE_RESP);
}


/* Ask to receive copies of all probe requests received.
 */
static int
madwifi_start_receive_prob_req(void *priv)
{
	struct ieee80211req_set_filter filt;

	wpa_printf(MSG_DEBUG, "%s Enter\n", __FUNCTION__);
	filt.app_filterype = IEEE80211_FILTER_TYPE_PROBE_REQ;

	return set80211priv(priv, IEEE80211_IOCTL_FILTERFRAME, &filt,
            			sizeof(struct ieee80211req_set_filter));
}

#if EAP_WPS
/* Handle management frames received:
 * (called back from l2_packet module):
 */
static void
madwifi_handle_mgmt_frames(
        void *ctx,                      /* struct madwifi_driver_data * */
        const unsigned char *src_addr,  /* do NOT use this */
        const unsigned char *buf,       /* raw packet */
        size_t len                      /* size of raw packet */
        )
{
        int packet_type;
        int packet_sub_type;
	u8 *ie;
        int ie_len;
	u8 *next_ie;
	u8 *endfrm;
	struct ieee80211_frame *wh;     /* the header */
        struct madwifi_driver_data *drv = ctx;

        /* Begins with ieee802.11 header, beginning with:
         *      FC0 byte: version, type and subtype
         *      FC1 byte: various flag bits.
         *      3 and possibly four addresses and a sequence no.
         */
        if (len < sizeof(*wh)) {
                return;
        }
        endfrm = (u8 *) (buf + len);
        packet_type = (buf[0] & IEEE80211_FC0_TYPE_MASK);
        packet_sub_type = (buf[0] & IEEE80211_FC0_SUBTYPE_MASK);
        if (packet_type != IEEE80211_FC0_TYPE_MGT) {
                return;
        }
        if (packet_sub_type != IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
                /* For now at least, only probe requests are expected */
                return;
        }
        /* Management frames always have a 3 address format,
         * for which struct ieee80211_frame is appropriate.
         * (addresses are: dest, src, bssid).
         */
    	wh = (struct ieee80211_frame *) buf;
        /* provided src_addr seems to be bogus.
         * Use wh->i_addr2 instead.
         */
        src_addr = wh->i_addr2;

        /* Following the header are information elements 
         * Search for the WPS information element...
         */
        for (ie = (u8 *)&wh[1]; ie <= endfrm-2 && 
                        (next_ie = (ie+(ie_len = ie[1]+2))) <= endfrm; 
                        ie = next_ie) {
                if (ie_len >= 6 && 
                                ie[0] == IEEE80211_ELEMID_VENDOR && 
		    	        // check for WFA-WSC OUI
			        ie[2] == 0x00 && ie[3] == 0x50 && 
                                ie[4] == 0xF2 && ie[5]==0x04) {
                        /* Contains WPS info elements which should be
                         * processed
                         */
                        eap_wps_handle_mgmt_frames(drv->hapd, src_addr, 
                                buf, len,
                                ie+6, ie_len-6);
		}
	}
       
	return;
}
#endif  /* EAP_WPS */

static int
madwifi_set_wps_assoc_resp_ie(void *priv, u8 *iebuf, int iebuflen)
{
	return madwifi_set_wps_ie(priv, iebuf, iebuflen, 
			IEEE80211_APPIE_FRAME_ASSOC_RESP);
}
#endif /* EAP_WPS */

static const struct driver_ops madwifi_driver_ops = {
	.name			= "madwifi",
	.init			= madwifi_init,
	.deinit			= madwifi_deinit,
	.set_ieee8021x		= madwifi_set_ieee8021x,
	.set_privacy		= madwifi_set_privacy,
	.set_encryption		= madwifi_set_key,
	.get_seqnum		= madwifi_get_seqnum,
	.flush			= madwifi_flush,
	.set_generic_elem	= madwifi_set_opt_ie,
	.wireless_event_init	= madwifi_wireless_event_init,
	.wireless_event_deinit	= madwifi_wireless_event_deinit,
	.sta_set_flags		= madwifi_sta_set_flags,
	.read_sta_data		= madwifi_read_sta_driver_data,
	.send_eapol		= madwifi_send_eapol,
	.sta_disassoc		= madwifi_sta_disassoc,
	.sta_deauth		= madwifi_sta_deauth,
	.set_ssid		= madwifi_set_ssid,
	.get_ssid		= madwifi_get_ssid,
	.set_countermeasures	= madwifi_set_countermeasures,
	.sta_clear_stats        = madwifi_sta_clear_stats,
	.commit			= madwifi_commit,
#ifdef EAP_WPS
	.set_wps_beacon_ie		= madwifi_set_wps_beacon_ie,
	.set_wps_probe_resp_ie	= madwifi_set_wps_probe_resp_ie,
	.set_wps_assoc_resp_ie	= madwifi_set_wps_assoc_resp_ie,
#endif /* EAP_WPS */
};

void madwifi_driver_register(void)
{
	driver_register(madwifi_driver_ops.name, &madwifi_driver_ops);
}

#ifdef MODIFIED_BY_SONY
int wext_set_key(void *priv, int alg,
				 const u8 *addr, int key_idx,
				 int set_tx, const u8 *seq, size_t seq_len,
				 const u8 *key, size_t key_len)
{
	struct madwifi_driver_data *drv = priv;
	struct hostapd_data *hapd = drv->hapd;
	struct hostapd_bss_config *conf = hapd->conf;
	struct iwreq iwr;
	int ret = 0;
	int ioctl_sock;

	wpa_printf(MSG_DEBUG,"%s: alg=%d key_idx=%d set_tx=%d seq_len=%lu "
		   "key_len=%lu", __FUNCTION__, alg, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (ioctl_sock < 0) {
		perror("socket(PF_INET,SOCK_DGRAM)");
		return -1;
	}

	os_memset(&iwr, 0, sizeof(iwr));
	os_strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.encoding.flags = key_idx + 1;
	if (alg == WPA_ALG_NONE)
		iwr.u.encoding.flags |= IW_ENCODE_DISABLED;
	if (conf->auth_algs & HOSTAPD_AUTH_OPEN)
		iwr.u.encoding.flags |= IW_ENCODE_OPEN;
	if (conf->auth_algs & HOSTAPD_AUTH_SHARED_KEY)
		iwr.u.encoding.flags |= IW_ENCODE_RESTRICTED;
	iwr.u.encoding.pointer = (caddr_t) key;
	iwr.u.encoding.length = key_len;
	if (ioctl(ioctl_sock, SIOCSIWENCODE, &iwr) < 0) {
		perror("ioctl[SIOCSIWENCODE]");
		ret = -1;
	}

	if (set_tx && alg != WPA_ALG_NONE) {
		os_memset(&iwr, 0, sizeof(iwr));
		os_strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
		iwr.u.encoding.flags = key_idx + 1;
		iwr.u.encoding.pointer = (caddr_t) key;
		iwr.u.encoding.length = 0;
		if (ioctl(ioctl_sock, SIOCSIWENCODE, &iwr) < 0) {
			perror("ioctl[SIOCSIWENCODE] (set_tx)");
			ret = -1;
		}
	}

	close(ioctl_sock);
	return ret;
}
#endif /* MODIFIED_BY_SONY */
