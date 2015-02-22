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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysdev.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include "ctrlEnv/mvCtrlEnvLib.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "gpp/mvGpp.h"
#include "mvOs.h"
#include "cpu/mvCpuCntrs.h"
#include "cpu/mvCpuL2Cntrs.h"
#include "eth-phy/mvEthPhy.h"

#include "ddr2/mvDramCounters.h"

#ifdef CONFIG_MV_CPU_PERF_CNTRS
MV_CPU_CNTRS_EVENT *proc_event = NULL;
#endif	/*  */

#ifdef CONFIG_MV_CPU_L2_PERF_CNTRS
MV_CPU_L2_CNTRS_EVENT *proc_l2_event = NULL;
#endif	/*  */

extern u32 mvCpuIfPrintSystemConfig(u8 *buffer, u32 index);

/* global variables from 'regdump' */
static struct proc_dir_entry *evb_resource_dump;
static u32 evb_resource_dump_request, evb_resource_dump_result;

/* Some service routines */
static int ishex(char ch)
{
	if (((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F')))
		return 1;
	return 0;
}

static int hex_value(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	return 0;
}

static int atoh(char *s, int len)
{
	int i = 0;
	while (ishex(*s) && len--) {
		i = i * 0x10 + hex_value(*s);
		s++;
	}
	return i;
}


#ifdef CONFIG_MV_CPU_PERF_CNTRS
void mv_proc_start_cntrs(int cc0, int cc1, int cc2, int cc3)
{
	printk(KERN_INFO "configure CPU counters %d %d %d %d \n", cc0, cc1, cc2, cc3);
	if (mvCpuCntrsProgram(0, cc0, "cc0", 0) != MV_OK)
		goto error;
	if (mvCpuCntrsProgram(1, cc1, "cc1", 0) != MV_OK)
		goto error;
	if (mvCpuCntrsProgram(2, cc2, "cc2", 0) != MV_OK)
		goto error;
	if (mvCpuCntrsProgram(3, cc3, "cc3", 0) != MV_OK)
		goto error;
	if (proc_event == NULL) {
		proc_event = mvCpuCntrsEventCreate("PROC_CPU_CNTRS", 1);
		if (proc_event == NULL)
			goto error;
	}
	mvCpuCntrsEventClear(proc_event);
	mvCpuCntrsReset();
	MV_CPU_CNTRS_START(proc_event);
	return;
error:
	printk(KERN_INFO "ERROR configuring counter\n");
	return;
}

void mv_proc_show_cntrs(void)
{
	if (proc_event != NULL) {
		MV_CPU_CNTRS_STOP(proc_event);
		MV_CPU_CNTRS_SHOW(proc_event);
	}
}


#endif	/*  */

#ifdef CONFIG_MV_CPU_L2_PERF_CNTRS
void mv_proc_start_l2_cntrs(int l20, int l21)
{
	printk(KERN_INFO "configure CPU L2 counters %d %d \n", l20, l21);
	if (mvCpuL2CntrsProgram(0, l20, "l20", 0) != MV_OK)
		goto error;
	if (mvCpuL2CntrsProgram(1, l21, "l21", 0) != MV_OK)
		goto error;
	if (proc_l2_event == NULL) {
		proc_l2_event = mvCpuL2CntrsEventCreate("PROC_CPU_L2_CNTRS", 1);
		if (proc_l2_event == NULL)
			goto error;
	}
	mvCpuL2CntrsEventClear(proc_l2_event);
	mvCpuL2CntrsReset();
	MV_CPU_L2_CNTRS_START(proc_l2_event);
	return;
error:
	printk(KERN_INFO "ERROR configuring L2 counter\n");
	return;
}

void mv_proc_show_l2_cntrs(void)
{
	if (proc_l2_event != NULL) {
		MV_CPU_L2_CNTRS_STOP(proc_l2_event);
		MV_CPU_L2_CNTRS_SHOW(proc_l2_event);
	}
}


#endif	/*  */

#ifdef CONFIG_MV_DRAM_STATS_CNTRS
void mv_proc_start_dram_stats_cntrs(int mode0, int mode1)
{
	printk(KERN_INFO "configure DRAM statistics counters %d %d \n", mode0, mode1);
	mvDramStatStop();
	mvDramStatClear();
	mvDramStatConfig(0, mode0);
	mvDramStatConfig(1, mode1);
	mvDramStatStart();
	return;
}

void mv_proc_show_dram_stats_cntrs(void)
{
	MV_U64 cnt0, cnt1, hclk;
	mvDramStatRead(&cnt0, &cnt1, &hclk);
	printk(KERN_INFO "Counter #0 - 0x%llx.\n", cnt0);
	printk(KERN_INFO "Counter #1 - 0x%llx.\n", cnt1);
	printk(KERN_INFO "HCLK       - 0x%llx.\n", hclk);
	return;
}


#endif	/*  */

/* The format of writing to this module is as follows -
   char 0 - r/w (Reading from register or Writing to register/memory)
   char 1 - space
   char 2 - register/mem_addr offset 7
   char 3 - register/mem_addr offset 6
   char 4 - register/mem_addr offset 5
   char 5 - register/mem_addr offset 4
   char 6 - register/mem_addr offset 3
   char 7 - register/mem_addr offset 2
   char 8 - register/mem_addr offset 1
   char 9 - register/mem_addr offset 0
   // The following is valid only if write request
   char 10 - space
   char 11 - register/mem_addr value 7
   char 12 - register/mem_addr value 6
   char 13 - register/mem_addr value 5
   char 14 - register/mem_addr value 4
   char 15 - register/mem_addr value 3
   char 16 - register/mem_addr value 2
   char 17 - register/mem_addr value 1
   char 18 - register/mem_addr value 0

*/

/********************************************************************
* evb_resource_dump_write -
*
* When written to the /proc/resource_dump file this function is called
*
* Inputs: file / data are not used. Buffer and count are the pointer
*         and length of the input string
* Returns: Read from GT register
* Outputs: count
*********************************************************************/
unsigned int kernel_align = 0;
extern unsigned int support_wait_for_interrupt;
int evb_resource_dump_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	/* Reading / Writing from system controller internal registers */
	if (!strncmp(buffer, "register", 8)) {
		if (buffer[10] == 'r') {
			evb_resource_dump_request = atoh((char *)((unsigned int)buffer + 12), 8);
			evb_resource_dump_result = MV_REG_READ(evb_resource_dump_request);
		}
		if (buffer[10] == 'w') {
			evb_resource_dump_request = atoh((char *)((unsigned int)buffer + 12), 8);
			evb_resource_dump_result = atoh((char *)((unsigned int)buffer + 12 + 8 + 1), 8);
			MV_REG_WRITE(evb_resource_dump_request, evb_resource_dump_result);
		}
	}

	/* Reading / Writing from 32bit address - mostly usable for memory */
	if (!strncmp(buffer, "memory  ", 8)) {
		if (buffer[10] == 'r') {
			evb_resource_dump_request = atoh((char *)((unsigned int)buffer + 12), 8);
			evb_resource_dump_result = *(unsigned int *)evb_resource_dump_request;
		}
		if (buffer[10] == 'w') {
			evb_resource_dump_request = atoh((char *)((unsigned int)buffer + 12), 8);
			evb_resource_dump_result = atoh((char *)((unsigned int)buffer + 12 + 8 + 1), 8);
			*(unsigned int *)evb_resource_dump_request = evb_resource_dump_result;
		}
	}

	/* Reading / Writing from a rgister via SMI */
	if (!strncmp(buffer, "smi", 3)) {
		unsigned short regVal;
		unsigned int dev_addr = atoh((char *)((unsigned int)buffer + 7), 8);
		if (buffer[5] == 'r') {
			evb_resource_dump_request = atoh((char *)((unsigned int)buffer + 7 + 8 + 1), 8);
			regVal = 0;
			mvEthPhyRegRead(dev_addr, evb_resource_dump_request, &regVal);
			evb_resource_dump_result = (u32) regVal;
		}
		if (buffer[5] == 'w') {
			evb_resource_dump_request = atoh((char *)((unsigned int)buffer + 7 + 8 + 1), 8);
			evb_resource_dump_result = atoh((char *)((unsigned int)buffer + 7 + 8 + 8 + 2), 8);
			mvEthPhyRegWrite(dev_addr, evb_resource_dump_request, (u16) evb_resource_dump_result);
		}
	}

#ifdef CONFIG_MV_CPU_PERF_CNTRS
	if (!strncmp(buffer, "start_cc", 8)) {
		int cc0, cc1, cc2, cc3;
		sscanf((char *)((unsigned int)buffer + 8), "%d %d %d %d", &cc0, &cc1, &cc2, &cc3);
		mv_proc_start_cntrs(cc0, cc1, cc2, cc3);
	}
	if (!strncmp(buffer, "show__cc", 8))
		mv_proc_show_cntrs();


#endif	/*  */
#ifdef CONFIG_MV_CPU_L2_PERF_CNTRS
	if (!strncmp(buffer, "start_l2", 8)) {
		int l20, l21;
		sscanf((char *)((unsigned int)buffer + 8), "%d %d", &l20, &l21);
		mv_proc_start_l2_cntrs(l20, l21);
	}
	if (!strncmp(buffer, "show__l2", 8))
		mv_proc_show_l2_cntrs();

#endif	/*  */

#ifdef CONFIG_MV_DRAM_STATS_CNTRS
	if (!strncmp(buffer, "start_dram_stats", strlen("start_dram_stats"))) {
		int mode0, mode1;
		sscanf((char *)((unsigned int)buffer + strlen("start_dram_stats")), "%d %d", &mode0, &mode1);
		mv_proc_start_dram_stats_cntrs(mode0, mode1);
	}
	if (!strncmp(buffer, "stop_dram_stats", strlen("stop_dram_stats")))
		mvDramStatStop();

	if (!strncmp(buffer, "show_dram_stats", strlen("show_dram_stats")))
		mv_proc_show_dram_stats_cntrs();

#endif	/*  */
	
	if (!strncmp(buffer, "idle_wfi", strlen("idle_wfi"))) {
		int en;
		sscanf((char *)((unsigned int)buffer + strlen("idle_wfi")), "%d", &en);
		support_wait_for_interrupt = en;
	}

	if (!strncmp(buffer, "show__ua", 8)) {
		if (kernel_align == 1)
			kernel_align = 0;
		else
			kernel_align = 1;
		printk(KERN_INFO "debug kernel align %d\n", kernel_align);
	}


	if (!strncmp(buffer, "cpu_freq", strlen("cpu_freq"))) {
		buffer += strlen("cpu_freq") + 1;
		if (!strncmp(buffer, "normal", strlen("normal"))) {
			printk(KERN_INFO "Entering fast mode.\n");
			mvCtrlPwrSaveOff();
		} else if (!strncmp(buffer, "ddr", strlen("ddr"))) {
			printk(KERN_INFO "Entering slow mode.\n");
			mvCtrlPwrSaveOn();
		}
	}

#if 0
	    if (!strncmp(buffer, "ddd", 3)) {
		unsigned int ii[10];
		int ip, sum = 0;
		volatile unsigned int *tt = (unsigned int *)((unsigned int)ii + 2);
		MV_CPU_CNTRS_EVENT *hal_rx_event = NULL;

		    /* 0 - instruction counters */
		    mvCpuCntrsProgram(0, MV_CPU_CNTRS_INSTRUCTIONS, "Instr", 25);

		    /* 1 - ICache misses counter */
		    mvCpuCntrsProgram(1, MV_CPU_CNTRS_ICACHE_READ_MISS, "IcMiss", 0);

		    /* 2 - cycles counter */
		    mvCpuCntrsProgram(2, MV_CPU_CNTRS_CYCLES, "Cycles", 21);

		    /* 3 - DCache read misses counter */
		    mvCpuCntrsProgram(3, MV_CPU_CNTRS_DCACHE_READ_MISS, "DcRdMiss", 0);
		hal_rx_event = mvCpuCntrsEventCreate("HAL_RX", 1);
		MV_CPU_CNTRS_START(hal_rx_event);
		for (ip = 0; ip < 1000; ip++)
			sum += *tt;
		MV_CPU_CNTRS_STOP(hal_rx_event);
		MV_CPU_CNTRS_SHOW(hal_rx_event);
	}

#endif	/*  */
	    return count;
}


/********************************************************************
* evb_resource_dump_read -
*
* When read from the /proc/resource_dump file this function is called
*
* Inputs: buffer_location and buffer_length and zero are not used.
*         buffer is the pointer where to post the result
* Returns: N/A
* Outputs: length of string posted
*********************************************************************/
int evb_resource_dump_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *zero,
			   void *ptr)
{
	if (offset > 0)
		return 0;
	return sprintf(buffer, "%08x\n", evb_resource_dump_result);
}


/********************************************************************
* start_regdump_memdump -
*
* Register the /proc/regdump file at the /proc filesystem
* Register the /proc/memdump file at the /proc filesystem
*
* Inputs: N/A
* Returns: N/A
* Outputs: N/A
*********************************************************************/
int __init start_resource_dump(void)
{

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	evb_resource_dump = create_proc_entry("resource_dump", 0666, &proc_root);
#else	/* */
	evb_resource_dump = create_proc_entry("resource_dump", 0666, NULL);

#endif	/* */
	evb_resource_dump->read_proc = evb_resource_dump_read;
	evb_resource_dump->write_proc = evb_resource_dump_write;
	evb_resource_dump->nlink = 1;
	return 0;
}

module_init(start_resource_dump);

/* global variables from 'regdump' */
static struct proc_dir_entry *soc_type;
static struct proc_dir_entry *board_type;
static struct proc_dir_entry *pon_type;

/********************************************************************
* soc_type_read -
*********************************************************************/
int soc_type_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *zero, void *ptr)
{
	int count = 0;
	char tmp_buffer[1000] = { 0 };

#ifdef CONFIG_ARCH_FEROCEON_MV78XX0
	char name[100] = { 0 };

#endif	/*  */
	if (offset > 0)
		return 0;

#ifdef CONFIG_ARCH_FEROCEON_MV78XX0
	mvCtrlModelRevNameGet(name);
	count += sprintf(tmp_buffer, "%s\n", name);

#endif	/*  */
#ifdef CONFIG_MV88F6281
	count += sprintf(tmp_buffer, "%s%x Rev %d\n", SOC_NAME_PREFIX, mvCtrlModelGet(), mvCtrlRevGet());

#endif	/* */
	count += mvCpuIfPrintSystemConfig(tmp_buffer, count);
	*(tmp_buffer + count) = '\0';
	sprintf(buffer, "%s", tmp_buffer);
	return count;
}


