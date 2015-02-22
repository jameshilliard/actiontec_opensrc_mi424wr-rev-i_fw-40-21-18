/*
 * MTD driver for Micron SyncFlash flash memory.
 *
 * Author: Jon McClintock <jonm@bluemug.com>
 *
 * Based loosely upon the LART flash driver, authored by Abraham vd Merwe
 * <abraham@2d3d.co.za>.
 *
 * Copyright 2003, Blue Mug, Inc. for Motorola, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * References:
 *
 * 	[1] Micron SyncFlash homepage
 * 		- http://www.syncflash.com/
 *
 * 	[2] MT28S4M16LC -- 4Mx16 SyncFlash memory datasheet
 * 		- http://syncflash.com/pdfs/datasheets/mt28s4m16lc_6.pdf
 *
 * 	[3] MTD internal API documentation
 * 		- http://www.linux-mtd.infradead.org/tech/
 *
 * Limitations:
 *
 * 	Even though this driver is written for Micron SyncFlash, it is quite
 * 	specific to the Motorola MX1 ADS development board.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include <asm/io.h>

/* partition support */
#define HAVE_PARTITIONS
#ifdef HAVE_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#ifndef CONFIG_ARCH_MX1ADS
#error The SyncFlash driver currently only supports the MX1 ADS platform.
#endif

/*
 * General flash configuration parameters.
 */
#define BUSWIDTH		4
#define FLASH_BLOCKSIZE		(256 * 1024 * BUSWIDTH)
#define FLASH_NUMBLOCKS		16

#define BUSWIDTH		4
#define FLASH_ADDRESS		IO_ADDRESS(MX1ADS_FLASH_BASE)

#define FLASH_MANUFACTURER	0x002C002C
#define FLASH_DEVICE_ID		0x00D300D3

/*
 * The size and extent of the bootloader in flash.
 */
#define NUM_BOOTLOADER_BLOCKS	1
#define BOOTLOADER_START	0x00000000
#define BOOTLOADER_LEN		(NUM_BOOTLOADER_BLOCKS * FLASH_BLOCKSIZE)

/*
 * The size and extent of the kernel in flash.
 */
#define NUM_KERNEL_BLOCKS	1
#define KERNEL_START		(BOOTLOADER_START + BOOTLOADER_LEN)
#define KERNEL_LEN		(NUM_KERNEL_BLOCKS * FLASH_BLOCKSIZE)

/* File system */
#define NUM_FILESYSTEM_BLOCKS	14
#define FILESYSTEM_START	(KERNEL_START + KERNEL_LEN)
#define FILESYSTEM_LEN		(NUM_FILESYSTEM_BLOCKS * FLASH_BLOCKSIZE)


/*
 * SDRAM controller register location and values. These are very specific
 * to the MX1.
 */
#define SDRAMC_REGISTER		IO_ADDRESS(0x00221004)

/*
 * This the mask we use to get the start of a block from a given address.
 */
#define BLOCK_MASK	(0xFFF00000)

/*
 * This is the A10 address line of the SyncFlash; it's used to initiate
 * a precharge command.
 */
#define SYNCFLASH_A10	(0x00100000)

/*
 * SDRAM controller MODE settings.
 */
#define CMD_NORMAL	(0x81020300)			/* Normal Mode */
#define CMD_PREC	(CMD_NORMAL + 0x10000000)	/* Precharge command */
#define CMD_AUTO	(CMD_NORMAL + 0x20000000)	/* Auto refresh */
#define CMD_LMR		(CMD_NORMAL + 0x30000000)	/* Load Mode Register */
#define CMD_LCR		(CMD_NORMAL + 0x60000000)	/* LCR Command */
#define CMD_PROGRAM	(CMD_NORMAL + 0x70000000)	/* SyncFlash Program */

/*
 * SyncFlash LCR Commands adjusted for the DBMX1 AHB internal address bus .
 */
#define LCR_READ_STATUS		(0x0001C000)	/* 0x70 */
#define LCR_READ_CONFIG		(0x00024000)	/* 0x90 */
#define LCR_ERASE_CONFIRM	(0x00008000)	/* 0x20 */
#define LCR_ERASE_NVMODE	(0x0000C000)	/* 0x30 */
#define LCR_PROG_NVMODE		(0x00028000)	/* 0xA0 */
#define LCR_SR_CLEAR		(0x00014000)	/* 0x50 */

/*
 * Status register bits
 */
#define SR_VPS_ERROR		(1 << 8)	/* Power-Up status error */
#define SR_ISM_READY		(1 << 7)	/* State machine isn't busy */
#define SR_ERASE_ERROR		(1 << 5)	/* Erase/Unprotect error */
#define SR_PROGRAM_ERROR	(1 << 4)	/* Program/Protect error */
#define SR_DEVICE_PROTECTED	(1 << 3)	/* Device is protected */
#define SR_ISM_STATUS_H		(1 << 2)	/* Bank ISM status, high bit */
#define SR_ISM_STATUS_L		(1 << 1)	/* Bank ISM status, low bit */
#define SR_DEVICE_ISM_STATUS	(1 << 0)	/* ISM is device-level */

