/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: eap_wps.c
//  Description: EAP-WPS main source
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

/* Atheros modifications from Sony code:
 * Many, but especially:
 * -- Sony had simultaneous possibility of both a default pin with no
 *  timeout and a push button method... however, it appeared these might
 *  collide.  This was changed to having only one combined method which
 *  normally uses a timeout (even for PIN method) but also has the
 *  configurable possibility of a default pin without timeout
 */

#include "includes.h"

#include "defs.h"
#include "common.h"
#include "eloop.h"
#include "base64.h"
#include "hostapd.h"
#include "wps_config.h"
#include "eap_i.h"
#include "eap_wps.h"
#include "wps_parser.h"
#include "wpa_ctrl.h"
#include "driver.h"

#ifdef CONFIG_CRYPTO_INTERNAL

#include "crypto.h"
#include "sha256.h"
#include "os.h"
/* openssl provides RAND_bytes; os_get_random is equivalent */
#define RAND_bytes(buf,n) os_get_random(buf,n)

#else   /* CONFIG_CRYPTO_INTERNAL */

#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#endif  /* CONFIG_CRYPTO_INTERNAL */

#ifdef WPS_OPT_UPNP
#include "upnp_wps_device.h"
#include "wps_opt_upnp.h"
#endif /* WPS_OPT_UPNP */

/* As a security measure, lock the AP after too many failures
 * (could be someone trying to guess the PIN!).
 * Recover will require restarting hostapd or using RECONFIGURE command.
 */
#define EAP_WPS_FAILURE_LIMIT 20


#define EAP_OPCODE_WPS_START	0x01
#define EAP_OPCODE_WPS_ACK		0x02
#define EAP_OPCODE_WPS_NACK		0x03
#define EAP_OPCODE_WPS_MSG		0x04
#define EAP_OPCODE_WPS_DONE		0x05
#define EAP_OPCODE_WPS_FLAG_ACK	0x06

#define EAP_FLAG_MF	0x01
#define EAP_FLAG_LF	0x02

#define EAP_VENDOR_ID_WPS	"\x00\x37\x2a"
#define EAP_VENDOR_TYPE_WPS	"\x00\x00\x00\x01"

/* Polling period */
#define EAP_WPS_PERIOD_SEC		1
#define EAP_WPS_PERIOD_USEC		0
/* Default timeout period after which session expires */
#define EAP_WPS_TIMEOUT_SEC		120

/* Message retry period and count.
 * WPS spec reccommends 5 second retransmit time with overall limit
 * of 15 seconds; my experience is that a shorter retransmit time
 * works well.
 */
#define EAP_WPS_RETRANS_SECONDS 3
#define EAP_WPS_MAX_RETRANS 5

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

const static u8 DH_P_VALUE[SIZE_1536_BITS] = 
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
    0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
    0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
    0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
    0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
    0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
    0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
    0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
    0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
    0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
    0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
    0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
    0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
    0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
    0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x23, 0x73, 0x27,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#ifdef CONFIG_CRYPTO_INTERNAL
const static u8 DH_G_VALUE[] = { 2 };
#else   /* CONFIG_CRYPTO_INTERNAL */
const static u32 DH_G_VALUE = 2;
#endif  /* CONFIG_CRYPTO_INTERNAL */

