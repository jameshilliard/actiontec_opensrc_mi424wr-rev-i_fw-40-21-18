/***********************************************************************
*
* network.c
*
* Code for handling the UDP socket we send/receive on.  All of our
* tunnels use a single UDP socket which stays open for the life of
* the application.
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

#include "l2tp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

typedef enum l2tp_sock_state_t {
    SOCK_INVALID,
    SOCK_VALID,
    SOCK_NEW
} l2tp_sock_state_t;

/* Our socket */
typedef struct l2tp_sock {
    struct l2tp_sock *next;
    int fd;
    l2tp_sock_state_t state;
    struct in_addr src;
    EventHandler *eh;
} l2tp_sock_t;
l2tp_sock_t *sock_head;

static void network_readable(EventSelector *es,
			     int fd,
			     unsigned int flags,
			     void *data);
char Hostname[MAX_HOSTNAME];

int l2tp_socket_is_valid(int fd)
{
    l2tp_sock_t *cur;

    for (cur = sock_head; cur && cur->fd != fd; cur = cur->next);
    return cur ? cur->state == SOCK_VALID : 0;
}

uint32_t l2tp_get_ip_by_fd(int fd)
{
    l2tp_sock_t *cur;

    for (cur = sock_head; cur && cur->fd != fd; cur = cur->next);
    return cur ? cur->src.s_addr : 0;
}

static l2tp_sock_t *l2tp_get_sock_by_ip(struct in_addr ip)
{
    l2tp_sock_t *cur;

    for (cur = sock_head; cur; cur = cur->next)
    {
	if (cur->src.s_addr == ip.s_addr)
	    break;
    }
    return cur;
}

int l2tp_get_fd_by_ip(struct in_addr ip)
{
    l2tp_sock_t *sock = l2tp_get_sock_by_ip(ip);

    if (sock && sock->state == SOCK_VALID)
	return sock->fd;
    return -1;
}

static void l2tp_sock_free(l2tp_sock_t **sock)
{
    l2tp_sock_t *cur = *sock;

    *sock = cur->next;
    if (cur->fd >= 0)
	close(cur->fd);
    free(cur);
}

static l2tp_sock_t *l2tp_sock_alloc(struct in_addr src)
{
    l2tp_sock_t *cur, **tmp;

    cur = calloc(1, sizeof(l2tp_sock_t));
    if (!cur)
	return NULL;

    cur->fd = -1;
    cur->src.s_addr = src.s_addr;
    cur->state = SOCK_NEW;

    for (tmp = &sock_head; *tmp; tmp = &(*tmp)->next);
    *tmp = cur;

    return cur;
}

static void
sigint_handler(int sig)
{
    static int count = 0;

    count++;
    fprintf(stderr, "In sigint handler: %d\n", count);
    if (count < 5) {
	l2tp_cleanup();
    }
    exit(1);
}

/**********************************************************************
* %FUNCTION: network_init
* %ARGUMENTS:
*  es -- an event selector
* %RETURNS:
*  >= 0 if all is OK, <0 if not
* %DESCRIPTION:
*  Initializes network; opens socket on UDP port 1701; sets up
*  event handler for incoming packets.
***********************************************************************/
int
l2tp_network_init(EventSelector *es)
{
    gethostname(Hostname, sizeof(Hostname));
    Hostname[sizeof(Hostname)-1] = 0;

    Event_HandleSignal(es, SIGINT, sigint_handler);
    return 0;
}

static int l2tp_check_sockets(EventSelector *es)
{
    struct sockaddr_in me;
    int flags;
    l2tp_sock_t *cur, **l2tp_sock = &sock_head;

    /* Close all tunnels with invalid socket. */
    l2tp_tunnel_stop_invalid();
    /* Check sockets list:
     *   - remove invalid sockets;
     *   - initiate new sockets.
     */
    while (*l2tp_sock)
    {
	int opt = 1;

	if ((*l2tp_sock)->state == SOCK_VALID)
	{
	    l2tp_sock = &(*l2tp_sock)->next;
	    continue;
	}
	if ((*l2tp_sock)->state == SOCK_INVALID)
	{
	    if ((*l2tp_sock)->eh)
		Event_DelHandler(es, (*l2tp_sock)->eh);
	    l2tp_sock_free(l2tp_sock);
	    continue;
	}
	/* New socket. */
	cur = *l2tp_sock;
	cur->fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (cur->fd < 0 || setsockopt(cur->fd, SOL_SOCKET, SO_REUSEADDR,
	    (void *)&opt, sizeof(opt)) < 0)
	{
	    l2tp_set_errmsg("network_init: socket: %s", strerror(errno));
	    return -1;
	}
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = cur->src.s_addr;
	me.sin_port = htons((uint16_t) Settings.listen_port);
	if (Settings.listen_port &&
	    bind(cur->fd, (struct sockaddr *) &me, sizeof(me)) < 0) {
	    l2tp_set_errmsg("network_init: bind: %s", strerror(errno));
	    return -1;
	}
	/* Set socket non-blocking */
	flags = fcntl(cur->fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(cur->fd, F_SETFL, flags);

	/* Set up the network read handler */
	cur->eh = Event_AddHandler(es, cur->fd, EVENT_FLAG_READABLE,
	    network_readable, NULL);
	if (!cur->eh)
	    return -1;
	cur->state = SOCK_VALID;
	l2tp_sock = &cur->next;
    }
    return 0;
}

/* Received new WAN IPs list. Add new sockets and remove sockets with wrong IP.
 */
int
l2tp_socket_init(EventSelector *es, struct in_addr *srcs, int nsrcs)
{
    int i;
    l2tp_sock_t *l2tp_sock;

    for (l2tp_sock = sock_head; l2tp_sock; l2tp_sock = l2tp_sock->next)
	l2tp_sock->state = SOCK_INVALID;

    for (i = 0; i < nsrcs; i++)
    {
	l2tp_sock = l2tp_get_sock_by_ip(srcs[i]);
	if (l2tp_sock)
	{
	    l2tp_sock->state = SOCK_VALID;
	    continue;
	}
	l2tp_sock = l2tp_sock_alloc(srcs[i]);

	if (!l2tp_sock)
	    goto Error;
    }
    if (l2tp_check_sockets(es))
	goto Error;

    return 0;

Error:
    /* If any error, then remove all sockets and close all tunnels. */
    for (l2tp_sock = sock_head; l2tp_sock; l2tp_sock = l2tp_sock->next)
	l2tp_sock->state = SOCK_INVALID;
    l2tp_check_sockets(es);
    return -1;
}

/**********************************************************************
* %FUNCTION: network_readable
* %ARGUMENTS:
*  es -- event selector
*  fd -- socket
*  flags -- event-handling flags telling what happened
*  data -- not used
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Called when a packet arrives on the UDP socket.
***********************************************************************/
static void
network_readable(EventSelector *es,
		 int fd,
		 unsigned int flags,
		 void *data)
{
    l2tp_dgram *dgram;

    struct sockaddr_in from;
    dgram = l2tp_dgram_take_from_wire(fd, &from);
    if (!dgram) return;

    /* It's a control packet if we get here */
    l2tp_tunnel_handle_received_control_datagram(fd, dgram, es, &from);
    l2tp_dgram_free(dgram);
}
