/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wps_opt_nfc.c
//  Description: EAP-WPS NFC option source
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
#include "wpa.h"
#include "eloop.h"
#include "config.h"
#include "wps_config.h"
#include "wpa_ctrl.h"
#include "state_machine.h"
#include "wps_parser.h"
#include "wps_opt_nfc.h"
#include "eap_wps.h"
#include "WpsNfcType.h"
#include "WpsNfc.h"

#define STATE_MACHINE_DATA struct wps_opt_nfc_sm
#define STATE_MACHINE_DEBUG_PREFIX "OPT_NFC"

#define WPS_OPT_NFC_COMP_FILENAME "wps_opf_nfc_cmp.conf"

#ifndef WPSNFCLIB_VERSION
#ifndef LIB_VERSION
#define WPSNFCLIB_VERSION 0
#define LIB_VERSION(a, b, c) 1
#else
#define WPSNFCLIB_VERSION LIB_VERSION(1, 0, 0) /* 1.0.0 */
#endif /* LIB_VERSION */
#endif /* WPSNFCLIB_VERSION */

/**
 * struct wps_opt_nfc_sm - Internal data for NFC state machines
 */

typedef enum {
	OPT_NFC_INACTIVE = 0,
	OPT_NFC_IDLE,
	OPT_NFC_SCANNING,
	OPT_NFC_SCAN_TIMEOUT,
	OPT_NFC_FOUND_TOKEN,
} opt_nfc_states;

struct wps_opt_nfc_sm {
 	opt_nfc_states OPT_NFC_state;
	Boolean changed;
 	struct wps_opt_nfc_sm_ctx *ctx;
	const char *nfcname;
	Boolean initialized;
	Boolean isOpenedDevice;
	Boolean existing;
	Boolean enablePort;
#define NFC_LOOP_PERIOD_SEC		1 /* [sec] */
#define NFC_LOOP_PERIOD_USEC	0 /* [usec] */
#define SCAN_TIMEOUT_SEC		30 /* [sec] */
#define SCAN_TIMEOUT_USEC		0 /* [usec] */
	struct os_time scanTimeout;
	Boolean foundToken;
	Boolean cancelCmd;
	enum {
		OPT_NFC_CMD_NONE = 0,
		OPT_NFC_CMD_READ,
		OPT_NFC_CMD_WRITE,
	} OPT_NFC_CMD_state;
	u8 *readBuf;
	u32 readBufLen;
#if WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1)
	int (*readCallback)(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len);
#else /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
	int (*readCallback)(struct wps_opt_nfc_sm *sm);
#endif /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
	void (*readTimeoutCallback)(struct wps_opt_nfc_sm *sm);
	u8 *writeBuf;
	u32 writeBufLen;
	int (*writeCallback)(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len);
	void (*writeTimeoutCallback)(struct wps_opt_nfc_sm *sm);
 };

enum wps_opt_nfc_ctrl_req_type {
	CTRL_REQ_TYPE_READ_TIMEOUT,
	CTRL_REQ_TYPE_WRITE_TIMEOUT,
	CTRL_REQ_TYPE_FAIL_READ,
	CTRL_REQ_TYPE_COMP_READ,
	CTRL_REQ_TYPE_COMP_WRITE
};


static int wps_opt_nfc_sm_is_timeout(struct wps_opt_nfc_sm *sm)
{
	struct os_time now;
	os_get_time(&now);

	if (now.sec > sm->scanTimeout.sec)
		return 1;
	else if ((now.sec == sm->scanTimeout.sec) &&
			 (now.usec >= sm->scanTimeout.usec))
		return 1;
	else
		return 0;
}


static void wps_opt_nfc_sm_request(struct wps_opt_nfc_sm *sm,
			   enum wps_opt_nfc_ctrl_req_type type,
			   const char *msg, size_t msglen)
{
	char *buf;
	size_t buflen;
	int len = 0;
	char *field;
	char *txt;

	if (sm == NULL)
		return;

	switch (type) {
	case CTRL_REQ_TYPE_READ_TIMEOUT:
		field = "NFC_READ_TIMEOUT";
		txt = "Request Timeout";
		break;
	case CTRL_REQ_TYPE_WRITE_TIMEOUT:
		field = "NFC_WRITE_TIMEOUT";
		txt = "Request Timeout";
		break;
	case CTRL_REQ_TYPE_FAIL_READ:
		field = "NFC_FAIL_READ";
		txt = "Fail Reading Token";
		break;
	case CTRL_REQ_TYPE_COMP_READ:
		field = "NFC_COMP_READ";
		txt = "Complete Reading Token";
		break;
	case CTRL_REQ_TYPE_COMP_WRITE:
		field = "NFC_COMP_WRITE";
		txt = "Complete Writing Token";
		break;
	default:
		return;
	}

	buflen = 100 + os_strlen(txt);
	buf = os_malloc(buflen);
	if (buf == NULL)
		return;
	len = os_snprintf(buf + len, buflen - len, WPA_CTRL_REQ "%s%s%s%s-%s ",
		       field, msg?":[":"", msg?msg:"", msg?"]":"", txt);
	if (len < 0 || (size_t) len >= buflen) {
		os_free(buf);
		return;
	}
	buf[buflen - 1] = '\0';
	hostapd_msg(sm->ctx->msg_ctx, MSG_INFO, "%s", buf);
	os_free(buf);
}

