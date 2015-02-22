/****************************************************************************
 *
 * rg/pkg/include/bio_socket.h
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

#ifndef _BIO_SOCKET_H_
#define _BIO_SOCKET_H_

#define OS_INCLUDE_STD
#include <os_includes.h>
/* Note this code is GPL Licence, so do not add any new 
 * openrg incude files here. Talk to TL or SA before any change
 */

int bio_accept(int server_fd);
int bio_write(int fd, void *buf, size_t count);
int bio_read(int fd, void *buf, size_t count);

#endif
