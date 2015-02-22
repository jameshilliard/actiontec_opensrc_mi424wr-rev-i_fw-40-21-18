/*
 *  linux/drivers/pld/pld_hotswap.c
 *
 *  Pld driver for Altera EPXA Excalibur devices
 *
 *
 *  Copyright 2001 Altera Corporation (cdavies@altera.com)
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
 *  $Id: pld_hotswap.c,v 1.1.1.1 2007/05/07 23:29:28 jungo Exp $
 *
 */

/*
 * pld_hotswap ops contains the basic operation required for adding
 * and removing devices from the system.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/pld/pld_hotswap.h>


static struct pld_hotswap_ops pldhs_driver_list={
       list: LIST_HEAD_INIT(pldhs_driver_list.list),
       name: "",
};

static spinlock_t list_lock=SPIN_LOCK_UNLOCKED;



static struct pld_hotswap_ops *  pldhs_find_driver(char* name)
{
       struct pld_hotswap_ops * ptr;
       struct list_head *list_pos;

       spin_lock(&list_lock);
       list_for_each(list_pos,&pldhs_driver_list.list){
               ptr=(struct pld_hotswap_ops *)list_pos;
               if(!strcmp(name, ptr->name)){
                       spin_unlock(&list_lock);
                       return ptr;

               }
       }
       spin_unlock(&list_lock);
       return 0;
}



int pldhs_register_driver(struct pld_hotswap_ops *drv)
{

       /* Check that the device is not already on the list
        * if so, do nothing */
       if(pldhs_find_driver(drv->name)){
               return 0;
       }

       printk(KERN_INFO "PLD: Registering hotswap driver %s\n",drv->name);
       /* Add this at the start of the list */
       spin_lock(&list_lock);
       list_add((struct list_head*)drv,&pldhs_driver_list.list);
       spin_unlock(&list_lock);

       return 0;
}

int pldhs_unregister_driver(char *name)
{
       struct pld_hotswap_ops *ptr;

       ptr=pldhs_find_driver(name);
       if(!ptr){
               return -ENODEV;
       }

       printk(KERN_INFO "PLD: Unregistering hotswap driver%s\n",name);
       spin_lock(&list_lock);
       list_del((struct list_head*)ptr);
       spin_unlock(&list_lock);

       return 0;
}

int pldhs_add_device(struct pldhs_dev_info* dev_info,char *drv_name, void* dev_ps_data)
{
       struct pld_hotswap_ops * ptr;

       ptr=pldhs_find_driver(drv_name);

       if(!ptr){
               /* try requesting this module*/
               request_module(drv_name);
               /* is the driver there now? */
               ptr=pldhs_find_driver(drv_name);
               if(!ptr){
                       printk("pld hotswap: Failed to load a driver for %s\n",drv_name);
                       return -ENODEV;
               }
       }

       if(!ptr->add_device){
               printk(KERN_WARNING "pldhs: no add_device() function for driver %s\n",drv_name);
               return 0;
       }

       return ptr->add_device(dev_info,dev_ps_data);
}

int pldhs_remove_devices(void)
{
       struct list_head *list_pos;
       struct pld_hotswap_ops * ptr;


       spin_lock(&list_lock);
       list_for_each(list_pos,&pldhs_driver_list.list){
               ptr=(struct pld_hotswap_ops *)list_pos;
               if(ptr->remove_devices)
                       ptr->remove_devices();

       }
       spin_unlock(&list_lock);

       return 0;
}

#ifdef CONFIG_PROC_FS
int pldhs_read_proc(char* buf,char** start,off_t offset,int count,int *eof,void *data){


       struct list_head *list_pos;
       struct pld_hotswap_ops * ptr;
       int i,len=0;

       *start=buf;
       spin_lock(&list_lock);
       list_for_each(list_pos,&pldhs_driver_list.list){
               ptr=(struct pld_hotswap_ops *)list_pos;
               if(ptr->proc_read){
                       i=ptr->proc_read(buf,start,offset,count,eof,data);
                       count-=i;
                       len+=i;
                       *start+=i;
               }
       }
       spin_unlock(&list_lock);

       *start=NULL;
       *eof=1;
       return len;
}

void __init pldhs_init(void){
       create_proc_read_entry("pld",0,NULL,pldhs_read_proc,NULL);
}

__initcall(pldhs_init);
#endif

EXPORT_SYMBOL(pldhs_register_driver);
EXPORT_SYMBOL(pldhs_unregister_driver);
