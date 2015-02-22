// tiny_upnp_wps_device.c -- EAP-WPS UPnP device code
// This has been greatly stripped down from the original file
// (upnp_wps_device.c) by Ted Merrill, Atheros Communications
// in order to eliminate use of the bulky libupnp library etc.
//
// History:
// upnp_wps_device.c is/was a shim layer between wps_opt_upnp.c and
// the libupnp library.
// The layering (by Sony) was well done; only a very minor modification
// to API of upnp_wps_device.c was required.
// libupnp was found to be undesirable because:
// -- It consumed too much code and data space
// -- It uses multiple threads, making debugging more difficult
//      and possibly reducing reliability.
// -- It uses static variables and only supports one instance.
// The shim and libupnp are here replaced by special code written 
// specifically for the needs of hostapd.
// Various shortcuts can and are taken to keep the code size small.
// Generally, execution time is not as crucial.
//
// BUGS:
// -- UPnP requires that we be able to resolve domain names.
// While uncommon, if we have to do it then it will stall the entire
// hostapd program, which is bad.
// This is because we use the standard linux getaddrinfo() function
// which is syncronous.
// An asyncronous solution would be to use the free "ares" library.
// -- Does not have a robust output buffering scheme.  Uses a single
// fixed size output buffer per TCP/HTTP connection, with possible (although
// unlikely) possibility of overflow and likely excessive use of RAM.
// A better solution would be to write the HTTP output as a buffered stream,
// using chunking: (handle header specially, then) generate data with
// a printf-like function into a buffer, catching buffer full condition,
// then send it out surrounded by http chunking.
// -- There is some code that could be separated out into the common
// library to be shared with wpa_supplicant.
// -- Needs renaming with module prefix to avoid polluting the debugger
// namespace and causing possible collisions with other static fncs 
// and structure declarations when using the debugger.
// -- I have not figured out yet why the original Sony implementation
// (and thus I) goto the trouble of storing strings in "vars[WPS_N_VARS]"
// or what the point of upnp_wps_device_send_wlan_event() really is.
// The original Sony code calls UpnpNotify when these are set, but this
// does not seem to result in anything appearing on network.
// Only vars 1, 2 and 3 seem to ever get set...
// -- Just what should be in the first event message sent after subscription
// for the WLANEvent field? If i pass it empty, Vista replies with OK
// but apparently barfs on the message.
// -- The http error code generation is pretty bogus, hopefully noone cares.
//
// Author: Ted Merrill, Atheros Communications, based upon earlier work
// as explained above and below.
//
// Copyright:
// Copyright 2008 Atheros Communications.
//
// The original header (of upnp_wps_device.c) reads:
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

// Portions from Intel libupnp files, e.g. genlib/net/http/httpreadwrite.c
// typical header:
/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
// * Neither name of Intel Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
/////////////////////////////////////////////////////////////////////////////

/*
 * Overview of WPS over UPnP:
 *
 * UPnP is a bloated Microsoft invention to allow devices to discover 
 * each other and control each other. 
 * If you have a crowded network full of such devices...
 * i hate to think about it.
 *
 * In UPnP terminology, a device is either a "device" (a server that
 * provides information about itself and allows itself to be controlled) 
 * or a "control point" (a client that controls "devices") or possibly both.
 * This file implements a UPnP "device".
 *
 * For us, we use use mostly basic UPnP discovery, but the control
 * part of interest is WPS carried via UPnP messages.
 * There is quite a bit of basic UPnP discovery to do before we can get
 * to WPS, however.
 *
 * UPnP discovery begins with "devices" send out multicast UDP packets
 * to a certain fixed multicast IP address and port, and "control points"
 * sending out other such UDP packets.
 * The packets sent by devices are NOTIFY packets (not to be confused
 * with TCP NOTIFY packets that are used later) and those sent by
 * control points are M-SEARCH packets.
 * These packets contain a simple HTTP style header.
 * The packets are sent redundantly to get around packet loss.
 * Devices respond to M-SEARCH packets with HTTP-like UPD packets 
 * containing HTTP/1.1 200 OK messages, which give similar information
 * as the UDP NOTIFY packets.
 *
 * The above UDP packets advertise the (arbitrary) TCP ports that
 * the respective parties will listen to.
 * The control point can then do a HTTP SUBSCRIBE (something like 
 * an HTTP PUT) after which the device can do a separate HTTP NOTIFY
 * (also like an HTTP PUT) to do event messaging.
 * The control point will also do HTTP GET of the "device file" listed
 * in the original UDP information from the device 
 * (see UPNP_WPS_DEVICE_XML_FILE below), and based on this will do
 * additional GETs... HTTP POSTs are done to cause an action.
 *
 * Beyond some basic information in HTTP headers, additional information
 * is in the HTTP bodies, in a format set by the SOAP and XML standards...
 * a markup language related to HTML used for web pages.
 * This language is intended to provide the ultimate in self-documentation
 * by providing a univeral namespace based on psuedo-URLs called URIs.
 * Note that although a URI looks like a URL (a web address), they
 * are never accessed as such but are used only as identifiers.
 *
 * The POST of a GetDeviceInfo gets information similar to what might
 * be obtained from a probe request or response on WiFi.
 * WPS messages M1-M8 are passed via a POST of a PutMessage;
 * the M1-M8 WPS messages are converted to a bin64 ascii representation
 * for enscapsulation.
 *
 * This of course glosses over a lot of details.
 *
 */

/* ENTIRE FILE IS COMPILED ONLY IF WPS_OPT_TINYUPNP IS PREDEFINED */
#ifdef WPS_OPT_TINYUPNP

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "upnp_wps_device.h"
#include "base64.h"
#include "eloop.h"
#include "httpread.h"
#include "eap_i.h"

/***************************************************************************
 * Configurable constants
 *
 **************************************************************************/
/* UPnP allows a client ("control point") to send a server like us ("device")
 * a domain name for registration, and we are supposed to resolve it.
 * This is bad because, using the standard Linux library, we will stall 
 * the entire hostapd waiting for resolution.
 * The "correct" solution would be to use an event driven library for
 * domain name resolution such as "ares"... however this would increase
 * code size further.
 * Since it is unlikely that we'll actually see such domain names,
 * we can just refuse to accept them...
 */
#define NO_DOMAIN_NAME_RESOLUTION 1  /* 1 to allow only dotted ip addresses */

#define UPNP_CACHE_SEC (UPNP_CACHE_SEC_MIN+1)   /* cache time we use */

#define UPNP_SUBSCRIBE_SEC (UPNP_CACHE_SEC_MIN+1)   /* subscribe time we use*/

/* "filenames" used in URLs that we service via our "web server": */
#define UPNP_WPS_DEVICE_XML_FILE "wps_device.xml" 
#define UPNP_WPS_SCPD_XML_FILE   "wps_scpd.xml"
#define UPNP_WPS_DEVICE_CONTROL_FILE "wps_control"
#define UPNP_WPS_DEVICE_EVENT_FILE "wps_event"

#define UPNP_ADVERTISE_REPEAT 2 /* no more than 3 */
#define MULTICAST_MAX_READ 1600 /* max bytes we'll read for UPD request */
#define MAX_MSEARCH 20          /* max simultaneous M-SEARCH replies ongoing*/

#define WEB_CONNECTION_TIMEOUT_SEC 30   /* Drop web connection after t.o. */
#define WEB_CONNECTION_MAX_READ 8000    /* Max we'll read for TCP request */
#define MAX_WEB_CONNECTIONS 10          /* max simultaneous web connects */
#define WEB_PAGE_BUFFER_SIZE 10000   /* buffer allocation size for web page */
#define WEB_PAGE_BUFFER_MARGIN 2000      /* warn if less than this much free*/

/* UPnP doesn't scale well.  If we were in a room with thousands of 
 * "control points" then potentially we would be "required" to handle
 * subscriptions for each of them, which would exhaust our memory.
 * So we must set a limit.
 * In practice we are unlikely to see more than one or two...
 */
#define MAX_SUBSCRIPTIONS 4    /* how many subscribing clients we handle */
#define MAX_ADDR_PER_SUBSCRIPTION 8

#define MAX_EVENTS_QUEUED 20    /* how far behind queued events an get */
#define EVENT_TIMEOUT_SEC 30   /* Drop sending event after t.o. */

/* How long to wait before sending event */
#define EVENT_DELAY_SECONDS 0
#define EVENT_DELAY_MSEC 0

/***************************************************************************
 * Non-configurable constants
 *
 **************************************************************************/
#define UUID_SIZE 16    /* per definition of uuid */
#define UPNP_CACHE_SEC_MIN 1800 /* min cachable time per UPnP standard */
#define UPNP_SUBSCRIBE_SEC_MIN 1800 /* min subscribe time per UPnP standard */
#define SSDP_TARGET  "239.0.0.0"
#define SSDP_NETMASK "255.0.0.0"
#define UPNP_MULTICAST_ADDRESS  "239.255.255.250"  /* for UPnP multicasting */
#define UPNP_MULTICAST_PORT 1900        /* UDP port we'll monitor for UPnP */

/***************************************************************************
 * Internal enumerations
 *
 **************************************************************************/
enum advertisement_type_enum {
        ADVERTISE_UP = 0,
        ADVERTISE_DOWN = 1,
        MSEARCH_REPLY = 2
};

/* Following is a list of variables defined for WFA Access Point definition.
 * The numbering is arbitrary; we reserve 0 to indicate null case.
 */
enum upnp_wps_var_enum {
        WPS_VAR_INVALID = 0,        /* not used */
        WPS_VAR_MESSAGE,
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
        WPS_N_VARS              /* keep last */
};

/* The following is list of all of the names of all of the variables.
 * These names are defined in document "WFAWLANConfig:1".
 *
 * MAKE SURE that this agrees with the enumeration order above!
 */
