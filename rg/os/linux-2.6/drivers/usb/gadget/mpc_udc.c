/*
 * driver/usb/gadget/mpc_udc.c
 *
 * PowerPC USB Device Controller Driver
 * Driver for USB module in PPC8349 for 8349MDS platform
 *
 * Copyright (c) 2004-2005 Freescale Semiconductor, Inc.
 *
 * Author: Li Yang (leoli@freescale.com)
 *         Jiang Bo (Tanya.jiang@freescale.com)
 * Based on bare board code of Dave Liu and Shlomi Gridish.
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
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
#if 0
#define	DEBUG
#define VERBOSE
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/usb_ch9.h>
#include <linux/usb_gadget.h>
#include <linux/usb_otg.h>
#include <linux/dma-mapping.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <asm/dma.h>

#include "mpc_udc.h"

#undef	USB_TRACE

#define	DRIVER_DESC	"Freescale PowerPC USB Device Controller driver"
#define	DRIVER_AUTHOR	"Li Yang/Jiang Bo"
#define	DRIVER_VERSION	"1 August 2005"

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

static const char driver_name[] = "fsl-usb2-dr";
static const char driver_desc[] = DRIVER_DESC;

volatile static struct usb_dr_device *usb_slave_regs = NULL;
volatile static struct usb_sys_interface *usb_sys_regs = NULL;

/* it is initialized in probe()  */
static struct mpc_udc *udc_controller = NULL;

/* ep_qh_base store the base address before 2K align */
static struct ep_queue_head *ep_qh_base = NULL;

/*ep name is important in gadget, it should obey the convention of ep_match()*/
static const char *const ep_name[] = {
	"ep0-control", NULL,	/* everyone has ep0 */
	/* 5 configurable endpoints */
	"ep1out",
	"ep1in",
	"ep2out",
	"ep2in",
	"ep3out",
	"ep3in",
	"ep4out",
	"ep4in",
	"ep5out",
	"ep5in"
};
static struct usb_endpoint_descriptor
mpc_ep0_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	0,
	.bmAttributes =		USB_ENDPOINT_XFER_CONTROL,
	.wMaxPacketSize =	USB_MAX_CTRL_PAYLOAD,
};


/********************************************************************
 * 	Internal Used Function
********************************************************************/
/*-----------------------------------------------------------------
 * done() - retire a request; caller blocked irqs
 * @status : when req->req.status is -EINPROGRESSS, it is input para
 *	     else it will be a output parameter
 * req->req.status : in ep_queue() it will be set as -EINPROGRESS
 *--------------------------------------------------------------*/
