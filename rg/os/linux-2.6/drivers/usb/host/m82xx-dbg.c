/*
 * drivers/usb/host/m82xx-dbg.h
 *
 * MPC82xx Host Controller Interface driver for USB.
 * (C) Copyright 2005 Compulab, Ltd
 * Mike Rapoport, mike@compulab.co.il
 *
 * Brad Parker, brad@heeltoe.com
 * (C) Copyright 2000-2004 Brad Parker <brad@heeltoe.com>
 *
 * Fixes and cleanup by:
 * George Panageas <gpana@intracom.gr>
 * Pantelis Antoniou <panto@intracom.gr>
 *
 * designed for the EmbeddedPlanet RPX lite board
 * (C) Copyright 2000 Embedded Planet
 * http://www.embeddedplanet.com
 *
 */

/* 
 *  m82xx-hcd debug stuff... 
 */
#define VERBOSE_DEBUG

#define usb_pipeslow(pipe)	(((pipe) >> 26) & 1)
#define ERR(stuff...)		printk(KERN_ERR "=> " stuff)
#define WARN(stuff...)		printk(KERN_WARNING "=>: " stuff)
#define INFO(stuff...)		printk(KERN_INFO "=> " stuff)

#ifdef VERBOSE_DEBUG
#define DBG(stuff...)		printk(KERN_DEBUG "=> " stuff)
#else
#define DBG(stuff...)		do {} while(0);
#endif

static char *queue_types[] = { "ISO", "INTR", "CTRL", "BULK" };
static char *queue_state[] = {"UNDEF", "SETUP", "SETUP2", "SETUP3",
			      "INTR", "BULK", "ISO" };


#define edstring(ed_type) ({ char *temp; \
	switch (ed_type) { \
	case PIPE_CONTROL:	temp = "ctrl"; break; \
	case PIPE_BULK:		temp = "bulk"; break; \
	case PIPE_INTERRUPT:	temp = "intr"; break; \
	default: 		temp = "isoc"; break; \
	}; temp;})
#define pipestring(pipe) edstring(usb_pipetype(pipe))

void print_urb(struct urb * urb, char * str, int small)
{
#ifdef VERBOSE_DEBUG
	unsigned int pipe= urb->pipe;

	if (!urb->dev || !urb->dev->bus) {
		dbg("%s URB: no dev", str);
		return;
	}

	printk(KERN_DEBUG "%s %p dev=%d ep=%d%s-%s flags=%x len=%d/%d stat=%d\n",
	       str,
	       urb,
	       usb_pipedevice (pipe),
	       usb_pipeendpoint (pipe),
	       usb_pipeout (pipe)? "out" : "in",
	       pipestring (pipe),
	       urb->transfer_flags,
	       urb->actual_length,
	       urb->transfer_buffer_length,
	       urb->status);
    
	if (small == 1) {
		int i, len;
	
		if (usb_pipecontrol (pipe)) {
			printk (KERN_DEBUG /* __FILE__ */ ": %s: setup(8):", str);
			for (i = 0; i < 8 ; i++)
				printk (KERN_DEBUG " %02x", ((__u8 *) urb->setup_packet) [i]);
			printk (KERN_DEBUG "\n");
		}
		if ( small == 2 ) {
		if (urb->transfer_buffer_length > 0 && urb->transfer_buffer) {
			printk (KERN_DEBUG __FILE__ ": %s: data(%d/%d):", str,
				urb->actual_length,
				urb->transfer_buffer_length);
			len = usb_pipeout (pipe)?
				urb->transfer_buffer_length: urb->actual_length;
			for (i = 0; i < 24 && i < len; i++)
				printk (KERN_DEBUG " %02x", ((__u8 *) urb->transfer_buffer) [i]);
			printk (KERN_DEBUG "%s stat:%d\n", i < len? "...": "", urb->status);
		}
		}
	}
#endif
}

