/****************************************************************************
 *
 * rg/pkg/ppp/kernel/be_pppoa.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 */

/* net/atm/pppoatm.c - RFC2364 PPP over ATM/AAL5 */

/* Copyright 1999-2000 by Mitchell Blank Jr */
/* Based on clip.c; 1995-1999 by Werner Almesberger, EPFL LRC/ICA */
/* And on ppp_async.c; Copyright 1999 Paul Mackerras */
/* And help from Jens Axboe */

/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 * This driver provides the encapsulation and framing for sending
 * and receiving PPP frames in ATM AAL5 PDUs.
 */

/*
 * One shortcoming of this driver is that it does not comply with
 * section 8 of RFC2364 - we are supposed to detect a change
 * in encapsulation and immediately abort the connection (in order
 * to avoid a black-hole being created if our peer loses state
 * and changes encapsulation unilaterally.  However, since the
 * ppp_generic layer actually does the decapsulation, we need
 * a way of notifying it when we _think_ there might be a problem)
 * There's two cases:
 *   1.	LLC-encapsulation was missing when it was enabled.  In
 *	this case, we should tell the upper layer "tear down
 *	this session if this skb looks ok to you"
 *   2.	LLC-encapsulation was present when it was disabled.  Then
 *	we need to tell the upper layer "this packet may be
 *	ok, but if its in error tear down the session"
 * These hooks are not yet available in ppp_generic
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmppp.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include <be_api_gpl.h>

#define SC_COMP_PROT	0x00000001	/* protocol compression (output) */

