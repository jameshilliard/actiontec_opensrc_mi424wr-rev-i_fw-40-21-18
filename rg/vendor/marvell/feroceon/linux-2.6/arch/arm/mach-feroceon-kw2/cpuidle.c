/*
 * arch/arm/mach-feroceon-kw/cpuidle.c
 *
 * CPU idle Marvell Kirkwood SoCs
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * The cpu idle uses wait-for-interrupt and DDR self refresh in order
 * to implement two idle states -
 * #1 wait-for-interrupt
 * #2 wait-for-interrupt and DDR self refresh
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/cpuidle.h>
#include <linux/io.h>
#include <asm/proc-fns.h>
#include <linux/proc_fs.h>
#include <config/mvSysHwConfig.h>
#include "mvCommon.h"
#include "mvOs.h"

#define KIRKWOOD_MAX_STATES	2

extern void mv_kw2_cpu_idle_enter(void);

#ifdef CONFIG_MV_PMU_PROC
extern struct proc_dir_entry *mv_pm_proc_entry;
struct proc_dir_entry *cpu_idle_proc;
#endif /* CONFIG_MV_PMU_PROC */

static struct cpuidle_device *kirkwood_cpu_idle_device;

static struct cpuidle_driver kirkwood_idle_driver = {
	.name =         "kirkwood_idle",
	.owner =        THIS_MODULE,
};

static DEFINE_PER_CPU(struct cpuidle_device, kirkwood_cpuidle_device);

static int device_registered;

/* Actual code that puts the SoC in different idle states */
static int kirkwood_enter_idle(struct cpuidle_device *dev,
			       struct cpuidle_state *state)
{
	struct timeval before, after;
	int idle_time;

#ifdef CONFIG_JTAG_DEBUG
	local_irq_enable(); 
	return 0;
#endif

	local_irq_disable();
	do_gettimeofday(&before);
	if (state == &dev->states[0])
		/* Wait for interrupt state */
		cpu_do_idle();
	else if (state == &dev->states[1]) {
		/*
		 * Following write will put DDR in self refresh.
		 * Note that we have 256 cycles before DDR puts it
		 * self in self-refresh, so the wait-for-interrupt
		 * call afterwards won't get the DDR from self refresh
		 * mode.
		 */
		mv_kw2_cpu_idle_enter();
	}
	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
			(after.tv_usec - before.tv_usec);
	return idle_time;
}

#ifdef CONFIG_MV_PMU_PROC
static int mv_cpu_idle_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	MV_U32	regs[4];

	/* Reading / Writing from system controller internal registers */
	if (!strncmp (buffer, "enable", strlen("enable"))) {
		if(device_registered == 0) {
			device_registered = 1;
			if (cpuidle_register_device(kirkwood_cpu_idle_device)) {
				printk(KERN_ERR "mv_cpu_idle_write: Failed registering\n");
				return -EIO;
			}
		}
		cpuidle_enable_device(kirkwood_cpu_idle_device);
	} else if (!strncmp (buffer, "disable", strlen("disable"))) {
		cpuidle_disable_device(kirkwood_cpu_idle_device);
	} else if (!strncmp (buffer, "test", strlen("test"))) {

		/* Store Interrupt mask registers. */
		regs[0] = MV_REG_READ(MV_IRQ_MASK_LOW_REG);
		regs[1] = MV_REG_READ(MV_IRQ_MASK_HIGH_REG);
		regs[2] = MV_REG_READ(MV_IRQ_MASK_ERROR_REG);

		/* Disable all interrupts . */
		MV_REG_WRITE(MV_IRQ_MASK_LOW_REG, 0x0);
		MV_REG_WRITE(MV_IRQ_MASK_HIGH_REG, 0x0);
		MV_REG_WRITE(MV_IRQ_MASK_ERROR_REG, 0x0);
		
		/* Enable only the UART interrupt. */
		MV_REG_BIT_SET(MV_IRQ_MASK_HIGH_REG, 1 << (UART_IRQ_NUM(0) - 32));
		
		printk(KERN_INFO "Press any key to leave deep idle:");
		mv_kw2_cpu_idle_enter();

		/* Restore Interrupt mask registers. */
		MV_REG_WRITE(MV_IRQ_MASK_LOW_REG, regs[0]);
		MV_REG_WRITE(MV_IRQ_MASK_HIGH_REG, regs[1]);
		MV_REG_WRITE(MV_IRQ_MASK_ERROR_REG, regs[2]);
	}

	return count;
}


static int mv_cpu_idle_read(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *zero, void *ptr)
{
	if (offset > 0)
		return 0;
	return sprintf(buffer, "enable - Enable CPU Idle framework.\n"
				"disable - Disable CPU idle framework.\n"
				"test - Manually enter CPU Idle state, exit by ket stroke (DEBUG ONLY).\n");
	
}

#endif /* CONFIG_MV_PMU_PROC */

/* Initialize CPU idle by registering the idle states */
static int kw_cpuidle_probe(struct platform_device *pdev)
{
	struct cpuidle_device *device;

	cpuidle_register_driver(&kirkwood_idle_driver);

	device = &per_cpu(kirkwood_cpuidle_device, smp_processor_id());
	device->state_count = KIRKWOOD_MAX_STATES;

	/* Wait for interrupt state */
	device->states[0].enter = kirkwood_enter_idle;
	device->states[0].exit_latency = 1;
	device->states[0].target_residency = 100;
	device->states[0].flags = CPUIDLE_FLAG_TIME_VALID;
	strcpy(device->states[0].name, "WFI");
	strcpy(device->states[0].desc, "Wait for interrupt");

	/* CPU Deep Idle state */
	device->states[1].enter = kirkwood_enter_idle;
	device->states[1].exit_latency = 10;
	device->states[1].target_residency = 5000;
	device->states[1].flags = CPUIDLE_FLAG_TIME_VALID;
	strcpy(device->states[1].name, "DEEP IDLE");
	strcpy(device->states[1].desc, "CPU Deep Idle");

#if 0
	if (cpuidle_register_device(device)) {
		printk(KERN_ERR "kirkwood_init_cpuidle: Failed registering\n");
		return -EIO;
	}
#endif
	kirkwood_cpu_idle_device = device;

#ifdef CONFIG_MV_PMU_PROC
	/* Create proc entry. */
	cpu_idle_proc = create_proc_entry("cpu_idle", 0666, mv_pm_proc_entry);
	cpu_idle_proc->read_proc = mv_cpu_idle_read;
	cpu_idle_proc->write_proc = mv_cpu_idle_write;
	cpu_idle_proc->nlink = 1;
#endif /* CONFIG_MV_PMU_PROC */

	return 0;
}

static int kw_cpuidle_remove(struct platform_device *pdev)
{
	remove_proc_entry("cpu_idle", cpu_idle_proc);
	cpuidle_unregister_device(kirkwood_cpu_idle_device);
	return 0;
}

struct platform_driver kw_cpuidle_driver = {
	.probe		= kw_cpuidle_probe,
	.remove		= kw_cpuidle_remove,
	.driver		= {
		.name	= "kw_cpuidle",
		.owner	= THIS_MODULE,
	},
};

static int __init kw_cpuidle_drv_init(void)
{
	device_registered = 0;
	return platform_driver_register(&kw_cpuidle_driver);
}

static void __exit kw_cpuidle_drv_cleanup(void)
{
	platform_driver_unregister(&kw_cpuidle_driver);
}

module_init(kw_cpuidle_drv_init);
module_exit(kw_cpuidle_drv_cleanup);
