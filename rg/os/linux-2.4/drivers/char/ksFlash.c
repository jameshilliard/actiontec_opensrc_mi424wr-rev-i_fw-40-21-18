/*
 *	ksFlash:	Flash device for AMD flash memory
 *
 *	(c) Copyright 2002 Kam Lee <Kamlee@kendin.com>, All Rights Reserved.
 *				http://www.micrel.com
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *	
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide 
 *	warranty for any of this software. This material is provided 
 *	"AS-IS" and at no charge.	
 *
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 *	Software only watchdog driver. Unlike its big brother the WDT501P
 *	driver this won't always recover a failed machine.
 *
 *  03/96: Angelo Haritsis <ah@doc.ic.ac.uk> :
 *	Modularised.
 *	Added soft_margin; use upon insmod to change the timer delay.
 *	NB: uses same minor as wdt (WATCHDOG_MINOR); we could use separate
 *	    minors.
 *
 *  20020903 Kam Lee
 *	officially release 1.0
 */
 
#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>

// asm is linked to the appropriate hardware directory
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/interrupt.h>
#include <asm/arch/platform.h>
#include "ksFlash.h"
#include "ksFlashIoc.h"

MODULE_LICENSE("GPL");

#define KSFLASH_MAJOR   245
#define VERSION         "1.0"
static void *FlashBasePtr=NULL;
static char *BlockBuffer=NULL;

/************************************************************************************/
/*                          Board Specific                                          */
/************************************************************************************/

/* Manufacturers */
#define MANUFACTURER_AMD                	0x0001
/* Commands */
#define AMD_CMD_UNLOCK_DATA_1		        0x00AA
#define AMD_CMD_UNLOCK_DATA_2		        0x0055
#define AMD_CMD_MANUFACTURER_UNLOCK_DATA	0x0090
#define AMD_CMD_UNLOCK_BYPASS_MODE		0x0020
#define AMD_CMD_PROGRAM_UNLOCK_DATA		0x00A0
#define AMD_CMD_RESET_DATA			0x00F0
#define AMD_CMD_SECTOR_ERASE_UNLOCK_DATA	0x0080
#define AMD_CMD_SECTOR_ERASE_UNLOCK_DATA_2	0x0030
#define AMD_CMD_UNLOCK_SECTOR		        0x0060

#define AMD_EMPTY_DATA                          0xFFFFFFFF

/* address */
#define AMD_ADDR_MANUFACTURER		        0x0000
#define AMD_ADDR_DEVICE_ID			0x0001

/* CFI standard */
/* All commands are defined in terms of 32-bit wide flash.*/
#define BLOCK_WRITE_MODE	0xE8E8E8E8
#define BLOCK_LOCK_BITS		0x60606060
#define BLOCK_LOCK_CONFIRM	0x01010101
#define CFI_DATA_OFFS		0x20202020

#define CFI_QUERY_CMD	        0x98
#define CFI_WRITE_CMD	        0x40    
#define CFI_ERASEBLK_CMD        0x20
#define CFI_ERASECNF_CMD        0xD0
#define CFI_CLEARSTS_CMD        0x50
#define CFI_READSTS_CMD         0x70
#define CFI_READ_ARRAY_CMD      0xF0    

#define STATUS_LOCKED		0x00010001
#define LOCK_ERROR		0x00100010
#define ERASE_ERROR		0x20
#define WRITE_ERROR		0x10

/* Offset assume byte accesses */
#define CFI_DATA_OFFSET		0x10
#define CFI_QUERY_OFFSET        0x55
#define CFI_SYS_SIZE_OFF	0x27
#define SYS_INFO_WB_OFF		0x2A
#define SYS_ERASE_REGION_OFF    0x2C
#define SYS_ERASE_SIZE_OFF	0x2D

#define MAX_WRITE_BUFF		0x0F

#define CFI_BLOCK_SIZE          ((thisFlashChip.cfiGeom.EraseRegionInfo2 >> 16)*256*CFI_INTERLEAVE)

#define AMD_D5_MASK	        0x20
#define AMD_D5_MASK8x2          0x2020
#define AMD_D5_MASK16x2         0x00200020
#define AMD_D5_MASK8x4          0x20202020
#define AMD_D6_MASK	        0x40

