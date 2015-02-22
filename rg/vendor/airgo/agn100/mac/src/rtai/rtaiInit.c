/*
 * Project: rtai_cpp - RTAI C++ Framework 
 *
 * File: $Id: rtaiInit.c,v 1.1.1.1 2007/05/07 23:33:58 jungo Exp $
 *
 * Copyright: (C) 2001,2002 Erwin Rol <erwin@muffin.org>
 *
 * Licence:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/unistd.h>
#include <rtaiWrapper.h>
#include <polaris.h>
#include <sysRtaiStartup.h>
#include <sysDef.h>
#include <aniParam.h>




//int   param_addr=0;

#ifndef SIR_RTAI_UT
//MODULE_PARM(param_addr, "i");
//MODULE_PARM_DESC(param_addr, "Specifies address of MAC parameter structure");
#endif

/* per radio rt lib app structure */
#ifdef ANI_AP_SDK
static unsigned int radio_state[NUM_RADIO] = {0};
#else
static unsigned int radio_state[NUM_RADIO];
#endif
static struct rtLibApp pRtApp[NUM_RADIO];


#define RADIO_UNINITIALIZED 0
#define RADIO_STARTED 1
#define RADIO_STARTING 2
#define RADIO_CLEARING 3

#if !defined SIR_RTAI_UT && !defined(ANI_NO_INSMOD)
static int __init
#else
int
#endif
mac_mod_init(void * param_addr)
{
	struct rtLibApp * rt;
   if (param_addr == 0)
        return 0;

   if (radio_state[((tAniMacParam*)param_addr)->radioId]!=RADIO_UNINITIALIZED) {
	   printk("Attempt to start radioID = %d w/ state %d,",
			  ((tAniMacParam*)param_addr)->radioId,radio_state[((tAniMacParam*)param_addr)->radioId]);
	   return 0;
   } 
   radio_state[((tAniMacParam*)param_addr)->radioId]=RADIO_STARTING;
   printk("Starting MAC FW module...radioID = %d NUM_RADIO %d - ",
                         ((tAniMacParam*)param_addr)->radioId,NUM_RADIO);
#ifdef ANI_MIPS
    printk("param_addr = 0x%08x start at %08X\n", (unsigned int)param_addr, (unsigned int)tx_sprintf);
#else
    printk("param_addr = 0x%08x\n", (unsigned int)param_addr);
#endif
 
#ifdef ANI_ENTRY_LEVEL_AP
    if ((((tAniMacParam*)param_addr)->radioId)!=0) {
		printk("<1>Cannot start radio id %d on entry level AP\n",(((tAniMacParam*)param_addr)->radioId));
		return 0;
	}
#endif
#ifndef ANI_MIPS
	//	gSirClockCount = (unsigned long)
	//   start_rt_timer(nano2count(SYS_TICK_DUR_NS));
#endif


   	rt = ((tAniMacParam*)param_addr)->rt = &(pRtApp[((tAniMacParam*)param_addr)->radioId]);
	//	if (pRtApp[((tAniMacParam*)param_addr)->radioId] == 0) {
	//	pRtApp[((tAniMacParam*)param_addr)->radioId] = (struct rtLibApp*)kmalloc(sizeof(struct rtLibApp));
	//}
	// rt = ((tAniMacParam*)param_addr)->rt = &pRtApp[((tAniMacParam*)param_addr)->radioId];

	((tAniMacParam*)param_addr)->pMac = sysMacModInit((unsigned long)param_addr,rt);
    tx_rtai_wrapper_init((unsigned long)param_addr,rt);

    sysRtaiStartup((tAniMacParam*)param_addr);
   radio_state[((tAniMacParam*)param_addr)->radioId]=RADIO_STARTED;
   
    //MOD_INC_USE_COUNT;
    return 0;
}

#if !defined SIR_RTAI_UT && !defined(ANI_NO_INSMOD)
static void __init
#else
void
#endif
mac_mod_exit(void * param_addr)
{
    if (param_addr == 0) {
		printk("%s: Cleaning MAC FW module: param_addr == 0\n", 
							 __FUNCTION__);
		
        return;
	}
	if (radio_state[((tAniMacParam*)param_addr)->radioId]!=RADIO_STARTED) {
	   printk("Attempt to remove radioID = %d w/ state %d,",
			  ((tAniMacParam*)param_addr)->radioId,radio_state[((tAniMacParam*)param_addr)->radioId]);
	   return;
   } 
   radio_state[((tAniMacParam*)param_addr)->radioId]=RADIO_CLEARING;
    printk("%s: Cleaning MAC FW module: radio Id %d\n", 
		    __FUNCTION__, ((tAniMacParam*)param_addr)->radioId);

 
    sysRtaiCleanup((void*)(((tAniMacParam*)param_addr)->pMac));

    sysMacModExit((void*)(((tAniMacParam*)param_addr)->pMac));
    
    // Cleans up RT TimerTask
    tx_rtai_wrapper_cleanup((struct rtLibApp *)(((tAniMacParam*)param_addr)->rt));

    // Synchronization parameter for HDD
    ((tAniMacParam*)param_addr)->pMsgBufAlloc = 0;
	radio_state[((tAniMacParam*)param_addr)->radioId]=RADIO_UNINITIALIZED;
	   
    //MOD_DEC_USE_COUNT;
	return;
}

#if !defined SIR_RTAI_UT && !defined(ANI_NO_INSMOD)
//module_init(mac_mod_init);
//module_exit(mac_mod_exit); 
#endif