static const char *upnp_wps_var_name[WPS_N_VARS] = {
        "Invalid",      /* not used */
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


/***************************************************************************
 * Internal data structure types
 *
 **************************************************************************/
/* forward struct declarations */
struct advertisement_state_machine;
struct web_connection;
struct wps_event;
struct subscription;
struct upnp_wps_device_sm;

/* 
 * advertisements are broadcast via UDP NOTIFYs, and are also the essence
 * of the reply to UDP M-SEARCH requests.
 * This struct handles both cases.
 *
 * A state machine is needed because a number of variant forms must
 * be sent in separate packets (did i mention UPnP was bloated?)
 * and spread out in time to avoid congestion.
 */
struct advertisement_state_machine {
        struct advertisement_state_machine *next;
        struct advertisement_state_machine *prev;  /* double-linked list */
        struct upnp_wps_device_sm *sm;  /* parent */
        enum advertisement_type_enum type;
        int state;
        int nerrors;
        /* use for M-SEARCH replies: */
        unsigned ip_addr;     /* client IP addr, network byte order */
        unsigned ip_port;     /* client port, native byte order */
};

/* 
 * Incoming web connections are recorded in this struct.
 * A web connection is a TCP connection to us, the server;
 * it is called a "web connection" because we use http and serve
 * data that looks like web pages.
 * State information is need to track the connection until we figure
 * out what they want and what we want to do about it.
 */
struct web_connection {
        struct web_connection *next;
        struct web_connection *prev;      /* double linked list */
        struct upnp_wps_device_sm *sm;          /* parent */
        int sd;         /* socket to read from */
        int sd_registered;        /* nonzero if we must cancel registration */
        // unsigned ip_addr;       /* of client, in host byte order */
        // unsigned ip_port;       /* of client, in native byte order */
        struct httpread *hread; /* state machine for reading socket */
        int n_rcvd_data;        /* how much data read so far */
        int done;               /* internal flag, set when we've finished */
};

/* 
 * An address of a subscriber (who may have multiple addresses).
 * We are supposed to send (via TCP) updates to each subscriber,
 * trying each address for a subscriber until we find one that seems
 * to work.
 */
struct subscr_addr {
        struct subscr_addr *next;
        struct subscr_addr *prev;       /* double linked list */
        struct subscription *s;         /* parent */
        char *domain_and_port;      /* domain and port part of url */
        char *path;             /* "filepath" part of url (from "mem") */
        struct sockaddr_in saddr;       /* address for doing connect */
};

/* 
 * "event" information that we send to each subscriber is remembered
 * in this struct.
 * The event can't be sent by simple UDP, horrors! It has to be sent
 * by a HTTP over TCP transation which requires various states...
 * it may also need to be retried at a different address (if more
 * than one is available).
 *
 * TODO: As an optimization we could share data between subscribers.
 */
struct wps_event {
        struct wps_event *next;
        struct wps_event *prev;         /* double linked list */
        struct subscription *s;         /* parent */
        unsigned subscriber_sequence;   /* which event for this subscription*/
        int retry;                      /* which retry */
        struct subscr_addr *addr;       /* address to connect to */
        void *data;             /* event data to send */
        int data_len;
        /* The following apply while we are sending an event message.
         */
        int sd;            /* -1 or socket descriptor for open connection */
        int sd_registered;        /* nonzero if we must cancel registration */
        struct httpread *hread; /* NULL or open connection for event msg */
};


/* 
 * subscribers to our events are recorded in this struct.
 * This includes a max of one outgoing connection (sending 
 * an "event message") per subscriber.
 * We also have to age out subscribers unless they renew.
 */
struct subscription {
        struct subscription *next;
        struct subscription *prev;      /* double linked list */
        struct upnp_wps_device_sm *sm;          /* parent */
        time_t timeout_time;    /* when to age out the subscription */
        unsigned next_subscriber_sequence;      /* number our messages */
        /* This uuid identifies the subscription and is randomly generated
         * by us and given to the subscriber when the subscription is
         * accepted; and is then included with each event sent to 
         * the subscriber.
         */
        u8 uuid[UUID_SIZE];
        /* Linked list of address alternatives (rotate through on failure)
         */
        struct subscr_addr *addr_list;
        int n_addr;             /* no. in list */
        /* Queued event messages. */
        struct wps_event *event_queue;
        int n_queue;            /* how many are queued */
        struct wps_event *current_event; /* non-NULL if being sent (not in q)*/
};

/* 
 * Our instance data corresponding to one WiFi network interface
 * (multiple might share the same wired network interface!).
 *
 * NOTE that this is known as an opaque struct declaration to users of
 * this file (so don't change the name of the struct unless you
 * change it externally also).
 */
struct upnp_wps_device_sm {
	struct upnp_wps_device_ctx *ctx;  /* callback table */
	void *priv;
	char *root_dir;
	char *desc_url;
        int started;    /* nonzero if we are active */
        char *net_if;           /* network interface we use */
        char *mac_addr_text;    /* mac addr of network i.f. we use */
        u8 mac_addr[6];         /* mac addr of network i.f. we use */
        char *ip_addr_text;     /* IP address of network i.f. we use */
        unsigned ip_addr;       /* IP address of network i.f. we use (host order) */
        int multicast_sd;       /* send multicast messages over this socket */
        int ssdp_sd;            /* receive discovery UPD packets on socket */
        int ssdp_sd_registered; /* nonzero if we must unregister */
        unsigned advertise_count;       /* how many advertisements done*/
        struct advertisement_state_machine advertisement;
        struct advertisement_state_machine *msearch_replies;
        int n_msearch_replies;  /* no. of pending M-SEARCH replies */
        int web_port;           /* our port that others get xml files from */
        int web_sd;             /* socket to listen for web requests */
        int web_sd_registered;    /* nonzero if we must cancel registration */
        struct web_connection *web_connections; /* linked list */
        int n_web_connections;  /* no. of pending web connections */
        /* Note: subscriptions are kept in expiry order */
        struct subscription *subscriptions;     /* linked list */
        int n_subscriptions;    /* no of current subscriptions */
        int event_send_all_queued;  /* if we are scheduled to send events soon */
        /* Configuration parameters 
         * These are sized bigger than limits elsewhere (e.g. config.c)
         * TODO: use #defines to parameterize these.
         *
         * Only the uuid (which has a definite size!) is vital;
         * truncation or even omission of the others is rarely harmful.
         */
	char uuid_string[SIZE_UUID*3/*allow for dashes*/+1];
        char manufacturer[80];
        char model_name[80];
        char model_number[80];
        char serial_number[80];
        char friendly_name[80];
        char manufacturer_url[80];
        char model_description[80];
        char model_url[80];
        char upc_string[80];

        /* "variables" that are reported via upnp messages etc. */
        /* TODO: why do we keep all these around? */
        char *vars[WPS_N_VARS];
};


/***************************************************************************
 * Data dumping for debugging
 *
 **************************************************************************/

static void uuid_format(char *buf, const u8 uuid[UUID_SIZE]);

static void tiny_upnp_dump_sm(struct upnp_wps_device_sm *sm, int level)
{
    struct subscription *s;
    if (!sm)
        return;
    wpa_printf(level, "tiny_upnp_wps DUMP of UPnP/WPS state:");
    s = sm->subscriptions; 
    if (s) do {
        char *addr = "?";
        char uuid_string[80];
        uuid_format(uuid_string, s->uuid);
        if (s->addr_list)
            addr = inet_ntoa(s->addr_list->saddr.sin_addr);
        wpa_printf(level, 
            "tiny_upnp_wps subscription %p: ip=%s uuid=%s neventsq=%d",
            s, addr, uuid_string, s->n_queue);
    } while ((s = s->next) != sm->subscriptions);
    return;
}

/***************************************************************************
 * Utilities
 *
 **************************************************************************/

/* upnp_wps_device_encode_base64 -- encode binary data into base 64
 * representation as required for some upnp traffic.
 * (Extracted from original upnp_wps_device.c).
 */
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


/* upnp_wps_device_decode_base64 -- decode base 64 to binary
 * required for some upnp traffic that we receive
 * (Extracted from original upnp_wps_device.c).
 */
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

		*decoded = base64_decode(data, data_len, decoded_len);
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

/* Format date per RFC (this code extracted from libupnp) */
static void format_date(
        char *buf,              /* must be big enough */
        time_t t                /* 0 to get current time */
        )
{
        const char *weekday_str = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
        const char *month_str = "Jan\0Feb\0Mar\0Apr\0May\0Jun\0"
                "Jul\0Aug\0Sep\0Oct\0Nov\0Dec";

        struct tm *date;

        if (t == 0) t = time(NULL);
        date = gmtime(&t);
        sprintf(buf, "%s, %02d %s %d %02d:%02d:%02d GMT",
                &weekday_str[date->tm_wday * 4], date->tm_mday,
                &month_str[date->tm_mon * 4], date->tm_year + 1900,
                date->tm_hour, date->tm_min, date->tm_sec);
}


/* Check tokens for equality, where tokens consist of letters, digits,
 * underscore and hyphen, and are matched case insensitive.
 */
static int token_eq(char *s1, char *s2)
{
        int c1;
        int c2;
        int end1 = 0;
        int end2 = 0;
        for (;;) {
                c1 = *s1++;
                c2 = *s2++;
                if (isalpha(c1) && isupper(c1)) c1 = tolower(c1);
                if (isalpha(c2) && isupper(c2)) c2 = tolower(c2);
                end1 = !(isalnum(c1) || c1 == '_' || c1 == '-');
                end2 = !(isalnum(c2) || c2 == '_' || c2 == '-');
                if (end1 || end2 || c1 != c2) break;
        }
        return (end1 && end2);  /* reached end of both words? */
}

/* Return length of token (see above for definition of token)
 */
static int token_length(char *s)
{
        char *begin = s;
        for (;;s++) {
                int c = *s;
                int end = !(isalnum(c) || c == '_' || c == '-');
                if (end) break;
        }
        return s-begin;
}

#if 0   /* not needed yet */
/* Check words for equality, where words consist of graphical characters
 * delimited by whitespace
 */
static int word_eq(char *s1, char *s2)
{
        int c1;
        int c2;
        int end1 = 0;
        int end2 = 0;
        for (;;) {
                c1 = *s1++;
                c2 = *s2++;
                if (isalpha(c1) && isupper(c1)) c1 = tolower(c1);
                if (isalpha(c2) && isupper(c2)) c2 = tolower(c2);
                end1 = !(isgraph(c1));
                end2 = !(isgraph(c2));
                if (end1 || end2 || c1 != c2) break;
        }
        return (end1 && end2);  /* reached end of both words? */
}
#endif

#if 0   /* not needed yet */
/* Return length of word (see above for definition of word)
 */
static int word_length(char *s)
{
        char *begin = s;
        for (;;s++) {
                int c = *s;
                int end = !(isgraph(c));
                if (end) break;
        }
        return s-begin;
}
#endif

/* return length of interword separation.
 * This accepts only spaces/tabs and thus will not traverse a line 
 * or buffer ending.
 */
static int word_separation_length(char *s)
{
        char *begin = s;
        for (;;s++) {
                int c = *s;
                if (c == ' ' || c == '\t') continue;
                break;
        }
        return s-begin;
}


/* No. of chars through (including) end of line 
 */
static int line_length(char *l)
{
        char * lp = l;
        while (*lp && *lp != '\n') lp++;
        if (*lp == '\n') lp++;
        return lp - l;
}

/* No. of chars excluding trailing whitespace
 */
static int line_length_stripped(char *l)
{
        char * lp = l + line_length(l);;
        while (lp > l && !isgraph(lp[-1])) lp--;
        return lp - l;
}


/* convert hex to binary 
 * Requires that c have been previously tested true with isxdigit().
 */
static int hex_value(int c)
{
        if (isdigit(c)) return c - '0';
        if (islower(c)) return 10 + c - 'a';
        return 10 + c - 'A';
}


/***************************************************************************
 * UUIDs (unique identifiers)
 *
 * These are supposed to be unique in all the world.
 * Sometimes permanent ones are used, sometimes temporary ones
 * based on random numbers... there are different rules for valid content
 * of different types.
 * Each uuid is 16 bytes long.
 **************************************************************************/

/* uuid_make -- construct a random uuid 
 * The UPnP documents don't seem to offer any guidelines as to which
 * method to use for constructing uuids for subscriptions... 
 * presumeably any method from rfc4122 is good enough;
 * i've chosen random number method.
 */
static void uuid_make(u8 uuid[UUID_SIZE])
{
        struct timeval tv;
        unsigned seed;
        int i;

        /* Add new entropy to random number generator each time
         * (assuming that the current time will do the job).
         */
        gettimeofday(&tv, NULL);
        seed = tv.tv_sec ^ tv.tv_usec ^ random();
        srandom(seed);

        for (i = 0; i < UUID_SIZE; i++) {
                uuid[i] = random();
        }

        /* Replace certain bits as specified in rfc4122 or X.667
         */
        uuid[6] &= 0x0f; uuid[6] |= (4 << 4);   /* version 4 == random gen */
        uuid[8] &= 0x3f; uuid[8] |= 0x80;
        return;
}

/* 
 * convert text representation to binary uuid 
 * Conventional representations use lower case hex digits separated by
 * hyphens as:  8-4-4-4-12   (where no.s are count of consecutive hex digits)
 * but upper case letters are allowed as well, and the hyphens may be optional.
 *
 * Returns nonzero on error.
 */
static int uuid_parse(u8 uuid[UUID_SIZE], const char *in)
{
        int i = 0;
        while (i < UUID_SIZE) {
                if (*in == '-') in++;
                if (isxdigit(in[0]) && isxdigit(in[1])) {;}
                else return 1;
                uuid[i++] = (hex_value(in[0]) << 4) | hex_value(in[1]);
                in += 2;
        }
        return 0;
}

/*
 * convert binary to hex representation.
 * buffer must be big enough.
 */
static void uuid_format(char *buf, const u8 uuid[UUID_SIZE])
{
        int i;
        for (i = 0; i < UUID_SIZE; i++) {
                if (i == 4 || i == 6 || i == 8 || i == 10) {
                    *buf++ = '-';
                }
                sprintf(buf, "%02x", uuid[i]);
                buf += 2;
        }
        return;
}


/***************************************************************************
 * XML parsing and formatting
 *
 * XML is a markup language based on unicode; usually (and in our case,
 * always!) based on utf-8.
 * utf-8 uses a variable number of bytes per character.
 * utf-8 has the advantage that all non-ascii unicode characters are
 * represented by sequences of non-ascii (high bit set) bytes,
 * whereas ascii characters are single ascii bytes,
 * thus we can use typical text processing.
 * (One other interesting thing about utf-8 is that it is possible to
 * look at any random byte and determine if it is the first byte of a
 * character as versus a continuation byte).
 * The base syntax of XML uses a few ascii punctionation characters;
 * any characters that would appear in the payload data are rewritten
 * using sequences e.g. &amp; for ampersand(&) and &lt for left angle
 * bracket (<)... five such escapes total (more can be defined but
 * that doesn't apply to our case).
 * Thus we can safely parse for angle brackets etc.
 *
 * XML describes tree structures of tagged data, with each element
 * beginning with an opening tag <label> and ending with a closing
 * tag </label> with matching label.
 * (There is also a self-closing tag <label/> which is supposed
 * to be equivalent to <label></label> i.e. no payload... but we
 * are unlikely to see it for our purpose).
 * Actually the opening tags are a little more complicated because they
 * can contain "attributes" after the label (delimited by ascii space or
 * tab chars) of the form attribute_label="value" or attribute_label='value';
 * as it turns out we don't have to read any of these attributes, just
 * ignore them. 
 * Labels are any sequence of chars other than space, tab, right angle
 * bracket (and ?), but may have an inner structure of 
 * <namespace><colon><plain_label> ... as it turns out, we can ignore
 * the namespaces, in fact we can ignore the entire tree hierarchy,
 * because the plain labels we are looking for will be unique
 * (not in general, but for this application).
 * We do however have to be careful to skip over the namespaces.
 *
 * In generating XML we have to be more careful, but that is easy
 * because everything we do is pretty canned... the only real care to
 * take is to escape any special chars in our payload.
 *
 **************************************************************************/

/* 
 * Advance to next tag, where a tag has form:
 *              <left angle bracket><...><right angle bracket>
 * Within the angle brackets, there is an optional leading forward slash
 * (which makes the tag an ending tag),
 * then an optional leading label (followed by colon) and then
 * the tag name itself.
 *
 * Note that angle brackets present in the original data must have
 * been encoded as &lt;  and &gt; 
 * ... so they won't trouble us.
 */
static int xml_next_tag(
        char *in,       /* input */
        char **out,     /* OUT: start of tag just after '<' */
        char **out_tagname,     /* OUT: start of name of tag, skipping namespace */
        char **end)     /* OUT: one after tag */
{
        while (*in && *in != '<') in++;
        if (*in != '<') return 1;
        *out = ++in;
        if (*in == '/') in++;
        *out_tagname = in;      /* maybe */
        while (isalnum(*in) || *in == '-') in++;
        if (*in == ':') *out_tagname = ++in;
        while (*in && *in != '>') in++;
        if (*in != '>') return 1;
        *end = ++in;
        return 0;
}

/* A POST body looks something like (per upnp spec):
 * <?xml version="1.0"?>
 * <s:Envelope
 *     xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
 *     s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
 *   <s:Body>
 *     <u:actionName xmlns:u="urn:schemas-upnp-org:service:serviceType:v">
 *       <argumentName>in arg value</argumentName>
 *       other in args and their values go here, if any
 *     </u:actionName>
 *   </s:Body>
 * </s:Envelope>
 *
 * where :
 *      s: might be some other namespace name followed by colon
 *      u: might be some other namespace name followed by colon
 *      actionName will be replaced according to action requested
 *      schema following actionName will be WFA scheme instead
 *      argumentName will be actual argument name
 *      (in arg value) will be actual argument value
 *
 * The prototype for upnp_get_first_document_item is from the original
 * upnp_wps_device.c file, and code that uses it has likewise been
 * copied from that original file.
 */

static int
upnp_get_first_document_item(char *doc, const char *item, char **value)
{
        const char *match = item;
        int match_len = strlen(item);
        char *tag;
        char *tagname;
        char *end;

        *value = NULL;          /* default, bad */

        /* This is crude: ignore any possible tag name conflicts and
         * go right to the first tag of this name.
         * This should be ok for the limited domain of UPnP messages.
         */
        for (;;) {
                if (xml_next_tag(doc, &tag, &tagname, &end)) return 1;
                doc = end;
                if (!strncasecmp(tagname, match, match_len) &&
                                *tag != '/' &&
                                (tagname[match_len] == '>' || 
                                !isgraph(tagname[match_len]) )) {
                        break;
                } 
        }
        end = doc;
        while (*end && *end != '<') end++;
        *value = os_zalloc(1+(end-doc));
        if (*value == NULL) return 1;
        memcpy(*value, doc, (end-doc));
        return 0;
}


/* xml_data_encode -- format data for xml file, escaping special characters.
 * Returns location of ending null char.
 *
 * Note that we assume we are using utf8 both as input and as output!
 * In utf8, characters may be classed as follows:
 *      0xxxxxxx(2) -- 1 byte ascii char
 *      11xxxxxx(2) -- 1st byte of multi-byte char w/ unicode value >= 0x80
 *              110xxxxx(2) -- 1st byte of 2 byte sequence (5 payload bits here)
 *              1110xxxx(2) -- 1st byte of 3 byte sequence (4 payload bits here)
 *              11110xxx(2) -- 1st byte of 4 byte sequence (3 payload bits here)
 *      10xxxxxx(2) -- extension byte (6 payload bits per byte)
 *      Some values implied by the above are however illegal because they
 *      do not represent unicode chars or are not the shortest encoding.
 * Actually, we can almost entirely ignore the above and just do 
 * text processing same as for ascii text.
 *
 * XML is written with arbitrary unicode characters, except that five
 * characters have special meaning and so must be escaped where they
 * appear in payload data... which we do here.
 */
char *xml_data_encode(
        char *b,                /* buffer to write to */
        const char *data,             /* data to encode */
        int len
        )
{
        int i;
        for (i = 0; i < len; i++) {
                unsigned c = ((u8 *)data)[i];
                if (c == '<')  {
                        strcpy(b, "&lt;");
                        b += 4;
                        continue;
                } 
                if (c == '>')  {
                        strcpy(b, "&gt;");
                        b += 4;
                        continue;
                } 
                if (c == '&')  {
                        strcpy(b, "&amp;");
                        b += 5;
                        continue;
                } 
                if (c == '\'')  {
                        strcpy(b, "&apos;");
                        b += 6;
                        continue;
                } 
                if (c == '"')  {
                        strcpy(b, "&quot;");
                        b += 6;
                        continue;
                } 
                /* We could try to represent control characters using
                 * the sequence:  &#x;   
                 * where x is replaced by a hex numeral,
                 * but not clear why we would do this...
                 */
                *b++ = c;
        }
        *b = 0; /* null terminate */
        return b;
}

/* xml_data_decode -- deformat data from xml file, unescaping special characters.
 * Returns location of ending null char.
 */
char *xml_data_decode(
        char *b,                /* buffer to write to */
        char *data,             /* data to decode */
        int len
        )
{
        int i;
        for (i = 0; i < len; i++) {
                unsigned c = ((u8 *)data)[i];
                if (c == '&') {
                        /* Note: other names can be defined by DTD but
                         * we ignore that possibility.
                         */
                        if (strncmp(data+i, "&lt;", 4) == 0) {
                                *b++ = '<'; i += 4;
                                continue;
                        }
                        if (strncmp(data+i, "&gt;", 4) == 0) {
                                *b++ = '>'; i += 4;
                                continue;
                        }
                        if (strncmp(data+i, "&amp;", 5) == 0) {
                                *b++ = '&'; i += 5;
                                continue;
                        }
                        if (strncmp(data+i, "&apos;", 6) == 0) {
                                *b++ = '\''; i += 6;
                                continue;
                        }
                        if (strncmp(data+i, "&quot;", 6) == 0) {
                                *b++ = '"'; i += 6;
                                continue;
                        }
                }
                *b++ = c;
        }
        *b = 0; /* null terminate */
        return b;
}

/* xml_add_tagged_data -- format tagged data as a new xml line.
 * Returns location of ending null char.
 *
 * tag must not have any special chars.
 * data may have special chars, which are escaped.
 */
char *xml_add_tagged_data(
        char *b,                /* buffer to write to */
        const char *indent,           /* string of whitespace for indention */
        const char *tag,
        const char *data
        )
{
        sprintf(b, "%s<%s>", indent, tag);
        b += strlen(b);
        b = xml_data_encode(b, data, strlen(data));
        sprintf(b, "</%s>\n", tag);
        b += strlen(b);
        return b;
}

 
/***************************************************************************
 * Advertisements.
 * These are multicast to the world to tell them we are here.
 * The individual packets are spread out in time to limit loss,
 * and then after a much longer period of time the whole sequence 
 * is repeated again (for NOTIFYs only).
 *
 **************************************************************************/

/* next_advertisement -- build next message and advance the state machine.
 * Don't forget to free the returned malloced memory.
 *
 * Note: next_advertisement is shared code with msearchreply_* functions
 */
static char *next_advertisement(
            struct advertisement_state_machine *a,
            int *islast         /* OUT: 1 if this is last message */
            )
{
        char *msg;
        char *m;
        time_t curr_time;
        char *NTString = "";
        char *uuid_string;
        struct tm *date;

        do {
                *islast = 0;
                uuid_string = a->sm->uuid_string;
                msg = os_zalloc(800);   /* more than big enough */
                if (msg == NULL) break;
                m = msg;
                switch (a->type) {
                        case ADVERTISE_UP:
                        case ADVERTISE_DOWN:
                                NTString = "NT";
                                strcpy(m, "NOTIFY * HTTP/1.1\r\n");
                                m += strlen(m);
                                sprintf(m, "HOST: %s:%d\r\n",
                                        UPNP_MULTICAST_ADDRESS,
                                        UPNP_MULTICAST_PORT);
                                m += strlen(m);
                                sprintf(m, "CACHE-CONTROL: max-age=%d\r\n", 
                                        UPNP_CACHE_SEC);
                                m += strlen(m);
                                sprintf(m, "NTS: %s\r\n",
                                        (a->type == ADVERTISE_UP ?
                                                "ssdp:alive" : "ssdp:byebye"));
                                m += strlen(m);
                        break;
                        case MSEARCH_REPLY:
                                NTString = "ST";
                                strcpy(m, "HTTP/1.1 200 OK\r\n");
                                m += strlen(m);
                                sprintf(m, "CACHE-CONTROL: max-age=%d\r\n", 
                                        UPNP_CACHE_SEC);
                                m += strlen(m);
                                curr_time = time(NULL);
                                date = gmtime(&curr_time);

                                strcpy(m, "DATE: ");
                                m += strlen(m);
                                format_date(m, 0);
                                m += strlen(m);
                                strcpy(m, "\r\n");
                                m += strlen(m);
                                
                                strcpy(m, "EXT:\r\n");
                                m += strlen(m);
                        break;
                        goto fail;
                }
                /* Where others may get our xml files from */
                if (a->type != ADVERTISE_DOWN) {
                        sprintf(m, "LOCATION: http://%s:%d/%s\r\n",
                                a->sm->ip_addr_text, 
                                a->sm->web_port,
                                UPNP_WPS_DEVICE_XML_FILE
                                );
                        m += strlen(m);
                }
                /* The SERVER line has three comma-separated fields:
                 *      operating system / version
                 *      upnp version
                 *      software package / version
                 * However, only the UPnP version is really required, the
                 * others can be place holders... for security reasons
                 * it is better to NOT provide extra information.
                 */
                sprintf(m, "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n");
                m += strlen(m);
                switch (a->state/UPNP_ADVERTISE_REPEAT) {
                case 0:
                        sprintf(m, "%s: upnp:rootdevice\r\n", NTString);
                        m += strlen(m);
                        sprintf(m, "USN: uuid:%s::upnp:rootdevice\r\n",
                                uuid_string);
                        m += strlen(m);
                break;
                case 1:
                        sprintf(m, "%s: uuid:%s\r\n", NTString,
                                uuid_string);
                        m += strlen(m);
                        sprintf(m, "USN: uuid:%s\r\n",
                                uuid_string);
                        m += strlen(m);
                break;
                case 2:
                        sprintf(m, 
"%s: urn:schemas-wifialliance-org:device:WFADevice:1\r\n", NTString);
                        m += strlen(m);
                        sprintf(m, 
"USN: uuid:%s::urn:schemas-wifialliance-org:device:WFADevice:1\r\n",
                                uuid_string);
                        m += strlen(m);
                break;
                case 3:
                        sprintf(m, 
"%s: urn:schemas-wifialliance-org:service:WFAWLANConfig:1\r\n", NTString);
                        m += strlen(m);
                        sprintf(m, 
"USN: uuid:%s::urn:schemas-wifialliance-org:service:WFAWLANConfig:1\r\n",
                                uuid_string);
                        m += strlen(m);
                break;
                }
                strcpy(m, "\r\n");
                m += strlen(m);
                if (a->state+1 >= 4*UPNP_ADVERTISE_REPEAT) {
                        *islast = 1;
                }
                return msg;
        } while(0);
        fail:
        if (msg) os_free(msg);
        return NULL;
}

static void advertisement_state_machine_handler(
        void *eloop_data,
        void *user_ctx
        );

static void advertisement_state_machine_stop(
        struct upnp_wps_device_sm *sm
        )
{
        eloop_cancel_timeout(advertisement_state_machine_handler, NULL, sm);
}

static void advertisement_state_machine_handler(
        void *eloop_data,
        void *user_ctx
        )
{
        struct upnp_wps_device_sm *sm = user_ctx;
        struct advertisement_state_machine *a = &sm->advertisement;
        char *msg;
        int next_timeout_msec = 100;
        int next_timeout_sec = 0;
        struct sockaddr_in dest;
        int islast = 0;

        /*
        * Each is sent twice (in case lost) w/ 100 msec delay between;
        * spec says no more than 3 times.
        * One pair for rootdevice, one pair for uuid, and a pair each for
        * each of the two urns.
        * The entire sequence must be repeated before cache control timeout
        * (which  is min  1800 seconds),
        * recommend random portion of half of the advertised cache control age
        * to ensure against loss... perhaps 1800/4 + rand*1800/4 ?
        * Delay random interval < 100 msec prior to initial sending.
        * TTL of 4
        */

        wpa_printf(MSG_MSGDUMP, "upnp_wps_device advertisement state=%d",
                a->state);
        msg = next_advertisement(a, &islast);
        if (msg == NULL) {
                return;
        }
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = inet_addr(UPNP_MULTICAST_ADDRESS);
        dest.sin_port = htons(UPNP_MULTICAST_PORT);

        if (sendto(sm->multicast_sd, msg, strlen(msg), 0, 
                        (struct sockaddr *)&dest, sizeof(dest)) == -1) {
                wpa_printf(MSG_ERROR, "tiny_upnp_wps sendto FAILURE %d (%s)", 
                    errno, strerror(errno));
                next_timeout_msec = 0;
                next_timeout_sec = 10;  /* ... later */
        } else {
                if (islast) {
                        a->state = 0;   /* wrap around */
                        if (a->type == ADVERTISE_DOWN) {
                                wpa_printf(MSG_DEBUG, 
                                        "tiny_upnp_wps did ADVERISE_DOWN");
                                a->type = ADVERTISE_UP;
                                /* do it all over again right away */
                        } else {
                                /* Start over again after a long timeout 
                                 * (see notes above) 
                                 */
                                next_timeout_msec = 0;
                                next_timeout_sec = UPNP_CACHE_SEC/4 + 
                                        (((UPNP_CACHE_SEC/4)*
                                        (random() & 0xFF)) >> 8);
                                ++sm->advertise_count;
                                wpa_printf(MSG_DEBUG, 
                                        "tiny_upnp_wps did ADVERISE_UP (#%u);"
                                        "next in %d sec", 
                                        sm->advertise_count,
                                        next_timeout_sec);
                        }
                } else {
                        a->state++;
                }
        }
        os_free(msg);
        if (eloop_register_timeout(next_timeout_sec, next_timeout_msec,
                advertisement_state_machine_handler, NULL, sm)) {
                wpa_printf(MSG_ERROR, "tiny_upnp_wps eloop_register_timeout FAILED");
                /* No way to recover (from malloc failure) */
                return;
        }
        return;
}


static int advertisement_state_machine_start(
        struct upnp_wps_device_sm *sm
        )
{
        struct advertisement_state_machine *a = &sm->advertisement;
        int next_timeout_msec;

        do {
                advertisement_state_machine_stop(sm);
                /* start out advertising down, this automatically switches
                 * to advertising up which signals our restart.
                 */
                a->type = ADVERTISE_DOWN;
                a->state = 0;
                a->sm = sm;
                /* (other fields not used here) */
                /* First timeout should be random interval < 100 msec */
                next_timeout_msec = (100 * (random() & 0xFF)) >> 8;
                if (eloop_register_timeout(0, next_timeout_msec,
                                advertisement_state_machine_handler, 
                                NULL, sm)) {
                        break;
                }
                return 0;
        }
        while(0);

        return -1;
}


/***************************************************************************
 * M-SEARCH replies
 * These are very similar to the multicast advertisements, with some 
 * small changes in data content; and they are sent (UDP) to a specific
 * address instead of multicast.
 * They are sent in response to a UDP M-SEARCH packet.
 *
 **************************************************************************/

static void msearchreply_state_machine_handler(
        void *eloop_data,
        void *user_ctx
        );

static void msearchreply_state_machine_stop(
        struct advertisement_state_machine *a
        )
{
        struct upnp_wps_device_sm *sm = a->sm;
        wpa_printf(MSG_DEBUG, "upnp_wps_device msearch stop");
        if (a->next == a) {
                sm->msearch_replies = NULL;
        } else  {
                if (sm->msearch_replies == a) {
                        sm->msearch_replies = a->next;
                }
                a->next->prev = a->prev;
                a->prev->next = a->next;
        }
        memset(a, 0, sizeof(*a));       /* aid debugging */
        os_free(a);
        sm->n_msearch_replies--;
        tiny_upnp_dump_sm(sm, MSG_DEBUG);     /* for debugging only */
}

static void msearchreply_state_machine_handler(
        void *eloop_data,
        void *user_ctx
        )
{
        struct advertisement_state_machine *a = user_ctx;
        struct upnp_wps_device_sm *sm = a->sm;
        char *msg;
        int next_timeout_msec = 100;
        int next_timeout_sec = 0;
        struct sockaddr_in dest;
        int islast = 0;

        /*
        * Each is sent twice (in case lost) w/ 100 msec delay between;
        * spec says no more than 3 times.
        * One pair for rootdevice, one pair for uuid, and a pair each for
        * each of the two urns.
        * The entire sequence must be repeated before cache control timeout
        * (which  is min  1800 seconds),
        * recommend random portion of half of the advertised cache control age
        * to ensure against loss... perhaps 1800/4 + rand*1800/4 ?
        * Delay random interval < 100 msec prior to initial sending.
        * TTL of 4
        */

        wpa_printf(MSG_MSGDUMP, "upnp_wps_device msearchreply state=%d",
                a->state);
        msg = next_advertisement(a, &islast);
        if (msg == NULL) {
                wpa_printf(MSG_ERROR, "upnp_wps_device next_advertisement failure");
                return;
        }
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = a->ip_addr;
        dest.sin_port = htons(a->ip_port);

        /* Send it on the multicast socket to avoid having to set up another socket.
         *      This should be entirely legal.
         */
        if (sendto(sm->multicast_sd, msg, strlen(msg), 0, 
                        (struct sockaddr *)&dest, sizeof(dest)) == -1) {
                wpa_printf(MSG_DEBUG, "upnp_wps_device msearchreply sendto errno %d (%s)"
                    " addr 0x%x port %d",
                    errno, strerror(errno), dest.sin_addr.s_addr, a->ip_port);
                /* Ignore error and hope for the best */
        }
        if (islast) {
                /* We are done! */
                wpa_printf(MSG_DEBUG, "upnp_wps_device msearch done");
                msearchreply_state_machine_stop(a);
                return;
        }
        a->state++;

        os_free(msg);
        wpa_printf(MSG_MSGDUMP, "upnp_wps_device msearchreply in %d.%03d sec",
                next_timeout_sec, next_timeout_msec);
        if (eloop_register_timeout(next_timeout_sec, next_timeout_msec,
                msearchreply_state_machine_handler, NULL, a)) {
                /* No way to recover (from malloc failure) */
                wpa_printf(MSG_ERROR, "msearch: eloop_register_timeout failure!");
        }
        return;
}


/* msearchreply_state_machine_start -- reply to M-SEARCH discovery request.
 *
 * Use TTL of 4 (this was done when socket set up).
 * A response should be given in randomized portion of min(MX,120) seconds
 * Note it says:
 * To be found, a device must send a UDP response to the source IP address
 * and port that sent the request to the multicast
 * channel. Devices respond if the ST header of the M-SEARCH request is
 * ssdp:all, upnp:rootdevice, (or) uuid: followed by a UUID
 * that exactly matches one advertised by the device.
 *
 */
static void msearchreply_state_machine_start(
        struct upnp_wps_device_sm *sm,
        unsigned ip_addr,     /* client IP addr, network byte order */
        unsigned ip_port,     /* client port, host byte order */
        int mx                /* maximum delay in seconds */
        )
{
        struct advertisement_state_machine *a;
        int next_timeout_sec;
        int next_timeout_msec;

        wpa_printf(MSG_DEBUG, "upnp_wps_device msearchreply start %d outstanding",
                sm->n_msearch_replies);
        if (sm->n_msearch_replies >= MAX_MSEARCH) {
                wpa_printf(MSG_ERROR, "upnp_wps_device: too many outstanding M-SEARCH replies");
                return;
        }

        do {
                a = os_zalloc(sizeof(*a));
                if (a == NULL) {
                        return;
                }
                a->type = MSEARCH_REPLY;
                a->state = 0;
                a->sm = sm;
                a->ip_addr = ip_addr;
                a->ip_port = ip_port;
                /* Wait a time depending on MX value */
                next_timeout_msec = (1000 * mx * (random() & 0xFF)) >> 8;
                next_timeout_sec = next_timeout_msec / 1000;
                next_timeout_msec = next_timeout_msec % 1000;
                if (eloop_register_timeout(next_timeout_sec, next_timeout_msec,
                        msearchreply_state_machine_handler, NULL, a)) {
                        /* No way to recover (from malloc failure) */
                        break;
                }
                /* Remember for future cleanup */
                if (sm->msearch_replies) {
                        a->next = sm->msearch_replies;
                        a->prev = a->next->prev;
                        a->prev->next = a;
                        a->next->prev = a;
                } else {
                        sm->msearch_replies = a->next = a->prev = a;
                }
                sm->n_msearch_replies++;
                return;
        } while(0);

        wpa_printf(MSG_ERROR, "upnp_wps_device msearchreply failure!");
        eloop_cancel_timeout(msearchreply_state_machine_handler, NULL, a);
        os_free(a);
        return;
}


/* Given that we have received a header w/ M-SEARCH, act upon it
 *
 * Format of M-SEARCH (case insensitive!):
 *
 * First line must be:
 *      M-SEARCH * HTTP/1.1
 * Other lines in arbitrary order:
 *      HOST:239.255.255.250:1900
 *      ST:<varies -- must match>
 *      MAN:"ssdp:discover"
 *      MX:<varies>
 *
 * when microsoft vista is still learning it's ip address 
 * it sends out host lines like:
 *      HOST:[FF02::C]:1900
 * 
 *
 */
static void ssdp_parse_msearch(
        struct upnp_wps_device_sm *sm,
        unsigned ip_addr,     /* client IP addr, network byte order */
        unsigned ip_port,     /* client port, host byte order */
        char *data      /*null terminated*/
        )
{
        char *match;
        char *end;
        int got_host = 0;
        int got_st = 0;
        int got_man = 0;
        int got_mx = 0;
        int mx = 0;

        do {
                /* Skip first line M-SEARCH * HTTP/1.1
                * (perhaps we should check remainder of line for syntax)
                */
                data += line_length(data);
                for (; *data != 0; data += line_length(data)) {
                        end = data + line_length_stripped(data);
                        if (token_eq(data, "host")) {
                                /* The host line indicates who the packet 
                                 * is addressed to... but do we really care?
                                 * Note that microsoft sometimes does funny
                                 * stuff with the HOST: line .
                                */
                                #if 0   /* could be */
                                data += token_length(data);
                                data += word_separation_length(data);
                                if (*data != ':') goto bad;
                                data++;
                                data += word_separation_length(data);
                                match = "239.255.255.250"; /* UPNP_MULTICAST_ADDRESS */
                                if (strncmp(data, match, strlen(match)))
                                        goto bad;
                                data += strlen(match);
                                if (*data == ':') {
                                        match = ":1900";
                                        if (strncmp(data, match, strlen(match)))
                                                goto bad;
                                        data += strlen(match);
                                }
                                #endif  /* could be */
                                got_host = 1;
                                continue;
                        }
                        if (token_eq(data, "st")) {
                                /* There are a number of forms; we look
                                 * for one that matches our case.
                                 */
                                data += token_length(data);
                                data += word_separation_length(data);
                                if (*data != ':') continue;
                                data++;
                                data += word_separation_length(data);
                                match = "ssdp:all";
                                if (!strncmp(data, match, strlen(match))) {
                                        got_st = 1;
                                        continue;
                                }
                                match = "upnp:rootdevice";
                                if (!strncmp(data, match, strlen(match))) {
                                        got_st = 1;
                                        continue;
                                }
                                match = "uuid:";
                                if (!strncmp(data, match, strlen(match))) {
                                        data += strlen(match);
                                        match = sm->uuid_string;
                                        if (!strncmp(data, match, strlen(match))) {
                                                got_st = 1;
                                        }
                                        continue;
                                }
                                match = 
"urn:schemas-upnp-org:device:InternetGatewayDevice:1";
                                if (!strncmp(data, match, strlen(match))) {
                                        got_st = 1;
                                        continue;
                                }
                                match = 
"urn:schemas-wifialliance-org:service:WFAWLANConfig:1";
                                if (!strncmp(data, match, strlen(match))) {
                                        got_st = 1;
                                        continue;
                                }
                                match = 
"urn:schemas-wifialliance-org:device:WFADevice:1";
                                if (!strncmp(data, match, strlen(match))) {
                                        got_st = 1;
                                        continue;
                                }
                                continue;
                        }
                        if (token_eq(data, "man")) {
                                data += token_length(data);
                                data += word_separation_length(data);
                                if (*data != ':') continue;
                                data++;
                                data += word_separation_length(data);
                                match = "\"ssdp:discover\"";
                                if (strncmp(data, match, strlen(match)))
                                        goto bad;
                                got_man = 1;
                                continue;
                        }
                        if (token_eq(data, "mx")) {
                                data += token_length(data);
                                data += word_separation_length(data);
                                if (*data != ':') continue;
                                data++;
                                data += word_separation_length(data);
                                mx = atol(data);
                                got_mx = 1;
                                continue;
                        }
                        /* ignore anything else */
                }
                if (got_host && got_st && got_man && got_mx && mx > 0) {;}
                else goto bad;
                if (mx > 120) mx = 120;   /* per spec */
                msearchreply_state_machine_start(sm, 
                        ip_addr, ip_port, mx);
                return;
        }
        while(0);

        bad:
        wpa_printf(MSG_INFO, "upnp_wps_device M-SEARCH parse failure");
        return;
}



/***************************************************************************
 * Listening for (UDP) discovery (M-SEARCH) packets
 *
 **************************************************************************/

static void ssdp_listener_stop(struct upnp_wps_device_sm *sm)
{
        if (sm->ssdp_sd_registered) {
                eloop_unregister_sock(sm->ssdp_sd, EVENT_TYPE_READ);
                sm->ssdp_sd_registered = 0;
        }
        if (sm->ssdp_sd != -1) {
                close(sm->ssdp_sd);
                sm->ssdp_sd = -1;
        }
}

static void ssdp_listener_handler(int sd, void *eloop_ctx, void *sock_ctx)
{
        struct upnp_wps_device_sm *sm = sock_ctx;
        struct sockaddr_in addr;        /* client address */
        socklen_t addr_len;
        unsigned ip_addr;       /* client address, network byte order */
        unsigned ip_port;       /* client port, host byte order */
        int nread;
        char buf[MULTICAST_MAX_READ];

        addr_len = sizeof(addr);
        nread = recvfrom(sm->ssdp_sd, buf, sizeof(buf)-1, 0,
                (struct sockaddr *)&addr, &addr_len);
        if (nread <= 0) {
                return;
        }
        buf[nread] = 0;         /* need null termination for algorithm */
        ip_addr = addr.sin_addr.s_addr;
        ip_port = ntohs(addr.sin_port);

        wpa_printf(MSG_MSGDUMP, "upnp_wps_device got ssdp packet:\n%s\n", buf);

        /* Parse first line */
        if (0 == strncasecmp(buf, "M-SEARCH", strlen("M-SEARCH")) &&
                    !isgraph(buf[strlen("M-SEARCH")])) {
                ssdp_parse_msearch(sm, ip_addr, ip_port, buf);
                return;
        }

        /* Ignore anything else */

        return;
}


/* Set up for receiving discovery (UDP) packets 
 */
static int ssdp_listener_start(struct upnp_wps_device_sm *sm)
{
        int sd = -1;
        struct sockaddr_in addr;
        struct ip_mreq mcast_addr;
        int onOff = 1;
        unsigned char ttl = 4; /* per upnp spec, keep IP packet time to live (TTL) small */

        do {
                sm->ssdp_sd = sd = socket(AF_INET, SOCK_DGRAM, 0);
                if (sd < 0) {
                        break;
                }
                if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0) {
                        break;
                }
                if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
                                &onOff, sizeof(onOff))) {
                        break;
                }
                os_memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = htonl(INADDR_ANY);
                addr.sin_port = htons(UPNP_MULTICAST_PORT);
                if (bind(sd, (struct sockaddr *)&addr, sizeof(addr))) {
                        break;
                }
                os_memset(&mcast_addr, 0, sizeof(mcast_addr));
                mcast_addr.imr_interface.s_addr = htonl(INADDR_ANY);
                mcast_addr.imr_multiaddr.s_addr = inet_addr(UPNP_MULTICAST_ADDRESS);
                if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                (char *)&mcast_addr, sizeof(mcast_addr))) {
                        break;
                }
                if (setsockopt( sd, IPPROTO_IP, IP_MULTICAST_TTL,
                                &ttl, sizeof(ttl))) {
                        break;
                }
                if (eloop_register_sock(sd, EVENT_TYPE_READ,
                                ssdp_listener_handler, NULL, sm)) {
                        break;
                }
                sm->ssdp_sd_registered = 1;
                return 0;
        } while(0);

        /* Error */
        wpa_printf(MSG_ERROR, "upnp_wps_device ssdp_listener_start failure!");
        ssdp_listener_stop(sm);
        return -1;
}