struct eap_format {
	u8 type;
	u8 vendor_id[3];
	u8 vendor_type[4];
	u8 op_code;
	u8 flags;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */


/* Send notifications to outside world...
 */
void eap_wps_request(
        struct hostapd_data *hapd,
	int req_type, 
        const char *msg)
{
	char *buf;
	size_t buflen;
	int len = 0;
	char *field;
	char *txt;

	if (!hapd)
		return;

	switch(req_type) {
	case CTRL_REQ_TYPE_READY:
		field = "WPS-JOB-READY";
		txt = "AP button pushed or PIN entered";
		break;
	case CTRL_REQ_TYPE_DONE:
		field = "WPS-JOB-DONE";
		txt = "AP WPS no longer ready";
		break;
	case CTRL_REQ_TYPE_SUCCESS:
		field = "EAP-WPS-SUCCESS";
		txt = "Complete EAP-WPS protocol";
		break;
	case CTRL_REQ_TYPE_FAIL:
		field = "EAP-WPS-FAIL";
		txt = "Fail EAP-WPS protocol";
		break;
	case CTRL_REQ_TYPE_PASSWORD:
		field = "EAP-WPS-PASSWORD";
		txt = "Request Password for EAP-WPS";
		break;
	case CTRL_REQ_TYPE_PBC_OVERLAP:
		field = "EAP-WPS-PBC-OVERLAP";
		txt = "Overlapped push button supplicants for EAP-WPS";
		break;
	case CTRL_REQ_TYPE_CONNECTED:
		field = "EAP-WPS-CONNECTED";
		txt = "WPS protocol begun with station";
		break;
	case CTRL_REQ_TYPE_SELFCONFIGURE:
		field = "EAP-WPS-SELFCONFIGURE";
		txt = "WPS protocol configures self";
                /* NOTE: msg is base64 encoding of WPS ies, which
                 * will be fairly long...
                 */
		break;
	default:
		return;
	}

	buflen = 100 + os_strlen(txt) + (msg ? strlen(msg) : 0);
	buf = os_malloc(buflen);
	if (buf == NULL)
		return;
	len = os_snprintf(buf, buflen, WPA_CTRL_REQ "%s%s%s%s-%s ",
		       field, msg?":[":"", msg?msg:"", msg?"]":"", txt);
	if (len < 0 || (size_t) len >= buflen) {
		os_free(buf);
		return;
	}
	hostapd_msg(hapd, MSG_INFO, "%s", buf);
	os_free(buf);
}



/* Track WPS stations announcing their intention to do WPS using
 * probe requests, so that we may detect conflicts.
 * It would be only necessary to track the two latest ones, since two is
 * enough to declare a conflict, except we would run into problems
 * when clearing on successful operation.
 *
 * Actual conflict detection is handled at the point where it is
 * an issue; at that point we can examine the time stamps and discard
 * anything too old.
 *
 * NOTE! Users must press the station button first, ap button second
 * in order to avoid falling victim to rogue station... of course,
 * they may still fall victim to a rogue AP in this case.
 */
static void
eap_wps_pbc_track(
        struct hostapd_data *hapd, 
        const u8 *addr  /* source address */
        )
{
        int idx;
        int oldest_idx = -1;
        time_t oldest_time = 0;
        for (idx = 0; idx < WPS_PBC_MAX; idx++) {
                if (!memcmp(hapd->wps_pbc_track[idx].addr, addr, ETH_ALEN)) {
                        /* re-use and update our entry */
                        hapd->wps_pbc_track[idx].timestamp = time(0);
                        return;
                }
        }
        /* re-use least recently used slot */
        for (idx = 0; idx < WPS_PBC_MAX; idx++) {
                time_t told = hapd->wps_pbc_track[idx].timestamp;
                /* Use "<=" so that we always have a selection! */
                if (told <= oldest_time) {
                        oldest_idx = idx;
                        oldest_time = told;
                }
        }
        idx = oldest_idx;
        memcpy(hapd->wps_pbc_track[idx].addr, addr, ETH_ALEN);
        hapd->wps_pbc_track[idx].timestamp = time(0);
        return;
}

/* Check for WPS push button conflicts
 * This is called at the pointing of starting a push button WPS session.
 * Returns nonzero on conflict.
 */
static int
eap_wps_pbc_check(
        struct hostapd_data *hapd, 
        const u8 *addr  /* source address */
        )
{
        int nconflict = 0;
        int nfound = 0;
        int idx;
        time_t now = time(0);
        time_t cutoff_time = now - 120/*seconds, per WPS spec*/;
        for (idx = 0; idx < WPS_PBC_MAX; idx++) {
                u8 *aold;
                time_t told = hapd->wps_pbc_track[idx].timestamp;
                if (told == 0) continue; /*unused*/
                if (told < cutoff_time) continue;
                aold = hapd->wps_pbc_track[idx].addr;
                if (memcmp(aold, addr, ETH_ALEN) == 0) {
                        wpa_printf(MSG_INFO, 
                        "WPS push-button probe req seen from supplicant "
                        "%02x:%02x:%02x:%02x:%02x:%02x %d seconds ago",
                        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
                        (int)(now - told));
                        nfound++;
                        continue;       /* ignore entries for ourself */
                }
                nconflict++;
                wpa_printf(MSG_ERROR, "WPS push-button conflict: "
                        "%02x:%02x:%02x:%02x:%02x:%02x now vs. "
                        "%02x:%02x:%02x:%02x:%02x:%02x %d seconds ago",
                        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
                        aold[0], aold[1], aold[2], aold[3], aold[4], aold[5],
                        (int)(now - told));
        }
        if (nfound == 0) {
                wpa_printf(MSG_ERROR, 
                "Previous WPS push-button probe req NOT SEEN from supplicant "
                "%02x:%02x:%02x:%02x:%02x:%02x !",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
                /* perhaps we should not allow 
                 * the operation to continue! ... the spec doesn't say that.
                 */
        }
        return nconflict;
}


/* Clear station from WPS push button checking...
 * do this only after successful operation.
 * This allows another station to be configured right away.
 */
static void
eap_wps_pbc_clear(
        struct hostapd_data *hapd, 
        const u8 *addr  /* source address */
        )
{
        int idx;
        for (idx = 0; idx < WPS_PBC_MAX; idx++) {
                u8 *aold;
                aold = hapd->wps_pbc_track[idx].addr;
                if (memcmp(aold, addr, ETH_ALEN) == 0) {
                        memset(hapd->wps_pbc_track+idx, 0,
                                sizeof(hapd->wps_pbc_track[idx]));
                }
        }
        return;
}


/* Clear failure count that would cause locking.
 * Call this with addr on successful operation only.
 * Call this with addr==NULL on explict user enable 
 * (thus e.g. pushing WPS button will clear lock).
 */
static void
eap_wps_failure_clear(
        struct hostapd_data *hapd, 
        const u8 *addr  /* NULL or source address */
        )
{
        hapd->conf->wps->nfailure = 0;
        hapd->conf->wps->ap_setup_locked = 0;
        if (addr)
                eap_wps_pbc_clear(hapd, addr);
}

/* Handle WPS i.e.s from probe requests received, 
 * so we can search for conflicts in use of push button method.
 */
void
eap_wps_handle_mgmt_frames(
        struct hostapd_data *hapd, 
        const u8 *addr,      /* source address */
        const u8 *frame,     /* complete probe request */
        size_t frame_len,       /* length of frame */
        const u8 *buf,       /* WPS information elements from frame! */
        size_t len)
{
	const u8 *ie;
        int ie_len;
	const u8 *next_ie;
	const u8 *endfrm;

        endfrm = (u8 *) (buf + len);

        #ifdef WPS_OPT_UPNP
        /* Send probe requests that contain wps ies to upnp subscribers
         * (external registrars)
         * ... hmmm, although apparently required by WPS spec (very vague)
         * and WFAWLANConfig:1 (also very vague),
         * Sony did not do this, and it doesn't seem like the intel code
         * did either.
         * Besides, there doesn't seem like enough useful information
         * for an external registrar to use... unless it is
         * going to implement the push-button lock out algorith?
         *
         * One worry about using this is that it might send out a big
         * volume of messages... 
         */
        #if 0   /* probably don't need this */
        if (hapd && hapd->conf && hapd->conf->wps &&
                    hapd->conf->wps->upnp_enabled) {
            (void) wps_opt_upnp_send_wlan_probe_event(
                hapd->wps_opt_upnp, addr,
                #if 0   /* is it the wps ies? */
                buf, len,
                #else   /* or entire probe request? */
                frame, frame_len
                #endif
                );
        }
        #endif  /* don't need this? */
        #endif /* WPS_OPT_UPNP */

        /* 
         * Search for the WPS "device password id" information element...
         */
        for (ie = buf; ie <= endfrm-4 && 
                (next_ie = (ie+(ie_len = 4+((ie[2]<<8)|ie[3])))) <= endfrm; 
                        ie = next_ie) {
                unsigned ie_type = (ie[0]<<8)|ie[1];
                if (ie_type == WPS_TYPE_DEVICE_PWD_ID && ie_len == 6) {
                        unsigned device_pwd_id = (ie[4]<<8)|ie[5];
                        if (device_pwd_id == 0x0004) {
                                /* push button method */
                                eap_wps_pbc_track(hapd, addr);
                        }
                        break;
                }
	}
       
	return;
}




static int eap_wps_clear_target_info(struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;

	do {
		if (!data || !data->target)
			break;

		target = data->target;

		os_free(target->manufacturer);
		os_free(target->model_name);
		os_free(target->model_number);
		os_free(target->serial_number);
		os_free(target->dev_name);
		if (target->config) {
			os_free(target->config);
			target->config = 0;
			target->config_len = 0;
		}

		os_memset(target, 0, sizeof(*target));
		ret = 0;
	} while (0);

	return ret;
}



/*
 *
 * Note:
 * Just to make life interesting, this function is called to 
 * initialize data for use by upnp "get device info" which
 * is not part of a WPS session at all.
 */
int eap_wps_config_init_data(
        struct hostapd_data *hapd,
	struct wps_config *conf,
	struct eap_wps_data *data,
        const u8 *supplicant_addr       /* NULL for e.g. upnp case */
        )
{
	int ret = -1;
        int wrap_credential = 0;

        wpa_printf(MSG_DEBUG, "eap_wps_config_init_data ENTER interface=%d",
                data->interface);
	do {
		if (!hapd || !conf || !data)
			break;
                if (conf->wps_disable) {
                        wpa_printf(MSG_DEBUG, "WPS is disabled for this BSS");
                        break;
                }

                if (conf->nfailure > EAP_WPS_FAILURE_LIMIT) {
                        wpa_printf(MSG_ERROR, 
"Locked out as security measure... push button, enter PIN, restart or RECONFIGURE hostapd!");
                        if (conf->ap_setup_locked == 0) {
                                conf->ap_setup_locked = 1;
                                (void) eap_wps_set_ie(hapd, conf);
                        }
                        break;
                }

		data->target = wpa_zalloc(sizeof(*data->target));
		if (!data->target)
			break;

		if (conf->wps_job_busy) {
                        if (conf->dev_pwd_len <= 0) break; /*sanity check */
                        /* All WPS activity is owned by the job,
                         * so we can copy current job parameters
                         * for this EAP session.
                         */
			data->dev_pwd_id = conf->dev_pwd_id;
			os_memcpy(data->dev_pwd, conf->dev_pwd, conf->dev_pwd_len);
			data->dev_pwd_len = conf->dev_pwd_len;
		        data->config_who = conf->config_who;
                        data->is_push_button = conf->is_push_button;
                        if (data->is_push_button) {
                                /* Avoid conflicts between two stations
                                 * using push button method
                                 */
                                if (supplicant_addr == NULL) {
                                        wpa_printf(MSG_ERROR,
                                                "eap_wps_config_init_data: "
                                                "inapprop. push button use");
                                        break;
                                }
                                if (memcmp(supplicant_addr, "\0\0\0\0\0\0", 6) 
                                                == 0) {
                                        wpa_printf(MSG_ERROR,
                                                "eap_wps_config_init_data: "
                                                "invalid supplicant addr");
                                        break;
                                }
                                if (eap_wps_pbc_check(hapd, supplicant_addr)) {
                                        wpa_printf(MSG_ERROR,
                                                "WPS PUSH BUTTON CONFLICT!\n");
				        eap_wps_request(hapd, CTRL_REQ_TYPE_PBC_OVERLAP, 0);
                                        break;
                                }
                                wpa_printf(MSG_DEBUG, 
                                        "WPS push button test passed OK");
                        }
			eap_wps_request(hapd, CTRL_REQ_TYPE_CONNECTED, 0);
		} else {
                        /* 
                         * we have no job. Don't copy job parameters!
                         * WPS will actually succeed only if default PIN
                         * is provided; however don't fail now because
                         * this function is called for other purposes
                         * as well as doing WPS. 
                         */
                        if (conf->default_pin && conf->default_pin[0] &&
                                        sizeof(data->dev_pwd) > 
                                                strlen(conf->default_pin)) {
                                wpa_printf(MSG_INFO, "eap_wps using default PIN");
                                strcpy((char *)data->dev_pwd,
                                        conf->default_pin);
                                data->dev_pwd_len = strlen(conf->default_pin);
                                data->dev_pwd_id = 0;  /* implies PIN method */
                                data->is_push_button = 0;
                        }
                        /* else let it slide for now... we'll catch it later*/
                }

                #if 0   /* WAS */
		if (conf->wps_job_busy && conf->set_pub_key) {
                        /* why? */
			os_memcpy(data->pubKey, conf->pub_key, sizeof(data->pubKey));
			if (conf->dh_secret)
				data->dh_secret = conf->dh_secret;
			data->preset_pubKey = 1;
		}
                #endif

                wrap_credential = (data->interface == REGISTRAR);
		if (eap_wps_config_get_ssid_configuration(hapd,
                    conf, data, &data->config, &data->config_len,
                    wrap_credential))
		        break;

		data->state = START;
		ret = 0;
	} while (0);

        wpa_printf(MSG_DEBUG, "eap_wps_config_init_data LEAVE ret=%d", ret);
	return ret;
}


#if 0   /* original, but now we do it later when we have more info */
static int eap_wps_init_data(struct eap_sm *sm, struct eap_wps_data *data)
{
	int ret = -1;
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
        #if 0   /* original sony */
	const u8 *pwd;
	size_t pwd_len;
        #endif

	do {
		if (!sm || !data || !conf || !hapd)
			break;

		if (eap_wps_config_init_data(hapd, conf, data, sm->addr))
			break;

                #if 0   /* original sony code: password from hostapd.eap_user file */
		pwd = sm->user->password;
		pwd_len = sm->user->password_len;
		if ( pwd && pwd_len ) {
			if (pwd_len > sizeof(data->dev_pwd))
				pwd_len = sizeof(data->dev_pwd);

			if (8 == pwd_len) {
				if (eap_wps_device_password_validation(pwd, (int)pwd_len))
					data->dev_pwd_id = WPS_DEVICEPWDID_USER_SPEC;
				else
					data->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
			} else
				data->dev_pwd_id = WPS_DEVICEPWDID_USER_SPEC;
			os_memcpy(data->dev_pwd, pwd, pwd_len);
			data->dev_pwd_len = pwd_len;

			conf->upnp_enabled = 0;
		}
                #endif  /* original sony code */

		ret = 0;
	} while (0);

	return ret;
}
#endif  /* original */


/* 
 * called from eap.c to initialize data for a new session.
 * Note that there can be a session per station that is managed by
 * hostapd.
 */
static void *eap_wps_init(struct eap_sm *sm)
{
	int result = -1;
	struct eap_wps_data *data;

        wpa_printf(MSG_DEBUG, "eap_wps_init ENTER");
	do {
		data = wpa_zalloc(sizeof(*data));
		if (data == NULL)
			break;

                #if 0   /* original, but now we do it later when we have more info */
		if (eap_wps_init_data(sm, data))
			break;
                #endif  /* original */

		sm->eap_method_priv = data;
		result = 0;
	} while (0);

	if (result) {
		os_free(data);
		data = 0;
	}

	return data;
}


void eap_wps_config_deinit_data(struct eap_wps_data *data)
{
	do {
		if (!data)
			break;

		if (data->rcvMsg) {
			os_free(data->rcvMsg);
			data->rcvMsg = 0;
			data->rcvMsgLen = 0;
			data->fragment = 0;
		}

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}

		if (!data->preset_pubKey && data->dh_secret) {
                        #if 1   /* Atheros */
			eap_wps_free_dh((void **)&data->dh_secret);
                        #else   /* from Sony */
			DH_free(data->dh_secret);
                        #endif
			data->dh_secret = 0;
		}

		if (data->config) {
			os_free(data->config);
			data->config = 0;
			data->config_len = 0;
		}

		if (data->target) {
			eap_wps_clear_target_info(data);
			os_free(data->target);
			data->target = 0;
		}

		os_free(data);
	} while (0);
}


/*
 * Called from eap.c not only at beginning of interaction but also
 * at the end, prior to freeing up the eap state machine (eap_sm) entirely.
 */
static void eap_wps_reset(struct eap_sm *sm, void *priv)
{
	struct eap_wps_data *data = (struct eap_wps_data *)priv;

	if (data == NULL)
		return;

        /* When a registrar is "selected" (which is the only way that
         * we make use of upnp) then a WPS operation should(?) be ONLY
         * with that registrar, and moreover we can only have one such
         * operation at a time.
         * So when any WPS operation finishes, it is fair to assume that
         * we should cancel any waiting for UPnP ...
         * although there would certainly seem to be some risk of
         * collision with a non-UPnP session that was started around
         * the same time.
         */
        #ifdef WPS_OPT_TINYUPNP
        if (data->config_who == WPS_CONFIG_WHO_EXTERNAL_REGISTRAR) {
	        struct hostapd_data *hapd = 
	                (struct hostapd_data *)eap_get_hostapd_data(sm);
                if (hapd && hapd->wps_opt_upnp) {
                        wps_opt_upnp_deinit_data(hapd->wps_opt_upnp);
                        /* Note this called eap_wps_config_deinit_data() */
                        return;
                }
        }
        #endif /* WPS_OPT_TINYUPNP */

	eap_wps_config_deinit_data(data);
}


int eap_wps_generate_sha256hash(u8 *inbuf, int inbuf_len, u8 *outbuf)
{
	int ret = -1;

	do {
		if (!inbuf || !inbuf_len || !outbuf)
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL
                {
                        const u8 *vec[1];
                        size_t vlen[1];
                        vec[0] = inbuf;
                        vlen[0] = inbuf_len;
                        sha256_vector(1, vec, vlen, outbuf);
                }
                #else /* CONFIG_CRYPTO_INTERNAL */
		if (!SHA256(inbuf, inbuf_len, outbuf))
			break;
                #endif /* CONFIG_CRYPTO_INTERNAL */

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_free_dh(void **dh)
{
	int ret = -1;
	do {
		if (!dh || !*dh)
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL
                os_free(*dh);
                *dh = NULL;
                #else /* CONFIG_CRYPTO_INTERNAL */
		DH_free(*dh);
		*dh = 0;
                #endif /* CONFIG_CRYPTO_INTERNAL */

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_generate_public_key(void **dh_secret, u8 *public_key)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        if (dh_secret) *dh_secret = NULL;

	do {
                size_t len;
		if (!dh_secret || !public_key)
			break;

                /* We here generate both private key and public key.
                * For compatibility with the openssl version of code
                * (from Sony), dh_secret retains the private key
                * it is NOT the Diffie-Helman shared secret!).
                * The private key is used later to generate various other
                * data that can be decrypted by recipient using the public key.
                */
                *dh_secret = os_malloc(SIZE_PUB_KEY);
                if (dh_secret == NULL) break;
                RAND_bytes(*dh_secret, SIZE_PUB_KEY);  /* make private key */
                len = SIZE_PUB_KEY;
                if (crypto_mod_exp(
                        DH_G_VALUE,
                        sizeof(DH_G_VALUE),
                        *dh_secret,     /* private key */
                        SIZE_PUB_KEY,
                        DH_P_VALUE,
                        sizeof(DH_P_VALUE),
                        public_key,     /* output */
                        &len            /* note: input/output */
                        ) ) break;
                if (0 < len && len < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(public_key+(SIZE_PUB_KEY-len),
                            public_key, len);
                        memset(public_key, 0, (SIZE_PUB_KEY-len));
                } else if (len != SIZE_PUB_KEY) 
                        break;
                ret = 0;
        } while (0);

        if (ret) {
            if (dh_secret && *dh_secret) os_free(*dh_secret);
            if (dh_secret) *dh_secret = NULL;
        }

        #else   /* CONFIG_CRYPTO_INTERNAL */

	u8 tmp[SIZE_PUB_KEY];
	DH *dh = 0;
	u32 g;
	int length;

	do {
		if (!dh_secret || !public_key)
			break;

		*dh_secret = 0;

		dh = DH_new();
		if(!dh)
			break;

		dh->p = BN_new();
		if (!dh->p)
			break;

		dh->g = BN_new();
		if (!dh->g)
			break;
	   
		if(!BN_bin2bn(DH_P_VALUE, SIZE_1536_BITS, dh->p))
			break;

		g = host_to_be32(DH_G_VALUE);
		if(!BN_bin2bn((u8 *)&g, 4, dh->g))
			break;

		if(!DH_generate_key(dh))
			break;

		length = BN_bn2bin(dh->pub_key, tmp);
		if (!length)
			break;

		length = BN_bn2bin(dh->pub_key, public_key);
                if (0 < length && length < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(public_key+(SIZE_PUB_KEY-length),
                            public_key, length);
                        memset(public_key, 0, (SIZE_PUB_KEY-length));
                } else if (length != SIZE_PUB_KEY)
                        break;
		ret = 0;
	} while (0);

	if (ret && dh) {
		DH_free(dh);
	} else if (dh) {
		*dh_secret = dh;
	}

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_generate_kdk(struct eap_wps_data *data, u8 *e_nonce, u8 *mac,
								u8 *r_nonce, u8 *kdk)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	do {
	        u8 *dh_secret = data->dh_secret;  /* actually, is private key*/
                u8 dhkey[SIZE_DHKEY/*32 bytes*/];
	        u8 shared_secret[SIZE_PUB_KEY];  /* the real DH Shared Secret*/
                const u8 *vec[3];
                size_t vlen[3];

		if (!dh_secret || !e_nonce || !mac || !r_nonce || !kdk)
			break;

                /* Calculate the Diffie-Hellman shared secret g^AB mod p
                * by calculating (PKr)^A mod p
                * (For compatibility with Sony code, dh_secret is NOT
                * the Diffie-Hellman Shared Secret but instead contains
                * just the private key).
                */
                size_t len = SIZE_PUB_KEY;
                if (crypto_mod_exp(
                        data->target->pubKey,
                        SIZE_PUB_KEY,
                        dh_secret,              /* our private key */
                        SIZE_PUB_KEY,
                        DH_P_VALUE,
                        sizeof(DH_P_VALUE),
                        shared_secret,         /* output */
                        &len               /* in/out */
                        )) break;
                if (0 < len && len < SIZE_PUB_KEY) {
                        /* Convert to fixed size big-endian integer */
                        memmove(shared_secret+(SIZE_PUB_KEY-len),
                            shared_secret, len);
                        memset(shared_secret, 0, (SIZE_PUB_KEY-len));
                } else if (len != SIZE_PUB_KEY) 
                        break;

                /* Calculate DHKey (hash of DHSecret)
                */
                vec[0] = shared_secret;
                vlen[0] = SIZE_PUB_KEY;  /* DH Secret size, 192 bytes */
                sha256_vector(
                        1,  // num_elem
                        vec,
                        vlen,
                        dhkey   /* output: 32 bytes */
                        );

                /* Calculate KDK (Key Derivation Key)
                */
                vec[0] = e_nonce;
                vlen[0] = SIZE_NONCE;
                vec[1] = mac;
                vlen[1] = SIZE_MAC_ADDR;
                vec[2] = r_nonce;
                vlen[2] = SIZE_NONCE;
                hmac_sha256_vector(
                        dhkey,
                        SIZE_DHKEY,
                        3,              /* num_elem */
                        vec,
                        vlen,
                        kdk     /* output: 32 bytes */
                        );
                ret = 0;
        } while (0);

        #else   /* CONFIG_CRYPTO_INTERNAL */

	DH *dh_secret = (DH *)data->dh_secret;
	BIGNUM *bn_peer = 0;
	u8 shared_secret[SIZE_PUB_KEY];
	int shared_secret_length;
	u8 sec_key_sha[SIZE_256_BITS];
	u8 kdk_src[SIZE_NONCE + SIZE_MAC_ADDR + SIZE_NONCE];
	int kdk_src_len;

	do {
		if (!dh_secret || !e_nonce || !mac || !r_nonce || !kdk)
			break;

		bn_peer = BN_new();
		if (!bn_peer)
			break;

		if (!BN_bin2bn(data->target->pubKey, SIZE_PUB_KEY, bn_peer))
			break;

		shared_secret_length = DH_compute_key(shared_secret, bn_peer, dh_secret);
		if (-1 == shared_secret_length)
			break;

		if (!SHA256(shared_secret, shared_secret_length, sec_key_sha))
			break;

		kdk_src_len = 0;
		os_memcpy((u8 *)kdk_src + kdk_src_len, e_nonce, SIZE_NONCE);
		kdk_src_len += SIZE_NONCE;
		os_memcpy((u8 *)kdk_src + kdk_src_len, mac, SIZE_MAC_ADDR);
		kdk_src_len += SIZE_MAC_ADDR;
		os_memcpy((u8 *)kdk_src + kdk_src_len, r_nonce, SIZE_NONCE);
		kdk_src_len += SIZE_NONCE;
		if (!HMAC(EVP_sha256(), sec_key_sha, SIZE_256_BITS,
				  kdk_src, kdk_src_len, kdk, NULL))
			break;

		ret = 0;
	} while (0);

	if (bn_peer)
		BN_free(bn_peer);

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_key_derive_func(struct eap_wps_data *data, 
						   u8 *kdk,
						   u8 keys[KDF_OUTPUT_SIZE])
{
        const char *personalization = WPS_PERSONALIZATION_STRING;
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	do {
                const u8 *vec[3];
                size_t vlen[3];
                u8 cb1[4];
                u8 cb2[4];
                int iter;

		WPA_PUT_BE32(cb2, KDF_KEY_BITS/*== 640*/);
                vec[0] = cb1;   /* Note: cb1 modified in loop below */
                vlen[0] = sizeof(cb1);
                vec[1] = (void *)personalization;
                vlen[1] = os_strlen(personalization);
                vec[2] = cb2;
                vlen[2] = sizeof(cb2);

                for (iter = 0; iter < KDF_N_ITERATIONS; iter++) {
		        WPA_PUT_BE32(cb1, iter+1);
                        hmac_sha256_vector(
                                kdk,
                                SIZE_KDK,
                                3,      /* num_elem */
                                vec,
                                vlen,
                                keys + SHA256_MAC_LEN*iter  /* out: 32 bytes/iteration */
                                );
                }
                ret = 0;
        } while (0);

        #else   /* CONFIG_CRYPTO_INTERNAL */

	u8 *prf;
	u32 prf_len;
	u8 *hmac = 0, *pos;
	u32 hmac_len = 0, length = 0;
	u32 i;

	do {
		prf_len = sizeof(u32) + os_strlen(personalization) + sizeof(u32);
		prf = os_malloc(prf_len);
		if (!prf)
			break;

		pos = prf + sizeof(u32);
		os_memcpy(pos, personalization, os_strlen(personalization));
		pos += os_strlen(personalization);
		WPA_PUT_BE32(pos, KDF_KEY_BITS);

		for (i = 1; i <= KDF_N_ITERATIONS; i++) {
			WPA_PUT_BE32(prf, i);
			length = 0;
			(void)HMAC(EVP_sha256(), kdk, SIZE_256_BITS, prf, prf_len, 0, &length);
			hmac = (u8 *)os_realloc(hmac, hmac_len + length);
			pos = hmac + hmac_len;
			if (!HMAC(EVP_sha256(), kdk, SIZE_256_BITS, prf, prf_len, pos, &length))
				break;
			hmac_len += length;
		}
		if (i <= KDF_N_ITERATIONS)
			break; 
		if ((KDF_KEY_BITS / 8) > hmac_len)
			break;

		if (!keys)
			break;
		os_memcpy(keys, hmac, KDF_KEY_BITS / 8);

		ret = 0;
	} while (0);

	if (prf)
		os_free(prf);
	if (hmac)
		os_free(hmac);

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_hmac_validation(struct eap_wps_data *data,
	   u8 *authenticator, u8 *auth_key)
{
	int ret = -1;

        #ifndef CONFIG_CRYPTO_INTERNAL
	u8 *hmac_src = 0;
	u32 hmac_src_len;
        #endif
	struct wps_data *wps = 0;
	u8 *buf = 0;
	size_t buf_len;
	u8 hmac[SIZE_256_BITS];

	do {
		if (!data || !authenticator || !auth_key)
			break;

                /* Atheros note: this Sony code goes to a lot of extra effort 
                 * to parse the data, remove the authenticator and then
                 * recreate the original packet minus the authenticator...
                 * not necessary since the authenticator will always
                 * be at the end... so it could be optimized...
                 */

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		if (wps_remove_value(wps, WPS_TYPE_AUTHENTICATOR))
			break;

		if (wps_write_wps_data(wps, &buf, &buf_len))
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL

                {
                        const u8 *vec[2];
                        size_t vlen[2];
                        vec[0] = data->sndMsg;
                        vlen[0] = data->sndMsgLen;
                        vec[1] = buf;
                        vlen[1] = buf_len;
                        hmac_sha256_vector(
                            auth_key,
                            SIZE_AUTH_KEY,
                            2,  /* num_elem */
                            vec,
                            vlen,
                            hmac);
                }

                #else   /* CONFIG_CRYPTO_INTERNAL */

		hmac_src_len = data->sndMsgLen + buf_len;
		hmac_src = os_malloc(hmac_src_len);
		if (!hmac_src)
			break;

		os_memcpy(hmac_src, data->sndMsg, data->sndMsgLen);
		os_memcpy(hmac_src + data->sndMsgLen, buf, buf_len);

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS, hmac_src, hmac_src_len, hmac, NULL))
			break;

                #endif   /* CONFIG_CRYPTO_INTERNAL */

		if (os_memcmp(hmac, authenticator, SIZE_64_BITS))
			break;

		ret = 0;
	} while (0);

        #ifndef CONFIG_CRYPTO_INTERNAL
	if (hmac_src)
		os_free(hmac_src);
        #endif
	if (buf)
		os_free(buf);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_encrypt_data(struct eap_wps_data *data,
								u8 *inbuf, int inbuf_len,
								u8 *encrKey,
								u8 *iv, u8 **cipher, int *cipher_len)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        void *aesHandle = NULL;

        if (cipher) *cipher = NULL;
        do {
                u8 *lastcipher;
                u8 *thiscipher;
                aesHandle = aes_encrypt_init(encrKey, ENCR_DATA_BLOCK_SIZE);

		RAND_bytes(iv, ENCR_DATA_BLOCK_SIZE);
                lastcipher = iv;

		if (!cipher || !cipher_len)
			break;

                /* The output is up to one block larger than the input */
                *cipher = os_malloc(inbuf_len+ENCR_DATA_BLOCK_SIZE);
                *cipher_len = 0;
                thiscipher = *cipher;
                for (;; ) {
                        u8 block[ENCR_DATA_BLOCK_SIZE];
                        int i;
                        int thislen = inbuf_len;
                        if (thislen > ENCR_DATA_BLOCK_SIZE)
                                thislen = ENCR_DATA_BLOCK_SIZE;
                        if (thislen > 0) 
                                memcpy(block, inbuf, thislen );
                        if (thislen < ENCR_DATA_BLOCK_SIZE) {
                                /* Last block: 
                                 * pad out with a byte value that gives the 
                                 * number of padding bytes.
                                 */
                                int npad = ENCR_DATA_BLOCK_SIZE - thislen;
                                int ipad;
                                for (ipad = 0; ipad < npad; ipad++) {
                                        block[ENCR_DATA_BLOCK_SIZE-ipad-1] = 
                                                npad;
                                }
                        }
                        /* Cipher Block Chaining (CBC) -- 
                         * xor the plain text with the last AES output
                         * (or initially, the "initialization vector").
                         */
                        for (i = 0; i < ENCR_DATA_BLOCK_SIZE; i++) {
                                block[i] ^= lastcipher[i];
                        }
                        /* And encrypt and store in output */
                        aes_encrypt(aesHandle, block, thiscipher);
                        lastcipher = thiscipher;
                        thiscipher += ENCR_DATA_BLOCK_SIZE;
                        *cipher_len += ENCR_DATA_BLOCK_SIZE;
                        if ( thislen < ENCR_DATA_BLOCK_SIZE ) {
                                ret = 0;
                                break;
                        }
                        inbuf += ENCR_DATA_BLOCK_SIZE;
                        inbuf_len -= ENCR_DATA_BLOCK_SIZE;
                }
        } while (0);
        if (aesHandle) aes_encrypt_deinit(aesHandle);

        #else   /* CONFIG_CRYPTO_INTERNAL */

	EVP_CIPHER_CTX ctx;
	u8 buf[1024];
	int buf_len;
	int length, curr_len; int block_size;

        if (cipher) *cipher = NULL;
	do {
		RAND_bytes(iv, SIZE_128_BITS);

		if (!cipher || !cipher_len)
			break;

		if (!EVP_EncryptInit(&ctx, EVP_aes_128_cbc(), encrKey, iv))
			break;

		length = inbuf_len;
		block_size = sizeof(buf) - SIZE_128_BITS;

		*cipher = 0;
		*cipher_len  = 0;
		while (length) {
			if (length > block_size)
				curr_len = block_size;
			else
				curr_len = length;

			if (!EVP_EncryptUpdate(&ctx, buf, &buf_len, inbuf, curr_len))
				break;
			*cipher = (u8 *)os_realloc(*cipher, *cipher_len + buf_len);
			os_memcpy(*cipher + *cipher_len, buf, buf_len);
			*cipher_len += buf_len;
			length -= curr_len;
		}

		if (length)
			break;

		if (!EVP_EncryptFinal(&ctx, buf, &buf_len))
			break;

		*cipher = (u8 *)os_realloc(*cipher, *cipher_len + buf_len);
		os_memcpy(*cipher + *cipher_len, buf, buf_len);
		*cipher_len += buf_len;

		ret = 0;
	} while (0);

        #endif   /* CONFIG_CRYPTO_INTERNAL */

	if (ret) {
		if (cipher_len)
			*cipher_len = 0;
		if (cipher && *cipher) {
			os_free(*cipher);
			*cipher = 0;
		}
	}

	return ret;
}


static int eap_wps_decrypt_data(struct eap_wps_data *data, u8 *iv,
								u8 *cipher, int cipher_len,
								u8 *encrKey, u8 **plain, int *plain_len)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        void *aesHandle = NULL;
        if (plain) *plain = NULL;

	do {
                u8 *out;
                int out_len = 0;

		if (!iv || !cipher || !encrKey || !plain || !plain_len)
			break;
                if (cipher_len <= 0 || 
                            (cipher_len & (ENCR_DATA_BLOCK_SIZE-1)) != 0) 
                        break;

                /* The plain text length is always less than the cipher
                 * text length (which contains 1 to 16 bytes of padding).
                 * No harm in allocating more than we need.
                 */
		*plain = os_malloc(cipher_len);
		*plain_len = 0;
                if (*plain == NULL) break;
                out = *plain;

                aesHandle = aes_decrypt_init(encrKey, ENCR_DATA_BLOCK_SIZE);
                if (aesHandle == NULL) break;

                while (cipher_len >= ENCR_DATA_BLOCK_SIZE) {
                        int block_len = ENCR_DATA_BLOCK_SIZE;
                        int i;
                        aes_decrypt(aesHandle, cipher, out);
                        /* Cipher Block Chaining (CBC) -- xor the plain text with
                         * the last AES output (or initially, the "initialization vector").
                         */
                        for (i = 0; i < ENCR_DATA_BLOCK_SIZE; i++) {
                                out[i] ^= iv[i];
                        }
                        iv = cipher;
                        cipher += ENCR_DATA_BLOCK_SIZE;
                        cipher_len -= ENCR_DATA_BLOCK_SIZE;
                        if (cipher_len < ENCR_DATA_BLOCK_SIZE) {
                                int npad;
                                /* cipher_len should be exactly 0
                                 * at this point... it must be a multiple
                                 * of blocks.  The last block should contain
                                 * between 1 and 16 bytes of padding,
                                 * with the last byte of padding saying
                                 * how many.
                                 */
                                if (cipher_len != 0) break;
                                npad = out[ENCR_DATA_BLOCK_SIZE-1];
                                if (npad > 0 && npad <= ENCR_DATA_BLOCK_SIZE) {
                                        block_len -= npad;
                                } else goto bad;
                        }
                        out += block_len;
                        out_len += block_len;
                }
                *plain_len = out_len;
                ret = 0;
                break;
        } while (0);
        bad:
        if (aesHandle) aes_decrypt_deinit(aesHandle);

        #else /* CONFIG_CRYPTO_INTERNAL */

	EVP_CIPHER_CTX ctx;
	u8 buf[1024];
	int buf_len = sizeof(buf);
	int length, curr_len;
	int block_size;

	do {
		if (!iv || !cipher || !encrKey || !plain || !plain_len)
			break;

		*plain = 0;
		*plain_len = 0;

		if (!EVP_DecryptInit(&ctx, EVP_aes_128_cbc(), encrKey, iv))
			break;

		length = cipher_len;
		block_size = sizeof(buf) - SIZE_128_BITS;

		while (length) {
			if (length > block_size)
				curr_len = block_size;
			else
				curr_len = length;

			if (!EVP_DecryptUpdate(&ctx, buf, &buf_len, cipher, curr_len))
				break;
			*plain = (u8 *)os_realloc(*plain, *plain_len + buf_len);
			os_memcpy(*plain + *plain_len, buf, buf_len);
			*plain_len += buf_len;
			length -= curr_len;
		}

		if (length)
			break;

		if (!EVP_DecryptFinal(&ctx, buf, &buf_len))
			break;

		*plain = (u8 *)os_realloc(*plain, *plain_len + buf_len);
		os_memcpy(*plain + *plain_len, buf, buf_len);
		*plain_len += buf_len;

		ret = 0;
	} while (0);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	if (ret) {
		if (plain_len)
			*plain_len = 0;
		if (plain && *plain) {
			os_free(*plain);
			*plain = 0;
		}
	}

	return ret;
}


static int eap_wps_encrsettings_creation(
	struct hostapd_data *hapd,
        struct eap_wps_data *data,
        u16 nonce_type, u8 *nonce,
        u8 *buf, size_t buf_len,
        u8 *auth_key, u8 *key_wrap_auth,
        u8 **encrs, size_t *encrs_len)
{
	int ret = -1;
	struct wps_data *wps = 0;
	u8 hmac[SIZE_256_BITS];
	size_t length = 0;
	u8 *tmp = 0;
	u8 *cipher = 0, iv[SIZE_128_BITS];
	int cipher_len;

	do {
		if (!auth_key || !key_wrap_auth || !encrs || !encrs_len)
			break;

		*encrs = 0;
		*encrs_len = 0;

		if (wps_create_wps_data(&wps))
			break;

		if (nonce) {
			length = SIZE_NONCE;
			if (wps_set_value(wps, nonce_type, nonce, length))
				break;

			length = 0;
			if (wps_write_wps_data(wps, &tmp, &length))
				break;
		}

		if (buf && buf_len) {
			(void)wps_destroy_wps_data(&wps);

			tmp = os_realloc(tmp, length + buf_len);
			if (!tmp)
				break;
			os_memcpy(tmp + length, buf, buf_len);
			length += buf_len;

			if (wps_create_wps_data(&wps))
				break;

			if (wps_parse_wps_data(tmp, length, wps))
				break;

                        #if 0   /* This breaks some stations and is not necessary */
                        /* Atheros Extensions */
                        if (wps_config_add_atheros_wps_ext(hapd, wps))
                                break;
                        #endif

			if (wps_write_wps_data(wps, &tmp, &length))
				break;
		}

                #ifdef CONFIG_CRYPTO_INTERNAL

                {
                        const u8 *vec[1];
                        size_t vlen[1];
                        vec[0] = tmp;
                        vlen[0] = length;
                        hmac_sha256_vector(
                                auth_key,
                                SIZE_AUTH_KEY,  /* auth_key size */
                                1,              /* num_elem */
                                vec,
                                vlen,
                                hmac     /* output: 32 bytes */
                                );
                }

                #else /* CONFIG_CRYPTO_INTERNAL */

		if (!HMAC(EVP_sha256(), auth_key, SIZE_AUTH_KEY, tmp, length, hmac, NULL))
			break;

                #endif /* CONFIG_CRYPTO_INTERNAL */

		if (wps_set_value(wps, WPS_TYPE_KEY_WRAP_AUTH, hmac, SIZE_64_BITS))
			break;

		os_free(tmp);
		tmp = 0;

		length = 0;
		if (wps_write_wps_data(wps, &tmp, &length))
			break;

		if (eap_wps_encrypt_data(data, tmp, length, key_wrap_auth, iv, &cipher, &cipher_len))
			break;

		*encrs = os_malloc(SIZE_128_BITS + cipher_len);
		if (!*encrs)
			break;
		os_memcpy(*encrs, iv, SIZE_128_BITS);
		os_memcpy(*encrs + SIZE_128_BITS, cipher, cipher_len);
		*encrs_len = SIZE_128_BITS + cipher_len;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (cipher)
		os_free(cipher);

	if (ret) {
		if (encrs_len)
			*encrs_len = 0;
		if (encrs && *encrs) {
			os_free(*encrs);
			*encrs = 0;
		}
	}

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_encrsettings_validation(struct eap_wps_data *data,
										   u8 *plain, int plain_len,
										   u8 *auth_key, u16 nonce_type,
										   u8 *nonce, u8 *key_wrap_auth)
{
	int ret = -1;
	struct wps_data *wps = 0;
	size_t length;
	u8 *buf = 0;
	u8 hmac[SIZE_256_BITS];

	do {
		if (!plain || !plain_len || !key_wrap_auth)
			break;
		
		if (wps_create_wps_data(&wps))
			break;
		if (wps_parse_wps_data(plain, plain_len, wps))
			break;

		if (nonce) {
		/* Nonce */
			length = SIZE_NONCE;
			if (wps_get_value(wps, nonce_type, nonce, &length))
				break;
		}

		/* Key Wrap Authenticator */
		length = SIZE_8_BYTES;
		if (wps_get_value(wps, WPS_TYPE_KEY_WRAP_AUTH, key_wrap_auth, &length))
			break;

		if (wps_remove_value(wps, WPS_TYPE_KEY_WRAP_AUTH))
			break;

		length = 0;
		if (wps_write_wps_data(wps, &buf, &length))
			break;

                #ifdef CONFIG_CRYPTO_INTERNAL

                {
                        const u8 *vec[1];
                        size_t vlen[1];
                        vec[0] = buf;
                        vlen[0] = length;
                        hmac_sha256_vector(
                                auth_key,
                                SIZE_AUTH_KEY,  /* auth_key size */
                                1,              /* num_elem */
                                vec,
                                vlen,
                                hmac     /* output: 32 bytes */
                                );
                }

                #else /* CONFIG_CRYPTO_INTERNAL */

		if (!HMAC(EVP_sha256(), auth_key, SIZE_AUTH_KEY, buf, length, hmac, NULL))
			break;

                #endif /* CONFIG_CRYPTO_INTERNAL */

		if (os_memcmp(hmac, key_wrap_auth, SIZE_64_BITS))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	if (ret) {
		if (nonce)
			os_memset(nonce, 0, SIZE_NONCE);
		if (key_wrap_auth)
			os_memset(key_wrap_auth, 0, SIZE_8_BYTES);
	}

	return ret;
}


static int eap_wps_generate_hash(struct eap_wps_data *data,
		 u8 *src, int src_len,
		 u8 *pub_key1, u8 *pub_key2,
		 u8 *auth_key,
		 u8 *psk, u8 *es, u8 *hash)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	do {
                const u8 *vec[4];
                size_t vlen[4];
	        u8 hash_tmp[SHA256_MAC_LEN];

		if (!src || !pub_key1 || !pub_key2 || !psk || !es || !auth_key)
			break;

                /* Generate psk1 or psk2 while we are at it 
                 * (based on parts of the wps password == PIN) 
                 */
                vec[0] = src;
                vlen[0] = src_len;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_AUTH_KEY,
                        1,              /* num_elem */
                        vec,
                        vlen,
                        hash_tmp     /* output: 32 bytes */
                        );
		os_memcpy(psk, hash_tmp, SIZE_128_BITS); /* first 16 bytes */

                /* Generate a nonce while we are at it */
		RAND_bytes(es, SIZE_128_BITS);

                /* Generate hash (includes above nonce and psk portion) */
                vec[0] = es;
                vlen[0] = SIZE_128_BITS;
                vec[1] = psk;
                vlen[1] = SIZE_128_BITS;        /* first 16 bytes only */
                vec[2] = pub_key1;
                vlen[2] = SIZE_PUB_KEY;
                vec[3] = pub_key2;
                vlen[3] = SIZE_PUB_KEY;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_AUTH_KEY,  /* auth_key size */
                        4,              /* num_elem */
                        vec,
                        vlen,
                        hash     /* output: 32 bytes */
                        );
		ret = 0;
	} while (0);

        #else /* CONFIG_CRYPTO_INTERNAL */

	u8 hash_tmp[SIZE_256_BITS];
	u8 hash_src[SIZE_128_BITS * 2 + SIZE_PUB_KEY * 2];
	u8 *tmp;

	do {
		if (!src || !pub_key1 || !pub_key2 || !psk || !es || !auth_key)
			break;

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS, src, src_len,
			 hash_tmp, NULL))
			break;
		os_memcpy(psk, hash_tmp, SIZE_128_BITS);

		RAND_bytes(es, SIZE_128_BITS);

		tmp = hash_src;
		os_memcpy(tmp, es, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, psk, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, pub_key1, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;
		os_memcpy(tmp, pub_key2, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS,
				  hash_src, tmp - hash_src, hash, NULL))
			break;

		ret = 0;
	} while (0);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


int eap_wps_generate_device_password_id(u16 *dev_pwd_id)
{
	int ret = -1;

	do {
		if (!dev_pwd_id)
			break;

		RAND_bytes((u8 *)dev_pwd_id, 2);
		*dev_pwd_id |= 0x8000;
		*dev_pwd_id &= 0xfff0;

		ret = 0;
	} while (0);

	return ret;
}


static u8 eap_wps_compute_device_password_checksum(u32 pin)
{
	u32 acc = 0;
	u32 tmp = pin * 10;

	acc += 3 * ((tmp / 10000000) % 10);
	acc += 1 * ((tmp / 1000000) % 10);
	acc += 3 * ((tmp / 100000) % 10);
	acc += 1 * ((tmp / 10000) % 10);
	acc += 3 * ((tmp / 1000) % 10);
	acc += 1 * ((tmp / 100) % 10);
	acc += 3 * ((tmp / 10) % 10);

	return (u8)(10 - (acc % 10)) % 10;
}


int eap_wps_generate_device_password(u8 *dev_pwd, int dev_pwd_len)
{
	int ret = -1;

	do {
		if (!dev_pwd || !dev_pwd_len)
			break;

		RAND_bytes(dev_pwd, dev_pwd_len);
		if (8 == dev_pwd_len) {
			u32 val;
			u8 check_sum, tmp[9];
			val = *(u32 *)dev_pwd;
			check_sum = eap_wps_compute_device_password_checksum(val);
			val = val * 10 + check_sum;
			os_snprintf((char *)tmp, 9, "%08u", val);
			os_memcpy(dev_pwd, tmp, 8);
		}

		ret = 0;
	} while (0);

	return ret;
}


static int eap_wps_oobdevpwd_public_key_hash_validation(const u8 *hashed, const u8 *raw)
{
	int ret = -1;
	u8 src[SIZE_256_BITS];

	do {
		if (!hashed || !raw)
			break;

		if (eap_wps_generate_sha256hash((u8 *)raw, SIZE_PUB_KEY, src))
			break;

		if (os_memcmp(hashed, src, SIZE_20_BYTES))
			break;

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_device_password_validation(const u8 *pwd, const int len)
{
	int ret = -1;
	u32 pin;
	char str_pin[9], *end;
	u8 check_sum;

	do {
		if (!pwd || 8 != len)
			break;

		os_memcpy(str_pin, pwd, 8);
		str_pin[8] = 0;
		pin = strtoul(str_pin, &end, 10);
		if (end != (str_pin + 8))
			break;

		check_sum = eap_wps_compute_device_password_checksum(pin / 10);
		if (check_sum != (u8)(pin % 10))
			break;

		ret = 0;
	} while (0);

	return ret;
}


static int eap_wps_calcurate_authenticator(struct eap_wps_data *data,
										   u8 *sndmsg, size_t sndmsg_len,
										   u8 *auth_key, u8 *authenticator)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

	u8 hmac[SIZE_256_BITS];

	do {
                const u8 *vec[2];
                size_t vlen[2];

		if (!data || !sndmsg || !authenticator)
			break;

                vec[0] = data->rcvMsg;
                vlen[0] = data->rcvMsgLen;
                vec[1] = sndmsg;
                vlen[1] = sndmsg_len;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_256_BITS,  /* auth_key size */
                        2,              /* num_elem */
                        vec,
                        vlen,
                        hmac     /* output: 32 bytes */
                        );
		os_memcpy(authenticator, hmac, SIZE_64_BITS);
		ret = 0;
	} while (0);

        #else /* CONFIG_CRYPTO_INTERNAL */

	u8 *hmac_src = 0;
	int hmac_src_len;
	u8 hmac[SIZE_256_BITS];

	do {
		if (!data || !sndmsg || !authenticator)
			break;

		hmac_src_len = data->rcvMsgLen + sndmsg_len;
		hmac_src = os_malloc(hmac_src_len);
		os_memcpy(hmac_src, data->rcvMsg, data->rcvMsgLen);
		os_memcpy(hmac_src + data->rcvMsgLen, sndmsg, sndmsg_len);

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS,
				  hmac_src, hmac_src_len, hmac, NULL))
			break;

		os_memcpy(authenticator, hmac, SIZE_64_BITS);

		ret = 0;
	} while (0);