#define KS8695_BUF_REG          IO_ADDRESS(0x03FF403C)

#define io_write(v,a)           (*(volatile unsigned int  *)(a) = (v))
#define io_read(a)              (*(volatile unsigned int  *)(a))


struct CFIGeometry {
  __u8  DevSize;
  __u16 InterfaceDesc;
  __u16 MaxBufWriteSize;
  __u8  NumEraseRegions;
  __u32 EraseRegionInfo;
  __u32 EraseRegionInfo2;
  __u32 EraseRegionInfo3;
  __u32 EraseRegionInfo4;
}__attribute__((packed));

struct CFIPrivate {
  int    state;
  spinlock_t mutex;
  __u32  totalBytes;
  struct CFIGeometry cfiGeom;
};

/************************************************************************
 ARM Flash File Format								
 ***********************************************************************/
#define FOOTER_OFFSET       0xFFEC
#define FOOTER_SIGNATURE    0xA0FFFF9F   /* ARM Flash Library */
typedef unsigned long   unsigned32;

typedef struct FooterType
{
    void *infoBase;             /* Address of first word of ImageFooter  */
    char *blockBase;            /* Start of area reserved by this footer */
    unsigned32 signature;       /* 'Magic' number proves it's a footer   */
    unsigned32 type;            /* Area type: ARM Image, SIB, customer   */
    unsigned32 checksum;        /* Just this structure                   */
} tFooter;

typedef void (*PFN) (void);
typedef struct ImageInfoType
{
    unsigned32 bootFlags;       /* Boot flags, compression etc.          */
    unsigned32 imageNumber;     /* Unique number, selects for boot etc.  */
    char *loadAddress;          /* Address program should be loaded to   */
    unsigned32 length;          /* Actual size of image                  */
    PFN address;                /* Image is executed from here           */
    char name[16];              /* Null terminated                       */
    char *headerBase;           /* Flash Address of any stripped header  */
    unsigned32 header_length;   /* Length of header in memory            */
    unsigned32 headerType;      /* AIF, RLF, s-record etc.               */
    unsigned32 checksum;        /* Image checksum (inc. this struct)     */
} tImageInfo;


static struct CFIPrivate thisFlashChip;
static int    Flash_Device_Size = 0;

static int kflash_read_word(__u32 adr, char * buffer, int size);