/***************************************************************************
 * "Files" that we serve via http.
 * Of course, it would be silly to read these from disk since they
 * are pretty canned.
 * The format of these files is given by WFA specifications.
 *
 **************************************************************************/

/* format_wps_device_xml -- produce content of "file" wps_device.xml
 * (UPNP_WPS_DEVICE_XML_FILE)
 *
 * If you make this larger, take a look at WEB_PAGE_BUFFER_SIZE !
 */
void format_wps_device_xml(
        struct upnp_wps_device_sm *sm,
        char *b                 /* buffer, must be big enough */
        )
{
        const char *s;

        /* Based upon sample wps_device.xml file from Sony */
        strcpy(b, "<?xml version=\"1.0\"?>\n");
        b += strlen(b);
        strcpy(b, "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n");
        b += strlen(b);
        strcpy(b, "  <specVersion>\n");
        b += strlen(b);
        strcpy(b, "    <major>1</major>\n");
        b += strlen(b);
        strcpy(b, "    <minor>0</minor>\n");
        b += strlen(b);
        strcpy(b, "  </specVersion>\n");
        b += strlen(b);
        strcpy(b, "  <device>\n");
        b += strlen(b);
        strcpy(b, "    <deviceType>urn:schemas-wifialliance-org:device:WFADevice:1</deviceType>\n");
        b += strlen(b);

        s = sm->friendly_name; s = ((s && *s) ? s : "WPS Access Point");
        b = xml_add_tagged_data(b, "    ", "friendlyName", s);

        s = sm->manufacturer; s = ((s && *s) ? s : "ManufacturerNameHere");
        b = xml_add_tagged_data(b, "    ", "manufacturer", s);

        s = sm->manufacturer_url; s = ((s && *s) ? s : "http://manufacturer.url.here");
        b = xml_add_tagged_data(b, "    ", "manufacturerURL", s);

        s = sm->model_description; s = ((s && *s) ? s : "Model description here");
        b = xml_add_tagged_data(b, "    ", "modelDescription", s);

        s = sm->model_name; s = ((s && *s) ? s : "Model name here");
        b = xml_add_tagged_data(b, "    ", "modelName", s);

        s = sm->model_number; s = ((s && *s) ? s : "Model number here");
        b = xml_add_tagged_data(b, "    ", "modelNumber", s);

        s = sm->model_url; s = ((s && *s) ? s : "http://model.url.here");
        b = xml_add_tagged_data(b, "    ", "modelURL", s);

        s = sm->serial_number; s = ((s && *s) ? s : "Serial number here");
        b = xml_add_tagged_data(b, "    ", "serialNumber", s);

        s = sm->uuid_string;    /* will be something */
        /* Need "uuid:" prefix, thus we can't use xml_add_tagged_data()
         * easily...
         */
        strcpy(b, "    <UDN>uuid:");
        b += strlen(b);
        b = xml_data_encode(b, s, strlen(s));
        strcpy(b, "</UDN>\n");
        b += strlen(b);

        s = sm->upc_string; s = ((s && *s) ? s : "UPC here");
        b = xml_add_tagged_data(b, "    ", "UPC", s);

        strcpy(b, "    <serviceList>\n");
        b += strlen(b);
        strcpy(b, "      <service>\n");
        b += strlen(b);
        strcpy(b, "        <serviceType>urn:schemas-wifialliance-org:service:WFAWLANConfig:1</serviceType>\n");
        b += strlen(b);
        strcpy(b, "        <serviceId>urn:wifialliance-org:serviceId:WFAWLANConfig1</serviceId>\n");
        b += strlen(b);
        strcpy(b, "        <SCPDURL>" UPNP_WPS_SCPD_XML_FILE "</SCPDURL>\n");
        b += strlen(b);
        strcpy(b, "        <controlURL>" UPNP_WPS_DEVICE_CONTROL_FILE "</controlURL>\n");
        b += strlen(b);
        strcpy(b, "        <eventSubURL>" UPNP_WPS_DEVICE_EVENT_FILE "</eventSubURL>\n");
        b += strlen(b);
        strcpy(b, "      </service>\n");
        b += strlen(b);
        strcpy(b, "    </serviceList>\n");
        b += strlen(b);
        strcpy(b, "</device>\n");
        b += strlen(b);
        strcpy(b, "</root>\n");
        b += strlen(b);
        return;
}