	if (hmac_src)
		os_free(hmac_src);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


static int eap_wps_hash_validation(struct eap_wps_data *data,
								   u8 *compared,
								   u8 *rsnonce, u8 *psk,
								   u8 *pub_key1, u8 *pub_key2,
								   u8 *auth_key)
{
	int ret = -1;

        #ifdef CONFIG_CRYPTO_INTERNAL

        do {
	        u8 target[SIZE_256_BITS];
                const u8 *vec[4];
                size_t vlen[4];

		if (!compared || !rsnonce || !psk || !pub_key1 || !pub_key2 || !auth_key)
			break;

                vec[0] = rsnonce;
                vlen[0] = SIZE_128_BITS;
                vec[1] = psk;
                vlen[1] = SIZE_128_BITS;
                vec[2] = pub_key1;
                vlen[2] = SIZE_PUB_KEY;
                vec[3] = pub_key2;
                vlen[3] = SIZE_PUB_KEY;
                hmac_sha256_vector(
                        auth_key,
                        SIZE_256_BITS,  /* auth_key size */
                        4,              /* num_elem */
                        vec,
                        vlen,
                        target     /* output: 32 bytes */
                        );

		if (os_memcmp(compared, target, SIZE_256_BITS))
			break;

                ret = 0;
        } while (0);

        #else /* CONFIG_CRYPTO_INTERNAL */

	u8 hash_src[SIZE_128_BITS * 2 + SIZE_PUB_KEY * 2];
	u8 *tmp;
	u8 target[SIZE_256_BITS];

	do {
		if (!compared || !rsnonce || !psk || !pub_key1 || !pub_key2 || !auth_key)
			break;

		tmp = hash_src;
		os_memcpy(tmp, rsnonce, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, psk, SIZE_128_BITS);
		tmp += SIZE_128_BITS;
		os_memcpy(tmp, pub_key1, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;
		os_memcpy(tmp, pub_key2, SIZE_PUB_KEY);
		tmp += SIZE_PUB_KEY;

		if (!HMAC(EVP_sha256(), auth_key, SIZE_256_BITS, hash_src, tmp - hash_src, target, NULL))
			 	break;

		if (os_memcmp(compared, target, SIZE_256_BITS))
			break;

		ret = 0;
	} while (0);

        #endif /* CONFIG_CRYPTO_INTERNAL */

	return ret;
}


int
eap_wps_config_get_ssid_configuration(struct hostapd_data *hapd,
									  struct wps_config *conf,
									  struct eap_wps_data *data,
									  u8 **config,
									  size_t *config_len,
									  Boolean wrap_credential)
{
	int ret = -1;
	struct wps_data *wps = 0;
	u8 *tmp = 0;
	u8 *tmp2 = 0;
        u8 *last = 0;
	size_t tmp_len;
	size_t tmp2_len;
        size_t last_len = 0;
        u8 nwIdx;       /* must be u8 */
        int quality;
        int ncredentials = 0;

	do {
		if (!hapd || !conf || !config || !config_len)
			break;
		*config = 0;
		*config_len = 0;

                /* Support for bizarre and poorly documented WPS feature
                 * whereby if we are "unconfigured" then we configure ourselves
                 * with random ssid/psk the first time someone asks us to
                 * serve up a configuration.
                 */
                if (conf->wps_state != WPS_WPSSTATE_CONFIGURED) {
                    data->autoconfig = 1;       /* remember for later  */
                }

		if (wrap_credential) {
                        /* This applies ONLY if we are registrar.
                         * As part of M8 we send one or more credentials
                         * that are individually wrapped.
                         */
			if (wps_create_wps_data(&wps))
				break;

                        /* credentials.
                         * To support mixed mode, we send two credentials,
                         * the first is best (WPA2-CCMP==AES) and the
                         * second is worst (WPA-TKIP).
                         * If we are not using mixed mode, both would be
                         * the same, and in case that matters we suppress
                         * the second one.
                         * 0 is best, 1 is worst.
                         * If we start with best, stations that only check
                         * the first one but can't work with best won't
                         * work... probably better start with worst...
                         */
                        #if 0   /* best first */
                        for (quality = WPS_CONFIG_QUALITY_BEST; quality <= WPS_CONFIG_QUALITY_WORST; quality++) 
                        #else   /* worst first */
                        for (quality = WPS_CONFIG_QUALITY_WORST; quality >= WPS_CONFIG_QUALITY_BEST; quality--) 
                        #endif
                        {
                                /* Must put at top: WPS_TYPE_NW_INDEX */
                                nwIdx = quality+1;
                                tmp2 = 0;
                                tmp2_len = 0;
		                if (wps_get_ap_ssid_configuration(
                                            hapd, &tmp2, &tmp2_len, 
                                            !wrap_credential, nwIdx, quality, 
                                            data->autoconfig))
                                        goto FatalError;
                                if (last && last_len == tmp2_len &&
                                        !memcmp(last, tmp2, last_len)) {
                                    /* don't put in duplicate */
                                    free(tmp2); tmp2_len = 0;
                                } else  {
			            if (wps_set_value(wps, WPS_TYPE_CREDENTIAL,
                                            tmp2, tmp2_len))
				        break;
                                    ncredentials++;
                                    free(last); 
                                    last = tmp2; last_len = tmp2_len;
                                    tmp2 = 0; tmp2_len = 0;
                                }
                        }
			if (wps_write_wps_data(wps, &tmp, &tmp_len))
				break;
		} else {
                        quality = WPS_CONFIG_QUALITY_BEST;
                        nwIdx = 0;
		        if (wps_get_ap_ssid_configuration(
                                    hapd, &tmp, &tmp_len, 
                                    !wrap_credential, nwIdx, quality, 
                                    data->autoconfig))
			        break;
                        ncredentials++;
		}
    		*config = os_malloc(tmp_len);
    		if (!*config)
    			break;
    		os_memcpy(*config, tmp, tmp_len);
    		*config_len = tmp_len;

		ret = 0;
	} while (0);

        FatalError:;

	if (tmp)
		os_free(tmp);

	if (tmp2)
		os_free(tmp2);

	if (last)
		os_free(last);

	if (wps)
		wps_destroy_wps_data(&wps);

	if (ret) {
		if (config && *config) {
			os_free(*config);
			*config = 0;
		}
		if (config_len)
			*config_len = 0;
	}

        wpa_printf(MSG_DEBUG, 
        "Leave eap_wps_config_get_ssid_configuration ret=%d ncredentials=%d",
            ret, ncredentials);

	return ret;
}


int
eap_wps_config_set_ssid_configuration(
        struct hostapd_data *hapd,
	struct wps_config *conf,
	u8 *raw_data, 
        size_t raw_data_len,
	char *out_filename)
{
	int ret = -1;
	struct wps_data *wps = 0;
        #define MAX_CRED 8
        u8 *bufs[MAX_CRED] = {};
        size_t lens[MAX_CRED] = {};
        int ncredentials = 0;
        int icredential;

	do {
		if (!hapd || !conf || !raw_data || !raw_data_len)
			break;

                /* This code is used in some cases where the credentials
                 * are wrapped (put inside of a CREDENTIAL element) and
                 * some cases where they are not.
                 * Rather than try to figure out which case is which
                 * (Sony had it wrong) i try for credential first, 
                 * and if that fails assume it is the plain stuff.
                 * ALSO, the Sony case did not account for multiple
                 * credentials, which is accounted for here.
                 * -Ted, Atheros
                 */

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(raw_data, raw_data_len, wps))
			break;

		(void)wps_get_value(wps, WPS_TYPE_CREDENTIAL, 0, &lens[0]);
		if (lens[0]) {
                        /* have credential(s) */
                        int itlv;

                        for (itlv = 0; itlv < wps->count; itlv++) {
                            struct wps_tlv *tlv = wps->tlvs[itlv];
                            if (tlv->type != WPS_TYPE_CREDENTIAL)
                                continue;
                            if (ncredentials == MAX_CRED) {
                                wpa_printf(MSG_INFO, "Extra credentials ignored");
                                break;
                            }
                            lens[ncredentials] = tlv->length;
                            bufs[ncredentials] = malloc(tlv->length);
                            if (!bufs[ncredentials])
                                goto FatalError;
			    if (wps_tlv_get_value(tlv, 
                                    bufs[ncredentials], &lens[ncredentials]))
				goto FatalError;
                            ncredentials++;
                        }
		} else {
			bufs[0] = os_malloc(raw_data_len);
			if (!bufs[0])
				break;
                        ncredentials = 1;
			os_memcpy(bufs[0], raw_data, raw_data_len);
			lens[0] = raw_data_len;
		}
    	        if (wps_set_ap_ssid_configuration(hapd, out_filename, 
                                ncredentials, bufs, lens, 
                                1/*set wps_configured=1*/)) 
    		        break;


		ret = 0;
	} while (0);
        
        FatalError:;

        for (icredential = 0; icredential < ncredentials; icredential++) 
                os_free(bufs[icredential]);
	(void)wps_destroy_wps_data(&wps);

        wpa_printf(MSG_DEBUG, "Leave eap_wps_config_set_ssid_configuration, ret=%d", ret);
	return ret;
}


/* wps_set_ssid_configuration -- set per-bss configuration based upon
 * raw WPS settings information element data as from M8 message.
 * In addition to direct WPS usage, this can also be used in a repeater
 * case, where wpa_supplicant receives M8 data and passes it to hostapd
 * to configure the AP the same.
 */
int wps_set_ssid_configuration(
        struct hostapd_data *hapd,
	u8 *raw_data, size_t raw_data_len,
        int do_save,    /* if zero, we don't save the changes */
        int do_restart  /* if zero, we don't restart after saving changes */
        )
{
#define EAP_WPS_COMP_FILENAME "eap_wps_cmp.conf"
	int ret = -1;
	struct wps_config *conf = hapd->conf->wps;
        char *filepath = NULL;

	do {
		if (!raw_data || !raw_data_len || !hapd || !conf)
			break;

                /* Make temporary file in same directory as final one */
                filepath = wps_config_temp_filepath_make(
                        hapd->conf->config_fname,
                        EAP_WPS_COMP_FILENAME);
                if (filepath == NULL) break;

		if (eap_wps_config_set_ssid_configuration(hapd, conf, raw_data,
												  raw_data_len,
												  filepath))
			break;
                /* Policy: if we are marking ourselves as unconfigured,
                 * then anyone can configure us so long as they have the PIN.
                 * If we have marked ourselves as configured, then we only
                 * allow this via explicit "configme" command.
                 */
                if (do_save) {
                        wpa_printf(MSG_INFO, "Allowed to save new configuration.");
                        /* If allowed to do so, move the temporary file
                         * to the one we are configured from, and reload
                         * configuration from it.
                         */
                        if (rename(filepath, hapd->conf->config_fname)) {
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
                        if (do_restart) {
                            hostapd_reload_configuration(hapd);
                        }
                        
		        ret = 0;
                } else {
                        wpa_printf(MSG_INFO, "Not allowed to save configuration: temp file remains!");
                }


	} while (0);

        os_free(filepath);
        wpa_printf(MSG_DEBUG, "Leave wps_set_ssid_configuration ret=%d", ret);
	return ret;
#undef EAP_WPS_COMP_FILENAME
}


static void eap_wps_set_ssid_configuration(struct eap_sm *sm,
	u8 *raw_data, size_t raw_data_len)
{
        struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
        int do_restart = 1;     /* always restart if we save */
        int do_save;
        char *encode_buf;
        
        if (!hapd || !hapd->conf || !hapd->conf->wps)
                return;


        /* Report change to possible monitoring program */
        encode_buf = base64_encode(raw_data, raw_data_len, NULL, 0);
        if (encode_buf == NULL)
                return;
        eap_wps_request(hapd, CTRL_REQ_TYPE_SELFCONFIGURE, encode_buf);
        free(encode_buf);

        /* if selfconfiguration is disabled, don't do anything more! */
        if (hostapd_self_configuration_protect) {
                wpa_printf(MSG_INFO, 
                "It is now up to manager program to use self-configure info");
                return;
        }

        /* Policy: if we are marking ourselves as unconfigured,
         * then anyone can configure us so long as they have the PIN.
         * If we have marked ourselves as configured, then we only
         * allow this via explicit "configme" command.
         * Otherwise, we go ahead and make a temporary file but don't
         * do anything with that temporary file.
         */
        do_save = (hapd->conf->wps->do_save || 
                (hapd->conf->wps->wps_state == WPS_WPSSTATE_UNCONFIGURED));
        (void) wps_set_ssid_configuration(hapd, raw_data, raw_data_len,
            do_save, do_restart);
        return;
}


u8 *
eap_wps_config_build_message_M1(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !msg_len)
			break;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M1;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* UUID-E */
		if (!conf->uuid_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_UUID_E, conf->uuid, sizeof(conf->uuid)))
			break;

