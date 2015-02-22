/****************************************************************************
 *
 * rg/pkg/freeswan/klips/net/ipsec/ipsec_log.h
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

#ifndef _IPSEC_LOG_H_
#define _IPSEC_LOG_H_

#include <linux/kernel.h>

struct iphdr;
extern int debug_log_all;
extern void ipsec_print_ip(struct iphdr *ip);

/* Return not 0 if allow log message, 0 otherwise */
int ipsec_rate_limit(void);

void ipsec_log(char *format, ...);
void klips_print_more(int is_start, char *format, ...);
void klips_print_more_finish(void);

#ifdef CONFIG_IPSEC_DEBUG
    #define KLIPS_PRINT(flag, format, args...) \
        ((flag) ? ipsec_log(KERN_INFO format , ## args) : 0)
    #define KLIPS_PRINTMORE_START(flag, format, args...) \
	((flag) ? klips_print_more(1, format , ## args) : 0)
    #define KLIPS_PRINTMORE(flag, format, args...) \
	((flag) ? klips_print_more(0, format , ## args) : 0)
    #define KLIPS_PRINTMORE_FINISH(flag) \
        ((flag) ? klips_print_more_finish() : 0)
    #define KLIPS_IP_PRINT(flag, ip) \
	((flag) && ipsec_rate_limit() ? ipsec_print_ip(ip) : 0)
#else
    #define KLIPS_PRINT(flag, format, args...) do ; while(0)
    #define KLIPS_PRINTMORE_START(flag, format, args...) do ; while(0)
    #define KLIPS_PRINTMORE(flag, format, args...) do ; while(0)
    #define KLIPS_PRINTMORE_FINISH(flag) do ; while(0)
    #define KLIPS_IP_PRINT(flag, ip) do ; while(0)
#endif

#endif