/* The following file "wps_scpd.xml" from Sony:
 *
 * If you make this larger, take a look at WEB_PAGE_BUFFER_SIZE !
 */
const char wps_scpd_xml[] =
"<?xml version=\"1.0\"?>\n"
"<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\n"
"  <specVersion>\n"
"    <major>1</major>\n"
"    <minor>0</minor>\n"
"  </specVersion>\n"
"  <actionList>\n"
"    <action>\n"
"      <name>GetDeviceInfo</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewDeviceInfo</name>\n"
"          <direction>out</direction>\n"
"          <relatedStateVariable>DeviceInfo</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>PutMessage</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewInMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>InMessage</relatedStateVariable>\n"
"        </argument>\n"
"        <argument>\n"
"          <name>NewOutMessage</name>\n"
"          <direction>out</direction>\n"
"          <relatedStateVariable>OutMessage</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>GetAPSettings</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>Message</relatedStateVariable>\n"
"        </argument>\n"
"        <argument>\n"
"          <name>NewAPSettings</name>\n"
"          <direction>out</direction>\n"
"          <relatedStateVariable>APSettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>SetAPSettings</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>APSettings</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>APSettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>DelAPSettings</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewAPSettings</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>APSettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>GetSTASettings</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>Message</relatedStateVariable>\n"
"        </argument>\n"
"        <argument>\n"
"          <name>NewSTASettings</name>\n"
"          <direction>out</direction>\n"
"          <relatedStateVariable>STASettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>SetSTASettings</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewSTASettings</name>\n"
"          <direction>out</direction>\n"
"          <relatedStateVariable>STASettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>DelSTASettings</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewSTASettings</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>STASettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>PutWLANResponse</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>Message</relatedStateVariable>\n"
"        </argument>\n"
"        <argument>\n"
"          <name>NewWLANEventType</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>WLANEventType</relatedStateVariable>\n"
"        </argument>\n"
"        <argument>\n"
"          <name>NewWLANEventMAC</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>WLANEventMAC</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>SetSelectedRegistrar</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>Message</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>RebootAP</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"        <name>NewAPSettings</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>APSettings</relatedStateVariable>\n"
"          </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>ResetAP</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>Message</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>RebootSTA</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewSTASettings</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>APSettings</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"    <action>\n"
"      <name>ResetSTA</name>\n"
"      <argumentList>\n"
"        <argument>\n"
"          <name>NewMessage</name>\n"
"          <direction>in</direction>\n"
"          <relatedStateVariable>Message</relatedStateVariable>\n"
"        </argument>\n"
"      </argumentList>\n"
"    </action>\n"
"  </actionList>\n"
"  <serviceStateTable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>Message</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>InMessage</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>OutMessage</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>DeviceInfo</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>APSettings</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"yes\">\n"
"      <name>APStatus</name>\n"
"      <dataType>ui1</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>STASettings</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"yes\">\n"
"      <name>STAStatus</name>\n"
"      <dataType>ui1</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"yes\">\n"
"      <name>WLANEvent</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>WLANEventType</name>\n"
"      <dataType>ui1</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>WLANEventMAC</name>\n"
"      <dataType>string</dataType>\n"
"    </stateVariable>\n"
"    <stateVariable sendEvents=\"no\">\n"
"      <name>WLANResponse</name>\n"
"      <dataType>bin.base64</dataType>\n"
"    </stateVariable>\n"
"  </serviceStateTable>\n"
"</scpd>\n"
;


/***************************************************************************
 *      Subscriber address handling.
 *      Since a subscriber may have an arbitrary number of addresses,
 *      we have to add a bunch of code to handle them.
 *
 *      Addresses are passed in text, and MAY be domain names instead
 *      of the (usual and expected) dotted ip addresses.
 *      Resolving domain names consumes a lot of resources.
 *      Worse, we are currently using the standard linux getaddrinfo
 *      which will block the entire program until complete or timeout!
 *      The proper solution would be to use the "ares" library or similar
 *      with more state machine steps etc... or just disable domain name
 *      resolution by setting NO_DOMAIN_NAME_RESOLUTION to 1 at top of
 *      this file.
 *
 **************************************************************************/

/* subscr_addr_delete -- delete single unlinked subscriber address
 * (be sure to unlink first if need be)
 */
static void subscr_addr_delete(
        struct subscr_addr *a
        )
{
        /* Note: do NOT free domain_and_port or path because
         * they point to memory within the allocation of "a".
         */
        memset(a, 0, sizeof(*a));       /* ease debugging */
        os_free(a);
        return;
}

/* subscr_addr_unlink -- unlink subscriber address from linked list
 */
static void subscr_addr_unlink(
        struct subscription *s,
        struct subscr_addr *a
        )
{
        struct subscr_addr **listp = &s->addr_list;
        s->n_addr--;
        a->next->prev = a->prev;
        a->prev->next = a->next;
        if (*listp == a) {
                if (a == a->next) { 
                        /* last in queue */
                        *listp = NULL;
                        assert(s->n_addr == 0);
                } else {
                        *listp = a->next;
                }
        }
        return;
}

/* subscr_addr_free_all -- unlink and delete list of subscriber addresses.
 */
static void subscr_addr_free_all(
        struct subscription *s
        )
{
        struct subscr_addr **listp = &s->addr_list;
        struct subscr_addr *a;
        while ((a = *listp) != NULL) {
                subscr_addr_unlink(s, a);
                subscr_addr_delete(a);
        }
        return;
}


/* subscr_addr_link -- add subscriber address to list of addresses
 */
static void subscr_addr_link(
        struct subscription *s,
        struct subscr_addr *a
        )
{
        struct subscr_addr **listp = &s->addr_list;
        s->n_addr++;
        if (*listp == NULL) {
                *listp = a->next = a->prev = a;
        } else {
                a->next = *listp;
                a->prev = (*listp)->prev;
                a->prev->next = a;
                a->next->prev = a;
        }
        return;
}


/* subscr_addr_add_url -- add address(es) for one url to subscription
 */
static void subscr_addr_add_url(
        struct subscription *s,
        char *url               /* null terminated */
        )
{
        int alloc_len;
        char *scratch_mem = NULL;
        char *mem;
        char *domain_and_port;
        char *delim;
        char *path;
        char *domain;
        int port = 80;  /* port to send to (default is port 80) */
        struct addrinfo hints;
        struct addrinfo *result;
        struct addrinfo *rp;
        int rerr;
        struct subscr_addr *a = NULL;

        do {
                /* url MUST begin with http: */
                if (strncasecmp(url, "http://", 7)) {
                        break;
                }
                url += 7;

                /* allocate memory for the extra stuff we need */
                alloc_len = (2*(strlen(url)+1));
                scratch_mem = os_zalloc(alloc_len);
                mem = scratch_mem;
                strcpy(mem, url);
                domain_and_port = mem;
                mem += 1+strlen(mem);
                delim = strchr(domain_and_port, '/');
                if (delim) {
                        *delim++ = 0;   /* null terminate domain and port */
                        path = delim;
                } else {
                        path = domain_and_port+strlen(domain_and_port);
                }
                domain = mem;
                strcpy(domain, domain_and_port);
                delim = strchr(domain, ':');
                if (delim) {
                        *delim++ = 0;   /* null terminate domain */
                        if (isdigit(*delim)) port = atol(delim);
                }

                /* getaddrinfo does the right thing with dotted decimal
                 * notations, or will resolve domain names.
                 * Resolving domain names will unfortunately hang the
                 * entire program until it is resolved or it times out
                 * internal to getaddrinfo; fortunately we think that
                 * the use of actual domain names (vs. dotted decimal
                 * notations) should be uncommon.
                 */
                memset(&hints, 0, sizeof(struct addrinfo));
                hints.ai_family = AF_INET;      /* IPv4 */
                hints.ai_socktype = SOCK_STREAM;
                #if NO_DOMAIN_NAME_RESOLUTION
                /* Suppress domain name resolutions that would halt
                 * the program for periods of time
                 */
                hints.ai_flags = AI_NUMERICHOST;
                #else
                /* Allow domain name resolution.
                 */
                hints.ai_flags = 0;
                #endif
                hints.ai_protocol = 0;          /* Any protocol? */
                rerr = getaddrinfo(domain, NULL/*fill in port ourselves*/,
                        &hints, &result);
                if (rerr) {
                        wpa_printf(MSG_INFO, "Resolve error %d (%s) on: %s",
                                rerr, gai_strerror(rerr), domain);
                        break;
                }
                for (rp = result; rp; rp = rp->ai_next) {
                        /* Limit no. of address to avoid 
                         * denial of service attack 
                         */
                        if (s->n_addr >= MAX_ADDR_PER_SUBSCRIPTION) {
                                wpa_printf(MSG_INFO, 
"subscr_addr_add_url: Ignoring excessive addresses");
                                break;
                        }

                        a = os_zalloc(sizeof(*a)+alloc_len);
                        if (a == NULL) {
                                continue;
                        }
                        a->s = s;
                        mem = (void *)(a+1);
                        a->domain_and_port = mem;
                        strcpy(mem, domain_and_port);
                        mem += 1+strlen(mem);
                        a->path = mem;
                        if (path[0] != '/') {
                                *mem++ = '/';
                        }
                        strcpy(mem, path);
                        mem += 1+strlen(mem);
                        memcpy(&a->saddr, rp->ai_addr, sizeof(a->saddr));
                        a->saddr.sin_port = htons(port);

                        subscr_addr_link(s, a);
                        a = NULL;       /* don't free it below */
                }
        } while (0);

        if (result) freeaddrinfo(result);
        os_free(scratch_mem);
        os_free(a);
}
        

/* subscr_addr_list_create -- create list from urls in string.
 *      Each url is enclosed by angle brackets.
 */
static void subscr_addr_list_create(
        struct subscription *s,
        char *url_list               /* null terminated; we overwrite! */
        )
{
        char *end;
        for (;;) {
                while (*url_list == ' ' || *url_list == '\t') url_list++;
                if (*url_list != '<') {
                        break;
                }
                url_list++;
                end = strchr(url_list, '>');
                if (end == NULL) {
                        break;
                }
                *end++ = 0;
                subscr_addr_add_url(s, url_list);
                url_list = end;
        }
        return;
}

/***************************************************************************
 *      Event message generation (to subscribers)
 *
 *      We make a separate copy for each message for each subscriber.
 *      This memory wasted could be limited (adding code complexity)
 *      by sharing copies, keeping a usage count and freeing when zero.
 *
 *      Sending a message requires using a HTTP over TCP NOTIFY 
 *      (like a PUT) which requires a number of states... bloatware!
 *
 **************************************************************************/

/* forward declarations */
static void event_send_all_later(
        struct upnp_wps_device_sm *sm
        );
static void event_timeout_handler(void *eloop_data, void *user_ctx);
static void subscription_unlink(
        struct subscription *s
        );
static void subscription_destroy(
        struct subscription *s
        );

/* event_clean -- clean sockets etc. of event
 * Leaves data, retry count etc. alone.
 */
static void event_clean(
        struct wps_event *e
        )
{
        if (e->s->current_event == e) {
                eloop_cancel_timeout(event_timeout_handler, NULL, e);
                e->s->current_event = NULL;
        }
        if (e->sd_registered) {
                eloop_unregister_sock(e->sd, EVENT_TYPE_WRITE);
                e->sd_registered = 0;
        }
        if (e->sd != -1) {
                close(e->sd);
                e->sd = -1;
        }
        if (e->hread) httpread_destroy(e->hread);
        e->hread = NULL;
        return;
}


/* event_delete -- delete single unqueued event
 * (be sure to dequeue first if need be)
 */
static void event_delete(
        struct wps_event *e
        )
{
        event_clean(e);
        os_free(e->data);
        e->data = NULL; /* ease debugging */
        os_free(e);
        return;
}


/* event_dequeue -- get next event from the queue 
 * Returns NULL if empty.
 */
static struct wps_event *event_dequeue(
        struct subscription *s
        )
{
        struct wps_event **event_head = &s->event_queue;
        struct wps_event *e = *event_head;
        if (e == NULL) return e;
        e->next->prev = e->prev;
        e->prev->next = e->next;
        if (*event_head == e) {
                if (e == e->next) { 
                        /* last in queue */
                        *event_head = NULL;
                } else {
                        *event_head = e->next;
                }
        }
        s->n_queue--;
        e->next = e->prev = NULL;
        /* but parent "s" is still valid */
        return e;
}


/* event_enqueue_at_end -- add event to end of queue
 */
static void event_enqueue_at_end(
        struct subscription *s,
        struct wps_event *e
        )
{
        struct wps_event **event_head = &s->event_queue;
        if (*event_head == NULL) {
                *event_head = e->next = e->prev = e;
        } else {
                e->next = *event_head;
                e->prev = e->next->prev;
                e->prev->next = e;
                e->next->prev = e;
        }
        s->n_queue++;
        return;
}


/* event_enqueue_at_begin -- add event to begin of queue
 * (appropriate for retrying event only)
 */
static void event_enqueue_at_begin(
        struct subscription *s,
        struct wps_event *e
        )
{
        struct wps_event **event_head = &s->event_queue;
        if (*event_head == NULL) {
                *event_head = e->next = e->prev = e;
        } else {
                e->prev = *event_head;
                e->next = e->prev->next;
                e->prev->next = e;
                e->next->prev = e;
                *event_head = e;
        }
        s->n_queue++;
        return;
}


/* event_delete_all -- delete entire event queue and current event
 */
static void event_delete_all(
        struct subscription *s
        )
{
        struct wps_event *e;
        while ((e = event_dequeue(s)) != NULL) {
                event_delete(e);
        }
        if (s->current_event) {
                event_delete(s->current_event);
                /* will set: s->current_event = NULL;  */
        }
        return;
}


/* event_retry -- called when we had a failure delivering event msg
 */