/********************************************************************
* board_type_read -
*********************************************************************/
int board_type_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *zero, void *ptr)
{
	char name_buff[50];
	if (offset > 0)
		return 0;
	mvBoardNameGet(name_buff);
	return sprintf(buffer, "%s\n", name_buff);
}


/********************************************************************
* pon_type_read -
*********************************************************************/
int pon_type_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *zero, void *ptr)
{
	MV_U32 ponType;
	if (offset > 0)
		return 0;
	ponType = mvBoardPonConfigGet();
	if (ponType == BOARD_EPON_CONFIG)
		return sprintf(buffer, "epon\n");

	else if (ponType == BOARD_GPON_CONFIG)
		return sprintf(buffer, "gpon\n");
	else
		return sprintf(buffer, "none\n");
}


/********************************************************************
* start_soc_type -
*********************************************************************/
int __init start_soc_type(void)
{
	struct proc_dir_entry *parent;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	parent = &proc_root;
#else	/*  */
	parent = NULL;
#endif	/*  */
	
	soc_type = create_proc_entry("soc_type", 0666, parent);
	soc_type->read_proc = soc_type_read;
	soc_type->write_proc = NULL;
	soc_type->nlink = 1;
	board_type = create_proc_entry("board_type", 0666, parent);
	board_type->read_proc = board_type_read;
	board_type->write_proc = NULL;
	board_type->nlink = 1;
	pon_type = create_proc_entry("pon_type", 0666, parent);
	pon_type->read_proc = pon_type_read;
	pon_type->write_proc = NULL;
	pon_type->nlink = 1;
	return 0;
}

void __exit stop_soc_type(void)
{

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	    remove_proc_entry("soc_type", &proc_root);
#else	/* */
	    remove_proc_entry("soc_type", NULL);
#endif	/* */
	    return;
}

module_init(start_soc_type);
module_exit(stop_soc_type);