static void done(struct mpc_ep *ep, struct mpc_req *req, int status)
{
	struct mpc_udc *udc = NULL;
	unsigned char stopped = ep->stopped;

	udc = (struct mpc_udc *) ep->udc;
	/* the req->queue pointer is used by ep_queue() func, in which
	 * the request will be added into a udc_ep->queue 'd tail
	 * so here the req will be dropped from the ep->queue
	 */
	list_del_init(&req->queue);

	/* req.status should be set as -EINPROGRESS in ep_queue() */
	if (req->req.status == -EINPROGRESS)
		req->req.status = status;
	else
		status = req->req.status;

	if (req->mapped) {
		dma_unmap_single(ep->udc->gadget.dev.parent,
			req->req.dma, req->req.length,
			ep_is_in(ep)
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);

		req->req.dma = DMA_ADDR_INVALID;
		req->mapped = 0;
	} else
		dma_sync_single_for_cpu(ep->udc->gadget.dev.parent,
			req->req.dma, req->req.length,
			ep_is_in(ep)
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
	if (status && (status != -ESHUTDOWN))
		VDBG("complete %s req %p stat %d len %u/%u",
			ep->ep.name, &req->req, status,
			req->req.actual, req->req.length);

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;

	spin_unlock(&ep->udc->lock);
	/* this complete() should a func implemented by gadget layer,
	 * eg fsg->bulk_in_complete() */
	if (req->req.complete)
		req->req.complete(&ep->ep, &req->req);

	spin_lock(&ep->udc->lock);
	ep->stopped = stopped;
}

/*-----------------------------------------------------------------
 * nuke(): delete all requests related to this ep
 * called by ep_disable() within spinlock held
 * add status paramter?
 *--------------------------------------------------------------*/
static void nuke(struct mpc_ep *ep, int status)
{
	ep->stopped = 1;

	/* Whether this eq has request linked */
	while (!list_empty(&ep->queue)) {
		struct mpc_req *req = NULL;

		req = list_entry(ep->queue.next, struct mpc_req, queue);

		done(ep, req, status);
	}
}


/*------------------------------------------------------------------
	Internal Hardware related function
 ------------------------------------------------------------------*/

/* @qh_addr is the aligned virt addr of ep QH addr
 * it is used to set endpointlistaddr Reg */
static int dr_controller_setup(void *qh_addr)
{
	unsigned int tmp = 0, portctrl = 0, ctrl = 0, otgsc = 0;

	/* before here, make sure usb_slave_regs has been initialized */
	if (!qh_addr)
		return -EINVAL;

	/* Stop and reset the usb controller */
	tmp = le32_to_cpu(usb_slave_regs->usbcmd);
	tmp &= ~USB_CMD_RUN_STOP;
	usb_slave_regs->usbcmd = cpu_to_le32(tmp);

	tmp = le32_to_cpu(usb_slave_regs->usbcmd);
	tmp |= USB_CMD_CTRL_RESET;
	usb_slave_regs->usbcmd = cpu_to_le32(tmp);

	/* Wait reset completed */
	while (le32_to_cpu(usb_slave_regs->usbcmd) & USB_CMD_CTRL_RESET) {
	}

	/* Set the controller as device mode */
	tmp = le32_to_cpu(usb_slave_regs->usbmode);
	tmp |= USB_MODE_CTRL_MODE_DEVICE;
	/* Disable Setup Lockout */
	tmp |= USB_MODE_SETUP_LOCK_OFF;
	usb_slave_regs->usbmode = cpu_to_le32(tmp);

	/* Clear the setup status */
	usb_slave_regs->usbsts = 0;

	tmp = virt_to_phys(qh_addr);
	tmp &= USB_EP_LIST_ADDRESS_MASK;
	usb_slave_regs->endpointlistaddr = cpu_to_le32(tmp);

	VDBG("vir[qh_base] is %p phy[qh_base] is 0x%8x reg is 0x%8x",
			qh_addr, (int)tmp, le32_to_cpu(usb_slave_regs->
							endpointlistaddr));

	/* Config PHY interface as ULPI for DR */
	portctrl = le32_to_cpu(usb_slave_regs->portsc1);
	portctrl &= ~PORTSCX_PHY_TYPE_SEL;
	portctrl |= PORTSCX_PTS_ULPI;

	usb_slave_regs->portsc1 = cpu_to_le32(portctrl);

	/* Config control enable i/o output, big endian register */
	ctrl = usb_sys_regs->control;
	ctrl |= USB_CTRL_IOENB;
	usb_sys_regs->control = ctrl;

	/* Config snooping, keep the cache consistent
	 * Snoop between 0x0 and 1GB */
	tmp = 0x00000000;
	tmp &= SNOOP_ADDRESS_MASK;
	usb_sys_regs->snoop1 = tmp | SNOOP_SIZE_1GB;

	/* FIXME: VBUS charge */
	otgsc = le32_to_cpu(usb_slave_regs->otgsc);
	otgsc |= OTGSC_CTRL_VUSB_CHARGE;
	usb_slave_regs->otgsc = le32_to_cpu(otgsc);

	return 0;
}

/* just Enable DR irq reg and Set Dr controller Run */
static void dr_controller_run(void)
{
	unsigned int tmp_intr = 0, tmp_cmd = 0;

	/*Enable DR irq reg */
	tmp_intr = USB_INTR_INT_EN | USB_INTR_ERR_INT_EN |
		USB_INTR_PTC_DETECT_EN | USB_INTR_RESET_EN |
		USB_INTR_DEVICE_SUSPEND | USB_INTR_SYS_ERR_EN;

	usb_slave_regs->usbintr = le32_to_cpu(tmp_intr);

	/* Set controller to Run */
	tmp_cmd = le32_to_cpu(usb_slave_regs->usbcmd);
	tmp_cmd |= USB_CMD_RUN_STOP;
	usb_slave_regs->usbcmd = le32_to_cpu(tmp_cmd);

	return;

}
static void dr_controller_stop(void)
{
	unsigned int tmp;

	/* disable all INTR */
	usb_slave_regs->usbintr = 0;

	/* FIXME: disable VBUS charge */
	tmp = le32_to_cpu(usb_slave_regs->otgsc);
	tmp &= ~OTGSC_CTRL_VUSB_CHARGE;
	usb_slave_regs->otgsc = le32_to_cpu(tmp);

	/* disable IO output */
	usb_sys_regs->control = 0;

	/* set controller to Stop */
	tmp = le32_to_cpu(usb_slave_regs->usbcmd);
	tmp &= ~USB_CMD_RUN_STOP;
	usb_slave_regs->usbcmd = le32_to_cpu(tmp);

	return;
}

void dr_ep_setup(unsigned char ep_num, unsigned char dir,
		unsigned char ep_type)
{
	unsigned int tmp_epctrl = 0;

	tmp_epctrl = le32_to_cpu(usb_slave_regs->endptctrl[ep_num]);
	if (dir) {
		if (ep_num)
			tmp_epctrl |= EPCTRL_TX_DATA_TOGGLE_RST;
		tmp_epctrl |= EPCTRL_TX_ENABLE;
		tmp_epctrl |=
			((unsigned int) (ep_type) << EPCTRL_TX_EP_TYPE_SHIFT);
	} else {
		if (ep_num)
			tmp_epctrl |= EPCTRL_RX_DATA_TOGGLE_RST;
		tmp_epctrl |= EPCTRL_RX_ENABLE;
		tmp_epctrl |=
			((unsigned int) (ep_type) << EPCTRL_RX_EP_TYPE_SHIFT);
	}

	usb_slave_regs->endptctrl[ep_num] = cpu_to_le32(tmp_epctrl);

	/* wait the write reg finished */
	while (!(le32_to_cpu(usb_slave_regs->endptctrl[ep_num]) &
		 (tmp_epctrl & (EPCTRL_TX_ENABLE | EPCTRL_RX_ENABLE))));


}

static void dr_ep_change_stall(unsigned char ep_num, unsigned char dir,
				int value)
{
	unsigned int tmp_epctrl = 0;

	tmp_epctrl = le32_to_cpu(usb_slave_regs->endptctrl[ep_num]);

	if (value) {
		/* set the stall bit */
		if (dir)
			tmp_epctrl |= EPCTRL_TX_EP_STALL;
		else
			tmp_epctrl |= EPCTRL_RX_EP_STALL;
	} else {
		/* clear the stall bit and reset data toggle */
		if (dir) {
			tmp_epctrl &= ~EPCTRL_TX_EP_STALL;
			tmp_epctrl |= EPCTRL_TX_DATA_TOGGLE_RST;
		}
		else {
			tmp_epctrl &= ~EPCTRL_RX_EP_STALL;
			tmp_epctrl |= EPCTRL_RX_DATA_TOGGLE_RST;
		}

	}
	usb_slave_regs->endptctrl[ep_num] = cpu_to_le32(tmp_epctrl);
}

#if 0
static void dr_ep_flush(unsigned char ep_num, unsigned char dir)
{
	unsigned int tmp_flush = 0;

	tmp_flush = le32_to_cpu(usb_slave_regs->endptflush);

	tmp_flush |= (dir == USB_SEND)?(EPFLUSH_TX_OFFSET<<ep_num):
		(1<<ep_num);
	usb_slave_regs->endptflush = cpu_to_le32(tmp_flush);
}
#endif


/********************************************************************
	Internal Structure Build up functions
********************************************************************/

/*------------------------------------------------------------------
* struct_ep_qh_setup(): set the Endpoint Capabilites field of QH
 * @zlt: Zero Length Termination Select
 * @mult: Mult field
 ------------------------------------------------------------------*/
static void struct_ep_qh_setup(void *handle, unsigned char ep_num,
				unsigned char dir, unsigned char ep_type,
				unsigned int max_pkt_len,
				unsigned int zlt, unsigned char mult)
{
	struct mpc_udc *udc = NULL;
	struct ep_queue_head *p_QH = NULL;
	unsigned int tmp = 0;

	udc = (struct mpc_udc *) handle;

	p_QH = &udc->ep_qh[2 * ep_num + dir];

	/* set the Endpoint Capabilites Reg of QH */
	switch (ep_type) {
	case USB_ENDPOINT_XFER_CONTROL:
		/* Interrupt On Setup (IOS). for control ep  */
		tmp = (max_pkt_len << EP_QUEUE_HEAD_MAX_PKT_LEN_POS) |
				EP_QUEUE_HEAD_IOS;
		break;
	case USB_ENDPOINT_XFER_ISOC:
		tmp = (max_pkt_len << EP_QUEUE_HEAD_MAX_PKT_LEN_POS) |
				(mult << EP_QUEUE_HEAD_MULT_POS);
		break;
	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		tmp = max_pkt_len << EP_QUEUE_HEAD_MAX_PKT_LEN_POS;
		if (zlt)
			tmp |= EP_QUEUE_HEAD_ZLT_SEL;
		break;
	default:
		VDBG("error ep type is %d",ep_type);
		return;
	}
	p_QH->max_pkt_length = le32_to_cpu(tmp);
	flush_dcache_range((unsigned long)p_QH, (unsigned long)p_QH +
			sizeof(struct ep_queue_head));

	return;
}

/* This function only to make code looks good
 * it is a collection of struct_ep_qh_setup and dr_ep_setup for ep0
 * ep0 should set OK before the bind() of gadget layer
 */
static void ep0_dr_and_qh_setup(struct mpc_udc *udc)
{
	/* the intialization of an ep includes: fields in QH, Regs,
	 * mpc_ep struct */
	struct_ep_qh_setup(udc, 0, USB_RECV,
			USB_ENDPOINT_XFER_CONTROL, USB_MAX_CTRL_PAYLOAD,
			0, 0);
	struct_ep_qh_setup(udc, 0, USB_SEND,
			USB_ENDPOINT_XFER_CONTROL, USB_MAX_CTRL_PAYLOAD,
			0, 0);
	dr_ep_setup(0, USB_RECV, USB_ENDPOINT_XFER_CONTROL);
	dr_ep_setup(0, USB_SEND, USB_ENDPOINT_XFER_CONTROL);

	return;

}

/***********************************************************************
		Endpoint Management Functions
***********************************************************************/

/*-------------------------------------------------------------------------
 * when configurations are set, or when interface settings change
 * for example the do_set_interface() in gadget layer,
 * the driver will enable or disable the relevant endpoints
 * ep0 will not use this func it is enable in probe()
-------------------------------------------------------------------------*/
static int mpc_ep_enable(struct usb_ep *_ep,
			const struct usb_endpoint_descriptor *desc)
{
	struct mpc_udc *udc = NULL;
	struct mpc_ep *ep = NULL;
	unsigned short max = 0;
	unsigned char mult = 0, zlt = 0;
	int retval = 0;
	unsigned long flags = 0;
	char *val = NULL; /* for debug */

	ep = container_of(_ep, struct mpc_ep, ep);

	/* catch various bogus parameters */
	if (!_ep || !desc || ep->desc || _ep->name == ep_name[0] ||
		(desc->bDescriptorType != USB_DT_ENDPOINT))
		/* FIXME: add judge for ep->bEndpointAddress */
		return -EINVAL;

	udc = ep->udc;

	if (!udc->driver || (udc->gadget.speed == USB_SPEED_UNKNOWN))
		return -ESHUTDOWN;

	max = le16_to_cpu(desc->wMaxPacketSize);
	retval = -EINVAL;

	/* check the max package size validate for this endpoint */
	/* Refer to USB2.0 spec table 9-13,
	 */
	switch (desc->bmAttributes & 0x03) {
	case USB_ENDPOINT_XFER_BULK:
		if (strstr(ep->ep.name, "-iso")
				|| strstr(ep->ep.name, "-int"))
			goto en_done;
		mult = 0;
		zlt = 1;
		switch (udc->gadget.speed) {
		case USB_SPEED_HIGH:
			if ((max == 128) || (max == 256) || (max == 512))
				break;
		default:
			switch (max) {
			case 4:
			case 8:
			case 16:
			case 32:
			case 64:
				break;
			default:
			case USB_SPEED_LOW:
				goto en_done;
			}
		}
		break;
	case USB_ENDPOINT_XFER_INT:
		if (strstr(ep->ep.name, "-iso"))	/* bulk is ok */
			goto en_done;
		mult = 0;
		zlt = 1;
		switch (udc->gadget.speed) {
		case USB_SPEED_HIGH:
			if (max <= 1024)
				break;
		case USB_SPEED_FULL:
			if (max <= 64)
				break;
		default:
			if (max <= 8)
				break;
			goto en_done;
		}
		break;
	case USB_ENDPOINT_XFER_ISOC:
		if (strstr(ep->ep.name, "-bulk") || strstr(ep->ep.name, "-int"))
			goto en_done;
		mult = (unsigned char) (1 +((le16_to_cpu(desc->wMaxPacketSize)
						>> 11) & 0x03));
		zlt = 0;
		switch (udc->gadget.speed) {
		case USB_SPEED_HIGH:
			if (max <= 1024)
				break;
		case USB_SPEED_FULL:
			if (max <= 1023)
				break;
		default:
			goto en_done;
		}
		break;
	case USB_ENDPOINT_XFER_CONTROL:
		if (strstr(ep->ep.name, "-iso")
				|| strstr(ep->ep.name, "-int"))
			goto en_done;
		mult = 0;
		zlt = 1;
		switch (udc->gadget.speed) {
		case USB_SPEED_HIGH:
		case USB_SPEED_FULL:
			switch (max) {
			case 1:
			case 2:
			case 4:
			case 8:
			case 16:
			case 32:
			case 64:
				break;
			default:
				goto en_done;
			}
		case USB_SPEED_LOW:
			switch (max) {
			case 1:
			case 2:
			case 4:
			case 8:
				break;
			default:
				goto en_done;
			}
		default:
			goto en_done;
		}
		break;

	default:
		goto en_done;
	}

	/* here initialize variable of ep */
	spin_lock_irqsave(&udc->lock, flags);
	ep->ep.maxpacket = max;
	ep->desc = desc;
	ep->stopped = 0;

	/* hardware special operation */

	/* Init EPx Queue Head (Ep Capabilites field in QH
	 * according to max, zlt, mult) */
	struct_ep_qh_setup((void *) udc, (unsigned char) ep_index(ep),
			(unsigned char) ((desc->
					bEndpointAddress & USB_DIR_IN) ?
					USB_SEND : USB_RECV),
			(unsigned char) (desc->bmAttributes &
					USB_ENDPOINT_XFERTYPE_MASK),
			max, zlt, mult);

	/* Init endpoint x at here */
	/* 83xx RM chapter 16.3.2.24, here init the endpoint ctrl reg */
	dr_ep_setup((unsigned char) ep_index(ep),
			(unsigned char) ((desc->bEndpointAddress & USB_DIR_IN)
					? USB_SEND : USB_RECV),
			(unsigned char) (desc->bmAttributes &
					USB_ENDPOINT_XFERTYPE_MASK));

	/* Now HW will be NAKing transfers to that EP,
	 * until a buffer is queued to it. */

	/* should have stop the lock */
	spin_unlock_irqrestore(&udc->lock, flags);
	retval = 0;
	switch (desc->bmAttributes & 0x03) {
	case USB_ENDPOINT_XFER_BULK:
		val = "bulk";
		break;
	case USB_ENDPOINT_XFER_ISOC:
		val = "iso";
		break;
	case USB_ENDPOINT_XFER_INT:
		val = "intr";
		break;
	default:
		val = "ctrl";
		break;
	}

	VDBG("enabled %s (ep%d%s-%s) maxpacket %d",ep->ep.name,
			ep->desc->bEndpointAddress & 0x0f,
			(desc->bEndpointAddress & USB_DIR_IN) ?
			"in" : "out", val, max);
en_done:
	return retval;
}

/*---------------------------------------------------------------------
 * @ep : the ep being unconfigured. May not be ep0
 * Any pending and uncomplete req will complete with status (-ESHUTDOWN)
*---------------------------------------------------------------------*/
static int mpc_ep_disable(struct usb_ep *_ep)
{
	struct mpc_udc *udc = NULL;
	struct mpc_ep *ep = NULL;
	unsigned long flags = 0;

	ep = container_of(_ep, struct mpc_ep, ep);
	if (!_ep || !ep->desc) {
		VDBG("%s not enabled", _ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	udc = (struct mpc_udc *) ep->udc;

	spin_lock_irqsave(&udc->lock, flags);

	/* Nuke all pending requests (does flush) */
	nuke(ep, -ESHUTDOWN);

	ep->desc = 0;
	ep->stopped = 1;
	spin_unlock_irqrestore(&udc->lock, flags);

	VDBG("disabled %s OK", _ep->name);
	return 0;
}

/*---------------------------------------------------------------------
 * allocate a request object used by this endpoint
 * the main operation is to insert the req->queue to the eq->queue
 * Returns the request, or null if one could not be allocated
*---------------------------------------------------------------------*/
static struct usb_request *mpc_alloc_request(struct usb_ep *_ep,
						gfp_t gfp_flags)
{
	struct mpc_req *req = NULL;

	req = kmalloc(sizeof *req, gfp_flags);
	if (!req)
		return NULL;

	memset(req, 0, sizeof *req);
	req->req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

static void mpc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{

	struct mpc_req *req = NULL;

	req = container_of(_req, struct mpc_req, req);

	if (_req)
		kfree(req);
}

/*------------------------------------------------------------------
 * Allocate an I/O buffer for the ep->req->buf
 * @len: length of the desired buffer
 * @dma: pointer to the buffer's DMA address; must be valid
 * 	 when gadget layer calls this function, ma is &req->dma
 * @gfp_flags : GFP_* flags to use
 * Returns a new buffer, or null if one could not be allocated
*---------------------------------------------------------------------*/
static void *mpc_alloc_buffer(struct usb_ep *_ep, unsigned bytes,
				dma_addr_t * dma, gfp_t gfp_flags)
{
	void *retval = NULL;

	if (!bytes)
		return 0;

	retval = kmalloc(bytes, gfp_flags);
	if (retval)
		*dma = virt_to_phys(retval);
	return retval;
}

/*------------------------------------------------------------------
 * Free an I/O buffer for the ep->req->buf
 * @dma:for 834x, we will not touch dma field
*---------------------------------------------------------------------*/
static void mpc_free_buffer(struct usb_ep *_ep, void *buf,
				dma_addr_t dma, unsigned bytes)
{
	if (buf)
		kfree(buf);
}



/*-------------------------------------------------------------------------*/

static int mpc_queue_td(struct mpc_ep *ep, struct mpc_req *req)
{
	int i = ep_index(ep) * 2 + ep_is_in(ep);
	u32 temp, bitmask, tmp_stat;
	struct ep_queue_head *dQH = &ep->udc->ep_qh[i];

	//VDBG("QH addr Register 0x%8x", usb_slave_regs->endpointlistaddr);
	//VDBG("ep_qh[%d] addr is 0x%8x", i, (u32)&(ep->udc->ep_qh[i]));

	bitmask = (ep_is_in(ep)) ? (1 << (ep_index(ep) + 16)) :
						(1 << (ep_index(ep)));

	/* check if the pipe is empty */
	if (!(list_empty(&ep->queue))) {
		/* Add td to the end */
		struct mpc_req *lastreq;
		lastreq = list_entry(ep->queue.prev, struct mpc_req, queue);
		lastreq->tail->next_td_ptr =
			cpu_to_le32(virt_to_phys(req->head) & DTD_ADDR_MASK);

		/* Read prime bit, if 1 goto done */
		if (usb_slave_regs->endpointprime & cpu_to_le32(bitmask))
			goto out;

		do {
			/* Set ATDTW bit in USBCMD */
			usb_slave_regs->usbcmd |=
				cpu_to_le32(USB_CMD_ATDTW);

			/* Read correct status bit */
			tmp_stat = le32_to_cpu(usb_slave_regs->
					endptstatus) & bitmask;
		} while (!(usb_slave_regs->
			usbcmd & cpu_to_le32(USB_CMD_ATDTW)));

		/* Write ATDTW bit to 0 */
		usb_slave_regs->usbcmd &= cpu_to_le32(~USB_CMD_ATDTW);

		if (tmp_stat)
			goto out;
	}

	/* Write dQH next pointer and terminate bit to 0 */
	temp = virt_to_phys((void *)req->head) &
			EP_QUEUE_HEAD_NEXT_POINTER_MASK;
	dQH->next_dtd_ptr = cpu_to_le32(temp);

	/* Clear active and halt bit */
	temp = cpu_to_le32(~(EP_QUEUE_HEAD_STATUS_ACTIVE |
			EP_QUEUE_HEAD_STATUS_HALT));
	dQH->size_ioc_int_sts &= temp;
	flush_dcache_range((unsigned long)dQH,(unsigned long)dQH +
			sizeof(struct ep_queue_head));

	/* Prime endpoint by writing 1 to ENDPTPRIME */
	temp = (ep_is_in(ep)) ? (1 << (ep_index(ep) + 16)) :
			(1 << (ep_index(ep)));

	usb_slave_regs->endpointprime = cpu_to_le32(temp);
out:
	return 0;
}

static int
mpc_build_dtd (struct mpc_req *req, unsigned max, struct ep_td_struct **address)
{
	unsigned length;
	u32 swap_temp;
	struct ep_td_struct *dtd;

	/* how big will this packet be? */
	length = min(req->req.length - req->req.actual, max);

	/* Assume CACHELINE alignment garantees 32-byte alignment */
	dtd = kmalloc(sizeof(struct ep_td_struct), GFP_KERNEL);
	/* check alignment */
	if ((u32) dtd & ~DTD_ADDR_MASK)
		panic("Can not allocate aligned memory for dtd");

	memset(dtd, 0, sizeof(struct ep_td_struct));

	/* Fill in the transfer size; set interrupt on every dtd;
	   set active bit */
	swap_temp = ((length << DTD_LENGTH_BIT_POS) | DTD_IOC
						| DTD_STATUS_ACTIVE);

	dtd->size_ioc_sts = cpu_to_le32(swap_temp);

	/* Clear reserved field */
	swap_temp = cpu_to_le32(dtd->size_ioc_sts);
	swap_temp &= ~DTD_RESERVED_FIELDS;
	dtd->size_ioc_sts = cpu_to_le32(swap_temp);

	/* Init all of buffer page pointers */
	swap_temp = (u32) (req->req.dma + req->req.actual);
	dtd->buff_ptr0 = cpu_to_le32(swap_temp);
	dtd->buff_ptr1 = cpu_to_le32(swap_temp + 0x1000);
	dtd->buff_ptr2 = cpu_to_le32(swap_temp + 0x2000);
	dtd->buff_ptr3 = cpu_to_le32(swap_temp + 0x3000);
	dtd->buff_ptr4 = cpu_to_le32(swap_temp + 0x4000);

	req->req.actual += length;
	*address = dtd;
	VDBG("length = %d address= 0x%x", length, (int) dtd);

	return length;
}

static int
mpc_req_to_dtd (struct mpc_req *req)
{
	unsigned		max;
	unsigned	count;
	int		is_last;
	int		is_first =1;
	struct ep_td_struct	*last_addr = NULL, *addr;

	max = EP_MAX_LENGTH_TRANSFER;
	do {
		count = mpc_build_dtd(req, max, &addr);

		if (is_first) {
			is_first = 0;
			req->head = addr;
		}
		else {
			last_addr->next_td_ptr
				= cpu_to_le32(virt_to_phys(addr));
			flush_dcache_range((unsigned long)last_addr,
					(unsigned long)last_addr +
					sizeof(struct ep_td_struct));
			last_addr = addr;
		}
		/* last packet is usually short (or a zlp) */
		if (unlikely (count != max))
			is_last = 1;
		else if (likely(req->req.length != req->req.actual)
					|| req->req.zero)
				is_last = 0;
			else
				is_last = 1;

		req->dtd_count ++;
	}while(!is_last);

	addr->next_td_ptr = cpu_to_le32(DTD_NEXT_TERMINATE);
	flush_dcache_range((unsigned long)addr, (unsigned long)addr +
			sizeof(struct ep_td_struct));

	req->tail = addr;

	return 0;
}


/* queues (submits) an I/O request to an endpoint */
static int
mpc_ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	struct mpc_ep *ep = container_of(_ep, struct mpc_ep, ep);
	struct mpc_req *req = container_of(_req, struct mpc_req, req);
	struct mpc_udc *udc;
	unsigned long flags;
	int is_iso = 0;

	/* catch various bogus parameters */
	if (!_req || !req->req.complete || !req->req.buf
		|| !list_empty(&req->queue) ) {
		VDBG("%s, bad params\n", __FUNCTION__);
		return -EINVAL;
	}
	if (!_ep || (!ep->desc && ep_index(ep))) {
		VDBG("%s, bad ep\n", __FUNCTION__);
		return -EINVAL;
	}
	if (ep->desc->bmAttributes == USB_ENDPOINT_XFER_ISOC) {
		if (req->req.length > ep->ep.maxpacket)
			return -EMSGSIZE;
		is_iso = 1;
	}

	udc = ep->udc;
	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	req->ep = ep;

	/* map virtual address to hardware */
	if (req->req.dma == DMA_ADDR_INVALID) {
		req->req.dma = dma_map_single(ep->udc->gadget.dev.parent,
					req->req.buf,
					req->req.length,
					ep_is_in(ep)
					? DMA_TO_DEVICE :
					DMA_FROM_DEVICE);
		req->mapped = 1;
	} else {
		dma_sync_single_for_device(ep->udc->gadget.dev.parent,
					req->req.dma, req->req.length,
					ep_is_in(ep)
					? DMA_TO_DEVICE :
					DMA_FROM_DEVICE);
		req->mapped = 0;
	}

	req->req.status = -EINPROGRESS;
	req->req.actual = 0;
	req->dtd_count = 0;

	spin_lock_irqsave(&udc->lock, flags);

	/* push the dtds to device queue */
	if (!mpc_req_to_dtd(req))
		mpc_queue_td(ep, req);

	/* EP0 */
	if ((ep_index(ep) == 0))
		udc->ep0_state = DATA_STATE_XMIT;

	/* irq handler advances the queue */
	if (req != NULL)
		list_add_tail(&req->queue, &ep->queue);
	spin_unlock_irqrestore(&udc->lock, flags);


	return 0;
}

/* dequeues (cancels, unlinks) an I/O request from an endpoint */
static int mpc_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct mpc_ep *ep = container_of(_ep, struct mpc_ep, ep);
	struct mpc_req *req;
	unsigned long flags;

	if (!_ep || !_req)
		return -EINVAL;

	spin_lock_irqsave(&ep->udc->lock, flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req) {
		spin_unlock_irqrestore(&ep->udc->lock, flags);
		return -EINVAL;
	}

	done(ep, req, -ECONNRESET);

	spin_unlock_irqrestore(&ep->udc->lock, flags);
	return 0;

}

/*-------------------------------------------------------------------------*/

/*-----------------------------------------------------------------
 * modify the endpoint halt feature
 * @ep: the non-isochronous endpoint being stalled
 * @value: 1--set halt  0--clear halt
 * Returns zero, or a negative error code.
*----------------------------------------------------------------*/
static int _mpc_ep_set_halt(struct usb_ep *_ep, int value)
{

	struct mpc_ep *ep = NULL;
	unsigned long flags = 0;
	int status = -EOPNOTSUPP; /* operation not supported */
	unsigned char ep_dir = 0, ep_num = 0;
	struct mpc_udc *udc = NULL;

	ep = container_of(_ep, struct mpc_ep, ep);
	udc = ep->udc;
	if (!_ep || !ep->desc) {
		status = -EINVAL;
		goto out;
	}

	if (ep->desc->bmAttributes == USB_ENDPOINT_XFER_ISOC) {
		status = -EOPNOTSUPP;
		goto out;
	}

	/* Attemp to halt IN ep will fail if any transfer requests
	   are still queue */
	if ( value && ep_is_in(ep) && !list_empty(&ep->queue) ) {

		status = -EAGAIN;
		goto out;
	}

	status = 0;
	ep_dir = ep_is_in(ep) ? USB_SEND : USB_RECV;
	ep_num = (unsigned char) (ep_index(ep));
	spin_lock_irqsave(&ep->udc->lock, flags);
	dr_ep_change_stall(ep_num, ep_dir, value);
	spin_unlock_irqrestore(&ep->udc->lock, flags);

	if ( ep_index(ep) == 0 )
	{
		udc->ep0_state = WAIT_FOR_SETUP;
		udc->ep0_dir = 0;
	}
out:
	VDBG(" %s %s halt stat %d", ep->ep.name, value ?
				"set" : "clear", status);

	return status;

}
static int mpc_ep_set_halt(struct usb_ep *_ep, int value)
{
	return (_mpc_ep_set_halt(_ep, value));
}

static struct usb_ep_ops mpc_ep_ops = {
	.enable = mpc_ep_enable,
	.disable = mpc_ep_disable,

	.alloc_request = mpc_alloc_request,
	.free_request = mpc_free_request,

	.alloc_buffer = mpc_alloc_buffer,
	.free_buffer = mpc_free_buffer,

	.queue = mpc_ep_queue,
	.dequeue = mpc_ep_dequeue,

	.set_halt = mpc_ep_set_halt,
//	.fifo_status	= mpc_ep_fifo_status,	/* report bytes in fifo */
//	.fifo_flush	= mpc_ep_fifo_flush,	/* flush fifo */
};

/*-------------------------------------------------------------------------
		Gadget Driver Layer Operations
-------------------------------------------------------------------------*/

/*************************************************************************
		Gadget Driver Layer Operations
*************************************************************************/

/*----------------------------------------------------------------------
 * Get the current frame number (from DR frame_index Reg )
 *----------------------------------------------------------------------*/
static int mpc_get_frame(struct usb_gadget *gadget)
{
	return (int) (le32_to_cpu(usb_slave_regs->frindex) &
			USB_FRINDEX_MASKS);
}

/*-----------------------------------------------------------------------
 * Tries to wake up the host connected to this gadget
 *
 * Return : 0-success
 * Negative-this feature not enabled by host or not supported by device hw
 * FIXME: RM 16.6.2.2.1 DR support this wake-up feature?
 -----------------------------------------------------------------------*/
static int mpc_wakeup(struct usb_gadget *gadget)
{
	return -ENOTSUPP;
}

/* sets the device selfpowered feature
 * this affects the device status reported by the hw driver
 * to reflect that it now has a local power supply
 * usually device hw has register for this feature
 */
static int
mpc_set_selfpowered(struct usb_gadget *gadget, int is_selfpowered)
{
	return -ENOTSUPP;
}

/* Notify controller that VBUS is powered, Called by whatever
   detects VBUS sessions */

static int mpc_vbus_session(struct usb_gadget *gadget, int is_active)
{
	return -ENOTSUPP;
}

/* constrain controller's VBUS power usage
 * This call is used by gadget drivers during SET_CONFIGURATION calls,
 * reporting how much power the device may consume.  For example, this
 * could affect how quickly batteries are recharged.
 *
 * Returns zero on success, else negative errno.
 */
static int mpc_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	return -ENOTSUPP;
}

/* Change Data+ pullup status
 * this func is used by usb_gadget_connect/disconnet
 */
static int mpc_pullup(struct usb_gadget *gadget, int is_on)
{
	return -ENOTSUPP;
}

/* defined in usb_gadget.h */
static struct usb_gadget_ops mpc_gadget_ops = {
	.get_frame = mpc_get_frame,
	.wakeup = mpc_wakeup,
	.set_selfpowered = mpc_set_selfpowered,
	.vbus_session = mpc_vbus_session,
	.vbus_draw = mpc_vbus_draw,
	.pullup = mpc_pullup,
};

static void Ep0Stall(struct mpc_udc *udc)
{
	u32 tmp;

	/* a protocol stall */
	tmp = le32_to_cpu(usb_slave_regs->endptctrl[0]);
	tmp |= EPCTRL_TX_EP_STALL | EPCTRL_RX_EP_STALL;
	usb_slave_regs->endptctrl[0] = cpu_to_le32(tmp);
	udc->ep0_state = WAIT_FOR_SETUP;
	udc->ep0_dir = 0;
}

/* if direction is EP_IN, the status is Device->Host
 * if direction is EP_OUT, the status transaction is Device<-Host
 */
static int ep0_prime_status(struct mpc_udc *udc, int direction)
{

	struct mpc_req *req = udc->status_req;
	struct mpc_ep *ep;
	int status = 0;

	if (direction == EP_DIR_IN)
		udc->ep0_dir = USB_DIR_IN;
	else
		udc->ep0_dir = USB_DIR_OUT;

	ep = &udc->eps[0];
	udc->ep0_state = WAIT_FOR_OUT_STATUS;

	req = container_of(mpc_alloc_request(&ep->ep, GFP_KERNEL),
			 struct mpc_req, req);
	req->ep = ep;
	req->req.length = 0;
	req->req.status = -EINPROGRESS;
	req->req.actual = 0;
	req->req.complete = NULL;

	if ((mpc_req_to_dtd(req) == 0))
		status = mpc_queue_td(ep, req);
	if (status)
		printk("Can't get control status request \n");
	list_add_tail(&req->queue, &ep->queue);

	return status;
}

static int udc_reset_ep_queue(struct mpc_udc *udc, u8 pipe)
{

	struct mpc_ep *ep = get_ep_by_pipe(udc, pipe);

	/* FIXME: collect completed requests? */
	if (!ep->name)
		return 0;

	nuke(ep, -ECONNRESET);

	return 0;
}

/*
 * ch9 Set address
 */
static void ch9SetAddress(struct mpc_udc *udc, u16 value, u16 index,
			  u16 length)
{
	/* Save the new address to device struct */
	udc->device_address = (u8) value;

	/* Update usb state */
	udc->usb_state = USB_STATE_ADDRESS;

	/* Status phase */
	if (ep0_prime_status(udc, EP_DIR_IN))
		Ep0Stall(udc);
	udc->ep0_state = WAIT_FOR_SETUP;
}


/*
 * ch9 Get status
 */
static void ch9GetStatus(struct mpc_udc *udc, u16 value, u16 index,
			 u16 length)
{
	u16 usb_status = 0;	/* fix me to give correct status */

	struct mpc_req *req;
	struct mpc_ep *ep;
	int status = 0;

	ep = &udc->eps[0];

	req = container_of(mpc_alloc_request(&ep->ep, GFP_KERNEL),
					struct mpc_req, req);
	req->req.length = 2;
	req->req.buf = &usb_status;
	req->req.status = -EINPROGRESS;
	req->req.actual = 0;

	/* data phase */
	if ((mpc_req_to_dtd(req) == 0))
		status = mpc_queue_td(ep, req);
	if (status) {
		printk("Can't respond to getstatus request \n");
		Ep0Stall(udc);
	} else
		udc->ep0_state = DATA_STATE_XMIT;

}


/*
 * ch9 Set config
 */
static void ch9SetConfig(struct mpc_udc *udc, u16 value, u16 index,
			 u16 length)
{

	udc->ep0_dir = USB_DIR_IN;
	if (udc->driver->setup(&udc->gadget, &udc->local_setup_buff) >= 0)
	{
		/* gadget layer deal with the status phase */
		udc->usb_state = USB_STATE_CONFIGURED;
		udc->ep0_state = WAIT_FOR_OUT_STATUS;
	}
}

static void setup_received_irq(struct mpc_udc *udc,
				struct usb_ctrlrequest *setup)
{
	unsigned wValue = le16_to_cpu(setup->wValue);
	unsigned wIndex = le16_to_cpu(setup->wIndex);
	unsigned wLength = le16_to_cpu(setup->wLength);

	udc_reset_ep_queue(udc, 0);

	/* We asume setup only occurs on EP0 */
	if (setup->bRequestType & USB_DIR_IN)
		udc->ep0_dir = USB_DIR_IN;
	else
		udc->ep0_dir = USB_DIR_OUT;

	switch (setup->bRequest) {
	case USB_BULK_RESET_REQUEST:
		if ((setup->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS)
			break;
		udc->ep0_dir = USB_DIR_IN;
		if (udc->driver->setup(&udc->gadget,
					&udc->local_setup_buff) >= 0)
			udc->ep0_state = WAIT_FOR_SETUP;
		break;

	case USB_REQ_GET_STATUS:
		if ((setup-> bRequestType & (USB_DIR_IN | USB_TYPE_STANDARD))
					!= (USB_DIR_IN | USB_TYPE_STANDARD))
			break;
		ch9GetStatus(udc, wValue, wIndex, wLength);
		break;

	case USB_REQ_SET_ADDRESS:
		if (setup->bRequestType != (USB_DIR_OUT | USB_TYPE_STANDARD |
						USB_RECIP_DEVICE))
			break;
		ch9SetAddress(udc, wValue, wIndex, wLength);
		break;

	case USB_REQ_SET_CONFIGURATION:
		if (setup->bRequestType != (USB_DIR_OUT | USB_TYPE_STANDARD |
						USB_RECIP_DEVICE))
			break;
		/* gadget layer take over the status phase */
		ch9SetConfig(udc, wValue, wIndex, wLength);
		break;
	case USB_REQ_SET_INTERFACE:
		if (setup->bRequestType != (USB_DIR_OUT | USB_TYPE_STANDARD |
						USB_RECIP_INTERFACE))
			break;
		udc->ep0_dir = USB_DIR_IN;
		if (udc->driver->setup(&udc->gadget,
					&udc->local_setup_buff) >= 0)
			/* gadget layer take over the status phase */
		break;
		/* Requests with no data phase */
	case USB_REQ_CLEAR_FEATURE:
	case USB_REQ_SET_FEATURE:
	{
		/* status transaction */
		int rc = -EOPNOTSUPP;
		/* we only support set/clear feature for endpoint */
		if (setup->bRequestType == USB_RECIP_ENDPOINT) {
			int dir = (wIndex & 0x0080) ?
					EP_DIR_IN: EP_DIR_OUT;
			int num = (wIndex & 0x000f) ;
			struct mpc_ep *ep ;

			if (wValue != 0 || wLength != 0
					|| (num *2 + dir) > USB_MAX_PIPES)
				break;
			ep = &udc->eps[num*2+dir];

			if (setup->bRequest == USB_REQ_SET_FEATURE) {
				rc = _mpc_ep_set_halt(&ep->ep, 1);
			}
			else {
				rc = _mpc_ep_set_halt(&ep->ep, 0);
			}

		}
		if ( rc == 0) {
			/* send status only if _mpc_ep_set_halt success */
			if (ep0_prime_status(udc, EP_DIR_IN))
				Ep0Stall(udc);
		}
		break;
	}
	default:
		if (udc->driver->setup(&udc->gadget, &udc->local_setup_buff)
						!= 0)
			{
				Ep0Stall(udc);
			}
		else if (setup->bRequestType & USB_DIR_IN)
			udc->ep0_state = DATA_STATE_XMIT;
		else
			udc->ep0_state = DATA_STATE_RECV;
		break;
	}

}



static void ep0_req_complete(struct mpc_udc *udc, struct mpc_ep *ep0,
				struct mpc_req *req)
{
	if (udc->usb_state == USB_STATE_ADDRESS) {
		/* Set the new address */
		u32 new_address = (u32) udc->device_address;
		usb_slave_regs->deviceaddr = cpu_to_le32(new_address <<
					USB_DEVICE_ADDRESS_BIT_POS);
	}

	switch (udc->ep0_state) {
	case DATA_STATE_XMIT:

		done(ep0, req, 0);
		/* receive status phase */
		if (ep0_prime_status(udc, EP_DIR_OUT))
			Ep0Stall(udc);
		break;

	case DATA_STATE_RECV:

		done(ep0, req, 0);
		/* send status phase */
		if (ep0_prime_status(udc, EP_DIR_IN))
			Ep0Stall(udc);
		break;

	case WAIT_FOR_OUT_STATUS:
		done(ep0, req, 0);
		udc->ep0_state = WAIT_FOR_SETUP;
		break;

	case WAIT_FOR_SETUP:
		VDBG("Unexpected interrupt");
		break;

	default:
		Ep0Stall(udc);
		break;
	}
}

static void tripwire_handler(struct mpc_udc *udc, u8 ep_num,
				u8 * buffer_ptr)
{
	u32 temp;
	struct ep_queue_head *qh;

	qh = &udc->ep_qh[ep_num*2 + EP_DIR_OUT];

	/* Clear bit in ENDPTSETUPSTAT */
	temp = cpu_to_le32(1 << ep_num);
	usb_slave_regs->endptsetupstat |= temp;

	/* while a hazard exists when setup package arrives */
	do {
		/* Set Setup Tripwire */
		temp = cpu_to_le32(USB_CMD_SUTW);
		usb_slave_regs->usbcmd |= temp;

		/* Copy the setup packet to local buffer */
		memcpy(buffer_ptr, (u8 *) qh->setup_buffer, 8);
	} while (!(le32_to_cpu(usb_slave_regs->usbcmd) & USB_CMD_SUTW));

	/* Clear Setup Tripwire */
	temp = le32_to_cpu(usb_slave_regs->usbcmd);
	temp &= ~USB_CMD_SUTW;
	usb_slave_regs->usbcmd = le32_to_cpu(temp);

}

/*process-ep_req(): free the completed Tds for this req */
/* FIXME: ERROR handling for multi-dtd requests */
static int process_ep_req(struct mpc_udc *udc, int pipe,
			struct mpc_req* curr_req)
{
	struct ep_td_struct *curr_td, *tmp_td;
	int	td_complete, actual, remaining_length, j, tmp;
	int	status = 0;
	int	errors = 0;
	struct  ep_queue_head *curr_qh = &udc->ep_qh[pipe];
	int direction = pipe % 2;

	curr_td = curr_req->head;
	td_complete = 0;
	actual = curr_req->req.length;

	for (j = 0; j < curr_req->dtd_count; j++) {
		remaining_length = ((le32_to_cpu(curr_td->size_ioc_sts)
				& DTD_PACKET_SIZE) >> DTD_LENGTH_BIT_POS);
		actual -= remaining_length;

		if ((errors = le32_to_cpu(curr_td->size_ioc_sts) &
						DTD_ERROR_MASK)) {
			if (errors & DTD_STATUS_HALTED) {
				printk("dTD error %08x \n", errors);
				/* Clear the errors and Halt condition */
				tmp = le32_to_cpu(curr_qh->size_ioc_int_sts);
				tmp &= ~errors;
				curr_qh->size_ioc_int_sts = cpu_to_le32(tmp);
				status = -EPIPE;
				/*FIXME clearing active bit, update
				 * nextTD pointer re-prime ep */

				break;
			}
			if (errors & DTD_STATUS_DATA_BUFF_ERR) {
				VDBG( "Transfer overflow");
				status = -EPROTO;
				break;
			}
			else if (errors & DTD_STATUS_TRANSACTION_ERR) {
				VDBG("ISO error");
				status = -EILSEQ;
				break;
			}
			else
				printk("Unknown error has occured (0x%x)!\r\n",
							errors);

		} else if (le32_to_cpu(curr_td->size_ioc_sts) &
							DTD_STATUS_ACTIVE) {
			VDBG("Request not wholly complete");
			status = REQ_UNCOMPLETE;
			return status;
		} else if (remaining_length)
			if (direction) {
				VDBG("Transmit dTD remaining length not zero");
				status = -EPROTO;
				break;
			} else {
				td_complete +=1;
				break;
			}
		else {
			td_complete += 1;
			VDBG("dTD transmitted successful ");
		}

		if (j != curr_req->dtd_count - 1)
			curr_td = (struct ep_td_struct*) phys_to_virt(
					le32_to_cpu(curr_td->next_td_ptr)
					& DTD_ADDR_MASK);
	}

	if (status)
		return status;

	curr_req->req.actual = actual;

	/* Free dtd for completed/error request */
	curr_td = curr_req->head;
	for (j = 0; j < curr_req->dtd_count; j++) {
		tmp_td = curr_td;
		if (j != curr_req->dtd_count - 1 )
			curr_td = (struct ep_td_struct*) phys_to_virt(
					le32_to_cpu(curr_td->next_td_ptr)
					& DTD_ADDR_MASK);
		kfree(tmp_td);
	}

	return status;

}

static void dtd_complete_irq(struct mpc_udc *udc)
{
	u32 bit_pos;
	int i, ep_num, direction, bit_mask, status;
	struct mpc_ep *curr_ep;
	struct mpc_req *curr_req, *temp_req;


	/* Clear the bits in the register */
	bit_pos = usb_slave_regs->endptcomplete;
	usb_slave_regs->endptcomplete = bit_pos;
	bit_pos = le32_to_cpu(bit_pos);

	if (!bit_pos)
		return;

	for (i = 0; i < USB_MAX_ENDPOINTS * 2; i++) {
		ep_num = i >> 1;
		direction = i % 2;

		bit_mask = 1 << (ep_num + 16 * direction);

		if (!(bit_pos & bit_mask))
			continue;

		curr_ep = get_ep_by_pipe(udc, i);

		/* If the ep is configured */
		if (curr_ep->name == NULL) {
			WARN("Invalid EP?");
			continue;
		}

		/* search all mpc_reqs of ep */
		list_for_each_entry_safe(curr_req, temp_req, &curr_ep->queue,
				    queue) {

			status = process_ep_req(udc, i, curr_req);
			if (status == REQ_UNCOMPLETE) {
				VDBG("Not all tds are completed in the req");
				break;
			}

			if (ep_num == 0) {
				ep0_req_complete(udc, curr_ep, curr_req);
				break;
			} else
				done(curr_ep, curr_req, status);

		}

	}
}

static void port_change_irq(struct mpc_udc *udc)
{
	u32 speed;

	if (udc->bus_reset)
		udc->bus_reset = FALSE;

	/* Bus resetting is finished */
	if (!(le32_to_cpu(usb_slave_regs->portsc1) & PORTSCX_PORT_RESET)) {
		/* Get the speed */
		speed = (le32_to_cpu(usb_slave_regs->portsc1) &
					PORTSCX_PORT_SPEED_MASK);
		switch (speed) {
		case PORTSCX_PORT_SPEED_HIGH:
			udc->gadget.speed = USB_SPEED_HIGH;
			break;
		case PORTSCX_PORT_SPEED_FULL:
			udc->gadget.speed = USB_SPEED_FULL;
			break;
		case PORTSCX_PORT_SPEED_LOW:
			udc->gadget.speed = USB_SPEED_LOW;
			break;
		default:
			udc->gadget.speed = USB_SPEED_UNKNOWN;
			break;
		}
	}

	/* Update USB state */
	if (!udc->resume_state)
		udc->usb_state = USB_STATE_DEFAULT;
}

static void suspend_irq(struct mpc_udc *udc)
{
	udc->resume_state = udc->usb_state;
	udc->usb_state = USB_STATE_SUSPENDED;

	/* report suspend to the driver ,serial.c not support this*/
	if (udc->driver->suspend)
		udc->driver->suspend(&udc->gadget);
}

static void resume_irq(struct mpc_udc *udc)
{
	udc->usb_state = udc->resume_state;
	udc->resume_state = 0;

	/* report resume to the driver , serial.c not support this*/
	if (udc->driver->resume)
		udc->driver->resume (&udc->gadget);

}

static int reset_queues(struct mpc_udc *udc)
{
	u8 pipe;

	for (pipe = 0; pipe < udc->max_pipes; pipe++)
		udc_reset_ep_queue(udc, pipe);

	/* report disconnect; the driver is already quiesced */
	udc->driver->disconnect(&udc->gadget);

	return 0;
}

/*
 *  Interrupt handler for USB reset received
 */

static void reset_irq(struct mpc_udc *udc)
{
	u32 temp;

	/* Clear the device address */
	temp = le32_to_cpu(usb_slave_regs->deviceaddr);
	temp &= ~USB_DEVICE_ADDRESS_MASK;
	usb_slave_regs->deviceaddr = cpu_to_le32(temp);
	udc->device_address = 0;

	/* Clear usb state */
	udc->usb_state = USB_STATE_DEFAULT;

	/* Clear all the setup token semaphores */
	temp = le32_to_cpu(usb_slave_regs->endptsetupstat);
	usb_slave_regs->endptsetupstat = cpu_to_le32(temp);

	/* Clear all the endpoint complete status bits */
	temp = le32_to_cpu(usb_slave_regs->endptcomplete);
	usb_slave_regs->endptcomplete = cpu_to_le32(temp);

	while (usb_slave_regs->endpointprime) {
		/* Wait until all endptprime bits cleared */
	}

	/* Write 1s to the Flush register */
	usb_slave_regs->endptflush = 0xFFFFFFFF;

	if (le32_to_cpu(usb_slave_regs->portsc1) & PORTSCX_PORT_RESET) {
		VDBG("Bus RESET");
		/* Bus is reseting */
		udc->bus_reset = TRUE;
		udc->ep0_state = WAIT_FOR_SETUP;
		udc->ep0_dir = 0;
		/* Reset all the queues, include XD, dTD, EP queue
		 * head and TR Queue */
		reset_queues(udc);
	} else {
		VDBG("Controller reset");
		/* initialize usb hw reg except for regs for EP, not
		 * touch usbintr reg */
		dr_controller_setup(udc->ep_qh);

		/* FIXME: Reset all internal used Queues */
		reset_queues(udc);

		ep0_dr_and_qh_setup(udc);

		/* Enable DR IRQ reg, Set Run bit, change udc state */
		dr_controller_run();
		udc->usb_state = USB_STATE_ATTACHED;
		udc->ep0_state =WAIT_FOR_SETUP;
		udc->ep0_dir = 0;
	}
}


/*
 * USB device controller interrupt handler
 */
static irqreturn_t mpc_udc_irq(int irq, void *_udc, struct pt_regs *r)
{
	struct mpc_udc *udc = _udc;
	u32 irq_src;
	irqreturn_t status = IRQ_NONE;
	unsigned long flags;

	spin_lock_irqsave(&udc->lock, flags);
	irq_src = usb_slave_regs->usbsts & usb_slave_regs->usbintr;
	/* Clear notification bits */
	usb_slave_regs->usbsts &= irq_src;

	irq_src = le32_to_cpu(irq_src);
	// VDBG("irq_src [0x%8x]", irq_src);

	/* USB Interrupt */
	if (irq_src & USB_STS_INT) {
		/* Setup package, we only support ep0 as control ep */
		if (usb_slave_regs->endptsetupstat & cpu_to_le32(
					EP_SETUP_STATUS_EP0)) {
			tripwire_handler(udc, 0,
					(u8 *) (&udc->local_setup_buff));
			setup_received_irq(udc, &udc->local_setup_buff);
			status = IRQ_HANDLED;
		}

		/* completion of dtd */
		if (usb_slave_regs->endptcomplete) {
			dtd_complete_irq(udc);
			status = IRQ_HANDLED;
		}
	}

	/* SOF (for ISO transfer) */
	if (irq_src & USB_STS_SOF) {
		status = IRQ_HANDLED;
	}

	/* Port Change */
	if (irq_src & USB_STS_PORT_CHANGE) {
		port_change_irq(udc);
		status = IRQ_HANDLED;
	}

	/* Reset Received */
	if (irq_src & USB_STS_RESET) {
		reset_irq(udc);
		status = IRQ_HANDLED;
	}

	/* Sleep Enable (Suspend) */
	if (irq_src & USB_STS_SUSPEND) {
		suspend_irq(udc);
		status = IRQ_HANDLED;
	} else if (udc->resume_state) {
		resume_irq(udc);
	}

	if (irq_src & (USB_STS_ERR | USB_STS_SYS_ERR)) {
		VDBG("Error IRQ %x ", irq_src);
	}

	spin_unlock_irqrestore(&udc->lock, flags);

	return status;
}


/*----------------------------------------------------------------*
 * tell the controller driver about gadget layer driver
 * The driver's bind function will be called to bind it to a gadget.
 * @driver: for example fsg_driver from file_storage.c
*----------------------------------------------------------------*/
int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	int retval = -ENODEV;
	unsigned long flags = 0;

	/* standard operations */
	if (!udc_controller)
		return -ENODEV;

	if (!driver || (driver->speed != USB_SPEED_FULL
			&& driver->speed != USB_SPEED_HIGH)
			|| !driver->bind || !driver->unbind ||
			!driver->disconnect || !driver->setup)
		return -EINVAL;

	if (udc_controller->driver)
		return -EBUSY;
	/* lock is needed but whether should use this lock or another */
	spin_lock_irqsave(&udc_controller->lock, flags);

	driver->driver.bus = 0;
	/* hook up the driver */
	udc_controller->driver = driver;
	udc_controller->gadget.dev.driver = &driver->driver;
	spin_unlock_irqrestore(&udc_controller->lock, flags);

	retval = driver->bind(&udc_controller->gadget);
	if (retval) {
		VDBG("bind to %s --> %d", driver->driver.name,
			retval);
		udc_controller->gadget.dev.driver = 0;
		udc_controller->driver = 0;
		goto out;
	}
	/* Enable DR IRQ reg and Set usbcmd reg  Run bit */
	dr_controller_run();
	udc_controller->usb_state = USB_STATE_ATTACHED;
	udc_controller->ep0_state =WAIT_FOR_SETUP;
	udc_controller->ep0_dir = 0;

	printk("mpc_udc: %s bind to driver %s \n", udc_controller->gadget.name,
		driver->driver.name);

out:
	return retval;


}

EXPORT_SYMBOL(usb_gadget_register_driver);

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct mpc_ep *loop_ep;
	unsigned long flags;

	if (!udc_controller)
		return -ENODEV;

	if (!driver || driver != udc_controller->driver)
		return -EINVAL;

	/* stop DR, disable intr */
	dr_controller_stop();

	/* in fact, no needed */
	udc_controller->usb_state = USB_STATE_ATTACHED;
	udc_controller->ep0_state =WAIT_FOR_SETUP;
	udc_controller->ep0_dir = 0;

	/* stand operation */
	spin_lock_irqsave(&udc_controller->lock, flags);
	udc_controller->gadget.speed = USB_SPEED_UNKNOWN;
	nuke(&udc_controller->eps[0], -ESHUTDOWN);
	list_for_each_entry(loop_ep, &udc_controller->gadget.ep_list,
				ep.ep_list)
		nuke(loop_ep, -ESHUTDOWN);
	spin_unlock_irqrestore(&udc_controller->lock, flags);

	/* unbind gadget and unhook driver. */
	driver->unbind(&udc_controller->gadget);
	udc_controller->gadget.dev.driver = 0;
	udc_controller->driver = 0;

	printk("unregistered gadget driver '%s'\r\n", driver->driver.name);
	return 0;
}

