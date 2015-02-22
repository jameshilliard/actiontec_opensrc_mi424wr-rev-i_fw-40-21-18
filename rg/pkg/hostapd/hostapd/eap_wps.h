/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: eap_wps.h
//  Description: EAP-WPS main source header
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

#ifndef EAP_WPS_H
#define EAP_WPS_H

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */


#include "wps_config.h"

struct eap_wps_target_info {
	u8		version;
	u8		uuid[SIZE_UUID];
	int		uuid_set;

	u8		mac[SIZE_MAC_ADDR];
	int		mac_set;

	u16		auth_type_flags;
	u16		encr_type_flags;
	u8		conn_type_flags;
	u16		config_methods;
	u8		wps_state;
	char		*manufacturer;
	size_t	manufacturer_len;
	char		*model_name;
	size_t	model_name_len;
	char		*model_number;
	size_t	model_number_len;
	char		*serial_number;
	size_t	serial_number_len;
	u8		prim_dev_type[SIZE_8_BYTES];
	char		*dev_name;
	size_t	dev_name_len;
	u8		rf_bands;
	u16		assoc_state;
	u16		config_error;
	u32		os_version;

	u8		nonce[SIZE_NONCE];
	u8		pubKey[SIZE_PUB_KEY];
	int		pubKey_set;
	u16		dev_pwd_id;
	u8		hash1[SIZE_WPS_HASH];
	u8		hash2[SIZE_WPS_HASH];

	u8		*config;
	size_t	config_len;
};

/* eap_wps_data is data for one WPS session.
 */
struct eap_wps_data {
	enum {START, M1, M2, M2D1, M2D2, M3, M4, M5, M6, M7, M8, DONE, ACK, NACK, FAILURE} state;
        /* interface is what role WE are playing */
	enum {NONE, REGISTRAR, ENROLLEE} interface;

        /* rcvMsg is last message we receive from station via wifi */
	u8		*rcvMsg;
	u32		rcvMsgLen;
	Boolean	fragment;

        /* sndMsg is last message we built OR proxy from external registrar,
         * and send to station via wifi.
         */
	u8		*sndMsg;        
	u32		sndMsgLen;

	u16		dev_pwd_id;
	u8		dev_pwd[SIZE_64_BYTES];
	u16		dev_pwd_len;

	u16		assoc_state;
	u16		config_error;

	u8		nonce[SIZE_NONCE];
	u8		pubKey[SIZE_PUB_KEY];
	int		preset_pubKey;

	void	*dh_secret;

	u8		authKey[SIZE_AUTH_KEY];
	u8		keyWrapKey[SIZE_KEY_WRAP_KEY];
	u8		emsk[SIZE_EMSK];

	u8		snonce1[SIZE_NONCE];
	u8		snonce2[SIZE_NONCE];
	u8		psk1[SIZE_128_BITS];
	u8		psk2[SIZE_128_BITS];
	u8		hash1[SIZE_WPS_HASH];
	u8		hash2[SIZE_WPS_HASH];

        enum wps_config_who config_who;
	int             is_push_button;
        int             autoconfig;     /* nonzero if we invent configuration*/
	u8		*config;
	size_t	config_len;

