/****************************************************************************
 *
 * rg/pkg/hostapd/hostapd/eap_stubs.c
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include "hostapd.h"
#include "eap.h"

struct eap_sm * eap_sm_init(void *eapol_ctx,
			    struct eapol_callbacks *eapol_cb,
                            struct eap_config *eap_conf)
{
	return NULL;
}

void eap_sm_deinit(struct eap_sm *sm)
{
}

int eap_sm_step(struct eap_sm *sm)
{
	return 0;
}

u8 eap_get_type(const char *name)
{
	return EAP_TYPE_NONE;
}

void eap_set_eapRespData(struct eap_sm *sm,
			 const u8 *eapRespData,
			 size_t eapRespDataLen)
{
}

void eap_sm_notify_cached(struct eap_sm *sm)
{
}

