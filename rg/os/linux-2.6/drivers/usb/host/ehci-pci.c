/*
 * EHCI HCD (Host Controller Driver) PCI Bus Glue.
 *
 * Copyright (c) 2000-2004 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef CONFIG_PCI
#error "This file is PCI bus glue.  CONFIG_PCI must be defined."
#endif

/*-------------------------------------------------------------------------*/

/* called after powerup, by probe or system-pm "wakeup" */
static int ehci_pci_reinit(struct ehci_hcd *ehci, struct pci_dev *pdev)
{
	u32			temp;
	int			retval;

	/* optional debug port, normally in the first BAR */
	temp = pci_find_capability(pdev, 0x0a);
	if (temp) {
		pci_read_config_dword(pdev, temp, &temp);
		temp >>= 16;
		if ((temp & (3 << 13)) == (1 << 13)) {
			temp &= 0x1fff;
			ehci->debug = ehci_to_hcd(ehci)->regs + temp;
			temp = readl(&ehci->debug->control);
			ehci_info(ehci, "debug port %d%s\n",
				HCS_DEBUG_PORT(ehci->hcs_params),
				(temp & DBGP_ENABLED)
					? " IN USE"
					: "");
			if (!(temp & DBGP_ENABLED))
				ehci->debug = NULL;
		}
	}

	/* we expect static quirk code to handle the "extended capabilities"
	 * (currently just BIOS handoff) allowed starting with EHCI 0.96
	 */

	/* PCI Memory-Write-Invalidate cycle support is optional (uncommon) */
	retval = pci_set_mwi(pdev);
	if (!retval)
		ehci_dbg(ehci, "MWI active\n");

	ehci_port_power(ehci, 0);

	return 0;
}

/* called during probe() after chip reset completes */
static int ehci_pci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	u32			temp;
	int			retval;

	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs + HC_LENGTH(readl(&ehci->caps->hc_capbase));
	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	/* NOTE:  only the parts below this line are PCI-specific */

	switch (pdev->vendor) {
	case PCI_VENDOR_ID_TDI:
		if (pdev->device == PCI_DEVICE_ID_TDI_EHCI) {
			ehci->is_tdi_rh_tt = 1;
			tdi_reset(ehci);
		}
		break;
	case PCI_VENDOR_ID_AMD:
		/* AMD8111 EHCI doesn't work, according to AMD errata */
		if (pdev->device == 0x7463) {
			ehci_info(ehci, "ignoring AMD8111 (errata)\n");
			retval = -EIO;
			goto done;
		}
		break;
	case PCI_VENDOR_ID_NVIDIA:
		/* NVidia reports that certain chips don't handle
		 * QH, ITD, or SITD addresses above 2GB.  (But TD,
		 * data buffer, and periodic schedule are normal.)
		 */
		switch (pdev->device) {
		case 0x003c:	/* MCP04 */
		case 0x005b:	/* CK804 */
		case 0x00d8:	/* CK8 */
		case 0x00e8:	/* CK8S */
			if (pci_set_consistent_dma_mask(pdev,
						DMA_31BIT_MASK) < 0)
				ehci_warn(ehci, "can't enable NVidia "
					"workaround for >2GB RAM\n");
			break;
		}
		break;
	}

	if (ehci_is_TDI(ehci))
		ehci_reset(ehci);

	/* at least the Genesys GL880S needs fixup here */
	temp = HCS_N_CC(ehci->hcs_params) * HCS_N_PCC(ehci->hcs_params);
	temp &= 0x0f;
	if (temp && HCS_N_PORTS(ehci->hcs_params) > temp) {
		ehci_dbg(ehci, "bogus port configuration: "
			"cc=%d x pcc=%d < ports=%d\n",
			HCS_N_CC(ehci->hcs_params),
			HCS_N_PCC(ehci->hcs_params),
			HCS_N_PORTS(ehci->hcs_params));

		switch (pdev->vendor) {
		case 0x17a0:		/* GENESYS */
			/* GL880S: should be PORTS=2 */
			temp |= (ehci->hcs_params & ~0xf);
			ehci->hcs_params = temp;
			break;
		case PCI_VENDOR_ID_NVIDIA:
			/* NF4: should be PCC=10 */
			break;
		}
	}

	/* Serial Bus Release Number is at PCI 0x60 offset */
	pci_read_config_byte(pdev, 0x60, &ehci->sbrn);

	/* Workaround current PCI init glitch:  wakeup bits aren't
	 * being set from PCI PM capability.
	 */
	if (!device_can_wakeup(&pdev->dev)) {
		u16	port_wake;

		pci_read_config_word(pdev, 0x62, &port_wake);
		if (port_wake & 0x0001)
			device_init_wakeup(&pdev->dev, 1);
	}

	retval = ehci_pci_reinit(ehci, pdev);
done:
	return retval;
}

/*-------------------------------------------------------------------------*/

#ifdef	CONFIG_PM

/* suspend/resume, section 4.3 */

/* These routines rely on the PCI bus glue
 * to handle powerdown and wakeup, and currently also on
 * transceivers that don't need any software attention to set up
 * the right sort of wakeup.
 * Also they depend on separate root hub suspend/resume.
 */

