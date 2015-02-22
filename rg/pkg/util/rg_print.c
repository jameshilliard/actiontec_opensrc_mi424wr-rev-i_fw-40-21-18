/****************************************************************************
 *
 * rg/pkg/util/rg_print.c
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

#define OS_INCLUDE_STD
#define OS_INCLUDE_STDARG
#include <os_includes.h>
#include <rg_def.h>
#include <util/rg_error.h>
#include "rg_print.h"

int snprintf_l(char *str, size_t size, const char *format, ...)
{
    int retval;
    va_list ap;

    va_start(ap, format);
    retval = vsnprintf(str, size, format, ap);
    if (retval<0)
	rg_error(LERR, "String '%s' truncated", str);
    va_end(ap);
    return retval;
}

int console_printf(const char *format, ...)
{
    va_list args;
    int rv;
    FILE *fp;

    fp = fopen(RG_OS_CONSOLE, "w");
    if (!fp)
	return -1;

    va_start(args, format);
    rv = vfprintf(fp, format, args);
    va_end(args);

    fclose(fp);

    return rv;
}

