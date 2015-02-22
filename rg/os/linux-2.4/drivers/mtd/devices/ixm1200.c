/*
 * drivers/mtd/devices/ixm1200.c
 *
 * MTD driver for the 28F320C3 Flash Memory on IXM1200 NPU Base Card
 *
 * Inspired Abraham vd Merwe <abraham@2d3d.co.za> 28F160F3 Flash memory driver on Lart.
 *
 * Copyright (c) 2001, 2d3D, Inc.
 *
 * Ported to 28F320C3 by Nikunj A Dadhania <nikunj.d@smartm.com> 
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 */

/* Comment this for prodution release */ 
// #define IXM1200_DEBUG */

/* partition support */
#define HAVE_PARTITIONS

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include <linux/delay.h>
#ifdef HAVE_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#include <asm/io.h>

#ifndef CONFIG_ARCH_IXM1200
#error This is for IXM1200 NPU Base Board only 
#endif

static char module_name[] = "ixm1200";

/*
 * These values is specific to 28FxxC3 flash memory.
 * See in "3 Volt Advanced+ Boot Block Flash Memory" Intel Datasheet
 *
 * The documentation is actually incorrect.  It says that each param
 * block is 4096 and that the 32Mb(4MB) part has 63 main blocks
 * at 32K each.  32K * 63 + 4K * 8 == 16Mb (2MB).  So either the
 * block size or the block count is wrong in the docs.
 *
 */
#define FLASH_BLOCKSIZE_PARAM		(8192)
#define FLASH_NUMBLOCKS_16m_PARAM	8
#define FLASH_NUMBLOCKS_8m_PARAM	8

/*
 * These values is specific to 28FxxxC3 flash memory.
 * See in "3 Volt Advanced+ Boot Block Flash Memory" Intel Datasheet
 */
#define FLASH_BLOCKSIZE_MAIN		(65536)
#define FLASH_NUMBLOCKS_16m_MAIN	63
#define FLASH_NUMBLOCKS_8m_MAIN		15

/*
 * These values are specific to IMX1200
 */

/* don't change this - a lot of the code _will_ break if you change this */
#define BUSWIDTH		4 

/*
 * See Table 6 in "3 Volt Advanced+ Boot Block Flash Memory" Intel Datasheet
 */
#define F28CMD_READ_ID        0x90909090
#define F28CMD_ERASE_SETUP    0x20202020
#define F28CMD_ERASE_CONFIRM  0xd0d0d0d0
#define F28CMD_PROGRAM_SETUP  0x40404040
#define F28CMD_READ_ARRAY     0xFFFFFFFF
#define F28CMD_READ_STATUS    0x70707070
#define F28CMD_CLEAR_STATUS   0x50505050
#define F28CMD_RESET          F28CMD_READ_ARRAY
#define F28CMD_CFGSETUP       0x60606060
#define F28CMD_CFGUNLOCK      0xd0d0d0d0

#define F28_MANUF_ID          0x0089
#define FTYPE_28F320C3B       0x88C5

#define BOOT_BLOCK_GROUP_SIZE 0x10000
#define FLASH_DEVICE_SIZE     0x400000

#define BIT(i) (1 << (i))

unsigned long flash_base = 0;

void ixm1200_clear_status(void)
{
	volatile unsigned long *lptr = (unsigned long  *)flash_base;
 	*lptr = F28CMD_CLEAR_STATUS;
}

void ixm1200_reset(void)
{
	volatile unsigned long *lptr = (unsigned long  *)flash_base;
	*lptr = F28CMD_RESET;
}

static __u8 ixm1200_read8 (__u32 offset)
{
	volatile __u8 *data = (__u8 *) (flash_base + offset);
#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG "%s(): 0x%.8x -> 0x%.2x\n",__FUNCTION__,offset,*data);
#endif
	return (*data);
}

static __u32 ixm1200_read32 (__u32 offset)
{
	volatile __u32 *data = (__u32 *) (flash_base + offset);
#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG "%s(): 0x%.8x -> 0x%.8x\n",__FUNCTION__,offset,*data);
#endif
	return (*data);
}


/*
 * If I don't use this delay the hardware which I am working is not
 * behaving consistently so the delay can be removed if not required.  
 */
void si_write(volatile unsigned long *lptr, unsigned long data)
{
	*lptr = F28CMD_PROGRAM_SETUP;
	mdelay(1);
	*lptr = data;
	
	/* after writing the flash will always be in read status register mode */
	
	while(!(*lptr & BIT(7))) /* status of the first device */
		;
} 

