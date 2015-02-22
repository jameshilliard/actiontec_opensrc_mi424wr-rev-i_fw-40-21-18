/* whack communicating routines
 * Copyright (C) 1997 Angelos D. Keromytis.
 * Copyright (C) 1998-2001  D. Hugh Redelmeier.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <arpa/nameser.h>	/* missing from <resolv.h> on old systems */
#include <stdlib.h>

#include <freeswan.h>
#include <pfkeyv2.h>
#include <pfkey.h>
#include "block_ip.h"

#include "constants.h"
#include "defs.h"
#include "id.h"
#include "x509.h"
#include "connections.h"	/* needs id.h */
#include "whack.h"	/* needs connections.h */
#include "packet.h"
#include "demux.h"	/* needs packet.h */
#include "state.h"
#include "ipsec_doi.h"	/* needs demux.h and state.h */
#include "kernel.h"
#include "rcv_whack.h"
#include "log.h"
#include "keys.h"
#include "adns.h"	/* needs <resolv.h> */
#include "dnskey.h"	/* needs keys.h and adns.h */
#include "server.h"
#include "timer.h"
#ifndef NO_IKE_ALG
#include "kernel_alg.h"
#include "ike_alg.h"
#ifndef NO_DB_OPS_STATS
#define NO_DB_CONTEXT
#include "db_ops.h"
#endif
#endif
#include "block_ip.h"
#include "rg_utils.h"

/* helper variables and function to decode strings from whack message */

static char *next_str
    , *str_roof;

static bool
unpack_str(char **p)
{
    char *end = memchr(next_str, '\0', str_roof - next_str);

    if (end == NULL)
    {
	return FALSE;	/* fishy: no end found */
    }
    else
    {
	*p = next_str == end? NULL : next_str;
	next_str = end + 1;
	return TRUE;
    }
}

/* bits loading keys from asynchronous DNS */

struct key_add_continuation {
    struct adns_continuation ac;
    int whack_fd;
};

static void
key_add_ugh(const struct id *keyid, err_t ugh)
{
    char name[IDTOA_BUF];	/* longer IDs will be truncated in message */

    (void)idtoa(keyid, name, sizeof(name));
    loglog(RC_NOKEY
	, "failure to fetch key for %s from DNS: %s", name, ugh);
}

static void
key_add_continue(struct adns_continuation *ac, err_t ugh)
{
    struct key_add_continuation *kc = (void *) ac;

    whack_log_fd = kc->whack_fd;
    if (ugh != NULL)
    {
	key_add_ugh(&ac->id, ugh);
    }
    else
    {
	remember_public_keys(&keys_from_dns);
    }
    close_any(whack_log_fd);
}


#if !defined(NO_IKE_ALG) || !defined(NO_KERNEL_ALG)

#define POLICY_MAX_SIZE 1024

struct openrg_propos_flags {
    int flag;
    const char *name;
};

static struct openrg_propos_flags openrg_enc[] = {
    {OPENRG_ENC_NULL, "null"},
    {OPENRG_ENC_1DES, "des"},
    {OPENRG_ENC_3DES, "3des"},
    {OPENRG_ENC_AES128, "aes128"},
    {OPENRG_ENC_AES192, "aes192"},
    {OPENRG_ENC_AES256, "aes256"},
    {0, NULL}
};
#endif

#if !defined(NO_IKE_ALG)
static struct openrg_propos_flags openrg_auth[] = {
    {OPENRG_AUTH_MD5, "md5"},
    {OPENRG_AUTH_SHA, "sha"},
    {0, NULL}
};

static struct openrg_propos_flags openrg_group[] = {
    {OPENRG_MODP_768, "modp768"},
    {OPENRG_MODP_1024, "modp1024"},
    {OPENRG_MODP_1536, "modp1536"},
    {0, NULL}
};

