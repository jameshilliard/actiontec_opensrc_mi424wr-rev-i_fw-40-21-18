
/*
 *  drivers/char/epxapld.c
 *
 *  Copyright (C) 2001 Altera Corporation
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
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/arch/excalibur.h>
#include <asm/arch/hardware.h>
#define PLD_CONF00_TYPE (volatile unsigned int *)
#define MODE_CTRL00_TYPE (volatile unsigned int *)
//#define DEBUG(x) x
#define DEBUG(x)

#include <asm/arch/mode_ctrl00.h>
#include <asm/arch/pld_conf00.h>
#ifdef CONFIG_PLD_HOTSWAP
#include <linux/pld/pld_hotswap.h>
#endif
#include <linux/pld/pld_epxa.h>

/*
 *	Macros
 */


#define PLD_BASE (IO_ADDRESS(EXC_PLD_CONFIG00_BASE))
#define CLOCK_DIV_RATIO ((1 + EXC_AHB2_CLK_FREQUENCY/32000000) & CONFIG_CONTROL_CLOCK_RATIO_MSK)
/*
 *	STRUCTURES
 */


struct  pld_sbihdr{
	unsigned int fpos;
	unsigned int temp;
};

static DECLARE_MUTEX(pld_sem);


static void lock_pld (void)
{
	/* Lock the pld i/f  */
	unsigned int tmp;

	tmp = readl(CONFIG_CONTROL(PLD_BASE));
	tmp |= CONFIG_CONTROL_LK_MSK;

	writel(tmp,CONFIG_CONTROL(PLD_BASE));
}

static void unlock_excalibur_pld (void)
{
	/* Unlock the pld i/f */

	if (readl(CONFIG_CONTROL(PLD_BASE)) & CONFIG_CONTROL_LK_MSK ){
		writel(CONFIG_UNLOCK_MAGIC, CONFIG_UNLOCK(PLD_BASE));
		while (readl(CONFIG_CONTROL(PLD_BASE)) & CONFIG_CONTROL_LK_MSK);
        }
}


static int place_pld_into_configure_mode (void)
{
	unsigned int tmp;


	if( readl(CONFIG_CONTROL(PLD_BASE)) & CONFIG_CONTROL_CO_MSK ){
		/*
		 *	Already being configured!!!
		 */
		printk(KERN_WARNING "pld0: Device already being configured!\n");
		return -EBUSY;
	}

	/* Set up the config clock */

	writel(CLOCK_DIV_RATIO,CONFIG_CONTROL_CLOCK(PLD_BASE));
	while(readl(CONFIG_CONTROL_CLOCK(PLD_BASE))
	      !=CLOCK_DIV_RATIO);
	/* Tell the device we wish to configure it */
	tmp = readl(CONFIG_CONTROL(PLD_BASE));
	tmp |= CONFIG_CONTROL_CO_MSK;
	writel(tmp,CONFIG_CONTROL(PLD_BASE));


	/*
	 *	Wait for the busy bit to clear then check for errors.
	 */

	while((tmp=readl(CONFIG_CONTROL(PLD_BASE))&CONFIG_CONTROL_B_MSK ));

	if ( tmp & CONFIG_CONTROL_E_MSK ){
		if ( tmp & CONFIG_CONTROL_ES_0_MSK ){
		        /* Already being programmed via JTAG */
			printk(KERN_WARNING "pld0:JTAG configuration alreay in progress\n");
			return -EBUSY;

		}
		if ( tmp & CONFIG_CONTROL_ES_1_MSK ){
			/* No config clock configured */
			printk(KERN_ERR "pld0:No config clock!\n");
			BUG();
			return -EBUSY;
		}
		if ( tmp & CONFIG_CONTROL_ES_2_MSK ){
			/* Already being programmed via External device */
			printk(KERN_WARNING "pld0:JTAG configuration alreay in progress\n");
			return -EBUSY;
		}
	}

	return 0;
}


static int write_pld_data_word(unsigned int config_data)
{
	unsigned int tmp;

	do{
		tmp = readl(CONFIG_CONTROL(PLD_BASE));
	}
        while ( ( tmp & CONFIG_CONTROL_B_MSK ) &&
		!( tmp & CONFIG_CONTROL_E_MSK ));

        if ( tmp & CONFIG_CONTROL_E_MSK ){
		printk("pld0: Error writing pld data, CONFIG_CONTROL=%#x\n",tmp);

		return -EILSEQ;
	}

        writel(config_data,CONFIG_CONTROL_DATA(PLD_BASE));
	return 0;

}


static int wait_for_device_to_configure (void)
{
	int i=0x10000;

	while(readl(CONFIG_CONTROL(PLD_BASE)) & CONFIG_CONTROL_B_MSK);

	/*
	 * Wait for the config bit (CO) to go low, indicating that everything
	 * is Ok. If it doesn't, assume that is screwed up somehow and
	 * clear the CO bit to abort the configuration.
	 */

	while(readl(CONFIG_CONTROL(PLD_BASE)) & CONFIG_CONTROL_B_MSK);

	while (i&&(readl(CONFIG_CONTROL(PLD_BASE)) & CONFIG_CONTROL_CO_MSK)){
		i--;
	}

	if (!i){
		writel(0,CONFIG_CONTROL(PLD_BASE));
		printk(KERN_WARNING "pld0: Invalid PLD config data\n");
		return -EILSEQ;
	}

	return 0;
}