EXPORT_SYMBOL(usb_gadget_unregister_driver);


/*-------------------------------------------------------------------------
		PROC File System Support
-------------------------------------------------------------------------*/
#ifdef CONFIG_USB_GADGET_DEBUG_FILES

#include <linux/seq_file.h>

static const char proc_filename[] = "driver/mpc_udc";

static int mpc_proc_read(char *page, char **start, off_t off, int count,
			 int *eof, void *_dev)
{
	char *buf = page;
	char *next = buf;
	unsigned size = count;
	unsigned long flags;
	int t, i;
	u32 tmp_reg;
	struct mpc_ep *ep = NULL;
	struct mpc_req *req;

	struct mpc_udc *udc = udc_controller;
	if (off != 0)
		return 0;

	spin_lock_irqsave(&udc->lock, flags);

	/* ------basic driver infomation ---- */
	t = scnprintf(next, size,
			DRIVER_DESC "\n" "%s version: %s\n"
			"Gadget driver: %s\n\n", driver_name, DRIVER_VERSION,
			udc->driver ? udc->driver->
			driver.name : "(none)");
	size -= t;
	next += t;

	/* ------ DR Registers ----- */
	tmp_reg = le32_to_cpu(usb_slave_regs->usbcmd);
	t = scnprintf(next, size,
			"USBCMD reg:\n" "SetupTW: %d\n" "Run/Stop: %s\n\n",
			(tmp_reg & USB_CMD_SUTW) ? 1 : 0,
			(tmp_reg & USB_CMD_RUN_STOP) ? "Run" : "Stop");
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->usbsts);
	t = scnprintf(next, size,
			"USB Status Reg:\n" "Dr Suspend: %d"
			"Reset Received: %d" "System Error: %s"
			"USB Error Interrupt: %s\n\n",
			(tmp_reg & USB_STS_SUSPEND) ? 1 : 0,
			(tmp_reg & USB_STS_RESET) ? 1 : 0,
			(tmp_reg & USB_STS_SYS_ERR) ? "Err" : "Normal",
			(tmp_reg & USB_STS_ERR) ? "Err detected" : "No err");
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->usbintr);
	t = scnprintf(next, size,
			"USB Intrrupt Enable Reg:\n"
			"Sleep Enable: %d" "SOF Received Enable: %d"
			"Reset Enable: %d\n" "System Error Enable: %d"
			"Port Change Dectected Enable: %d\n"
			"USB Error Intr Enable: %d"
			"USB Intr Enable: %d\n\n",
			(tmp_reg & USB_INTR_DEVICE_SUSPEND) ? 1 : 0,
			(tmp_reg & USB_INTR_SOF_EN) ? 1 : 0,
			(tmp_reg & USB_INTR_RESET_EN) ? 1 : 0,
			(tmp_reg & USB_INTR_SYS_ERR_EN) ? 1 : 0,
			(tmp_reg & USB_INTR_PTC_DETECT_EN) ? 1 : 0,
			(tmp_reg & USB_INTR_ERR_INT_EN) ? 1 : 0,
			(tmp_reg & USB_INTR_INT_EN) ? 1 : 0);
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->frindex);
	t = scnprintf(next, size,
			"USB Frame Index Reg:" "Frame Number is 0x%x\n\n",
			(tmp_reg & USB_FRINDEX_MASKS));
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->deviceaddr);
	t = scnprintf(next, size,
			"USB Device Address Reg:" "Device Addr is 0x%x\n\n",
			(tmp_reg & USB_DEVICE_ADDRESS_MASK));
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->endpointlistaddr);
	t = scnprintf(next, size,
			"USB Endpoint List Address Reg:"
			"Device Addr is 0x%x\n\n",
			(tmp_reg & USB_EP_LIST_ADDRESS_MASK));
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->portsc1);
	t = scnprintf(next, size,
		"USB Port Status&Control Reg:\n"
		"Port Transceiver Type : %s" "Port Speed: %s \n"
		"PHY Low Power Suspend: %s" "Port Reset: %s"
		"Port Suspend Mode: %s \n" "Over-current Change: %s"
		"Port Enable/Disable Change: %s\n"
		"Port Enabled/Disabled: %s"
		"Current Connect Status: %s\n\n", ( {
			char *s;
			switch (tmp_reg & PORTSCX_PTS_FSLS) {
			case PORTSCX_PTS_UTMI:
				s = "UTMI"; break;
			case PORTSCX_PTS_ULPI:
				s = "ULPI "; break;
			case PORTSCX_PTS_FSLS:
				s = "FS/LS Serial"; break;
			default:
				s = "None"; break;
			}
			s;} ), ( {
			char *s;
			switch (tmp_reg & PORTSCX_PORT_SPEED_UNDEF) {
			case PORTSCX_PORT_SPEED_FULL:
				s = "Full Speed"; break;
			case PORTSCX_PORT_SPEED_LOW:
				s = "Low Speed"; break;
			case PORTSCX_PORT_SPEED_HIGH:
				s = "High Speed"; break;
			default:
				s = "Undefined"; break;
			}
			s;
		} ),
		(tmp_reg & PORTSCX_PHY_LOW_POWER_SPD) ?
		"Normal PHY mode" : "Low power mode",
		(tmp_reg & PORTSCX_PORT_RESET) ? "In Reset" :
		"Not in Reset",
		(tmp_reg & PORTSCX_PORT_SUSPEND) ? "In " : "Not in",
		(tmp_reg & PORTSCX_OVER_CURRENT_CHG) ? "Dected" :
		"No",
		(tmp_reg & PORTSCX_PORT_EN_DIS_CHANGE) ? "Disable" :
		"Not change",
		(tmp_reg & PORTSCX_PORT_ENABLE) ? "Enable" :
		"Not correct",
		(tmp_reg & PORTSCX_CURRENT_CONNECT_STATUS) ?
		"Attached" : "Not-Att");
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->usbmode);
	t = scnprintf(next, size,
		      "USB Mode Reg:" "Controller Mode is : %s\n\n", ( {
				char *s;
				switch (tmp_reg & USB_MODE_CTRL_MODE_HOST) {
				case USB_MODE_CTRL_MODE_IDLE:
					s = "Idle"; break;
				case USB_MODE_CTRL_MODE_DEVICE:
					s = "Device Controller"; break;
				case USB_MODE_CTRL_MODE_HOST:
					s = "Host Controller"; break;
				default:
					s = "None"; break;
				}
				s;
			} ));
	size -= t;
	next += t;

	tmp_reg = le32_to_cpu(usb_slave_regs->endptsetupstat);
	t = scnprintf(next, size,
			"Endpoint Setup Status Reg:" "SETUP on ep 0x%x\n\n",
			(tmp_reg & EP_SETUP_STATUS_MASK));
	size -= t;
	next += t;

	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		tmp_reg = le32_to_cpu(usb_slave_regs->endptctrl[i]);
		t = scnprintf(next, size, "EP Ctrl Reg [0x%x]: = [0x%x]\n",
				i, tmp_reg);
		size -= t;
		next += t;
	}
	tmp_reg = le32_to_cpu(usb_slave_regs->endpointprime);
	t = scnprintf(next, size, "EP Prime Reg = [0x%x]\n", tmp_reg);
	size -= t;
	next += t;

	tmp_reg = usb_sys_regs->snoop1;
	t = scnprintf(next, size, "\nSnoop1 Reg : = [0x%x]\n\n", tmp_reg);
	size -= t;
	next += t;

	tmp_reg = usb_sys_regs->control;
	t = scnprintf(next, size, "General Control Reg : = [0x%x]\n\n",
				tmp_reg);
	size -= t;
	next += t;

	/* ------mpc_udc, mpc_ep, mpc_request structure information ----- */
	ep = &udc->eps[0];
	t = scnprintf(next, size, "For %s Maxpkt is 0x%x index is 0x%x\n",
			ep->ep.name, ep_maxpacket(ep), ep_index(ep));
	size -= t;
	next += t;

	if (list_empty(&ep->queue)) {
		t = scnprintf(next, size, "its req queue is empty\n\n");
		size -= t;
		next += t;
	} else {
		list_for_each_entry(req, &ep->queue, queue) {
			t = scnprintf(next, size,
				"req %p actual 0x%x length 0x%x  buf %p\n",
				&req->req, req->req.actual,
				req->req.length, req->req.buf);
			size -= t;
			next += t;
		}
	}
	/* other gadget->eplist ep */
	list_for_each_entry(ep, &udc->gadget.ep_list,  ep.ep_list) {
		if (ep->desc) {
			t = scnprintf(next, size,
				"\nFor %s Maxpkt is 0x%x index is 0x%x\n",
				ep->ep.name,
				ep_maxpacket(ep),
				ep_index(ep));
			size -= t;
			next += t;

			if (list_empty(&ep->queue)) {
				t = scnprintf(next, size,
					      "its req queue is empty\n\n");
				size -= t;
				next += t;
			} else {
				list_for_each_entry(req, &ep->queue, queue) {
					t = scnprintf(next, size,
						"req %p actual 0x%x length"
						"0x%x  buf %p\n",
						&req->req, req->req.actual,
						req->req.length, req->req.buf);
					size -= t;
					next += t;
					}	// end for each_entry of ep req
				}	//end for else
			}	//end for if(ep->queue)
		}		// end (ep->desc)

	spin_unlock_irqrestore(&udc->lock, flags);

	*eof = 1;
	return count - size;
}

