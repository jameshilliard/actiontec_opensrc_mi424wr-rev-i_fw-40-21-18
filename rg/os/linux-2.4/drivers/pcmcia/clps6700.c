/*
 *  linux/drivers/pcmcia/clps6700.c
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/page.h>

#include <asm/arch/syspld.h>
#include <asm/hardware/clps7111.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>

#include "clps6700.h"

#define DEBUG

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("CL-PS6700 PCMCIA socket driver");

#define NR_CLPS6700	2

struct clps6700_skt {
	u_int			nr;
	u_int			physbase;
	u_int			regbase;
	u_int			pmr;
	u_int			cpcr;
	u_int			cpcr_3v3;
	u_int			cpcr_5v0;
	u_int			cur_pmr;
	u_int			cur_cicr;
	u_int			cur_pcimr;
	u_int			cur_cpcr;
	void			(*handler)(void *, u_int);
	void			*handler_info;

	u_int			ev_pending;
	spinlock_t		ev_lock;
};

static struct clps6700_skt *skts[NR_CLPS6700];

static int clps6700_sock_init(u_int sock)
{
	struct clps6700_skt *skt = skts[sock];

	skt->cur_cicr  = 0;
	skt->cur_pmr   = skt->pmr;
	skt->cur_pcimr = 0;
	skt->cur_cpcr  = skt->cpcr;

#ifdef DEBUG
	printk("skt%d: sock_init()\n", sock);
#endif

	__raw_writel(skt->cur_pmr,	skt->regbase + PMR);
	__raw_writel(skt->cur_cpcr,	skt->regbase + CPCR);
	__raw_writel(0x01f8,		skt->regbase + SICR);
	__raw_writel(0x0000,		skt->regbase + DMACR);
	__raw_writel(skt->cur_cicr,	skt->regbase + CICR);
	__raw_writel(0x1f00,		skt->regbase + CITR0A);
	__raw_writel(0x0000,		skt->regbase + CITR0B);
	__raw_writel(0x1f00,		skt->regbase + CITR1A);
	__raw_writel(0x0000,		skt->regbase + CITR1B);
	__raw_writel(skt->cur_pcimr,	skt->regbase + PCIMR);

	/*
	 * Enable Auto Idle Mode in PM register
	 */
	__raw_writel(-1,		skt->regbase + PCIRR1);
	__raw_writel(-1,		skt->regbase + PCIRR2);
	__raw_writel(-1,		skt->regbase + PCIRR3);

	return 0;
}

static int clps6700_suspend(u_int sock)
{
	return 0;
}

static int clps6700_register_callback(u_int sock, void (*handler)(void *, u_int), void *info)
{
	struct clps6700_skt *skt = skts[sock];

#ifdef DEBUG
	printk("skt%d: register_callback: %p (%p)\n", sock, handler, info);
#endif

	skt->handler_info = info;
	skt->handler = handler;

	return 0;
}

static int clps6700_inquire_socket(u_int sock, socket_cap_t *cap)
{
	cap->features = SS_CAP_PCCARD | SS_CAP_STATIC_MAP | SS_CAP_MEM_ALIGN;
	cap->irq_mask = 0;		/* available IRQs for this socket */
	cap->map_size = PAGE_SIZE;	/* minimum mapping size */
	cap->pci_irq  = 0;		/* PCI interrupt number */
	cap->cb_dev   = NULL;
	cap->bus      = NULL;
	return 0;
}

