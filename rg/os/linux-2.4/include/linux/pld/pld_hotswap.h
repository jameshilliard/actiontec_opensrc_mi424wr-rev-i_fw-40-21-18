#ifndef __LINUX_PLD_HOTSWAP_H
#define __LINUX_PLD_HOTSWAP_H
/*
 *  linux/drivers/char/pld/pld_hotswap.h
 *
 *  Pld driver for Altera EPXA Excalibur devices
 *
 *  Copyright 2001 Altera Corporation
 *
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
 *
 *  $Id: pld_hotswap.h,v 1.1.1.1 2007/05/07 23:29:55 jungo Exp $
 *
 */

/*
 * The pld_hotswap ops contain the basic operation required for adding
 * and removing devices from the system.
 */

struct pldhs_dev_info{
       unsigned int size;
       unsigned short vendor_id;
       unsigned short product_id;
       unsigned char fsize;
       unsigned char nsize;
       unsigned short pssize;
       unsigned short cmdsize;
       unsigned char irq;
       unsigned char version;
       unsigned int base_addr;
       unsigned int reg_size;
};

struct pldhs_dev_desc{
       struct pldhs_dev_info* info;
       char* name;
       void* data;
};

#include <linux/pld/pld_epxa.h>


#ifdef __KERNEL__
struct pld_hotswap_ops{
       struct list_head list;
       char* name;
       int (*add_device)(struct pldhs_dev_info *dev_info, void* dev_ps_data);
       int (*remove_devices)(void);
       int (*proc_read)(char* buf,char** start,off_t offset,int count,int *eof,void *data);
};

/*
 * These functions are called by the device drivers to register functions
 * which should be called when devices are added and removed
 */
extern int pldhs_register_driver(struct pld_hotswap_ops *drv);
extern int pldhs_unregister_driver(char *driver);

/*
 * These functions are called by the pld loader to add and remove
 * device instances from the system. The call the functions regsistered
 * the the particular deriver for this purpose
 */
extern int pldhs_add_device(struct pldhs_dev_info* dev_info,char *drv_name, void* dev_ps_data);
extern int pldhs_remove_devices(void);

/* Macro for formatting data to appear in the proc/pld file */
#define PLDHS_READ_PROC_DATA(buf,name,index,base,irq,ps_string) \
                    sprintf(buf,":%s:%d Base Address, %#lx, IRQ, %d#\n%s\n",\
                    name,index,base,irq,ps_string)

#endif

#endif
