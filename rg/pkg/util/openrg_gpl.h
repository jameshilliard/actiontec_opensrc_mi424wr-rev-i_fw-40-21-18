/****************************************************************************
 *
 * rg/pkg/util/openrg_gpl.h
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
 */

#ifndef _OPENRG_GPL_H_
#define _OPENRG_GPL_H_

#include <rg_types.h>

#ifndef MZERO
#define MZERO(buf) memset(&(buf), 0, sizeof(buf))
#endif

#include <util/rg_error.h>
#include <util/log_entity_ext_proc.h>

void sys_sleep(unsigned int seconds);
int sock_socket(int type, u32 src_ip, u16 src_port);
void socket_close(int fd);
int gpl_sys_rg_chrdev_open(int type, int mode);

#endif