static int ixm1200_probe (void)
{
	unsigned long l_mfrid;
	unsigned short mfrid, devid;

	/* setup "Read Identifier Codes" mode */
	*(unsigned long *)flash_base = F28CMD_READ_ID;

	l_mfrid = ixm1200_read32 (0x00000000);
	mfrid = (unsigned short)l_mfrid;
	devid = (unsigned short)(l_mfrid >>16);

	/* put the flash back into command mode */
	ixm1200_reset();

	printk("Flash manufacturer id %x device id %x at %x\n", mfrid, devid, flash_base);
   
	return (mfrid == F28_MANUF_ID && (devid == FTYPE_28F320C3B ));
}

/*
 * Erase one block of flash memory at offset ``offset'' which is any
 * address within the block which should be erased.
 *
 * Returns 1 if successful, 0 otherwise.
 */
static inline int ixm1200_erase_block (__u32 offset)
{
	volatile unsigned long *data = 
		(unsigned long*)(flash_base + 2 * offset);
	int ix;

#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG "%s(): %#010x\n",__FUNCTION__,data);
#endif

	*data = F28CMD_CFGSETUP;
	*data = F28CMD_CFGUNLOCK;

	*data = F28CMD_ERASE_SETUP;
	/* I am playing little safe. This will be removed when I am sure that its not really required */ 
	mdelay(1);
	*data = F28CMD_ERASE_CONFIRM;


	/* after erasing, the flash will always be in read status register mode */

	for(ix = (BUSWIDTH/2) - 1; ix >=0; ix--)
	{
		while(!(*data & BIT(7))) /* status of the first device */
			;

		if(*data & BIT(6)) {
			printk("Erasing suspended status %x\n", *data);
			*data = F28CMD_ERASE_CONFIRM;
			printk("Trying to resume erasing\n");
			while(!(*data & BIT(7))) /* status of the first device */
				;

		}

		if((*data & (BIT(6) | BIT(5) | BIT(3) | BIT(1))) ) {
			printk("%s(): Error in erasing flash, status is %x\n",__FUNCTION__, *data);
			ixm1200_clear_status();
			return 0;
		}
		ixm1200_clear_status();
		ixm1200_reset();
		return (1);
	}
}

static int ixm1200_erase (struct mtd_info *mtd, struct erase_info *instr)
{
	__u32 addr,len;
	int i,first;

#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG "%s(addr = 0x%.8x, len = %d)\n",__FUNCTION__,instr->addr,instr->len);
#endif

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
	for (i = 0; i < mtd->numeraseregions && instr->addr >= mtd->eraseregions[i].offset; i++) ;
	i--;

        /*
	 * ok, now i is pointing at the erase region in which this
	 * erase request starts. Check the start of the requested
	 * erase range is aligned with the erase size which is in
	 * effect here.
	 */
	if (instr->addr & (mtd->eraseregions[i].erasesize - 1)) return (-EINVAL);

	/* Remember the erase region we start on */
	first = i;

	/*
	 * next, check that the end of the requested erase is aligned
	 * with the erase region at that address.
	 *
	 * as before, drop back one to point at the region in which
	 * the address actually falls
	 */
	for (; i < mtd->numeraseregions && instr->addr + instr->len >= mtd->eraseregions[i].offset; i++) ;
	i--;

	/* is the end aligned on a block boundary? */
	if ((instr->addr + instr->len) & (mtd->eraseregions[i].erasesize - 1)) 
		return (-EINVAL);

	addr = instr->addr;
	len = instr->len;

	i = first;

#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG "%s(erasesize = 0x%.8x)\n",__FUNCTION__,mtd->eraseregions[i].erasesize);
#endif

	/* now erase those blocks */
	while (len)
	{
		if(!ixm1200_erase_block (addr))
		{
			instr->state = MTD_ERASE_FAILED;
			return (-EIO);
		}

		addr += mtd->eraseregions[i].erasesize;
		len -= mtd->eraseregions[i].erasesize;

		if (addr == mtd->eraseregions[i].offset + (mtd->eraseregions[i].erasesize * mtd->eraseregions[i].numblocks)) i++;
	}

	instr->state = MTD_ERASE_DONE;
	if (instr->callback) instr->callback (instr);

	return (0);
}