#define create_proc_file()	create_proc_read_entry(proc_filename, \
				0, NULL, mpc_proc_read, NULL)

#define remove_proc_file()	remove_proc_entry(proc_filename, NULL)

#else				/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define create_proc_file()	do {} while (0)
#define remove_proc_file()	do {} while (0)

#endif				/*CONFIG_USB_GADGET_DEBUG_FILES */


/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------
 * Release the mpc specific udc structure
 * it is not stand gadget function
 * it is called when the last reference to the device is removed;
 * it is called from the embedded kobject's release method.
 * All device structures registered with the core must have a
 * release method, or the kernel prints out scary complaints
 *-------------------------------------------------------------------------*/
static void mpc_udc_release(struct device *dev)
{
	complete(udc_controller->done);
	kfree(ep_qh_base);
	ep_qh_base = NULL;
	kfree(udc_controller);
	udc_controller = NULL;
}


/******************************************************************
	Internal Structure Build up functions -2
*******************************************************************/
/*------------------------------------------------------------------
 * this func will init resource for globle controller
 * Return the udc handle on success or Null on failing
 ------------------------------------------------------------------*/
static void *struct_udc_setup(struct platform_device *pdev)
{
	struct mpc_udc *udc = NULL;
	unsigned int tmp_sz = 0;

	udc = (struct mpc_udc *)kmalloc(sizeof(struct mpc_udc), GFP_KERNEL);
	if (udc == NULL) {
		printk("malloc udc failed\n");
		goto cleanup;
	}
	/* Zero out the internal USB state structure */
	memset(udc, 0, sizeof(struct mpc_udc));

	/* initialized QHs, take care the 2K align */
	tmp_sz = USB_MAX_PIPES * sizeof(struct ep_queue_head);
	udc->ep_qh = (struct ep_queue_head *) KMALLOC_ALIGN(tmp_sz, GFP_KERNEL,
						2 * 1024,
						(void **) &ep_qh_base);
	if (!udc->ep_qh) {
		printk("malloc QHs for udc failed\n");
		goto cleanup;
	}

	udc->resume_state = USB_STATE_NOTATTACHED;
	udc->usb_state = USB_STATE_POWERED;
	udc->ep0_dir = 0;
	/* initliaze the mpc_udc lock */
	spin_lock_init(&udc->lock);

	return udc;

cleanup:
	kfree(udc);
	return NULL;
}

