/****************************************************************************
 *
 * rg/pkg/util/bio_socket.c
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

#define OS_INCLUDE_SOCKET
#define OS_INCLUDE_INET
#include <os_includes.h>

#include <bio_socket.h>
/* Note this code is GPL Licence, so do not add any new 
 * openrg incude files here. Talk to TL or SA before any change
 */

int bio_accept(int server_fd)
{
    struct sockaddr_in dummy_addr;
    int fd, dummy_addr_len = sizeof(dummy_addr);

    /* In VxWorks accept() can't receive NULL as its second parameter, this is
     * why we use the dummy buffer even though we don't care for the value it
     * receives
     */
    while ((fd = accept(server_fd, (struct sockaddr *)&dummy_addr,
	&dummy_addr_len)) < 0)
    {
	if (errno != EINTR && errno != EAGAIN) 
	    return -1;
    }
    return fd;
}

/* This function is blocking and it writes from the socket exactly 'count'
 * number of bytes of fails on socket error
 */
int bio_write(int fd, void *buf, size_t count)
{
    int rc, total = 0;
    
    while (total < count)
    {
	rc = write(fd, buf + total, count - total);
	if (rc < 0)
	{
	    if (errno == EINTR || errno == EAGAIN)
		continue;
	    return -1;
	}
	else if (!rc)
	    return -1;

	total += rc;
    }
    
    return 0;
}

/* This function is blocking and it reads from the socket count bytes */
int bio_read(int fd, void *buf, size_t count)
{
    int rc, total = 0;

    while (total!=count)
    {
	rc = read(fd, buf + total, count - total);
	if (rc < 0)
	{
	    if (errno == EINTR || errno == EAGAIN)
		continue;
	    return -1;
	}
	else if (!rc)
	    return -1;

	total += rc;
    }
    
    return 0;
}