static void wps_opt_nfc_port_timer_tick(void *wps_opt_nfc_ctx, void *timeout_ctx)
{
	struct wps_opt_nfc_sm *sm = timeout_ctx;

	eloop_register_timeout(NFC_LOOP_PERIOD_SEC, NFC_LOOP_PERIOD_USEC, wps_opt_nfc_port_timer_tick, wps_opt_nfc_ctx, sm);
	wps_opt_nfc_sm_step(sm);
}

SM_STATE(OPT_NFC, INACTIVE)
{
	SM_ENTRY(OPT_NFC, INACTIVE);

	if (sm->isOpenedDevice) {
		WpsNfcCloseDevice();
		sm->isOpenedDevice = 0;
	}
}


SM_STATE(OPT_NFC, IDLE)
{
	SM_ENTRY(OPT_NFC, IDLE);

	if (!sm->isOpenedDevice && sm->nfcname &&
		(WPS_NFCLIB_ERR_SUCCESS ==
		 WpsNfcOpenDevice((const int8 * const)sm->nfcname)))
		 sm->isOpenedDevice = 1;
}


SM_STATE(OPT_NFC, SCANNING)
{
	uint32 nfcRet;
	SM_ENTRY(OPT_NFC, SCANNING)

	do {
		nfcRet = WpsNfcTokenDiscovery();
		if (WPS_NFCLIB_ERR_SUCCESS == nfcRet) {
			sm->foundToken = 1;
			break;
		}

		if (WPS_NFCLIB_ERR_TARGET_NOT_FOUND != nfcRet)
			sm->enablePort = 0;

		sm->foundToken = 0;
	} while (0);
}


SM_STATE(OPT_NFC, SCAN_TIMEOUT)
{
	SM_ENTRY(OPT_NFC, SCAN_TIMEOUT);
	switch (sm->OPT_NFC_CMD_state) {
	case OPT_NFC_CMD_READ:
		os_free(sm->readBuf);
		sm->readBuf = 0;
		sm->readBufLen = 0;
		if (!sm->cancelCmd && sm->readTimeoutCallback)
			sm->readTimeoutCallback(sm);
		break;
	case OPT_NFC_CMD_WRITE:
		os_free(sm->writeBuf);
		sm->writeBuf = 0;
		sm->writeBufLen = 0;
		if (!sm->cancelCmd && sm->writeTimeoutCallback)
			sm->writeTimeoutCallback(sm);
		break;
	default:
		break;
	}
	sm->cancelCmd = 0;
	sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;
}


SM_STATE(OPT_NFC, FOUND_TOKEN)
{
#if WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1)
	uint32 buf_len;
#endif /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */

	SM_ENTRY(OPT_NFC, FOUND_TOKEN)

	switch (sm->OPT_NFC_CMD_state) {
	case OPT_NFC_CMD_READ:
#if WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1)
		if (WPS_NFCLIB_ERR_SUCCESS ==
			WpsNfcReadToken((int8 * const)sm->readBuf,
				            (uint32 * const)&buf_len)) {
			sm->readBufLen = buf_len;
			if (sm->readCallback)
				sm->readCallback(sm, sm->readBuf, sm->readBufLen);
			os_free(sm->readBuf);
			sm->readBuf = 0;
			sm->readBufLen = 0;
			sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;
		} else
			sm->foundToken = 0;
#else /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
		if (sm->readCallback && sm->readCallback(sm)) {
			sm->foundToken = 0;
			break;
		}
		os_free(sm->readBuf);
		sm->readBuf = 0;
		sm->readBufLen = 0;
		sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;
#endif /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
		break;
	case OPT_NFC_CMD_WRITE:
		if (WPS_NFCLIB_ERR_SUCCESS ==
			WpsNfcWriteToken((int8 * const)sm->writeBuf, sm->writeBufLen)) {
			sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;
			if (sm->writeCallback)
				sm->writeCallback(sm, sm->writeBuf, sm->writeBufLen);
			os_free(sm->writeBuf);
			sm->writeBuf = 0;
			sm->writeBufLen = 0;
			sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;
		} else
			sm->foundToken = 0;
		break;
	default:
		if (sm->readBuf) {
			os_free(sm->readBuf);
			sm->readBuf = 0;
			sm->readBufLen = 0;
		}
		if (sm->writeBuf) {
			os_free(sm->writeBuf);
			sm->writeBuf = 0;
			sm->writeBufLen = 0;
		}
		sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;
		break;
	}
}


