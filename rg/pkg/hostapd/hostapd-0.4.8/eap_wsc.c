/*
 * hostapd / Wi-Fi Simple Configuration 7C Proposal
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hostapd.h"
#include "common.h"
#include "eloop.h"
#include "eap_i.h"
#include "eap_wsc.h"
#include "UdpLib.h"

#define WSC_EAP_UDP_PORT            37000
#define WSC_EAP_UDP_ADDR            "127.0.0.1"
#define WSC_EVENT_PORT              38100
#define WSC_EVENT_ADDR              "127.0.0.1"
#define WSC_EVENT_RX_BUF_SIZE       40
#define WSC_EVENT_ACK_STRING        "WSC_EVENT_ACK"

int wsc_event_notify(char * pDataSend)
{
    static int initialized=0;    
    static int event_sock=0;    
    struct sockaddr_in to;
    struct sockaddr_in from;
    char tmpBuffer[WSC_EVENT_RX_BUF_SIZE];
    int recvBytes;
    
    if(initialized==0){
        event_sock = udp_open();
        initialized ++;
    }
    
    memset(&to,0,sizeof(to));
    to.sin_addr.s_addr = inet_addr(WSC_EVENT_ADDR);
    to.sin_family = AF_INET;
    to.sin_port = htons(WSC_EVENT_PORT);
    
    if (udp_write(event_sock, pDataSend, strlen(pDataSend), &to) < strlen(pDataSend))
    {
        wpa_printf(MSG_ERROR, "Hostapd: Sending WSC event to "
                "upper Layer failed");
        return 1;
    }

    recvBytes = udp_read_timed(event_sock, (char *) tmpBuffer, 
            WSC_EVENT_RX_BUF_SIZE, &from, 15);  

    if (recvBytes == -1)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: Event messageACK timeout failure\n");
        return 1;
    }

    if(strncmp(tmpBuffer,WSC_EVENT_ACK_STRING, strlen(WSC_EVENT_ACK_STRING))!=0)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: wrong WSC_EVENT ACK message \n");
        return 1;
    }

    return 0;
}


static u8 * eap_wsc_com_buildReq(struct eap_sm *sm, struct eap_wsc_data *data,
                                int id, size_t *reqDataLen)
{
    u8 *req = NULL;
    int recvBytes;
    struct sockaddr_in from;
    struct sockaddr_in to;
    WSC_NOTIFY_DATA notifyData;
	WSC_NOTIFY_DATA * recvNotify;

    notifyData.type = WSC_NOTIFY_TYPE_BUILDREQ;
    notifyData.u.bldReq.id = id;
    notifyData.u.bldReq.state = data->state;
    notifyData.length = 0;
   
    to.sin_addr.s_addr = inet_addr(WSC_EAP_UDP_ADDR);
    to.sin_family = AF_INET;
    to.sin_port = htons(WSC_EAP_UDP_PORT);
    memcpy(notifyData.sta_mac_addr, sm->sta->addr, ETH_ALEN);

    if (udp_write(data->udpFdEap, (char *) &notifyData, 
			sizeof(WSC_NOTIFY_DATA), &to) < sizeof(WSC_NOTIFY_DATA))
    {
        wpa_printf(MSG_INFO, "EAP-WSC: Sending Eap message to "
                "upper Layer failed\n");
        data->state = FAILURE;
        return NULL;
    }

    recvBytes = udp_read_timed(data->udpFdEap, (char *) data->recvBuf, 
            WSC_RECVBUF_SIZE, &from, 15);   /* Jerry timeout value */

    if (recvBytes == -1)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: req Reading EAP message "
                "from upper layer failed\n");
        data->state = FAILURE;
        return NULL;
    }

	recvNotify = (WSC_NOTIFY_DATA *) data->recvBuf;

	if ( (recvNotify->type != WSC_NOTIFY_TYPE_BUILDREQ_RESULT) ||
	     (recvNotify->length == 0) ||
		 (recvNotify->u.bldReqResult.result != WSC_NOTIFY_RESULT_SUCCESS) )
	{
		wpa_printf(MSG_INFO, "EAP-WSC: Build Request failed "
				"soemwhere\n");
		data->state = FAILURE;
		return NULL;
	}

    req = (u8 *) malloc(recvNotify->length);
    if ( ! req)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: Memory allocation "
                "for the request failed\n");
        data->state = FAILURE;
        return NULL;
    }

    memcpy(req, recvNotify + 1, recvNotify->length);
    *reqDataLen = recvNotify->length;

    data->state = CONTINUE;

    sm->eapol_cb->set_eap_respTimeout(sm->eapol_ctx,15);
	
    return req;
}


