/****************************************************************************
 *
 * rg/pkg/util/log_entity_ext_proc.c
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

#include "log_entity_ext_proc.h"
#include "log_entity_id.h"
#include "str.h"
#include <syslog.h>

static char *full_ident;

int rg_openlog_initialized;

void rg_openlog_full(log_entity_id_t entity_id, const char *ident, int option,
    int facility)
{
    /* The string must be dynamically allocated, because openlog() doesn't
     * duplicate it */
    str_printf(&full_ident, "%d.%s", entity_id, ident ? ident : "unknown");
    openlog(full_ident, option, facility);
    rg_openlog_initialized = 1;
}

void rg_closelog(void)
{
    closelog();
    str_free(&full_ident);
    rg_openlog_initialized = 0;
}