/*------------------------------------------------------------------*/
/* proc entry for driver debug */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int proc_m8xxhci_privateh_show(struct seq_file *s, void *unused)
{
	int i;
	struct m8xxhci_private		*m8xxhci_private = s->private;

	seq_printf(s, "===> Misc stats:\n");
	seq_printf(s, "sof = %ld", m8xxhci_private->stats.sof);
	seq_printf(s, "\tisrs = %ld", m8xxhci_private->stats.isrs);
	seq_printf(s, "\tcpm_irqs = %ld", m8xxhci_private->stats.cpm_interrupts);
	seq_printf(s, "\toverrun = %ld\n", m8xxhci_private->stats.overrun);

	seq_printf(s, "rxb = %ld", m8xxhci_private->stats.rxb);
	seq_printf(s, "\ttxb = %ld", m8xxhci_private->stats.txb);
	seq_printf(s, "\tbsy = %ld", m8xxhci_private->stats.bsy);
	seq_printf(s, "\tsof = %ld\n", m8xxhci_private->stats.sof);

	seq_printf(s, "\t");
	for ( i = 0; i < 4; i++ )
		seq_printf(s, "txe[%d] = %ld\t", i, m8xxhci_private->stats.txe[i]);
	seq_printf(s, "\n");

	seq_printf(s, "\t");
	for ( i = 0; i < 4; i++ )
		seq_printf(s, "nak[%d] = %ld\t", i, m8xxhci_private->stats.nak[i]);
	seq_printf(s, "\n");

	seq_printf(s, "\t");
	for ( i = 0; i < 4; i++ )
		seq_printf(s, "to[%d] = %ld\t", i, m8xxhci_private->stats.to[i]);
	seq_printf(s, "\n");

	seq_printf(s, "\t");
	for ( i = 0; i < 4; i++ )
		seq_printf(s, "stall[%d] = %ld\t ", i, m8xxhci_private->stats.stall[i]);
	seq_printf(s, "\n");

	seq_printf(s, "\ttx_err = %ld", m8xxhci_private->stats.tx_err);
	seq_printf(s, "\ttx_nak = %ld", m8xxhci_private->stats.tx_nak);
	seq_printf(s, "\ttx_stal = %ld", m8xxhci_private->stats.tx_stal);
	seq_printf(s, "\ttx_to = %ld", m8xxhci_private->stats.tx_to);
	seq_printf(s, "\ttx_un = %ld\n", m8xxhci_private->stats.tx_un);

	seq_printf(s, "\t");
	for ( i = 0; i < MAX_Q_TYPES; i++)
		seq_printf(s, "enqs[%s] = %ld ", queue_types[i],
			   m8xxhci_private->stats.enqueues[i]);
	seq_printf(s, "\n");

	seq_printf(s, "\t");
	for ( i = 0; i < MAX_Q_TYPES; i++)
		seq_printf(s, "cpls[%s] = %ld ", queue_types[i],
			   m8xxhci_private->stats.completes[i]);
	seq_printf(s, "\n");

	seq_printf(s, "===> Periodic schedule:\n");
	for ( i = 0; i < 32; i++ ) {
		struct m8xxhci_ep* ep = m8xxhci_private->periodic[i];
		if ( ep ) {
			seq_printf(s, "%d [%d]:", i, m8xxhci_private->load[i]);
			while ( ep ) {
				seq_printf(s, "%p ", ep);
				ep = ep->next;
			}
			seq_printf(s, "\n");
		}
	}
	
	return 0;
}

static int proc_m8xxhci_privateh_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_m8xxhci_privateh_show, PDE(inode)->data);
}

static struct file_operations proc_ops = {
	.open		= proc_m8xxhci_privateh_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
		   
static const char proc_filename[] = "driver/m8xxhci_privateh";

static void create_debug_file(struct m8xxhci_private *m8xxhci_private)
{
	struct proc_dir_entry *pde;
	printk(KERN_INFO "PQ2USB: debug file creation\n");

	pde = create_proc_entry(proc_filename, 0666, NULL);
	if (pde == NULL)
		return;

	pde->proc_fops = &proc_ops;
	pde->data = m8xxhci_private;
	m8xxhci_private->pde = pde;
}

static void remove_debug_file(struct m8xxhci_private *m8xxhci_private)
{
	if (m8xxhci_private->pde)
		remove_proc_entry(proc_filename, NULL);
}


