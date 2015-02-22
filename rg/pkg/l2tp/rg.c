/****************************************************************************
 *
 * rg/pkg/l2tp/rg.c
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

#include "l2tp.h"
#include "rg_ipc.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>

#include <be_api_gpl.h>
#include <util/openrg_gpl.h>
#include <kos_chardev_id.h>
#include <util/alloc.h>

#define HANDLER_NAME "jungo"

typedef struct {
    EventHandler *read_event;	/* Read event handler */
    int fd;			/* socket of master tty */
    int attached;		/* 1 if char device is already attached 
				   0 otherwise */
} rg_session_context_t;

typedef struct {
    uint32_t id;		/* as received in the open message */
    enum {
	PEER_ACTIVE = 0,
	PEER_WAITING_CLOSE = 1,
    } state;
} rg_peer_context_t;

typedef struct rg_ipc_send_info_t {
    EventHandler *retry_event;	/* Retry event handler */
    uint16_t port;
    l2tpd_cmd_t cmd;
    /* 'id' is:
     *   - connection ID when this message notifies about established
     *     connection;
     *   - unaccept tunnel ID when the message notifies about new incoming
     *     request.
     */
    uint32_t id;
    /* For L2TP server only: the new connection requester IP and IP of interface
     * that recieve the request.
     * For L2TP client remote_ip.s_addr and local ip.s_addr must be 0.
     */
    struct in_addr remote_ip;
    struct in_addr local_ip;
} rg_ipc_send_info_t;

static int rg_session_close(EventSelector *es, uint32_t id);
static rg_peer_context_t *rg_alloc_peer_context(uint32_t id);
static int rg_session_open(EventSelector *es, uint32_t id, struct in_addr addr,
    struct in_addr rg_addr, char *secret, int secret_len);
static int establish_session(l2tp_session *ses);
static void close_session(l2tp_session *ses, char const *reason);
static void rg_tunnel_close(l2tp_tunnel *tunnel);
static void handle_frame(l2tp_session *ses, unsigned char *buf, size_t len);
static void readable(EventSelector *es, int fd, unsigned int flags, void *data);
static int l2tp_peer_is_equal_id(l2tp_peer *peer, void *param);
static void rg_session_cont_free(l2tp_session *ses);
static int rg_send_ipc(uint16_t port, l2tpd_cmd_t cmd, EventSelector *es,
    uint32_t id, struct in_addr *remote_ip, struct in_addr *local_ip);
static int rg_new_request_notify(l2tp_tunnel *tunnel, uint32_t unaccept_id,
    uint32_t remote, uint32_t local);
static l2tp_peer *rg_get_lns_peer(l2tp_tunnel *tunnel, struct in_addr *peer_ip,
    uint16_t port);

static l2tp_call_ops my_ops = {
    establish_session,
    close_session,
    handle_frame
};

static l2tp_lac_handler my_lac_handler = {
    NULL,
    HANDLER_NAME,
    &my_ops
};

static l2tp_lns_handler my_lns_handler = {
    NULL,
    HANDLER_NAME,
    &my_ops
};

static int ipc_sock = -1;
static EventHandler *ipc_event = NULL;
static char l2tp_server_secret[MAX_SECRET_LEN];
static uint32_t l2tp_server_secret_len;