#define SR_ERROR		(SR_VPS_ERROR|SR_ERASE_ERROR|SR_PROGRAM_ERROR|SR_DEVICE_PROTECTED)

#define STATUS_VALUE(a)		((a) | ((a) << 16))

/*
 * Device configuration register offsets
 */
#define DC_MANUFACTURER		(0 * BUSWIDTH)
#define DC_DEVICE_ID		(1 * BUSWIDTH)
#define DC_BLOCK_PROTECT	(2 * BUSWIDTH)
#define DC_DEVICE_PROTECT	(3 * BUSWIDTH)

#define FL_WORD(addr) (*(volatile unsigned long*)(addr))

static char module_name[] = "syncflash";

inline __u8 read8 (__u32 offset)
{
	return *(volatile __u8 *) (FLASH_ADDRESS + offset);
}

inline __u32 read32 (__u32 offset)
{
	return *(volatile __u32 *) (FLASH_ADDRESS + offset);
}

inline void write32 (__u32 x,__u32 offset)
{
	*(volatile __u32 *) (FLASH_ADDRESS + offset) = x;
}

static __u32 read_device_configuration_register(__u32 reg_number)
{
	__u32 tmp;

	/* Setup the SDRAM controller to issue an LCR command. */
	FL_WORD(SDRAMC_REGISTER) = CMD_LCR;

	/* Perform a read to issue the Read Device Configuration Command. */
	tmp = read32(LCR_READ_CONFIG);

	/* Return the SDRAM controller to normal mode. */
	FL_WORD(SDRAMC_REGISTER) = CMD_NORMAL;

	/* Return the value of the specified register. */
	tmp = read32(reg_number);

	return tmp;
}

/*
 * Get the status of the flash devices.
 */
static __u32 flash_read_status()
{
	__u32 status, tmp;

	/* Enter the SyncFlash Program READ/WRITE mode. */
	FL_WORD(SDRAMC_REGISTER) = CMD_PROGRAM;

	/* Read the status register. */
	status = read32(LCR_READ_STATUS);

	/* Clear the status register. */
	FL_WORD(SDRAMC_REGISTER) = CMD_LCR;
	tmp = read32(LCR_SR_CLEAR);

	/* Return to Normal mode. */
	FL_WORD(SDRAMC_REGISTER) = CMD_NORMAL;

	return status;
}

/*
 * Loop until both write state machines are ready.
 */
static __u32 flash_status_wait()
{
	__u32 status;
	do {
		status = flash_read_status();
	} while ((status & STATUS_VALUE(SR_ISM_READY)) !=
			STATUS_VALUE(SR_ISM_READY));
	return status;
}

/*
 * Loop until the Write State machine is ready, then do a full error
 * check.  Clear status and leave the flash in Read Array mode; return
 * 0 for no error, -1 for error.
 */
static int flash_status_full_check()
{
	__u32 status;

	status = flash_status_wait() & STATUS_VALUE(SR_ERROR);
	return status ? -EIO : 0;
}

/*
 * Return the flash to the normal mode.
 */
static void flash_normal_mode()
{
	__u32 tmp;

	/* First issue a precharge all command. */
	FL_WORD(SDRAMC_REGISTER) = CMD_PREC;
	tmp = read32(SYNCFLASH_A10);

	/* Now place the SDRAM controller in Normal mode. */
	FL_WORD(SDRAMC_REGISTER) = CMD_NORMAL;
}

/*
 * Probe for SyncFlash memory on MX1ADS board.
 *
 * Returns 1 if we found SyncFlash memory, 0 otherwise.
 */
static int flash_probe (void)
{
	__u32 manufacturer, device_id;

	/* For some reason, the first read doesn't work, so we do it
	 * twice. */
	manufacturer = read_device_configuration_register(DC_MANUFACTURER);
	manufacturer = read_device_configuration_register(DC_MANUFACTURER);
	device_id = read_device_configuration_register(DC_DEVICE_ID);

	printk("SyncFlash probe: manufacturer 0x%08lx, device_id 0x%08lx\n",
		manufacturer, device_id);
	return (manufacturer == FLASH_MANUFACTURER &&
		device_id == FLASH_DEVICE_ID);
}

/*
 * Erase one block of flash memory at offset ``offset'' which is any
 * address within the block which should be erased.
 *
 * Returns 0 if successful, -1 otherwise.
 */