#if 0
#define DPRINTK(format, args...) \
	printk(KERN_DEBUG "pppoatm: " format, ##args)
#else
#define DPRINTK(format, args...)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define MOD_DEC_USE_COUNT module_put(THIS_MODULE)
#define MOD_INC_USE_COUNT __module_get(THIS_MODULE);
#endif

enum pppoatm_encaps {
	e_autodetect = PPPOATM_ENCAPS_AUTODETECT,
	e_vc = PPPOATM_ENCAPS_VC,
	e_llc = PPPOATM_ENCAPS_LLC,
	e_vc_hdlc = PPPOATM_ENCAPS_VC_HDLC,
};

struct pppoatm_vcc {
	struct atm_vcc	*atmvcc;	/* VCC descriptor */
	void (*old_push)(struct atm_vcc *, struct sk_buff *);
	void (*old_pop)(struct atm_vcc *, struct sk_buff *);
					/* keep old push/pop for detaching */
	enum pppoatm_encaps encaps;
	int flags;			/* SC_COMP_PROT - compress protocol */
	struct ppp_softc *ppp_sc;	/* interface to generic ppp layer */
	struct tasklet_struct wakeup_tasklet;
	struct sk_buff *delayed_lcp; /* One packet queue in case the LCP 
				      * packet arrives before the BE was 
				      * attached */
};


static ppp_input_cb_t ppp_input_cb;
static ppp_be_attach_cb_t ppp_be_attach_cb;
static ppp_be_detach_cb_t ppp_be_detach_cb;

#define DEVNAME(pvcc) knet_netdev_name_get(&(pvcc)->ppp_sc->sc_if)

/*
 * Header used for LLC Encapsulated PPP (4 bytes) followed by the LCP protocol
 * ID (0xC021) used in autodetection
 */
static const unsigned char pppllc[6] = { 0xFE, 0xFE, 0x03, 0xCF, 0xC0, 0x21 };
#define LLC_LEN		(4)
static const unsigned char hdlc[2] = { 0xFF, 0x03 };
#define HDLC_LEN	(2)

static int use_hdlc;
static inline struct pppoatm_vcc *atmvcc_to_pvcc(const struct atm_vcc *atmvcc)
{
	return (struct pppoatm_vcc *) (atmvcc->user_back);
}

/*
 * We can't do this directly from our _pop handler, since the ppp code
 * doesn't want to be called in interrupt context, so we do it from
 * a tasklet
 */
static void pppoatm_wakeup_sender(unsigned long arg)
{
	/* XXX rg_ppp_output_wakeup((struct ppp_channel *) arg); */
}

/*
 * This gets called every time the ATM card has finished sending our
 * skb.  The ->old_pop will take care up normal atm flow control,
 * but we also need to wake up the device if we blocked it
 */
static void pppoatm_pop(struct atm_vcc *atmvcc, struct sk_buff *skb)
{
	struct pppoatm_vcc *pvcc = atmvcc_to_pvcc(atmvcc);
	pvcc->old_pop(atmvcc, skb);
	/*
	 * We don't really always want to do this since it's
	 * really inefficient - it would be much better if we could
	 * test if we had actually throttled the generic layer.
	 * Unfortunately then there would be a nasty SMP race where
	 * we could clear that flag just as we refuse another packet.
	 * For now we do the safe thing.
	 */
	tasklet_schedule(&pvcc->wakeup_tasklet);
}

/*
 * Unbind from PPP - currently we only do this when closing the socket,
 * but we could put this into an ioctl if need be
 */
static void pppoatm_unassign_vcc(struct atm_vcc *atmvcc)
{
	struct pppoatm_vcc *pvcc;
	pvcc = atmvcc_to_pvcc(atmvcc);
	atmvcc->push = pvcc->old_push;
	atmvcc->pop = pvcc->old_pop;
	tasklet_disable(&pvcc->wakeup_tasklet);
	atmvcc->user_back = NULL;
	if (pvcc->delayed_lcp)
	{
	    dev_kfree_skb_any(pvcc->delayed_lcp);
	    pvcc->delayed_lcp = NULL;
	}
	kfree(pvcc);
	/* Gee, I hope we have the big kernel lock here... */
	MOD_DEC_USE_COUNT;
}

/* Called when an AAL5 PDU comes in */
static void pppoatm_push(struct atm_vcc *atmvcc, struct sk_buff *skb)
{
	struct pppoatm_vcc *pvcc = atmvcc_to_pvcc(atmvcc);
	DPRINTK("pppoatm push\n");
	if (skb == NULL) {			/* VCC was closed */
		DPRINTK("removing ATMPPP VCC %p\n", pvcc);
		pppoatm_unassign_vcc(atmvcc);
		atmvcc->push(atmvcc, NULL);	/* Pass along bad news */
		return;
	}
	atm_return(atmvcc, skb->truesize);
	switch (pvcc->encaps) {
	case e_llc:
		if (skb->len >= LLC_LEN &&
		    !memcmp(skb->data, pppllc, LLC_LEN))
			skb_pull(skb, LLC_LEN);
		break;
	case e_autodetect:
#if 0 /* We don't support autodetect right now */
		if (pvcc->ppp_sc == NULL) {	/* Not bound yet! */
			kfree_skb(skb);
			return;
		}
		if (skb->len >= sizeof(pppllc) &&
		    !memcmp(skb->data, pppllc, sizeof(pppllc))) {
			pvcc->encaps = e_llc;
			skb_pull(skb, LLC_LEN);
			break;
		}
		if (skb->len >= (sizeof(pppllc) - LLC_LEN) &&
		    !memcmp(skb->data, &pppllc[LLC_LEN],
		    sizeof(pppllc) - LLC_LEN)) {
			pvcc->encaps = e_vc;
			pvcc->chan.mtu = LLC_LEN;
			break;
		}
		DPRINTK("%s: Couldn't autodetect yet "
		    "(skb: %02X %02X %02X %02X %02X %02X)\n",
		    DEVNAME(pvcc),
		    skb->data[0], skb->data[1], skb->data[2],
		    skb->data[3], skb->data[4], skb->data[5]);
#endif
		goto error;
	case e_vc:
		if (skb->len >= HDLC_LEN &&
		    !memcmp(skb->data, hdlc, HDLC_LEN))
			skb_pull(skb, HDLC_LEN);
		break;
	case e_vc_hdlc:
		break;
	}
	/* We are not attached yet */
	if (!pvcc->ppp_sc)
	{
	    if (!pvcc->delayed_lcp)
	    {
		pvcc->delayed_lcp = skb;
		return;
	    }
	    DPRINTK("pppoa: drop PPP packet on unattached backend\n");
	    goto error;
	}
	ppp_input_cb(pvcc->ppp_sc, skb);
	return;
    error:
	kfree_skb(skb);
	/* XXX rg_ppp_input_error(&pvcc->ppp_sc, 0); */
}

/*
 * Called by the if_ppp.c to send a packet - returns true if packet
 * was accepted.  If we return false, then it's our job to call
 * rg_ppp_output_wakeup() when we're feeling more up to it.
 * Note that in the ENOMEM case (as opposed to the !atm_may_send case)
 * we should really drop the packet, but the generic layer doesn't
 * support this yet.
 */
static ppp_send_res_t rg_pppoa_output(void *o, struct sk_buff **skb)
{
	struct pppoatm_vcc *pvcc = o;
	ATM_SKB(*skb)->vcc = pvcc->atmvcc;
	DPRINTK("%s: rg_pppoa_output (skb=0x%p, vcc=0x%p)\n",
	    DEVNAME(pvcc), *skb, pvcc->atmvcc);
	if ((*skb)->data[0] == '\0' && (pvcc->flags & SC_COMP_PROT))
		(void) skb_pull(*skb, 1);
	switch (pvcc->encaps) {		/* LLC encapsulation needed */
	case e_llc:
		if (skb_headroom(*skb) < LLC_LEN) {
			struct sk_buff *n;
			n = skb_realloc_headroom(*skb, LLC_LEN);
			if (n != NULL &&
			    !atm_may_send(pvcc->atmvcc, n->truesize)) {
				kfree_skb(n);
				goto nospace;
			}
			kfree_skb(*skb);
			if ((*skb = n) == NULL)
				return PPP_PACKET_DROPPED;
		} else if (!atm_may_send(pvcc->atmvcc, (*skb)->truesize))
			goto nospace;
		memcpy(skb_push(*skb, LLC_LEN), pppllc, LLC_LEN);
		break;
	case e_vc:
		if (use_hdlc)
		{
		    if (skb_headroom(*skb) < HDLC_LEN) {
			struct sk_buff *n;
			n = skb_realloc_headroom(*skb, HDLC_LEN);
			if (n != NULL &&
			    !atm_may_send(pvcc->atmvcc, n->truesize)) {
				kfree_skb(n);
				goto nospace;
			}
			kfree_skb(*skb);
			if ((*skb = n) == NULL)
			    return PPP_PACKET_DROPPED;
		    } else if (!atm_may_send(pvcc->atmvcc, (*skb)->truesize))
			    goto nospace;
		    memcpy(skb_push(*skb, HDLC_LEN), hdlc, HDLC_LEN);
		}
		else
		    if (!atm_may_send(pvcc->atmvcc, (*skb)->truesize))
			goto nospace;
		break;
	case e_autodetect:
		DPRINTK("%s: Trying to send without setting encaps!\n", DEVNAME(pvcc));
		kfree_skb(*skb);
		return PPP_PACKET_DROPPED;
	case e_vc_hdlc:
		break;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	atomic_add((*skb)->truesize, &ATM_SKB(*skb)->vcc->sk->wmem_alloc);
	ATM_SKB(*skb)->iovcnt = 0;
#else
	atomic_add((*skb)->truesize, &sk_atm(ATM_SKB(*skb)->vcc)->sk_wmem_alloc);
#endif
	ATM_SKB(*skb)->atm_options = ATM_SKB(*skb)->vcc->atm_options;
	DPRINTK("%s: atm_skb(%p)->vcc(%p)->dev(%p)\n",
	    DEVNAME(pvcc), *skb, ATM_SKB(*skb)->vcc,
	    ATM_SKB(*skb)->vcc->dev);
	return ATM_SKB(*skb)->vcc->send(ATM_SKB(*skb)->vcc, *skb)
	    ? PPP_PACKET_DROPPED : PPP_PACKET_SENT;
    nospace:
	/*
	 * We don't have space to send this SKB now, but we might have
	 * already applied SC_COMP_PROT compression, so may need to undo
	 */
	if ((pvcc->flags & SC_COMP_PROT) && skb_headroom(*skb) > 0 &&
	    (*skb)->data[-1] == '\0')
		(void) skb_push(*skb, 1);
	return PPP_RETRY_LATER;
}

static int pppoatm_assign_vcc(struct atm_vcc *atmvcc, unsigned long arg)
{
	struct atm_backend_ppp be;
	struct pppoatm_vcc *pvcc;
	/*
	 * Each PPPoATM instance has its own tasklet - this is just a
	 * prototypical one used to initialize them
	 */
	static const DECLARE_TASKLET(tasklet_proto, pppoatm_wakeup_sender, 0);
	if (copy_from_user(&be, (void *) arg, sizeof be))
		return -EFAULT;
	if (be.encaps != PPPOATM_ENCAPS_AUTODETECT &&
	    be.encaps != PPPOATM_ENCAPS_VC && be.encaps != PPPOATM_ENCAPS_LLC &&
	    be.encaps != PPPOATM_ENCAPS_VC_HDLC)
		return -EINVAL;
	MOD_INC_USE_COUNT;
	pvcc = kmalloc(sizeof(*pvcc), GFP_KERNEL);
	if (pvcc == NULL) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(pvcc, 0, sizeof(*pvcc));
	if (be.encaps == PPPOATM_ENCAPS_VC_HDLC)
	{
	    printk("PPPOATM: Using HDLC Encapsulation\n");
	    be.encaps = PPPOATM_ENCAPS_VC;
	    use_hdlc = 1;
	}
	else
	{
	    printk("PPPOATM: Not using HDLC Encapsulation\n");
	    use_hdlc = 0;
	}
	pvcc->atmvcc = atmvcc;
	pvcc->old_push = atmvcc->push;
	pvcc->old_pop = atmvcc->pop;
	pvcc->encaps = (enum pppoatm_encaps) be.encaps;
	pvcc->wakeup_tasklet = tasklet_proto;
	pvcc->wakeup_tasklet.data = (unsigned long) pvcc->ppp_sc;
	atmvcc->user_back = pvcc;
	atmvcc->push = pppoatm_push;
	atmvcc->pop = pppoatm_pop;
	return 0;
}

static int rg_pppoa_attach(struct atm_vcc *atmvcc, char *if_name)
{
	struct pppoatm_vcc *pvcc = atmvcc_to_pvcc(atmvcc);
	pvcc->ppp_sc = ppp_be_attach_cb(if_name, 0, 0, rg_pppoa_output, pvcc);
	if (pvcc->ppp_sc && pvcc->delayed_lcp)
	{
	    ppp_input_cb(pvcc->ppp_sc, pvcc->delayed_lcp);
	    pvcc->delayed_lcp = NULL;
	}
	return pvcc->ppp_sc ? 0 : -EFAULT;
}

static int rg_pppoa_detach(struct atm_vcc *atmvcc)
{
	struct pppoatm_vcc *pvcc = atmvcc_to_pvcc(atmvcc);
	ppp_be_detach_cb(pvcc->ppp_sc);
	pvcc->ppp_sc = NULL;
	return 0;
}

/*
 * This handles ioctls actually performed on our vcc - we must return
 * -ENOIOCTLCMD for any unrecognized ioctl
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static int pppoatm_ioctl(struct atm_vcc *atmvcc, unsigned int cmd,
	unsigned long arg)
{
#else
static int pppoatm_ioctl(struct socket *sock, unsigned int cmd,
	unsigned long arg)
{
	struct atm_vcc *atmvcc = ATM_SD(sock);

#endif
	if (cmd != ATM_SETBACKEND && atmvcc->push != pppoatm_push)
		return -ENOIOCTLCMD;
	switch (cmd) {
	case ATM_SETBACKEND: {
		atm_backend_t b;
		if (get_user(b, (atm_backend_t *) arg))
			return -EFAULT;
		if (b != ATM_BACKEND_PPP)
			return -ENOIOCTLCMD;
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return pppoatm_assign_vcc(atmvcc, arg);
		}
	case PPPBE_ATTACH: {
		char if_name[IFNAMSIZ];
		if (copy_from_user(if_name, (void *)arg, IFNAMSIZ))
			return -EFAULT;
		return rg_pppoa_attach(atmvcc, if_name);
		}
	case PPPBE_DETACH:
		return rg_pppoa_detach(atmvcc);
	}
	return -ENOIOCTLCMD;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
extern int (*pppoatm_ioctl_hook)(struct atm_vcc *,
	 unsigned int, unsigned long);
#else
static struct atm_ioctl pppoatm_ioctl_ops = {
	.owner	= THIS_MODULE,
	.ioctl	= pppoatm_ioctl,
};
#endif

int pppoatm_start(ppp_input_cb_t p1, ppp_be_attach_cb_t p2, ppp_be_detach_cb_t p3)
{
    ppp_input_cb = p1;
    ppp_be_attach_cb = p2;
    ppp_be_detach_cb = p3;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	pppoatm_ioctl_hook = pppoatm_ioctl;
#else
	register_atm_ioctl(&pppoatm_ioctl_ops);
#endif
	return 0;
}
EXPORT_SYMBOL(pppoatm_start);

void pppoatm_stop(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	pppoatm_ioctl_hook = NULL;
#else
	deregister_atm_ioctl(&pppoatm_ioctl_ops);
#endif
}
EXPORT_SYMBOL(pppoatm_stop);

static int __init pppoatm_init(void)
{
	return 0;
}

static void __exit pppoatm_exit(void)
{
}

module_init(pppoatm_init);
module_exit(pppoatm_exit);

MODULE_AUTHOR("Mitchell Blank Jr <mitch@sfgoth.com>");
MODULE_DESCRIPTION("RFC2364 PPP over ATM/AAL5");
