/******************************************************************************
 *
 * Name:	skdebug.h
 * Project:	Gigabit Ethernet Adapters, Common Modules
 * Version:	$Revision: 2.4 $
 * Date:	$Date: 2005/12/14 16:11:35 $
 * Purpose:	SK specific DEBUG support
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	LICENSE:
 *	(C)Copyright 1998-2002 SysKonnect.
 *	(C)Copyright 2002-2005 Marvell.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *	The information in this file is provided "AS IS" without warranty.
 *	/LICENSE
 *
 ******************************************************************************/

#ifndef __INC_SKDEBUG_H
#define __INC_SKDEBUG_H

/* #define DEBUG */
#ifdef	DEBUG
#ifndef SK_DBG_MSG
#define SK_DBG_MSG(pAC,comp,cat,arg) \
		if ( ((comp) & SK_DBG_CHKMOD(pAC)) &&	\
		      ((cat) & SK_DBG_CHKCAT(pAC)) ) {	\
			SK_DBG_PRINTF arg;		\
		}
#endif
#else
#define SK_DBG_MSG(pAC,comp,lev,arg)
#endif

/* PLS NOTE:
 * =========
 * Due to any restrictions of kernel printf routines do not use other
 * format identifiers as: %x %d %c %s .
 * Never use any combined format identifiers such as: %lx %ld in your
 * printf - argument (arg) because some OS specific kernel printfs may
 * only support some basic identifiers.
 */

/* Debug modules */

#define SK_DBGMOD_MERR	0x00000001L	/* general module error indication */
#define SK_DBGMOD_HWM	0x00000002L	/* Hardware init module */
#define SK_DBGMOD_RLMT	0x00000004L	/* RLMT module */
#define SK_DBGMOD_VPD	0x00000008L	/* VPD module */
#define SK_DBGMOD_I2C	0x00000010L	/* I2C module */
#define SK_DBGMOD_PNMI	0x00000020L	/* PNMI module */
#define SK_DBGMOD_CSUM	0x00000040L	/* CSUM module */
#define SK_DBGMOD_ADDR	0x00000080L	/* ADDR module */
#define SK_DBGMOD_PECP	0x00000100L	/* PECP module */
#define SK_DBGMOD_POWM	0x00000200L	/* Power Management module */
#ifdef SK_ASF
#define SK_DBGMOD_ASF	0x00000400L	/* ASF module */
#endif
#ifdef SK_LBFO
#define SK_DBGMOD_LACP	0x00000800L	/* link aggregation control protocol */
#define SK_DBGMOD_FD	0x00001000L	/* frame distributor (link aggregation) */
#endif /* SK_LBFO */

/* Debug events */

#define SK_DBGCAT_INIT	0x00000001L	/* module/driver initialization */
#define SK_DBGCAT_CTRL	0x00000002L	/* controlling devices */
#define SK_DBGCAT_ERR	0x00000004L	/* error handling paths */
#define SK_DBGCAT_TX	0x00000008L	/* transmit path */
#define SK_DBGCAT_RX	0x00000010L	/* receive path */
#define SK_DBGCAT_IRQ	0x00000020L	/* general IRQ handling */
#define SK_DBGCAT_QUEUE	0x00000040L	/* any queue management */
#define SK_DBGCAT_DUMP	0x00000080L	/* large data output e.g. hex dump */
#define SK_DBGCAT_FATAL	0x00000100L	/* fatal error */

#endif	/* __INC_SKDEBUG_H */