	struct eap_wps_target_info *target;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

#define EAP_WPS_TIMEOUT_SEC		120
#define EAP_WPS_TIMEOUT_USEC	0

int eap_wps_config_init_data(struct hostapd_data *hapd,
	struct wps_config *conf,
	struct eap_wps_data *data,
        const u8 *supplicant_addr
        );
void eap_wps_config_deinit_data(struct eap_wps_data *data);

int eap_wps_free_dh(void **dh);
int eap_wps_generate_sha256hash(u8 *inbuf, int inbuf_len, u8 *outbuf);
int eap_wps_generate_public_key(void **dh_secret, u8 *public_key);
int eap_wps_generate_device_password_id(u16 *dev_pwd_id);
int eap_wps_generate_device_password(u8 *dev_pwd, int dev_pwd_len);
struct eap_wps_enable_params {
        /* passed to eap_wps_enable() */
        u8 *dev_pwd;    /* 00000000 for push button method */
        int dev_pwd_len;
        /* filter args are not currently in use for hostapd */
        int filter_bssid_flag;   /* accept only given bssid? */
        u8  *filter_bssid;     /* 6 bytes; used if filter_bssid_flag */
        int filter_ssid_length;  /* accept only given essid? */
        u8  *filter_ssid;
        int seconds_timeout;    /* 0 for default timeout, -1 for no timeout */
        enum wps_config_who config_who;
        int do_save;            /* for WPS_CONFIG_WHO_ME */
        /* For external registrar case only: */
	u16 dev_pwd_id;     /* for external registrar : beacon i.e.s */
        u16 selreg_config_methods;      /* for external registrar : beacon i.e.s */
};
int eap_wps_enable(struct hostapd_data *hapd, struct wps_config *wps,
                struct eap_wps_enable_params *params);
/*
 * enum eap_wps_disable_reason -- reason for termination of explicit WPS job.
 *
 * Hmmm...... a global set of message identifiers would work better.
 */
enum eap_wps_disable_reason {
        /* success -- job ended with success */
        eap_wps_disable_reason_success = 0,     /* MUST be zero */
        eap_wps_disable_reason_misc_failure,
        /* bad_parameter -- job died immediately due to invalid request */
        eap_wps_disable_reason_bad_parameter,
        eap_wps_disable_reason_bad_pin,
        /* timeout -- job killed due to excessive time */
        eap_wps_disable_reason_timeout,
        /* user_stop -- job killed from user interface or higher layer */
        eap_wps_disable_reason_user_stop,
        /* registrar_stop -- registar de-selected itself */
        eap_wps_disable_reason_registrar_stop,
        /* initialization -- called once for initialization side effects */
        eap_wps_disable_reason_initialization
};
int eap_wps_disable(struct hostapd_data *hapd, struct wps_config * wps,
        enum eap_wps_disable_reason);

int eap_wps_device_password_validation(const u8 *pwd, const int len);

int eap_wps_set_ie(struct hostapd_data *hapd, struct wps_config *wps);

int eap_wps_config_get_ssid_configuration(struct hostapd_data *hapd,
										  struct wps_config *conf,
										  struct eap_wps_data *data,
										  u8 **config,
										  size_t *config_len,
										  Boolean wrap_credential);
int eap_wps_config_get_auto_configuration(struct hostapd_data *hapd,
										  struct wps_config *conf,
										  struct eap_wps_data *data,
										  u8 **config,
										  size_t *config_len,
										  Boolean wrap_credential);
int eap_wps_config_set_ssid_configuration(struct hostapd_data *hapd,
										  struct wps_config *conf,
										  u8 *raw_data, size_t raw_data_len,
										  char *out_filename);

u8 *eap_wps_config_build_message_M1(struct hostapd_data *hapd, 
        struct wps_config *conf, struct eap_wps_data *data, size_t *msg_len);
int eap_wps_config_process_message_M1(struct wps_config *conf,
									  struct eap_wps_data *data);
// u8 *eap_wps_config_build_message_M2(struct hostapd_data *hapd, struct wps_config *conf,
// 									struct eap_wps_data *data,
// 									size_t *msg_len);
// u8 *eap_wps_config_build_message_M2D(struct hostapd_data *hapd, struct wps_config *conf,
// 									 struct eap_wps_data *data,
// 									 size_t *msg_len);
int eap_wps_config_process_message_M2(struct wps_config *conf,
									  struct eap_wps_data *data,
									  Boolean *with_config);
int eap_wps_config_process_message_M2D(struct wps_config *conf,
									   struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M3(struct hostapd_data *hapd, struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M3(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M4(struct hostapd_data *hapd, struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M4(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M5(struct hostapd_data *hapd, struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M5(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M6(struct hostapd_data *hapd, struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M6(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M7(struct hostapd_data *hapd, struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M7(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_M8(struct hostapd_data *hapd, struct wps_config *conf,
									struct eap_wps_data *data,
									size_t *msg_len);
int eap_wps_config_process_message_M8(struct wps_config *conf,
									  struct eap_wps_data *data);
u8 *eap_wps_config_build_message_special(struct hostapd_data *hapd, struct wps_config *conf,
										 struct eap_wps_data *data,
										 u8 msg_type,
										 u8 *e_nonce, u8 *r_nonce,
										 size_t *msg_len);
int eap_wps_config_process_message_special(
	struct hostapd_data *hapd,
        struct wps_config *conf,
	struct eap_wps_data *data,
	u8 msg_type,
	u8 *e_nonce, 
        u8 *r_nonce);

void
eap_wps_handle_mgmt_frames(
        struct hostapd_data *hapd, 
        const u8 *src_addr, 
        const u8 *frame,     /* complete probe request */
        size_t frame_len,       /* length of frame */
        const u8 *buf,       /* WPS information elements from frame! */
        size_t len);

/* Notification types:
 *     Notifications are given ONLY for explicit WPS jobs, where
 *     the AP's button was pushed or PIN entered to AP; and NEVER for
 *     WPS operations done without user intervention (based upon
 *     the default PIN if set).
 *     A single READY is sent at the begin of the job, and a DONE
 *     is sent at the end.
 *     In between, there may be a number of EAP sessions in support of the
 *     job, each of which begins with an CONNECTED; end of EAP session
 *     is not necessarily marked.
 *     If the WPS protocol completes, a SUCCESS is sent.
 *
 *     Job begin/end:
 *              READY -- ready to do explicit WPS job (push button pressed
 *                      or PIN entered), with timeout
 *              DONE  -- always sent at end of termination of WPS job.
 *     Job event messages:
 *              CONNECTED -- EAP session begun with station
 *              SELFCONFIGURE -- WPS session wants the AP to self configure
 *                      itself.  Message content is base64 of wps ies.
 *                      Hostapd will go on to actually 
 *                      self-configure itself following sending this message
 *                      unless self-configuration is otherwise disabled.
 *              SUCCESS -- WPS operation completed OK
 *                      (Note that the EAP session always technically "fails")
 *              FAIL -- EAP session failed, may retry.
 *                      Note: In some cases, the failure is deliberate on part
 *                      of the station, which may have learned all it wants
 *                      to learn before the protocol is done.
 *              PBC_OVERLAP -- EAP session failure due to conflicting
 *                      attempted/indicated push button sessions from different
 *                      stations within 2 minutes.
 *                      This failure may be temporary.
 *              PASSWORD -- WPS needs password (PIN)
 *                      This is obsolete; PINs must be pre-provided now.
 */
enum {
    CTRL_REQ_TYPE_READY,
    CTRL_REQ_TYPE_DONE,
    CTRL_REQ_TYPE_SUCCESS,
    CTRL_REQ_TYPE_FAIL,
    CTRL_REQ_TYPE_PASSWORD,
    CTRL_REQ_TYPE_PBC_OVERLAP,
    CTRL_REQ_TYPE_CONNECTED,
    CTRL_REQ_TYPE_SELFCONFIGURE
};


/* Send notifications to outside world...
 */
void eap_wps_request(
        struct hostapd_data *hapd,
	int req_type, 
        const char *msg);

struct wps_data;
int wps_config_add_atheros_wps_ext(
        struct hostapd_data *hapd, struct wps_data *wps);

int wps_set_ssid_configuration(
        struct hostapd_data *hapd,
	u8 *raw_data, size_t raw_data_len,
        int do_save,    /* if zero, we don't save the changes */
        int do_restart  /* if zero, we don't restart after saving changes */
        );


#endif /* EAP_WPS_H */