static int rg_backend_attach(uint32_t id, char *dev_name)
{
    l2tp_tunnel *tunnel;
    l2tp_peer *peer;
    l2tp_session *ses;
    void *dummy;
    rg_session_context_t *cont;

    /* search matching session */
    if (!(peer = l2tp_peer_find_by_func(l2tp_peer_is_equal_id, &id)) ||
	!peer->private)
    {
	l2tp_set_errmsg("peer for id %d not found", id);
	return -1;
    }
    
    if (!(tunnel = tunnel_find_bypeer(peer)))
    {
	l2tp_set_errmsg("tunnel for id %d not found", id);
	return -1;
    }
    
    if (!(ses = l2tp_tunnel_first_session(tunnel, &dummy)))
    {
	l2tp_set_errmsg("session for id %d not found", id);
	return -1;
    }

    cont = malloc(sizeof(rg_session_context_t));
    if (!cont)
    {
	l2tp_set_errmsg("unable to allocate session private memory");
	return -1;
    }

    ses->private = cont;
    memset(cont, 0, sizeof(rg_session_context_t));

    /* open char device and attach to it */
    if ((cont->fd = gpl_sys_rg_chrdev_open(KOS_CDT_PPPCHARDEV_BACKEND, O_RDWR))
	< 0)
    {
	l2tp_set_errmsg("Can't open ppp chardevice");
	goto Error;
    }

    if (ioctl(cont->fd, PPPBE_ATTACH, dev_name) < 0)
    {
	l2tp_set_errmsg("failed attach l2tpd to device %s", dev_name);
	goto Error;
    }
    cont->attached = 1;

    cont->read_event = Event_AddHandler(ses->tunnel->es, cont->fd,
	EVENT_FLAG_READABLE, readable, ses);

    l2tp_db(DBG_CONTROL, "session %u to %s opened, fd %d",
	((rg_peer_context_t *)ses->tunnel->peer->private)->id,
       	inet_ntoa(ses->tunnel->peer->addr.sin_addr), cont->fd);
    return 0;
Error:
    rg_session_cont_free(ses);
    return -1;
}

static int l2tp_wait_for_new_req_reply = 0;

/* called by l2tpd main event loop when there is something to read from ipc
 * socket */
