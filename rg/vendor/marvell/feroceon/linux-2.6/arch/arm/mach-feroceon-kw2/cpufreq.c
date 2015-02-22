/*
 * arch/arm/mach-kirkwood/cpufreq.c
 *
 * Clock scaling for Kirkwood SoC
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <config/mvSysHwConfig.h>
#include "mvCommon.h"
#include "mvOs.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "cpu/mvCpu.h"

#undef DEBUG
#ifdef DEBUG
#define DB(x)	x
#else
#define DB(x)
#endif

extern struct proc_dir_entry *mv_pm_proc_entry;

enum kw_cpufreq_range {
	KW_CPUFREQ_LOW 		= 0,
	KW_CPUFREQ_HIGH 	= 1
};

static struct cpufreq_frequency_table kw_freqs[] = {
	{ KW_CPUFREQ_LOW, 0                  },
	{ KW_CPUFREQ_HIGH, 0                  },
	{ 0, CPUFREQ_TABLE_END  }
};


/*
 * Power management function: set or unset powersave mode
 */
static inline void kw_set_powersave(u8 on)
{
	DB(printk(KERN_DEBUG "cpufreq: Setting PowerSaveState to %s\n", on ? "on" : "off"));

	if (on)
		mvCtrlPwrSaveOn();
	else
		mvCtrlPwrSaveOff();
}

static int kw_cpufreq_verify(struct cpufreq_policy *policy)
{
	if (unlikely(!cpu_online(policy->cpu)))
		return -ENODEV;

	return cpufreq_frequency_table_verify(policy, kw_freqs);
}

/*
 * Get the current frequency for a given cpu.
 */
static unsigned int kw_cpufreq_get(unsigned int cpu)
{
	unsigned int freq;
	u32 reg;

	if (unlikely(!cpu_online(cpu)))
		return -ENODEV;

	/* To get the current frequency, we have to check if
	* the powersave mode is set. */
	reg = MV_REG_READ(POWER_MNG_CTRL_REG);

	if (reg & PMC_POWERSAVE_EN)
		freq = kw_freqs[KW_CPUFREQ_LOW].frequency;
	else
		freq = kw_freqs[KW_CPUFREQ_HIGH].frequency;

	return freq;
}

/*
 * Set the frequency for a given cpu.
 */
static int kw_cpufreq_target(struct cpufreq_policy *policy,
		unsigned int target_freq, unsigned int relation)
{
	unsigned int index;
	struct cpufreq_freqs freqs;

	if (unlikely(!cpu_online(policy->cpu)))
		return -ENODEV;

	/* Lookup the next frequency */
	if (unlikely(cpufreq_frequency_table_target(policy,
		kw_freqs, target_freq, relation, &index)))
		return -EINVAL;


	freqs.old = policy->cur;
	freqs.new = kw_freqs[index].frequency;
	freqs.cpu = policy->cpu;

	DB(printk(KERN_DEBUG "cpufreq: Setting CPU Frequency to %u KHz\n",freqs.new));

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/* Interruptions will be disabled in the low level power mode
	* functions. */
	if (index == KW_CPUFREQ_LOW)
		kw_set_powersave(1);
	else if (index == KW_CPUFREQ_HIGH)
		kw_set_powersave(0);
	else
		return -EINVAL;

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

static int kw_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	if (unlikely(!cpu_online(policy->cpu)))
		return -ENODEV;

	kw_freqs[KW_CPUFREQ_HIGH].frequency = mvCpuPclkGet()/1000;
	/* CPU low frequency is the DDR frequency. */
	kw_freqs[KW_CPUFREQ_LOW].frequency  = mvBoardSysClkGet()/1000;

	printk(KERN_DEBUG
			"cpufreq: High frequency: %uKHz - Low frequency: %uKHz\n",
			kw_freqs[KW_CPUFREQ_HIGH].frequency,
			kw_freqs[KW_CPUFREQ_LOW].frequency);

	policy->cpuinfo.transition_latency = 5000;
	policy->cur = kw_cpufreq_get(0);
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	cpufreq_frequency_table_get_attr(kw_freqs, policy->cpu);

	return cpufreq_frequency_table_cpuinfo(policy, kw_freqs);
}


static int kw_cpufreq_cpu_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);
	return 0;
}

static struct freq_attr *kw_freq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};


static struct cpufreq_driver kw_freq_driver = {
	.owner          = THIS_MODULE,
	.name           = "kw_cpufreq",
	.init           = kw_cpufreq_cpu_init,
	.verify         = kw_cpufreq_verify,
	.exit		= kw_cpufreq_cpu_exit,
	.target         = kw_cpufreq_target,
	.get            = kw_cpufreq_get,
	.attr           = kw_freq_attr,
};

#ifdef CONFIG_MV_PMU_PROC
static int mv_cpu_freq_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	struct cpufreq_policy policy;

	/* Reading / Writing from system controller internal registers */
	if (!strncmp (buffer, "enable", strlen("enable"))) {
		cpufreq_register_driver(&kw_freq_driver);
	} else if (!strncmp (buffer, "disable", strlen("disable"))) {
		cpufreq_get_policy(&policy, smp_processor_id());
		kw_cpufreq_target(&policy, kw_freqs[KW_CPUFREQ_HIGH].frequency, CPUFREQ_RELATION_H);
		cpufreq_unregister_driver(&kw_freq_driver);
	} else if (!strncmp (buffer, "fast", strlen("fast"))) {
		mvCtrlPwrSaveOff();
	} else if (!strncmp (buffer, "slow", strlen("slow"))) {
		mvCtrlPwrSaveOn();
	}

	return count;
}


static int mv_cpu_freq_read(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *zero, void *ptr)
{
	if (offset > 0)
		return 0;
	return sprintf(buffer, "enable - Enable CPU-Freq framework.\n"
				"disable - Disable CPU-Freq framework.\n"
				"fast - Manually set the CPU to fast frequency mode (in Disable mode).\n"
				"slow - Manually set the CPU to slow frequency mode (in Disable mode).\n");
}

#endif /* CONFIG_MV_PMU_PROC */

static int __init kw_cpufreq_init(void)
{
#ifdef CONFIG_MV_PMU_PROC
	struct proc_dir_entry *cpu_freq_proc;
#endif /* CONFIG_MV_PMU_PROC */

	printk(KERN_INFO "cpufreq: Init kirkwood cpufreq driver\n");

#ifdef CONFIG_MV_PMU_PROC
	/* Create proc entry. */
	cpu_freq_proc = create_proc_entry("cpu_freq", 0666, mv_pm_proc_entry);
	cpu_freq_proc->read_proc = mv_cpu_freq_read;
	cpu_freq_proc->write_proc = mv_cpu_freq_write;
	cpu_freq_proc->nlink = 1;
#endif /* CONFIG_MV_PMU_PROC */

	return cpufreq_register_driver(&kw_freq_driver);
}

static void __exit kw_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&kw_freq_driver);
}


MODULE_AUTHOR("Marvell Semiconductors ltd.");
MODULE_DESCRIPTION("CPU frequency scaling for Kirkwood SoC");
MODULE_LICENSE("GPL");
module_init(kw_cpufreq_init);
module_exit(kw_cpufreq_exit);