		/* MAC Address */
		if (!conf->mac_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_MAC_ADDR, conf->mac, sizeof(conf->mac)))
			break;

		/* Enrollee Nonce */
		RAND_bytes(data->nonce, sizeof(data->nonce));
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, data->nonce, sizeof(data->nonce)))
			break;

		/* Public Key */
		if (!data->preset_pubKey) {
			if (data->dh_secret)
				eap_wps_free_dh(&data->dh_secret);
			if (eap_wps_generate_public_key(&data->dh_secret, data->pubKey))
				break;
		}
		if (wps_set_value(wps, WPS_TYPE_PUBLIC_KEY, data->pubKey, sizeof(data->pubKey)))
			break;

		/* Authentication Type Flags */
		if (wps_set_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &conf->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_set_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &conf->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_set_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &conf->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_METHODS, &conf->config_methods, 0))
			break;

		/* Wi-Fi Protected Setup State */
		if (wps_set_value(wps, WPS_TYPE_WPSSTATE, &conf->wps_state, 0))
			break;

		/* Manufacturer */
		if (wps_set_value(wps, WPS_TYPE_MANUFACTURER, conf->manufacturer, conf->manufacturer_len))
			break;

		/* Model Name */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NAME, conf->model_name, conf->model_name_len))
			break;

		/* Model Number */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NUMBER, conf->model_number, conf->model_number_len))
			break;

		/* Serial Number */
		if (wps_set_value(wps, WPS_TYPE_SERIAL_NUM, conf->serial_number, conf->serial_number_len))
			break;

		/* Primary Device Type */
		if (wps_set_value(wps, WPS_TYPE_PRIM_DEV_TYPE, conf->prim_dev_type, sizeof(conf->prim_dev_type)))
			break;

		/* Device Name */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_NAME, conf->dev_name, conf->dev_name_len))
			break;

		/* RF Bands */
		if (wps_set_value(wps, WPS_TYPE_RF_BANDS, &conf->rf_bands, 0))
			break;

		/* Association State */
		if (wps_set_value(wps, WPS_TYPE_ASSOC_STATE, &data->assoc_state, 0))
			break;

		/* Device Passwork ID */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_PWD_ID, &data->dev_pwd_id, 0))
			break;

		/* Configuration Error */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_ERROR, &data->config_error, 0))
			break;

		/* OS Version */
		if (wps_set_value(wps, WPS_TYPE_OS_VERSION, &conf->os_version, 0))
			break;

                #if 0   /* This breaks some stations and is not necessary */
                /* Atheros Extensions */
                if (wps_config_add_atheros_wps_ext(hapd, wps))
                        break;
                #endif

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M1(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M1(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int
eap_wps_config_process_message_M1(struct wps_config *conf,
								  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_data *wps = 0;
	u8 msg_type;
	struct eap_wps_target_info *target;
	size_t length;

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		eap_wps_clear_target_info(data);

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &target->version, 0))
			break;
		if ((target->version != WPS_VERSION) && (target->version != WPS_VERSION_EX))
			break;

		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M1)
			break;

		/* UUID-E */
		length = sizeof(target->uuid);
		if (wps_get_value(wps, WPS_TYPE_UUID_E, target->uuid, &length))
			break;

		/* MAC Address */
		length = sizeof(target->mac);
		if (wps_get_value(wps, WPS_TYPE_MAC_ADDR, target->mac, &length))
			break;
		target->mac_set = 1;

		/* Enrollee Nonce */
		length = sizeof(target->nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, &length))
			break;

		/* Public Key */
		length = sizeof(target->pubKey);
		if (wps_get_value(wps, WPS_TYPE_PUBLIC_KEY, target->pubKey, &length))
			break;
                if (0 < length && length < SIZE_PUB_KEY) {
                        /* Defensive programming in case other side omitted
                        *   leading zeroes 
                        */
                        memmove(target->pubKey+(SIZE_PUB_KEY-length), 
                            target->pubKey, length);
                        memset(target->pubKey, 0, (SIZE_PUB_KEY-length));
                } else if (length != SIZE_PUB_KEY)
                        break;
		if (data->preset_pubKey) {
			if (eap_wps_oobdevpwd_public_key_hash_validation(data->pubKey, target->pubKey))
				break;

			os_memset(data->pubKey, 0, sizeof(data->pubKey));
			data->preset_pubKey = 0;
		}

		/* Authentication Type Flags */
		if (wps_get_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &target->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_get_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &target->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_get_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &target->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_METHODS, &target->config_methods, 0))
			break;

		/* Manufacturer */
		(void)wps_get_value(wps, WPS_TYPE_MANUFACTURER, 0, &length);
		if (!length)
			break;
		target->manufacturer = os_zalloc(length+1);
		target->manufacturer_len = length;
		if (wps_get_value(wps, WPS_TYPE_MANUFACTURER, target->manufacturer, &length))
			break;

		/* Model Name */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NAME, 0, &length);
		if (!length)
			break;
		target->model_name = os_zalloc(length+1);
		target->model_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NAME, target->model_name, &length))
			break;

		/* Model Number */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, 0, &length);
		if (!length)
			break;
		target->model_number = os_zalloc(length+1);
		target->model_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, target->model_number, &length))
			break;

		/* Serial Number */
		(void)wps_get_value(wps, WPS_TYPE_SERIAL_NUM, 0, &length);
		if (!length)
			break;
		target->serial_number = os_zalloc(length+1);
		target->serial_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_SERIAL_NUM, target->serial_number, &length))
			break;

		/* Primary Device Type */
		length = sizeof(target->prim_dev_type);
		if (wps_get_value(wps, WPS_TYPE_PRIM_DEV_TYPE, target->prim_dev_type, &length))
			break;

		/* Device Name */
		(void)wps_get_value(wps, WPS_TYPE_DEVICE_NAME, 0, &length);
		if (!length)
			break;
		target->dev_name = os_zalloc(length+1);
		target->dev_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_DEVICE_NAME, target->dev_name, &length))
			break;

		/* RF Bands */
		if (wps_get_value(wps, WPS_TYPE_RF_BANDS, &target->rf_bands, 0))
			break;

		/* Association State */
		if (wps_get_value(wps, WPS_TYPE_ASSOC_STATE, &target->assoc_state, 0))
			break;

		/* Configuration Error */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
			break;

		/* OS Version */
		if (wps_get_value(wps, WPS_TYPE_OS_VERSION, &target->os_version, 0))
			break;

		ret = 0;
	} while (0);

	if (ret)
		eap_wps_clear_target_info(data);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M1(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

                #if 0   /* ORIGINAL */
                if (conf->upnp_enabled) {
                        wpa_printf(MSG_DEBUG, "WPS process M1 proxy it to UPNP");
			(void)wps_opt_upnp_send_wlan_eap_event(
                                hapd->wps_opt_upnp, sm, data);

		} else
                #else
                /* Proxy M1 messages always! But if no "selected registrar"
                 * then proceed to handle it ourself.
                 */
                if (!conf->wps_upnp_disable) {
                        wpa_printf(MSG_DEBUG, "WPS process M1 proxy it to UPNP");
			(void)wps_opt_upnp_send_wlan_eap_event(
                                hapd->wps_opt_upnp, sm, data);

		}
                /* why?: if (!conf->upnp_enabled) */
                #endif
#endif /* WPS_OPT_UPNP */
		if (eap_wps_config_process_message_M1(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


static int
eap_wps_config_build_message_M2_M2D(
        struct hostapd_data *hapd, 
        struct wps_config *conf,
	struct eap_wps_data *data, 
        struct wps_data *wps)
{
	int ret = -1;

	do {
		if (!conf || !data || !wps)
			break;

		/* Authentication Type Flags */
		if (wps_set_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &conf->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_set_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &conf->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_set_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &conf->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_METHODS, &conf->config_methods, 0))
			break;

		/* Manufacturer */
		if (wps_set_value(wps, WPS_TYPE_MANUFACTURER, conf->manufacturer, conf->manufacturer_len))
			break;

		/* Model Name */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NAME, conf->model_name, conf->model_name_len))
			break;

		/* Model Number */
		if (wps_set_value(wps, WPS_TYPE_MODEL_NUMBER, conf->model_number, conf->model_number_len))
			break;

		/* Serial Number */
		if (wps_set_value(wps, WPS_TYPE_SERIAL_NUM, conf->serial_number, conf->serial_number_len))
			break;

		/* Primary Device Type */
		if (wps_set_value(wps, WPS_TYPE_PRIM_DEV_TYPE, conf->prim_dev_type, sizeof(conf->prim_dev_type)))
			break;

		/* Device Name */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_NAME, conf->dev_name, conf->dev_name_len))
			break;

		/* RF Bands */
		if (wps_set_value(wps, WPS_TYPE_RF_BANDS, &conf->rf_bands, 0))
			break;

		/* Association State */
		if (wps_set_value(wps, WPS_TYPE_ASSOC_STATE, &data->assoc_state, 0))
			break;

		/* Configuration Error */
		if (wps_set_value(wps, WPS_TYPE_CONFIG_ERROR, &data->config_error, 0))
			break;

		ret = 0;
	} while (0);

	return ret;
}