static void rg_input(EventSelector *es, int sock, unsigned int flags,
    void *data)
{
    int fd, rc = -1;
    u8 secret[MAX_SECRET_LEN];
    l2tpd_cmd_t cmd;
    u32 conn_id, secret_len;

    /* read message */
    if ((fd = ipc_accept(sock))<0 || (rc = ipc_u32_read(fd, (u32 *)&cmd)))
	goto Exit;

    /* handle message */
    switch (cmd)
    {
    case L2TP_RG2D_CLIENT_CONNECT:
	{
	    struct in_addr serv_ip, rg_ip;

	    if ((rc = ipc_u32_read(fd, &conn_id)) ||
		(rc = ipc_read(fd, (u8 *)&serv_ip, sizeof(serv_ip))) ||
		(rc = ipc_read(fd, (u8 *)&rg_ip, sizeof(rg_ip))) ||
		(rc = ipc_u32_read(fd, &secret_len)))
	    {
		goto Exit;
	    }
	    if (secret_len)
	    {
		if (secret_len > MAX_SECRET_LEN ||
		    (rc = ipc_read(fd, secret, secret_len)))
		{
		    goto Exit;
		}
	    }
	    
	    /* open new session */
	    rg_session_open(es, conn_id, serv_ip, rg_ip, secret, secret_len);
	}
	break;
    case L2TP_RG2D_CLOSE:
	if ((rc = ipc_u32_read(fd, &conn_id)))
	    goto Exit;
	/* close session */
	rg_session_close(es, conn_id);
	break;
    case L2TP_RG2D_ATTACH:
	{
	    char *dev_name;

	    if ((rc = ipc_u32_read(fd, &conn_id)) ||
		!(dev_name = ipc_string_read(fd)))
	    {
		goto Exit;
	    }
	    rg_backend_attach(conn_id, dev_name);
	    free(dev_name);
	}
	break;
    case L2TP_RG2D_DETACH:
	{
	    l2tp_tunnel *tunnel;
	    l2tp_session *ses;
	    void *dummy;

	    if ((rc = ipc_u32_read(fd, &conn_id)))
		goto Exit;
	    tunnel = tunnel_find_bypeer(l2tp_peer_find_by_func(
		l2tp_peer_is_equal_id, &conn_id));
	    if (!tunnel)
		break;
	    ses = l2tp_tunnel_first_session(tunnel, &dummy);
	    if (!ses)
		break;
	    rg_session_cont_free(ses);
	    break;
	}
    case L2TP_RG2D_SERVER_CONNECT:
	{
	    u32 unaccept_id;
	    rg_peer_context_t *conn;

	    l2tp_wait_for_new_req_reply = 0;

	    if ((rc = ipc_u32_read(fd, &conn_id)) ||
		(rc = ipc_u32_read(fd, &unaccept_id)))
	    {
		goto Exit;
	    }
	    conn = rg_alloc_peer_context(conn_id);
	    if (!conn)
	    {
		tunnel_reject_SCCRQ(unaccept_id);
		goto Exit;
	    }
	    tunnel_accept_SCCRQ(unaccept_id, conn);
	}
	break;
    case L2TP_RG2D_SERVER_START:
	if ((rc = ipc_u32_read(fd, &secret_len)))
	    goto Exit;
	if (!secret_len)
	{
	    l2tp_server_secret_len = 0;
	    memset(l2tp_server_secret, 0, sizeof(l2tp_server_secret));
	}
	else
	{
	    if (secret_len > MAX_SECRET_LEN ||
		(rc = ipc_read(fd, secret, secret_len)))
	    {
		goto Exit;
	    }
	    /* All OK, now update L2TP server's shared secret. */
	    l2tp_server_secret_len = secret_len;
	    memset(l2tp_server_secret, 0, sizeof(l2tp_server_secret));
	    memcpy(l2tp_server_secret, secret, secret_len);
	}

	/* Set L2TP server callbacks. */
	tunnel_set_new_request_cb(rg_new_request_notify);
	tunnel_set_get_lns_peer(rg_get_lns_peer);

	break;
    case L2TP_RG2D_SERVER_STOP:
	/* Stop accept new requests. */
	tunnel_set_new_request_cb(NULL);
	tunnel_set_get_lns_peer(NULL);
	/* No stop active incoming connection at this point - main task downs
	 * all appropriated devices.
	 */
	break;
    case L2TP_RG2D_REJECT_REQUEST:
	{
	    uint32_t unaccept_id;

	    l2tp_wait_for_new_req_reply = 0;

	    if ((rc = ipc_u32_read(fd, &unaccept_id)))
		goto Exit;
	    tunnel_reject_SCCRQ(unaccept_id);
	}
	break;
    case L2TP_RG2D_UPDATE_IPS:
	{
	    uint32_t num;
	    struct in_addr *srcs = NULL;

	    if ((rc = ipc_u32_read(fd, &num)))
		goto Exit;
	    if (num)
	    {
		srcs = calloc(num, sizeof(*srcs));
		if (!srcs)
		    goto Exit;
		if ((rc = ipc_read(fd, srcs, num * sizeof(*srcs))))
		{
		    free(srcs);
		    goto Exit;
		}
	    }
	    l2tp_socket_init(es, srcs, num);
	    free(srcs);
	    break;
	}
    default:
	break;
    }

Exit:
    if (fd>=0)
	ipc_server_close(fd, rc);
}

static int l2tp_ipc_open(EventSelector *es)
{
    ipc_sock = ipc_bind_listen_port(htons(RG_IPC_PORT_MT_2_L2TPD));
    if (ipc_sock < 0)
    {
	l2tp_set_errmsg("could not open ipc socket");
	return -1;
    }

    ipc_event = Event_AddHandler(es, ipc_sock, EVENT_FLAG_READABLE, rg_input,
	NULL);
    
    return 0;
}

static void l2tp_ipc_close(EventSelector *es)
{
    if (ipc_event)
    {
	Event_DelHandler(es, ipc_event);
	ipc_event = NULL;
    }

    if (ipc_sock < 0)
	return;
    
    close(ipc_sock);
    ipc_sock = -1;
}

