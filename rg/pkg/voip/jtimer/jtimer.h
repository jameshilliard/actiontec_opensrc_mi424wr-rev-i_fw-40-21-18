/****************************************************************************
 *
 * rg/pkg/voip/jtimer/jtimer.h
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

#ifndef _JTIMER_H_
#define _JTIMER_H_

#define JTIMER_TIMERCONFIG _IOW(RG_IOCTL_PREFIX_JTIMER, 47, int)
#define JTIMER_GETEVENT _IOR(RG_IOCTL_PREFIX_JTIMER, 8, int)
#define JTIMER_TIMERPING _IOW(RG_IOCTL_PREFIX_JTIMER, 42, int)
#define JTIMER_TIMERACK _IOW(RG_IOCTL_PREFIX_JTIMER, 48, int)
#define JTIMER_TIMERPONG _IOW(RG_IOCTL_PREFIX_JTIMER, 53, int)
#define JTIMER_EVENT_TIMER_EXPIRED 15
#define JTIMER_EVENT_TIMER_PING 16
#define JTIMER_EVENT_NONE 0

#endif