static inline void amd_send_unlock(char * basePtr)
{
    cfi_send_gen_cmd(AMD_CMD_UNLOCK_DATA_1, 0x555, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
    cfi_send_gen_cmd(AMD_CMD_UNLOCK_DATA_2, 0x2aa, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
}
 
static inline void amd_reset_bypass(char * basePtr)
{
    cfi_send_gen_cmd(AMD_CMD_MANUFACTURER_UNLOCK_DATA, 0x555, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
    cfi_send_gen_cmd(0, 0, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
}
 
static inline void amd_send_cmd(char * basePtr, __u32 cmd)
{
    amd_send_unlock(basePtr);
    cfi_send_gen_cmd(cmd, 0, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
}
 
static inline void amd_send_cmd_to_addr(char * basePtr, __u32 cmd, char * address)
{
    amd_send_unlock(basePtr);
    cfi_send_gen_cmd(cmd, 0, (__u32) address, CFI_DEVICETYPE, CFI_INTERLEAVE);
}

/* No bits toggling: no erase/program in operation. */
static const int  WRITE_STATUS_OK = 0;
/* This sector is currently erase-suspended. */
static const int  WRITE_STATUS_ERASE_SUSPEND = 1;
/* An erase/program is currently in operation. */
static const int  WRITE_STATUS_BUSY = 2;
/* Operation exceeded internal pulse count time-limits. */
static const int  WRITE_STATUS_TIMEOUT = 3;

static int amd_get_status8(__u32 *address, __u32 data)
{
    udelay(20);
    if ( CFI_INTERLEAVE == 1 ) {
      __u8 data1;	 	    
      data1 = (__u8) cfi_read((__u32)address);
      if (data1 == (__u8)data )
        return WRITE_STATUS_OK;
    
      while ( 0 ==(data1 & AMD_D5_MASK)){
        udelay(20);
        data1 = (__u8) cfi_read((__u32)address);
        if (data1 == (__u8)data)
            return WRITE_STATUS_OK;
      }
      udelay(20);
      data1 = (__u8) cfi_read((__u32)address);
      if (data1 == (__u8)data)
        return WRITE_STATUS_OK;
      return WRITE_STATUS_TIMEOUT;
    }
    else if ( CFI_INTERLEAVE == 2 && cfi_buswidth_is_2() ){
       __u16 data1;
       data1 = (__u16) cfi_read((__u32)address);
       if (data1 == (__u16)data)
           return WRITE_STATUS_OK;

       while ( 0 ==(data1 & AMD_D5_MASK8x2)) {
       udelay(20);
       data1 = (__u16) cfi_read((__u32)address);
       if (data1 == (__u16)data)
	   return WRITE_STATUS_OK;
       }
       udelay(20);
       data1 = (__u16) cfi_read((__u32)address);
       if (data1 == (__u16)data)
	        return WRITE_STATUS_OK;
       return WRITE_STATUS_TIMEOUT;
   }
   else if (CFI_INTERLEAVE == 4 || (CFI_INTERLEAVE == 2 && cfi_buswidth_is_4())) {  
       __u32 data1;
       data1 = (__u32) cfi_read((__u32)address);
      
       if (data1 == data)
           return WRITE_STATUS_OK;
       
       if ( CFI_INTERLEAVE == 4 ) {
         while ( 0 ==(data1 & AMD_D5_MASK8x4)) {
         udelay(20);
         data1 = (__u32) cfi_read((__u32)address);
         if (data1 == data)
             return WRITE_STATUS_OK;
         }
       }
       else {
	 while ( AMD_D5_MASK16x2 != (data1 & AMD_D5_MASK16x2)) {
         udelay(20);
         data1 = (__u32)cfi_read((__u32)address);
         if (data1 == data)
         return WRITE_STATUS_OK;
         }
       }
       udelay(20);
       data1 = (__u32) cfi_read((__u32)address);
       if (data1 == data)
           return WRITE_STATUS_OK;
       return WRITE_STATUS_TIMEOUT;
   }
}


#if 0
/* for some reason this function does not work
 * at all
 */
static int amd_get_status8(__u8 *address, u8 data)
{
    data = data;
    
    if ( CFI_INTERLEAVE == 1 ) 
    {
       volatile unsigned char  data1, data2, toggle;
       data = data;

      /* Read DQ0-DQ7 */
      data1 = (volatile unsigned char) cfi_read((__u32)address);
      data2 = (volatile unsigned char) cfi_read((__u32)address);

      while ( 0 == ( data2 & AMD_D5_MASK ))
      {
         toggle = data1 ^ data2;
         if ( toggle == 0 )
            return WRITE_STATUS_OK;
         data1 = (volatile unsigned char) cfi_read((__u32)address);
         data2 = (volatile unsigned char) cfi_read((__u32)address);
      }
      /*time out, check again */
      data1 = (volatile unsigned char) cfi_read((__u32)address);
      data2 = (volatile unsigned char) cfi_read((__u32)address);
      toggle = data1 ^ data2;
      if ( toggle == 0 )
         return WRITE_STATUS_OK;
      else
          /*program/Erase operation not complete, return timeout*/
         return WRITE_STATUS_TIMEOUT;
    }
    else if ( CFI_INTERLEAVE == 2 && cfi_buswidth_is_2())
    {
       volatile unsigned short data1, data2, toggle;

       /* Read DQ0-DQ7 */
       data1 = (volatile unsigned short) cfi_read((__u32)address);
       data2 = (volatile unsigned short) cfi_read((__u32)address);

       while ( 0 == ( data2 & AMD_D5_MASK ))
       {
           toggle = data1 ^ data2;
           if ( toggle == 0 )
                  return WRITE_STATUS_OK;
           data1 = (volatile unsigned short) cfi_read((__u32)address);
           data2 = (volatile unsigned short) cfi_read((__u32)address);
      }
      /*time out, check again */
      data1 = (volatile unsigned short) cfi_read((__u32)address);
      data2 = (volatile unsigned short) cfi_read((__u32)address);
      toggle = data1 ^ data2;
      if ( toggle == 0 )
         return WRITE_STATUS_OK;
      else
          /*program/Erase operation not complete, return timeout*/
         return WRITE_STATUS_TIMEOUT;
    }
    else if ( CFI_INTERLEAVE == 4 || (CFI_INTERLEAVE == 2 && cfi_buswidth_is_4()))
    {
       volatile unsigned int data1, data2, toggle;

       /* Read DQ0-DQ7 */
       data1 = (volatile unsigned int) cfi_read((__u32)address);
       data2 = (volatile unsigned int) cfi_read((__u32)address);
       while ( 0 == ( data2 & AMD_D5_MASK ))
       {
          toggle = data1 ^ data2;
          if ( toggle == 0 )
               return WRITE_STATUS_OK;
          data1 = (volatile unsigned int)cfi_read((__u32)address);
          data2 = ( volatile unsigned int)cfi_read((__u32)address);
       }
       /*time out, check again */
       data1 = (volatile unsigned int) cfi_read((__u32)address);
       data2 = (volatile unsigned int) cfi_read((__u32)address);
       toggle = data1 ^ data2;
       if ( toggle == 0 )
           return WRITE_STATUS_OK;
       else
       {
          /*program/Erase operation not complete, return timeout*/
          //printk("data read 0x%0x, should be 0x%0x at 0x%0x\n", data1, data, (__u32)address);
          return WRITE_STATUS_TIMEOUT;  
       } 
    }  
}
#endif

static int amd_get_status(__u32 * address, __u32 data)
{
   return amd_get_status8(address, data);
}

static int probe_new_chip(char * basePtr)
{
        __u32 mfr_id;
	__u32 dev_id;

	amd_send_cmd(basePtr, AMD_CMD_RESET_DATA);
	amd_send_cmd(basePtr, AMD_CMD_MANUFACTURER_UNLOCK_DATA);

	mfr_id = cfi_read((__u32) basePtr + 
			  cfi_build_cmd_addr(AMD_ADDR_MANUFACTURER, CFI_INTERLEAVE, CFI_DEVICETYPE));
	dev_id = cfi_read((__u32) basePtr + 
			  cfi_build_cmd_addr(AMD_ADDR_DEVICE_ID, CFI_INTERLEAVE, CFI_DEVICETYPE));
	cfi_send_gen_cmd(AMD_CMD_RESET_DATA, 0, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
	return 0;
}

static inline int qry_present(__u32 base, int interleave, int device_type)
{
    int ofs = interleave * device_type;
    printk("Check flash type.\n");    
    if (cfi_read(base+ofs*0x10)==cfi_build_cmd('Q') &&
	cfi_read(base+ofs*0x11)==cfi_build_cmd('R') &&
	cfi_read(base+ofs*0x12)==cfi_build_cmd('Y'))
      return 1;	
    return 0; 
}

static __u32 ksflash_cfi_doIdentify(char * basePtr, struct CFIGeometry * cfiGeom)
{
    int i;
    __u32 flashSize=0;
    int ofs = CFI_INTERLEAVE * CFI_DEVICETYPE;	

    probe_new_chip(basePtr);
    cfi_send_gen_cmd(CFI_READ_ARRAY_CMD, 0, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
    cfi_send_gen_cmd(CFI_QUERY_CMD, CFI_QUERY_OFFSET, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);

    if (qry_present((__u32) basePtr, CFI_INTERLEAVE, CFI_DEVICETYPE))
    {
      if (cfiGeom != (struct CFIGeometry *) NULL) 
      {
	memset((void *) cfiGeom, 0, sizeof(struct CFIGeometry));	
	for (i=0; i<sizeof(struct CFIGeometry); i++) 
	  ((unsigned char *)cfiGeom)[i] =  cfi_read_query((__u32) basePtr + (CFI_SYS_SIZE_OFF + i)*ofs);

        printk("Flash Size = 0x%x, first block size = 0x%x, second block size = 0x%0x \n",
	       (1<<cfiGeom->DevSize)*CFI_INTERLEAVE, ((cfiGeom->EraseRegionInfo >>16)*256)*CFI_INTERLEAVE,
	       ((cfiGeom->EraseRegionInfo2 >>16)*256)*CFI_INTERLEAVE);

       flashSize = (1 << cfiGeom->DevSize) * CFI_INTERLEAVE;
      }
    }
    if ( cfiGeom->NumEraseRegions ==  1 )
       cfiGeom->EraseRegionInfo2 = cfiGeom->EraseRegionInfo;
    /* Put it back into Read Mode */
    cfi_send_gen_cmd(CFI_READ_ARRAY_CMD, 0, (__u32) basePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
    return flashSize;
}

static int ksflash_cfi_identify(void)
{
    char * baseptr;
    __u32 flashSize=0, chipSize;

    baseptr = (char *) FlashBasePtr;
    chipSize = ksflash_cfi_doIdentify(baseptr, &(thisFlashChip.cfiGeom));
    flashSize = chipSize;
    thisFlashChip.totalBytes = flashSize;
    Flash_Device_Size = flashSize;
    return flashSize;
}

static int __init ksflash_cfi_init(void *base, u_int size)
{
	int ret=0;
	if (ksflash_cfi_identify() <= 0)
	  return -ENXIO;
	return ret;
}

static int __init flash_init(void)
{
	int err = -EBUSY;

	/* double the size to avoid page fault, and search for real size */
	if (request_mem_region(KS8695_FLASH_START, KS8695_FLASH_SIZE, "flash") == NULL) 
		goto out;

 	/*  no different with ioremap which is non cache*/
	FlashBasePtr = ioremap_nocache(KS8695_FLASH_START, KS8695_FLASH_SIZE);
	err = -ENOMEM;
	if (FlashBasePtr == NULL)
	  goto release;

	err = ksflash_cfi_init(FlashBasePtr, KS8695_FLASH_SIZE);
	// unmap, 
	// remap at each open to allow 1 instance of flash access at any time
	iounmap(FlashBasePtr);
	FlashBasePtr = NULL;

	if (err) {
release:
	  release_mem_region(KS8695_FLASH_START, KS8695_FLASH_SIZE);
	} else
	thisFlashChip.state = FL_READY;
	  
out:
	return err;
}

static void __exit flash_exit(void)
{
   release_mem_region(KS8695_FLASH_START, thisFlashChip.totalBytes);
}


static int ksflash_cfi_erase_block(__u32 *address)
{
    unsigned int rel, offset;

    if (thisFlashChip.state != FL_READY)
      return -EPERM;

    rel = io_read(KS8695_BUF_REG);
    io_write(0x300000, KS8695_BUF_REG);

    spin_lock_bh(&thisFlashChip.mutex);
    offset = 0x555 * CFI_DEVICETYPE * CFI_INTERLEAVE;
    amd_send_cmd_to_addr((char *) FlashBasePtr, AMD_CMD_SECTOR_ERASE_UNLOCK_DATA, ((char *)address)+offset);
    amd_send_cmd_to_addr((char *) FlashBasePtr, AMD_CMD_SECTOR_ERASE_UNLOCK_DATA_2, (char *) address);
    thisFlashChip.state = FL_ERASING;
    //cfi_udelay(1000000);
    spin_unlock_bh(&thisFlashChip.mutex);

    spin_lock_bh(&thisFlashChip.mutex);
    if ( amd_get_status(address, AMD_EMPTY_DATA) == WRITE_STATUS_TIMEOUT )
    {
	thisFlashChip.state = FL_STATUS;
	spin_unlock_bh(&thisFlashChip.mutex);
	printk(KERN_ERR "waiting for chip to be ready timed out in erase\n");
	return -EIO;
    }
    /* Put it back into Read Mode */
    cfi_send_gen_cmd(CFI_READ_ARRAY_CMD, 0, (__u32) address, CFI_DEVICETYPE, CFI_INTERLEAVE);
    thisFlashChip.state = FL_READY;
    spin_unlock_bh(&thisFlashChip.mutex);
    io_write(rel, KS8695_BUF_REG);
    return 0;
}

static int ksflash_cfi_write_word(__u32 *address, __u32 data)
{
   if (thisFlashChip.state != FL_READY)
       return -EIO;
   spin_lock_bh(&thisFlashChip.mutex);
   amd_send_unlock((char *) FlashBasePtr);
   cfi_send_gen_cmd(AMD_CMD_PROGRAM_UNLOCK_DATA, 0x555, (__u32) FlashBasePtr, CFI_DEVICETYPE, CFI_INTERLEAVE);
   cfi_write(data, (__u32) address);
   thisFlashChip.state = FL_WRITING;

   if (amd_get_status(address, data) == WRITE_STATUS_TIMEOUT)
   {
      thisFlashChip.state = FL_STATUS;
      spin_unlock_bh(&thisFlashChip.mutex);
      printk(KERN_ERR "waiting for chip to be ready timed out in word write at %x\n",(__u32) address);
      return -EIO;
   } // write is done
   cfi_send_gen_cmd(CFI_READ_ARRAY_CMD, 0, (__u32) address, CFI_DEVICETYPE, CFI_INTERLEAVE);
   thisFlashChip.state = FL_READY;
   spin_unlock_bh(&thisFlashChip.mutex);
   return 0;
}

static int kflash_write_word(__u32 adr, char * data, int count)
{
        int len=count, status;
	int rtn = 0;
	char *tdata = data;
	__u32 address = adr;
	__u32 datum;

	//udelay(500);
	//amd_send_cmd((char *) FlashBasePtr, AMD_CMD_UNLOCK_BYPASS_MODE);
	
	if (cfi_buswidth_is_1()) 
	{
	   while ( len ) 
	   {
              datum = *(__u8 *)tdata;		   
	      if ((status = ksflash_cfi_write_word((__u32 *) address, datum)) != 0)
	      {
	        rtn =  status;
		break;
	      }
	      len--;
	      tdata++;
	      address++;
	   }
	} 
	else if (cfi_buswidth_is_2())
       	{
	  while ( len ) 
	  {
  	    datum = *(__u16*)tdata;
	    if ((status = ksflash_cfi_write_word((__u32 *) address, datum)) != 0) 
	    {
	      rtn =  status;
	      break;
	   }
	   len -= 2;
	   tdata += 2;
	   address += 2;
	  }
	} 
	else if (cfi_buswidth_is_4())
       	{
	   while ( len ) 
	   {
  	     datum = *(__u32*)tdata;
	     if ((status = ksflash_cfi_write_word((__u32 *) address, datum)) != 0) 
	     {
	        rtn = status;
		break;
	     }
	     len -= 4;
	     tdata += 4;
	     address +=4;
	   }
	}
	// reset bypass
	//amd_reset_bypass((char *) FlashBasePtr);
	/* Put it back into Read Mode */
	cfi_send_gen_cmd(CFI_READ_ARRAY_CMD, 0, (__u32)adr, CFI_DEVICETYPE, CFI_INTERLEAVE);
	return (rtn);
}

static int kflash_read_word(__u32 adr, char * buffer, int size)
{
    int len=size;
    __u32 address=adr;
    char * buf = buffer;

    if (cfi_buswidth_is_1())
    {
      while (len > 0 )
      { 
	*(__u8*) buf = (__u8) cfi_read(address);
	len--;
	buf++;
	address++;
      }
    }
    else if (cfi_buswidth_is_2())
    {
       while (len > 0) 
       { 
	*(__u16*) buf = (__u16) cfi_read(address);
	len -= 2;
	buf += 2;
	address += 2;
       }
    }
    else if (cfi_buswidth_is_4())
    {
       while ( len > 0 )
       { 
	 *(__u32*) buf = cfi_read(address);	
	 len -= 4;
	 buf += 4;
	 address += 4;
       }
    } 
    else 
      return -EINVAL;

    return size;
}

/************************************************************************************/

static int ksflash_open(struct inode *inode, struct file *file)
{
        // allow 1 instance to access Flash at any time
	if (FlashBasePtr != NULL)
	        return EMFILE;
	FlashBasePtr = ioremap_nocache(KS8695_FLASH_START, KS8695_FLASH_SIZE);
        if (FlashBasePtr == NULL) 
                return ENOSPC;
        MOD_INC_USE_COUNT;
        return 0;
}

static int ksflash_release(struct inode *inode, struct file *file)
{
        if (FlashBasePtr != NULL)
                iounmap(FlashBasePtr);
        FlashBasePtr = NULL;
        MOD_DEC_USE_COUNT;
        return 0;
}

#if 0
static ssize_t ksflash_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
    
    int written=0, i;
    __u32 address, rel;
    int status;
    //char *tmpData;
    tImageInfo * pImage;
    tFooter * pfooter;
    int image_size=0;

    rel = io_read(KS8695_BUF_REG);
    io_write(0x300000, KS8695_BUF_REG);
    // set up permission
    if (*ppos == 0)
      return -EPERM;
    if (*ppos+len > thisFlashChip.totalBytes)
    {
      printk("EACCES: %d @ %u bytes\n",(__u32) (*ppos+len),(__u32) thisFlashChip.totalBytes);
      return -EFBIG;
    }
    if (*ppos < CFI_BLOCK_SIZE)
      return -ENXIO;
    // must starts at CFI_BLOCK_SIZE multples
    if ((*ppos & (CFI_BLOCK_SIZE - 1)) != 0) 
      return -ENXIO;
    // must be CFI_BLOCK_SIZE
    //if (len < CFI_BLOCK_SIZE)
    //  return -EPERM;

    // retreive info, caller must set up footer info
    // work only with 64k - footer size - info size

    pfooter = (tFooter *) (data+FOOTER_OFFSET);
    if (pfooter->signature == FOOTER_SIGNATURE)
    {
      i = (int) (pfooter->infoBase);
      i &= 0xFFFF;
      pImage = (tImageInfo *) (data + i);
      image_size = i + sizeof(tImageInfo);
    }
    if (image_size == 0)
    {
      printk("write 0!\n");
      return -EPERM;
    }
    // retreive the first & last block

    // erase the blocks
    address = (__u32) FlashBasePtr;
    address += *ppos;
 
    if ((status = ksflash_cfi_erase_block((__u32 *) address)) != 0) 
    {
      printk("erase error %d\n",status);
      return status;
    }
    if ((status = kflash_write_word(address, (char *) data, image_size)) != 0)
    {
      printk("fail to wirte configuration file to flash\n");
      return status;
    }

    address += FOOTER_OFFSET;
    if ((status = kflash_write_word(address, (char *) (data+FOOTER_OFFSET), sizeof(tFooter))) != 0)
    {
      printk("fail to wirte configuration file to flash\n");	    
      return status;
    }

    written = image_size;
    *ppos += written;
    io_write(rel, KS8695_BUF_REG);
    return written;
}
#endif

static ssize_t ksflash_writedb(char *data)
{
    
    int written=0, i;
    __u32 address, rel;
    int status;

    tImageInfo * pImage;
    tFooter * pfooter;
    int image_size=0;

    rel = io_read(KS8695_BUF_REG);
    io_write(0x300000, KS8695_BUF_REG);

    pfooter = (tFooter *) (data+FOOTER_OFFSET);
    if (pfooter->signature == FOOTER_SIGNATURE)
    {
      i = (int) (pfooter->infoBase);
      i &= 0xFFFF;
      pImage = (tImageInfo *) (data + i);
      image_size = i + sizeof(tImageInfo);
    }
    if (image_size == 0)
    {
      printk("write 0!\n");
      return -EPERM;
    }
    // retreive the first & last block

    // erase the blocks
    address = (__u32) FlashBasePtr;
    address += thisFlashChip.totalBytes - CFI_BLOCK_SIZE;
 
    if ((status = ksflash_cfi_erase_block((__u32 *) address)) != 0) 
    {
      printk("erase error %d\n",status);
      return status;
    }
    if ((status = kflash_write_word(address, (char *) data, image_size)) != 0)
    {
      printk("fail to wirte configuration file to flash\n");
      return status;
    }

    address += FOOTER_OFFSET;
    if ((status = kflash_write_word(address, (char *) (data+FOOTER_OFFSET), sizeof(tFooter))) != 0)
    {
      printk("fail to wirte configuration file to flash\n");	    
      return status;
    }

    written = image_size;
    io_write(rel, KS8695_BUF_REG);
    return written;
}

static ssize_t ksflash_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
  __u32 address, rel, written;
  int status;
  
  rel = io_read(KS8695_BUF_REG);
  io_write(0x300000, KS8695_BUF_REG);
  
  if ( file->f_pos + len >= thisFlashChip.totalBytes )
  {
      printk("EACCES: %d @ %u bytes\n",(__u32) (file->f_pos + len),(__u32) thisFlashChip.totalBytes);
      return -EFBIG;
  }
  
  address = (__u32) FlashBasePtr;
  address += file->f_pos;
  if ((status = kflash_write_word(address, (char *) data, len)) != 0)
  {
     printk("fail to wirte to flash\n");
             return status;
  }
  io_write(rel, KS8695_BUF_REG);
  file->f_pos += len;
  written = len;
  return (written);
}
  	  
                                                                                                          
static ssize_t ksflash_read(struct file *file, char *data, size_t len, loff_t *ppos)
{
    int tmpLen, readd;
    __u32 address;
    char * buf;

    if (thisFlashChip.state != FL_READY)
      return -EPERM;

    address = (__u32) FlashBasePtr;
    address += *ppos;
    tmpLen = len;
    buf = data;
    readd = 0;
    while (tmpLen >= CFI_BLOCK_SIZE)
    {
      kflash_read_word(address, buf, CFI_BLOCK_SIZE);
      address += CFI_BLOCK_SIZE;
      tmpLen -= CFI_BLOCK_SIZE;
      buf += CFI_BLOCK_SIZE;
      readd += CFI_BLOCK_SIZE;
    }

    // read remaining data
    if (tmpLen)
    {
      kflash_read_word(address, buf, tmpLen);
      readd += tmpLen;
    }

    *ppos += readd;
    return readd;
}

static int ksflash_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
    __u32 address;
    int status;

    switch (cmd)
    {
      case KSFLASH_IOC_ERASE:
 	   address = (__u32) FlashBasePtr;
	   address += (__u32) arg;
	   if ((status = ksflash_cfi_erase_block((__u32 *) address)) != 0)
           {
	      printk("erase error %d\n",status);
	      return status;
	   }
	   break;
      case KSFLASH_IOC_GETSIZE:
           if ( Flash_Device_Size == 0 )
                ksflash_cfi_identify();
           return Flash_Device_Size;
      case KSFLASH_IOC_WRITEDB:
           if ( Flash_Device_Size == 0 )
	        ksflash_cfi_identify();
	   address = (__u32) arg;
	   status = ksflash_writedb((char *)address);
	   return status;
      default:
           printk("unknown command.\n");
           return -EINVAL;
    }
    return 0;
}

static loff_t ksflash_lseek(struct file * file, loff_t offset, int orig)
{
	switch (orig) {
		case 0:
			file->f_pos = offset;
			return file->f_pos;
		case 1:
			file->f_pos += offset;
			return file->f_pos;
		default:
			return -EINVAL;
	}
}

static struct file_operations ksflash_fops = {
	owner:		THIS_MODULE,
	llseek:		ksflash_lseek,
	read:		ksflash_read,
	write:		ksflash_write,
	ioctl:		ksflash_ioctl,
	open:		ksflash_open,
	release:	ksflash_release,
};

static char banner[] __initdata = KERN_INFO "Micrel KS8695 Flash Driver v%s\n";

static int __init ksflash_init(void)
{
        int rtn;
	BlockBuffer = kmalloc(CFI_BLOCK_SIZE, GFP_KERNEL);
	if (BlockBuffer == NULL) {
		printk("not enough memory!\n");
	        return -1;
	}

	thisFlashChip.state = FL_UNKNOWN;
	thisFlashChip.mutex = SPIN_LOCK_UNLOCKED;

	rtn = register_chrdev(KSFLASH_MAJOR, "ksflash", 
					 (struct file_operations *) &ksflash_fops);

	if ( rtn < 0 ) 
	{ 
	        printk("Can't register flash device driver!");
	        return rtn;
	}
	
        printk(banner, VERSION);
	return flash_init();
}

static void __exit ksflash_exit(void)
{
        unregister_chrdev(KSFLASH_MAJOR, "ksflash");
        flash_exit();
}

module_init(ksflash_init);
module_exit(ksflash_exit);