int rg_init(EventSelector *es)
{
    rg_openlog("", LOG_PID | LOG_NDELAY | LOG_CONS, LOG_DAEMON);
    l2tp_session_register_lac_handler(&my_lac_handler);
    l2tp_session_register_lns_handler(&my_lns_handler);

    return l2tp_ipc_open(es);
}

static int l2tp_peer_is_equal_id(l2tp_peer *peer, void *param)
{
    rg_peer_context_t *cont = peer->private;

    return cont && cont->id == *(uint32_t *)param;
}

static int rg_session_close(EventSelector *es, uint32_t id)
{
    l2tp_tunnel *tunnel;
    l2tp_peer *peer;
    rg_peer_context_t *cont;
    l2tp_session *ses;
    void *dummy;

    /* search matching session */
    peer = l2tp_peer_find_by_func(l2tp_peer_is_equal_id, &id);
    if (!peer || !peer->private)
    {
	l2tp_set_errmsg("session %u not found", id);
	return -1;
    }
    cont = peer->private;
    switch (cont->state)
    {
    case PEER_ACTIVE:
	tunnel = tunnel_find_bypeer(peer);
	peer_release(peer);
	if (!tunnel)
	{
	    l2tp_set_errmsg("no tunnel found for session %u to %s", id,
	       	inet_ntoa(peer->addr.sin_addr));
	    peer_free(peer);
	    return -1;
	}
	cont->state = PEER_WAITING_CLOSE;
	ses = l2tp_tunnel_first_session(tunnel, &dummy);
	if (ses)
	    rg_session_cont_free(ses);
	l2tp_tunnel_stop_tunnel(tunnel, "Closed by " RG_PROD_STR);
	DBG(l2tp_db(DBG_FLOW, "tunnel stopped for session %u to %s", id,
	    inet_ntoa(peer->addr.sin_addr)));
	break;
    case PEER_WAITING_CLOSE:
	/* should never be reached */
	l2tp_set_errmsg("session %u to %s already closing", id, 
	    inet_ntoa(peer->addr.sin_addr));
	return -1;
    }

    return 0;
}

static void rg_peer_close(l2tp_peer *p)
{
    if (!p->private)
	return;

    free(p->private);
    p->private = NULL;
}

static l2tp_peer *rg_get_lns_peer(l2tp_tunnel *tunnel, struct in_addr *peer_ip,
    uint16_t port)
{
    /* Peer's context is unknown at this time and will be updated later. */
    l2tp_peer *p = l2tp_peer_insert_and_fill(0, HANDLER_NAME, peer_ip,
	port, &tunnel->local_addr, l2tp_server_secret, l2tp_server_secret_len,
	NULL, rg_peer_close);
    if (!p)
	return NULL;
    tunnel->close = rg_tunnel_close;
    return p;
}

static rg_peer_context_t *rg_alloc_peer_context(uint32_t id)
{
    rg_peer_context_t *cont = malloc(sizeof(*cont));

    if (!cont)
	return NULL;
    memset(cont, 0, sizeof(*cont));
    cont->state = PEER_ACTIVE;
    cont->id = id;
    return cont;
}

static int rg_session_open(EventSelector *es, uint32_t id,
    struct in_addr peer_addr, struct in_addr rg_addr, char *secret,
    int secret_len)
{
    l2tp_peer *p;
    rg_peer_context_t *cont;
    l2tp_session *ses;

    l2tp_db(DBG_CONTROL, "opening session %u to %s", id, inet_ntoa(peer_addr));

    cont = rg_alloc_peer_context(id);
    if (!cont)
    {
	l2tp_set_errmsg("could not allocate peer context for session %u to %s", 
	    id, inet_ntoa(peer_addr));
	return -1;
    }

    p = l2tp_peer_insert_and_fill(1, HANDLER_NAME, &peer_addr,
	L2TP_PORT, &rg_addr, secret, secret_len, cont, rg_peer_close);
    if (!p)
    {
	l2tp_set_errmsg("could not create peer for session %u to %s", id,
	    inet_ntoa(peer_addr));
	free(cont);
	return  -1;
    }

    ses = l2tp_session_call_lns(p, "foobar", es, NULL);
    if (!ses)
    {
	l2tp_set_errmsg("could not call peer of session %u to %s", id,
	    inet_ntoa(p->addr.sin_addr));
	peer_release(p);
	peer_free(p);
	return -1;
    }
    ses->tunnel->close = rg_tunnel_close;

    return 0;
}