static void make_alg_ike_string(int openrg_main_policy, char **var)
{
    char *buf, *tmp;
    int i, j, k, len;

    *var = NULL;
    buf = tmp = (char *)malloc(POLICY_MAX_SIZE);
    if (!buf)
	return;
    for (i=0; openrg_enc[i].name; i++)
    {
	if (!(openrg_enc[i].flag & openrg_main_policy))
	    continue;
	for (j=0; openrg_auth[j].name; j++)
	{
	    if (!(openrg_auth[j].flag & openrg_main_policy))
		continue;
	    for (k=0; openrg_group[k].name; k++)
	    {
		if (!(openrg_group[k].flag & openrg_main_policy))
		    continue;
		len = snprintf(tmp, POLICY_MAX_SIZE-(tmp-buf), "%s-%s-%s,",
		    openrg_enc[i].name, openrg_auth[j].name,
		    openrg_group[k].name);
		if (len<0)
		{
		    loglog(RC_LOG_SERIOUS, "main mode policy was truncated");
		    goto Exit;
		}
		tmp += len;
	    }
	}
    }

Exit:
    tmp = strrchr(buf, ',');
    /* Set strict flag. */
    if (tmp)
	*tmp = '!';
    *var = strdup(buf);
    free(buf);
}
#endif

#if !defined(NO_KERNEL_ALG)
static struct openrg_propos_flags openrg_auth_p2[] = {
    {OPENRG_AUTH_MD5, "md5"},
    {OPENRG_AUTH_SHA, "sha1"},
    {0, NULL}
};

static void make_alg_esp_string(int openrg_quick_policy, char **var)
{
    int i, j, len;
    char *buf, *tmp;

    *var = NULL;
    buf = tmp = (char *)malloc(POLICY_MAX_SIZE);
    if (!buf)
	return;
    *buf = 0;
    for (i=0; openrg_enc[i].name; i++)
    {
	if (!(openrg_enc[i].flag & openrg_quick_policy))
	    continue;
	for (j=0; openrg_auth_p2[j].name; j++)
	{
	    if (!(openrg_auth_p2[j].flag & openrg_quick_policy))
		continue;
	    len = snprintf(tmp, POLICY_MAX_SIZE-(tmp-buf), "%s-%s,",
		openrg_enc[i].name, openrg_auth_p2[j].name);
	    if (len<0)
	    {
		loglog(RC_LOG_SERIOUS, "quick mode policy was truncated");
		goto Exit;
	    }
	    tmp += len;
	}
    }
    tmp = strrchr(buf, ',');
    /* Set strict flag. */
    if (tmp)
	*tmp = '!';
    if (!*buf)
	goto Exit;
    /* Add pfs group if signed. */
    for (i=0; openrg_group[i].name; i++)
    {
	if (!(openrg_group[i].flag & openrg_quick_policy))
	    continue;
	strcpy(tmp, ";");
	strcat(tmp, openrg_group[i].name);
	break;
    }

Exit:
    *var = strdup(buf);
    free(buf);
}
#endif

/* Handle a kernel request. Supposedly, there's a message in
 * the kernelsock socket.
 */
