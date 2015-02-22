/******************************************************************************
 *
 * Name:	skgehwt.c
 * Project:	Gigabit Ethernet Adapters, Event Scheduler Module
 * Version:	$Revision: 2.2 $
 * Date:	$Date: 2004/05/28 13:39:04 $
 * Purpose:	Hardware Timer
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	(C)Copyright 1998-2002 SysKonnect GmbH.
 *	(C)Copyright 2002-2004 Marvell.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#include <config.h>
 
#ifdef CONFIG_SK98

/*
 *	Event queue and dispatcher
 */
#if (defined(DEBUG) || ((!defined(LINT)) && (!defined(SK_SLIM))))
static const char SysKonnectFileId[] =
	"@(#) $Id: skgehwt.c,v 2.2 2004/05/28 13:39:04 rschmidt Exp $ (C) Marvell.";
#endif

#include "h/skdrv1st.h"		/* Driver Specific Definitions */
#include "h/skdrv2nd.h"		/* Adapter Control- and Driver specific Def. */

#ifdef __C2MAN__
/*
 *   Hardware Timer function queue management.
 */
intro()
{}
#endif

/*
 * Prototypes of local functions.
 */
#define	SK_HWT_MAX	65000UL * 160		/* ca. 10 sec. */

/* correction factor */
#define	SK_HWT_FAC	(10 * (SK_U32)pAC->GIni.GIHstClkFact / 16)

/*
 * Initialize hardware timer.
 *
 * Must be called during init level 1.
 */
void	SkHwtInit(
SK_AC	*pAC,	/* Adapters context */
SK_IOC	Ioc)	/* IoContext */
{
	pAC->Hwt.TStart = 0 ;
	pAC->Hwt.TStop	= 0 ;
	pAC->Hwt.TActive = SK_FALSE;

	SkHwtStop(pAC, Ioc);
}

/*
 *
 * Start hardware timer (clock ticks are 16us).
 *
 */
void	SkHwtStart(
SK_AC	*pAC,	/* Adapters context */
SK_IOC	Ioc,	/* IoContext */
SK_U32	Time)	/* Time in usec to load the timer */
{
	if (Time > SK_HWT_MAX)
		Time = SK_HWT_MAX;

	pAC->Hwt.TStart = Time;
	pAC->Hwt.TStop = 0L;

	if (!Time) {
		Time = 1L;
	}

	SK_OUT32(Ioc, B2_TI_INI, Time * SK_HWT_FAC);

	SK_OUT16(Ioc, B2_TI_CTRL, TIM_START);	/* Start timer */

	pAC->Hwt.TActive = SK_TRUE;
}

/*
 * Stop hardware timer.
 * and clear the timer IRQ
 */
void	SkHwtStop(
SK_AC	*pAC,	/* Adapters context */
SK_IOC	Ioc)	/* IoContext */
{
	SK_OUT16(Ioc, B2_TI_CTRL, TIM_STOP);

	SK_OUT16(Ioc, B2_TI_CTRL, TIM_CLR_IRQ);

	pAC->Hwt.TActive = SK_FALSE;
}

/*
 *	Stop hardware timer and read time elapsed since last start.
 *
 * returns
 *	The elapsed time since last start in units of 16us.
 *
 */
SK_U32	SkHwtRead(
SK_AC	*pAC,	/* Adapters context */
SK_IOC	Ioc)	/* IoContext */
{
	SK_U32	TRead;
	SK_U32	IStatus;
	SK_U32	TimerInt;

	TimerInt = CHIP_ID_YUKON_2(pAC) ? Y2_IS_TIMINT : IS_TIMINT;

	if (pAC->Hwt.TActive) {
		
		SkHwtStop(pAC, Ioc);

		SK_IN32(Ioc, B2_TI_VAL, &TRead);
		TRead /= SK_HWT_FAC;

		SK_IN32(Ioc, B0_ISRC, &IStatus);

		/* Check if timer expired (or wrapped around) */
		if ((TRead > pAC->Hwt.TStart) || ((IStatus & TimerInt) != 0)) {

			SkHwtStop(pAC, Ioc);

			pAC->Hwt.TStop = pAC->Hwt.TStart;
		}
		else {

			pAC->Hwt.TStop = pAC->Hwt.TStart - TRead;
		}
	}
	return(pAC->Hwt.TStop);
}

/*
 * interrupt source= timer
 */
void	SkHwtIsr(
SK_AC	*pAC,	/* Adapters context */
SK_IOC	Ioc)	/* IoContext */
{
	SkHwtStop(pAC, Ioc);

	pAC->Hwt.TStop = pAC->Hwt.TStart;

	SkTimerDone(pAC, Ioc);
}

/* End of file */

#endif
