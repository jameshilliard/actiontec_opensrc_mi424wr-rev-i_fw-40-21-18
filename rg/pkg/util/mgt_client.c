/****************************************************************************
 *
 * rg/pkg/util/mgt_client.c
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define MGT_PORT_LOOPBACK 7019
#define MIN_STR_LEN 16

/* Connection context
 */
typedef struct {
    int sock;
} mgt_connection_t;

static int mgt_write(mgt_connection_t *conn, void *data, int len)
{
    int ret;

    ret = write(conn->sock, data, len);

    return ret;
}

static int mgt_read(mgt_connection_t *conn, void *data, int len)
{
    int ret;

    ret = read(conn->sock, data, len);

    return ret;
}

static void str_putc(char **s, int *len, char c)
{
    int new_len = 0;

    if (!*len)
	new_len = MIN_STR_LEN;
    else if (*s && strlen(*s) + 1 >= *len) 
	new_len = *len * 2;

    if (new_len)
    {
	char *news = realloc(*s, new_len);
	if (!news)
	    return;

	*s = news;
	memset(*s + *len, 0, new_len - *len);
	(*len) = new_len;
    }

    memcpy(*s + strlen(*s), &c, 1);
}

/* Close a connection object
 */
static void mgt_close(mgt_connection_t *conn)
{
    if (conn->sock >= 0)
	close(conn->sock);
    free(conn);
}

/* Open a connection with a CPE device
 */
static mgt_connection_t *mgt_open(void)
{
    struct sockaddr_in sa;
    mgt_connection_t *conn;
    unsigned short port = MGT_PORT_LOOPBACK;
    in_addr_t ip = inet_addr("127.0.0.1");
    
    if (!(conn = malloc(sizeof(mgt_connection_t))))
    {
	printf("malloc failed\n");
	return NULL;
    }

    /* Create a socket */
    if ((conn->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	printf("socket failed\n");
	goto Error;
    }

    /* Connect to the socket */
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = ip;
    sa.sin_port = htons(port);
    
    if (connect(conn->sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
	printf("connect to socket %d failed\n", conn->sock);
	goto Error;
    }

    return conn;

Error:
    mgt_close(conn);

    return NULL;
}

/* Execute a command on an existing connection and receive the results */
int mgt_command(char *cmd, char **result)
{
    char c;
    char *rc_str = NULL;
    int rc = -1;
    int len = 0;
    mgt_connection_t *conn;

    if (!cmd || !*cmd)
	return -1;

    if (!(conn = mgt_open()))
	return -1;
    
    mgt_write(conn, cmd, strlen(cmd));
    mgt_write(conn, "\0", 1);

    while (mgt_read(conn, &c, 1) == 1 && c)
	str_putc(result, &len, c);
    len = 0;
    while (mgt_read(conn, &c, 1) == 1 && c)
	str_putc(&rc_str, &len, c);

    if (rc_str)
    {
	rc = atoi(rc_str);
	free(rc_str);
    }

    mgt_close(conn);
    return rc;
}