static int ehci_pci_suspend(struct usb_hcd *hcd, pm_message_t message)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	unsigned long		flags;
	int			rc = 0;

	if (time_before(jiffies, ehci->next_statechange))
		msleep(10);

	/* Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 *
	 * This is still racy as hcd->state is manipulated outside of
	 * any locks =P But that will be a different fix.
	 */
	spin_lock_irqsave (&ehci->lock, flags);
	if (hcd->state != HC_STATE_SUSPENDED) {
		rc = -EINVAL;
		goto bail;
	}
	writel (0, &ehci->regs->intr_enable);
	(void)readl(&ehci->regs->intr_enable);

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
 bail:
	spin_unlock_irqrestore (&ehci->lock, flags);

	// could save FLADJ in case of Vaux power loss
	// ... we'd only use it to handle clock skew

	return rc;
}

static int ehci_pci_resume(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	unsigned		port;
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	int			retval = -EINVAL;

	// maybe restore FLADJ

	if (time_before(jiffies, ehci->next_statechange))
		msleep(100);

	/* Mark hardware accessible again as we are out of D3 state by now */
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	/* If CF is clear, we lost PCI Vaux power and need to restart.  */
	if (readl(&ehci->regs->configured_flag) != FLAG_CF)
		goto restart;

	/* If any port is suspended (or owned by the companion),
	 * we know we can/must resume the HC (and mustn't reset it).
	 * We just defer that to the root hub code.
	 */
	for (port = HCS_N_PORTS(ehci->hcs_params); port > 0; ) {
		u32	status;
		port--;
		status = readl(&ehci->regs->port_status [port]);
		if (!(status & PORT_POWER))
			continue;
		if (status & (PORT_SUSPEND | PORT_RESUME | PORT_OWNER)) {
			usb_hcd_resume_root_hub(hcd);
			return 0;
		}
	}

restart:
	ehci_dbg(ehci, "lost power, restarting\n");
	usb_root_hub_lost_power(hcd->self.root_hub);

	/* Else reset, to cope with power loss or flush-to-storage
	 * style "resume" having let BIOS kick in during reboot.
	 */
	(void) ehci_halt(ehci);
	(void) ehci_reset(ehci);
	(void) ehci_pci_reinit(ehci, pdev);

	/* emptying the schedule aborts any urbs */
	spin_lock_irq(&ehci->lock);
	if (ehci->reclaim)
		ehci->reclaim_ready = 1;
	ehci_work(ehci, NULL);
	spin_unlock_irq(&ehci->lock);

	/* restart; khubd will disconnect devices */
	retval = ehci_run(hcd);

	/* here we "know" root ports should always stay powered */
	ehci_port_power(ehci, 1);

	return retval;
}
#endif

static const struct hc_driver ehci_pci_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"EHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ehci_irq,
	.flags =		HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset =		ehci_pci_setup,
	.start =		ehci_run,
#ifdef	CONFIG_PM
	.suspend =		ehci_pci_suspend,
	.resume =		ehci_pci_resume,
#endif
	.stop =			ehci_stop,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ehci_urb_enqueue,
	.urb_dequeue =		ehci_urb_dequeue,
	.endpoint_disable =	ehci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ehci_hub_status_data,
	.hub_control =		ehci_hub_control,
	.bus_suspend =		ehci_bus_suspend,
	.bus_resume =		ehci_bus_resume,
};

/*-------------------------------------------------------------------------*/

/* PCI driver selection metadata; PCI hotplugging uses this */
static const struct pci_device_id pci_ids [] = { {
	/* handle any USB 2.0 EHCI controller */
	PCI_DEVICE_CLASS(((PCI_CLASS_SERIAL_USB << 8) | 0x20), ~0),
	.driver_data =	(unsigned long) &ehci_pci_hc_driver,
	},
	{ /* end: all zeroes */ }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver ehci_pci_driver = {
	.name =		(char *) hcd_name,
	.id_table =	pci_ids,

	.probe =	usb_hcd_pci_probe,
	.remove =	usb_hcd_pci_remove,

#ifdef	CONFIG_PM
	.suspend =	usb_hcd_pci_suspend,
	.resume =	usb_hcd_pci_resume,
#endif
};

#if defined(CONFIG_ARCH_FEROCEON) || defined(CONFIG_MARVELL)
#include "ehci_marvell.c"
#endif

static int __init ehci_hcd_pci_init(void)
{
#if defined(CONFIG_ARCH_FEROCEON) || defined(CONFIG_MARVELL)
	int retval = 0;
#endif
	if (usb_disabled())
		return -ENODEV;

	pr_debug("%s: block sizes: qh %Zd qtd %Zd itd %Zd sitd %Zd\n",
		hcd_name,
		sizeof(struct ehci_qh), sizeof(struct ehci_qtd),
		sizeof(struct ehci_itd), sizeof(struct ehci_sitd));
#if defined(CONFIG_ARCH_FEROCEON) || defined(CONFIG_MARVELL)
	retval = platform_driver_register(&ehci_marvell_driver);
	if (retval < 0)
		return retval;
#endif
	retval = pci_register_driver(&ehci_pci_driver);
#if defined(CONFIG_ARCH_FEROCEON) || defined(CONFIG_MARVELL)
	if (retval < 0)
		platform_driver_unregister(&ehci_marvell_driver);
#endif
	return retval;
}
module_init(ehci_hcd_pci_init);

static void __exit ehci_hcd_pci_cleanup(void)
{
#if defined(CONFIG_ARCH_FEROCEON) || defined(CONFIG_MARVELL)
	platform_driver_unregister(&ehci_marvell_driver);
#endif
	pci_unregister_driver(&ehci_pci_driver);
}
module_exit(ehci_hcd_pci_cleanup);
