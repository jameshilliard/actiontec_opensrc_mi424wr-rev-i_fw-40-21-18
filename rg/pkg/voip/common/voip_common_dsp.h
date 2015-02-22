/****************************************************************************
 *
 * rg/pkg/voip/common/voip_common_dsp.h
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

#ifndef _VOIP_COMMON_DSP_H_
#define _VOIP_COMMON_DSP_H_

typedef struct {
    void *data;
    int size;
} hw_config_data_t;

#define VOIP_DSP_CONF _IOWR(RG_IOCTL_PREFIX_VOIP, 0, hw_config_data_t)

#endif
