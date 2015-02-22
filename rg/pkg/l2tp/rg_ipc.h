/****************************************************************************
 *
 * rg/pkg/l2tp/rg_ipc.h
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

#ifndef _RG_IPC_H_
#define _RG_IPC_H_

#include <ipc.h>

typedef enum {
    /* Start connect to specified server.
     * Parameters:
     *   u32     - connection ID,
     *   u32     - server IP,
     *   u32     - shared key length (shared_len),
     *   [char * - shared key (shared_len bytes); it is sent only if shared_len
     *             is greater than 0]
     */
    L2TP_RG2D_CLIENT_CONNECT = 1,
    /* Attach PPP device.
     * Parameters:
     *   u32 - connection ID,
     *   char * - PPP device name
     */
    L2TP_RG2D_ATTACH = 2,
    /* Detach PPP device.
     * Parameter:
     *   u32 - connection ID.
     */
    L2TP_RG2D_DETACH = 3,
    /* Close active connection.
     * Parameter:
     *   u32 - connection ID,
     */
    L2TP_RG2D_CLOSE = 4,
    /* Start negotiation with unaccepted requester. No need to send shared
     * secret, because it is one for all incoming L2TP connections and is set at
     * L2TP_RG2D_SERVER_START message.
     * Parameters:
     *   u32     - unaccepted requeset ID,
     *   u32     - connection ID,
     */
    L2TP_RG2D_SERVER_CONNECT = 5,
    /* Start L2TP server.
     * Parameters:
     *   u32     - shared key length (shared_len),
     *   [char * - shared key (shared_len bytes); it is sent only if shared_len
     *             is greater than 0]
     */
    L2TP_RG2D_SERVER_START = 6,
    /* Stop L2TP server.
     * No parameters.
     */
    L2TP_RG2D_SERVER_STOP = 7,
    /* New connection request arrives notification.
     * The command is sent to L2TP task.
     * Parameter:
     *   u32 - new request (unaccepted) ID.
     */
    L2TP_D2RG_NEW_REQUEST = 8,
    /* Reject incoming request.
     * The command is sent by L2TP server task to the daemon.
     * Parameter:
     *   u32            - the request (unaccepted) ID.
     *   struct in_addr - the requester IP.
     */
    L2TP_RG2D_REJECT_REQUEST = 9,
    /* Establish connection notification.
     * The command is sent to L2TP plugin.
     * Parameter:
     *   u32 - connection ID.
     */
    L2TP_D2RG_CONNECTED = 10,
    /* Update binded IPs */
    L2TP_RG2D_UPDATE_IPS = 11,
} l2tpd_cmd_t;

#endif