static int ixm1200_read (struct mtd_info *mtd,loff_t from,size_t len,size_t *retlen,u_char *buf)
{
#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG "%s(from = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) from,len);
#endif

	/* sanity checks */
	if (!len) return (0);
	if (from + len > mtd->size) return (-EINVAL);

	/* we always read len bytes */
	*retlen = len;

	/* first, we read bytes until we reach a dword boundary */
	if (from & (BUSWIDTH - 1))
	{
		int gap = BUSWIDTH - (from & (BUSWIDTH - 1));
		
		while (len && gap--) *buf++ = ixm1200_read8(from++), len--;

	}

	/* now we read dwords until we reach a non-dword boundary */
	while (len >= BUSWIDTH)
	{
		*((__u32 *) buf) = ixm1200_read32(from);

		buf += BUSWIDTH;
		from += BUSWIDTH;
		len -= BUSWIDTH;
	}

	/* top up the last unaligned bytes */
	if (len & (BUSWIDTH - 1))
		while (len--) *buf++ = ixm1200_read8(from++);

	return (0);
}

/*
 * Write one dword ``x'' to flash memory at offset ``offset''. ``offset''
 * must be 32 bits, i.e. it must be on a dword boundary.
 *
 * Returns 1 if successful, 0 otherwise.
 */
static inline int ixm1200_write_dword (__u32 offset, __u32 x)
{

/* 
 * Due to some unidentified reasons which I not able to comprehend the
 * char driver that Intel has provided writes to the offset * 2 if he
 * wants to write a dword at offset with respect to the flash_base.  
 *
 */
   unsigned long *lptr = (unsigned long *)(flash_base + 2 * offset);
#ifdef IXM1200_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x <- 0x%.8x\n",__FUNCTION__,offset,x);
#endif

   *lptr = F28CMD_CFGSETUP;
   *lptr = F28CMD_CFGUNLOCK;

#ifdef IXM1200_DEBUG
   printk(KERN_DEBUG "programming %x in location %x\n", x & 0xffff, lptr);
#endif

   si_write(lptr++ ,x & 0xFFFF);
   si_write(lptr++ ,(x >> 16) & 0xFFFF);

   if ((*lptr) & (BIT(5) | BIT(4) | BIT(3) | BIT(1)))
   {
	   printk("Status is : %x\n",*lptr);
	   ixm1200_clear_status();
	   ixm1200_reset();
	   return -ENXIO;
   }
   ixm1200_reset();
   return (1);
}