static int pld_open(struct inode* inode, struct file *filep)
{

	struct pld_sbihdr* sbihdr;

	/* Check the device minor number */
	if(minor(inode->i_rdev)){
		DEBUG(printk("pld0: minor=%d",minor(inode->i_rdev));)
			return -ENODEV;
	}

	/* Create our private data and link it to the file structure */
	sbihdr=kmalloc(sizeof(struct pld_sbihdr),GFP_KERNEL);

	if(!sbihdr)
		return -ENOMEM;

	filep->private_data=sbihdr;

	sbihdr->fpos=0;
	sbihdr->temp=0;
	return 0;
}

static int pld_release(struct inode* inode, struct file *filep){
	int ret_code;

	kfree(filep->private_data);
      	ret_code=wait_for_device_to_configure();
	lock_pld();
	return ret_code;
}

static ssize_t pld_write(struct file* filep, const char* data, size_t count, loff_t* ppos){

	struct pld_sbihdr* sbihdr=filep->private_data;
	int bytes_left=count;
	int result;
	DEBUG(int i=0);


	/* Can't seek (pwrite) on pld.  */
	if (ppos != &filep->f_pos)
		return -ESPIPE;


	/* Check access to the whole are in one go */
	if(!access_ok(VERIFY_READ,(const void*)data, count)){
		return -EFAULT;
	}

	/*
	 * We now lock against writes.
	 */
	if (down_interruptible(&pld_sem))
		return -ERESTARTSYS;

	if(!sbihdr->fpos){
		/*
		 * unlock the pld and place in configure mode
		 */
		unlock_excalibur_pld();
		result=place_pld_into_configure_mode();
		if(result)
			return result;
	}
	DEBUG(printk("data= %#x count=%#x 0ffset=%#x\n",*data, count, *ppos));

	while(bytes_left){
		char tmp;
		__get_user(tmp,data);
		/* read our header ! */
		sbihdr->temp|=tmp << (8*(sbihdr->fpos&3));
		if((sbihdr->fpos&3)==3){
			if(write_pld_data_word(sbihdr->temp))
			{
				DEBUG(printk("pos=%d\n",sbihdr->fpos);)
					return -EILSEQ;
			}
			DEBUG(if(i<10){)
			      DEBUG(printk("fpos2 :%#x data=%#x\n",sbihdr->fpos,sbihdr->temp));
			      DEBUG(i++);
			      DEBUG(});
			sbihdr->temp=0;
			DEBUG(words_written++);
		}
		sbihdr->fpos++;
		data++;
		bytes_left--;
	}

	up(&pld_sem);
	return (count);
}

int pld_ioctl(struct inode *inode, struct file *filep, unsigned int cmd, unsigned long arg)
{

	switch(cmd){

#ifdef CONFIG_PLD_HOTSWAP
	case PLD_IOC_ADD_PLD_DEV:
	{
		struct pldhs_dev_desc desc;
		struct pldhs_dev_info info;
		char *name;
		void *data;
		int result=0;

		result=copy_from_user(&desc,(const void*)arg,sizeof(struct pldhs_dev_desc));
		if(result)
			return -EFAULT;
		result=copy_from_user(&info,(const void*)desc.info,sizeof(struct pldhs_dev_info));
		if(result)
			return -EFAULT;
		name=kmalloc(info.nsize,GFP_KERNEL);
		if(!name)
			return -ENOMEM;

		result=copy_from_user(name,(const void*)desc.name,info.nsize);
		if(result){
			result=-EFAULT;
			goto ioctl_out;
		}

		data=kmalloc(info.pssize,GFP_KERNEL);
		if(!data){
			result=-ENOMEM;
			goto ioctl_out;
		}

		result=copy_from_user(data,(const void*)desc.data,info.pssize);
		if(result){
			result=-EFAULT;
			goto ioctl_out1;
		}
		result=pldhs_add_device(&info,name,data);

	ioctl_out1:
		kfree(data);
	ioctl_out:
		kfree(name);
		return result;

	}

	case PLD_IOC_REMOVE_PLD_DEVS:
		pldhs_remove_devices();
		return 0;

	case PLD_IOC_SET_INT_MODE:
		if(cmd==3){
			printk(KERN_INFO "Interrupt mode set to 3 (Six individual interrupts)\n");
			return 0;
		}else{
			printk(KERN_INFO "There is no interrupt handler available for this mode (%d). You will need to add one\n to implement whatever scheme you require\n");
			return -EACCES;
		}
#endif
	default:
		return -ENOTTY;
	}
}


static struct file_operations pld_fops={
	write:      pld_write,
	ioctl:      pld_ioctl,
	open:       pld_open,
	release:    pld_release
};

static int __init pld_init(void){
	int major;
	major=register_chrdev(0,"pld",&pld_fops);
	printk(KERN_INFO "Using PLD major num=%d\n",major);
	if (major<0){
	        return major;
	}
	return 0;
}

__initcall(pld_init);
