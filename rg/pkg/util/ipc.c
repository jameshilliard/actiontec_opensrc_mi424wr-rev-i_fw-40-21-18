/****************************************************************************
 *
 * rg/pkg/util/ipc.c
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

#define OS_INCLUDE_INET
#define OS_INCLUDE_SOCKET
#define OS_INCLUDE_IO
#include <os_includes.h>
#include <util/alloc.h>
#include <rg_os.h>

/* OpenRG GPL includes */
#include <bio_socket.h>
#include <ipc.h>
#include "memory.h"
#include "openrg_gpl.h"
#ifdef __OS_VXWORKS__
#include "vx_net_fixup.h"
#endif
/* Note this code is GPL Licence, so do not add any new 
 * openrg incude files here. Talk to TL or SA before any change
 */

#define SYNC_STR "sync"

#define IPC_WAIT_TRIES 3000 /* 30 seconds */

int ipc_wait_for_server(u16 port)
{
    struct sockaddr_in sa;
    int fd, i, rc;

    MZERO(sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr("127.0.0.1");
    sa.sin_port = port;

    if ((fd = sock_socket(SOCK_STREAM, 0, 0)) < 0)
	return rg_error(LERR, "Failed creating socket");
    
    /* Try to connect to server infinitely */ 
    for (i = IPC_WAIT_TRIES; i; i--)
    {
 	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) >= 0)
	    break;

	/* Sleep for 10ms before trying again */
	usleep(10000);
    }

    /* Once the server accepts the connection, it tries to send the client a
     * sync message. It will fail if we close the socket now. So we keep the
     * socket open until a sync is received, to keep the server happy. */
    rc = !i || ipc_client_sync(fd);

    socket_close(fd);
    
    return rc;
}

/* Low level TCP/IPC */
int ipc_connect(u16 port)
{
    return ipc_connect_ip(port, inet_addr("127.0.0.1"));
}
    