static void rg_ipc_send_msg_free(EventSelector *es,
    rg_ipc_send_info_t *msg_info)
{
    if (msg_info->retry_event)
	Event_DelHandler(es, msg_info->retry_event);

    free(msg_info);
}

static void rg_send_ipc_try(EventSelector *es, int fd_dummy,
    unsigned int flags_dummy, void *data)
{
    rg_ipc_send_info_t *msg_info = data;
    int rc = -1, fd = -1;

    l2tp_ipc_close(es);

    if ((fd = ipc_connect(htons(msg_info->port)))<0 ||
	(rc = ipc_u32_write(fd, msg_info->cmd)) ||
	(rc = ipc_u32_write(fd, msg_info->id)) ||
	(msg_info->remote_ip.s_addr &&
	(rc = ipc_write(fd, (uint8_t *)&msg_info->remote_ip.s_addr,
	sizeof(msg_info->remote_ip.s_addr)))) ||
	(msg_info->local_ip.s_addr &&
	(rc = ipc_write(fd, (uint8_t *)&msg_info->local_ip.s_addr,
	sizeof(msg_info->local_ip.s_addr)))))
    {
	struct timeval t;

	/* schedule retry 0.1 seconds from now */
	t.tv_sec = 0;
	t.tv_usec = 100000;
	msg_info->retry_event =
	    Event_AddTimerHandler(es, t, rg_send_ipc_try, msg_info);
	goto Exit;
    }
    rg_ipc_send_msg_free(es, msg_info);

Exit:
    l2tp_ipc_open(es);
    if (fd >= 0)
	ipc_client_close(fd, rc);
}

static int rg_send_ipc(uint16_t port, l2tpd_cmd_t cmd, EventSelector *es,
    uint32_t id, struct in_addr *remote_ip, struct in_addr *local_ip)
{
    rg_ipc_send_info_t *msg_info;

    msg_info = zalloc_l(sizeof(*msg_info));
    if (!msg_info)
	return -1;

    msg_info->cmd = cmd;
    msg_info->port = port;
    msg_info->id = id;
    if (remote_ip)
	msg_info->remote_ip.s_addr = remote_ip->s_addr;
    if (local_ip)
	msg_info->local_ip.s_addr = local_ip->s_addr;

    rg_send_ipc_try(es, 0, 0, msg_info);

    return 0;
}

static void rg_session_cont_free(l2tp_session *ses)
{
    rg_session_context_t *cont = ses->private;

    if (!cont)
	return;

    if (cont->read_event)
	Event_DelHandler(ses->tunnel->es, cont->read_event);
    if (cont->attached)
	ioctl(cont->fd, PPPBE_DETACH, NULL); 
    if (cont->fd >= 0)
	close(cont->fd);
    free(cont);
    ses->private = NULL;
}

