/*
 * Copyright (c) 2005 freescale semiconductor
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/config.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/usb.h>


#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>

#include <asm/unaligned.h>

#include "../core/hcd.h"
#include "fsl-usb.h"

#define DRIVER_VERSION "$Revision: 1.1.1.1 $"
#define DRIVER_AUTHOR "Hunter Wu"
#define DRIVER_DESC "USB 2.0 Freescale EHCI Driver"
#define DRIVER_INFO DRIVER_VERSION " " DRIVER_DESC

MODULE_DESCRIPTION("MPC8249 USB Host Controller Driver");

//#include "ehci-hcd.c"

void mpc8349_board_init(void)
{
#ifndef CONFIG_MPC8349_ITX
	volatile unsigned char *bcsr5_p;

	/* if SYS board is plug into PIB board, force to use the PHY on SYS board */
	bcsr5_p = (volatile unsigned char *)(CFG_BCSR_BASE + 0x00000005);
	if ( (*bcsr5_p & BCSR5_INT_USB) == 0 )
		*bcsr5_p = (*bcsr5_p | BCSR5_INT_USB);
#endif
}

void mpc8349_usb_clk_cfg(void)
{
	unsigned long sccr;
	volatile unsigned long *p;

	p = (volatile unsigned long *)(CFG_IMMR_BASE + SCCR_OFFS); /* SCCR */
	sccr = *p;

#ifndef CONFIG_MPC8349_ITX
#if defined(CONFIG_MPH_USB_SUPPORT)
	sccr &= ~SCCR_USB_MPHCM_11;
	sccr |= SCCR_USB_MPHCM_11;  /* USB CLK 1:3 CSB CLK */
	*p = sccr;
#elif defined(CONFIG_DR_USB_SUPPORT)
	sccr &= ~SCCR_USB_DRCM_11;
	sccr |= SCCR_USB_DRCM_11;  /* USB CLK 1:3 CSB CLK */
	*p = sccr;
#endif

#else /* CONFIG_MPC8349_ITX */
	sccr &= ~(0x00F00000);
	sccr |= 0x00F00000;  /* USB CLK 1:3 CSB CLK */
	*p = sccr;
#endif	/* CONFIG_MPC8349_ITX */

}

void mpc8349_usb_pin_cfg(void)
{
	unsigned long sicrl;
	volatile unsigned long *p;

	p = (volatile unsigned long *)(CFG_IMMR_BASE + SICRL_OFFS); /* SCCR */
	sicrl = *p;

#ifndef CONFIG_MPC8349_ITX
#if defined(CONFIG_MPH_USB_SUPPORT)
#ifdef CONFIG_MPH0_USB_ENABLE
	sicrl &= ~SICRL_USB0;
	*p = sicrl;
#endif

#ifdef CONFIG_MPH1_USB_ENABLE
	sicrl &= ~SICRL_USB1;
	*p = sicrl;
#endif
#elif defined(CONFIG_DR_USB_SUPPORT)
	sicrl &= ~SICRL_USB0;
	sicrl |= SICRL_USB1 ;
	*p = sicrl;
#if defined(CONFIG_DR_UTMI)
	sicrl &= ~SICRL_USB0;
	sicrl |= SICRL_USB0;
	*p = sicrl;
#endif

#endif

#else /* CONFIG_MPC8349_ITX */
	sicrl &= ~(0x60000000);
	sicrl |= 0x40000000;
	*p = sicrl;

#endif /* CONFIG_MPC8349_ITX */
}