/*----------------------------------------------------------------
 * set up the mpc_ep struct for eps
 * ep0out isnot used so do nothing here
 * ep0in should be taken care
 * It also link this mpc_ep->ep to gadget->ep_list
 *--------------------------------------------------------------*/
static int struct_ep_setup(void *handle, unsigned char pipe_num)
{
	struct mpc_udc *udc = (struct mpc_udc *) handle;
	struct mpc_ep *ep = get_ep_by_pipe(udc, pipe_num);

	ep->udc = udc;
	strcpy(ep->name, ep_name[pipe_num]);
	ep->ep.name = ep_name[pipe_num];
	ep->ep.ops = &mpc_ep_ops;
	ep->stopped = 0;

	/* for ep0: the desc defined here;
	 * for other eps, gadget layer called ep_enable with defined desc
	 */
	/* for ep0: maxP defined in desc
	 * for other eps, maxP is set by epautoconfig() called by gadget layer
	 */
	if (pipe_num == 0)
	{
		ep->desc = &mpc_ep0_desc;
		ep->ep.maxpacket = USB_MAX_CTRL_PAYLOAD;
	}
	else
	{
		ep->ep.maxpacket = (unsigned short) ~0;
		ep->desc = NULL;
	}

	//ep->last_io = XX_CurrentTime();
	/* the queue lists any req for this ep */
	INIT_LIST_HEAD(&ep->queue);

	/* mpc_ep->ep.ep_list: gadget ep_list hold all of its eps
	 * so only the first should init--it is ep0' */
#if 0
	if (pipe_num == 0)
		list_del_init(&udc->eps[0].ep.ep_list);
#endif
	/* gagdet.ep_list used for ep_autoconfig so no ep0*/
	if (pipe_num != 0)
		list_add_tail(&ep->ep.ep_list, &udc->gadget.ep_list);
	ep->gadget = &udc->gadget;

	return 0;
}

