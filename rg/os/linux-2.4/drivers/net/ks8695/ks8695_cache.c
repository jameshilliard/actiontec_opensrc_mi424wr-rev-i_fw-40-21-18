/*
	Copyright (c) 2002, Micrel Kendin Operations

	Written 2002 by LIQUN RUAN

	This software may be used and distributed according to the terms of 
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice. This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as lruan@kendin.com
	Micrel Kendin Operations
	486 Mercury Dr.
	Sunnyvale, CA 94085

	This driver is for Kendin's KS8695 SOHO Router Chipset as ethernet driver.

	Support and updates available at
	www.kendin.com or www.micrel.com

*/
#include "ks8695_drv.h"
#include "ks8695_cache.h"

/* for 922T, the values are fixed */
#if	0
static uint32_t	uICacheLineLen = 8;		/* 8 dwords, 32 bytes */
static uint32_t	uICacheSize = 8192;		/* 8K cache size */
#endif

static uint32_t	bPowerSaving = FALSE;
static uint32_t	bAllowPowerSaving = FALSE;

/*
 * ks8695_icache_read_c9
 *	This function is use to read lockdown register
 *
 * Argument(s)
 *	NONE
 *
 * Return(s)
 *	NONE
 */
void ks8695_icache_read_c9(void)
{
	register int base;

	__asm__(
		"mrc p15, 0, %0, c9, c0, 1"
		 : "=r" (base)
		);

	DRV_INFO("%s: lockdown index=%d", __FUNCTION__, (base >> 26));
}

/*
 * ks8695_icache_unlock
 *	This function is use to unlock the icache locked previously
 *
 * Argument(s)
 *	NONE.
 *
 * Return(s)
 *	NONE.
 */
void ks8695_icache_unlock(void)
{
#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	__asm__(
		"MOV r1, #0\n"
		"MCR	p15, 0, r1, c9, c0, 1\n"	/* reset victim base to 0 */
	);

	DRV_INFO("%s", __FUNCTION__);
}

/*
 * ks8695_icache_change_policy
 *	This function is use to change cache policy for ARM chipset
 *
 * Argument(s)
 *	bRoundRobin		round robin or random mode
 *
 * Return(s)
 *	NONE
 */
void ks8695_icache_change_policy(int bRoundRobin)
{
	uint32_t tmp;

	__asm__ ( 
		"mrc p15, 0, r1, c1, c0, 0\n"
		"mov	r2, %1\n"
		"cmp	r2, #0\n"
		"orrne r1, r1, #0x4000\n"
		"biceq r1, r1, #0x4000\n"
		"mov	%0, r1\n"
		/* Write this to the control register */
		"mcr p15, 0, r1, c1, c0, 0\n"
		/* Make sure the pipeline is clear of any cached entries */
		"nop\n"
		"nop\n"
		"nop\n" 
		: "=r" (tmp)
		: "r" (bRoundRobin)
		: "r1", "r2"
		);

/*#ifdef	DEBUG_THIS*/
	DRV_INFO("Icache mode = %s", bRoundRobin ? "roundrobin" : "random");
/*#endif*/
}

/*
 * ks8695_enable_power_saving
 *	This function is use to enable/disable power saving
 *
 * Argument(s)
 *	bSaving		
 *
 * Return(s)
 *	NONE
 */
void ks8695_enable_power_saving(int bEnablePowerSaving)
{
	bAllowPowerSaving = bEnablePowerSaving;
}

/*
 * ks8695_power_saving
 *	This function is use to put ARM chipset in low power mode (wait for interrupt)
 *
 * Argument(s)
 *	bSaving		
 *
 * Return(s)
 *	NONE
 */
void ks8695_power_saving(int bSaving)
{
	uint32_t tmp;

	/* if not allowed by configuration option */
	if (!bAllowPowerSaving)
		return;

	/* if already set */
	if (bPowerSaving == bSaving)
		return;

	bPowerSaving = bSaving;

	__asm__ ( 
		"mov	r1, %1\n"
		"mcr p15, 0, r1, c7, c0, 4\n"
		/* Make sure the pipeline is clear of any cached entries */
		"nop\n"
		"nop\n"
		"nop\n" 
		: "=r" (tmp)
		: "r" (bSaving)
		: "r1", "r2"
		);

	DRV_INFO("%s: power saving = %s", __FUNCTION__, bSaving ? "enabled" : "disabled");
}