SM_STEP(OPT_NFC)
{
	if (sm->existing)
		sm->enablePort = 0;
	else if (!sm->initialized) {
		if (WPS_NFCLIB_ERR_SUCCESS == WpsNfcInit()) {
			sm->initialized = 1;
			sm->isOpenedDevice = 0;
		}
	}

	if (!sm->initialized)
		return;

	do {
		sm->changed = 0;

		if (OPT_NFC_CMD_NONE != sm->OPT_NFC_CMD_state) {
			if (sm->cancelCmd || wps_opt_nfc_sm_is_timeout(sm))
				SM_ENTER_GLOBAL(OPT_NFC, SCAN_TIMEOUT);
		}

		switch(sm->OPT_NFC_state) {
		case OPT_NFC_INACTIVE:
			if (sm->enablePort)
				SM_ENTER_GLOBAL(OPT_NFC, IDLE);
			break;
		case OPT_NFC_IDLE:
			if (!sm->enablePort)
				SM_ENTER_GLOBAL(OPT_NFC, INACTIVE);
			else if (sm->OPT_NFC_CMD_state != OPT_NFC_CMD_NONE) {
				SM_ENTER_GLOBAL(OPT_NFC, SCANNING);
				if (OPT_NFC_SCANNING == sm->OPT_NFC_state)
					sm->changed = 0;
			}
			break;
		case OPT_NFC_SCANNING:
			if (!sm->enablePort)
				SM_ENTER_GLOBAL(OPT_NFC, INACTIVE);
			else {
				if (sm->foundToken)
					SM_ENTER_GLOBAL(OPT_NFC, FOUND_TOKEN);
				else if (OPT_NFC_CMD_NONE == sm->OPT_NFC_CMD_state)
					SM_ENTER_GLOBAL(OPT_NFC, IDLE);
				else
					SM_ENTER_GLOBAL(OPT_NFC, SCANNING);
			}
			break;
		case OPT_NFC_SCAN_TIMEOUT:
			SM_ENTER_GLOBAL(OPT_NFC, IDLE);
			break;
		case OPT_NFC_FOUND_TOKEN:
			wpa_printf(MSG_DEBUG, "WPS_OPT_NFC: Found Token");
			if (!sm->enablePort) {
				SM_ENTER_GLOBAL(OPT_NFC, INACTIVE);
				break;
			} else if (!sm->foundToken)
				SM_ENTER_GLOBAL(OPT_NFC, SCANNING);
			else
				SM_ENTER_GLOBAL(OPT_NFC, IDLE);
			break;
		default:
			sm->enablePort = 0;
			SM_ENTER_GLOBAL(OPT_NFC, INACTIVE);
			break;
		}
	} while (sm->changed);

	if ((sm->existing) && (sm->initialized)) {
			WpsNfcDeinit();
			sm->initialized = 0;
			sm->existing = 0;
	}
}


void wps_opt_nfc_sm_step(struct wps_opt_nfc_sm *sm)
{
	SM_STEP_RUN(OPT_NFC);
}

void wps_opt_nfc_sm_set_ifname(struct wps_opt_nfc_sm *sm, const char *nfcname)
{
	if (sm) {
		sm->nfcname = nfcname;
	}
}

struct wps_opt_nfc_sm *wps_opt_nfc_sm_init(struct wps_opt_nfc_sm_ctx *ctx)
{
	struct wps_opt_nfc_sm *sm;

	sm = wpa_zalloc(sizeof(*sm));
	if (sm == NULL)
		return NULL;
	sm->ctx = ctx;

	eloop_register_timeout(NFC_LOOP_PERIOD_SEC, NFC_LOOP_PERIOD_USEC, wps_opt_nfc_port_timer_tick, NULL, sm);

	return sm;
}