static int ixm1200_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,const u_char *buf)
{
	int retVal=0, i, n, gap, count=0;
	unsigned long *lptr = (unsigned long *)(flash_base + 2 * to);
	unsigned long *lbuf = (unsigned long *)buf;
	__u8 tmp[4];

#ifdef IXM1200_DEBUG
	printk ("%s(to = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) to,len);
#endif

	*retlen = 0;

	/* sanity checks */
	if (!len) return (0);
	if (to + len > mtd->size) return (-EINVAL);
	/* Write until we hit the dword boundary */
	if(to & (BUSWIDTH - 1))
	{
		__u32 aligned = to & ~(BUSWIDTH - 1);
		gap = to - aligned;
		*((u32 *)tmp) = ixm1200_read32(aligned);
	
		i = n = 0;

		while(gap--) i++;
		while(len-- && i < BUSWIDTH) tmp[i++] = buf[n++];
		// while(i < BUSWIDTH) tmp[i++]=0xFF;
			
		lbuf=(unsigned long *)tmp;
		
		if(ixm1200_write_dword(aligned, tmp) != 1) return -EIO;

		to += n;
		buf += n;
		*retlen +=n;
	}

	while(len >=BUSWIDTH)
	{
		u32 val = *((u32 *)buf);

		if(ixm1200_write_dword(to, val) != 1) return -EIO;
		to += BUSWIDTH;
		buf += BUSWIDTH;
		*retlen += BUSWIDTH;
		len -= BUSWIDTH;
	}
	n = *retlen;
	if (len & (BUSWIDTH - 1))
	{
		*((u32 *)tmp) = ixm1200_read32(to);

		printk(KERN_DEBUG "Read (%#010x) from flash\n", *((u32 *)tmp));

		i = n = 0;
		while (len--) {
			printk(KERN_DEBUG "Unaligned bytes %c(%#04x) %d %d\n", buf[n], buf[n], n, i);
			tmp[i++] = buf[n++];
		}

		printk(KERN_DEBUG "Write (%#010x) to flash\n", *((u32 *)tmp));

		if(ixm1200_write_dword(to, *((__u32 *)tmp)) != 1) return -EIO;
		*retlen += n;
	}

	ixm1200_reset();
	return (0);
}


#define NB_OF(x) (sizeof (x) / sizeof (x[0]))

static struct mtd_info mtd;

static struct mtd_erase_region_info si_erase_regions[] =
{
	/* parameter blocks */
	{
		offset: 0x00000000,
		erasesize: FLASH_BLOCKSIZE_PARAM,
		numblocks: FLASH_NUMBLOCKS_16m_PARAM
	},
	/* main blocks */
	{
		offset: FLASH_BLOCKSIZE_PARAM * FLASH_NUMBLOCKS_16m_PARAM,
		erasesize: FLASH_BLOCKSIZE_MAIN,
		numblocks: FLASH_NUMBLOCKS_16m_MAIN
	}
};

#ifdef HAVE_PARTITIONS
static struct mtd_partition si_partitions[] =
{
	{
		name: "BootManager",
	 	offset: 0,			
		size: 0x200000,
		mask_flags: MTD_WRITEABLE
	},
	{
		name: "file system",
		offset: MTDPART_OFS_APPEND,
		size: MTDPART_SIZ_FULL
	}
};
#endif

int __init ixm1200_init (void)
{
	int result;
	memset (&mtd,0,sizeof (mtd));

	flash_base = (unsigned long)ioremap(0x00000000, FLASH_DEVICE_SIZE*2);
	
	printk ("%s: Probing for 28F320x3 flash on IXM1200\n",module_name);

	if (!ixm1200_probe ())
	{
		printk (KERN_WARNING "%s: Found no IXM1200 compatible flash device\n",module_name);
		return (-ENXIO);
	}

	mtd.name = module_name;
	mtd.type = MTD_NORFLASH;
	mtd.flags = MTD_CAP_NORFLASH;
	mtd.size = FLASH_BLOCKSIZE_PARAM * FLASH_NUMBLOCKS_16m_PARAM + FLASH_BLOCKSIZE_MAIN * FLASH_NUMBLOCKS_16m_MAIN;
	mtd.erasesize = FLASH_BLOCKSIZE_MAIN;
	mtd.numeraseregions = NB_OF (si_erase_regions);
	mtd.eraseregions = si_erase_regions;
	mtd.module = THIS_MODULE;
	mtd.erase = ixm1200_erase;
	mtd.read = ixm1200_read;
	mtd.write = ixm1200_write;

#ifdef IXM1200_DEBUG
	printk (KERN_DEBUG
		"mtd.name = %s\n"
		"mtd.size = 0x%.8x (%uM)\n"
		"mtd.erasesize = 0x%.8x (%uK)\n"
		"mtd.numeraseregions = %d\n",
		mtd.name,
		mtd.size,mtd.size / (1024*1024),
		mtd.erasesize,mtd.erasesize / 1024,
		mtd.numeraseregions);

	if (mtd.numeraseregions)
		for (result = 0; result < mtd.numeraseregions; result++)
			printk (KERN_DEBUG
				"\n\n"
				"mtd.eraseregions[%d].offset = 0x%.8x\n"
				"mtd.eraseregions[%d].erasesize = 0x%.8x (%uK)\n"
				"mtd.eraseregions[%d].numblocks = %d\n",
				result,mtd.eraseregions[result].offset,
				result,mtd.eraseregions[result].erasesize,mtd.eraseregions[result].erasesize / 1024,
				result,mtd.eraseregions[result].numblocks);

#ifdef HAVE_PARTITIONS
	printk ("\npartitions = %d\n",NB_OF (si_partitions));

	for (result = 0; result < NB_OF (si_partitions); result++)
		printk (KERN_DEBUG
			"\n\n"
			"si_partitions[%d].name = %s\n"
			"si_partitions[%d].offset = 0x%.8x\n"
			"si_partitions[%d].size = 0x%.8x (%uK)\n",
			result,si_partitions[result].name,
			result,si_partitions[result].offset,
			result,si_partitions[result].size,si_partitions[result].size / 1024);
#endif
#endif

#ifndef HAVE_PARTITIONS
	result = add_mtd_device (&mtd);
#else
	result = add_mtd_partitions (&mtd,si_partitions,NB_OF (si_partitions));
#endif

	return (result);
}

void __exit ixm1200_exit (void)
{
#ifndef HAVE_PARTITIONS
	del_mtd_device (&mtd);
#else
	del_mtd_partitions (&mtd);
#endif
}

module_init (ixm1200_init);
module_exit (ixm1200_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikunj A. Dadhania <nikunj.d@smartm.com>");
MODULE_DESCRIPTION("MTD driver for Intel 28F320C3 on IXM1200 board");