/* Initialize board specific registers */
static void board_init(void)
{
	unsigned long sccr;
	unsigned long sicrl, sicrh;
	volatile unsigned long *p;

/* MPC8349_ITX */
#ifndef CONFIG_MPC8349_ITX
	volatile unsigned char *bcsr_virt;
	volatile unsigned char *bcsr5_p;

	/* if SYS board is plug into PIB board, force to use the PHY on SYS board */
	bcsr_virt = (volatile unsigned char *)ioremap(BCSR_PHYS_ADDR, BCSR_SIZE);
	bcsr5_p = (volatile unsigned char *)( bcsr_virt + 0x00000005);
	if ( (*bcsr5_p & BCSR5_INT_USB) == 0 )
		*bcsr5_p = (*bcsr5_p | BCSR5_INT_USB);
	iounmap(bcsr_virt);
#endif

	p = (volatile unsigned long *)(GADGET_IMMR_BASE + 0x00A08); /* SCCR */
	sccr = *p;
	sccr &= ~SCCR_USB_11_CSB_CLK;
	sccr |= SCCR_USB_11_CSB_CLK;  /* USB CLK 1:3 CSB CLK */
	*p = sccr;

	/* SICRL[1,2] */
	p = (volatile unsigned long *)(GADGET_IMMR_BASE + 0x00114);
	sicrl = *p;
/* MPC8349_ITX */
#ifndef CONFIG_MPC8349_ITX
	*p = sicrl | SICRL_USB0 | SICRL_USB1;
#else
	sicrl &= ~(0x60000000);
	sicrl |= 0x40000000;
	*p = sicrl;
#endif

/* MPC8349_ITX */
#ifndef CONFIG_MPC8349_ITX
	/* SICRH[14] */
	p = (volatile unsigned long *)(GADGET_IMMR_BASE + 0x00118);
	sicrh = *p;
	*p = sicrh | SICRH_TSCE2_F;
#endif

}