void wps_opt_nfc_sm_deinit(struct wps_opt_nfc_sm *sm)
{
	if (sm == NULL)
		return;
	eloop_cancel_timeout(wps_opt_nfc_port_timer_tick, sm->ctx, sm);
	sm->existing = 1;
	wps_opt_nfc_sm_step(sm);
	os_free(sm->ctx);
	os_free(sm);
}


static void wps_opt_nfc_sm_command(void *wps_opt_nfc_ctx, void *timeout_ctx)
{
	struct wps_opt_nfc_sm *sm = (struct wps_opt_nfc_sm *)wps_opt_nfc_ctx;
	struct wps_opt_nfc_sm *cmd = (struct wps_opt_nfc_sm *)timeout_ctx;

	if ((OPT_NFC_INACTIVE == sm->OPT_NFC_state) ||
		(OPT_NFC_IDLE == sm->OPT_NFC_state)) {
		switch (cmd->OPT_NFC_CMD_state) {
		case OPT_NFC_CMD_READ:
			sm->readBuf = cmd->readBuf;
			sm->readBufLen = cmd->readBufLen;
			sm->readCallback = cmd->readCallback;
			sm->readTimeoutCallback = cmd->readTimeoutCallback;
			break;
		case OPT_NFC_CMD_WRITE:
			sm->writeBuf = cmd->writeBuf;
			sm->writeBufLen = cmd->writeBufLen;
			sm->writeCallback = cmd->writeCallback;
			sm->writeTimeoutCallback = cmd->writeTimeoutCallback;
			break;
		default:
			break;
		}
		os_get_time(&sm->scanTimeout);
		sm->scanTimeout.sec += SCAN_TIMEOUT_SEC;
		sm->scanTimeout.usec += SCAN_TIMEOUT_USEC;
		sm->OPT_NFC_CMD_state = cmd->OPT_NFC_CMD_state;

		os_free(cmd);
		if (!sm->enablePort)
			sm->enablePort = 1;
	} else
		eloop_register_timeout(NFC_LOOP_PERIOD_SEC, NFC_LOOP_PERIOD_USEC, wps_opt_nfc_sm_command, sm, cmd);
}


static int wps_opt_nfc_sm_read_command(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len,
#if WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1)
		int (*callback)(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len),
#else /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
		int (*callback)(struct wps_opt_nfc_sm *sm),
#endif /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
		void (*timeout)(struct wps_opt_nfc_sm *sm))
{
	struct wps_opt_nfc_sm *cmd;

	if (sm->cancelCmd)
		sm->cancelCmd = 0;

	cmd = (struct wps_opt_nfc_sm *)calloc(sizeof(struct wps_opt_nfc_sm), 1);
	if (!cmd)
		return -1;

	cmd->readBuf = buf;
	cmd->readBufLen = len;
	cmd->OPT_NFC_CMD_state = OPT_NFC_CMD_READ;
	cmd->readCallback = callback;
	cmd->readTimeoutCallback = timeout;

	if (OPT_NFC_CMD_NONE != sm->OPT_NFC_CMD_state)
		sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;

	eloop_register_timeout(NFC_LOOP_PERIOD_SEC, NFC_LOOP_PERIOD_USEC, wps_opt_nfc_sm_command, sm, cmd);

	return 0;
}


static int wps_opt_nfc_sm_write_command(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len,
		int (*callback)(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len),
		void (*timeout)(struct wps_opt_nfc_sm *sm))
{
	struct wps_opt_nfc_sm *cmd;

	if (sm->cancelCmd)
		sm->cancelCmd = 0;

	cmd = (struct wps_opt_nfc_sm *)calloc(sizeof(struct wps_opt_nfc_sm), 1);
	if (!cmd)
		return -1;

	cmd->writeBuf = buf;
	cmd->writeBufLen = len;
	cmd->OPT_NFC_CMD_state = OPT_NFC_CMD_WRITE;
	cmd->writeCallback = callback;
	cmd->writeTimeoutCallback = timeout;

	if (OPT_NFC_CMD_NONE != sm->OPT_NFC_CMD_state)
		sm->OPT_NFC_CMD_state = OPT_NFC_CMD_NONE;

	eloop_register_timeout(NFC_LOOP_PERIOD_SEC, NFC_LOOP_PERIOD_USEC, wps_opt_nfc_sm_command, sm, cmd);

	return 0;
}