static int __clps6700_get_status(struct clps6700_skt *skt)
{
	unsigned int v, val;

	v = __raw_readl(skt->regbase + PCIILR);
	val = 0;
	if ((v & (PCM_CD1 | PCM_CD2)) == 0)
		val |= SS_DETECT;
	if ((v & (PCM_BVD2 | PCM_BVD1)) == PCM_BVD1)
		val |= SS_BATWARN;
	if ((v & PCM_BVD2) == 0)
		val |= SS_BATDEAD;

	if (v & PCM_RDYL)
		val |= SS_READY;
	if (v & PCM_VS1)
		val |= SS_3VCARD;
	if (v & PCM_VS2)
		val |= SS_XVCARD;

#ifdef DEBUG
	printk("skt%d: PCIILR: %08x -> (%s %s %s %s %s %s)\n",
		skt->nr, v,
		val & SS_READY   ? "rdy" : "---",
		val & SS_DETECT  ? "det" : "---",
		val & SS_BATWARN ? "bw"  : "--",
		val & SS_BATDEAD ? "bd"  : "--",
		val & SS_3VCARD  ? "3v"  : "--",
		val & SS_XVCARD  ? "xv"  : "--");
#endif
	return val;
}

static int clps6700_get_status(u_int sock, u_int *valp)
{
	struct clps6700_skt *skt = skts[sock];

	*valp = __clps6700_get_status(skt);

	return 0; /* not used! */
}

static int clps6700_get_socket(u_int sock, socket_state_t *state)
{
	return -EINVAL;
}

static int clps6700_set_socket(u_int sock, socket_state_t *state)
{
	struct clps6700_skt *skt = skts[sock];
	unsigned long flags;
	u_int cpcr = skt->cur_cpcr, pmr = skt->cur_pmr, cicr = skt->cur_cicr;
	u_int pcimr = 0;

	cicr &= ~(CICR_ENABLE | CICR_RESET | CICR_IOMODE);

	if (state->flags & SS_PWR_AUTO)
		pmr |= PMR_DCAR | PMR_PDCR;

	/*
	 * Note! We must NOT assert the Card Enable bit until reset has
	 * been de-asserted.  Some cards indicate not ready, which then
	 * hangs our next access.  (Bug in CLPS6700?)
	 */
	if (state->flags & SS_RESET)
		cicr |= CICR_RESET | CICR_RESETOE;
	else if (state->flags & SS_OUTPUT_ENA)
		cicr |= CICR_ENABLE;

	if (state->flags & SS_IOCARD) {
		cicr |= CICR_IOMODE;

/*		if (state->csc_mask & SS_STSCHG)*/
	} else {
		if (state->csc_mask & SS_BATDEAD)
			pcimr |= PCM_BVD2;
		if (state->csc_mask & SS_BATWARN)
			pcimr |= PCM_BVD1;
		if (state->csc_mask & SS_READY)
			pcimr |= PCM_RDYL;
	}

	if (state->csc_mask & SS_DETECT)
		pcimr |= PCM_CD1 | PCM_CD2;

	switch (state->Vcc) {
	case 0:				break;
	case 33: cpcr |= skt->cpcr_3v3; pmr |= PMR_CPE; break;
	case 50: cpcr |= skt->cpcr_5v0; pmr |= PMR_CPE; break;
	default: return -EINVAL;
	}

#ifdef DEBUG
	printk("skt%d: PMR: %04x, CPCR: %04x, CICR: %04x PCIMR: %04x "
		"(Vcc = %d, flags = %c%c%c%c, csc = %c%c%c%c%c)\n",
		sock, pmr, cpcr, cicr, pcimr, state->Vcc,
		state->flags & SS_RESET      ? 'r' : '-',
		state->flags & SS_PWR_AUTO   ? 'p' : '-',
		state->flags & SS_IOCARD     ? 'i' : '-',
		state->flags & SS_OUTPUT_ENA ? 'o' : '-',
		state->csc_mask & SS_STSCHG  ? 's' : '-',
		state->csc_mask & SS_BATDEAD ? 'd' : '-',
		state->csc_mask & SS_BATWARN ? 'w' : '-',
		state->csc_mask & SS_READY   ? 'r' : '-',
		state->csc_mask & SS_DETECT  ? 'c' : '-');
#endif

	save_flags_cli(flags);

	if (skt->cur_cpcr != cpcr) {
		skt->cur_cpcr = cpcr;
		__raw_writel(skt->cur_cpcr, skt->regbase + CPCR);
	}

	if (skt->cur_pmr != pmr) {
		skt->cur_pmr = pmr;
		__raw_writel(skt->cur_pmr, skt->regbase + PMR);
	}
	if (skt->cur_pcimr != pcimr) {
		skt->cur_pcimr = pcimr;
		__raw_writel(skt->cur_pcimr, skt->regbase + PCIMR);
	}
	if (skt->cur_cicr != cicr) {
		skt->cur_cicr = cicr;
		__raw_writel(skt->cur_cicr, skt->regbase + CICR);
	}

	restore_flags(flags);

	return 0;
}

