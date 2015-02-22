/****************************************************************************
 *
 * rg/pkg/freeswan/klips/net/ipsec/alg/ipsec_alg_static_init.c
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

#include <linux/kernel.h>
#include "../ipsec_alg.h"

void ipsec_alg_static_init(void)
{ 
    int __attribute__ ((unused)) err=0;

#ifdef CONFIG_IPSEC_ALG_STATIC
#ifdef CONFIG_IPSEC_ALG_1DES
    {
	extern int ipsec_1des_init(void);

	ipsec_1des_init();
    }
#endif
#ifdef CONFIG_IPSEC_ALG_3DES
    {
	extern int ipsec_3des_init(void);

	ipsec_3des_init();
    }
#endif
#ifdef CONFIG_IPSEC_ALG_NULL
    {
	extern int ipsec_null_init(void);

	ipsec_null_init();
    }
#endif
#ifdef CONFIG_IPSEC_ALG_AES
    {
	extern int ipsec_aes_init(void);

	ipsec_aes_init();
    }
#endif
#ifdef CONFIG_IPSEC_ALG_MD5
    {
	extern int ipsec_md5_init(void);

	ipsec_md5_init();
    }
#endif
#ifdef CONFIG_IPSEC_ALG_MD5
    {
	extern int ipsec_sha1_init(void);

	ipsec_sha1_init();
    }
#endif
#endif
}