static int wps_opt_nfc_read_password_callback(struct wps_opt_nfc_sm *sm, u8 *buf, size_t len)
{
	Boolean ret = -1;
	struct wps_data *wps = 0;
	struct wps_config *conf;
	u8 version;
	u8 *oobdevpwd = 0;
	size_t length;
	u8 *pwd;
	size_t pwd_len;
	u8 dev_pwd[SIZE_64_BYTES + 1];
	char msg[32];

	do {
		conf = (sm->ctx->get_conf(sm->ctx->ctx))->wps;
		if (!conf)
			break;

		if (wps_create_wps_data(&wps))
			break;

		if(wps_parse_wps_data(buf, len, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, NULL))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* OOB Device Password */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_OOB_DEV_PWD, NULL, &length);
		if (!length)
			break;
		oobdevpwd = (u8 *)calloc(1, length);
		if (!oobdevpwd)
			break;
		if(wps_get_value(wps, WPS_TYPE_OOB_DEV_PWD, oobdevpwd, &length))
			break;

		if (length < 20 + 2 + 6)
			break;

		os_memcpy(conf->pub_key, oobdevpwd, 20);
		conf->set_pub_key = 1;
		conf->dev_pwd_id = WPA_GET_BE16(oobdevpwd + 20);
		pwd = oobdevpwd + 22;
		pwd_len = length - 22;
		conf->dev_pwd_len = wpa_snprintf_hex_uppercase((char *)dev_pwd, sizeof(dev_pwd), pwd, pwd_len);
		os_memcpy(conf->dev_pwd, dev_pwd, sizeof(conf->dev_pwd));

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);
	if (oobdevpwd)
		os_free(oobdevpwd);

	os_snprintf(msg, sizeof(msg), "Password Token");
	if (ret) {
		wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_FAIL_READ, msg, os_strlen(msg));
	} else {
		wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_COMP_READ, msg, os_strlen(msg));
	}

	return ret;
}


#if WPSNFCLIB_VERSION >= LIB_VERSION(1, 1, 1)
static int wps_opt_nfc_read_password_callback2(struct wps_opt_nfc_sm *sm)
{
	Boolean ret = -1;
	struct wps_data *wps = 0;
	u8 version;
	size_t length;
	int8 *buf = 0;
	uint32 len = 0;
	uint32 num = 0;
	uint32 i;

	do {
		if (WPS_NFCLIB_ERR_SUCCESS != WpsNfcReadTokenMessage(&num))
			break;

		do {
			if (!num)
				break;

			for (i = 0; (i < num) && ret; i++) {
				len = 0;
				if (WPS_NFCLIB_ERR_SUCCESS !=
					WpsNfcGetRecordFromMessage(i, &buf, &len))
					continue;

				do {
					if (wps_create_wps_data(&wps))
						break;

					if(wps_parse_wps_data((uint8 *)buf, len, wps))
						break;

					/* Version */
					if (wps_get_value(wps, WPS_TYPE_VERSION, &version, NULL))
						break;
					if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
						break;

					/* OOB Device Password */
					length = 0;
					(void)wps_get_value(wps, WPS_TYPE_OOB_DEV_PWD, NULL, &length);
					if (!length)
						break;

					ret = 0;
				} while (0);

				(void)wps_destroy_wps_data(&wps);

				if (ret) {
					if (buf) os_free(buf);
					buf = 0;
				}
			}
		} while (0);
		(void)wps_opt_nfc_read_password_callback(sm, (uint8 *)buf, len);
		ret = 0;
	} while (0);

	if (buf) os_free(buf);

	return ret;
}
#endif /* WPSNFCLIB_VERSION >= LIB_VERSION(1, 1, 1) */


static void wps_opt_nfc_read_password_timeout_callback(struct wps_opt_nfc_sm *sm)
{
	char msg[32];
	os_snprintf(msg, sizeof(msg), "Password Token");
	wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_READ_TIMEOUT, msg, os_strlen(msg));
}


static int wps_opt_nfc_write_password_callback(struct wps_opt_nfc_sm *sm, u8 *buf, size_t len)
{
	char msg[32];
	os_snprintf(msg, sizeof(msg), "Password Token");
	wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_COMP_WRITE, msg, os_strlen(msg));
	return 0;
}


static void wps_opt_nfc_write_password_timeout_callback(struct wps_opt_nfc_sm *sm)
{
	char msg[32];
	os_snprintf(msg, sizeof(msg), "Password Token");
	wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_WRITE_TIMEOUT, msg, os_strlen(msg));
}


