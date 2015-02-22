/****************************************************************************
 *
 * rg/pkg/voip/asterisk/jdsp_common.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <stdio.h>
#include "asterisk/jdsp_common.h"
#include "asterisk/channel.h"

static int codec_rtp2ast[][2] = {
	{ JRTP_PAYLOAD_PCMU, AST_FORMAT_ULAW },
	{ JRTP_PAYLOAD_PCMA, AST_FORMAT_ALAW },
	{ JRTP_PAYLOAD_G729, AST_FORMAT_G729A },
	{ JRTP_PAYLOAD_G726_32, AST_FORMAT_G726 },
	{ JRTP_PAYLOAD_G723, AST_FORMAT_G723_1 },
	{ JRTP_PAYLOAD_G722, AST_FORMAT_G722 },
	{ -1, -1 }
};

static char *events[] = {
	[PHONE_KEY_0] = "DTMF key 0",
	[PHONE_KEY_1] = "DTMF key 1",
	[PHONE_KEY_2] = "DTMF key 2",
	[PHONE_KEY_3] = "DTMF key 3",
	[PHONE_KEY_4] = "DTMF key 4",
	[PHONE_KEY_5] = "DTMF key 5",
	[PHONE_KEY_6] = "DTMF key 6",
	[PHONE_KEY_7] = "DTMF key 7",
	[PHONE_KEY_8] = "DTMF key 8",
	[PHONE_KEY_9] = "DTMF key 9",
	[PHONE_KEY_ASTERISK] = "DTMF key *",
	[PHONE_KEY_POUND] = "DTMF key #",

	[PHONE_KEY_HOOK_ON] = "Hook on",
	[PHONE_KEY_HOOK_OFF] = "Hook off",
	[PHONE_KEY_FLASH] = "Flash",
	[PHONE_KEY_FAX_CNG] = "Fax calling tone",
	[PHONE_KEY_FAX_CED] = "Fax answering tone",
	[PHONE_KEY_RING_ON] = "Ring started",
	[PHONE_KEY_RING_OFF] = "Ring stopped",
};

char *jdsp_event2str(phone_event_t *ev)
{
	static char buf[256];
	char *event_name = NULL;

	if (ev->key >= 0 && ev->key < sizeof(events)/sizeof(events[0]))
		event_name = events[ev->key];

	/* Pay attention not all elements of events[] are initialized */
	if (!event_name)
		event_name = "Unknown";

	sprintf(buf, "%s(%d) %s", event_name, ev->key, ev->pressed ? "pressed" :
		"released");

	return buf;
}

char jdsp_key2char(phone_key_t key)
{
	switch (key)
	{
	case PHONE_KEY_ASTERISK:
		return '*';
	case PHONE_KEY_POUND:
		return '#';
	default:
		/* We assume that argument "key" is one of the DTMF tones */
		return key - PHONE_KEY_0 + '0';
	}
}

int jdsp_codec_ast2rtp(int ast_format)
{
	int i;

	for (i = 0; codec_rtp2ast[i][0] != -1; i++ )
	{
		if (codec_rtp2ast[i][1] == ast_format)
			return codec_rtp2ast[i][0];
	}
	ast_log(LOG_ERROR, "Unknown AST format %d\n", ast_format);
	/* In the error case return default value (PCMU) */
	return 0;
}

int jdsp_codec_rtp2ast(unsigned char *rtpheader)
{
	int i;
	int payloadtype;

	payloadtype = (ntohl(*(unsigned int *)rtpheader) & 0x7f0000) >> 16;
	for (i = 0; codec_rtp2ast[i][0] != -1; i++ )
	{
		if (codec_rtp2ast[i][0] == payloadtype)
			return codec_rtp2ast[i][1];
	}
	/* This is not really error case. Received RTP packet can contain non voice
	 * data */
	ast_log(LOG_DEBUG, "Unknown RTP payload type %d\n", payloadtype);
	return -1;
}