static int ipc_connect_ip_ex(u16 port, u32 addr, int quiet_conn_fail)
{
    struct sockaddr_in sa;
    int fd = -1;

    while (1)
    {
	if ((fd = sock_socket(SOCK_STREAM, 0, 0)) < 0)
	    return -1;
	
	MZERO(sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = addr;
	sa.sin_port = port;
 	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
	{
	    socket_close(fd);
	    if (!quiet_conn_fail)
		rg_error(LERR, "Failed ipc connect %m");
	    return -1;
	}
	/* Linux 2.4 kernel has a bug with localhost connections.
	 * After the server calls listen(), then the client calls
	 * connect() and returns with success immediately, before
	 * the server calls accept()!
	 * Therefore, a SYNC_STR command is used to make sure the connect
	 * succeeded.
	 * This has not been tested on Linux 2.2 whether it too has
	 * this bug. It probably does not exist on VxWorks, since VxWorks
	 * TCP/IP stack is completely different (its BSD based).
	 */
	if (!ipc_client_sync(fd))
	    break;
	socket_close(fd);
	rg_error(LWARN, "Failed ipc connect sync - retrying");
	sys_sleep(1);
    }

    return fd;
}

int ipc_connect_ip(u16 port, u32 addr)
{
    return ipc_connect_ip_ex(port, addr, 0);
}

int ipc_connect_quiet(u16 port)
{
    return ipc_connect_ip_ex(port, inet_addr("127.0.0.1"), 1);
}

int ipc_listen(u16 port)
{
    return ipc_listen_ip(port, inet_addr("127.0.0.1"));
}

int ipc_listen_ip(u16 port, u32 addr)
{
    int fd;
    
    if ((fd = sock_socket(SOCK_STREAM, addr, port)) < 0)
	return -1;
    /* 5 is a big enough queue length */
    if (listen(fd, 5) < 0)
    {
	rg_error(LERR, "Failed ipc listen %m");
	goto Error;
    }
    return fd;

Error:
    socket_close(fd);
    return -1;
}

int ipc_accept(int server_fd)
{
    int fd;

    if ((fd = bio_accept(server_fd)) < 0)
	return rg_error(LERR, "%d: failed ipc accept %m", server_fd);
    if (ipc_server_sync(fd))
    {
	rg_error(LERR, "%d: failed ipc accept sync", server_fd);
	goto Error;
    }
    return fd;

Error:
    socket_close(fd);
    return -1;
}

int ipc_client_close(int fd, int last_error)
{
    int ret = 0;
    
    if (!last_error)
	ret = ipc_client_sync(fd);
    socket_close(fd);
    return ret;
}

int ipc_server_close(int fd, int last_error)
{
    int ret = 0;
    
    if (!last_error)
	ret = ipc_server_sync(fd);
    socket_close(fd);
    return ret;
}

int ipc_client_sync(int fd)
{
    char *sync = NULL;
    int rc = -1;

    if (ipc_string_write(fd, SYNC_STR))
	goto Exit;
    if (!(sync = ipc_string_read(fd)) || strcmp(sync, SYNC_STR))
	goto Exit;
    rc = 0;

Exit:
    if (rc)
	rg_error(LERR, "%d: failed ipc client sync", fd);
    nfree(sync);
    return rc;
}

int ipc_server_sync(int fd)
{
    char *sync = NULL;
    int rc = -1;

    if (!(sync = ipc_string_read(fd)) || strcmp(sync, SYNC_STR))
	goto Exit;
    if (ipc_string_write(fd, SYNC_STR))
	goto Exit;
    rc = 0;

Exit:
    if (rc)
	rg_error(LERR, "%d: failed ipc server sync", fd);
    nfree(sync);
    return rc;
}

/* This function is blocking and it writes from the socket exactly 'count'
 * number of bytes
 */
int ipc_write(int fd, void *buf, size_t count)
{
    int rc;
 
    if ((rc = bio_write(fd, buf, count)))
	rg_error_f(LERR, "%d: failed write() %m", fd);
    return rc;
}

int ipc_read(int fd, void *buf, size_t count)
{
    return bio_read(fd, buf, count);
}

int ipc_u32_read(int fd, u32 *n)
{
    if (ipc_read(fd, n, sizeof(*n)))
	return -1;
    *n = ntohl(*n);
    return 0;
}

int ipc_u32_write(int fd, u32 n)
{
    n = htonl(n);
    return ipc_write(fd, &n, sizeof(n));
}

int ipc_varbuf_read(int fd, char **data, int *len)
{
    if (ipc_u32_read(fd, len))
	return -1;
    if (!(*data = malloc_l(*len)))
	goto Error;
    if (ipc_read(fd, *data, *len))
	goto Error;
    return 0;

Error:
    nfree(*data);
    return -1;
}

int ipc_varbuf_write(int fd, char *data, int len)
{
    if (ipc_u32_write(fd, len))
	return -1;
    if (ipc_write(fd, data, len))
	return -1;
    return 0;
}

char *ipc_string_read(int fd)
{
    char *data;
    int len;
    
    if (ipc_varbuf_read(fd, &data, &len) || !len)
	return NULL;
    /* '\0' should be sent by the other side, we want to be on the safe side */
    data[len-1] = 0;
    return data;
}

int ipc_string_write(int fd, char *str)
{
    return ipc_varbuf_write(fd, str, strlen(str) + 1);
}

/* High level simple API */

/* Open socket on a specific port for reading. */
int ipc_bind_listen_port(u16 port)
{
    return ipc_listen(port);
}

/* For reading data from a socket - use it in the module task. */
int ipc_accept_buf_read(int server_fd, void *data, int data_len)
{
    int fd, rc;
    
    if ((fd = ipc_accept(server_fd)) < 0)
	return -1;
    rc = ipc_read(fd, data, data_len);
    ipc_server_close(fd, rc);
    return rc;
}

/* For writing data to a specific port - use it in the external process. */
int ipc_connect_buf_write(u16 port, void *data, int data_len)
{
    int fd, rc;

    if ((fd = ipc_connect(port)) < 0)
	return -1;
    rc = ipc_write(fd, data, data_len);
    ipc_client_close(fd, rc);
    return rc;
}