static int wps_opt_nfc_read_config_callback(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len)
{
	Boolean ret = -1;
	struct wps_data *wps = 0;
	u8 version;
	u8 * credential = 0;
	size_t length;
	char msg[64];

	do {
		if (wps_create_wps_data(&wps))
			break;

		if(wps_parse_wps_data(buf, len, wps))
			break;

		/* Version */
		if (wps_get_value(wps, WPS_TYPE_VERSION, &version, NULL))
			break;
		if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
			break;

		/* Creadential */
		length = 0;
		(void)wps_get_value(wps, WPS_TYPE_CREDENTIAL, NULL, &length);
		if (!length)
			break;
		credential = (u8 *)calloc(1, length);
		if (!credential)
			break;
		if(wps_get_value(wps, WPS_TYPE_CREDENTIAL, credential, &length))
			break;

		if (wps_set_ap_ssid_configuration(sm->ctx->ctx, WPS_OPT_NFC_COMP_FILENAME, 1, &credential, &length, 1))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);
	if (credential)
		os_free(credential);

	if (ret) {
		os_snprintf(msg, sizeof(msg), "Config Token");
		wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_FAIL_READ, msg, os_strlen(msg));
	} else {
		os_snprintf(msg, sizeof(msg), "Config Token:%s", WPS_OPT_NFC_COMP_FILENAME);
		wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_COMP_READ, msg, os_strlen(msg));
	}

	return ret;
}


#if WPSNFCLIB_VERSION >= LIB_VERSION(1, 1, 1)
static int wps_opt_nfc_read_config_callback2(struct wps_opt_nfc_sm *sm)
{
	Boolean ret = -1;
	struct wps_data *wps = 0;
	u8 version;
	size_t length;
	int8 *buf = 0;
	uint32 len = 0;
	uint32 num = 0;
	uint32 i;

	do {
		if (WPS_NFCLIB_ERR_SUCCESS != WpsNfcReadTokenMessage(&num))
			break;

		do {
			if (!num)
				break;

			for (i = 0; (i < num) && ret; i++) {
				len = 0;
				if (WPS_NFCLIB_ERR_SUCCESS !=
					WpsNfcGetRecordFromMessage(i, &buf, &len))
					continue;

				do {
					if (wps_create_wps_data(&wps))
						break;

					if(wps_parse_wps_data((uint8 *)buf, len, wps))
						break;

					/* Version */
					if (wps_get_value(wps, WPS_TYPE_VERSION, &version, NULL))
						break;
					if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
						break;

					/* Creadential */
					length = 0;
					(void)wps_get_value(wps, WPS_TYPE_CREDENTIAL, NULL, &length);
					if (!length)
						break;

					ret = 0;
				} while (0);

				(void)wps_destroy_wps_data(&wps);

				if (ret) {
					if (buf) os_free(buf);
					buf = 0;
				}
			}
		} while (0);
		(void)wps_opt_nfc_read_config_callback(sm, (uint8 *)buf, len);
		ret = 0;
	} while (0);

	if (buf) os_free(buf);

	return ret;
}
#endif /* WPSNFCLIB_VERSION >= LIB_VERSION(1, 1, 1) */


static void wps_opt_nfc_read_config_timeout_callback(struct wps_opt_nfc_sm *sm)
{
	char *msg = "Config Token";
	wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_READ_TIMEOUT, msg, os_strlen(msg));
}


static int wps_opt_nfc_write_config_callback(struct wps_opt_nfc_sm *sm, u8 * buf, size_t len)
{
	int ret = 0;
	struct hostapd_bss_config *conf;
	struct wps_config *wps = 0;
	struct wps_data *data = 0;
	u8 version;
	u8 * credential = 0;
	size_t length;
	char msg[64];

	os_snprintf(msg, sizeof(msg), "Config Token");

	do {
		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf && !conf->wps)
			break;
		wps = conf->wps;

		if (WPS_WPSSTATE_UNCONFIGURED == wps->wps_state) {
			ret = -1;

			do {
				if (wps_create_wps_data(&data))
					break;

				if(wps_parse_wps_data(buf, len, data))
					break;

				/* Version */
				if (wps_get_value(data, WPS_TYPE_VERSION, &version, NULL))
					break;
				if ((version != WPS_VERSION) && (version != WPS_VERSION_EX))
					break;

				/* Creadential */
				length = 0;
				(void)wps_get_value(data, WPS_TYPE_CREDENTIAL, NULL, &length);
				if (!length)
					break;
				credential = (u8 *)calloc(1, length);
				if (!credential)
					break;
				if(wps_get_value(data, WPS_TYPE_CREDENTIAL, credential, &length))
					break;

				if (wps_set_ap_ssid_configuration(sm->ctx->ctx, WPS_OPT_NFC_COMP_FILENAME, 1, &credential, 1, &length, 1))
					break;

				ret = 0;
			} while (0);

			(void)wps_destroy_wps_data(&data);
			if (credential)
				os_free(credential);

			if (!ret)
				os_snprintf(msg, sizeof(msg), "Config Token:%s", WPS_OPT_NFC_COMP_FILENAME);
		}
	} while (0);

	wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_COMP_WRITE, msg, os_strlen(msg));

	return ret;
}