static void event_retry(
        struct wps_event *e,
        int do_next_address             /* skip address e.g. on connect fail */
        )
{
        struct subscription *s = e->s;
        struct upnp_wps_device_sm *sm = s->sm;

        event_clean(e);
        /* will set: s->current_event = NULL; */

        if (do_next_address) e->retry++;
        if (e->retry >= s->n_addr) {
                /* Just give up */
                wpa_printf(MSG_INFO, "upnp wps device: giving up on sending event");
                return;
        }
        event_enqueue_at_begin(s, e);
        event_send_all_later(sm);
        return;
}

/* called if the overall event-sending process takes too long
 */
static void event_timeout_handler(void *eloop_data, void *user_ctx)
{
        struct wps_event *e = user_ctx;
        struct subscription *s = e->s;

        assert (e == s->current_event);

        wpa_printf(MSG_INFO, "upnp device event send timeout");
        event_retry(e, 1);
        return;
}


/* event_got_response_handler -- called back when http response
 * is received.
 */
static void event_got_response_handler(
        struct httpread *handle, 
        void *cookie, 
        enum httpread_event en
        )
{
        struct wps_event *e = cookie;
        struct subscription *s = e->s;
        struct upnp_wps_device_sm *sm = s->sm;
        struct httpread *hread = e->hread;
        int reply_code = 0;

        assert(e == s->current_event);
        eloop_cancel_timeout(event_timeout_handler, NULL, e);

        if (en == HTTPREAD_EVENT_FILE_READY) {
                if (httpread_hdr_type_get(hread) == HTTPREAD_HDR_TYPE_REPLY) {
                        reply_code = httpread_reply_code_get(hread);
                        if (reply_code == 200) {
                                wpa_printf(MSG_DEBUG, 
                                        "upnp device got event reply OK");
                                event_delete(e);  
                                goto send_more;
                        } else {
                                wpa_printf(MSG_INFO, 
                                        "upnp device got event reply code %d",
                                        reply_code);
                                goto bad;
                        }
                } else {
                        wpa_printf(MSG_INFO, 
                                "upnp device got bogus event response %d", en);
                }
        } else {
                wpa_printf(MSG_INFO, 
                        "upnp device event response timeout/fail");
                goto bad;
        }
        event_retry(e, 1);
        goto send_more;

        send_more:
        /* Schedule sending more if there is more to send */
        if (s->event_queue) event_send_all_later(sm);
        return;

        bad:
        /* If other side doesn't like what we say, forget about them. 
         * (There is no way to tell other side that we are dropping
         * them...).
         * Alternately, we could just do event_delete(e)
         */
        wpa_printf(MSG_INFO, "Deleting subscription due to errors");
        subscription_unlink(s);
        subscription_destroy(s);
        return;
}



/* event_send_tx_ready -- actually write event message
 *
 * Prequisite: subscription socket descriptor has become ready to
 * write (because connection to subscriber has been made).
 *
 * It is also possible that we are called because the connect has failed;
 * it is possible to test for this, or we can just go ahead and then
 * the write will fail.
 */
static void event_send_tx_ready(int sock, void *eloop_ctx, void *sock_ctx)
{
        struct wps_event *e = sock_ctx;
        struct subscription *s = e->s;
        struct upnp_wps_device_sm *sm = s->sm;
        char *buf = NULL;
        char *b;

        assert(e == s->current_event);
        assert(e->sd == sock);

        buf = os_zalloc(WEB_PAGE_BUFFER_SIZE);
        if (buf == NULL) {
                event_retry(e, 0);
                goto bad;
        }
        b = buf;
        sprintf(b, "NOTIFY %s HTTP/1.1\r\n", e->addr->path);
        b += strlen(b);
        strcpy(b, "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n");
        b += strlen(b);
        sprintf(b, "HOST: %s\r\n", e->addr->domain_and_port);
        b += strlen(b);
        strcpy(b, "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
                        "NT: upnp:event\r\n"
                        "NTS: upnp:propchange\r\n");
        b += strlen(b);
        strcpy(b, "SID: uuid:");
        b += strlen(b);
        uuid_format(b, s->uuid);
        b += strlen(b);
        strcpy(b, "\r\n");
        b += strlen(b);
        sprintf(b, "SEQ: %u\r\n", e->subscriber_sequence);
        b += strlen(b);
        sprintf(b, "CONTENT-LENGTH: %d\r\n", e->data_len);
        b += strlen(b);
        strcpy(b, "\r\n");      /* terminating empty line */
        b += strlen(b);
        memcpy(b, e->data, e->data_len);
        b += e->data_len;

        /* Since the message size is pretty small, we should be
         * able to get the operating system to buffer what we give it
         * and not have to come back again later to write more...
         */
        #if 0
        /* we could: Turn blocking back on? */
        fcntl(e->sd, F_SETFL, 0);
        #endif
        if (write(e->sd, buf, (b-buf)) != (b-buf)) {
                wpa_printf(MSG_ERROR, 
                        "upnp_wps_device Failed to send event errno=%d (%s)",
                        errno, strerror(errno));
                event_retry(e, 1);
                goto bad;
        }
        os_free(buf);  buf = NULL;

        if (e->sd_registered) {
                e->sd_registered = 0;
                eloop_unregister_sock(e->sd, EVENT_TYPE_WRITE);
        }
        /* Set up to read the reply */
        e->hread = httpread_create(
                e->sd,
                event_got_response_handler,
                e,      /* cookie */
                0,      /* no data expected! */
                EVENT_TIMEOUT_SEC
                );
        if (e->hread == NULL) {
                wpa_printf(MSG_ERROR, "upnp_wps_device httpread_create failed");
                event_retry(e, 0);
                goto bad;
        }
        return;

        bad:
        /* Schedule sending more if there is more to send */
        if (s->event_queue) event_send_all_later(sm);
        os_free(buf);
        return;
}



/* event_send_start -- prepare to send a event message to subscriber
 *
 * This gets complicated because:
 * -- The message is sent via TCP and we have to keep the stream open
 *      for 30 seconds to get a response... then close it.
 * -- But we might have other event happen in the meantime... 
 *      we have to queue them, if we lose them then the subscriber will
 *      be forced to unsubscribe and subscribe again.
 * -- If multiple URLs are provided then we are supposed to try successive
 *      ones after 30 second timeout.
 * -- The URLs might use domain names instead of dotted decimal addresses,
 *      and resolution of those may cause unwanted sleeping.
 * -- Doing the initial TCP connect can take a while, so we have to come
 *      back after connection and then send the data.
 *
 * Returns nonzero on error;
 *
 * Prerequisite: No current event send (s->current_event == NULL)
 *      and non-empty queue.
 */
static int event_send_start(
        struct subscription *s
        )
{
        struct wps_event *e;
        int itry;

        /* Assume we are called ONLY with no current event and
         * ONLY with nonempty event queue and ONLY with at least
         * one address to send to.
         */
        assert(s->addr_list != NULL);
        assert(s->current_event == NULL);
        assert(s->event_queue != NULL);

        s->current_event = e = event_dequeue(s);

        /* Use address acc. to no. of retries */
        e->addr = s->addr_list;
        for (itry = 0; itry < e->retry; itry++) {
                e->addr = e->addr->next;
        }

        e->sd = socket(AF_INET, SOCK_STREAM, 0);
        if (e->sd < 0) {
                event_retry(e, 0);
                return -1;
        }
        /* set non-blocking so we don't sleep waiting for connection 
         */
        if (fcntl(e->sd, F_SETFL, O_NONBLOCK) != 0) {
                event_retry(e, 0);
                return -1;
        }
        /* start the connect. it might succeed immediately but more
         * likely will return errno EINPROGRESS.
         */
        if (connect(e->sd, 
                        (struct sockaddr *)&e->addr->saddr, 
                        sizeof(e->addr->saddr))) {
                if (errno == EINPROGRESS) {
                } else {
                        event_retry(e, 1);
                        return -1;
                }
        }
        /* Call back when ready for writing (or on failure...).
         */
        if (eloop_register_sock(e->sd, EVENT_TYPE_WRITE,
                        event_send_tx_ready, NULL, e)) {
                event_retry(e, 0);
                return -1;
        }
        e->sd_registered = 1;
        /* Don't wait forever!
         */
        if (eloop_register_timeout(EVENT_TIMEOUT_SEC, 0/*msec*/,
                        event_timeout_handler, NULL, e)) {
                event_retry(e, 0);
                return -1;
        }
        return 0;
}


/* event_send_all_later_handler -- actually send events as needed
 */
static void event_send_all_later_handler(void *eloop_data, void *user_ctx)
{
        struct upnp_wps_device_sm *sm = user_ctx;
        struct subscription *s;
        struct subscription *s_old;
        int nerrors = 0;

        sm->event_send_all_queued = 0;
        if ((s = sm->subscriptions) != NULL)
        do {
                if (s->addr_list == NULL) {
                        /* if we've given up on all addresses */
                        wpa_printf(MSG_INFO,
"upnp wps device: removing subscription w/ no addresses");
                        s_old = s;
                        s = s_old->next;
                        subscription_unlink(s_old);
                        subscription_destroy(s_old);
                } else {
                        if (s->current_event == NULL /* not busy */ &&
                                        s->event_queue != NULL /*more to do*/) {
                                if (event_send_start(s)) {
                                        nerrors++;
                                }
                        }
                        s = s->next;
                }
        } while (sm->subscriptions != NULL && s != sm->subscriptions);

        if (nerrors) {
                /* Try again later */
                event_send_all_later(sm);
        }
        return;
}
        

/* event_send_all_later -- schedule sending events to all subscribers
 * that need it.
 * This avoids two problems:
 * -- After getting a subscription, we should not send the first event
 *      until after our reply is fully queued to be sent back,
 * -- Possible stack depth or infinite recursion issues.
 */
static void event_send_all_later(
        struct upnp_wps_device_sm *sm
        )
{
        tiny_upnp_dump_sm(sm, MSG_DEBUG);     /* for debugging only */

        /* The exact time in the future isn't too important
         * (although Sony code fails if we put it far at all into the
         * future...).
         * Waiting a bit might let us do several together.
         */
        if (sm->event_send_all_queued) return;
        sm->event_send_all_queued = 1;
        if (eloop_register_timeout(
                        EVENT_DELAY_SECONDS, EVENT_DELAY_MSEC,
                        event_send_all_later_handler, NULL, sm)) {
                /* malloc failure, can't recover easily */
                wpa_printf(MSG_ERROR, 
                        "upnp device eloop_register_timeout fail");
        }
        return;
}

/* event_send_stop_all -- cleanup
 */
static void event_send_stop_all(
        struct upnp_wps_device_sm *sm
        )
{
        if (sm->event_send_all_queued) {
                eloop_cancel_timeout(event_send_all_later_handler, NULL, sm);
        }
        sm->event_send_all_queued = 0;
        return;
}

/* event_add -- add a new event to a queue
 */
static int event_add(
        struct subscription *s,
        const void *data,       /* is copied; caller retains ownership */
        int data_len
        )
{
        struct wps_event *e;

        if (s->n_queue >= MAX_EVENTS_QUEUED) {
                wpa_printf(MSG_INFO, 
                "upnp wps device: Too many events queued for subscriber");
                return 1;
        }

        e = os_zalloc(sizeof(*e));
        if (e == NULL) {
                return 1;
        }
        e->s = s;
        e->sd = -1;
        e->data = os_zalloc(data_len+1/*ease debugging*/);
        if (e->data == NULL) {
                os_free(e);
                return 1;
        }
        memcpy(e->data, data, data_len);
        e->data_len = data_len;
        e->subscriber_sequence = s->next_subscriber_sequence++;
        if (s->next_subscriber_sequence == 0) s->next_subscriber_sequence++;
        event_enqueue_at_end(s, e);
        event_send_all_later(s->sm);
        return 0;
}


/* upnp_wps_device_set_var -- remember variable.
 * (Call upnp_wps_device_send_var() to send value of variable as a event).
 */
static void upnp_wps_device_set_var(
        struct upnp_wps_device_sm *sm,
        enum upnp_wps_var_enum evar,    /* which variable */
        char *value                     /* NULL (== empty) or value */
        )
{
        /* Enqueue event message for all subscribers */
        char *var_value;
        const char *var_name;

        if (evar <= 0 || evar >= WPS_N_VARS) {
                wpa_printf(MSG_ERROR, "upnp_wps_device_set_var: invalid evar %d", evar);
                return;
        }
        if (value == NULL) value = "";
        os_free(sm->vars[evar]);
        sm->vars[evar] = var_value = os_strdup(value);
        var_name = upnp_wps_var_name[evar];

        wpa_printf(MSG_DEBUG, "upnp wps device: set var %d (%s) to: `%s'",
                evar, var_name, var_value);
        return;
}



/* upnp_wps_device_send_var -- queue event messages for values of variables.
 * Up to 3 variables can be passed (must be different from each other)
 * and all are passed in a single event message to each subscriber ;
 * a zero evar terminates the list.
 */
static void upnp_wps_device_send_var(
        struct upnp_wps_device_sm *sm,
        enum upnp_wps_var_enum evar0,
        enum upnp_wps_var_enum evar1,
        enum upnp_wps_var_enum evar2
        )
{
        /* Enqueue event message for all subscribers */

        const int nvars = 3;    /* max no. of variables passed */
        char *buf = NULL;      /* holds event message */
        int buf_size = 0;
        char *b;
        int ivar;
        enum upnp_wps_var_enum evar;
        char *var_value = NULL;
        const char *var_name = NULL;
        struct subscription *s;
        const char *format_head = 
                /* actually, utf-8 is default, but it doesn't hurt to specify it */
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">\n";
        const char *format_property =
                "    <e:property>\n"
                /* var name, var value, var name must be pasted in: */
                "        <%s>%s</%s>\n"
                "    </e:property>\n";
        const char *format_tail = 
                "</e:propertyset>\n";

        
        if (sm->subscriptions == NULL) {
                /* optimize */
                return;
        }

        /* Determine buffer size needed first */
        buf_size += strlen(format_head);
        for (ivar = 0; ivar < nvars; ivar++) {
                switch (ivar) {
                        case 0: evar = evar0; break;
                        case 1: evar = evar1; break;
                        case 2: evar = evar2; break;
                        default:
                        return;
                }
                if (evar <= 0 || evar >= WPS_N_VARS) {
                        /* 0 terminates list */
                        break;
                }
                var_name = upnp_wps_var_name[evar];
                var_value = sm->vars[evar];
                if (var_value == NULL) var_value = "";
                buf_size += strlen(format_property) +
                    2 * strlen(var_name) +
                    strlen(var_value);
        }
        buf_size += strlen(format_tail);
        buf_size++;     /* add term. null */

        buf = os_malloc(buf_size);
        if (buf == NULL) {
                return;
        }
        b = buf;
        strcpy(b, format_head);
        b += strlen(b);
        for (ivar = 0; ivar < nvars; ivar++) {
                switch (ivar) {
                        case 0: evar = evar0; break;
                        case 1: evar = evar1; break;
                        case 2: evar = evar2; break;
                        default:
                        return;
                }
                if (evar <= 0 || evar >= WPS_N_VARS) {
                        /* 0 terminates list */
                        break;
                }
                var_name = upnp_wps_var_name[evar];
                var_value = sm->vars[evar];
                if (var_value == NULL) var_value = "";
                sprintf(b, format_property, var_name, var_value, var_name);
                b += strlen(b);
        }
        strcpy(b, format_tail);
        b += strlen(b);

        s = sm->subscriptions;
        do {
                if (event_add(s, buf, (b-buf))) {
                        wpa_printf(MSG_ERROR, 
"upnp wps device: dropping subscriber due to event backlog");
                        struct subscription *s_old = s;
                        s = s_old->next;
                        subscription_unlink(s_old);
                        subscription_destroy(s_old);
                } else {
                        s = s->next;
                }
        } while (s != sm->subscriptions);

        os_free(buf);

        return;
}




/***************************************************************************
 * Event subscription (subscriber machines register with us to
 * receive event messages).
 * This is the result of an incoming HTTP over TCP SUBSCRIBE request.
 *
 **************************************************************************/

/* subscription_unlink -- remove from the active list
 */
static void subscription_unlink(
        struct subscription *s
        )
{
        struct upnp_wps_device_sm *sm = s->sm;

        if (s->next == s) {
                /* only one? */
                sm->subscriptions = NULL;
        } else  {
                if (sm->subscriptions == s) {
                        sm->subscriptions = s->next;
                }
                s->next->prev = s->prev;
                s->prev->next = s->next;
        }
        sm->n_subscriptions--;
        return;
}

/* subscription_link_to_end -- link to end of active list
 * (should have high expiry time!)
 */
static void subscription_link_to_end(
        struct subscription *s
        )
{
        struct upnp_wps_device_sm *sm = s->sm;

        if (sm->subscriptions) {
                s->next = sm->subscriptions;
                s->prev = s->next->prev;
                s->prev->next = s;
                s->next->prev = s;
        } else {
                sm->subscriptions = s->next = s->prev = s;
        }
        sm->n_subscriptions++;
        return;
}


/* subscription_destroy -- destroy an unlinked subscription
 * Be sure to unlink first if necessary.
 */
static void subscription_destroy(
        struct subscription *s
        )
{
        wpa_printf(MSG_INFO, "upnp wps device: destroy subscription %p", s);
        if (s->addr_list) {
                subscr_addr_free_all(s);
        }
        event_delete_all(s);
        memset(s, 0, sizeof(*s));       /* aid debugging */
        os_free(s);
        return;
}


/* subscription_list_age -- remove expired subscriptions
 */
static void subscription_list_age(
        struct upnp_wps_device_sm *sm, 
        time_t now
        )
{
        struct subscription *s;
        while ((s = sm->subscriptions) != NULL && s->timeout_time < now) {
                wpa_printf(MSG_INFO, "upnp wps device: removing aged subscription");
                subscription_unlink(s);
                subscription_destroy(s);
        }
        return;
}

/* subscription_find -- return existing subscription matching uuid, if any
 * returns NULL if not found
 */
static struct subscription *subscription_find(
        struct upnp_wps_device_sm *sm, 
        u8 uuid[UUID_SIZE]
        )
{
        struct subscription *s0 = sm->subscriptions;
        struct subscription *s = s0;

        if (s0 == NULL) return NULL;
        do {
                int i;
                for (i = 0; i < UUID_SIZE; i++) {
                        if (s->uuid[i] != uuid[i]) goto do_next;
                }
                /* Found match */
                return s;
                do_next:
                s = s->next;
        } while (s != s0);
        return NULL;
}