static int clps6700_get_io_map(u_int sock, struct pccard_io_map *io)
{
	return -EINVAL;
}

static int clps6700_set_io_map(u_int sock, struct pccard_io_map *io)
{
	printk("skt%d: iomap: %d: speed %d, flags %X start %X stop %X\n",
		sock, io->map, io->speed, io->flags, io->start, io->stop);
	return 0;
}

static int clps6700_get_mem_map(u_int sock, struct pccard_mem_map *mem)
{
	return -EINVAL;
}

/*
 * Set the memory map attributes for this socket.  (ie, mem->speed)
 * Note that since we have SS_CAP_STATIC_MAP set, we don't need to do
 * any mapping here at all; we just need to return the address (suitable
 * for ioremap) to map the requested space in mem->sys_start.
 *
 * flags & MAP_ATTRIB indicates whether we want attribute space.
 */
static int clps6700_set_mem_map(u_int sock, struct pccard_mem_map *mem)
{
	struct clps6700_skt *skt = skts[sock];
	u_int off;

	printk("skt%d: memmap: %d: speed %d, flags %X start %lX stop %lX card %X\n",
		sock, mem->map, mem->speed, mem->flags, mem->sys_start,
		mem->sys_stop, mem->card_start);

	if (mem->flags & MAP_ATTRIB)
		off = CLPS6700_ATTRIB_BASE;
	else
		off = CLPS6700_MEM_BASE;

	mem->sys_start  = skt->physbase + off;
	mem->sys_start += mem->card_start;

	return 0;
}

static void clps6700_proc_setup(u_int sock, struct proc_dir_entry *base)
{
}

static struct pccard_operations clps6700_operations = {
	init:			clps6700_sock_init,
	suspend:		clps6700_suspend,
	register_callback:	clps6700_register_callback,
	inquire_socket:		clps6700_inquire_socket,
	get_status:		clps6700_get_status,
	get_socket:		clps6700_get_socket,
	set_socket:		clps6700_set_socket,
	get_io_map:		clps6700_get_io_map,
	set_io_map:		clps6700_set_io_map,
	get_mem_map:		clps6700_get_mem_map,
	set_mem_map:		clps6700_set_mem_map,
	proc_setup:		clps6700_proc_setup
};

static void clps6700_bh(void *dummy)
{
	int i;

	for (i = 0; i < NR_CLPS6700; i++) {
		struct clps6700_skt *skt = skts[i];
		unsigned long flags;
		u_int events;

		if (!skt)
			continue;

		/*
		 * Note!  We must read the pending event state
		 * with interrupts disabled, otherwise we race
		 * with our own interrupt routine!
		 */
		spin_lock_irqsave(&skt->ev_lock, flags);
		events = skt->ev_pending;
		skt->ev_pending = 0;
		spin_unlock_irqrestore(&skt->ev_lock, flags);	

		if (skt->handler && events)
			skt->handler(skt->handler_info, events);
	}
}

static struct tq_struct clps6700_task = {
	routine:	clps6700_bh
};

