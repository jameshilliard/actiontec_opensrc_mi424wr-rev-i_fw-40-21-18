/*************************************************************************
Copyright (c) 2006 Cavium Networks (support@cavium.com). All rights
reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. Cavium Networks' name may not be used to endorse or promote products
derived from this software without specific prior written permission.

This Software, including technical data, may be subject to U.S. export
control laws, including the U.S. Export Administration Act and its
associated regulations, and may be subject to export or import
regulations in other countries. You warrant that You will comply
strictly in all respects with all such regulations and acknowledge that
you have the responsibility to obtain licenses to export, re-export or
import the Software.

TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
AND WITH ALL FAULTS AND CAVIUM MAKES NO PROMISES, REPRESENTATIONS OR
WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY)
WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. THE ENTIRE
RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.

*************************************************************************/

/**  File defining different Octeon model IDs and macros
 *   to compare them.
 * $Id: octeon-model.h,v 1.1.2.1 2007/08/17 03:14:06 jungo Exp $
 */

#ifndef __OCTEON_MODEL_H__
#define __OCTEON_MODEL_H__

/* Defines to represent the different versions of Octeon.  */
#define OCTEON_CN38XX_PASS1     0x000d0000 
#define OCTEON_CN38XX_PASS2     0x000d0001   
#define OCTEON_PASS1        	OCTEON_CN38XX_PASS1   /* First Octeon Chip */
#define OCTEON_CN38XX       	OCTEON_CN38XX_PASS2   /* The official name for 16 core/1MByte L2 Octeon */
#define OCTEON_PASS2        	OCTEON_CN38XX_PASS2   /* Second Pass Octeon Chip */
#define OCTEON_CN36XX       	OCTEON_CN38XX_PASS2   /* Octeon 512KB L2, 1 interface */
#define OCTEON_CN31XX       	0x000d0100   /* Two core Octeon with USB */
#define OCTEON_CN30XX       	0x000d0200   /* Single core Octeon */



/* This matches models regargless of the revision.  This should be used for any model
 * ** differentiation that does not care about revision.  (Which should mostly be errata
 * ** related.) */
#define OCTEON_MODEL_MASK   0x00ffff00
#define OCTEON_MATCH_MODEL(x,y)     (((x) & OCTEON_MODEL_MASK) == ((y) & OCTEON_MODEL_MASK))
/* Match revision only 
 * ** Note: 'pass' is revsion + 1  (eg: pass 2 = revsion 1)*/
#define OCTEON_REVISION_MASK   0xff
#define OCTEON_MATCH_REVISION(x,y)     (((x) & OCTEON_REVISION_MASK) == ((y) & OCTEON_REVISION_MASK))

#endif    /* __OCTEON_MODEL_H__ */