/* subscription_first_event -- send format/queue event that is automatically
 * sent on a new subscription.
 */
int subscription_first_event(
        struct subscription *s
        )
{
        const char *format1 = 
                /* actually, utf-8 is default, but it doesn't hurt to specify it */
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">\n"
                /* Why do we report STAStatus if we are not a STA?
                 * I don't know, but that is what is done.
                 */
                "    <e:property>\n"
                "        <STAStatus>1</STAStatus>\n"
                "    </e:property>\n"
                /* APStatus is apparently a bit set,
                 *              0x1 = configuration change (but is always set?)
                 *              0x10 = ap is locked
                 */
                "    <e:property>\n"
                "        <APStatus>%d</APStatus>\n"
                "    </e:property>\n"
                /* Per spec, we send out the last WLANEvent, whatever it 
                 * was.... !
                 */
                "    <e:property>\n"
                "        <WLANEvent>%s</WLANEvent>\n"
                "    </e:property>\n"
                "</e:propertyset>\n";
        const char *format2 = 
                /* actually, utf-8 is default, but it doesn't hurt to specify it */
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">\n"
                /* Why do we report STAStatus if we are not a STA?
                 * I don't know, but that is what is done.
                 */
                "    <e:property>\n"
                "        <STAStatus>1</STAStatus>\n"
                "    </e:property>\n"
                /* APStatus is apparently a bit set,
                 *              0x1 = configuration change (but is always set?)
                 *              0x10 = ap is locked
                 */
                "    <e:property>\n"
                "        <APStatus>%d</APStatus>\n"
                "    </e:property>\n"
                /* Omit WLANEvent for this format */
                "</e:propertyset>\n";
        char *wlan_event;
        char *buf;
        int buf_size;
        int ap_status = 1;      /* TODO: add 0x10 if access point is locked */

        /* this is pretty bogus, but it is what the spec says */
        wlan_event = s->sm->vars[WPS_VAR_WLANEVENT];
        if (wlan_event == NULL || *wlan_event == 0) {
                wpa_printf(MSG_ERROR, 
                        "upnp wps device: ERROR: WLANEvent not known yet");
                wlan_event = "";
        }
        buf_size = strlen(format1) + 10 + strlen(wlan_event) + 1;
        buf = os_malloc(buf_size);
        if (buf == NULL) {
                return 1;
        }
        /* Omit WLANEvent if we don't have one */
        if (wlan_event && *wlan_event)
            sprintf(buf, format1, ap_status, wlan_event);
        else
            sprintf(buf, format2, ap_status);
        if (event_add(s, buf, strlen(buf))) {
                os_free(buf);
                return 1;
        }
        os_free(buf);
        return 0;
}


/* subscription_start -- remember a UPnP control point to send events to.
 * Returns NULL on error, or pointer to new subscription structure.
 */
static struct subscription * subscription_start(
        struct upnp_wps_device_sm *sm,
        char *callback_urls      /* malloc' mem given to the subscription */
        )
{
        struct subscription *s = NULL;
        time_t now = time(0);
        time_t expire = now + UPNP_SUBSCRIBE_SEC;

        /* Get rid of expired subscriptions so we have room */
        subscription_list_age(sm, now);

        /* if too many subscriptions, bail */
        if (sm->n_subscriptions >= MAX_SUBSCRIPTIONS) {
                struct subscription *s = sm->subscriptions;
                wpa_printf(MSG_ERROR, 
                        "upnp wps device: TOO MANY SUBSCRIPTIONS, trashing oldest");
                subscription_unlink(s);
                subscription_destroy(s);
        }

        s = os_zalloc(sizeof(*s));
        if (s == NULL) {
                return NULL;
        }
        s->sm = sm;
        s->timeout_time = expire;
        uuid_make(s->uuid);
        subscr_addr_list_create(s, callback_urls);
        /* Add to end of list, since it has the highest
         * expiration time
         */
        subscription_link_to_end(s);
        /* Queue up immediate event message (our last event) 
         * as required by UPnP spec.
         */
        if (subscription_first_event(s)) {
                wpa_printf(MSG_ERROR, 
"upnp wps device: dropping subscriber due to event backlog");
                subscription_unlink(s);
                subscription_destroy(s);
                return NULL;
        }
        wpa_printf(MSG_INFO, "upnp wps device: subscription %p started w/ %s",
                s, callback_urls);
        /* Schedule sending this */
        event_send_all_later(sm);
        return s;
}


/* subscription_renew -- find subscription and reset timeout
 */
static struct subscription *subscription_renew(
        struct upnp_wps_device_sm *sm, 
        u8 uuid[UUID_SIZE]
        )
{
        time_t now = time(0);
        time_t expire = now + UPNP_SUBSCRIBE_SEC;
        struct subscription *s = subscription_find(sm, uuid);
        if (s == NULL) {
                return s;
        }
        wpa_printf(MSG_INFO, "upnp wps device: subscription renewed");
        subscription_unlink(s);
        s->timeout_time = expire;
        /* add back to end of list, since it now has highest expiry */
        subscription_link_to_end(s);
        return s;
}

/***************************************************************************
 * Web connections (we serve pages of info about ourselves, handle
 * requests, etc. etc.).
 *
 **************************************************************************/

static void web_connection_stop(
        struct web_connection *c
        )
{
        struct upnp_wps_device_sm *sm = c->sm;

        httpread_destroy(c->hread);
        c->hread = NULL;
        close(c->sd);
        c->sd = -1;
        if (c->next == c) {
                sm->web_connections = NULL;
        } else  {
                if (sm->web_connections == c) {
                        sm->web_connections = c->next;
                }
                c->next->prev = c->prev;
                c->prev->next = c->next;
        }
        memset(c, 0, sizeof(*c));       /* aid debugging */
        os_free(c);
        sm->n_web_connections--;
        return;
}

/* Given that we have received a header w/ GET, act upon it
 *
 * Format of GET (case-insensitive):
 *
 * First line must be:
 *      GET /<file> HTTP/1.1
 * Since we don't do anything fancy we just ignore other lines.
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Connection: close
 * Content-Type: text/xml
 * Date: <rfc1123-date>
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_get(
        struct web_connection *c,
        char *filename
        )
{
        struct upnp_wps_device_sm *sm = c->sm;
        // unsigned ip_addr = c->ip_addr;
        // unsigned ip_port = c->ip_port;
        char *buf;      /* output buffer, allocated */
        char *b;
        char *put_length_here = NULL;
        char *body_start = NULL;

        buf = os_zalloc(WEB_PAGE_BUFFER_SIZE);
        if (buf == NULL) {
                return;
        }
        b = buf;
        /* Assuming we will be successful, put in the output header
         * first.
         * Note: we do not keep connections alive (and httpread does
         * not support it)... therefore we must have Connection: close.
         */
        strcpy(b, "HTTP/1.1 200 OK\r\n"
                    "Connection: close\r\n"
                    "Content-Type: text/xml; charset=\"utf-8\"\r\n" /*ASSUME xml files only */
                    );
        b += strlen(b);

        strcpy(b, "Date: ");
        b += strlen(b);
        format_date(b, 0/*now*/);
        b += strlen(b);
        strcpy(b, "\r\n");
        b += strlen(b);
        strcpy(b, "Server: Unspecified, UPnP/1.0, Unspecified\r\n");
        b += strlen(b);

        strcpy(b, "Content-Length: ");
        b += strlen(b);
        /* 
         * We'll paste the length in later, leaveing some extra whitespace.
         * http code is supposed to be tolerant of extra whitespace.
         */
        put_length_here = b;    /* put length here later */
        strcpy(b, "        \r\n");      /* spaces to be replaced by length */
        b += strlen(b);

        /* terminating empty line */
        strcpy(b, "\r\n");      
        b += strlen(b);

        body_start = b;

        /* it is not required that filenames be case insensitive but
         * it is allowed and can't hurt.
         */
        if (filename == NULL) filename = "(null)";      /* just in case */
        if (!strcasecmp(filename,  UPNP_WPS_DEVICE_XML_FILE)) {
                format_wps_device_xml(sm, b);
                b += strlen(b);
        } else
        if (!strcasecmp(filename, UPNP_WPS_SCPD_XML_FILE)) {
                strcpy(b, wps_scpd_xml);
                b += strlen(b);
        } else {
                /* File not found */
                wpa_printf(MSG_DEBUG, "upnp_wps_device: file not found: %s",
                        filename);
                b = buf;
                strcpy(b, "HTTP/1.1 404 Not Found\r\n"
                            "Connection: close\r\n");
                b += strlen(b);
                strcpy(b, "Date: ");
                b += strlen(b);
                format_date(b, 0/*now*/);
                b += strlen(b);
                strcpy(b, "\r\n");
                b += strlen(b);
                /* terminating empty line */
                strcpy(b, "\r\n");      
                b += strlen(b);
                put_length_here = NULL;
                body_start = NULL;
        }
        /* Now patch in the content length at the end */
        if (body_start && put_length_here) {
                int body_length = b - body_start;
                char len_buf[10];
                sprintf(len_buf, "%d", body_length);
                memcpy(put_length_here, len_buf, strlen(len_buf));
        }

        /* We use a fixed sized allocation for simplicity, which is a risk.
         * We hope to catch any issues in testing by observing messages.
         */
        wpa_printf(MSG_DEBUG, "upnp_wps_device: used %lu of %d web buffer bytes",
                (b-buf), WEB_PAGE_BUFFER_SIZE);
        if ((WEB_PAGE_BUFFER_SIZE - (b-buf)) < WEB_PAGE_BUFFER_MARGIN) {
                wpa_printf(MSG_ERROR, "upnp_wps_device: INCREASE WEB_PAGE_BUFFER_SIZE!");
        }
        errno = 0;
        if (write(c->sd, buf, (b-buf)) != (b-buf)) {
                wpa_printf(MSG_ERROR, 
                        "upnp_wps_device Failed to send file errno=%d (%s)",
                        errno, strerror(errno));
        }
        os_free(buf);
        return;
}