static int eap_wsc_com_process(struct eap_sm *sm, struct eap_wsc_data *data,
                        u8 * respData, unsigned int respDataLen)
{
    int recvBytes;
    struct sockaddr_in from;
    struct sockaddr_in to;
    u8 * sendBuf;
    u32 sendBufLen;
    WSC_NOTIFY_DATA notifyData;
    WSC_NOTIFY_DATA * recvNotify;

    notifyData.type = WSC_NOTIFY_TYPE_PROCESS_RESP;
    notifyData.length = respDataLen;
    notifyData.u.process.state = data->state;
    memcpy(notifyData.sta_mac_addr, sm->sta->addr, ETH_ALEN);
   
    sendBuf = (u8 *) malloc(sizeof(WSC_NOTIFY_DATA) + respDataLen);
    if ( ! sendBuf)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: Memory allocation "
                "for the sendBuf failed\n");
        data->state = FAILURE;
	    return -1;
    }

    memcpy(sendBuf, &notifyData, sizeof(WSC_NOTIFY_DATA));
    memcpy(sendBuf + sizeof(WSC_NOTIFY_DATA), respData, respDataLen);
    sendBufLen = sizeof(WSC_NOTIFY_DATA) + respDataLen;

    to.sin_addr.s_addr = inet_addr(WSC_EAP_UDP_ADDR);
    to.sin_family = AF_INET;
    to.sin_port = htons(WSC_EAP_UDP_PORT);

    if (udp_write(data->udpFdEap, (char *) sendBuf, 
			sendBufLen, &to) < sendBufLen)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: com Sending Eap message to "
                "upper Layer failed\n");
        data->state = FAILURE;
		free(sendBuf);
        return -1;
    }

	free(sendBuf);

    recvBytes = udp_read_timed(data->udpFdEap, (char *) data->recvBuf, 
            WSC_RECVBUF_SIZE, &from, 15);  /* Jerry timeout value change*/

    if (recvBytes == -1)
    {
        wpa_printf(MSG_INFO, "EAP-WSC: com Reading EAP message "
                "from upper layer failed\n");
        data->state = FAILURE;
        return -1;
    }

	recvNotify = (WSC_NOTIFY_DATA *) data->recvBuf;
	/* printf("type = %d, length = %d, result = %d\n",
		recvNotify->type,
		recvNotify->length,
		recvNotify->u.processResult.result);*/
	if ( (recvNotify->type != WSC_NOTIFY_TYPE_PROCESS_RESULT) ||
		 (recvNotify->u.processResult.result != WSC_NOTIFY_RESULT_SUCCESS) )
	{
		wpa_printf(MSG_DEBUG, "EAP-WSC: Process Message failed "
				"somewhere\n");
		data->state = FAILURE;
		return -1;
	}
	
	data->state = CONTINUE;
	
    return 0;
}

static void * eap_wsc_init(struct eap_sm *sm)
{
    struct eap_wsc_data *data;

    data = malloc(sizeof(*data));
    if (data == NULL)
        return data;
    memset(data, 0, sizeof(*data));
	data->state = START;

    data->udpFdEap = udp_open();

    sm->eap_method_priv = data;

    return data;
}


static void eap_wsc_reset(struct eap_sm *sm, void *priv)
{
    wpa_printf(MSG_DEBUG, "EAP-WSC: Entered eap_wsc_reset");

    struct eap_wsc_data *data = (struct eap_wsc_data *)priv;
    if (data == NULL)
        return;

    if (data->udpFdEap != -1)
    {
        udp_close(data->udpFdEap);
        data->udpFdEap = -1;
    }

    free(data);
}


static u8 * eap_wsc_buildReq(struct eap_sm *sm, void *priv, int id,
                 size_t *reqDataLen)
{
    struct eap_wsc_data *data = priv;

    wpa_printf(MSG_DEBUG, "EAP-WSC: Entered eap_wsc_buildReq");
    switch (data->state) {
    case START:
    case CONTINUE:
        return eap_wsc_com_buildReq(sm, data, id, reqDataLen);
    default:
        wpa_printf(MSG_DEBUG, "EAP-WSC: %s - unexpected state %d",
               __func__, data->state);
        return NULL;
    }
    return NULL;
}


static Boolean eap_wsc_check(struct eap_sm *sm, void *priv,
                 u8 *respData, size_t respDataLen)
{
    struct eap_hdr *resp;
    size_t len;

    wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WSC: Entered eap_wsc_check *#@*#@*#@");
    resp = (struct eap_hdr *) respData;
    if ((respDataLen < sizeof(*resp) + 2) || 
        (len = ntohs(resp->length)) > respDataLen) 
    {
        wpa_printf(MSG_INFO, "EAP-WSC : Invalid frame");
        return TRUE;
    }
    return FALSE;
}


static void eap_wsc_process(struct eap_sm *sm, void *priv,
                u8 *respData, size_t respDataLen)
{
    struct eap_wsc_data *data = priv;
    struct eap_hdr *resp;

    wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WSC: Entered eap_wsc_process *#@*#@*#@");

    resp = (struct eap_hdr *) respData;
    wpa_printf(MSG_DEBUG, "EAP-WSC : Received packet(len=%lu) ",
               (unsigned long) respDataLen);

    if (eap_wsc_com_process(sm, data, respData, respDataLen) < 0) 
    {
        wpa_printf(MSG_DEBUG, "EAP-WSC : WSC  processing failed");
        data->state = FAILURE;
        return;
    }
}


static Boolean eap_wsc_isDone(struct eap_sm *sm, void *priv)
{
    wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WSC: Entered eap_wsc_isDone *#@*#@*#@");
    struct eap_wsc_data *data = priv;
    return data->state == SUCCESS || data->state == FAILURE;
}

#if 0
static u8 * eap_wsc_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
    /*Our EAP method ALWAYS ends in EAP_FAIL, so this function shouldn't be
      called*/
    return NULL;
}
#endif

static Boolean eap_wsc_isSuccess(struct eap_sm *sm, void *priv)
{
    wpa_printf(MSG_DEBUG,"@#*@#*@#*EAP-WSC: Entered eap_wsc_isSuccess *#@*#@*#@");
    struct eap_wsc_data *data = priv;
    return data->state == SUCCESS;
}


const struct eap_method eap_method_wsc =
{
    .method = EAP_TYPE_WSC,
    .name = "WSC",
    .init = eap_wsc_init,
    .reset = eap_wsc_reset,
    .buildReq = eap_wsc_buildReq,
    .check = eap_wsc_check,
    .process = eap_wsc_process,
    .isDone = eap_wsc_isDone,
    .getKey = NULL,
    .isSuccess = eap_wsc_isSuccess,
};

