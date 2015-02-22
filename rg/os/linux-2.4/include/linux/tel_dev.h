/****************************************************************************
 *
 * rg/os/linux-2.4/include/linux/tel_dev.h
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

#ifndef _TEL_DEV_H_
#define _TEL_DEV_H_

#include <linux/ioctl.h>

#define TEL_DEV "/dev/tel"

/* ioctl() operations */

/* Get number of available extensions */
#define TEL_GET_EXT_NUM _IOR('g', 0x01, unsigned int)
/* Get extension hook state (on/off hook) */
#define TEL_GET_EXT_HOOK_STATE _IOWR('g', 0x02, unsigned int)

typedef enum {
    TEL_ON_HOOK = 0,
    TEL_OFF_HOOK = 1,
} tel_ext_hook_state_t;

#endif