static inline int erase_block (__u32 offset)
{
	__u32 tmp;

	/* Mask off the lower bits of the address to get the first address
	 * in the flash block. */
	offset &= (__u32)BLOCK_MASK;

	/* Perform a read and precharge of the bank before the LCR|ACT|WRIT
	 * sequence to avoid the inadvertent precharge command occurring
	 * during the LCR_ACT_WRIT sequence. */
	FL_WORD(SDRAMC_REGISTER) = CMD_NORMAL;
	tmp = read32(offset);
	FL_WORD(SDRAMC_REGISTER) = CMD_PREC;
	tmp = read32(offset);

	/* Now start the actual erase. */

	/* LCR|ACT|WRIT sequence */
	FL_WORD(SDRAMC_REGISTER) = CMD_LCR;
	write32(0, offset + LCR_ERASE_CONFIRM);

	/* Return to normal mode to issue the erase confirm. */
	FL_WORD(SDRAMC_REGISTER) = CMD_NORMAL;
	write32(0xD0D0D0D0, offset);

	if (flash_status_full_check()) {
		printk (KERN_WARNING "%s: erase error at address 0x%.8x.\n",
			module_name, offset);
		return (-1);
	}

	flash_normal_mode();

	return 0;
}

static int flash_erase (struct mtd_info *mtd,struct erase_info *instr)
{
	__u32 addr,len;
	int i,first;

	/* sanity checks */
	if (instr->addr + instr->len > mtd->size) return (-EINVAL);

	/*
	 * check that both start and end of the requested erase are
	 * aligned with the erasesize at the appropriate addresses.
	 *
	 * skip all erase regions which are ended before the start of
	 * the requested erase. Actually, to save on the calculations,
	 * we skip to the first erase region which starts after the
	 * start of the requested erase, and then go back one.
	 */
	for (i = 0; (i < mtd->numeraseregions) &&
		    (instr->addr >= mtd->eraseregions[i].offset); i++) ;
	i--;

	/*
	 * ok, now i is pointing at the erase region in which this
	 * erase request starts. Check the start of the requested
	 * erase range is aligned with the erase size which is in
	 * effect here.
	 */
	if (instr->addr & (mtd->eraseregions[i].erasesize - 1))
		return (-EINVAL);

	/* Remember the erase region we start on */
	first = i;

	/*
	 * next, check that the end of the requested erase is aligned
	 * with the erase region at that address.
	 *
	 * as before, drop back one to point at the region in which
	 * the address actually falls
	 */
	for (;
	     (i < mtd->numeraseregions) &&
	     ((instr->addr + instr->len) >= mtd->eraseregions[i].offset) ;
	     i++) ;
	i--;

	/* is the end aligned on a block boundary? */
	if ((instr->addr + instr->len) & (mtd->eraseregions[i].erasesize - 1))
		return (-EINVAL);

	addr = instr->addr;
	len = instr->len;

	i = first;

	/* now erase those blocks */
	while (len)
	{
		if (erase_block (addr))
		{
			instr->state = MTD_ERASE_FAILED;
			return (-EIO);
		}

		addr += mtd->eraseregions[i].erasesize;
		len -= mtd->eraseregions[i].erasesize;

		if (addr == (mtd->eraseregions[i].offset +
				(mtd->eraseregions[i].erasesize *
				 mtd->eraseregions[i].numblocks)))
			i++;
	}

	instr->state = MTD_ERASE_DONE;
	if (instr->callback) instr->callback (instr);

	return (0);
}

static int flash_read (struct mtd_info *mtd, loff_t from,
		       size_t len, size_t *retlen, u_char *buf)
{
	/* Sanity checks. */
	if (!len) return (0);
	if (from + len > mtd->size) return (-EINVAL);

	/* Ensure that we are in normal mode. */
	flash_normal_mode();

	/* We always read len bytes. */
	*retlen = len;

	/* first, we read bytes until we reach a dword boundary */
	if (from & (BUSWIDTH - 1))
	{
		int gap = BUSWIDTH - (from & (BUSWIDTH - 1));
		while (len && gap--) *buf++ = read8(from++), len--;
	}

	/* now we read dwords until we reach a non-dword boundary */
	while (len >= BUSWIDTH)
	{
		*((__u32 *) buf) = read32(from);

		buf += BUSWIDTH;
		from += BUSWIDTH;
		len -= BUSWIDTH;
	}

	/* top up the last unaligned bytes */
	if (len & (BUSWIDTH - 1))
		while (len--) *buf++ = read8(from++);

	return (0);
}

/*
 * Write one dword ``x'' to flash memory at offset ``offset''. ``offset''
 * must be 32 bits, i.e. it must be on a dword boundary.
 *
 * Returns 0 if successful, -1 otherwise.
 */