/* Driver probe functions */

 /* all intialize operations implemented here except Enable usb_intr reg
  */
static int __init mpc_udc_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	/* FIXME: add platform_data */
	/* struct mpc_usb_config  *config = dev->platform_data; */
	unsigned int tmp_status = -ENODEV;
	unsigned int i;
	unsigned int DR_irq = 0;

	if (strcmp(pdev->name, "fsl-usb2-dr")) {
		VDBG("Wrong device\n");
		return -ENODEV;
	}

	/* board env setting should be OK before here
	 * including:
	 * Set up I2C if using USB port of PMC board
	 * Set SCCR for usb clock
	 * Set SICR for io pin according to 83xx RM table 16-1
	 * Set up PCA9555 according PHY interface type if usb port of PMC
	 *
	 * For us now, using usb port from SYS board and ULPI phy type.
	 * so no i2c setting, sicr[1:2] is 0b10, usb:csb=1:3
	 */
	board_init();

	/* Initialize the udc structure including QH member and other member */
	udc_controller = (struct mpc_udc *)struct_udc_setup(pdev);
	if (!udc_controller)
	{
		VDBG("udc_controll is NULL \n");
		return -ENOMEM;
	}

	/* Initialize the dr register map addr */
	if (pdev->resource[0].flags != IORESOURCE_MEM
			|| pdev->resource[1].flags != IORESOURCE_IRQ) {
		return -ENODEV;
	}

	if (!request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				driver_name)) {
		printk("request mem region for %s failed \n", pdev->name);
		return -EBUSY;
	}

	usb_slave_regs = ioremap(pdev->resource[0].start,
			sizeof(struct usb_dr_device));

	usb_sys_regs = (struct usb_sys_interface *) ((int)usb_slave_regs +
			USB_DR_SYS_OFFSET);

	/* initialize usb hw reg except for regs for EP,
	 * leave usbintr reg untouched*/
	dr_controller_setup(udc_controller->ep_qh);


	/* here comes the stand operations for probe
	 * set the mpc_udc->gadget.xxx
	 */
	udc_controller->gadget.ops = &mpc_gadget_ops;

	/* gadget.ep0 is a pointer */
	udc_controller->gadget.ep0 = &udc_controller->eps[0].ep;

	INIT_LIST_HEAD(&udc_controller->gadget.ep_list);

	udc_controller->gadget.speed = USB_SPEED_UNKNOWN;

	/* name: Identifies the controller hardware type. */
	udc_controller->gadget.name = driver_name;

	device_initialize(&udc_controller->gadget.dev);

	strcpy(udc_controller->gadget.dev.bus_id, "gadget");

	udc_controller->gadget.dev.release = mpc_udc_release;
	udc_controller->gadget.dev.parent = &pdev->dev;

	/* for an EP, the intialization includes: fields in QH, Regs,
	 * mpc_ep struct */
	ep0_dr_and_qh_setup(udc_controller);

	for (i = 0; i < USB_MAX_PIPES ; i++) {
		/*because the ep type isnot decide here so
		 * struct_ep_qh_setup() and dr_ep_setup()
		 * should be called in ep_enable()
		 */

		if ( ep_name[i] != NULL )
			/* setup the mpc_ep struct and link ep.ep.list
			 * into gadget.ep_list */
			struct_ep_setup(udc_controller, i);
	}

	/* request irq and disable DR  */
	DR_irq = pdev->resource[1].start;

	tmp_status = request_irq(DR_irq, mpc_udc_irq, 0,
			driver_name,udc_controller);

	if (tmp_status != 0) {
		printk("cannot request irq %d err %d \n", DR_irq,
		       tmp_status);
		return tmp_status;
	}

	create_proc_file();

	device_add(&udc_controller->gadget.dev);

	return 0;

}