static void clps6700_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct clps6700_skt *skt = dev_id;
	u_int val, events;

	val = __raw_readl(skt->regbase + PCISR);
	if (!val)
		return;

	__raw_writel(val, skt->regbase + PCICR);

	events = 0;
	if (val & (PCM_CD1 | PCM_CD2))
		events |= SS_DETECT;
	if (val & PCM_BVD1)
		events |= SS_BATWARN;
	if (val & PCM_BVD2)
		events |= SS_BATDEAD;
	if (val & PCM_RDYL)
		events |= SS_READY;

	spin_lock(&skt->ev_lock);
	skt->ev_pending |= events;
	spin_unlock(&skt->ev_lock);
	schedule_task(&clps6700_task);
}

static int __init clps6700_init_skt(int nr)
{
	struct clps6700_skt *skt;
	int ret;

	skt = kmalloc(sizeof(struct clps6700_skt), GFP_KERNEL);
	if (!skt)
		return -ENOMEM;

	memset(skt, 0, sizeof(struct clps6700_skt));

	spin_lock_init(&skt->ev_lock);

	skt->nr       = nr;
	skt->physbase = nr ? CS5_PHYS_BASE : CS4_PHYS_BASE;
	skt->pmr      = PMR_AUTOIDLE | PMR_MCPE | PMR_CDWEAK;
	skt->cpcr     = CPCR_PDIR(PCTL1|PCTL0);
	skt->cpcr_3v3 = CPCR_PON(PCTL0);
	skt->cpcr_5v0 = CPCR_PON(PCTL0);	/* we only do 3v3 */

	skt->cur_pmr  = skt->pmr;

	skt->regbase = (u_int)ioremap(skt->physbase + CLPS6700_REG_BASE,
					CLPS6700_REG_SIZE);
	ret = -ENOMEM;
	if (!skt->regbase)
		goto err_free;

	skts[nr] = skt;

	ret = request_irq(IRQ_EINT3, clps6700_interrupt,
			  SA_SHIRQ, "pcmcia", skt);

	if (ret) {
		printk(KERN_ERR "clps6700: unable to grab irq%d (%d)\n",
		       IRQ_EINT3, ret);
		goto err_unmap;
	}
	return 0;

err_unmap:
	iounmap((void *)skt->regbase);
err_free:
	kfree(skt);
	skts[nr] = NULL;
	return ret;
}

static void clps6700_free_resources(void)
{
	int i;

	for (i = NR_CLPS6700; i >= 0; i--) {
		struct clps6700_skt *skt = skts[i];

		skts[i] = NULL;
		if (skt == NULL)
			continue;

		free_irq(IRQ_EINT3, skt);
		if (skt->regbase) {
			__raw_writel(skt->pmr,  skt->regbase + PMR);
			__raw_writel(skt->cpcr, skt->regbase + CPCR);
			__raw_writel(0,         skt->regbase + CICR);
			__raw_writel(0,         skt->regbase + PCIMR);
		}
		iounmap((void *)skt->regbase);
		kfree(skt);
	}
}

static int __init clps6700_init(void)
{
	unsigned int v;
	int err, nr;

	PLD_CF = 0;
	v = clps_readl(SYSCON2) | SYSCON2_PCCARD1 | SYSCON2_PCCARD2;
	clps_writel(v, SYSCON2);
	v = clps_readl(SYSCON1) | SYSCON1_EXCKEN;
	clps_writel(v, SYSCON1);

	for (nr = 0; nr < NR_CLPS6700; nr++) {
		err = clps6700_init_skt(nr);
		if (err)
			goto free;
	}

	err = register_ss_entry(nr, &clps6700_operations);
	if (err)
		goto free;

	return 0;

free:
	clps6700_free_resources();
	/*
	 * An error occurred.  Unmap and free all CLPS6700
	 */
	return err;
}

static void __exit clps6700_exit(void)
{
	unregister_ss_entry(&clps6700_operations);
	clps6700_free_resources();
}

module_init(clps6700_init);
module_exit(clps6700_exit);