void mpc8349_usb_reset(void)
{
	u32      portsc;
#ifndef CONFIG_MPC8349_ITX
#if defined(CONFIG_MPH_USB_SUPPORT)
	t_USB_MPH_MAP *p_MphMemMap;
	/* Enable PHY interface in the control reg. */
	p_MphMemMap = (t_USB_MPH_MAP *)MPC83xx_USB_MPH_BASE;
	p_MphMemMap->control = 0x00000004;
	p_MphMemMap->snoop1 = 0x0000001b;
#ifdef CONFIG_MPH0_USB_ENABLE
	portsc = readl(&p_MphMemMap->port_status[0]);
	portsc &= ~PORT_TS;
#if defined(CONFIG_MPH0_ULPI)
	portsc |= PORT_TS_ULPI;
#elif defined (CONFIG_MPH0_SERIAL)
	portsc |= PORT_TS_SERIAL;
#endif
	writel(portsc,&p_MphMemMap->port_status[0]);
#endif

#ifdef CONFIG_MPH1_USB_ENABLE
	portsc = readl(&p_MphMemMap->port_status[1]);
	portsc &= ~PORT_TS;
#if defined(CONFIG_MPH1_ULPI)
	portsc |= PORT_TS_ULPI;
#elif defined (CONFIG_MPH1_SERIAL)
	portsc |= PORT_TS_SERIAL;
#endif
	writel(portsc,&p_MphMemMap->port_status[1]);
#endif

	p_MphMemMap->pri_ctrl = 0x0000000c;
	p_MphMemMap->age_cnt_thresh = 0x00000040;
	p_MphMemMap->si_ctrl= 0x00000001;

#elif defined(CONFIG_DR_USB_SUPPORT)
	t_USB_DR_MAP *p_DrMemMap;
	p_DrMemMap = (t_USB_DR_MAP *)MPC83xx_USB_DR_BASE;
	p_DrMemMap->control = 0x00000004;
	p_DrMemMap->snoop1 = 0x0000001b;
	portsc = readl(&p_DrMemMap->port_status[0]);
	portsc &= ~PORT_TS;
#if defined(CONFIG_DR_ULPI)
	portsc |= PORT_TS_ULPI;
#elif defined(CONFIG_DR_SERIAL)
	portsc |= PORT_TS_SERIAL;
#elif defined(CONFIG_DR_UTMI)
	portsc |= PORT_TS_ULPI;
#endif

	writel(portsc,&p_DrMemMap->port_status[0]);
	writel(0x00000003,&p_DrMemMap->usbmode);
	p_DrMemMap->pri_ctrl = 0x0000000c;
	p_DrMemMap->age_cnt_thresh = 0x00000040;
	p_DrMemMap->si_ctrl= 0x00000001;
#endif

#else /* CONFIG_MPC8349_ITX */

	t_USB_MPH_MAP *p_MphMemMap;
	/* Enable PHY interface in the control reg. */
	p_MphMemMap = (t_USB_MPH_MAP *)MPC83xx_USB_MPH_BASE;
	p_MphMemMap->control = 0x00000004;
	p_MphMemMap->snoop1 = 0x0000001b;

	/* CONFIG_MPH0_USB_ENABLE */
	portsc = readl(&p_MphMemMap->port_status[0]);
	portsc &= ~PORT_TS;

	/* CONFIG_MPH0_ULPI */
	portsc |= PORT_TS_ULPI;
	writel(portsc,&p_MphMemMap->port_status[0]);

	p_MphMemMap->pri_ctrl = 0x0000000c;
	p_MphMemMap->age_cnt_thresh = 0x00000040;
	p_MphMemMap->si_ctrl= 0x00000001;
#endif /* CONFIG_MPC8349_ITX */
}

static int __init
fsl_usb20_probe(struct device *dev)
{
	struct usb_hcd		*hcd;
	int 			retval;
#if defined (CONFIG_MPH_USB_SUPPORT)
	t_USB_MPH_MAP 		*p_MphMemMap;
#elif defined (CONFIG_DR_USB_SUPPORT)
    	t_USB_DR_MAP 		*p_DrMemMap;
#endif
	extern const struct hc_driver ehci_driver;
	static u64 dma_mask = ~0;

	dev->dma_mask = &dma_mask;
	
	/* XXX request_mem_region(), ioremap_nocache() XXX */

	mpc8349_board_init();
	mpc8349_usb_clk_cfg();
	mpc8349_usb_pin_cfg();

	hcd = usb_create_hcd(&ehci_driver, dev, dev->bus_id);
	if (!hcd) {
		retval = -ENOMEM;
		goto err1;
	}
	hcd->product_desc = "fsl usb20";

#if defined(CONFIG_MPH_USB_SUPPORT)
	p_MphMemMap = (t_USB_MPH_MAP *)MPC83xx_USB_MPH_BASE;
        hcd->regs = (void *)(&p_MphMemMap->hc_capbase);
        hcd->irq = MPC83xx_USB_MPH_IVEC;
#elif defined (CONFIG_DR_USB_SUPPORT)
        p_DrMemMap = (t_USB_DR_MAP *)MPC83xx_USB_DR_BASE;
    	hcd->regs = (void *)(&p_DrMemMap->hc_capbase);
        hcd->irq = MPC83xx_USB_DR_IVEC;
#endif

	retval = usb_add_hcd (hcd, hcd->irq, SA_INTERRUPT);
	if (retval != 0)
		goto err2;
	return retval;

err2:
	usb_put_hcd(hcd);
err1:
	dev_err (dev, "init fsl usb20 fail, %d\n", retval);
	return retval;
}

static int __init_or_module
fsl_usb20_remove(struct device *dev)
{
	struct usb_hcd *hcd;
	
	hcd = dev_get_drvdata(dev);
	if (!hcd)
		return -1;

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	/* XXX iounmap(), release_mem_region() here XXX */
	return 0;
}

#if defined (CONFIG_MPH_USB_SUPPORT)
static const char	fsl_usb20_name[] = "fsl-usb2-mph";
#elif defined (CONFIG_DR_USB_SUPPORT)
static const char	fsl_usb20_name[] = "fsl-usb2-dr";
#endif

static struct device_driver fsl_usb20_driver = {
	.name =		(char *)fsl_usb20_name,
	.bus =		&platform_bus_type,

	.probe =	fsl_usb20_probe,
	.remove =	fsl_usb20_remove,
};

static int __init mpc8349_usb_hc_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	printk(KERN_INFO"driver %s, %s\n", fsl_usb20_name, DRIVER_VERSION);
	return driver_register(&fsl_usb20_driver);
}

static void __exit mpc8349_usb_hc_deinit(void)
{
	driver_unregister(&fsl_usb20_driver);
}

MODULE_DESCRIPTION (DRIVER_INFO);
MODULE_AUTHOR (DRIVER_AUTHOR);
MODULE_LICENSE ("GPL");
module_init(mpc8349_usb_hc_init);
module_exit(mpc8349_usb_hc_deinit);