/* Given that we have received a header w/ POST, act upon it
 *
 * Format of POST (case-insensitive):
 *
 * First line must be:
 *      POST /<file> HTTP/1.1
 * Since we don't do anything fancy we just ignore other lines.
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Connection: close
 * Content-Type: text/xml
 * Date: <rfc1123-date>
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_post(
        struct web_connection *c,
        char *filename
        )
{
        int ret = 500;          /* http reply code */
        struct upnp_wps_device_sm *sm = c->sm;
        /* Temporaries */
        char *match;
        int match_len;
        /* Major parse pointers; don't call free on these */
        char *data = httpread_data_get(c->hread);       /* body of http msg */
        char *action = NULL;            /* the action requested of us */
        int action_len = 0;
        /* since there is never more than one reply argument, we need
         * only remember the one: (leave NULL if no reply args)
         */
        char *replyname = NULL;         /* argument name for the reply */
        char *replydata = NULL;         /* data for the reply */
        /* Allocated intermediate buffers; must be fred at end */
        u8 *raw_msg = NULL;
        size_t raw_msg_len;
        u8 *decoded = NULL;
        size_t decoded_len;
        char *msg = NULL;
        char *in_msg = NULL;
        char *out_msg = NULL;
        size_t out_msg_len;
        char *device_info = NULL;
        size_t device_info_len;
        char *sta_settings = NULL;
        char *ap_settings = NULL;
        size_t ap_settings_len = 0;
        char *type = NULL;
        char *mac = NULL;
        /* Reply variables */
        char *buf;      /* output buffer, allocated */
        char *b;
        char *put_length_here = NULL;
        char *body_start = NULL;


        ret = 401;      /* Invalid Action */
        if (strcasecmp(filename, UPNP_WPS_DEVICE_CONTROL_FILE)) {
                wpa_printf(MSG_INFO, 
                    "tiny_upnp_wps_device invalid post filename %s", filename);
                goto bad;
        }
        /* The SOAPAction line of the header tells us what we want to do
         */
        b = httpread_hdr_line_get(c->hread, "SOAPAction:");
        if (b == NULL) {
                goto bad;
        }
        if (*b == '"') b++;
        else {
                goto bad;
        }
        match = "urn:schemas-wifialliance-org:service:WFAWLANConfig:";
        match_len = strlen(match);
        if (strncasecmp(b, match, match_len)) {
                goto bad;
        }
        b += match_len;
        /* skip over version */
        while (isgraph(*b) && *b != '#') b++;
        if (*b != '#') {
                goto bad;
        }
        b++;
        /* Following the sharp(#) should be the action and a double quote */
        action = b;
        while (isgraph(*b) && *b != '"') b++;
        if (*b != '"') {
                goto bad;
        }
        action_len = b-action;
        /* There are quite a few possible actions. Although we 
         * appear to support them all here, only a few are supported
         * by callbacks at higher levels (unless someone has modfied that).
         */
        if (!strncasecmp("GetDeviceInfo", action, action_len)) {
                wpa_printf(MSG_DEBUG, "upnp wps device: handle GetDeviceInfo");
                if (sm->ctx->received_req_get_device_info(
                                sm->priv, &raw_msg, &raw_msg_len)) {
                        wpa_printf(MSG_INFO, "tiny_upnp GetDeviceInfo failed");
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (upnp_wps_device_encode_base64(
                                raw_msg, raw_msg_len, 
                                &device_info, &device_info_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                replyname = "NewDeviceInfo";
                replydata = device_info;
                upnp_wps_device_set_var(sm, WPS_VAR_DEVICEINFO, device_info);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_DEVICEINFO, 0, 0);
                #endif
                goto good;

        }
        if (!strncasecmp("PutMessage", action, action_len)) {
                /* PutMessage is used by external upnp-based registrar
                 * to perform WPS operation with the access point itself;
                 * as compared with PutWLANResponse which is for proxying.
                 */
                wpa_printf(MSG_INFO, "upnp wps device: handle PutMessage");
                if (upnp_get_first_document_item(
                                data, "NewInMessage", &in_msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(in_msg, os_strlen(in_msg),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_put_message) goto bad;
                if (sm->ctx->received_req_put_message(
                                    sm->priv, decoded, decoded_len,
                                    &raw_msg, &raw_msg_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (upnp_wps_device_encode_base64(
                                raw_msg, raw_msg_len, 
                                &out_msg, &out_msg_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_INMESSAGE, in_msg);
                upnp_wps_device_set_var(sm, WPS_VAR_OUTMESSAGE, out_msg);
                #if 0
                upnp_wps_device_send_var(sm, 
                        WPS_VAR_INMESSAGE, WPS_VAR_OUTMESSAGE, 0);
                #endif
                replydata = out_msg;
                replyname = "NewOutMessage";
                goto good;

        }
        if (!strncasecmp("GetAPSettings", action, action_len)) {
                wpa_printf(MSG_DEBUG, "upnp wps device: handle GetAPSettings");
                if (upnp_get_first_document_item(data, "NewMessage", &msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(msg, os_strlen(msg),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_get_ap_settings) goto bad;
                if (sm->ctx->received_req_get_ap_settings(
                                    sm->priv, decoded, decoded_len,
                                    &raw_msg, &raw_msg_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (upnp_wps_device_encode_base64(
                                raw_msg, raw_msg_len, 
                                &ap_settings, &ap_settings_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
                upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);
                #if 0
                upnp_wps_device_send_var(sm, 
                        WPS_VAR_MESSAGE, WPS_VAR_APSETTINGS, 0);
                #endif
                replyname = "NewAPSettings";
                replydata = ap_settings;
                goto good;
        }
        if (!strncasecmp("SetAPSettings", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle SetAPSettings");
                if (upnp_get_first_document_item(
                                data, "NewAPSettings", &ap_settings)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                ap_settings, os_strlen(ap_settings),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_set_ap_settings) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_set_ap_settings(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_APSETTINGS, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;       /* ? */
                goto good;
        }
        if (!strncasecmp("DelAPSettings", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle DelAPSettings");
                char *ap_settings = NULL;
                if (upnp_get_first_document_item(
                                data, "NewAPSettings", &ap_settings)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                ap_settings, os_strlen(ap_settings),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_del_ap_settings) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_del_ap_settings(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                replyname = NULL;
                replydata = NULL;
                upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_APSETTINGS, 0, 0);
                #endif
                goto good;
        }
        if (!strncasecmp("GetSTASettings", action, action_len)) {
                wpa_printf(MSG_DEBUG, "upnp wps device: handle GetSTASettings");
                if (upnp_get_first_document_item(data, "NewMessage", &msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                msg, os_strlen(msg),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_get_sta_settings) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_get_sta_settings(
                                sm->priv, decoded, decoded_len,
                                &raw_msg, &raw_msg_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
                upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);
                #if 0
                upnp_wps_device_send_var(sm, 
                        WPS_VAR_MESSAGE, WPS_VAR_STASETTINGS, 0);
                #endif
                replyname = "NewSTASettings";
                replydata = sta_settings;
                goto good;
        }
        if (!strncasecmp("SetSTASettings", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle SetSTASettings");
                if (upnp_get_first_document_item(
                                data, "NewSTASettings", &sta_settings)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                sta_settings, os_strlen(sta_settings),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_set_sta_settings) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_set_sta_settings(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_STASETTINGS, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("DelSTASettings", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle DelSTASettings");
                if (upnp_get_first_document_item(
                                data, "NewSTASettings", &sta_settings)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                sta_settings, os_strlen(sta_settings),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_del_sta_settings) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_del_sta_settings(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_STASETTINGS, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("PutWLANResponse", action, action_len)) {
                /* External upnp-based registrar is passing us a message
                 * to be proxied over to a wifi-based client of ours.
                 */
                wpa_printf(MSG_INFO, "upnp wps device: handle PutWLANResponse");
                int ev_type;
                if (upnp_get_first_document_item(data, "NewMessage", &msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                msg, os_strlen(msg), &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (upnp_get_first_document_item(
                                data, "NewWLANEventType", &type)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                ev_type = atol(type);
                if (upnp_get_first_document_item(
                                data, "NewWLANEventMAC", &mac)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (!sm->ctx->received_req_put_wlan_event_response) {
                        wpa_printf(MSG_INFO, 
                            "Fail: !sm->ctx->received_req_put_wlan_event_response");
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_put_wlan_event_response(
                                sm->priv, ev_type,
                                decoded, decoded_len)) {
                        wpa_printf(MSG_INFO,
                            "Fail: sm->ctx->received_req_put_wlan_event_response");
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
                upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTTYPE, type);
                upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTMAC, mac);
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("SetSelectedRegistrar", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle SetSelectedRegistrar");
                if (upnp_get_first_document_item(data, "NewMessage", &msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                msg, os_strlen(msg),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_set_selected_registrar) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_set_selected_registrar(sm->priv,

                                                  decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_MESSAGE, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("RebootAP", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle RebootAP");
                if (upnp_get_first_document_item(
                                data, "NewAPSettings", &ap_settings)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                ap_settings, os_strlen(ap_settings),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_reboot_ap) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_reboot_ap(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_APSETTINGS, ap_settings);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_APSETTINGS, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("ResetAP", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle ResetAP");
                if (upnp_get_first_document_item(data, "NewMessage", &msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                msg, os_strlen(msg),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_reset_ap) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_reset_ap(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_MESSAGE, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("RebootSTA", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle RebootSTA");
                if (upnp_get_first_document_item(
                                data, "NewSTASettings", &sta_settings)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                sta_settings, os_strlen(sta_settings),
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_reboot_sta) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_reboot_sta(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_STASETTINGS, sta_settings);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_STASETTINGS, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        if (!strncasecmp("ResetSTA", action, action_len)) {
                wpa_printf(MSG_INFO, "upnp wps device: handle ResetSTA");
                if (upnp_get_first_document_item(data, "NewMessage", &msg)) {
                        ret = 600;      /* InvalidArg (could be out of mem) */
                        goto bad;
                }
                if (upnp_wps_device_decode_base64(
                                msg, os_strlen(msg), 
                                &decoded, &decoded_len)) {
                        ret = 603;      /* Out of memory */
                        goto bad;
                }
                if (!sm->ctx->received_req_reset_sta) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                if (sm->ctx->received_req_reset_sta(
                                sm->priv, decoded, decoded_len)) {
                        ret = 500;      /* Internal server error */
                        goto bad;
                }
                upnp_wps_device_set_var(sm, WPS_VAR_MESSAGE, msg);
                #if 0
                upnp_wps_device_send_var(sm, WPS_VAR_MESSAGE, 0, 0);
                #endif
                replyname = NULL;
                replydata = NULL;
                goto good;
        }
        wpa_printf(MSG_INFO, "upnp wps device: unknown POST type");
        goto bad;

        bad:
        wpa_printf(MSG_INFO, "upnp wps device: POST failure ret=%d", ret);
        goto make_reply;

        good:
        ret = 200;

        make_reply:
        /* Parameters of the response:
         *      action(action_len) -- action we are responding to
         *      replyname -- a name we need for the reply
         *      replydata -- NULL or null-terminated string
         */
        buf = os_zalloc(WEB_PAGE_BUFFER_SIZE);
        if (buf == NULL) {
                goto very_bad;
        }
        b = buf;
        /* Assuming we will be successful, put in the output header
         * first.
         * Note: we do not keep connections alive (and httpread does
         * not support it)... therefore we must have Connection: close.
         */
        if (ret == 200) {
                strcpy(b, "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/xml; charset=\"utf-8\"\r\n" /*ASSUME xml files only */
                        );
        } else {
                sprintf(b, "HTTP/1.1 %d Error\r\n", ret);
        }
        b += strlen(b);
        strcpy(b, "Connection: close\r\n");
        b += strlen(b);
        strcpy(b, "Content-Length: ");
        b += strlen(b);
        /* 
         * We'll paste the length in later, leaveing some extra whitespace.
         * http code is supposed to be tolerant of extra whitespace.
         */
        put_length_here = b;    /* put length here later */
        strcpy(b, "        \r\n");      /* spaces to be replaced by length */
        b += strlen(b);
        strcpy(b, "Date: ");
        b += strlen(b);
        format_date(b, 0/*now*/);
        b += strlen(b);
        strcpy(b, "\r\n");
        b += strlen(b);

        /* terminating empty line */
        strcpy(b, "\r\n");      
        b += strlen(b);
        body_start = b;

        if (ret == 200 /*OK*/) {
                strcpy(b, 
                        "<?xml version=\"1.0\"?>\n"
                        "<s:Envelope\n"
                        "    xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
                        "    s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                        "  <s:Body>\n");
                b += strlen(b);
                sprintf(b, "    <u:%.*sResponse", action_len, action);
                b += strlen(b);
                strcpy(b, " xmlns:u=\"urn:schemas-wifialliance-org:service:WFAWLANConfig:1\">\n");
                b += strlen(b);
                if (replydata && replyname) {
                        // might possibly need to escape part of reply data? ...
                        // probably not, unlikely to have ampersand(&) or left
                        // angle bracket (<) in it...
                        sprintf(b, "<%s>%s</%s>\n", replyname, replydata, replyname);
                        b += strlen(b);
                }
                sprintf(b, "    </u:%.*sResponse>\n", action_len, action);
                b += strlen(b);
                strcpy(b, "  </s:Body>\n</s:Envelope>\n");
                b += strlen(b);
        } else {
                /* Error case */
                strcpy(b, 
                        "<?xml version=\"1.0\"?>\n"
                        "<s:Envelope\n"
                        "     xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
                        "     s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                        "  <s:Body>\n"
                        "     <s:Fault>\n"
                        "       <faultcode>s:Client</faultcode>\n"
                        "       <faultstring>UPnPError</faultstring>\n"
                        "       <detail>\n"
                        "         <UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\n"
                        );
                b += strlen(b);
                sprintf(b, 
                        "           <errorCode>%d</errorCode>\n", ret);
                b += strlen(b);
                strcpy(b, 
                        "           <errorDescription>Error</errorDescription>\n"
                        "         </UPnPError>\n"
                        "       </detail>\n"
                        "     </s:Fault>\n"
                        "  </s:Body>\n"
                        "</s:Envelope>\n"
                        );
                b += strlen(b);
        }

        /* Now patch in the content length at the end */
        if (body_start && put_length_here) {
                int body_length = b - body_start;
                char len_buf[10];
                sprintf(len_buf, "%d", body_length);
                memcpy(put_length_here, len_buf, strlen(len_buf));
        }
        /* We use a fixed sized allocation for simplicity, which is a risk.
         * We hope to catch any issues in testing by observing messages.
         */
        wpa_printf(MSG_DEBUG, "upnp_wps_device: used %lu of %d web buffer bytes",
                (b-buf), WEB_PAGE_BUFFER_SIZE);
        if ((WEB_PAGE_BUFFER_SIZE - (b-buf)) < WEB_PAGE_BUFFER_MARGIN) {
                wpa_printf(MSG_ERROR, "upnp_wps_device: INCREASE WEB_PAGE_BUFFER_SIZE!");
        }
        errno = 0;
        if (write(c->sd, buf, (b-buf)) != (b-buf)) {
                wpa_printf(MSG_ERROR, 
                        "upnp_wps_device Failed to send msg errno=%d (%s)",
                        errno, strerror(errno));
                goto very_bad;
        }
        goto cleanup;

        very_bad:
        wpa_printf(MSG_ERROR, "upnp wps device: POST: can't reply");
        goto cleanup;

        cleanup:
        os_free(buf);
        os_free(raw_msg);
        os_free(decoded);
        os_free(msg);
        os_free(in_msg);
        os_free(out_msg);
        os_free(device_info);
        os_free(sta_settings);
        os_free(ap_settings);
        os_free(type);
        os_free(mac);

        return;
}




/* Given that we have received a header w/ SUBSCRIBE, act upon it
 *
 * Format of SUBSCRIBE (case-insensitive):
 *
 * First line must be:
 *      SUBSCRIBE /wps_event HTTP/1.1
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Server: xx, UPnP/1.0, xx
 * SID: uuid:xxxxxxxxx
 * Timeout: Second-<n>
 * Content-Length: 0
 * Date: xxxx
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_subscribe(
        struct web_connection *c,
        char *filename
        )
{
        struct upnp_wps_device_sm *sm = c->sm;
        // unsigned ip_addr = c->ip_addr;
        // unsigned ip_port = c->ip_port;
        char *buf;      /* output buffer, allocated */
        char *b;
        char *hdr = httpread_hdr_get(c->hread);
        char *h;
        char *match;
        int match_len;
        char *end;
        int len;
        int got_nt = 0;
        u8 uuid[UUID_SIZE];
        int got_uuid = 0;
        char *callback_urls = NULL;
        struct subscription *s = NULL;

        buf = os_zalloc(WEB_PAGE_BUFFER_SIZE);
        if (buf == NULL) {
                return;
        }
        b = buf;

        /* Parse/validate headers */
        h = hdr;
        /* First line: SUBSCRIBE /wps_event HTTP/1.1
         * has already been parsed.
         */
        if (strcasecmp(filename, UPNP_WPS_DEVICE_EVENT_FILE) != 0) {
                goto error_412;
        }
        end = strchr(h, '\n');

        for (; end != NULL; h=end+1) {
                /* Option line by option line */
                h = end+1;
                end = strchr(h, '\n');
                if (end == NULL) break; /* no unterminated lines allowed */

                /* NT assures that it is our type of subscription;
                 * not used for a renewl.
                 **/
                match = "NT:"; match_len = strlen(match);
                if (strncasecmp(h, match, match_len) == 0) {
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        match = "upnp:event";
                        match_len = strlen(match);
                        if (strncasecmp(h, match, match_len) != 0) 
                                goto error_400;
                        got_nt = 1;
                        continue;
                }
                /* HOST should refer to us */
                #if 0
                match = "HOST:"; match_len = strlen(match);
                if (strncasecmp(h, match, match_len) == 0) {
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        .....
                }
                #endif
                /* CALLBACK gives one or more URLs for NOTIFYs 
                 * to be sent as a result of the subscription.
                 * Each URL is enclosed in angle brackets.
                 */
                match = "CALLBACK:"; match_len = strlen(match);
                if (strncasecmp(h, match, match_len) == 0) {
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        len = end-h;
                        callback_urls = os_malloc(len+1);
                        if (callback_urls == NULL) {
                                goto error_500;
                        }
                        memcpy(callback_urls, h, len);
                        callback_urls[len] = 0;
                        continue;
                }
                /* SID is only for renewal */
                match = "SID:"; match_len = strlen(match);
                if (strncasecmp(h, match, match_len) == 0) {
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        match = "uuid:"; match_len = strlen(match);
                        if (strncasecmp(h, match, match_len) != 0) {
                                goto error_400;
                        }
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        if (uuid_parse(uuid, h)) {
                                goto error_400;
                        }
                        got_uuid = 1;
                        continue;
                }
                /* TIMEOUT is requested timeout, but apparently we can
                 * just ignore this.
                 */
        }

        if (got_uuid) {
                /* renewal */
                if (callback_urls) goto error_400;
                s = subscription_renew(sm, uuid);
                if (s == NULL) goto error_412;
        } else
        if (callback_urls) {
                if (! got_nt) goto error_412;
                s = subscription_start(sm, callback_urls);
                if (s == NULL) goto error_500;
                callback_urls = NULL;   /* is now owned by subscription */
        } else {
                goto error_412;
        }


        /* success 
         */
        strcpy(b, "HTTP/1.1 200 OK\r\n"
                    "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 0\r\n"
                    );
        b += strlen(b);
        strcpy(b, "SID: uuid:");
        b += strlen(b);
        uuid_format(b, s->uuid);        /*subscription id*/
        b += strlen(b);
        strcpy(b, "\r\n");
        b += strlen(b);
        sprintf(b, "Timeout: Second-%d\r\n", UPNP_SUBSCRIBE_SEC);
        b += strlen(b);
        strcpy(b, "Date: ");
        b += strlen(b);
        format_date(b, 0);
        b += strlen(b);
        strcpy(b, "\r\n");
        b += strlen(b);
        /* And empty line to terminate header: */
        strcpy(b, "\r\n");
        b += strlen(b);
        goto send_msg;

        /* Per UPnP spec:
        * Errors
        * Incompatible headers
        *            400 Bad Request. If SID header and one of NT or CALLBACK headers are present, the publisher must respond with HTTP
        *            error 400 Bad Request.
        * Missing or invalid CALLBACK
        *            412 Precondition Failed. If CALLBACK header is missing or does not contain a valid HTTP URL, the publisher must
        *            respond with HTTP error 412 Precondition Failed.
        * Invalid NT
        *            412 Precondition Failed. If NT header does not equal upnp:event, the publisher must respond with HTTP error 412
        *            Precondition Failed.
        * [For resubscription, use 412 if unknown uuid].
        * Unable to accept subscription
        *            5xx. If a publisher is not able to accept a subscription (such as due to insufficient resources), it must respond with a
        *            HTTP 500-series error code.
        */
        error_400:
        strcpy(b, "HTTP/1.1 400 Bad request\r\n"
                "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        goto send_msg;

        error_412:
        strcpy(b, "HTTP/1.1 412 Precondition failed\r\n"
                "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        goto send_msg;

        error_500:
        strcpy(b, "HTTP/1.1 500 Internal server error\r\n"
                "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        goto send_msg;

        #if 0
        error_599:
        /* Not a standard http error, but is in the 500 class */
        strcpy(b, "HTTP/1.1 599 Too many subscriptions\r\n"
                "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        goto send_msg;
        #endif

        send_msg:
        wpa_printf(MSG_DEBUG, "UPNP SUBSCRIBE RESPONSE: >>>%.*s<<<",
                (int)(b-buf), buf);
        errno = 0;
        if (write(c->sd, buf, (b-buf)) != (b-buf)) {
                wpa_printf(MSG_ERROR, 
                        "tiny_upnp_wps_device Failed to send file errno=%d (%s)",
                        errno, strerror(errno));
        }
        os_free(buf);
        os_free(callback_urls);
        return;
}



/* Given that we have received a header w/ UNSUBSCRIBE, act upon it
 *
 * Format of UNSUBSCRIBE (case-insensitive):
 *
 * First line must be:
 *      UNSUBSCRIBE /wps_event HTTP/1.1
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Content-Length: 0
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_unsubscribe(
        struct web_connection *c,
        char *filename
        )
{
        struct upnp_wps_device_sm *sm = c->sm;
        char *buf;      /* output buffer, allocated */
        char *b;
        char *hdr = httpread_hdr_get(c->hread);
        char *h;
        char *match;
        int match_len;
        char *end;
        u8 uuid[UUID_SIZE];
        int got_uuid = 0;
        struct subscription *s = NULL;

        buf = os_zalloc(WEB_PAGE_BUFFER_SIZE);
        if (buf == NULL) {
                return;
        }
        b = buf;

        /* Parse/validate headers */
        h = hdr;
        /* First line: UNSUBSCRIBE /wps_event HTTP/1.1
         * has already been parsed.
         */
        if (strcasecmp(filename, UPNP_WPS_DEVICE_EVENT_FILE) != 0) {
                goto error_412;
        }
        end = strchr(h, '\n');

        for (; end != NULL; h=end+1) {
                /* Option line by option line */
                h = end+1;
                end = strchr(h, '\n');
                if (end == NULL) break; /* no unterminated lines allowed */

                /* HOST should refer to us */
                #if 0
                match = "HOST:"; match_len = strlen(match);
                if (strncasecmp(h, match, match_len) == 0) {
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        .....
                }
                #endif
                /* SID is only for renewal */
                match = "SID:"; match_len = strlen(match);
                if (strncasecmp(h, match, match_len) == 0) {
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        match = "uuid:"; match_len = strlen(match);
                        if (strncasecmp(h, match, match_len) != 0) {
                                goto error_400;
                        }
                        h += match_len;
                        while (*h == ' ' || *h == '\t') h++;
                        if (uuid_parse(uuid, h)) {
                                goto error_400;
                        }
                        got_uuid = 1;
                        continue;
                }
        }

        if (got_uuid) {
                s = subscription_find(sm, uuid);
                if (s) {
                        wpa_printf(MSG_INFO, "upnp_wps_device: unsubscribing %p %s",
                            s, 
                            (s && s->addr_list && s->addr_list->domain_and_port)? 
                            s->addr_list->domain_and_port : "-null-");
                        subscription_unlink(s);
                        subscription_destroy(s);
                }
        } else {
                wpa_printf(MSG_INFO, "upnp wps device: unsubscribe fails (not found)");
                goto error_412;
        }


        /* success 
         */
        strcpy(b, "HTTP/1.1 200 OK\r\n"
                    "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"
                    );
        goto send_msg;

        error_400:
        strcpy(b, "HTTP/1.1 400 Bad request\r\n"
                "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        goto send_msg;

        error_412:
        strcpy(b, "HTTP/1.1 412 Precondition failed\r\n"
                "Server: unspecified, UPnP/1.0, unspecifed\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        goto send_msg;

        send_msg:
        errno = 0;
        if (write(c->sd, buf, (b-buf)) != (b-buf)) {
                wpa_printf(MSG_ERROR, 
                        "upnp_wps_device Failed to send file errno=%d (%s)",
                        errno, strerror(errno));
        }
        os_free(buf);
        return;
}


/*
 * Send error in response to unknown requests
 */
static void web_connection_parse_other(
        struct web_connection *c,
        char *filename
        )
{
        char *buf;
        char *b;

        buf = os_zalloc(WEB_PAGE_BUFFER_SIZE);
        if (buf == NULL) {
                return;
        }
        b = buf;
        strcpy(b, "HTTP/1.1 501 Unimplimented\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        b += strlen(b);
        errno = 0;
        if (write(c->sd, buf, (b-buf)) != (b-buf)) {
                wpa_printf(MSG_ERROR, 
                        "tiny_upnp_wps_device Failed to send file errno=%d (%s)",
                        errno, strerror(errno));
        }
        os_free(buf);
        return;
}



/* Called when we have gotten an apparently valid http request.
 */
static void web_connection_check_data(struct web_connection *c)
{
        struct httpread *hread = c->hread;
        enum httpread_hdr_type htype = httpread_hdr_type_get(hread);
        // char *data = httpread_data_get(hread);
        char *filename = httpread_uri_get(hread);
        struct upnp_wps_device_sm *sm = c->sm;

        c->done = 1;
        /* Trim leading slashes from filename */
        if (filename) while (*filename == '/') filename++;

        wpa_printf(MSG_DEBUG, "UPNP GOT HTTP REQUEST type %d", htype);

        switch(htype) {
                case HTTPREAD_HDR_TYPE_GET:
                        web_connection_parse_get(c, filename);
                break;
                case HTTPREAD_HDR_TYPE_POST:
                        web_connection_parse_post(c, filename);
                break;
                case HTTPREAD_HDR_TYPE_SUBSCRIBE:
                        web_connection_parse_subscribe(c, filename);
                break;
                case HTTPREAD_HDR_TYPE_UNSUBSCRIBE:
                        web_connection_parse_unsubscribe(c, filename);
                break;
                /* We are not required to support M-POST; just plain
                 * POST is supposed to work, so we only support that.
                 * If for some reason we need to support M-POST, it is
                 * mostly the same as POST, with small differences.
                 */
                default:
                        /* Send 501 for anything else */
                        web_connection_parse_other(c, filename);
                break;
        }
 
        tiny_upnp_dump_sm(sm, MSG_DEBUG);     /* for debugging only */
        return;
}



/*
 * called back when we have gotten request
 */
static void web_connection_got_file_handler(
        struct httpread *handle, 
        void *cookie, 
        enum httpread_event en
        )
{
        struct web_connection *c = cookie;

        if (en == HTTPREAD_EVENT_FILE_READY) {
                web_connection_check_data(c);
        }
        web_connection_stop(c);
        return;
}



/* web_connection_start -- the socket descriptor sd is handed over
 * for ownership by the wps upnp state machine.
 */
static void web_connection_start(
        struct upnp_wps_device_sm *sm, 
        int sd,
        unsigned ip_addr,       /* of client, in host byte order */
        unsigned ip_port        /* of client, in native byte order */
        )
{
        struct web_connection *c = NULL;

        /* if too many connections, bail */
        if (sm->n_web_connections >= MAX_WEB_CONNECTIONS) {
                close(sd);
                return;
        }

        do {
                c = os_zalloc(sizeof(*c));
                if (c == NULL) {
                        return;
                }
                c->sm = sm;
                c->sd = sd;
                // c->ip_addr = ip_addr;
                // c->ip_port = ip_port;
                // setting non-blocking should not be necessary for read,
                // and can mess up sending where blocking might be better.
                // if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0) break;
                c->hread = httpread_create(
                        c->sd,
                        web_connection_got_file_handler,
                        c,      /* cookie */
                        WEB_CONNECTION_MAX_READ,
                        WEB_CONNECTION_TIMEOUT_SEC
                        );
                if (c->hread == NULL) {
                        break;
                }
                if (sm->web_connections) {
                        c->next = sm->web_connections;
                        c->prev = c->next->prev;
                        c->prev->next = c;
                        c->next->prev = c;
                } else {
                        sm->web_connections = c->next = c->prev = c;
                }
                sm->n_web_connections++;
                return;
        } while (0);

        if (c) web_connection_stop(c);
        return;
}


/***************************************************************************
 * Listening for web connections
 * We have a single TCP listening port, and hand off connections
 * as we get them.
 *
 **************************************************************************/

static void web_listener_stop(struct upnp_wps_device_sm *sm)
{
        if (sm->web_sd_registered) {
                sm->web_sd_registered = 0;
                eloop_unregister_sock(sm->web_sd, EVENT_TYPE_READ);
        }
        if (sm->web_sd != -1) {
                close(sm->web_sd);
        }
        sm->web_sd = -1;
}

static void web_listener_handler(int sd, void *eloop_ctx, void *sock_ctx)
{
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        struct upnp_wps_device_sm *sm = sock_ctx;
        int new_sd;

        /* Create state for new connection */
        /* Remember so we can cancel if need be */
        new_sd = accept(sm->web_sd, (struct sockaddr *)&addr, &addr_len);
        if (new_sd < 0) {
                wpa_printf(MSG_ERROR, "upnp web listener accept errno=%d (%s) web_sd=%d",
                        errno, strerror(errno), sm->web_sd);
                return;
        }
        web_connection_start(sm, new_sd, addr.sin_addr.s_addr, htons(addr.sin_port));
        return;
}

static int web_listener_start(struct upnp_wps_device_sm *sm)
{
        struct sockaddr_in addr;
        int port;

        do {
                sm->web_sd = socket(AF_INET, SOCK_STREAM, 0);
                if (sm->web_sd < 0) {
                        break;
                }
                if (fcntl(sm->web_sd, F_SETFL, O_NONBLOCK) != 0) {
                        break;
                }
                port = 49152;  /* first non-reserved port */
                for (;;) {
                        addr.sin_family = AF_INET;
                        addr.sin_addr.s_addr = sm->ip_addr;
                        addr.sin_port = htons(port);
                        if (bind(sm->web_sd, (struct sockaddr *)&addr,
                                    sizeof(addr)) == 0 ) {
                                break;
                        }
                        if (errno == EADDRINUSE) {
                                /* search for unused port */
                                if (++port == 65535) goto fail;
                                continue;
                        }
                        goto fail;
                }
                if (listen(sm->web_sd, 10/*max backlog*/) != 0) break;
                if (fcntl(sm->web_sd, F_SETFL, O_NONBLOCK) != 0) break;
                if (eloop_register_sock(sm->web_sd, EVENT_TYPE_READ,
                                web_listener_handler, NULL, sm)) {
                        break;
                }
                sm->web_sd_registered = 1;
                sm->web_port = port;

                return 0;
        } while(0);

        fail:
        /* Error */
        web_listener_stop(sm);
        return -1;
}



/***************************************************************************
 * Event notification -- tell everyone else that something happened
 *
 * In particular, events include WPS messages from clients that are
 * proxied to external registrars.
 *
 **************************************************************************/


int
upnp_wps_device_send_wlan_event(struct upnp_wps_device_sm *sm,
        const u8 from_mac_addr[6],
	int ev_type,
	const u8 *msg, size_t msg_len)
{
	int ret = -1;
	char type[2];
        const u8 *mac = from_mac_addr;
        char mac_text[18];
	u8 *raw = 0;
	size_t raw_len;
	char *val = 0;
	size_t val_len;
	int pos = 0;

	do {
		if (!sm)
			break;

		os_snprintf(type, sizeof(type), "%1u", ev_type);

		raw_len = 1 + 17 + ((msg && msg_len)?msg_len:0);
		raw = (u8 *)wpa_zalloc(raw_len);
		if (!raw)
			break;

		*(raw + pos) = (u8)ev_type;
		pos += 1;
                /* Yes, we really do use the mac address as text here,
                 * in format xx:xx:xx:xx:xx:xx .
                 * Sony does it this way and Intel did it this way...
                 * "WFAWLANConfig:1" document just says "mac address",
                 * it doesn't say text but ...
                 * This address is clearly specified in the document
                 * to be that of the enrollee, NOT the AP.
                 * The Sony code uses the mac address of the AP
                 * which is wrong!
                 */
                sprintf(mac_text, "%02x:%02x:%02x:%02x:%02x:%02x",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                wpa_printf(MSG_INFO, "upnp_wps_device_send_wlan_event: proxying WLANEvent from %s", mac_text);
                os_memcpy(raw+pos, mac_text, 17);
		pos += 17;
		if (msg && msg_len) {
			os_memcpy(raw + pos, msg, msg_len);
			pos += msg_len;
		}
                raw_len = pos;

		if (upnp_wps_device_encode_base64(raw, raw_len, &val, &val_len))
			break;

		upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTTYPE, type);
		upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENTMAC, mac_text);
		upnp_wps_device_set_var(sm, WPS_VAR_WLANEVENT, val);

                /* Only the WLANEVENT var is sent (but it incorporates
                 * type and mac into itself, see above code)
                 */
		upnp_wps_device_send_var(sm, WPS_VAR_WLANEVENT, 0, 0);

		ret = 0;
	} while (0);

	if (raw) os_free(raw);
	if (val) os_free(val);

	return ret;
}





/***************************************************************************
 * Activation / Deactivation
 * Brings us to life and takes us down.
 * But does not do full memory deallocation, which is done by "init".
 *
 **************************************************************************/


/* add_ssdp_network -- assures that the multicast address will be properly
 * handled by linux networking code (by a modification to routing tables).
 * This must be done per network interface.
 * It really only needs to be done once after booting up, but doesn't
 * hurt to do it more frequently "to be safe".
 *
 * This is from original upnp_wps_device.c file.
 */
static int add_ssdp_network(char *net_if)
{
	int ret = -1;
	int sock = -1;
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
                                wpa_printf(MSG_INFO, "add_ssdp_network() ioctl errno %d (%s)",
                                    errno, strerror(errno));
				break;
                        }
		}

		ret = 0;
	} while (0);

	if (-1 != sock)
		close(sock);

	return ret;
}


/* get_netif_info -- get hw and ip addresses for network device
 */
static int get_netif_info(
        char *net_if,           /* IN */
        unsigned *ip_addr,       /* OUT, ip addr in network byte order */
        char **ip_addr_text,     /* OUT, malloc'd dotted decimal */
        u8 mac[6],              /* OUT, mac addr filled in */
        char **mac_addr_text     /* OUT, malloc'd xx:xx:xx:xx:xx:xx */
        )
{
	struct ifreq req;
	int sock = -1;
        struct sockaddr_in *addr;
        struct in_addr in_addr;

        *ip_addr_text = NULL;
        *mac_addr_text = NULL;

        do {
                *ip_addr_text = os_zalloc(16);
                *mac_addr_text = os_zalloc(18);
                if (ip_addr_text == NULL || mac_addr_text == NULL) break;

		if(0 > (sock = socket(AF_INET, SOCK_DGRAM, 0)))
			break;

                strncpy(req.ifr_name, net_if, sizeof(req.ifr_name));
		if (0 > ioctl(sock, SIOCGIFADDR, &req))
			break;
                addr = (void *)&req.ifr_addr;
                *ip_addr = addr->sin_addr.s_addr;
                in_addr.s_addr = *ip_addr;
		os_snprintf(*ip_addr_text, 16, "%s", inet_ntoa(in_addr));

                strncpy(req.ifr_name, net_if, sizeof(req.ifr_name));
		if (0 > ioctl(sock, SIOCGIFHWADDR, &req))
			break;
                memcpy(mac, req.ifr_addr.sa_data, 6);
                os_snprintf(*mac_addr_text, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
                        req.ifr_addr.sa_data[0],
                        req.ifr_addr.sa_data[1],
                        req.ifr_addr.sa_data[2],
                        req.ifr_addr.sa_data[3],
                        req.ifr_addr.sa_data[4],
                        req.ifr_addr.sa_data[5]);

                close(sock); sock = -1;
                return 0;
        } while (0);
        /* Failure. */
        close(sock);
        os_free(*ip_addr_text);
        os_free(*mac_addr_text);
        return -1;
}


int
upnp_wps_device_stop(struct upnp_wps_device_sm *sm)
{
        int ivar;
        wpa_printf(MSG_INFO, "upnp_wps_device_stop ENTER");
	do {
		if (!sm) return -1;

		if (!sm->started) return 0;

                web_listener_stop(sm);
                while (sm->web_connections)
                        web_connection_stop(sm->web_connections);
                while (sm->msearch_replies)
                        msearchreply_state_machine_stop(sm->msearch_replies);
                while (sm->subscriptions)  {
                        struct subscription *s = sm->subscriptions;
                        subscription_unlink(s);
                        subscription_destroy(s);
                }
                advertisement_state_machine_stop(sm);
                event_send_stop_all(sm);
                for (ivar = 0; ivar < WPS_N_VARS; ivar++) {
                        os_free(sm->vars[ivar]);
                }
                os_free(sm->net_if); sm->net_if = NULL;
                os_free(sm->mac_addr_text); sm->mac_addr_text = NULL;
                os_free(sm->ip_addr_text); sm->ip_addr_text = NULL;
                if (sm->multicast_sd > 0) close(sm->multicast_sd);
                sm->multicast_sd = -1;
                ssdp_listener_stop(sm);
	} while (0);

	sm->started = 0;
	return 0;
}


int
upnp_wps_device_start(struct upnp_wps_device_sm *sm, char *net_if)
{
        int sd = -1;
        unsigned char ttl = 4; /* per upnp spec, keep IP packet time to live (TTL) small */

	do {
		if (!sm || !net_if)
			return -1;

		if (sm->started)
			upnp_wps_device_stop(sm);

                sm->net_if = strdup(net_if);
                sm->multicast_sd = -1;
                sm->ssdp_sd = -1;
                sm->started = 1;
                sm->advertise_count = 0;

                /* Fix up linux multicast handling */
		if (add_ssdp_network(net_if)) {
			break;
                }

                /* Determine which IP and mac address we're using */
                if (get_netif_info(net_if, 
                                &sm->ip_addr, &sm->ip_addr_text,
                                sm->mac_addr, &sm->mac_addr_text)) {
                        break;
                }

                /* Listen for incoming TCP connections so that others
                 * can fetch our "xml files" from us.
                 */
                if (web_listener_start(sm)) {
                        break;
                }

                /* Set up for receiving discovery (UDP) packets */
                if (ssdp_listener_start(sm)) {
                        break;
                }

                /* Set up for sending multicast */
                sm->multicast_sd = sd = socket(AF_INET, SOCK_DGRAM, 0);
                if (sd < 0) {
                        break;
                }
                #if 0   /* maybe ok if we sometimes block on writes */
                if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0) {
                        break;
                }
                #endif
                if (setsockopt( sd, IPPROTO_IP, IP_MULTICAST_IF,
                                &sm->ip_addr, sizeof(sm->ip_addr))) {
                        break;
                }
                if (setsockopt( sd, IPPROTO_IP, IP_MULTICAST_TTL,
                                &ttl, sizeof(ttl))) {
                        break;
                }
                #if 0   /* not needed, because we don't receive using multicast_sd */
                struct ip_mreq mreq;
                mreq.imr_multiaddr.s_addr =  inet_addr(UPNP_MULTICAST_ADDRESS);
                mreq.imr_interface.s_addr = sm->ip_addr;
                wpa_printf(MSG_DEBUG, "upnp_wps_device multicast addr 0x%x if addr 0x%x",
                    mreq.imr_multiaddr.s_addr, mreq.imr_interface.s_addr);
                if (setsockopt (sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
                        wpa_printf(MSG_ERROR, 
                            "upnp_wps_device setsockopt IP_ADD_MEMBERSHIP errno %d (%s)",
                                errno, strerror(errno));
                        break;
                }
                #endif  /* not needed */
                /* What about IP_MULTICAST_LOOP ? ... it seems to be on by default?
                 * which aids debugging i suppose but isn't really necessary?
                 */

                /* Broadcast NOTIFY messages to let the world know we exist.
                 *      This is done via a state machine since the messages
                 *      should not be all sent out at once.
                 */
                if (advertisement_state_machine_start(sm)) {
                        break;
                }

		return 0;
	} while (0);

        /* Here on errors */
        upnp_wps_device_stop(sm);
	return -1;
}


/***************************************************************************
 * Runtime initialization  and de-initialization.
 * Per how Sony does it, this sets up some basic data structures...
 * called only once.
 *
 **************************************************************************/

/* upnp_wps_device_deinit -- called on program termination 
 */
void
upnp_wps_device_deinit(struct upnp_wps_device_sm *sm)
{
        wpa_printf(MSG_INFO, "upnp_wps_device_deinit called");
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

/* upnp_wps_device_init -- called when program starts 
 * ctx is given to us, and we must eventually free it.
 */
struct upnp_wps_device_sm *
upnp_wps_device_init(
        struct upnp_wps_device_ctx *ctx,        /* callback table */
	const struct wps_config *conf,  /* extract needed info from only */
	void *priv)             /* passed in callbacks */
{
        wpa_printf(MSG_INFO, "upnp_wps_device_init called");
	struct upnp_wps_device_sm *sm = NULL;
	do {
		sm = os_zalloc(sizeof(*sm));
		if (!sm) {
			break;
                }
		sm->ctx = ctx;
		sm->priv = priv;
                /* Make a copy of some WPS configuration for future use.
                 */
                if (conf->uuid_set) {
                        uuid_format(sm->uuid_string, conf->uuid);
                }       /* else we'll do it later */
                if (conf->manufacturer)
                        strncpy(sm->manufacturer, conf->manufacturer, 
                                sizeof(sm->manufacturer)-1);
                if (conf->model_name)
                        strncpy(sm->model_name, conf->model_name, 
                                sizeof(sm->model_name)-1);
                if (conf->model_number)
                        strncpy(sm->model_number, conf->model_number, 
                                sizeof(sm->model_number)-1);
                if (conf->serial_number)
                        strncpy(sm->serial_number, conf->serial_number, 
                                sizeof(sm->serial_number)-1);
                if (conf->friendly_name)
                        strncpy(sm->friendly_name, conf->friendly_name, 
                                sizeof(sm->friendly_name)-1);
                if (conf->manufacturer_url)
                        strncpy(sm->manufacturer_url, conf->manufacturer_url, 
                                sizeof(sm->manufacturer_url)-1);
                if (conf->model_description)
                        strncpy(sm->model_description, conf->model_description, 
                                sizeof(sm->model_description)-1);
                if (conf->model_url)
                        strncpy(sm->model_url, conf->model_url, 
                                sizeof(sm->model_url)-1);
                if (conf->upc_string)
                        strncpy(sm->upc_string, conf->upc_string, 
                                sizeof(sm->upc_string)-1);
                /* TODO: add additional fields to the configuration
                 * so that they can be put into xml file...
                 * copy them over here, and then put them into the
                 * xml file when needed.
                 */
                return sm;
	} while (0);

        wpa_printf(MSG_ERROR, "upnp_wps_device_deinit failed!");
        upnp_wps_device_deinit(sm);
	return sm;
}

/* 
 * A debugging hack:
 * READVERTISE causes the tiny upnp state machine to restart it's
 * broadcast advertisements, beginning with a "byebye"... this should
 * with any luck fix any failure to subscribe issues.
 * It should also cause existing subscriptions to be abandoned by
 * clients who will hopefully get new ones... 
 * it would likely interfere with an ongoing wps upnp operation.
 */
void upnp_device_readvertise(
        struct upnp_wps_device_sm *sm)
{
    wpa_printf(MSG_ERROR, "TINY UPNP READVERTISE Enter");
    tiny_upnp_dump_sm(sm, MSG_ERROR);
    wpa_printf(MSG_ERROR, "TINY UPNP READVERTISE Start");
    advertisement_state_machine_start(sm);
}


/***************************************************************************
 * End
 *
 **************************************************************************/

#endif  /* WPS_OPT_TINYUPNP */

