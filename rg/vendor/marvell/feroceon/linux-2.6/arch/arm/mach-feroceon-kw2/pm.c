/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//#define DEBUG
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <asm/mach/time.h>

#include "ctrlEnv/mvCtrlEnvLib.h"
#include "ctrlEnv/sys/mvCpuIf.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "gpp/mvGpp.h"
#include "device/mvDeviceRegs.h"
#include "cntmr/mvCntmrRegs.h"
#include "cpu/mvCpu.h"

#define	MV_PMU_REGS_BASE	MV_PMU_REGS_OFFSET

#define PMU_L2C_CTRL_AND_CONF	(MV_PMU_REGS_BASE + 0x004)
#define PMU_L2C_EVENT_STATUS	(MV_PMU_REGS_BASE + 0x020)
#define PMU_CPU_CTRL_AND_CONF	(MV_PMU_REGS_BASE + 0x104)
#define PMU_CPU_STATUS_MASK	(MV_PMU_REGS_BASE + 0x10C)
#define PMU_CPU_EVENT_STATUS	(MV_PMU_REGS_BASE + 0x120)
#define PMU_CPU_BOOT_ADDR	(MV_PMU_REGS_BASE + 0x124)
#define PMU_PWR_UP_DELAY	(MV_DEV_PMU_REGS_OFFSET + 0x014)

static unsigned int   	count_standby;

extern int kw2_cpu_suspend(void);
extern int kw2_cpu_resume(void);
extern MV_CPU_DEC_WIN* mv_sys_map(void);

static int mv_pm_enter(suspend_state_t state);

static MV_AHB_TO_MBUS_DEC_WIN ahbAddrDecWin[MAX_AHB_TO_MBUS_WINS];
static MV_ADDR_WIN ahbAddrWinRemap[MAX_AHB_TO_MBUS_WINS];

#define CONFIG_PMU_PROC

void mv_kw2_cpu_idle_enter(void)
{
	mv_pm_enter(PM_SUSPEND_STANDBY);
	return;
}


static void save_kw2_cpu_win_state(void)
{
	u32 i;
	MV_AHB_TO_MBUS_DEC_WIN	winInfo;

	/* Save CPU windows state, and enable access for Bootrom	*
	** according to SoC default address decoding windows.		*/
	for(i = 0; i < MAX_AHB_TO_MBUS_WINS; i++) {
		mvAhbToMbusWinGet(i, &ahbAddrDecWin[i]);
		mvAhbToMbusWinRemapGet(i, &ahbAddrWinRemap[i]);
		
		/* Disable the window */
		mvAhbToMbusWinEnable(i, MV_FALSE);
	}

	/* Open default windows for Bootrom, PnC and internal regs.	*/
	/* Bootrom */
	winInfo.target = BOOT_ROM_CS;
	winInfo.addrWin.baseLow = 0xF8000000;
	winInfo.addrWin.baseHigh = 0x0;
	winInfo.addrWin.size = _128M;
	winInfo.enable = MV_TRUE;
	mvAhbToMbusWinSet(7, &winInfo);

	/* PnC */
	winInfo.target = PNC_BM;
	winInfo.addrWin.baseLow = 0xC0060000;
	winInfo.addrWin.baseHigh = 0x0;
	winInfo.addrWin.size = _64K;
	winInfo.enable = MV_TRUE;
	mvAhbToMbusWinSet(6, &winInfo);
#if 0
	/* Internal regs */
	winInfo.target = INTER_REGS;
	winInfo.addrWin.baseLow = 0xD0000000;
	winInfo.addrWin.baseHigh = 0x0;
	winInfo.addrWin.size = _1M;
	winInfo.enable = MV_TRUE;
	mvAhbToMbusWinSet(MV_AHB_TO_MBUS_INTREG_WIN, &winInfo);
#endif
	/* Cesa SRAM */
	winInfo.target = CRYPT1_ENG;
	winInfo.addrWin.baseLow = 0xC8010000;
	winInfo.addrWin.baseHigh = 0x0;
	winInfo.addrWin.size = _64K;
	winInfo.enable = MV_TRUE;
	mvAhbToMbusWinSet(4, &winInfo);

}


static void restore_kw2_cpu_win_state(void)
{
	mvCpuIfInit(mv_sys_map());
}

unsigned long suspend_phys_addr(void * physaddr)
{
	return virt_to_phys(physaddr);
}

static void mv_enter_standby(void)
{
	u32 reg;
	static MV_U32 pwrUpDelay = 0;
	

	pr_debug("kw2_standby: Entering STANDBY mode.\n");
	
	if (pwrUpDelay == 0)
		pwrUpDelay = mvBoardPwrUpDelayGet();

	count_standby++;

	save_kw2_cpu_win_state();

	/* Prepare resume PC */
	MV_REG_WRITE(PMU_CPU_BOOT_ADDR, virt_to_phys(kw2_cpu_resume));

	MV_REG_WRITE(PMU_PWR_UP_DELAY, pwrUpDelay);

	MV_REG_WRITE(PMU_CPU_STATUS_MASK, 0x00310000);

	/* L2 Power down enable */
	if (mvCpuL2Exists())
		MV_REG_BIT_SET(PMU_L2C_CTRL_AND_CONF, BIT20);

	/* CPU Power down enable */
	MV_REG_BIT_SET(PMU_CPU_CTRL_AND_CONF, BIT20);

	/* CPU Power down request */
	MV_REG_BIT_SET(PMU_CPU_CTRL_AND_CONF, BIT16);

	/* Suspend the CPU only */
	if (kw2_cpu_suspend() == 0)
		cpu_init();

	restore_kw2_cpu_win_state();

	reg = MV_REG_READ(PMU_CPU_STATUS_MASK);
	reg &= ~0x3310000;
	MV_REG_WRITE(PMU_CPU_STATUS_MASK, reg);

	pr_debug("kw2_standby: Exiting STANDBY mode.\n");	
}


static int mv_pm_enter(suspend_state_t state)
{
	int ret = 0;

	switch (state) {
	case PM_SUSPEND_STANDBY:
		mv_enter_standby();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}


int __init mv_pm_init(void)
{
	printk(KERN_INFO "Marvell Kirkwood2 Power Management Initializing\n");

	return 0;
}

late_initcall(mv_pm_init);