static void wps_opt_nfc_write_config_timeout_callback(struct wps_opt_nfc_sm *sm)
{
	char msg[32];
	os_snprintf(msg, sizeof(msg), "Config Token");
	wps_opt_nfc_sm_request(sm, CTRL_REQ_TYPE_WRITE_TIMEOUT, msg, os_strlen(msg));
}


int wps_opt_nfc_cancel_nfc_comand(struct wps_opt_nfc_sm *sm)
{
	sm->cancelCmd = 1;
	return 0;
}


static int wps_opt_nfc_generate_oob_device_password(
	struct wps_opt_nfc_sm *sm, u8 *hash,
	u16 *dev_pwd_id, u8 *pwd, int pwd_len)
{
	int ret = -1;
	struct hostapd_bss_config *conf;
	struct wps_config *wps = 0;
	u8 dev_pwd[SIZE_64_BYTES + 1];
	int dev_pwd_len;
	u8 tmp[SIZE_256_BITS];

	do {
		if (!hash || !dev_pwd_id || !pwd )
			break;

		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf && !conf->wps)
			break;
		wps = conf->wps;
		if ((16 > pwd_len) || ((pwd_len * 2) > sizeof(wps->dev_pwd)))
			break;

		if (eap_wps_generate_public_key(&wps->dh_secret, wps->pub_key))
			break;
		wps->set_pub_key = 1;

		if (eap_wps_generate_sha256hash(wps->pub_key, sizeof(wps->pub_key), tmp))
			break;
		os_memcpy(hash, tmp, SIZE_20_BYTES);

		if (eap_wps_generate_device_password_id(dev_pwd_id))
			break;

		if (eap_wps_generate_device_password(pwd, pwd_len))
			break;

		dev_pwd_len = wpa_snprintf_hex_uppercase((char *)dev_pwd, sizeof(dev_pwd), pwd, pwd_len);
		if (dev_pwd_len != (pwd_len * 2))
			break;

		wps->dev_pwd_id = *dev_pwd_id;
		os_memcpy(wps->dev_pwd, dev_pwd, dev_pwd_len);
		wps->dev_pwd_len = dev_pwd_len;

		ret = 0;
	} while (0);

	if (ret) {
		if (wps) {
			if (wps->dh_secret)
				eap_wps_free_dh(&wps->dh_secret);
			wps->set_pub_key = 0;

			wps->dev_pwd_len = 0;
			wps->dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;
		}
	}

	return ret;
}


int wps_opt_nfc_read_password_token(struct wps_opt_nfc_sm *sm)
{
#define DEFAULT_READ_BUF_SIZE 0x800
	int ret = -1;
	struct hostapd_bss_config *conf;
	u8 * buf = 0;
	size_t len;

	do {
		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf)
			break;

		buf = (u8 *)calloc(1, DEFAULT_READ_BUF_SIZE);
		if (!buf)
			break;
		len = DEFAULT_READ_BUF_SIZE;
		if (wps_opt_nfc_sm_read_command(sm, buf, len,
#if WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1)
										wps_opt_nfc_read_password_callback,
#else /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
										wps_opt_nfc_read_password_callback2,
#endif /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
										wps_opt_nfc_read_password_timeout_callback))
			break;

		ret = 0;
	} while (0);

	if (ret && buf) {
		os_free(buf);
		buf = 0;
		len = 0;
	}

	return ret;
#undef DEFAULT_READ_BUF_SIZE
}


int wps_opt_nfc_write_password_token(struct wps_opt_nfc_sm *sm)
{
	int ret = -1;
	struct hostapd_bss_config *conf;
	struct wps_data *wps = 0;
	u8 version;
	u8 pub_key_hash[20];
	u16 dev_pwd_id;
	u8 dev_pwd[32];
	u8 oob_dev_pwd[sizeof(pub_key_hash) + sizeof(dev_pwd_id) + sizeof(dev_pwd)];
	u8 *tmp;
	u8 *buf = 0;
	size_t buf_len;

	do {
		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf)
			break;

		if (wps_create_wps_data(&wps))
			break;

		if (wps_opt_nfc_generate_oob_device_password(sm, pub_key_hash,
					&dev_pwd_id, dev_pwd, sizeof(dev_pwd)))
			break;
		tmp = oob_dev_pwd;
		os_memcpy(tmp, pub_key_hash, sizeof(pub_key_hash));
		tmp += sizeof(pub_key_hash);
		WPA_PUT_BE16(tmp, dev_pwd_id);
		tmp += sizeof(dev_pwd_id);
		os_memcpy(tmp, dev_pwd, sizeof(dev_pwd));
		tmp += sizeof(dev_pwd);

		/* Version */
		if (conf->wps && conf->wps->version)
			version = conf->wps->version;
		else
			version = WPS_VERSION;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;

		/* OOB Device Password */
		if (wps_set_value(wps, WPS_TYPE_OOB_DEV_PWD, oob_dev_pwd, sizeof(oob_dev_pwd)))
			break;

		if(wps_write_wps_data(wps, &buf, &buf_len))
			break;

		if (wps_opt_nfc_sm_write_command(sm, buf, buf_len,
										 wps_opt_nfc_write_password_callback,
										 wps_opt_nfc_write_password_timeout_callback))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);
	if (ret && buf) {
		os_free(buf);
		buf = 0;
	}

	return ret;
}


