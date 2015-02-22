/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004, 2005 Cavium Networks
 *
 * Simple /proc interface to Octeon Information
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "hal.h"
#include "cvmx-app-init.h"

/**
 * User is reading /proc/octeon_info
 *
 * @param m
 * @param v
 * @return
 */
static int octeon_info_show(struct seq_file *m, void *v)
{
    extern cvmx_bootinfo_t *octeon_bootinfo;
#if defined(CONFIG_CAVIUM_RESERVE32) && CONFIG_CAVIUM_RESERVE32
    extern uint64_t octeon_reserve32_memory;
#endif

    seq_printf(m, "processor_id:        0x%x\n", read_c0_prid());
    seq_printf(m, "boot_flags:          0x%x\n", octeon_bootinfo->flags);
    seq_printf(m, "dram_size:           %u\n", octeon_bootinfo->dram_size);
    seq_printf(m, "phy_mem_desc_addr:   0x%x\n", octeon_bootinfo->phy_mem_desc_addr);
    seq_printf(m, "eclock_hz:           %u\n", octeon_bootinfo->eclock_hz);
    seq_printf(m, "dclock_hz:           %u\n", octeon_bootinfo->dclock_hz);
    seq_printf(m, "spi_clock_hz:        %u\n", octeon_bootinfo->spi_clock_hz);
    seq_printf(m, "board_type:          %u\n", octeon_bootinfo->board_type);
    seq_printf(m, "board_rev_major:     %u\n", octeon_bootinfo->board_rev_major);
    seq_printf(m, "board_rev_minor:     %u\n", octeon_bootinfo->board_rev_minor);
    seq_printf(m, "chip_type:           %u\n", octeon_bootinfo->chip_type);
    seq_printf(m, "chip_rev_major:      %u\n", octeon_bootinfo->chip_rev_major);
    seq_printf(m, "chip_rev_minor:      %u\n", octeon_bootinfo->chip_rev_minor);
    seq_printf(m, "board_serial_number: %s\n", octeon_bootinfo->board_serial_number);
    seq_printf(m, "mac_addr_base:       %02x:%02x:%02x:%02x:%02x:%02x\n",
               (int)octeon_bootinfo->mac_addr_base[0],
               (int)octeon_bootinfo->mac_addr_base[1],
               (int)octeon_bootinfo->mac_addr_base[2],
               (int)octeon_bootinfo->mac_addr_base[3],
               (int)octeon_bootinfo->mac_addr_base[4],
               (int)octeon_bootinfo->mac_addr_base[5]);
    seq_printf(m, "mac_addr_count:      %u\n", octeon_bootinfo->mac_addr_count);
#if CONFIG_CAVIUM_RESERVE32
    seq_printf(m, "32bit_shared_mem_base: 0x%lx\n", octeon_reserve32_memory);
    seq_printf(m, "32bit_shared_mem_size: 0x%x\n", octeon_reserve32_memory ? CONFIG_CAVIUM_RESERVE32<<20 : 0);
#else
    seq_printf(m, "32bit_shared_mem_base: 0x%lx\n", 0ul);
    seq_printf(m, "32bit_shared_mem_size: 0x%x\n", 0);
#endif
    return 0;
}


/**
 * /proc/octeon_info was openned. Use the single_open iterator
 *
 * @param inode
 * @param file
 * @return
 */
static int octeon_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, octeon_info_show, NULL);
}


static struct file_operations octeon_info_operations = {
	.open		= octeon_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/**
 * Module initialization
 *
 * @return
 */
static int __init octeon_info_init(void)
{
    struct proc_dir_entry *entry = create_proc_entry("octeon_info", 0, NULL);
	if (entry == NULL)
        return -1;

    entry->proc_fops = &octeon_info_operations;
	return 0;
}


/**
 * Module cleanup
 *
 * @return
 */
static void __exit octeon_info_cleanup(void)
{
    remove_proc_entry("octeon_info", NULL);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cavium Networks <support@caviumnetworks.com>");
MODULE_DESCRIPTION("Cavium Networks Octeon information interface.");
module_init(octeon_info_init);
module_exit(octeon_info_cleanup);