/* Driver removal functions
 * Free resources
 * Finish pending transaction
 */
static int __exit mpc_udc_remove(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	DECLARE_COMPLETION(done);

	if (!udc_controller)
		return -ENODEV;
	udc_controller->done = &done;

	/* DR has been stopped in usb_gadget_unregister_driver() */

	/* remove proc */
	remove_proc_file();

	/* free irq */
	free_irq(pdev->resource[1].start, udc_controller);

	/* deinitlaize all ep: strcut */
	/* deinitialize ep0: reg and QH */

	/* free region */
	release_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1);

	device_unregister(&udc_controller->gadget.dev);
	/* free udc --wait for the release() finished */
	wait_for_completion(&done);

	return 0;
}

/*-----------------------------------------------------------------
 * Modify Power management attributes
 * Here we stop the DR controller and disable the irq
 -----------------------------------------------------------------*/
static int mpc_udc_suspend(struct device *dev, u32 state, u32 level)
{
	dr_controller_stop();
	return 0;
}

/*-----------------------------------------------------------------
 * Invoked on USB resume. May be called in_interrupt.
 * Here we start the DR controller and enable the irq
 *-----------------------------------------------------------------*/
static int mpc_udc_resume(struct device *dev, u32 level)
{
	/*Enable DR irq reg and set controller Run */
	dr_controller_run();
	return 0;
}

/*-------------------------------------------------------------------------
	Register entry point for the peripheral controller driver
--------------------------------------------------------------------------*/

static struct device_driver udc_driver = {
	.name = (char *) driver_name,
	.bus = &platform_bus_type,
	.probe = mpc_udc_probe,
	.remove = __exit_p(mpc_udc_remove),//only implement with MODULE defined

	/* these suspend and resume are not related to usb suspend and resume */
	.suspend = mpc_udc_suspend,
	.resume = mpc_udc_resume,
};


static int __init udc_init(void)
{
	printk("%s version %s init \n", driver_desc, DRIVER_VERSION);
	return driver_register(&udc_driver);
}

module_init(udc_init);

static void __exit udc_exit(void)
{
	driver_unregister(&udc_driver);
}

module_exit(udc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