int wps_opt_nfc_read_config_token(struct wps_opt_nfc_sm *sm)
{
#define DEFAULT_READ_BUF_SIZE 0x800
	int ret = -1;
	u8 * buf = 0;
	size_t len;

	do {
		buf = (u8 *)calloc(1, DEFAULT_READ_BUF_SIZE);
		if (!buf)
			break;
		len = DEFAULT_READ_BUF_SIZE;
		if (wps_opt_nfc_sm_read_command(sm, buf, len,
#if WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1)
										wps_opt_nfc_read_config_callback,
#else /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
										wps_opt_nfc_read_config_callback2,
#endif /* WPSNFCLIB_VERSION < LIB_VERSION(1, 1, 1) */
										wps_opt_nfc_read_config_timeout_callback))
			break;
		ret = 0;
	} while (0);

	if (ret && buf) {
		os_free(buf);
		buf = 0;
		len = 0;
	}

	return ret;
#undef DEFAULT_READ_BUF_SIZE
}


int wps_opt_nfc_write_config_token(struct wps_opt_nfc_sm *sm)
{
	int ret = -1;
	struct hostapd_bss_config *conf;
	struct wps_data *wps = 0;
	u8 version;
	u8 nwIdx;
	u8 *buf = 0, *tmp = 0;
	size_t tmp_len;
	size_t buf_len;
	size_t len;

	do {
		conf = sm->ctx->get_conf(sm->ctx->ctx);
		if (!conf)
			break;

		/* Make Creadential Attribute */
                #if 0   /* Was, from Sony */
		if (conf->wps && (WPS_WPSSTATE_UNCONFIGURED == conf->wps->wps_state)) {
			if (wps_get_ap_auto_configuration(sm->ctx->ctx, &tmp, &tmp_len))
				break;
		} else {
                #else   /* Was, from Sony */
                {
                #endif  /* Was, from Sony */
			if (wps_get_ap_ssid_configuration(sm->ctx->ctx, &tmp, &tmp_len, 0, 0/*nwIdx*/, 0 /*?*/, 0/*? autoconfig*/))
				break;
		}

		if (wps_create_wps_data(&wps))
			break;

		/* Network Index */
		nwIdx = 1;
		if (wps_set_value(wps, WPS_TYPE_NW_INDEX, &nwIdx, 0))
			break;

		buf_len = 0;
		if (wps_write_wps_data(wps, &buf, &buf_len))
			break;

		(void)wps_destroy_wps_data(&wps);

		buf = os_realloc(buf, buf_len + tmp_len);
		if (!buf)
			break;
		os_memcpy(buf + buf_len, tmp, tmp_len);
		buf_len += tmp_len;

		if (wps_create_wps_data(&wps))
			break;

		/* Version */
		if (conf->wps && conf->wps->version)
			version = conf->wps->version;
		else
			version = WPS_VERSION;
		if (wps_set_value(wps, WPS_TYPE_VERSION, &version, 0))
			break;

		/* Credential */
		if (wps_set_value(wps, WPS_TYPE_CREDENTIAL, buf, (u16)buf_len))
			break;

		os_free(buf);
		buf = 0;
		len = 0;
		if(wps_write_wps_data(wps, &buf, &len))
			break;

		if (wps_opt_nfc_sm_write_command(sm, buf, len,
										 wps_opt_nfc_write_config_callback,
										 wps_opt_nfc_write_config_timeout_callback))
			break;

		ret = 0;
	} while (0);

	(void)wps_destroy_wps_data(&wps);
	if (ret && buf) {
		os_free(buf);
		buf = 0;
	}

	if (tmp)
		os_free(tmp);

	return ret;
}


