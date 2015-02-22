/*
 * hostapd / Wi-Fi Simple Configuration 7C Proposal
 * Copyright (c) 2005 Intel Corporation. All rights reserved.
 * Contact Information: Harsha Hegde  <harsha.hegde@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README, README_WSC and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "eloop.h"
#include "hostapd.h"
#include "driver.h"
#include "UdpLib.h"
#include "wsc_ie.h"

// Comment out the next line if debug strings are not needed
#define U_DEBUG

void DEBUGF(char *, ...);

static WSC_IE_DATA * g_wsc_ie_data;


static void wsc_ie_read_callback(int sock, void *eloop_ctx, void *sock_ctx)
{
	WSC_IE_DATA * data = eloop_ctx;
	WSC_IE_COMMAND_DATA * cmdData;
	char readBuf[WSC_WLAN_DATA_MAX_LENGTH];
	int recvBytes;
	struct sockaddr_in from;
	u8 * bufPtr;

	wpa_printf(MSG_DEBUG, "WSC_IE: Entered wsc_ie_read_callback. " "sock = %d", sock);

	recvBytes = udp_read(data->udpFdCom, readBuf,
            			WSC_WLAN_DATA_MAX_LENGTH, &from);

	if (recvBytes == -1) {
		wpa_printf(MSG_ERROR, "WSC_IE: Reading Command message "
				"from upper layer failed");
		return;
	}

	cmdData = (WSC_IE_COMMAND_DATA *) readBuf;

	if (cmdData->type == WSC_IE_TYPE_SET_BEACON_IE) {
		wpa_printf(MSG_DEBUG, "WSC_IE: SET_BEACON_IE from upper layer");
		bufPtr = (u8 *) &(cmdData->data[0]);
		hostapd_set_wsc_beacon_ie(data->hapd, bufPtr, cmdData->length);

	} else if (cmdData->type == WSC_IE_TYPE_SET_PROBE_RESPONSE_IE) {
		wpa_printf(MSG_DEBUG, "WSC_IE: SET_PR_RESP_IE from upper layer");
		bufPtr = (u8 *) &(cmdData->data[0]);
		hostapd_set_wsc_probe_resp_ie(data->hapd, bufPtr, cmdData->length);
	} else {
		wpa_printf(MSG_ERROR, "WSC_IE: Wrong command type from upper layer");
		return;
	}
	return;
}

int wsc_ie_init(struct hostapd_data *hapd)
{
	struct sockaddr_in to;
	char sendBuf[5];

	wpa_printf(MSG_DEBUG, "WSC_IE: In wsc_ie_init");
	
	g_wsc_ie_data = malloc(sizeof(WSC_IE_DATA));
	
	if (g_wsc_ie_data == NULL) {
		return -1;
	}

	memset(g_wsc_ie_data, 0, sizeof(WSC_IE_DATA));

	g_wsc_ie_data->hapd = hapd;
    	g_wsc_ie_data->udpFdCom = udp_open();

	eloop_register_read_sock(g_wsc_ie_data->udpFdCom, wsc_ie_read_callback,
                 		 g_wsc_ie_data, NULL);
	/* Send a start packet */
	strcpy(sendBuf, "PORT");
	to.sin_addr.s_addr = inet_addr(WSC_WLAN_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = htons(WSC_WLAN_UDP_PORT);

	if (udp_write(g_wsc_ie_data->udpFdCom, sendBuf, 5, &to) < 5) {
		wpa_printf(MSG_ERROR, "WSC_IE: Sending Port message to "
				"upper Layer failed");
		return -1;
	} else {
		wpa_printf(MSG_DEBUG, "WSC_IE: send port = %d", WSC_WLAN_UDP_PORT );
	}
	return 0;
}

int wsc_ie_deinit(struct hostapd_data *hapd)
{
#ifdef ACTION_TEC
	if (g_wsc_ie_data == NULL)
		return 0;
#endif
	if (g_wsc_ie_data->udpFdCom != -1) {
		eloop_unregister_read_sock(g_wsc_ie_data->udpFdCom);
		udp_close(g_wsc_ie_data->udpFdCom);
		g_wsc_ie_data->udpFdCom = -1;
	}

	g_wsc_ie_data->hapd = NULL;

	free(g_wsc_ie_data);
	g_wsc_ie_data = NULL;

	return 0;
}
