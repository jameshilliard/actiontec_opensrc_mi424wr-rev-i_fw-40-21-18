/****************************************************************************
 *
 * rg/os/linux-2.6/include/linux/if_ipsec.h
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

#ifndef _LINUX_IF_IPSEC_H_
#define _LINUX_IF_IPSEC_H_

#ifdef __KERNEL__

extern void ipsec_ioctl_hook_set(int (*hook)(unsigned long));

#endif

/* Passed in vlan_ioctl_args structure to determine behaviour. */
typedef enum ipsec_ioctl_cmd {
    ADD_IPSEC_DEV_CMD = 0,
    DEL_IPSEC_DEV_CMD = 1,
    IPSEC_ANTI_REPLAY_CMD = 2,
} ipsec_ioctl_cmd;

struct ipsec_ioctl_args {
    ipsec_ioctl_cmd cmd;
    char dev_name[IFNAMSIZ];
};

#endif