static u8 *eap_wps_config_build_message_M2(
	        struct hostapd_data *hapd,
                struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 kdk[SIZE_256_BITS];
	u8 keys[KDF_OUTPUT_SIZE];
	u8 authenticator[SIZE_8_BYTES];
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M2;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Registrar Nonce */
		RAND_bytes(data->nonce, sizeof(data->nonce));
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, data->nonce, sizeof(data->nonce)))
			break;

		/* UUID-R */
		if (!conf->uuid_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_UUID_R, conf->uuid, sizeof(conf->uuid)))
			break;

		/* Public Key */
		if (!data->preset_pubKey) {
			if (data->dh_secret)
				eap_wps_free_dh(&data->dh_secret);
			if (eap_wps_generate_public_key(&data->dh_secret, data->pubKey))
				break;
		}
		if (wps_set_value(wps, WPS_TYPE_PUBLIC_KEY, data->pubKey, sizeof(data->pubKey)))
			break;

		/* M2/M2D common data */
		if (eap_wps_config_build_message_M2_M2D(hapd, conf, data, wps))
			break;

		/* Device Password ID */
		if (wps_set_value(wps, WPS_TYPE_DEVICE_PWD_ID, &data->dev_pwd_id, 0))
			break;

		/* OS Version */
		if (wps_set_value(wps, WPS_TYPE_OS_VERSION, &conf->os_version, 0))
			break;

		/* Encrypted Settings */
#if 0
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, encrs_len))
			break;
#endif
                #if 0   /* This breaks some stations and is not necessary */
                /* Atheros Extensions */
                if (wps_config_add_atheros_wps_ext(hapd, wps))
                        break;
                #endif

		/* Generate KDK */
		if (!target->mac_set)
			break;
		if (eap_wps_generate_kdk(data, target->nonce, target->mac, data->nonce, kdk))
			break;

		/* Key Derivation Function */
		if (eap_wps_key_derive_func(data, kdk, keys))
			break;
		os_memcpy(data->authKey, keys, SIZE_256_BITS);
		os_memcpy(data->keyWrapKey, keys + SIZE_256_BITS, SIZE_128_BITS);
		os_memcpy(data->emsk, keys + SIZE_256_BITS + SIZE_128_BITS, SIZE_256_BITS);
                /* last 16 bytes are unused */

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M2(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

#ifdef WPS_OPT_UPNP
		conf->upnp_enabled = 0;
#endif /* WPS_OPT_UPNP */
		msg = eap_wps_config_build_message_M2(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


static u8 *eap_wps_config_build_message_M2D(
	        struct hostapd_data *hapd,
                struct wps_config *conf,
		struct eap_wps_data *data,
		size_t *msg_len)
{
	u8 *msg = 0;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M2D;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Registrar Nonce */
		RAND_bytes(data->nonce, sizeof(data->nonce));
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, data->nonce, sizeof(data->nonce)))
			break;

		/* UUID-R */
		if (!conf->uuid_set)
			break;
		if (wps_set_value(wps, WPS_TYPE_UUID_R, conf->uuid, sizeof(conf->uuid)))
			break;

		/* M2/M2D common data */
		if (eap_wps_config_build_message_M2_M2D(hapd, conf, data, wps))
			break;

		/* OS Version */
		if (wps_set_value(wps, WPS_TYPE_OS_VERSION, &conf->os_version, 0))
			break;

                #if 0   /* This breaks some stations and is not necessary */
                /* Atheros Extensions */
                if (wps_config_add_atheros_wps_ext(hapd, wps))
                        break;
                #endif

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M2D(struct eap_sm *sm,
									 struct eap_wps_data *data,
									 size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (/*WAS conf->upnp_iface*/ !conf->wps_upnp_disable) {
			u8 msg_type;
                        /* Arghhh!! Sony code was syncronous and expected
                         * and waited 2 seconds for registrar to send
                         * a message back... and didnt' do anything smart
                         * if the registrar didn't meet that deadline.
                         * With #ifdef WPS_OPT_TINYUPNP we instead stall
                         * the eap s.m. until we get a message from
                         * registrar, so this >should< succeed here.
                         */
                        /* Oddly! the received message is passed back
                         * both by return value "msg" and in data->sndMsg
                         * (both have same data, but are different allocations).
                         */
			msg = wps_opt_upnp_received_message(hapd->wps_opt_upnp, data,
												msg_len);
			if (msg) {
				size_t length;
				do {
					target = data->target;
					if (!target)
						break;

					/* Get Enrollee Nonce from M1 */
					if (wps_create_wps_data(&wps))
						break;
					if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
						break;

					/* Message Type */
					if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
						break;
					if (msg_type != WPS_MSGTYPE_M1)
						break;

					/* Enrollee Nonce */
					length = sizeof(target->nonce);
					if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, &length))
						break;
					(void)wps_destroy_wps_data(&wps);

					/* Get Registrar Nonce from M2 */
					if (wps_create_wps_data(&wps))
						break;
					if (wps_parse_wps_data(data->sndMsg, data->sndMsgLen, wps))
						break;

					/* Message Type */
					if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
						break;
                                        wpa_printf(MSG_INFO,
                                            "M2* Msg type %d at %s:%d",
                                                msg_type,
                                                __FILE__, __LINE__);
					if ((msg_type != WPS_MSGTYPE_M2) && (msg_type != WPS_MSGTYPE_M2D))
						break;

					/* Registrar Nonce */
					length = sizeof(data->nonce);
					if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, data->nonce, &length))
						break;
					(void)wps_destroy_wps_data(&wps);
				} while (0);
				(void)wps_destroy_wps_data(&wps);

				msg_type = wps_get_message_type(msg, *msg_len);
                                wpa_printf(MSG_INFO,
                                    "New Msg type %d at %s:%d",
                                        msg_type,
                                        __FILE__, __LINE__);
				switch(msg_type) {
				case WPS_MSGTYPE_M2:
                                        /* Here we finally got the M2.
                                         * Problem is that we could
                                         * conceivably get another M2D
                                         * from some registrar at any time,
                                         * which will mess us up since
                                         * we will probably confuse it with
                                         * an M4 or somehow mistreat it.
                                         * Sigh...
                                         */
					data->state = M3;
					break;
				case WPS_MSGTYPE_M2D:
                                        /* A registrar sent us an M2D
                                         * which is just information of
                                         * not much use but which we have
                                         * to pass to the station and which
                                         * the station has to ACK, thus
                                         * we expect an ACK.
                                         * Oddly, a single vista machine
                                         * will sometimes (as registrar)
                                         * send M2D and then M2 later.
                                         */
					data->state = ACK;
					break;
				case WPS_MSGTYPE_NACK:
					data->state = NACK;
                                        conf->nfailure++;
					break;
				default:
					if (msg) {
						os_free(msg);
						msg = 0;
					}
					*msg_len = 0;
					break;
				}
			}
		}

		if (!msg && M2D1 == data->state)
#endif /* WPS_OPT_UPNP */
		msg = eap_wps_config_build_message_M2D(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


static int eap_wps_config_process_message_M2_M2D(struct wps_config *conf,
	       struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 tmp[SIZE_64_BYTES];
	size_t length;

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		eap_wps_clear_target_info(data);

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &target->version, 0))
			break;
		if ((target->version != WPS_VERSION) && (target->version != WPS_VERSION_EX))
			break;

		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if ((msg_type != WPS_MSGTYPE_M2) && (msg_type != WPS_MSGTYPE_M2D))
			break;

		/* Enrollee Nonce */
		length = sizeof(tmp);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, tmp, &length))
			break;
		if (os_memcmp(data->nonce, tmp, sizeof(data->nonce)))
			break;

		/* Registrar Nonce */
		length = sizeof(target->nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, &length))
			break;

		/* UUID-R */
		length = sizeof(target->uuid);
		if (wps_get_value(wps, WPS_TYPE_UUID_R, target->uuid, &length))
			break;

		/* Authentication Type Flags */
		if (wps_get_value(wps, WPS_TYPE_AUTH_TYPE_FLAGS, &target->auth_type_flags, 0))
			break;

		/* Encryption Type Flags */
		if (wps_get_value(wps, WPS_TYPE_ENCR_TYPE_FLAGS, &target->encr_type_flags, 0))
			break;

		/* Connection Type Flags */
		if (wps_get_value(wps, WPS_TYPE_CONN_TYPE_FLAGS, &target->conn_type_flags, 0))
			break;

		/* Config Methods */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_METHODS, &target->config_methods, 0))
			break;

		/* Manufacturer */
		(void)wps_get_value(wps, WPS_TYPE_MANUFACTURER, 0, &length);
		if (!length)
			break;
		target->manufacturer = os_zalloc(length+1);
		target->manufacturer_len = length;
		if (wps_get_value(wps, WPS_TYPE_MANUFACTURER, target->manufacturer, &length))
			break;

		/* Model Name */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NAME, 0, &length);
		if (!length)
			break;
		target->model_name = os_zalloc(length+1);
		target->model_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NAME, target->model_name, &length))
			break;

		/* Model Number */
		(void)wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, 0, &length);
		if (!length)
			break;
		target->model_number = os_zalloc(length+1);
		target->model_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_MODEL_NUMBER, target->model_number, &length))
			break;

		/* Serial Number */
		(void)wps_get_value(wps, WPS_TYPE_SERIAL_NUM, 0, &length);
		if (!length)
			break;
		target->serial_number = os_zalloc(length+1);
		target->serial_number_len = length;
		if (wps_get_value(wps, WPS_TYPE_SERIAL_NUM, target->serial_number, &length))
			break;

		/* Primary Device Type */
		length = sizeof(target->prim_dev_type);
		if (wps_get_value(wps, WPS_TYPE_PRIM_DEV_TYPE, target->prim_dev_type, &length))
			break;

		/* Device Name */
		(void)wps_get_value(wps, WPS_TYPE_DEVICE_NAME, 0, &length);
		if (!length)
			break;
		target->dev_name = os_zalloc(length+1);
		target->dev_name_len = length;
		if (wps_get_value(wps, WPS_TYPE_DEVICE_NAME, target->dev_name, &length))
			break;

		/* RF Bands */
		if (wps_get_value(wps, WPS_TYPE_RF_BANDS, &target->rf_bands, 0))
			break;

		/* Association State */
		if (wps_get_value(wps, WPS_TYPE_ASSOC_STATE, &target->assoc_state, 0))
			break;

		/* Configuration Error */
		if (wps_get_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
			break;

		/* OS Version */
		if (wps_get_value(wps, WPS_TYPE_OS_VERSION, &target->os_version, 0))
			break;

		ret = 0;
	} while (0);

	if (ret)
		eap_wps_clear_target_info(data);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


int eap_wps_config_process_message_M2(struct wps_config *conf,
		struct eap_wps_data *data,
		Boolean *with_config)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 kdk[SIZE_256_BITS];
	u8 keys[KDF_OUTPUT_SIZE];
	size_t length;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (with_config)
			*with_config = 0;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M2)
			break;

		if (eap_wps_config_process_message_M2_M2D(conf, data))
			break;

		/* Public Key */
		length = sizeof(target->pubKey);
		if (wps_get_value(wps, WPS_TYPE_PUBLIC_KEY, target->pubKey, &length))
			break;
                if (0 < length && length < SIZE_PUB_KEY) {
                        /* Defensive programming in case other side omitted
                        *   leading zeroes 
                        */
                        memmove(target->pubKey+(SIZE_PUB_KEY-length), 
                            target->pubKey, length);
                        memset(target->pubKey, 0, (SIZE_PUB_KEY-length));
                } else if (length != SIZE_PUB_KEY)
                        break;

		/* Device Password ID */
		if (wps_get_value(wps, WPS_TYPE_DEVICE_PWD_ID, &target->dev_pwd_id, 0))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* Generate KDK */
		if (eap_wps_generate_kdk(data, data->nonce, conf->mac, target->nonce, kdk))
			break;

		/* Key Derivation Function */
		if (eap_wps_key_derive_func(data, kdk, keys))
			break;
		os_memcpy(data->authKey, keys, SIZE_256_BITS);
		os_memcpy(data->keyWrapKey, keys + SIZE_256_BITS, SIZE_128_BITS);
		os_memcpy(data->emsk, keys + SIZE_256_BITS + SIZE_128_BITS, SIZE_256_BITS);
                /* last 16 bytes are unused */

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (length) {
			u8 *encrs = 0;
			u8 *iv, *cipher;
			int cipher_len;
			u8 *config = 0;
			int config_len;
			int fail = 1;

			do {
				encrs = os_malloc(length);
				if (!encrs)
					break;
				if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, &length))
					break;

				iv = encrs;
				cipher = encrs + SIZE_128_BITS;
				cipher_len = length - SIZE_128_BITS;
				if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &config, &config_len))
					break;

				target->config = config;
				target->config_len = config_len;

				fail = 0;
			} while (0);
			
			if (encrs)
				os_free(encrs);
			if (fail && config) {
				os_free(config);
				target->config = 0;
				target->config_len = 0;
			}
			if (fail)
				break;

			if (with_config)
				*with_config = 1;
		}

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M2(struct eap_sm *sm,
									  struct eap_wps_data *data,
									  Boolean *with_config)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M2(conf, data, with_config))
			break;

		ret = 0;
	} while (0);

	return ret;
}