static int flash_write_dword(__u32 offset, __u32 x)
{
	__u32 tmp;

	/* First issue a precharge all command. */
	FL_WORD(SDRAMC_REGISTER) = CMD_PREC;
	tmp = read32(SYNCFLASH_A10);

	/* Enter the SyncFlash programming mode. */
	FL_WORD(SDRAMC_REGISTER) = CMD_PROGRAM;
	write32(x, offset);

	/* Wait for the write to complete. */
	flash_status_wait();

	/* Return to normal mode. */
	flash_normal_mode();

	return 0;
}

static int flash_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,const u_char *buf)
{
	__u8 tmp[4];
	int i,n;

	*retlen = 0;

	/* Sanity checks */
	if (!len) return (0);
	if (to + len > mtd->size) return (-EINVAL);

	/* First, we write a 0xFF.... padded byte until we reach a
	 * dword boundary. */
	if (to & (BUSWIDTH - 1))
	{
		__u32 aligned = to & ~(BUSWIDTH - 1);
		int gap = to - aligned;

		i = n = 0;

		while (gap--) tmp[i++] = 0xFF;
		while (len && i < BUSWIDTH) tmp[i++] = buf[n++], len--;
		while (i < BUSWIDTH) tmp[i++] = 0xFF;

		if (flash_write_dword(aligned, *((__u32 *) tmp)))
			return (-EIO);

		to += n;
		buf += n;
		*retlen += n;
	}

	/* Now we write dwords until we reach a non-dword boundary. */
	while (len >= BUSWIDTH)
	{
		if (flash_write_dword (to,*((__u32 *) buf))) return (-EIO);

		to += BUSWIDTH;
		buf += BUSWIDTH;
		*retlen += BUSWIDTH;
		len -= BUSWIDTH;
	}

	/* Top up the last unaligned bytes, padded with 0xFF.... */
	if (len & (BUSWIDTH - 1))
	{
		i = n = 0;

		while (len--) tmp[i++] = buf[n++];
		while (i < BUSWIDTH) tmp[i++] = 0xFF;

		if (flash_write_dword (to,*((__u32 *) tmp))) return (-EIO);

		*retlen += n;
	}

	return flash_status_full_check();
}



#define NB_OF(x) (sizeof (x) / sizeof (x[0]))

static struct mtd_info mtd;

static struct mtd_erase_region_info erase_regions[] =
{
	/* flash blocks */
	{
		offset: 	0x00000000,
		erasesize: 	FLASH_BLOCKSIZE,
		numblocks: 	FLASH_NUMBLOCKS
	},
};

#ifdef HAVE_PARTITIONS
static struct mtd_partition syncflash_partitions[] =
{
   /* bootloader */
   {
	       name: "bootloader",
	     offset: BOOTLOADER_START,
	       size: BOOTLOADER_LEN,
	 mask_flags: 0
   },
   /* Kernel */
   {
	       name: "kernel",
	     offset: KERNEL_START,			/* MTDPART_OFS_APPEND */
	       size: KERNEL_LEN,
	 mask_flags: 0
   },
   /* file system */
   {
	       name: "file system",
	     offset: FILESYSTEM_START,			/* MTDPART_OFS_APPEND */
	       size: FILESYSTEM_LEN,			/* MTDPART_SIZ_FULL */
	 mask_flags: 0
   }
};
#endif

int __init syncflash_init (void)
{
	int result;

	memset (&mtd,0,sizeof (mtd));

	printk ("MTD driver for Micron SyncFlash.\n");
	printk ("%s: Probing for SyncFlash on MX1ADS...\n",module_name);

	if (!flash_probe ())
	{
		printk (KERN_WARNING "%s: Found no SyncFlash devices\n",
			module_name);
		return (-ENXIO);
	}

	printk ("%s: Found a SyncFlash device.\n",module_name);

	mtd.name = module_name;
	mtd.type = MTD_NORFLASH;
	mtd.flags = MTD_CAP_NORFLASH;
	mtd.size = FLASH_BLOCKSIZE * FLASH_NUMBLOCKS;

	mtd.erasesize = FLASH_BLOCKSIZE;
	mtd.numeraseregions = NB_OF(erase_regions);
	mtd.eraseregions = erase_regions;

	mtd.module = THIS_MODULE;

	mtd.erase = flash_erase;
	mtd.read = flash_read;
	mtd.write = flash_write;

#ifndef HAVE_PARTITIONS
	result = add_mtd_device(&mtd);
#else
	result = add_mtd_partitions(&mtd,
				    syncflash_partitions,
				    NB_OF(syncflash_partitions));
#endif

	return (result);
}

void __exit syncflash_exit (void)
{
#ifndef HAVE_PARTITIONS
	del_mtd_device (&mtd);
#else
	del_mtd_partitions (&mtd);
#endif
}

module_init (syncflash_init);
module_exit (syncflash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jon McClintock <jonm@bluemug.com>");
MODULE_DESCRIPTION("MTD driver for Micron MT28S4M16LC SyncFlash on MX1ADS board");