void
whack_handle(int whackctlfd)
{
    struct whack_message msg;
    struct sockaddr_un whackaddr;
    int whackaddrlen = sizeof(whackaddr);
    int whackfd = accept(whackctlfd, (struct sockaddr *)&whackaddr, &whackaddrlen);
    ssize_t n;
#if !defined(NO_IKE_ALG)
    int is_ike_allocated = 0;
#endif
#if !defined(NO_KERNEL_ALG)
    int is_esp_allocated = 0;
#endif

    if (whackfd < 0)
    {
	log_errno((e, "accept() failed in whack_handle()"));
	return;
    }
    n = read(whackfd, &msg, sizeof(msg));
    if (n == -1)
    {
	log_errno((e, "read() failed in whack_handle()"));
	close(whackfd);
	return;
    }

    whack_log_fd = whackfd;

    /* sanity check message */
    {
	err_t ugh = NULL;

	next_str = msg.string;
	str_roof = (char *)&msg + n;

	if (next_str > str_roof)
	{
	    ugh = builddiag("truncated message from whack: got %d bytes; expected %d.  Message ignored."
		, (int)n, (int) sizeof(msg));
	}
	else if (msg.magic != WHACK_MAGIC)
	{
	    ugh = builddiag("message from whack has bad magic %d; should be %d; probably wrong version.  Message ignored"
		, msg.magic, WHACK_MAGIC);
	}
	else if (!unpack_str(&msg.name)		/* string 1 */
	|| !unpack_str(&msg.left.id)		/* string 2 */
	|| !unpack_str(&msg.left.cert)		/* string 3 */
	|| !unpack_str(&msg.left.updown)	/* string 4 */
	|| !unpack_str(&msg.right.id)		/* string 5 */
	|| !unpack_str(&msg.right.cert)		/* string 6 */
	|| !unpack_str(&msg.right.updown)	/* string 7 */
	|| !unpack_str(&msg.keyid)		/* string 8 */
#ifndef NO_IKE_ALG
	|| !unpack_str(&msg.ike)		/* string 9 */
#endif
#ifndef NO_KERNEL_ALG
	|| !unpack_str(&msg.esp)		/* string 10 */
#endif
	|| str_roof - next_str != (ptrdiff_t)msg.keyval.len)	/* check chunk */
	{
	    ugh = "message from whack contains bad string";
	}
	else
	{
	    msg.keyval.ptr = next_str;	/* grab chunk */
	}

	if (ugh != NULL)
	{
	    loglog(RC_BADWHACKMESSAGE, "%s", ugh);
	    whack_log_fd = NULL_FD;
	    close(whackfd);
	    return;
	}

#if !defined(NO_IKE_ALG)
	if (!msg.ike || !*msg.ike)
	{
	    make_alg_ike_string(msg.openrg_main_policy, &msg.ike);
	    is_ike_allocated = 1;
	}
#endif
#if !defined(NO_KERNEL_ALG)
	if (!msg.esp || !*msg.esp)
	{
	    make_alg_esp_string(msg.openrg_quick_policy, &msg.esp);
	    is_esp_allocated = 1;
	}
#endif
    }

    if (msg.whack_options)
    {
#if defined(DEBUG) || defined(CONFIG_PLUTO_DEBUG)
	if (msg.name == NULL)
	{
	    /* we do a two-step so that if either old or new would
	     * cause the message to print, it will be printed.
	     */
	    cur_debugging |= msg.debugging;
	    DBG(DBG_CONTROL
		, DBG_log("base debugging = %s"
		    , bitnamesof(debug_bit_names, msg.debugging)));
	    cur_debugging = base_debugging = msg.debugging;
	}
	else if (!msg.whack_connection)
	{
	    struct connection *c = con_by_name(msg.name, TRUE);

	    if (c != NULL)
	    {
		c->extra_debugging = msg.debugging;
		DBG(DBG_CONTROL
		    , DBG_log("\"%s\" extra_debugging = %s"
			, c->name
			, bitnamesof(debug_bit_names, c->extra_debugging)));
	    }
	}
#endif
    }

    /* Deleting combined with adding a connection works as replace.
     * To make this more useful, in only this combination,
     * delete will silently ignore the lack of the connection.
     */
    if (msg.whack_delete)
    {
	struct connection *c = con_by_name(msg.name, !msg.whack_connection);

	/* note: this is a "while" because road warrior
	 * leads to multiple connections with the same name.
	 */
	for (; c != NULL; c = con_by_name(msg.name, FALSE))
	{
	    c->deleted_by_whack = TRUE;
	    delete_connection(c);
	}
    }

    if (msg.whack_deletestate)
    {
	struct state *st = state_with_serialno(msg.whack_deletestateno);

	if (st == NULL)
	{
	    loglog(RC_UNKNOWN_NAME, "no state #%lu to delete"
		, msg.whack_deletestateno);
	}
	else
	{
	    delete_state(st);
	}
    }

    if (msg.whack_deleteinstance)
	delete_connection_instance(msg.name, msg.whack_deleteinstanceno);

    if (msg.whack_connection)
	add_connection(&msg);

    /* process "listen" before any operation that could require it */
    if (msg.whack_listen)
    {
	log("listening for IKE messages");
	listening = TRUE;
	find_ifaces();
	load_preshared_secrets();
    }
    if (msg.whack_unlisten)
    {
	log("no longer listening for IKE messages");
	listening = FALSE;
    }

    if (msg.whack_reread & REREAD_SECRETS)
    {
	load_preshared_secrets();
    }

   if (msg.whack_reread & REREAD_MYCERT)
    {
	load_mycert();
    }

   if (msg.whack_reread & REREAD_CACERTS)
    {
	load_cacerts();
    }

   if (msg.whack_reread & REREAD_CRLS)
    {
	load_crls();
    }

   if (msg.whack_list & LIST_PUBKEYS)
    {
	list_public_keys(msg.whack_utc);
    }

    if (msg.whack_list & LIST_CERTS)
    {
	list_certs(msg.whack_utc);
    }

    if (msg.whack_list & LIST_CACERTS)
    {
	list_cacerts(msg.whack_utc);
    }

    if (msg.whack_list & LIST_CRLS)
    {
	list_crls(msg.whack_utc, strict_crl_policy);
    }

    if (msg.whack_list & LIST_EVENTS)
    {
	timer_list();
    }

    if (msg.whack_key)
    {
	/* add a public key */
	struct id keyid;
	err_t ugh = atoid(msg.keyid, &keyid);

	if (ugh != NULL)
	{
	    loglog(RC_BADID, "bad --keyid \"%s\": %s", msg.keyid, ugh);
	}
	else
	{
	    if (!msg.whack_addkey)
		delete_public_keys(&keyid, msg.pubkey_alg);

	    if (msg.keyval.len == 0)
	    {
		struct key_add_continuation *kc
		    = alloc_thing(struct key_add_continuation
			, "key add continuation");
		int wfd = dup_any(whackfd);

		kc->whack_fd = wfd;
		ugh = start_adns_query(&keyid
		    , NULL
		    , T_KEY
		    , key_add_continue
		    , &kc->ac);

		if (ugh != NULL)
		{
		    key_add_ugh(&keyid, ugh);
		    close_any(wfd);
		}
	    }
	    else
	    {
		ugh = add_public_key(&keyid, DAL_LOCAL, msg.pubkey_alg
		    , &msg.keyval, &pubkeys);
		if (ugh != NULL)
		    loglog(RC_LOG_SERIOUS, "%s", ugh);
	    }
	}
    }

    if (msg.whack_route)
    {
	if (!listening)
	    whack_log(RC_DEAF, "need --listen before --route");
	else
	{
	    struct connection *c = con_by_name(msg.name, TRUE);

	    if (c != NULL)
	    {
		set_cur_connection(c);
		if (!oriented(*c))
		    whack_log(RC_ORIENT
			, "we have no ipsecN interface for either end of this connection");
		else if (!trap_connection(c))
		    whack_log(RC_ROUTE, "could not route");
		reset_cur_connection();
	    }
	}
    }

    if (msg.whack_unroute)
    {
	struct connection *c = con_by_name(msg.name, TRUE);

	if (c != NULL)
	{
	    set_cur_connection(c);
	    if (c->routing >= RT_ROUTED_TUNNEL)
		whack_log(RC_RTBUSY, "cannot unroute: route busy");
	    else
		unroute_connection(c);
	    reset_cur_connection();
	}
    }

    if (msg.whack_initiate)
    {
	if (!listening)
	    whack_log(RC_DEAF, "need --listen before --initiate");
	else
	    initiate_connection(msg.name
		, msg.whack_async? NULL_FD : dup_any(whackfd));
    }

    if (msg.whack_oppo_initiate)
    {
	if (!listening)
	    whack_log(RC_DEAF, "need --listen before opportunistic initiation");
	else
	    initiate_opportunistic(&msg.oppo_my_client, &msg.oppo_peer_client, 0
		, FALSE
		, msg.whack_async? NULL_FD : dup_any(whackfd));
    }

    if (msg.whack_terminate)
	terminate_connection(msg.name);

    if (msg.whack_status)
    {
	show_ifaces_status();
	whack_log(RC_COMMENT, BLANK_FORMAT);	/* spacer */
#ifndef NO_KERNEL_ALG
	kernel_alg_show_status();
	whack_log(RC_COMMENT, BLANK_FORMAT);	/* spacer */
#endif
#ifndef NO_IKE_ALG
	ike_alg_show_status();
	whack_log(RC_COMMENT, BLANK_FORMAT);	/* spacer */
#endif
#ifndef NO_DB_OPS_STATS
	db_ops_show_status();
	whack_log(RC_COMMENT, BLANK_FORMAT);	/* spacer */
#endif
	show_connections_status();
	whack_log(RC_COMMENT, BLANK_FORMAT);	/* spacer */
	show_states_status();
#ifdef KLIPS
	whack_log(RC_COMMENT, BLANK_FORMAT);	/* spacer */
	show_shunt_status();
#endif
    }

    if (msg.whack_shutdown)
    {
	log("shutting down");
	exit_pluto(0);	/* delete lock and leave, with 0 status */
    }

    if (msg.whack_anti_replay)
	pfkey_anti_replay_enabled_set(msg.anti_replay_enabled);

    if (msg.whack_block_ip)
    {
	block_peer_set(msg.block_ip_reject_num, msg.block_ip_period);
    }

    whack_log_fd = NULL_FD;
    close(whackfd);
    
#if !defined(NO_IKE_ALG)
    if (is_ike_allocated)
	free(msg.ike);
#endif
#if !defined(NO_KERNEL_ALG)
    if (is_esp_allocated)
	free(msg.esp);
#endif
}