int eap_wps_config_process_message_M2D(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M2D)
			break;

		if (eap_wps_config_process_message_M2_M2D(conf, data))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M2D(struct eap_sm *sm,
									   struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M2D(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 * eap_wps_config_build_message_M3(
        struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 authenticator[SIZE_8_BYTES];
	u8 u8val;
	size_t length;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M3;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Generate Device Password, if it hasn't been set yet */
		if (!data->dev_pwd_len) {
                        /* Note this is equivalent to !conf->wps_job_busy */
                        if (conf->default_pin && conf->default_pin[0] &&
                                        sizeof(data->dev_pwd) > 
                                                strlen(conf->default_pin)) {
                                wpa_printf(MSG_INFO, "eap_wps using default PIN");
                                strcpy((char *)data->dev_pwd,
                                        conf->default_pin);
                                data->dev_pwd_len = strlen(conf->default_pin);
                                data->dev_pwd_id = 0;  /* implies PIN method */
                        } else {
                                wpa_printf(MSG_WARNING, "Failure: WPS password/PIN not set!");
                                break;
                        }
		}

		/* E-Hash1 */
		if (eap_wps_generate_hash(data, data->dev_pwd,
								   data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   data->pubKey, target->pubKey, data->authKey,
								   data->psk1, data->snonce1, data->hash1))
			break;
		if(wps_set_value(wps, WPS_TYPE_E_HASH1, data->hash1, sizeof(data->hash1)))
			break;

		/* E-Hash2 */
		if (eap_wps_generate_hash(data, data->dev_pwd + data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   data->dev_pwd_len/2,
								   data->pubKey, target->pubKey, data->authKey,
								   data->psk2, data->snonce2, data->hash2))
			break;
		if(wps_set_value(wps, WPS_TYPE_E_HASH2, data->hash2, sizeof(data->hash2)))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M3(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M3(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M3(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 tmp[SIZE_64_BYTES];
	size_t length;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M3)
			break;

		/* Registrar Nonce */
		length = sizeof(tmp);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, tmp, &length))
			break;
		if (os_memcmp(tmp, data->nonce, sizeof(data->nonce)))
			break;

		/* E-Hash1 */
		length = sizeof(target->hash1);
		if (wps_get_value(wps, WPS_TYPE_E_HASH1, target->hash1, &length))
			break;

		/* E-Hash2 */
		length = sizeof(target->hash2);
		if (wps_get_value(wps, WPS_TYPE_E_HASH2, target->hash2, &length))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M3(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled) {
			if (wps_opt_upnp_send_wlan_eap_event(hapd->wps_opt_upnp, sm, data))
				break;
		} else
#endif /* WPS_OPT_UPNP */
		if (eap_wps_config_process_message_M3(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M4(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 authenticator[SIZE_8_BYTES];
	u8 u8val;
	size_t length;
	u8 *encrs;
	size_t encrs_len;

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M4;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		if (!data->dev_pwd_len)
			break;

		/* R-Hash1 */
		if (eap_wps_generate_hash(data, data->dev_pwd,
								   data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   target->pubKey, data->pubKey, data->authKey,
								   data->psk1, data->snonce1, data->hash1))
			break;
		if(wps_set_value(wps, WPS_TYPE_R_HASH1, data->hash1, sizeof(data->hash1)))
			break;

		/* R-Hash2 */
		if (eap_wps_generate_hash(data, data->dev_pwd + data->dev_pwd_len/2 + data->dev_pwd_len%2,
								   data->dev_pwd_len/2,
								   target->pubKey, data->pubKey, data->authKey,
								   data->psk2, data->snonce2, data->hash2))
			break;
		if(wps_set_value(wps, WPS_TYPE_R_HASH2, data->hash2, sizeof(data->hash2)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(hapd, data, WPS_TYPE_R_SNONCE1, data->snonce1, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;


		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M4(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled)
			msg = wps_opt_upnp_received_message(hapd->wps_opt_upnp, data,
												msg_len);
		else
#endif /* WPS_OPT_UPNP */
		msg = eap_wps_config_build_message_M4(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M4(struct wps_config *conf,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M4)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* R-Hash1 */
		length = sizeof(target->hash1);
		if (wps_get_value(wps, WPS_TYPE_R_HASH1, target->hash1, &length))
			break;

		/* R-Hash2 */
		length = sizeof(target->hash2);
		if (wps_get_value(wps, WPS_TYPE_R_HASH2, target->hash2, &length))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_R_SNONCE1, rsnonce, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* RHash1 validation */
		if (eap_wps_hash_validation(data, target->hash1, rsnonce, data->psk1, data->pubKey, target->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M4(struct eap_sm *sm,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M4(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M5(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	size_t encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M5;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(hapd, data, WPS_TYPE_E_SNONCE1, data->snonce1, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M5(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M5(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M5(struct wps_config *conf,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	u8 version;
	struct wps_data *wps = 0;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M5)
			break;

		/* Registrar Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_E_SNONCE1, rsnonce, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* EHash1 validation */
		if (eap_wps_hash_validation(data, target->hash1, rsnonce, data->psk1, target->pubKey, data->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M5(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled) {
			if (wps_opt_upnp_send_wlan_eap_event(hapd->wps_opt_upnp, sm, data))
				break;
		} else
#endif /* WPS_OPT_UPNP */
		if (eap_wps_config_process_message_M5(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M6(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	size_t encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M6;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(hapd, data, WPS_TYPE_R_SNONCE2, data->snonce2, 0, 0, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M6(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled)
			msg = wps_opt_upnp_received_message(hapd->wps_opt_upnp, data,
												msg_len);
		else
#endif /* WPS_OPT_UPNP */
		msg = eap_wps_config_build_message_M6(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M6(struct wps_config *conf,
		 struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M6)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_R_SNONCE2, rsnonce, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* RHash2 validation */
		if (eap_wps_hash_validation(data, target->hash2, rsnonce, data->psk2, data->pubKey, target->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M6(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M6(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M7(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	size_t encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M7;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(hapd, data, WPS_TYPE_E_SNONCE2, data->snonce2, data->config, data->config_len, data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;
		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M7(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

		msg = eap_wps_config_build_message_M7(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M7(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 rsnonce[SIZE_NONCE];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M7)
			break;

		/* Registrar Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len, data->authKey,
											WPS_TYPE_E_SNONCE2, rsnonce, keyWrapAuth))
			break;
		if (target->config)
			os_free(target->config);
		target->config = decrypted;
		target->config_len = decrypted_len;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		/* EHash2 validation */
		if (eap_wps_hash_validation(data, target->hash2, rsnonce, data->psk2, target->pubKey, data->pubKey, data->authKey))
			break;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (ret && decrypted) {
		os_free(decrypted);
		if (data->target) {
			target = data->target;
			target->config = 0;
			target->config_len = 0;
		}
	}

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M7(struct eap_sm *sm,
									  struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled) {
			if (wps_opt_upnp_send_wlan_eap_event(hapd->wps_opt_upnp, sm, data))
				break;
		} else
#endif /* WPS_OPT_UPNP */
		if (eap_wps_config_process_message_M7(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_M8(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	u8 u8val;
	size_t length;
	u8 *encrs = 0;
	size_t encrs_len;
	u8 authenticator[SIZE_8_BYTES];

	do {
		if (!conf || !data || !data->target || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		u8val = WPS_MSGTYPE_M8;
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, target->nonce, sizeof(target->nonce)))
			break;

		/* Encrypted Settings */
		if (eap_wps_encrsettings_creation(hapd, data, 0, 0,
					data->config, data->config_len,
					data->authKey, data->keyWrapKey, &encrs, &encrs_len))
			break;

		if (wps_set_value(wps, WPS_TYPE_ENCR_SETTINGS, encrs, (u16)encrs_len))
			break;

		/* Authenticator */
		length = 0;
		if (wps_write_wps_data(wps, &msg, &length))
			break;
		if (eap_wps_calcurate_authenticator(data, msg, length,
									data->authKey, authenticator)) {
			os_free(msg);
			msg = 0;
			break;
		}
		os_free(msg);
		msg = 0;
		if (wps_set_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, sizeof(authenticator)))
			break;

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	if (encrs)
		os_free(encrs);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_M8(struct eap_sm *sm,
									struct eap_wps_data *data,
									size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !msg_len || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled)
			msg = wps_opt_upnp_received_message(hapd->wps_opt_upnp, data,
												msg_len);
		else
#endif /* WPS_OPT_UPNP */
		msg = eap_wps_config_build_message_M8(hapd, conf, data, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_M8(struct wps_config *conf,
		struct eap_wps_data *data)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 msg_type;
	u8 nonce[SIZE_NONCE];
	size_t length;
	u8 *tmp = 0, *iv, *cipher, *decrypted = 0;
	int cipher_len, decrypted_len;
	u8 authenticator[SIZE_8_BYTES];
	u8 keyWrapAuth[SIZE_64_BITS];

	do {
		if (!conf || !data || !data->target)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;
		if (msg_type != WPS_MSGTYPE_M8)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(data->nonce, nonce, sizeof(data->nonce)))
			break;

		/* Encrypted Settings */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, 0, &length);
		if (!length)
			break;
		tmp = os_malloc(length);
		if (!tmp)
			break;
		if (wps_get_value(wps, WPS_TYPE_ENCR_SETTINGS, tmp, &length))
			break;
		iv = tmp;
		cipher = tmp + SIZE_128_BITS;
		cipher_len = length - SIZE_128_BITS;
		if (eap_wps_decrypt_data(data, iv, cipher, cipher_len, data->keyWrapKey, &decrypted, &decrypted_len))
			break;
		if (eap_wps_encrsettings_validation(data, decrypted, decrypted_len,
											data->authKey, 0, 0, keyWrapAuth))
			break;

		/* Authenticator */
		length = sizeof(authenticator);
		if (wps_get_value(wps, WPS_TYPE_AUTHENTICATOR, authenticator, &length))
			break;

		/* HMAC validation */
		if (eap_wps_hmac_validation(data, authenticator, data->authKey))
			break;

		if (target->config)
			os_free(target->config);
		target->config = decrypted;
		target->config_len = decrypted_len;

		ret = 0;
	} while (0);

	if (tmp)
		os_free(tmp);
	if (ret && decrypted)
		os_free(decrypted);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_M8(struct eap_sm *sm,
									 struct eap_wps_data *data)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

	do {
		if (!sm || !data || !conf)
			break;

		if (eap_wps_config_process_message_M8(conf, data))
			break;

		ret = 0;
	} while (0);

	return ret;
}


u8 *eap_wps_config_build_message_special(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	u8 msg_type,
	u8 *e_nonce, u8 *r_nonce,
	size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_data *wps = 0;
	struct eap_wps_target_info *target;
	size_t length;

	do {
		if (!conf || !data || !data->target || !e_nonce || !r_nonce || !msg_len)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (!conf->version)
			break;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &conf->version, 0))
			break;

		/* Message Type */
		if (wps_set_value(wps, WPS_TYPE_MSG_TYPE, &msg_type, 0))
			break;

		/* Enrollee Nonce */
		if (wps_set_value(wps, WPS_TYPE_ENROLLEE_NONCE, e_nonce, SIZE_UUID))
			break;

		/* Registrar Nonce */
		if (wps_set_value(wps, WPS_TYPE_REGISTRAR_NONCE, r_nonce, SIZE_UUID))
			break;

		/* Configuration Error */
		if (WPS_MSGTYPE_NACK == msg_type) {
			if (wps_set_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
				break;
		}

		if (wps_write_wps_data(wps, &msg, &length))
			break;

		*msg_len = length;

		if (data->sndMsg) {
			os_free(data->sndMsg);
			data->sndMsg = 0;
			data->sndMsgLen = 0;
		}
		data->sndMsg = os_malloc(*msg_len);
		if (!data->sndMsg) {
			os_free(msg);
			msg = 0;
			*msg_len = 0;
			break;
		}

		os_memcpy(data->sndMsg, msg, *msg_len);
		data->sndMsgLen = *msg_len;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return msg;
}


static u8 *eap_wps_build_message_special(struct eap_sm *sm,
										 struct eap_wps_data *data,
										 u8 msg_type,
										 u8 *e_nonce, u8 *r_nonce,
										 size_t *msg_len)
{
	u8 *msg = 0;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

	do {
		if (!sm || !data || !e_nonce || !r_nonce || !msg_len || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (conf->upnp_enabled)
			msg = wps_opt_upnp_received_message(hapd->wps_opt_upnp, data,
												msg_len);
		else
#endif /* WPS_OPT_UPNP */
		msg = eap_wps_config_build_message_special(hapd, conf, data, msg_type,
			e_nonce, r_nonce, msg_len);
	} while (0);

	return msg;
}


int eap_wps_config_process_message_special(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	u8 msg_type,
	u8 *e_nonce, 
        u8 *r_nonce)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	struct wps_data *wps = 0;
	u8 version;
	u8 u8val;
	u8 nonce[SIZE_NONCE];
	size_t length;

	do {
		if (!conf || !data || !data->target || !e_nonce || !r_nonce)
			break;
		target = data->target;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_parse_wps_data(data->rcvMsg, data->rcvMsgLen, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Message Type */
		if (wps_get_value(wps, WPS_TYPE_MSG_TYPE, &u8val, 0))
			break;
		if (msg_type != u8val)
			break;

		/* Enrollee Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_ENROLLEE_NONCE, nonce, &length))
			break;
		if (os_memcmp(e_nonce, nonce, length))
			break;

		/* Registrar Nonce */
		length = sizeof(nonce);
		if (wps_get_value(wps, WPS_TYPE_REGISTRAR_NONCE, nonce, &length))
			break;
		if (os_memcmp(r_nonce, nonce, length))
			break;

		if (msg_type == WPS_MSGTYPE_NACK) {
			/* Configuration Error */
			if (wps_get_value(wps, WPS_TYPE_CONFIG_ERROR, &target->config_error, 0))
				break;
		}

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);

	return ret;
}


static int eap_wps_process_message_special(struct eap_sm *sm,
										   struct eap_wps_data *data,
										   u8 msg_type,
										   u8 *e_nonce, u8 *r_nonce)
{
	int ret = -1;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
#ifdef WPS_OPT_UPNP
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);
#endif /* WPS_OPT_UPNP */

        wpa_printf(MSG_INFO, "Enter eap_wps_process_message_special state %d",
            data->state);
	do {
		if (!sm || !data || !e_nonce || !r_nonce || !conf)
			break;

#ifdef WPS_OPT_UPNP
		if (!hapd)
			break;

		if (msg_type == WPS_MSGTYPE_DONE) {
                        if (hapd->wps_opt_upnp)
			(void)wps_opt_upnp_send_wlan_eap_event(
                                hapd->wps_opt_upnp, sm, data);
		} else if (conf->upnp_enabled) {
			if (wps_opt_upnp_send_wlan_eap_event(
                                hapd->wps_opt_upnp, sm, data))
				break;
		} else
#endif /* WPS_OPT_UPNP */
		if (eap_wps_config_process_message_special(
                                hapd, conf, data, msg_type, e_nonce, r_nonce))
			break;

		ret = 0;
	} while (0);
        wpa_printf(MSG_INFO, "Exit eap_wps_process_message_special state %d ret %d",
            data->state, ret);

	return ret;
}


static u8 *eap_wps_build_packet(u8 code, u8 identifier, u8 op_code, u8 flags,
		u8 *msg, size_t msg_len, size_t *rsp_len)
{
	u8 *rsp = 0;
	struct eap_hdr *rsp_hdr;
	struct eap_format *rsp_fmt;
	u8 *tmp;
#ifdef WPS_OPT_UPNP
	u8 msg_type;
#endif /* WPS_OPT_UPNP */

	do {
		if ((!msg && msg_len) || !rsp_len)
			break;

		if (flags & EAP_FLAG_LF)
			*rsp_len = sizeof(*rsp_hdr) + sizeof(*rsp_fmt) + msg_len + 2;
		else
			*rsp_len = sizeof(*rsp_hdr) + sizeof(*rsp_fmt) + msg_len;
		rsp = wpa_zalloc(*rsp_len);
		
		if (rsp) {
			rsp_hdr = (struct eap_hdr *)rsp;
			rsp_hdr->code = code;
			rsp_hdr->identifier = identifier;
			rsp_hdr->length = host_to_be16(*rsp_len);

			rsp_fmt = (struct eap_format *)(rsp_hdr + 1);
			rsp_fmt->type = EAP_TYPE_EXPANDED;
			os_memcpy(rsp_fmt->vendor_id, EAP_VENDOR_ID_WPS, sizeof(rsp_fmt->vendor_id));
			os_memcpy(rsp_fmt->vendor_type, EAP_VENDOR_TYPE_WPS, sizeof(rsp_fmt->vendor_type));
#ifdef WPS_OPT_UPNP
			msg_type = wps_get_message_type(msg, msg_len);
			switch (msg_type) {
			case WPS_MSGTYPE_ACK:
				if (op_code != EAP_OPCODE_WPS_ACK)
					op_code = EAP_OPCODE_WPS_ACK;
				break;
			case WPS_MSGTYPE_DONE:
				if (op_code != EAP_OPCODE_WPS_DONE)
					op_code = EAP_OPCODE_WPS_DONE;
				break;
			case WPS_MSGTYPE_NACK:
				if (op_code != EAP_OPCODE_WPS_NACK)
					op_code = EAP_OPCODE_WPS_NACK;
				break;
			default:
				break;
			}
#endif /* WPS_OPT_UPNP */
			rsp_fmt->op_code = op_code;
			rsp_fmt->flags = flags;

			tmp = (u8 *)(rsp_fmt + 1);
			if (flags & EAP_FLAG_LF) {
				WPA_PUT_BE16(tmp, msg_len);
				tmp += 2;
			}

			if (msg_len)
				os_memcpy(tmp, msg, msg_len);
		}
	} while (0);

	if (!rsp && rsp_len)
		*rsp_len = 0;

	return rsp;
}


static u8 *eap_wps_build_req_registrar(struct eap_sm *sm,
									   struct eap_wps_data *data,
									   u8 req_identifier,
									   size_t *req_len)
{
	u8 *req = 0;
	u8 *wps_msg = 0;
	size_t wps_msg_len;
	struct eap_wps_target_info *target = data->target;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);

        wpa_printf(MSG_DEBUG, "eap_wps_build_req_registrar state %d",
                data->state);

	do {
		switch (data->state) {
		case START:
		{
                        wpa_printf(MSG_INFO, "WPS Build Registar START");
			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
									   EAP_OPCODE_WPS_START, 0, NULL, 0,
									   req_len);
			if(!req)
				break;
			data->state = M1;
			break;
		}
		case M2:
		{
			/* Build M2 message */
                        wpa_printf(MSG_INFO, "WPS Build Registar M2");
			if (!(wps_msg = eap_wps_build_message_M2(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M3;
			break;
		}
		case M2D1:
		{
#ifdef WPS_OPT_UPNP
			int prev_state = data->state;
#endif /* WPS_OPT_UPNP */

			/* Build M2D message */
                        wpa_printf(MSG_INFO, "WPS Build Registar M2D-1");
			if (!(wps_msg = eap_wps_build_message_M2D(sm, data, &wps_msg_len))) {
				if (M2D2 == data->state)
					data->state = FAILURE;
				break;
			}

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
#ifdef WPS_OPT_UPNP
			/* data->state may change in eap_wps_build_message_M2D */
			/* when UPnP is enabled */
			if ((M2D1 == prev_state) && ((NACK != data->state) && (M3 != data->state))) {
#endif /* WPS_OPT_UPNP */
			eap_wps_request(hapd, CTRL_REQ_TYPE_PASSWORD, "REGISTRAR");
			data->state = ACK;
#ifdef WPS_OPT_UPNP
			}
#endif /* WPS_OPT_UPNP */
			break;
		}
#ifdef WPS_OPT_UPNP
		case M2D2:
		{
			if (!data->sndMsg || !data->sndMsgLen)
				data->state = FAILURE;
			else {
				/* Build M2D message */
                                wpa_printf(MSG_INFO, "WPS Build Registar M2D-2");
				req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
											EAP_OPCODE_WPS_MSG, 0,
											data->sndMsg, data->sndMsgLen,
											req_len);
				if(!req)
					break;
				data->state = ACK;
			}
			break;
		}
#endif /* WPS_OPT_UPNP */
		case M4:
		{
			/* Build M4 message */
                        wpa_printf(MSG_INFO, "WPS Build Registar M4");
			if (!(wps_msg = eap_wps_build_message_M4(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M5;
			break;
		}
		case M6:
		{
			/* Build M6 message */
                        wpa_printf(MSG_INFO, "WPS Build Registar M6");
			if (!(wps_msg = eap_wps_build_message_M6(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M7;
			break;
		}
		case M8:
		{
			/* Build M8 message */
                        wpa_printf(MSG_INFO, "WPS Build Registar M8");
			if (!(wps_msg = eap_wps_build_message_M8(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = DONE;
                        eap_wps_failure_clear(hapd, sm->addr);
			break;
		}
		case NACK:
		{
			/* Build NACK */
                        wpa_printf(MSG_INFO, "WPS Build Registar NACK");
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_NACK, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = NACK;
                        /* Fall through! */
		}
		default:
		{
			/* Build NACK */
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_NACK, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = NACK;
                        conf->nfailure++;
			break;
		}
		}
	} while (0);

	if (wps_msg)
		os_free(wps_msg);

        if (req == NULL) {
                wpa_printf(MSG_ERROR, "WPS Build Message FAILED");
        }
	return req;
}


/*
 * eap_wps_process_registrar is called when WE are the registrar.
 */
static int eap_wps_process_registrar(struct eap_sm *sm,
	struct eap_wps_data *data,
	u8 req_identifier,
	u8 req_op_code)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	int prev_state;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);

	do {
		if (!sm || !data || !data->target || !hapd)
			break;
		target = data->target;

		if (data->interface != REGISTRAR) {
                        wpa_printf(MSG_ERROR, "WPS Process Registrar: interface=%d",
                                data->interface);
			break;
                }

		switch (req_op_code) {
		case EAP_OPCODE_WPS_NACK:
                        /* Note that although NACK can indicate a problem
                         * with PIN mismatch (and therefore possible
                         * intrusion? although a smart intruder would
                         * not senda NAK?), 
                         * when we get as far as M7 the PIN
                         * has been verified and so it is not an intrusion.
                         */
                        if (data->state < M7) conf->nfailure++;
			data->state = NACK;
			break;
		default:
			break;
		}

		prev_state = data->state;
                wpa_printf(MSG_INFO, 
                        "WPS Process Registrar state=%d upnp_enabled=%d", 
                        prev_state, conf->upnp_enabled);
		switch (data->state) {
		case M1:
		{
			/* Should be received M1 message */
                        wpa_printf(MSG_INFO, "WPS Process Registrar M1?");
			if (!eap_wps_process_message_M1(sm, data)) {
				if (data->dev_pwd_len && !conf->upnp_enabled) {
                                        /* We handle session directly */
					data->state = M2;
				} else {
                                        /* We do proxying for this session */
					data->state = M2D1;
				}
			} else {
				data->state = NACK;
                                conf->nfailure++;
                        }
			break;
		}
		case M3:
		{
			/* Should be received M3 message */
                        wpa_printf(MSG_INFO, "WPS Process Registrar M3?");
			if (!eap_wps_process_message_M3(sm, data)) {
				data->state = M4;
			} else {
				data->state = NACK;
                                conf->nfailure++;
                        }
			break;
		}
		case M5:
		{
			/* Should be received M5 message */
                        wpa_printf(MSG_INFO, "WPS Process Registrar M5?");
			if (!eap_wps_process_message_M5(sm, data)) {
				data->state = M6;
			} else {
				data->state = NACK;
                                conf->nfailure++;
                        }
			break;
		}
		case M7:
		{
			/* Should be received M7 message */
                        wpa_printf(MSG_INFO, "WPS Process Registrar M7?");
			if (!eap_wps_process_message_M7(sm, data)) {
				data->state = M8;
			} else {
				data->state = NACK;
                                conf->nfailure++;
                        }
			break;
		}
		case DONE:
		{
			/* Should be received Done */
                        wpa_printf(MSG_INFO, "WPS Process Registrar DONE?");
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_DONE, target->nonce, data->nonce)) {
                                eap_wps_failure_clear(hapd, sm->addr);
				/* Send EAP-WPS complete message */
		                if (conf->wps_job_busy) {
                                        /* Disable now that we are done */
					(void)eap_wps_disable(hapd, conf,
                                                eap_wps_disable_reason_success);
				}

                                /* WPS always ends in EAP "failure"
                                 * per how it is specified... because
                                 * WPS never actually authenticates anyone.
                                 */
				data->state = FAILURE;
                                /* Support for bizarre and poorly documented
                                 * WPS feature whereby we configure ourselves
                                 * with random ssid/psk if we were marked
                                 * "unconfigured" when someone asked us
                                 * to configure >them<.
                                 * We put this random data into target->config
                                 * earlier, but only after serving it 
                                 * successfully are we allowed (and required)
                                 * to make it our own data...
                                 *
                                 * Note: currently, this results in
                                 * restarting of hostapd.
                                 */
                                if (data->autoconfig) {
                                    eap_wps_set_ssid_configuration(
                                                sm, data->config, 
                                                data->config_len);
                                }
			} else {
				data->state = NACK;
                                conf->nfailure++;
                        }
			break;
		}
		case ACK:
		{
			/* Should be received ACK */
                        wpa_printf(MSG_INFO, "WPS Process Registrar ACK?");
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_ACK, target->nonce, data->nonce)) {
#ifdef WPS_OPT_UPNP
                                if (! conf->upnp_enabled)
#endif
#ifndef WPS_OPT_UPNP
                                /* if we are registrar, if we send an M2D
                                 * message, then we are done.
                                 * Unless we are doing UPnP of course,
                                 * in which case there can be multiple
                                 * registrars sending all sorts of stuff.
                                 */
				data->state = FAILURE;
#else /* WPS_OPT_UPNP */
                                #if 0   /* original xxxxxxxxx */
				u8 *wps_msg = 0;
				size_t wps_msg_len;

				/* Check if other M2 or M2D message is available */
				data->state = M2D2;
				if (!(wps_msg = eap_wps_build_message_M2D(sm, data, &wps_msg_len))) {
                                    wpa_printf(MSG_INFO, 
                                        "eap_wps_process_registrar state ACK fail at %d", __LINE__);
					data->state = FAILURE;
					break;
				}
				os_free(wps_msg);
                                #else
                                /* This code is entered as a result of ACK
                                 * to an M2D that we pass on from the registrar.
                                 * The old code resulted in loss of data 
                                 * and failure.
                                 * It makes more sense to me at least to
                                 * just return to the M2D1 state that we
                                 * started with: which is appropriate if
                                 * we don't know if we'll get an M2 or M2D
                                 * next, which is certainly the case here.
                                 *
                                 * A related problem is that AFTER we 
                                 * finally get the M2, we could conceivably
                                 * get another M2D from the registrar...
                                 * which we'll have to ignore.
                                 */ 
                                wpa_printf(MSG_INFO,
                                    "Processed ACK, goto M2D1");
                                data->state = M2D1; 
                                #endif
#endif /* WPS_OPT_UPNP */
			} else {
                                wpa_printf(MSG_INFO, 
                                    "eap_wps_process_registrar state ACK fail at %d", __LINE__);
			        data->state = FAILURE;
                        }
			break;
		}
		case NACK:
		{
			struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

			/* Should be received NACK */
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_NACK, target->nonce, data->nonce)) {
                                #if 0 /* Was from Sony... */
                                /* ... don't auto disable, allow retries to work */
				if (conf->wps_job_busy) {
					(void)eap_wps_disable(hapd, conf, xx);
				}
                                #endif  /* Was */

#ifdef WPS_OPT_UPNP
				if (!conf->upnp_enabled)
#endif /* WPS_OPT_UPNP */
				/* Send EAP-WPS fail message */
				eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "NACK from station");
			}

			data->state = FAILURE;
			break;
		}
		default:
		{
			break;
		}
		}

		if (prev_state != data->state) {
			ret = 0;
                        wpa_printf(MSG_INFO, "WPS Process Registrar old=%d new=%d", 
                                prev_state, data->state);
                } else {
                        /* This happens only for UPnP external registrar,
                         * where we are waiting for the external registrar
                         * to give us e.g. an M2 message... meanwhile the
                         * enrollee gets impatient and resends us e.g. an M1
                         * which leads to this.
                         * There should be no harm in ignoring the 
                         * received message.
                         * TODO: there should be a better scheme for 
                         * ignoring resent messages, e.g. compare received
                         * message with last one received before we get
                         * anywhere near here.
                         */
                        ret = 0;        /* ignore */
                        wpa_printf(MSG_ERROR, "WPS Process Registrar No state change");
                }
	} while (0);

        if (ret) {
                wpa_printf(MSG_ERROR, "WPS Process Registrar FAILED");
        }
	return ret;
}


static u8 *eap_wps_build_req_enrollee(struct eap_sm *sm,
									  struct eap_wps_data *data,
									  u8 req_identifier,
									  size_t *req_len)
{
	u8 *req = 0;
	u8 *wps_msg = 0;
	size_t wps_msg_len;
	struct eap_wps_target_info *target;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);

	do {
		if (!sm || !data || !data->target || !req_len || !hapd)
			break;
		target = data->target;

		switch (data->state) {
		case START:
		{
			/* Should be received Start Message */
			/* Build M1 message */
                        wpa_printf(MSG_INFO, "WPS Build Enrollee M1");
			if (!(wps_msg = eap_wps_build_message_M1(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M2;
			break;
		}
		case M3:
		{
			/* Build M3 message */
                        wpa_printf(MSG_INFO, "WPS Build Enrollee M3");
			if (!(wps_msg = eap_wps_build_message_M3(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M4;
			break;
		}
		case M5:
		{
			/* Build M5 message */
                        wpa_printf(MSG_INFO, "WPS Build Enrollee M5");
			if (!(wps_msg = eap_wps_build_message_M5(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M6;
			break;
		}
		case M7:
		{
			/* Build M7 message */
                        wpa_printf(MSG_INFO, "WPS Build Enrollee M7");
			if (!(wps_msg = eap_wps_build_message_M7(sm, data, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_MSG, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = M8;
			break;
		}
		case NACK:
		{
                        wpa_printf(MSG_INFO, "WPS Build Enrollee NACK");
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, data->nonce, target->nonce, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_NACK, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
                        /* Note that although NACK can indicate a problem
                         * with PIN mismatch (and therefore possible
                         * intrusion), when we get as far as M7 the PIN
                         * has been verified.
                         */
                        if (data->state < M7) conf->nfailure++;
			data->state = NACK;
			break;
		}
		case DONE:
		{
			/* Build Done */
                        eap_wps_failure_clear(hapd, sm->addr);
                        wpa_printf(MSG_INFO, "WPS Build Enrollee DONE");
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_DONE, data->nonce, target->nonce, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_DONE, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = ACK;
			break;
		}
		default:
		{
			/* Build NACK */
                        wpa_printf(MSG_INFO, "WPS Build Enrollee NACK (Default)");
			if (!(wps_msg = eap_wps_build_message_special(sm, data, WPS_MSGTYPE_NACK, data->nonce, target->nonce, &wps_msg_len)))
				break;

			req = eap_wps_build_packet(EAP_CODE_REQUEST, req_identifier,
										EAP_OPCODE_WPS_NACK, 0,
										wps_msg, wps_msg_len,
										req_len);
			if(!req)
				break;
			data->state = NACK;
                        conf->nfailure++;
			break;
		}
		}
	} while (0);

        if (req == NULL) {
                wpa_printf(MSG_ERROR, "WPS Build Message FAILED");
        }
	return req;
}


/*
 * eap_wps_process_enrollee is called when WE are the enrollee.
 * or when we proxy for the enrollee... 
 */
static int eap_wps_process_enrollee(struct eap_sm *sm,
									struct eap_wps_data *data,
									u8 rsp_identifier,
									u8 rsp_op_code)
{
	int ret = -1;
	struct eap_wps_target_info *target;
	int prev_state;
	struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);

	do {
		if (!sm || !data || !data->target || !hapd)
			break;
		target = data->target;

		if (data->interface != ENROLLEE) {
                        wpa_printf(MSG_ERROR, "WPS Process Enrollee: interface=%d",
                                data->interface);
			break;
                }

		switch (rsp_op_code) {
		case EAP_OPCODE_WPS_MSG:
			break;
		case EAP_OPCODE_WPS_NACK:
			data->state = NACK;
                        conf->nfailure++;
			break;
		default:
			break;
		}

		prev_state = data->state;
		switch (data->state) {
		case M2:
		{
			Boolean with_config;
			/* Should be received M2/M2D message */
                        wpa_printf(MSG_INFO, "WPS Process Enrollee M2?");
			if (!eap_wps_process_message_M2(sm, data, &with_config)) {
				/* Received M2 */
				if (with_config) {
					/* Build Done message */
					data->state = DONE;
                                        eap_wps_failure_clear(hapd, sm->addr);
				} else {
					/* Build M3 message */
					data->state = M3;
				}
			} else if (!eap_wps_process_message_M2D(sm, data)) {
				/* Received M2D */
				/* Build NACK message */
				data->state = NACK;
                                /* not a security issue!: conf->nfailure++; */
			}
			break;
		}
		case M4:
		{
			/* Should be received M4 message */
                        wpa_printf(MSG_INFO, "WPS Process Enrollee M4?");
			if (!eap_wps_process_message_M4(sm, data)) {
				/* Build M5 message */
				data->state = M5;
			}
			break;
		}
		case M6:
		{
			/* Should be received M6 message */
                        wpa_printf(MSG_INFO, "WPS Process Enrollee M6?");
			if (!eap_wps_process_message_M6(sm, data)) {
				/* Build M7 message */
				data->state = M7;
			}
			break;
		}
		case M8:
		{
			/* Should be received M8 message */
                        /* At this point, we have sent our credentials
                         * in an M7 message but the other side decided
                         * it wanted to give us new ones... which we may
                         * not allow... of course, it might give us the
                         * ones we already have, sigh.
                         * We couldn't say anything earlier because the
                         * point of the exchange might have been the M7
                         * message.
                         */
                        wpa_printf(MSG_INFO, "WPS Process Enrollee M8?");
			if (!eap_wps_process_message_M8(sm, data)) {
                                if ((conf->wps_job_busy && conf->do_save) ||
                                    (!conf->wps_job_busy && 
                                            conf->wps_state == 
                                                WPS_WPSSTATE_UNCONFIGURED)) {
                                        wpa_printf(MSG_INFO,
                                                "WPS M8 Msg rcvd OK");
				        data->state = DONE;
                                        eap_wps_failure_clear(hapd, sm->addr);
                                } else {
                                        wpa_printf(MSG_INFO,
                                                "WPS M8 Msg rcvd but config mod disallowed!");
                                        data->state = NACK;
                                }
			}
			break;
		}
		case ACK:
		{
			struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);

			/* Should be received ACK message */
                        wpa_printf(MSG_INFO, "WPS Process Enrollee ACK?");
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_ACK, data->nonce, target->nonce)) {
                                if (target->config == NULL) {
                                        wpa_printf(MSG_INFO, 
"WPS Process Enrollee Ack not configuring due to no config!");
					eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "No config!");
                                } else
                                if ((conf->wps_job_busy && conf->do_save) ||
                                    (!conf->wps_job_busy && 
                                            conf->wps_state == 
                                                WPS_WPSSTATE_UNCONFIGURED)) {
					/* Set Network Configuration */
                                        wpa_printf(MSG_INFO, 
"WPS Process Enrollee Ack configuring ourselves!");
					eap_wps_set_ssid_configuration(
                                                sm, target->config, 
                                                target->config_len);
                                } else {
                                        wpa_printf(MSG_INFO, 
                                            "WPS Process Enrollee Ack self-configuring disallowed!");
					eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Self-configuring disallowed!");
                                }

				if (conf->wps_job_busy) {
                                        /* Disable now that we are done */
                                        wpa_printf(MSG_INFO, 
                                            "WPS Process Enrollee Ack"
                                            " disable job now we're done");
					/* Send EAP-WPS complete message */
					(void)eap_wps_disable(hapd, conf,
                                                eap_wps_disable_reason_success);
                                        eap_wps_failure_clear(hapd, sm->addr);
				}

                                /* Yes, this is how WPS is supposed to work.
                                 * After it succeeds (or not), the EAP
                                 * state machine is supposed to end with
                                 * "failure"... since no authentication
                                 * actually happened, we just used a side-effect
                                 */
				data->state = FAILURE;
			} else {
                                wpa_printf(MSG_ERROR, "WPS Process Enrollee ACK failed");
                        }
			break;
		}
		case NACK:
		{
			/* Should be received NACK message */
                        wpa_printf(MSG_INFO, "WPS Process Enrollee NACK?");
			if (!eap_wps_process_message_special(sm, data, WPS_MSGTYPE_NACK, data->nonce, target->nonce)) {
                                #if 0 /* Was from Sony; but don't auto disable */
				if (conf->wps_job_busy) {
					(void)eap_wps_disable(hapd, conf, xx);
				}
                                #endif  /* Was */

				/* Send EAP-WPS fail message */
				eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "NACK from station");
			}

			data->state = FAILURE;
			break;
		}
		default:
		{
			break;
		}
		}

		if (prev_state != data->state) {
			ret = 0;
                        wpa_printf(MSG_INFO, "WPS Process Enrollee old=%d new=%d",
                                prev_state, data->state);
                } else {
                        /* Since this might just have been a resend of
                         * a previously sent message we haven't replied to
                         * yet, let's ignore it... should hurt.
                         * TODO: have a better scheme of identifying
                         * before we ever get here if rcvd msg is same
                         * as previously recvd msg (and ignoring it).
                         */
                        ret = 0;        /* ignore */
                        wpa_printf(MSG_ERROR, "WPS Process Enrollee: no state change");
                }
	} while (0);

        if (ret) wpa_printf(MSG_ERROR, "WPS Process Enrollee FAILED");
	return ret;
}


static u8 *eap_wps_build_req(struct eap_sm *sm, void *priv, int id,
							 size_t *req_len)
{
	u8 *req = 0;
	struct eap_wps_data *data = (struct eap_wps_data *)priv;
	const u8 *identity;
        struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);

        wpa_printf(MSG_DEBUG, "eap_wps_build_req ENTER identity=%.*s conf: config_who=%d",
                (int)sm->identity_len, sm->identity, conf->config_who);
	do {
		if (data->fragment) {
			req = eap_wps_build_packet(EAP_CODE_REQUEST, id,
									   EAP_OPCODE_WPS_FLAG_ACK, 0, NULL, 0,
									   req_len);
			break;
		}

		identity = sm->identity;
		if (0 == os_strncmp((char *)identity, WPS_IDENTITY_REGISTRAR, os_strlen(WPS_IDENTITY_REGISTRAR)))
			data->interface = ENROLLEE;
		else if (0 == os_strncmp((char *)identity, WPS_IDENTITY_ENROLLEE, os_strlen(WPS_IDENTITY_ENROLLEE)))
			data->interface = REGISTRAR;
		else {
			/* Error */
                        wpa_printf(MSG_ERROR, "eap_wps_build_req: Got unknown identity %.*s",
                                (int)sm->identity_len, identity);
			return 0;
		}

                /* Now that we know what we're doing, we can initialize
                * the "target" data.
                */
                if (data->target == NULL) {
                        if (eap_wps_config_init_data(hapd, conf, data, sm->addr)) {
                                break;
                        }
                }

                wpa_printf(MSG_DEBUG, "eap_wps_build_req data: config_who=%d interface=%d",
                        data->config_who, data->interface);

		switch (data->interface) {
		case REGISTRAR:
			req = eap_wps_build_req_registrar(sm, data, id, req_len);
			break;
		case ENROLLEE:
			req = eap_wps_build_req_enrollee(sm, data, id, req_len);
			break;
		default:
			break;
		}
	} while (0);

	return req;
}


static Boolean eap_wps_check(struct eap_sm *sm, void *priv,
							 u8 *resp, size_t resp_len)
{
	Boolean ret = TRUE;
	struct eap_hdr *req_hdr = (struct eap_hdr *)resp;
	struct eap_format *req_fmt;
	u16 msg_len;

	do {
		req_fmt = (struct eap_format *)(req_hdr + 1);
		if (be_to_host16(req_hdr->length) != resp_len) {
			break;
		} else if ((EAP_TYPE_EXPANDED != req_fmt->type) ||
				   (0 != os_memcmp(req_fmt->vendor_id, EAP_VENDOR_ID_WPS,
				                sizeof(req_fmt->vendor_id))) ||
				   (0 != os_memcmp(req_fmt->vendor_type, EAP_VENDOR_TYPE_WPS,
				                sizeof(req_fmt->vendor_type)))) {
			break;
		}

		if (req_fmt->flags & EAP_FLAG_LF) {
			msg_len = req_hdr->length - (sizeof(*req_hdr) + sizeof(*req_fmt));
			if (msg_len != WPA_GET_BE16((u8 *)req_fmt + 1)) {
				break;
			}
		}

		ret = FALSE;
	} while (0);

	return ret;
}


static void eap_wps_process(struct eap_sm *sm, void *priv,
		u8 *resp, size_t resp_len)
{
	int ret = -1;
	struct eap_wps_data *data = (struct eap_wps_data *)priv;
	struct eap_hdr *req_hdr = (struct eap_hdr *)resp;
	struct eap_format *req_fmt;
	const u8 *identity;
	u8 *raw;
	u16 msg_len;
        struct wps_config *conf = (struct wps_config *)eap_get_wps_config(sm);
	struct hostapd_data *hapd = (struct hostapd_data *)eap_get_hostapd_data(sm);

        wpa_printf(MSG_DEBUG, "eap_wps_process ENTER identity=%.*s", 
                (int)sm->identity_len, sm->identity);

	do {
		req_fmt = (struct eap_format *)(req_hdr + 1);

		if (req_fmt->flags & EAP_FLAG_LF) {
			raw = (u8 *)(req_fmt + 1);
			msg_len = req_hdr->length - (sizeof(*req_hdr) + sizeof(*req_fmt));
			if (msg_len != WPA_GET_BE16((u8 *)req_fmt + 1)) {
				break;
			}
		} else {
			raw = (u8 *)(req_fmt + 1);
			msg_len = resp_len - (sizeof(*req_hdr) + sizeof(*req_fmt));
		}

		if (data->fragment) {
			data->fragment = 0;
			data->rcvMsg = (u8 *)os_realloc(data->rcvMsg, data->rcvMsgLen + msg_len);
			if (data->rcvMsg) {
				os_memcpy(data->rcvMsg + data->rcvMsgLen, raw, msg_len);
				data->rcvMsgLen += msg_len;
			}
		} else {
			if (data->rcvMsg)
				os_free(data->rcvMsg);
			data->rcvMsg = os_malloc(msg_len);
			if (data->rcvMsg) {
				os_memcpy(data->rcvMsg, raw, msg_len);
				data->rcvMsgLen = msg_len;
			}
		}

		if (!data->rcvMsg) {
			/* Memory allocation Error */
			data->rcvMsgLen = 0;
			break;
		}

		if (req_fmt->flags & EAP_FLAG_MF) {
			data->fragment = 1;
			ret = 0;
			break;
		}

		identity = sm->identity;
		if (0 == os_strncmp((char *)identity, WPS_IDENTITY_REGISTRAR, os_strlen(WPS_IDENTITY_REGISTRAR)))
			data->interface = ENROLLEE;
		else if (0 == os_strncmp((char *)identity, WPS_IDENTITY_ENROLLEE, os_strlen(WPS_IDENTITY_ENROLLEE)))
			data->interface = REGISTRAR;
		else {
			/* Error */
                        wpa_printf(MSG_ERROR, "eap_wps_process: Got unknown identity %.*s",
                                (int)sm->identity_len, identity);
			break;
		}

                if (data->target == NULL) {
                        /* Now that we know what we're doing, we can initialize
                        * the "target" data.
                        */
                        wpa_printf(MSG_DEBUG, "eap_wps_process: init_data");
		        if (eap_wps_config_init_data(hapd, conf, data, sm->addr)) {
			        break;
                        }
                }

		switch (data->interface) {
		case REGISTRAR:
                        /* We are acting as registrar... */
			ret = eap_wps_process_registrar(sm, data,
				req_hdr->identifier,
				req_fmt->op_code);
			break;
		case ENROLLEE:
                        /* We are acting as enrollee... */
			ret = eap_wps_process_enrollee(sm, data,
				req_hdr->identifier,
				req_fmt->op_code);
			break;
		default:
			break;
		}
	} while (0);

	if (ret) {
		data->state = NACK;
                conf->nfailure++;
        } else {
                #ifdef WPS_OPT_TINYUPNP
                if (conf->wps_upnp_disable) {
                        /* no upnp */
                } else
                if (data->dev_pwd_len && !conf->upnp_enabled) {
                        /* handle locally in spite of upnp */
                } else { 
                        /* Prevent eap state machine from wanting us
                         * to build the response... we must wait until
                         * the registrar provides it.
                         */
                        wpa_printf(MSG_INFO, "eap_wps_process state %d waiting for registar...", data->state);
                        wps_opt_upnp_wait_eap_sm(hapd->wps_opt_upnp, sm, data->state);
                }
                #endif  /* WPS_OPT_TINYUPNP */
        }

	return;
}


static Boolean eap_wps_is_done(struct eap_sm *sm, void *priv)
{
	struct eap_wps_data *data = priv;
	return data->state == FAILURE;
}


static Boolean eap_wps_is_success(struct eap_sm *sm, void *priv)
{
	return FALSE;
}



/* Called from eap to determine the retransmit time to use (in seconds).
 * A zero value suppresses retransmits.
 */
static int eap_wps_get_timeout(struct eap_sm *sm, void *priv)
{
	/* struct eap_wps_data *data = priv; */
        /* While we are at it, cap the maximum no. of retransmits
         * to appropriate value for WPS.
         */
        sm->MaxRetrans = EAP_WPS_MAX_RETRANS;
        return EAP_WPS_RETRANS_SECONDS;
}


int eap_server_wps_register(void)
{
	struct eap_method *eap;
	int ret;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				    WPA_GET_BE24(EAP_VENDOR_ID_WPS), WPA_GET_BE32(EAP_VENDOR_TYPE_WPS), "WPS");
	if (eap == NULL)
		return -1;

	eap->init = eap_wps_init;
	eap->reset = eap_wps_reset;
	eap->buildReq = eap_wps_build_req;
	eap->check = eap_wps_check;
	eap->process = eap_wps_process;
	eap->isDone = eap_wps_is_done;
	eap->isSuccess = eap_wps_is_success;
        eap->getTimeout = eap_wps_get_timeout;

	ret = eap_server_method_register(eap);
	if (ret)
		eap_server_method_free(eap);
	return ret;
}


/* Caution: this depends on wps->wps_job_busy being correct, 
 * and on correct value for selreg,
 * as well as other parameters.
 */
int eap_wps_set_ie(struct hostapd_data *hapd, struct wps_config *wps)
{
	int ret = -1;
	u8 *iebuf = 0;
	size_t iebuflen;

	do {
		if (!hapd)
			break;
		if (!wps)
			break;
                if (wps->wps_disable)
                        break;

		/* Create WPS Beacon IE */
		if (wps_config_create_beacon_ie(hapd, &iebuf, &iebuflen)) {
			break;
		}
		/* Set WPS Beacon IE */
		if (hostapd_set_wps_beacon_ie(hapd, iebuf, iebuflen)) {
			break;
		}
		os_free(iebuf);
		iebuf = 0;
		/* Create WPS ProbeResp IE */
		if (wps_config_create_probe_resp_ie(hapd, &iebuf, &iebuflen)) {
			break;
		}
		/* Set WPS ProbeResp IE */
		if (hostapd_set_wps_probe_resp_ie(hapd, iebuf, iebuflen)) {
			break;
		}
		os_free(iebuf);
		iebuf = 0;
                #if WPS_DO_ASSOC_RESP_IE
                /* This is reported to break some clients; is required?
                 * only if client uses WPS association request (uncommon?).
                 */
		/* Create WPS AssocResp IE */
		if (wps_config_create_assoc_resp_ie(hapd, &iebuf, &iebuflen)) {
			break;
		}
		/* Set WPS AssocResp IE */
		if (hostapd_set_wps_assoc_resp_ie(hapd, iebuf, iebuflen)) {
			break;
		}
                #endif  /* WPS_DO_ASSOC_RESP_IE */

		ret = 0;
	} while (0);

	if (iebuf)
		os_free(iebuf);

	return ret;
}


static void eap_wps_timer_tick(void *ctx, void *conf)
{
        struct hostapd_data *hapd = ctx;
	struct wps_config *wps = conf;
	struct os_time now;
	int timeout = 0;

	if(!wps->wps_job_busy) {
                wpa_printf(MSG_ERROR, "eap_wps_timer_tick called unexpectedly");
		return;
	}

        /* note, seconds_timeout is -1 to disable timing out,
         * and should never be zero.
         */
        if (wps->seconds_timeout > 0) {
	        os_get_time(&now);
	        if (now.sec > wps->end_time.sec)
		        timeout = 1;
	        else if ((now.sec == wps->end_time.sec) &&
			        (now.usec >= wps->end_time.usec))
		        timeout = 1;
        }

	if (timeout) {
                if (wps->wps_done) {
                        /* If we are done but the other side has never
                         * sent the final message, just go ahead and
                         * decide we are done anyway.
                         */
                        wpa_printf(MSG_ERROR, "WPS DONE, ignoring missing final message");
		        wpa_msg(hapd, MSG_INFO, "WPS DONE");
                        (void) eap_wps_disable(hapd, wps,
                                eap_wps_disable_reason_success);
                } else {
                        wpa_printf(MSG_ERROR, "WPS timeout");
		        wpa_msg(hapd, MSG_INFO, "WPS timeout");
                        (void) eap_wps_disable(hapd, wps,
                                eap_wps_disable_reason_timeout);
                }
	} else {
		eloop_register_timeout(EAP_WPS_PERIOD_SEC, EAP_WPS_PERIOD_USEC,
                        eap_wps_timer_tick, hapd, conf);
        }
}

/* Cancel WPS job (due to session completion, failure, timeout etc.).
 */
int eap_wps_disable(struct hostapd_data *hapd, struct wps_config *conf,
        enum eap_wps_disable_reason reason)
{
	int ret = -1;
        int was_enabled = conf->wps_job_busy;

        wpa_printf(MSG_DEBUG, "ENTER eap_wps_disable, reason=%d", reason);

        if (was_enabled) switch(reason) {
                case eap_wps_disable_reason_success :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_SUCCESS, "Success");
                break;
                case eap_wps_disable_reason_misc_failure :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Misc failure");
                break;
                case eap_wps_disable_reason_bad_parameter :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Bad Parameter");
                break;
                case eap_wps_disable_reason_bad_pin :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Bad PIN");
                break;
                case eap_wps_disable_reason_timeout :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Timeout");
                break;
                case eap_wps_disable_reason_user_stop :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "User stop");
                break;
                case eap_wps_disable_reason_registrar_stop :
		        eap_wps_request(hapd, CTRL_REQ_TYPE_FAIL, "Registrar stop");
                break;
                case eap_wps_disable_reason_initialization :
                break;
        }

	do {
		os_memset(&conf->end_time, 0, sizeof(conf->end_time));

		conf->wps_job_busy = 0;
		if (was_enabled) {
			eloop_cancel_timeout(eap_wps_timer_tick, hapd, conf);
		        conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
		        os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
		        conf->dev_pwd_len = 0;
		        conf->selreg_config_methods = 0;
                        if (conf->default_pin && conf->default_pin[0]) {
                                conf->selreg_config_methods = WPS_CONFMET_LABEL;
                        }
		        conf->selreg = 0;
                        conf->do_save = 0;
                        conf->config_who = WPS_CONFIG_WHO_UNKNOWN;
	                (void) (eap_wps_set_ie(hapd, conf));

			hostapd_msg(hapd, MSG_INFO, "WPS stop, reason %d",
                                reason);
		}
		if (conf->set_pub_key) {
			if (conf->dh_secret)
				eap_wps_free_dh(&conf->dh_secret);
			os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
			conf->set_pub_key = 0;
		}
		conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
		os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
		conf->dev_pwd_len = 0;

                #ifdef WPS_OPT_UPNP
                /* cancel any ongoing session w/ external registrar
                 */
                if (hapd->wps_opt_upnp) {
                        wps_opt_upnp_deinit_data(hapd->wps_opt_upnp);
                }
                #endif
                conf->upnp_enabled = 0; /* to be sure */

		ret = 0;
	} while (0);

        if (was_enabled)
		eap_wps_request(hapd, CTRL_REQ_TYPE_DONE, "Done");
	return ret;
}



/* 
 * Enable ourselves for an explicit WPS session, usually as a
 * "selected registrar".
 * This also specifies the allowed mode, timeout and other parameters.
 *
 * There can potentially be a separate WPS session ongoing for each
 * station managed by hostapd, but only one may involve a selected 
 * registrar.
 * The selected registrar might be an external registrar, via UPnP.
 *
 * When there is no selected registrar, sessions may still occur using
 * the default PIN if one is configured... any number of such sessions
 * can occur simultaneously, but if there are two many failures then
 * WPS is locked down (hostapd restart required to fix this).
 *
 * Another confusing issue is that we continue to support some 
 * wps related activities (M1,M2 messaging) even when WPS is not enabled,
 * in order to provide non-secret information to upnp.
 * Therefore we bail on doing WPS only at the point where secret
 * information gets involved (more specifically, at the point where we
 * need the PIN and don't have one).
 *
 * Returns 0 if successful, nonzero if error.
 * In case of error, wps is properly disabled.
 */
int eap_wps_enable(struct hostapd_data *hapd, struct wps_config *conf,
        struct eap_wps_enable_params *params)
{
        u8 *dev_pwd = params->dev_pwd;    /* 00000000 for push button method */
        int dev_pwd_len = params->dev_pwd_len;
        /* NOTE! filter_bssid and filter_ssid are NOT
         * yet implemented for hostapd ... and perhaps never
         * will be.
         * (The wpa_supplicant version of eap_wps_enable DOES
         * use them).
         */
        int filter_bssid_flag = params->filter_bssid_flag;   /* accept only given bssid? */
        u8  *filter_bssid = params->filter_bssid;     /* used if filter_bssid_flag */
        int filter_ssid_length = params->filter_ssid_length;  /* accept only given essid? */
        u8  *filter_ssid = params->filter_ssid;
        int seconds_timeout = params->seconds_timeout; /* 0 for default timeout, -1 for no timeout */
        enum wps_config_who config_who = params->config_who;
        int do_save = params->do_save;           /* for WPS_CONFIG_WHO_ME */
	enum eap_wps_disable_reason ret = 
            eap_wps_disable_reason_bad_parameter; /* default error code */
        int was_enabled = conf->wps_job_busy;
        int is_push_button = 0;

        if (was_enabled && config_who != conf->config_who) {
                /* different configuration target -- start all over again */
                wpa_printf(MSG_INFO, "eap_wps_enable: different config_who, disable first");
                eap_wps_disable(hapd, conf, eap_wps_disable_reason_user_stop);
                was_enabled = 0;
        }
        if (conf->wps_disable) {
                wpa_printf(MSG_DEBUG, "WPS is disabled for this BSS");
                return -1;
        }
        if (config_who == WPS_CONFIG_WHO_EXTERNAL_REGISTRAR) {
                /* external registrar taking over; we will be proxying */
                if (conf->wps_upnp_disable) {
                        wpa_printf(MSG_DEBUG, "WPS UPnP disabled for this BSS");
                        return -1;
                }
                /* Specify bogus PIN just so we don't crash later on.
                 * This will not actually be used.
                 */
                dev_pwd = (u8 *)"00000000";
                dev_pwd_len = 8;
        } else {
                /* command given to us (e.g. via web page or push button) */
                is_push_button = 
                        (dev_pwd_len == 8 && memcmp(dev_pwd, "00000000", 8) == 0);
                if (dev_pwd_len == 0) {
                        /* Default to push button method */
                        dev_pwd = (u8 *)"00000000";
                        dev_pwd_len = 8;
                        is_push_button = 1;
                }
                #if 1   /* this policy decision could be changed */
                if (is_push_button && config_who == WPS_CONFIG_WHO_ME) {
                        wpa_printf(MSG_ERROR,
                                "WPS pushbutton mode not allowed for `configme' "
                                "due to security reasons.");
                        return -1;
                }
                #endif
        }
        if (seconds_timeout == 0) seconds_timeout = conf->default_timeout;
        if (seconds_timeout == 0) seconds_timeout = EAP_WPS_TIMEOUT_SEC;

        if (was_enabled && dev_pwd_len == conf->dev_pwd_len && 
                memcmp(conf->dev_pwd, dev_pwd, conf->dev_pwd_len) == 0) {
                /* pin has not changed... 
                 * just keep the existing session going
                 * but with extended timeout.
                 */
	        eloop_cancel_timeout(eap_wps_timer_tick, hapd, conf);

                /* Why do we always free this.... ? (this is what Sony did) */
                wps_config_free_dh(&conf->dh_secret);
    	        os_memset(conf->pub_key, 0, sizeof(conf->pub_key));
    	        conf->set_pub_key = 0;
        } else if (was_enabled) {
                /* different pin -- start all over again */
                wpa_printf(MSG_INFO, "eap_wps_enable: different PIN, disable first");
                eap_wps_disable(hapd, conf, eap_wps_disable_reason_user_stop);
                was_enabled = 0;
        }

        wpa_printf(MSG_INFO, "eap_wps_enable pwd=%.*s to=%d sec ",
                dev_pwd_len, dev_pwd, seconds_timeout);

	do {
                if (dev_pwd_len == 0 || dev_pwd_len > sizeof(conf->dev_pwd))
                        break;          /* sanity check */

		conf->wps_job_busy = 1;   /* make sure we clean up after this */
                conf->is_push_button = is_push_button;
                conf->upnp_enabled = 
                        (config_who == WPS_CONFIG_WHO_EXTERNAL_REGISTRAR);
                conf->config_who = config_who;
                conf->seconds_timeout = seconds_timeout;
                /* Note: seconds_timeout == -1 to disable timeout */
                if (seconds_timeout > 0) {
		        (void)os_get_time(&conf->end_time);
		        conf->end_time.sec += seconds_timeout;
		        /* conf->end_time.usec += 0; */
                }
		os_memset(conf->dev_pwd, 0, sizeof(conf->dev_pwd));
		conf->dev_pwd_len = dev_pwd_len;
		os_memcpy(conf->dev_pwd, dev_pwd, conf->dev_pwd_len);

                if (config_who == WPS_CONFIG_WHO_EXTERNAL_REGISTRAR) {
                        conf->do_save = 0;  /* note wps_state overrides */
                        /* Do not reset failure count since the
                         * external registrar could be cracked.
                         */
                } else
                if (config_who == WPS_CONFIG_WHO_ME) {
                        /* user-input -- configure self */
                        conf->do_save = do_save;
                        /* Reset failure count on explicit command
                         * since this should be safe
                         */
                        eap_wps_failure_clear(hapd, NULL);
                } else
                if (config_who == WPS_CONFIG_WHO_THEM) {
                        /* user-input -- configure a station */
                        conf->do_save = 0;  /* note wps_state overrides */
                        /* Reset failure count on explicit command
                         * since this should be safe
                         */
                        eap_wps_failure_clear(hapd, NULL);
                } else {
                        wpa_printf(MSG_ERROR, "Invalid config_who %d",
                                config_who);
                        break;
                }

                /* Set values for beacon and probe response i.e.s: */
                if (config_who == WPS_CONFIG_WHO_EXTERNAL_REGISTRAR) {
			conf->dev_pwd_id = params->dev_pwd_id;
                        conf->selreg_config_methods = params->selreg_config_methods;
                } else if (conf->is_push_button) {
			conf->dev_pwd_id = WPS_DEVICEPWDID_PUSH_BTN;
                        conf->selreg_config_methods = WPS_CONFMET_PBC;
                } else if ((8 == dev_pwd_len)) {
                        /* WPS allows various lengths, but length 8
                        *  is special... must be numeric and last digit
                        *  must be checksum of first 7.
                        */
                        if (eap_wps_device_password_validation(
                                        (u8 *)dev_pwd, dev_pwd_len)) {
                                wpa_printf(MSG_ERROR,
                                    "wps_enable: Bad checksum on PIN");
                                ret = eap_wps_disable_reason_bad_pin;
                                break;
                        }
			conf->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT /*USE PIN*/;
                        conf->selreg_config_methods = WPS_CONFMET_KEYPAD;
		} else {
			conf->dev_pwd_id = WPS_DEVICEPWDID_USER_SPEC;
                        conf->selreg_config_methods = WPS_CONFMET_KEYPAD;
                }
	        conf->selreg = 1;
       	        if (eap_wps_set_ie(hapd, conf)) {
		        break;
                }


                /* NOTE! filter_bssid and filter_ssid are NOT
                 * yet implemented for hostapd ... and perhaps never
                 * will be.
                 * (The wpa_supplicant version of eap_wps_enable DOES
                 * use them).
                 */
                if (filter_bssid_flag) {
                        conf->filter_bssid_flag = 1;
                        memcpy(conf->filter_bssid, filter_bssid,
                                sizeof(conf->filter_bssid));
                } else conf->filter_bssid_flag = 0;
                if (filter_ssid_length > 0) {
                        if (filter_ssid_length > 32) {
                            break;      /* invalid */
                        }
                        conf->filter_ssid_length = filter_ssid_length;
                        memset(conf->filter_ssid, 0, sizeof(conf->filter_ssid));
                        memcpy(conf->filter_ssid, filter_ssid,
                                filter_ssid_length);
                } else conf->filter_ssid_length = 0;

                if (was_enabled) {
		        eap_wps_request(hapd, CTRL_REQ_TYPE_READY, "restart");
                } else {
		        eap_wps_request(hapd, CTRL_REQ_TYPE_READY, "new start");
                }
		eloop_register_timeout(
                        EAP_WPS_PERIOD_SEC, EAP_WPS_PERIOD_USEC, 
                        eap_wps_timer_tick, hapd, conf);

		ret = eap_wps_disable_reason_success;   /* == 0 */
	} while (0);

        if (ret) {
                wpa_printf(MSG_ERROR, "eap_wps_enable failed, disabling...");
                eap_wps_disable(hapd, conf, ret);
        }
	return ret;
}