static int rg_new_request_notify(l2tp_tunnel *tunnel, uint32_t unaccept_id,
    uint32_t remote, uint32_t local)
{
    struct in_addr remote_ip, local_ip;

    remote_ip.s_addr = remote;
    local_ip.s_addr = local;

    /* Temporary hack until B37933 is fixed.
     * Do not accept any requests from client until main_task send answer about
     * l2tp process' previous new request notification. Without this fix both
     * main_task and l2tp process may send ipc message at same time that can
     * stuck OpenRG.
     */
    if (l2tp_wait_for_new_req_reply)
	return -1;
    l2tp_wait_for_new_req_reply = 1;

    /* update caller that session is open */
    return rg_send_ipc(RG_IPC_PORT_L2TPD_2_TASK, L2TP_D2RG_NEW_REQUEST,
	tunnel->es, unaccept_id, &remote_ip, &local_ip);
}

static int establish_session(l2tp_session *ses)
{
    /* update caller that session is open */
    return rg_send_ipc(RG_IPC_PORT_L2TPD_2_MT, L2TP_D2RG_CONNECTED,
	ses->tunnel->es,
	((rg_peer_context_t *)ses->tunnel->peer->private)->id, NULL, NULL);
}

/* close_session() is called just before ses is freed */
static void close_session(l2tp_session *ses, char const *reason)
{
    l2tp_db(DBG_CONTROL, "session %u to %s closed. reason %s",
	((rg_peer_context_t *)ses->tunnel->peer->private)->id,
       	inet_ntoa(ses->tunnel->peer->addr.sin_addr), reason ? reason : "-");

    if (ses->private)
	rg_session_cont_free(ses);
}

/* rg_tunnel_close() is called just before tunnel is freed */
static void rg_tunnel_close(l2tp_tunnel *tunnel)
{
    rg_peer_context_t *cont = tunnel->peer->private;

    if (!cont)
	return;

    DBG(l2tp_db(DBG_TUNNEL, "tunnel of session %u to %s closed",
	((rg_peer_context_t *)tunnel->peer->private)->id,
       	inet_ntoa(tunnel->peer->addr.sin_addr)));

    switch (cont->state)
    {
    case PEER_ACTIVE:
	peer_release(tunnel->peer);
	/* fall through */
    case PEER_WAITING_CLOSE:
	/* peer already released, either here or in rg_session_close() */
	peer_free(tunnel->peer);
	break;
    }
}

static void handle_frame(l2tp_session *ses, unsigned char *buf, size_t len)
{
    int n;
    rg_session_context_t *cont = ses->private;

    /* chardev was not attached yet */
    if (!cont)
	return;
    
    /* TODO: Add error checking */
    n = write(cont->fd, buf, len);

    DBG(l2tp_db(DBG_XMIT_RCV,
	"handle_frame() sent to pppd %d octets of %d (%d-%s):\n",
	n, len, errno, strerror(errno)));
    DBG(l2tp_db(DBG_XMIT_RCV_DUMP, "orig: %s\n", dump(buf, len)));
}

static void readable(EventSelector *es, int fd, unsigned int flags, void *data)
{
    unsigned char buf[MAX_PACKET_LEN+EXTRA_HEADER_ROOM];
    int n;
    l2tp_session *ses = (l2tp_session *)data;
    rg_session_context_t *cont;
    int iters = 5;

    /* It seems to be better to read in a loop than to go
     * back to select loop.  However, don't loop forever, or
     * we could have a DoS potential */
    while (iters--)
    {
	n = read(fd, buf + EXTRA_HEADER_ROOM, MAX_PACKET_LEN);

	if (n < 0)
	{
	    if (errno != EIO)
		continue;
	    return;
	}

	DBG(l2tp_db(DBG_XMIT_RCV, 
	    "readable() got from pppd %d octets (%d-%s)\n",
	    n, errno, strerror(errno)));
	DBG(l2tp_db(DBG_XMIT_RCV_DUMP, "readable(): %s\n", dump(buf, n)));
	errno = 0;

	/* TODO: Check this.... */
	if (n <= 2)
	    return;

	if (!ses)
	    continue;

	cont = ses->private;

	l2tp_dgram_send_ppp_frame(ses, buf+EXTRA_HEADER_ROOM, n); 
    }
}

