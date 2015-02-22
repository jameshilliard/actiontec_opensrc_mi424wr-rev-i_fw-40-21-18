/****************************************************************************
 *
 * rg/pkg/include/ipc.h
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

#ifndef _IPC_H_
#define _IPC_H_

#define OS_INCLUDE_INET
#include <os_includes.h>
#include <rg_types.h>

#define RG_IPC_PORT_BASE 7000

#define RG_IPC_PORT_OPENRG RG_IPC_PORT_BASE+0
/* Port used for connection with OpenRG CLI from external task/thread
 * when OpenRG does not control the console. (e.g. under VxWorks) */
#define CLI_SERVER_PORT RG_IPC_PORT_BASE+1

/* used for autotest for executing commands remotely */
#define RG_IPC_PORT_AUTOTEST_AGENT RG_IPC_PORT_BASE+2

#define RG_IPC_PORT_PPTPS RG_IPC_PORT_BASE+11
#define RG_IPC_PORT_IPSEC RG_IPC_PORT_BASE+12
#define RG_IPC_PORT_SYNCT RG_IPC_PORT_BASE+13
#define RG_IPC_PORT_X509 RG_IPC_PORT_BASE+14
#define RG_IPC_PORT_L2TPD_2_MT RG_IPC_PORT_BASE+15
#define RG_IPC_PORT_MT_2_L2TPD RG_IPC_PORT_BASE+16
#define RG_IPC_PORT_PTHREAD_VOIP_TASK RG_IPC_PORT_BASE+17
#define RG_IPC_PORT_MAIN_VOIP_TASK RG_IPC_PORT_BASE+18
#define RG_IPC_PORT_RMT_MNG_LOOPBACK RG_IPC_PORT_BASE+19 /* Don't change !!! */
#define RG_IPC_PORT_RMT_MNG RG_IPC_PORT_BASE+20 /* Don't change !!! */
#define RG_IPC_PORT_RMT_MNG_SSL RG_IPC_PORT_BASE+21 /* Don't change !!! */
#define RG_IPC_PORT_JOSIP RG_IPC_PORT_BASE+22
#define RG_IPC_PORT_PTHREAD_LOCK_VOIP_TASK RG_IPC_PORT_BASE+23
#define RG_IPC_PORT_L2TPD_2_TASK RG_IPC_PORT_BASE+24
#define RG_IPC_PORT_UPNP_AV_INDEXER RG_IPC_PORT_BASE+25
#define RG_IPC_PORT_UPNP_AV_INDEXER_EVENT RG_IPC_PORT_BASE+26
#define RG_IPC_PORT_WSC RG_IPC_PORT_BASE+27
#ifdef ACTION_TEC_TR143
#define RG_IPC_PORT_TR143 RG_IPC_PORT_BASE+28
#define RG_IPC_PORT_UDHECHO_STAT RG_IPC_PORT_BASE+29
#define RG_IPC_PORT_TESTCLIENT RG_IPC_PORT_BASE+30
#endif
#define RG_IPC_PORT_EAP_WPS RG_IPC_PORT_BASE+31

/* Low level IPC functions */
int ipc_connect(u16 port);
int ipc_connect_quiet(u16 port); /* Do not issue error on connect() failure */
int ipc_connect_ip(u16 port, u32 addr);
int ipc_listen(u16 port);
int ipc_listen_ip(u16 port, u32 addr);
int ipc_accept(int server_fd);
int ipc_write(int fd, void *buf, size_t count);
int ipc_read(int fd, void *buf, size_t count);

/* Close IPC connection from the client's side. 'last_error' is the last error
 * code received by previous IPC operation. It should be 0 upon success
 */
int ipc_client_close(int fd, int last_error);

/* Close IPC connection from the server's side. 'last_error' is the last error
 * code received by previous IPC operation. It should be 0 upon success
 */
int ipc_server_close(int fd, int last_error);
int ipc_server_sync(int fd);
int ipc_client_sync(int fd);

/* IPC stream helpers */
int ipc_u32_read(int fd, u32 *n);
int ipc_u32_write(int fd, u32 n);
int ipc_varbuf_read(int fd, char **data, int *len);
int ipc_varbuf_write(int fd, char *data, int len);
char *ipc_string_read(int fd);
int ipc_string_write(int fd, char *str);

/* Open socket on a specific port for reading.
 * port is in network order (call htons before calling this function).
 */
int ipc_bind_listen_port(u16 port);

/* For reading data from a socket - use it in the module task. 
 * Returns how many bytes were read, -1 for error (similar to read())
 */
int ipc_accept_buf_read(int sock, void *data, int data_len);

/* For writing data to a specific port - use it in the external process. 
 * port is in network order (call htons before calling this function).
 * Returns number of bytes written, -1 for error (similar to write())
 */
int ipc_connect_buf_write(u16 port, void *data, int data_len);

/* Busy waits until the IPC server starts accepting connections. */
int ipc_wait_for_server(u16 port);

#endif

