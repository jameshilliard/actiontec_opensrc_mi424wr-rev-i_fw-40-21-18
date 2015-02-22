/*------------------------------------------------------------------------
 . smc9194.c
 . This is a driver for SMC's 9000 series of Ethernet cards.
 .
 . Copyright (C) 1996 by Erik Stahlman
 . This software may be used and distributed according to the terms
 . of the GNU General Public License, incorporated herein by reference.
 .
 . "Features" of the SMC chip:
 .   4608 byte packet memory. ( for the 91C92.  Others have more )
 .   EEPROM for configuration
 .   AUI/TP selection  ( mine has 10Base2/10BaseT select )
 .
 . Arguments:
 . 	io     = for the base address
 .	irq    = for the IRQ
 .	ifport = 0 for autodetect, 1 for TP, 2 for AUI ( or 10base2 )
 .
 . author:
 . 	Erik Stahlman				( erik@vt.edu )
 . contributors:
 .      Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 .
 . Hardware multicast code from Peter Cammaert ( pc@denkart.be )
 .
 . Sources:
 .    o   SMC databook
 .    o   skeleton.c by Donald Becker ( becker@scyld.com )
 .    o   ( a LOT of advice from Becker as well )
 .
 . History:
 .	12/07/95  Erik Stahlman  written, got receive/xmit handled
 . 	01/03/96  Erik Stahlman  worked out some bugs, actually usable!!! :-)
 .	01/06/96  Erik Stahlman	 cleaned up some, better testing, etc
 .	01/29/96  Erik Stahlman	 fixed autoirq, added multicast
 . 	02/01/96  Erik Stahlman	 1. disabled all interrupts in smc_reset
 .		   		 2. got rid of post-decrementing bug -- UGH.
 .	02/13/96  Erik Stahlman  Tried to fix autoirq failure.  Added more
 .				 descriptive error messages.
 .	02/15/96  Erik Stahlman  Fixed typo that caused detection failure
 . 	02/23/96  Erik Stahlman	 Modified it to fit into kernel tree
 .				 Added support to change hardware address
 .				 Cleared stats on opens
 .	02/26/96  Erik Stahlman	 Trial support for Kernel 1.2.13
 .				 Kludge for automatic IRQ detection
 .	03/04/96  Erik Stahlman	 Fixed kernel 1.3.70 +
 .				 Fixed bug reported by Gardner Buchanan in
 .				   smc_enable, with outw instead of outb
 .	03/06/96  Erik Stahlman  Added hardware multicast from Peter Cammaert
 .	04/14/00  Heiko Pruessing (SMA Regelsysteme)  Fixed bug in chip memory
 .				 allocation
 .      08/20/00  Arnaldo Melo   fix kfree(skb) in smc_hardware_send_packet
 .      12/15/00  Christian Jullien fix "Warning: kfree_skb on hard IRQ"
 .	06/23/01  Russell King   Separate out IO functions for different bus
 .				 types.
 .				 Use dev->name instead of CARDNAME for printk
 .				 Add ethtool support, full duplex support
 .				 Add LAN91C96 support.
 .      11/08/01 Matt Domsch     Use common crc32 function
 ----------------------------------------------------------------------------*/

#define DRV_NAME	"smc9194"
#define DRV_VERSION	"0.15"

static const char version[] =
	DRV_NAME ".c:v" DRV_VERSION " 12/15/00 by Erik Stahlman (erik@vt.edu)\n";

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/errno.h>
#include <linux/ethtool.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#ifdef CONFIG_ARCH_SA1100
#include <asm/hardware.h>
#include <asm/arch/assabet.h>
#endif

#include "smc9194.h"
/*------------------------------------------------------------------------
 .
 . Configuration options, for the experienced user to change.
 .
 -------------------------------------------------------------------------*/

/*
 . Do you want to use 32 bit xfers?  This should work on all chips, as
 . the chipset is designed to accommodate them.
*/
#ifdef __sh__
#undef USE_32_BIT
#else
#define USE_32_BIT 1
#endif

/*
 .the SMC9194 can be at any of the following port addresses.  To change,
 .for a slightly different card, you can add it to the array.  Keep in
 .mind that the array must end in zero.
*/
static unsigned int smc_portlist[] __initdata = { 
	0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
	0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0, 0x3E0, 0
};

/*
 . Wait time for memory to be free.  This probably shouldn't be
 . tuned that much, as waiting for this means nothing else happens
 . in the system
*/
#define MEMORY_WAIT_TIME 16

/*
 . DEBUGGING LEVELS
 .
 . 0 for normal operation
 . 1 for slightly more details
 . >2 for various levels of increasingly useless information
 .    2 for interrupt tracking, status flags
 .    3 for packet dumps, etc.
*/
#define SMC_DEBUG 0

#if (SMC_DEBUG > 2 )
#define PRINTK3(x) printk x
#else
#define PRINTK3(x)
#endif

#if SMC_DEBUG > 1
#define PRINTK2(x) printk x
#else
#define PRINTK2(x)
#endif

#ifdef SMC_DEBUG
#define PRINTK(x) printk x
#else
#define PRINTK(x)
#endif


/*------------------------------------------------------------------------
 .
 . The internal workings of the driver.  If you are changing anything
 . here with the SMC stuff, you should have the datasheet and known
 . what you are doing.
 .
 -------------------------------------------------------------------------*/
#define CARDNAME "SMC9194"

static const char *chip_ids[15] = { 
	NULL,
	NULL,
	NULL,
	"SMC91C90/91C92",	/* 3 */
	"SMC91C94/91C96",	/* 4 */
	"SMC91C95",		/* 5 */
	NULL,
	"SMC91C100",		/* 7 */
	"SMC91C100FD",		/* 8 */
	NULL,
	NULL,
	NULL, 
	NULL,
	NULL,
	NULL
};

static const char * interfaces[2] = {
	"TP",
	"AUI"
};


/*-----------------------------------------------------------------
 .
 .  The driver can be entered at any of the following entry points.
 .
 .------------------------------------------------------------------  */

/*
 . This is called by  register_netdev().  It is responsible for
 . checking the portlist for the SMC9000 series chipset.  If it finds
 . one, then it will initialize the device, find the hardware information,
 . and sets up the appropriate device parameters.
 . NOTE: Interrupts are *OFF* when this procedure is called.
 .
 . NB:This shouldn't be static since it is referred to externally.
*/
int smc_init(struct net_device *dev);

/*
 . The kernel calls this function when someone wants to use the device,
 . typically 'ifconfig ethX up'.
*/
static int smc_open(struct net_device *dev);

/*
 . This handles the ethtool interface
*/
static int smc_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

/*
 . Our watchdog timed out. Called by the networking layer
*/
static void smc_timeout(struct net_device *dev);

/*
 . This is called by the kernel in response to 'ifconfig ethX down'.  It
 . is responsible for cleaning up everything that the open routine
 . does, and maybe putting the card into a powerdown state.
*/
static int smc_close(struct net_device *dev);

/*
 . This routine allows the proc file system to query the driver's
 . statistics.
*/
static struct net_device_stats * smc_query_statistics(struct net_device *dev);

/*
 . Finally, a call to set promiscuous mode (for TCPDUMP and related
 . programs) and multicast modes.
*/
static void smc_set_multicast_list(struct net_device *dev);


/*---------------------------------------------------------------
 .
 . Interrupt level calls..
 .
 ----------------------------------------------------------------*/

/*
 . Handles the actual interrupt
*/
static void smc_interrupt(int irq, void *, struct pt_regs *regs);
/*
 . This is a separate procedure to handle the receipt of a packet, to
 . leave the interrupt code looking slightly cleaner
*/
static inline void smc_rcv(struct net_device *dev);
/*
 . This handles a TX interrupt, which is only called when an error
 . relating to a packet is sent.
*/
static inline void smc_tx(struct net_device * dev);

/*
 ------------------------------------------------------------
 .
 . Internal routines
 .
 ------------------------------------------------------------
*/

/*
 . Test if a given location contains a chip, trying to cause as
 . little damage as possible if it's not a SMC chip.
*/
static int smc_probe(struct net_device *dev, int ioaddr);

/* this is called to actually send the packet to the chip */
static void smc_hardware_send_packet(struct net_device * dev);

/* Since I am not sure if I will have enough room in the chip's ram
 . to store the packet, I call this routine, which either sends it
 . now, or generates an interrupt when the card is ready for the
 . packet */
static int smc_wait_to_send_packet(struct sk_buff * skb, struct net_device *dev);

/* this does a soft reset on the device */
static void smc_reset(struct net_device *dev);

/* Enable Interrupts, Receive, and Transmit */
static void smc_enable(struct net_device *dev);

/* this puts the device in an inactive state */
static void smc_shutdown(struct net_device *dev);

/* This routine will find the IRQ of the driver if one is not
 . specified in the input to the device.  */
static int smc_findirq(struct net_device *dev);

#ifndef CONFIG_ASSABET_NEPONSET
/*
 * These functions allow us to handle IO addressing as we wish - this
 * ethernet controller can be connected to a variety of busses.  Some
 * busses do not support 16 bit or 32 bit transfers.  --rmk
 */
static inline u8 smc_inb(u_int base, u_int reg)
{
	return inb(base + reg);
}

static inline u16 smc_inw(u_int base, u_int reg)
{
	return inw(base + reg);
}

static inline void smc_ins(u_int base, u_int reg, u8 *data, u_int len)
{
	u_int port = base + reg;
#ifdef USE_32_BIT
	/* QUESTION:  Like in the TX routine, do I want
	   to send the DWORDs or the bytes first, or some
	   mixture.  A mixture might improve already slow PIO
	   performance  */
	PRINTK3((" Reading %d dwords (and %d bytes) \n",
		len >> 2, len & 3));
	insl(port, data, len >> 2);
	/* read the left over bytes */
	insb(port, data + (len & ~3), len & 3);
#else
	PRINTK3((" Reading %d words and %d byte(s) \n",
		len >> 1, len & 1));
	insw(port, data, len >> 1);
	if (len & 1) {
		data += len & ~1;
		*data = inb(port);
	}
#endif
}

static inline void smc_outb(u8 val, u_int base, u_int reg)
{
	outb(val, base + reg);
}

static inline void smc_outw(u16 val, u_int base, u_int reg)
{
	outw(val, base + reg);
}

static inline void smc_outl(u32 val, u_int base, u_int reg)
{
	u_int port = base + reg;
#ifdef USE_32_BIT
	outl(val, port);
#else
	outw(val, port);
	outw(val >> 16, port);
#endif
}

static inline void smc_outs(u_int base, u_int reg, u8 *data, u_int len)
{
	u_int port = base + reg;
#ifdef USE_32_BIT
	if (len & 2) {
		outsl(port, data, len >> 2);
		outw(*((word *)(data + (len & ~3))), port);
	}
	else
		outsl(port, data, len >> 2);
#else
	outsw(port, data, len >> 1);
#endif
}


/*-------------------------------------------------------------------------
 .  I define some macros to make it easier to do somewhat common
 . or slightly complicated, repeated tasks. 
 --------------------------------------------------------------------------*/

/* select a register bank, 0 to 3  */

#define SMC_SELECT_BANK(x)				\
	{						\
		smc_outw(x, ioaddr, BANK_SELECT);	\
	} 

/* define a small delay for the reset */
#define SMC_DELAY()					\
	{						\
		smc_inw(ioaddr, RCR);			\
		smc_inw(ioaddr, RCR);			\
		smc_inw(ioaddr, RCR);			\
	}

/* this enables an interrupt in the interrupt mask register */
#define SMC_ENABLE_INT(x)				\
	{						\
		byte mask;				\
		mask = smc_inb(ioaddr, INT_MASK);	\
		mask |= (x);				\
		smc_outb(mask, ioaddr, INT_MASK);	\
	}

/* this sets the absolutel interrupt mask */
#define SMC_SET_INT(x)					\
	{						\
		smc_outw((x), INT_MASK);		\
	}

#else

#undef SMC_IO_EXTENT
#define SMC_IO_EXTENT	(16 << 2)

/*
 * These functions allow us to handle IO addressing as we wish - this
 * ethernet controller can be connected to a variety of busses.  Some
 * busses do not support 16 bit or 32 bit transfers.  --rmk
 */
static inline u8 smc_inb(u_int base, u_int reg)
{
	u_int port = base + reg * 4;

	return readb(port);
}

static inline u16 smc_inw(u_int base, u_int reg)
{
	u_int port = base + reg * 4;

	return readb(port) | readb(port + 4) << 8;
}

static inline void smc_ins(u_int base, u_int reg, u8 *data, u_int len)
{
	u_int port = base + reg * 4;

	insb(port, data, len);
}

static inline void smc_outb(u8 val, u_int base, u_int reg)
{
	u_int port = base + reg * 4;

	writeb(val, port);
}

static inline void smc_outw(u16 val, u_int base, u_int reg)
{
	u_int port = base + reg * 4;

	writeb(val, port);
	writeb(val >> 8, port + 4);
}

static inline void smc_outl(u32 val, u_int base, u_int reg)
{
	u_int port = base + reg * 4;

	writeb(val, port);
	writeb(val >> 8, port + 4);
	writeb(val >> 16, port + 8);
	writeb(val >> 24, port + 12);
}

static inline void smc_outs(u_int base, u_int reg, u8 *data, u_int len)
{
	u_int port = base + reg * 4;

	outsb(port, data, len & ~1);
}

/*-------------------------------------------------------------------------
 .  I define some macros to make it easier to do somewhat common
 . or slightly complicated, repeated tasks. 
 --------------------------------------------------------------------------*/

/* select a register bank, 0 to 3  */

#define SMC_SELECT_BANK(x)				\
	{						\
		smc_outb(x, ioaddr, BANK_SELECT);	\
	} 

/* define a small delay for the reset */
#define SMC_DELAY()					\
	{						\
		smc_inb(ioaddr, RCR);			\
		smc_inb(ioaddr, RCR);			\
		smc_inb(ioaddr, RCR);			\
	}

/* this enables an interrupt in the interrupt mask register */
#define SMC_ENABLE_INT(x)				\
	{						\
		byte mask;				\
		mask = smc_inb(ioaddr, INT_MASK);	\
		mask |= (x);				\
		smc_outb(mask, ioaddr, INT_MASK);	\
	}

/* this sets the absolutel interrupt mask */
#define SMC_SET_INT(x)					\
	{						\
		smc_outb((x), ioaddr, INT_MASK);	\
	}

#endif

/*
 . A rather simple routine to print out a packet for debugging purposes.
*/
#if SMC_DEBUG > 2
static void print_packet(byte * buf, int length)
{
	int i;
	int remainder;
	int lines;

	printk("Packet of length %d \n", length);
	lines = length / 16;
	remainder = length % 16;

	for (i = 0; i < lines ; i ++) {
		int cur;

		for (cur = 0; cur < 8; cur ++) {
			byte a, b;

			a = *(buf ++);
			b = *(buf ++);
			printk("%02x%02x ", a, b);
		}
		printk("\n");
	}
	for (i = 0; i < remainder/2 ; i++) {
		byte a, b;

		a = *(buf ++);
		b = *(buf ++);
		printk("%02x%02x ", a, b);
	}
	if (remainder & 1) {
		byte a;

		a = *buf++;
		printk("%02x", a);
	}
	printk("\n");
}
#else
#define print_packet(buf,len) do { } while (0)
#endif

/*
 . Function: smc_reset(struct net_device *dev)
 . Purpose:
 .  	This sets the SMC91xx chip to its normal state, hopefully from whatever
 . 	mess that any other DOS driver has put it in.
 .
 . Maybe I should reset more registers to defaults in here?  SOFTRESET  should
 . do that for me.
 .
 . Method:
 .	1.  send a SOFT RESET
 .	2.  wait for it to finish
 .	3.  enable autorelease mode
 .	4.  reset the memory management unit
 .	5.  clear all interrupts
 .
*/
static void smc_reset(struct net_device *dev)
{
	u_int ioaddr = dev->base_addr;

	/* This resets the registers mostly to defaults, but doesn't
	   affect EEPROM.  That seems unnecessary */
	SMC_SELECT_BANK(0);
	smc_outw(RCR_SOFTRESET, ioaddr, RCR);

	/* this should pause enough for the chip to be happy */
	SMC_DELAY();

	/* Set the transmit and receive configuration registers to
	   default values */
	smc_outw(RCR_CLEAR, ioaddr, RCR);
	smc_outw(TCR_CLEAR, ioaddr, TCR);

	/* set the control register to automatically
	   release successfully transmitted packets, to make the best
	   use out of our limited memory */
	SMC_SELECT_BANK(1);
	smc_outw(smc_inw(ioaddr, CONTROL) | CTL_AUTO_RELEASE, ioaddr, CONTROL);

	/* Reset the MMU */
	SMC_SELECT_BANK(2);
	smc_outw(MC_RESET, ioaddr, MMU_CMD);

	/* Note:  It doesn't seem that waiting for the MMU busy is needed here,
	   but this is a place where future chipsets _COULD_ break.  Be wary
 	   of issuing another MMU command right after this */
	SMC_SET_INT(0);
}

/*
 . Function: smc_enable
 . Purpose: let the chip talk to the outside work
 . Method:
 .	1.  Enable the transmitter
 .	2.  Enable the receiver
 .	3.  Enable interrupts
*/
static void smc_enable(struct net_device *dev)
{
	u_int ioaddr = dev->base_addr;
	SMC_SELECT_BANK(0);
	/* see the header file for options in TCR/RCR NORMAL*/
	smc_outw(TCR_NORMAL, ioaddr, TCR);
	smc_outw(RCR_NORMAL, ioaddr, RCR);

	/* now, enable interrupts */
	SMC_SELECT_BANK(2);
	SMC_SET_INT(SMC_INTERRUPT_MASK);
}

/*
 . Function: smc_shutdown(struct net_device *dev)
 . Purpose:  closes down the SMC91xxx chip.
 . Method:
 .	1. zero the interrupt mask
 .	2. clear the enable receive flag
 .	3. clear the enable xmit flags
 .
 . TODO:
 .   (1) maybe utilize power down mode.
 .	Why not yet?  Because while the chip will go into power down mode,
 .	the manual says that it will wake up in response to any I/O requests
 .	in the register space.   Empirical results do not show this working.
*/
static void smc_shutdown(struct net_device *dev)
{
	u_int ioaddr = dev->base_addr;

	/* no more interrupts for me */
	SMC_SELECT_BANK(2);
	SMC_SET_INT(0);

	/* and tell the card to stay away from that nasty outside world */
	SMC_SELECT_BANK(0);
	smc_outb(RCR_CLEAR, ioaddr, RCR);
	smc_outb(TCR_CLEAR, ioaddr, TCR);
#if 0
	/* finally, shut the chip down */
	SMC_SELECT_BANK(1);
	smc_outw(smc_inw(ioaddr, CONTROL), CTL_POWERDOWN, ioaddr, CONTROL);
#endif
}


/*
 . Function: smc_setmulticast(int ioaddr, int count, dev_mc_list * adds)
 . Purpose:
 .    This sets the internal hardware table to filter out unwanted multicast
 .    packets before they take up memory.
 .
 .    The SMC chip uses a hash table where the high 6 bits of the CRC of
 .    address are the offset into the table.  If that bit is 1, then the
 .    multicast packet is accepted.  Otherwise, it's dropped silently.
 .
 .    To use the 6 bits as an offset into the table, the high 3 bits are the
 .    number of the 8 bit register, while the low 3 bits are the bit within
 .    that register.
 .
 . This routine is based very heavily on the one provided by Peter Cammaert.
*/


static void smc_setmulticast(struct net_device *dev, int count, struct dev_mc_list * addrs)
{
	u_int ioaddr = dev->base_addr;
	int			i;
	unsigned char		multicast_table[8];
	struct dev_mc_list	* cur_addr;
	/* table for flipping the order of 3 bits */
	unsigned char invert3[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	/* start with a table of all zeros: reject all */
	memset(multicast_table, 0, sizeof(multicast_table));

	cur_addr = addrs;
	for (i = 0; i < count ; i ++, cur_addr = cur_addr->next) {
		int position;

		/* do we have a pointer here? */
		if (!cur_addr)
			break;
		/* make sure this is a multicast address - shouldn't this
		   be a given if we have it here ? */
		if (!(*cur_addr->dmi_addr & 1))
			continue;

		/* only use the low order bits */
		position = ether_crc_le(6, cur_addr->dmi_addr) & 0x3f;

		/* do some messy swapping to put the bit in the right spot */
		multicast_table[invert3[position&7]] |=
					(1<<invert3[(position>>3)&7]);

	}
	/* now, the table can be loaded into the chipset */
	SMC_SELECT_BANK(3);

	for (i = 0; i < 8 ; i++) {
		smc_outb(multicast_table[i], ioaddr, MULTICAST1 + i);
	}
}

/*
 . Function: smc_wait_to_send_packet(struct sk_buff * skb, struct net_device *)
 . Purpose:
 .    Attempt to allocate memory for a packet, if chip-memory is not
 .    available, then tell the card to generate an interrupt when it
 .    is available.
 .
 . Algorithm:
 .
 . o	if the saved_skb is not currently null, then drop this packet
 .	on the floor.  This should never happen, because of TBUSY.
 . o	if the saved_skb is null, then replace it with the current packet,
 . o	See if I can sending it now.
 . o 	(NO): Enable interrupts and let the interrupt handler deal with it.
 . o	(YES):Send it now.
*/
static int smc_wait_to_send_packet(struct sk_buff * skb, struct net_device * dev)
{
	struct smc_local *lp 	= (struct smc_local *)dev->priv;
	u_int ioaddr	 	= dev->base_addr;
	word 			length;
	unsigned short 		numPages;
	word			time_out;

	netif_stop_queue(dev);
	/* Well, I want to send the packet.. but I don't know
	   if I can send it right now...  */

	if (lp->saved_skb) {
		/* THIS SHOULD NEVER HAPPEN. */
		lp->stats.tx_aborted_errors++;
		printk("%s: Bad Craziness - sent packet while busy.\n",
			dev->name);
		return 1;
	}

	length = skb->len;
		
	if(length < ETH_ZLEN)
	{
		skb = skb_padto(skb, ETH_ZLEN);
		if(skb == NULL)
		{
			netif_wake_queue(dev);
			return 0;
		}
		length = ETH_ZLEN;
	}
	lp->saved_skb = skb;

	/*
	** The MMU wants the number of pages to be the number of 256 bytes
	** 'pages', minus 1 (since a packet can't ever have 0 pages :))
	**
	** Pkt size for allocating is data length +6 (for additional status words,
	** length and ctl!) If odd size last byte is included in this header.
	*/
	numPages = ((length & 0xfffe) + 6) / 256;

	if (numPages > 7) {
		printk("%s: Far too big packet error.\n", dev->name);
		/* freeing the packet is a good thing here... but should
		 . any packets of this size get down here?   */
		dev_kfree_skb (skb);
		lp->saved_skb = NULL;
		/* this IS an error, but, i don't want the skb saved */
		netif_wake_queue(dev);
		return 0;
	}

	/* either way, a packet is waiting now */
	lp->packets_waiting++;

	/* now, try to allocate the memory */
	SMC_SELECT_BANK(2);
	smc_outw(MC_ALLOC | numPages, ioaddr, MMU_CMD);
	/*
 	. Performance Hack
	.
 	. wait a short amount of time.. if I can send a packet now, I send
	. it now.  Otherwise, I enable an interrupt and wait for one to be
	. available.
	.
	. I could have handled this a slightly different way, by checking to
	. see if any memory was available in the FREE MEMORY register.  However,
	. either way, I need to generate an allocation, and the allocation works
	. no matter what, so I saw no point in checking free memory.
	*/
	time_out = MEMORY_WAIT_TIME;
	do {
		word	status;

		status = smc_inb(ioaddr, INTERRUPT);
		if (status & IM_ALLOC_INT) {
			/* acknowledge the interrupt */
			smc_outb(IM_ALLOC_INT, ioaddr, INTERRUPT);
			break;
		}
 	} while (-- time_out);

 	if (!time_out) {
		/* oh well, wait until the chip finds memory later */
		SMC_ENABLE_INT(IM_ALLOC_INT);
		PRINTK2(("%s: memory allocation deferred.\n", dev->name));
		/* it's deferred, but I'll handle it later */
		return 0;
 	}
	/* or YES! I can send the packet now.. */
	smc_hardware_send_packet(dev);
	netif_wake_queue(dev);
	return 0;
}

/*
 . Function:  smc_hardware_send_packet(struct net_device *)
 . Purpose:
 .	This sends the actual packet to the SMC9xxx chip.
 .
 . Algorithm:
 . 	First, see if a saved_skb is available.
 .		(this should NOT be called if there is no 'saved_skb'
 .	Now, find the packet number that the chip allocated
 .	Point the data pointers at it in memory
 .	Set the length word in the chip's memory
 .	Dump the packet to chip memory
 .	Check if a last byte is needed (odd length packet)
 .		if so, set the control flag right
 . 	Tell the card to send it
 .	Enable the transmit interrupt, so I know if it failed
 . 	Free the kernel data if I actually sent it.
*/
static void smc_hardware_send_packet(struct net_device *dev)
{
	struct smc_local *lp = (struct smc_local *)dev->priv;
	struct sk_buff *skb = lp->saved_skb;
	word length, lastword;
	u_int ioaddr = dev->base_addr;
	byte packet_no;
	byte *buf;

	if (!skb) {
		PRINTK(("%s: In XMIT with no packet to send\n", dev->name));
		return;
	}

	length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	buf = skb->data;

	/* If I get here, I _know_ there is a packet slot waiting for me */
	packet_no = smc_inb(ioaddr, PNR_ARR + 1);
	if (packet_no & 0x80) {
		/* or isn't there?  BAD CHIP! */
		printk(KERN_DEBUG "%s: Memory allocation failed.\n",
			dev->name);
		dev_kfree_skb_any(skb);
		lp->saved_skb = NULL;
		netif_wake_queue(dev);
		return;
	}

	/* we have a packet address, so tell the card to use it */
	smc_outb(packet_no, ioaddr, PNR_ARR);

	/* point to the beginning of the packet */
	smc_outw(PTR_AUTOINC, ioaddr, POINTER);

 	PRINTK3(("%s: Trying to xmit packet of length %x\n",
		dev->name, length));

	print_packet(buf, length);

	/* send the packet length (+6 for status, length and ctl byte)
 	   and the status word (set to zeros) */
	smc_outl((length + 6) << 16, ioaddr, DATA_1);

	/* send the actual data
	 . I _think_ it's faster to send the longs first, and then
	 . mop up by sending the last word.  It depends heavily
 	 . on alignment, at least on the 486.  Maybe it would be
 	 . a good idea to check which is optimal?  But that could take
	 . almost as much time as is saved?
	*/
	smc_outs(ioaddr, DATA_1, buf, length);

	/* Send the last byte, if there is one.   */
	if ((length & 1) == 0)
		lastword = 0;
	else
		lastword = 0x2000 | buf[length - 1];
	smc_outw(lastword, ioaddr, DATA_1);

	/* enable the interrupts */
	SMC_ENABLE_INT(IM_TX_INT | IM_TX_EMPTY_INT);

	/* and let the chipset deal with it */
	smc_outw(MC_ENQUEUE, ioaddr, MMU_CMD);

	PRINTK2(("%s: Sent packet of length %d\n", dev->name, length));

	lp->saved_skb = NULL;
	dev_kfree_skb_any (skb);

	dev->trans_start = jiffies;

	/* we can send another packet */
	netif_wake_queue(dev);

	return;
}

/*-------------------------------------------------------------------------
 |
 | smc_init(struct net_device * dev)
 |   Input parameters:
 |	dev->base_addr == 0, try to find all possible locations
 |	dev->base_addr == 1, return failure code
 |	dev->base_addr == 2, always allocate space,  and return success
 |	dev->base_addr == <anything else>   this is the address to check
 |
 |   Output:
 |	0 --> there is a device
 |	anything else, error
 |
 ---------------------------------------------------------------------------
*/
int __init smc_init(struct net_device *dev)
{
	int ret = -ENODEV;
#if defined(CONFIG_ASSABET_NEPONSET)
	if (machine_is_assabet() && machine_has_neponset()) {
		unsigned int *addr;
		unsigned char ecor;
		unsigned long flags;

		NCR_0 |= NCR_ENET_OSC_EN;
		dev->irq = IRQ_NEPONSET_SMC9196;

		/*
		 * Map the attribute space.  This is overkill, but clean.
		 */
		addr = ioremap(0x18000000 + (1 << 25), 64 * 1024 * 4);
		if (!addr)
			return -ENOMEM;

		/*
		 * Reset the device.  We must disable IRQs around this.
		 */
		local_irq_save(flags);
		ecor = readl(addr + ECOR) & ~ECOR_RESET;
		writel(ecor | ECOR_RESET, addr + ECOR);
		udelay(100);

		/*
		 * The device will ignore all writes to the enable bit while
		 * reset is asserted, even if the reset bit is cleared in the
		 * same write.  Must clear reset first, then enable the device.
		 */
		writel(ecor, addr + ECOR);
		writel(ecor | ECOR_ENABLE, addr + ECOR);

		/*
		 * Force byte mode.
		 */
		writel(readl(addr + ECSR) | ECSR_IOIS8, addr + ECSR);
		local_irq_restore(flags);

		iounmap(addr);

		/*
		 * Wait for the chip to wake up.
		 */
		mdelay(1);

		/*
		 * Map the real registers.
		 */
		addr = ioremap(0x18000000, 8 * 1024);
		if (!addr)
			return -ENOMEM;

		ret = smc_probe(dev, (int)addr);
		if (ret)
			iounmap(addr);
	}

#elif defined(CONFIG_ISA)
	int i;
	int base_addr = dev->base_addr;

	SET_MODULE_OWNER(dev);

	/*  try a specific location */
	if (base_addr > 0x1ff)
		return smc_probe(dev, base_addr);
	else if (base_addr != 0)
		return -ENXIO;

	/* check every ethernet address */
	for (i = 0; smc_portlist[i]; i++)
		if (smc_probe(dev, smc_portlist[i]) == 0)
			return 0;

	/* couldn't find anything */
#endif
	return ret;
}

/*----------------------------------------------------------------------
 . smc_findirq
 .
 . This routine has a simple purpose -- make the SMC chip generate an
 . interrupt, so an auto-detect routine can detect it, and find the IRQ,
 ------------------------------------------------------------------------
*/
int __init smc_findirq(struct net_device *dev)
{
	int	timeout = 20;
	unsigned long cookie;
	u_int ioaddr = dev->base_addr;


	/* I have to do a STI() here, because this is called from
	   a routine that does an CLI during this process, making it
	   rather difficult to get interrupts for auto detection */
	sti();

	cookie = probe_irq_on();

	/*
	 * What I try to do here is trigger an ALLOC_INT. This is done
	 * by allocating a small chunk of memory, which will give an interrupt
	 * when done.
	 */

	/* enable ALLOCation interrupts ONLY. */
	SMC_SELECT_BANK(2);
	SMC_SET_INT(IM_ALLOC_INT);

	/*
 	 . Allocate 512 bytes of memory.  Note that the chip was just
	 . reset so all the memory is available
	*/
	smc_outw(MC_ALLOC | 1, ioaddr, MMU_CMD);

	/*
	 . Wait until positive that the interrupt has been generated
	*/
	while (timeout) {
		byte	int_status;

		int_status = smc_inb(ioaddr, INTERRUPT);

		if (int_status & IM_ALLOC_INT)
			break;		/* got the interrupt */
		timeout--;
	}
	/* there is really nothing that I can do here if timeout fails,
	   as autoirq_report will return a 0 anyway, which is what I
	   want in this case.   Plus, the clean up is needed in both
	   cases.  */

	/* DELAY HERE!
	   On a fast machine, the status might change before the interrupt
	   is given to the processor.  This means that the interrupt was
	   never detected, and autoirq_report fails to report anything.
	   This should fix autoirq_* problems.
	*/
	SMC_DELAY();
	SMC_DELAY();

	/* and disable all interrupts again */
	SMC_SET_INT(0);

	/* clear hardware interrupts again, because that's how it
	   was when I was called... */
	cli();

	/* and return what I found */
	return probe_irq_off(cookie);
}

static int __init smc_probe_chip(struct net_device *dev, int ioaddr)
{
	unsigned int temp;

	/* First, see if the high byte is 0x33 */
	temp = smc_inw(ioaddr, BANK_SELECT);
	if ((temp & 0xFF00) != 0x3300)
		return -ENODEV;

	/* The above MIGHT indicate a device, but I need to write to further
 	   test this.  */
	smc_outw(0, ioaddr, BANK_SELECT);
	temp = smc_inw(ioaddr, BANK_SELECT);
	if ((temp & 0xFF00) != 0x3300)
		return -ENODEV;

#ifndef CONFIG_ASSABET_NEPONSET
	/* well, we've already written once, so hopefully another time won't
 	   hurt.  This time, I need to switch the bank register to bank 1,
	   so I can access the base address register */
	SMC_SELECT_BANK(1);
	temp = smc_inw(ioaddr, BASE);
	if (ioaddr != (temp >> 3 & 0x3E0)) {
		printk("%s: IOADDR %x doesn't match configuration (%x)."
			"Probably not a SMC chip\n", dev->name,
			ioaddr, (base_address_register >> 3) & 0x3E0);
		/* well, the base address register didn't match.  Must not have
		   been a SMC chip after all. */
		return -ENODEV;
	}
#endif

	return 0;
}

/*
 . If dev->irq is 0, then the device has to be banged on to see
 . what the IRQ is.
 .
 . This banging doesn't always detect the IRQ, for unknown reasons.
 . a workaround is to reset the chip and try again.
 .
 . Interestingly, the DOS packet driver *SETS* the IRQ on the card to
 . be what is requested on the command line.   I don't do that, mostly
 . because the card that I have uses a non-standard method of accessing
 . the IRQs, and because this _should_ work in most configurations.
 .
 . Specifying an IRQ is done with the assumption that the user knows
 . what (s)he is doing.  No checking is done!!!!
 .
*/
static int __init smc_probe_irq(struct net_device *dev)
{
	if (dev->irq < 2) {
		int	trials;

		trials = 3;
		while (trials--) {
			dev->irq = smc_findirq(dev);
			if (dev->irq)
				break;
			/* kick the card and try again */
			smc_reset(dev);
		}
	}
	if (dev->irq == 0) {
		printk("%s: Couldn't autodetect your IRQ. Use irq=xx.\n",
			dev->name);
		return -ENODEV;
	}

	/*
	 * Some machines (eg, PCs) need to cannonicalize their IRQs.
	 */
	dev->irq = irq_cannonicalize(dev->irq);

	return 0;
}

/*----------------------------------------------------------------------
 . Function: smc_probe(struct net_device *dev, int ioaddr)
 .
 . Purpose:
 .	Tests to see if a given ioaddr points to an SMC9xxx chip.
 .	Returns a 0 on success
 .
 . Algorithm:
 .	(1) see if the high byte of BANK_SELECT is 0x33
 . 	(2) compare the ioaddr with the base register's address
 .	(3) see if I recognize the chip ID in the appropriate register
 .
 .---------------------------------------------------------------------
 */

/*---------------------------------------------------------------
 . Here I do typical initialization tasks.
 .
 . o  Initialize the structure if needed
 . o  print out my vanity message if not done so already
 . o  print out what type of hardware is detected
 . o  print out the ethernet address
 . o  find the IRQ
 . o  set up my private data
 . o  configure the dev structure with my subroutines
 . o  actually GRAB the irq.
 . o  GRAB the region
 .-----------------------------------------------------------------
*/
static int __init smc_probe(struct net_device *dev, int ioaddr)
{
	struct smc_local *smc;
	int i, memory, retval;
	static unsigned version_printed;

	const char *version_string;

	/* registers */
	word revision_register;
	word configuration_register;
	word memory_info_register;
	word memory_cfg_register;

	/* Grab the region so that no one else tries to probe our ioports. */
	if (!request_region(ioaddr, SMC_IO_EXTENT, dev->name))
		return -EBUSY;

	/*
	 * Do the basic probes.
	 */
	retval = smc_probe_chip(dev, ioaddr);
	if (retval)
		goto err_out;

	/*  check if the revision register is something that I recognize.
	    These might need to be added to later, as future revisions
	    could be added.  */
	SMC_SELECT_BANK(3);
	revision_register = smc_inw(ioaddr, REVISION);
	version_string = chip_ids[(revision_register >> 4) & 15];
	if (!version_string) {
		/* I don't recognize this chip, so... */
		printk("%s: IO %x: unrecognized revision register: %x, "
			"contact author.\n", dev->name, ioaddr,
			revision_register);

		retval = -ENODEV;
		goto err_out;
	}

	/* at this point I'll assume that the chip is an SMC9xxx.
	   It might be prudent to check a listing of MAC addresses
	   against the hardware address, or do some other tests. */

	if (version_printed++ == 0)
		printk(KERN_INFO "%s", version);

	/* fill in some of the fields */
	dev->base_addr = ioaddr;

	/*
 	 . Get the MAC address (bank 1, regs 4 - 9)
	*/
	SMC_SELECT_BANK(1);
	for (i = 0; i < 6; i += 2) {
		word	address;

		address = smc_inw(ioaddr, ADDR0 + i);
		dev->dev_addr[i + 1] = address >> 8;
		dev->dev_addr[i] = address & 0xFF;
	}

	if (!is_valid_ether_addr(dev->dev_addr))
		printk("%s: Invalid ethernet MAC address.  Please set using "
			"ifconfig\n", dev->name);

	/* get the memory information */

	SMC_SELECT_BANK(0);
	memory_info_register = smc_inw(ioaddr, MIR);
	memory_cfg_register = smc_inw(ioaddr, MCR);
	memory = (memory_cfg_register >> 9) & 0x7;  /* multiplier */
	memory *= 256 * (memory_info_register & 0xFF);

	/* now, reset the chip, and put it into a known state */
	smc_reset(dev);

	/*
	 * Ok, now that we have everything in a
	 * sane state, probe for the interrupt.
	 */
	retval = smc_probe_irq(dev);
	if (retval)
		goto err_out;

	/* Initialize the private structure. */
	if (dev->priv == NULL) {
		dev->priv = kmalloc(sizeof(struct smc_local), GFP_KERNEL);
		if (dev->priv == NULL) {
			retval = -ENOMEM;
			goto err_out;
		}
	}

	smc = dev->priv;

	/* set the private data to zero by default */
	memset(smc, 0, sizeof(struct smc_local));

	/*
	 * Get the interface characteristics.
	 * is it using AUI or 10BaseT ?
	 */
	switch (dev->if_port) {
	case IF_PORT_10BASET:
		smc->port = PORT_TP;
		break;

	case IF_PORT_AUI:
		smc->port = PORT_AUI;
		break;

	default:
		SMC_SELECT_BANK(1);
		configuration_register = smc_inw(ioaddr, CONFIG);
		if (configuration_register & CFG_AUI_SELECT) {
			dev->if_port = IF_PORT_AUI;
			smc->port = PORT_AUI;
		} else {
			dev->if_port = IF_PORT_10BASET;
			smc->port = PORT_TP;
		}
		break;
	}

	/* all interfaces are half-duplex by default */
	smc->duplex = DUPLEX_HALF;

	/* now, print out the card info, in a short format.. */
	printk("%s: %s (rev %d) at %#3x IRQ:%d INTF:%s MEM:%db ", dev->name,
		version_string, revision_register & 15, ioaddr, dev->irq,
		interfaces[smc->port], memory);
	/*
	 . Print the Ethernet address
	*/
	printk("ADDR: ");
	for (i = 0; i < 5; i++)
		printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x\n", dev->dev_addr[5]);

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	/* Grab the IRQ */
	retval = request_irq(dev->irq, &smc_interrupt, 0, dev->name, dev);
	if (retval) {
		printk("%s: unable to get IRQ %d (irqval=%d).\n", dev->name,
			dev->irq, retval);
		kfree(dev->priv);
		dev->priv = NULL;
		goto err_out;
	}

	dev->open		= smc_open;
	dev->stop		= smc_close;
	dev->hard_start_xmit	= smc_wait_to_send_packet;
	dev->tx_timeout 	= smc_timeout;
	dev->watchdog_timeo	= HZ/20;
	dev->get_stats		= smc_query_statistics;
	dev->set_multicast_list = smc_set_multicast_list;
	dev->do_ioctl		= smc_ioctl;

	return 0;

err_out:
	release_region(ioaddr, SMC_IO_EXTENT);
	return retval;
}

/*
 * This is responsible for setting the chip appropriately
 * for the interface type.  This should only be called while
 * the interface is up and running.
 */
static void smc_set_port(struct net_device *dev)
{
	struct smc_local *smc = dev->priv;
	u_int ioaddr = dev->base_addr;
	u_int val;

	SMC_SELECT_BANK(1);
	val = smc_inw(ioaddr, CONFIG);
	switch (smc->port) {
	case PORT_TP:
		val &= ~CFG_AUI_SELECT;
		break;

	case PORT_AUI:
		val |= CFG_AUI_SELECT;
		break;
	}
	smc_outw(val, ioaddr, CONFIG);

	SMC_SELECT_BANK(0);
	val = smc_inw(ioaddr, TCR);
	switch (smc->duplex) {
	case DUPLEX_HALF:
		val &= ~TCR_FDSE;
		break;

	case DUPLEX_FULL:
		val |= TCR_FDSE;
		break;
	}
	smc_outw(val, ioaddr, TCR);
}

/*
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc ..
 *
 */
static int smc_open(struct net_device *dev)
{
	struct smc_local *smc = dev->priv;
	u_int ioaddr = dev->base_addr;
	int i;

	/*
	 * Check that the address is valid.  If its not, refuse
	 * to bring the device up.  The user must specify an
	 * address using ifconfig eth0 hw ether xx:xx:xx:xx:xx:xx
	 */
	if (!is_valid_ether_addr(dev->dev_addr))
		return -EINVAL;

	/* clear out all the junk that was put here before... */
	smc->saved_skb = NULL;
	smc->packets_waiting = 0;

	/* reset the hardware */
	smc_reset(dev);
	smc_enable(dev);

	/* Select which interface to use */
	smc_set_port(dev);

	/*
		According to Becker, I have to set the hardware address
		at this point, because the (l)user can set it with an
		ioctl.  Easily done...
	*/
	SMC_SELECT_BANK(1);
	for (i = 0; i < 6; i += 2) {
		word	address;

		address = dev->dev_addr[i + 1] << 8 ;
		address |= dev->dev_addr[i];
		smc_outw(address, ioaddr, ADDR0 + i);
	}
	
	netif_start_queue(dev);
	return 0;
}

/*
 * This is our template.  Fill the rest in at run-time
 */
static const struct ethtool_cmd ecmd_template = {
	supported:	SUPPORTED_10baseT_Half |
			SUPPORTED_10baseT_Full |
			SUPPORTED_TP |
			SUPPORTED_AUI,
	speed:		SPEED_10,
	autoneg:	AUTONEG_DISABLE,
	maxtxpkt:	1,
	maxrxpkt:	1,
	transceiver:	XCVR_INTERNAL,
};

static int smc_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct smc_local *smc = dev->priv;
	u32 etcmd;
	int ret = -EINVAL;

	if (cmd != SIOCETHTOOL)
		return -EOPNOTSUPP;

	if (get_user(etcmd, (u32 *)rq->ifr_data))
		return -EFAULT;

	switch (etcmd) {
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = ecmd_template;

		ecmd.cmd = etcmd;
		ecmd.port = smc->port;
		ecmd.duplex = smc->duplex;

		ret = copy_to_user(rq->ifr_data, &ecmd, sizeof(ecmd))
				? -EFAULT : 0;
		break;
	}

	case ETHTOOL_SSET: {
		struct ethtool_cmd ecmd;

		ret = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			break;

		ret = -EFAULT;
		if (copy_from_user(&ecmd, rq->ifr_data, sizeof(ecmd)))
			break;

		/*
		 * Sanity-check the arguments.
		 */
		ret = -EINVAL;
		if (ecmd.autoneg != AUTONEG_DISABLE)
			break;
		if (ecmd.speed != SPEED_10)
			break;
		if (ecmd.duplex != DUPLEX_HALF && ecmd.duplex != DUPLEX_FULL)
			break;
		if (ecmd.port != PORT_TP && ecmd.port != PORT_AUI)
			break;

		smc->port   = ecmd.port;
		smc->duplex = ecmd.duplex;

		if (netif_running(dev))
			smc_set_port(dev);

		ret = 0;
		break;
	}

	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo edrv;

		memset(&edrv, 0, sizeof(edrv));

		edrv.cmd = etcmd;
		strcpy(edrv.driver, DRV_NAME);
		strcpy(edrv.version, DRV_VERSION);
		sprintf(edrv.bus_info, "ISA:%8.8lx:%d",
			dev->base_addr, dev->irq);

		ret = copy_to_user(rq->ifr_data, &edrv, sizeof(edrv))
				? -EFAULT : 0;
		break;
	}
	}

	return ret;
}

/*--------------------------------------------------------
 . Called by the kernel to send a packet out into the void
 . of the net.  This routine is largely based on
 . skeleton.c, from Becker.
 .--------------------------------------------------------
*/

static void smc_timeout(struct net_device *dev)
{
	/* If we get here, some higher level has decided we are broken.
	   There should really be a "kick me" function call instead. */
	printk(KERN_WARNING "%s: transmit timed out\n", dev->name);
	/* "kick" the adaptor */
	smc_reset(dev);
	smc_enable(dev);
	dev->trans_start = jiffies;
	/* clear anything saved */
	((struct smc_local *)dev->priv)->saved_skb = NULL;
	netif_wake_queue(dev);
}

/*--------------------------------------------------------------------
 .
 . This is the main routine of the driver, to handle the device when
 . it needs some attention.
 .
 . So:
 .   first, save state of the chipset
 .   branch off into routines to handle each case, and acknowledge
 .	    each to the interrupt register
 .   and finally restore state.
 .
 ---------------------------------------------------------------------*/

static void smc_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
	struct net_device *dev 	= dev_id;
	u_int ioaddr 		= dev->base_addr;
	struct smc_local *lp 	= (struct smc_local *)dev->priv;

	byte	status;
	word	card_stats;
	byte	mask;
	int	timeout;
	/* state registers */
	word	saved_bank;
	word	saved_pointer;



	PRINTK3(("%s: SMC interrupt started\n", dev->name));

	saved_bank = smc_inw(ioaddr, BANK_SELECT);

	SMC_SELECT_BANK(2);
	saved_pointer = smc_inw(ioaddr, POINTER);

	mask = smc_inb(ioaddr, INT_MASK);
	/* clear all interrupts */
	SMC_SET_INT(0);


	/* set a timeout value, so I don't stay here forever */
	timeout = 4;

	PRINTK2((KERN_WARNING "%s: MASK IS %x\n", dev->name, mask));
	do {
		/* read the status flag, and mask it */
		status = smc_inb(ioaddr, INTERRUPT) & mask;
		if (!status)
			break;

		PRINTK3((KERN_WARNING "%s: handling interrupt status %x\n",
			dev->name, status));

		if (status & IM_RCV_INT) {
			/* Got a packet(s). */
			PRINTK2((KERN_WARNING "%s: receive interrupt\n",
				dev->name));
			smc_rcv(dev);
		} else if (status & IM_TX_INT) {
			PRINTK2((KERN_WARNING "%s: TX ERROR handled\n",
				dev->name));
			smc_tx(dev);
			smc_outb(IM_TX_INT, ioaddr, INTERRUPT);
		} else if (status & IM_TX_EMPTY_INT) {
			/* update stats */
			SMC_SELECT_BANK(0);
			card_stats = smc_inw(ioaddr, COUNTER);
			/* single collisions */
			lp->stats.collisions += card_stats & 0xF;
			card_stats >>= 4;
			/* multiple collisions */
			lp->stats.collisions += card_stats & 0xF;

			/* these are for when linux supports these statistics */

			SMC_SELECT_BANK(2);
			PRINTK2((KERN_WARNING "%s: TX_BUFFER_EMPTY handled\n",
				dev->name));
			smc_outb(IM_TX_EMPTY_INT, ioaddr, INTERRUPT);
			mask &= ~IM_TX_EMPTY_INT;
			lp->stats.tx_packets += lp->packets_waiting;
			lp->packets_waiting = 0;

		} else if (status & IM_ALLOC_INT) {
			PRINTK2((KERN_DEBUG "%s: Allocation interrupt\n",
				dev->name));
			/* clear this interrupt so it doesn't happen again */
			mask &= ~IM_ALLOC_INT;

			smc_hardware_send_packet(dev);

			/* enable xmit interrupts based on this */
			mask |= (IM_TX_EMPTY_INT | IM_TX_INT);

			/* and let the card send more packets to me */
			netif_wake_queue(dev);
			
			PRINTK2(("%s: Handoff done successfully.\n",
				dev->name));
		} else if (status & IM_RX_OVRN_INT) {
			lp->stats.rx_errors++;
			lp->stats.rx_fifo_errors++;
			smc_outb(IM_RX_OVRN_INT, ioaddr, INTERRUPT);
		} else if (status & IM_EPH_INT) {
			PRINTK(("%s: UNSUPPORTED: EPH INTERRUPT\n",
				dev->name));
		} else if (status & IM_ERCV_INT) {
			PRINTK(("%s: UNSUPPORTED: ERCV INTERRUPT\n",
				dev->name));
			smc_outb(IM_ERCV_INT, ioaddr, INTERRUPT);
		}
	} while (timeout --);


	/* restore state register */
	SMC_SELECT_BANK(2);
	SMC_SET_INT(mask);

	PRINTK3((KERN_WARNING "%s: MASK is now %x\n", dev->name, mask));
	smc_outw(saved_pointer, ioaddr, POINTER);

	SMC_SELECT_BANK(saved_bank);

	PRINTK3(("%s: Interrupt done\n", dev->name));
	return;
}

/*-------------------------------------------------------------
 .
 . smc_rcv - receive a packet from the card
 .
 . There is (at least) a packet waiting to be read from
 . chip-memory.
 .
 . o Read the status
 . o If an error, record it
 . o otherwise, read in the packet
 --------------------------------------------------------------
*/
static void smc_rcv(struct net_device *dev)
{
	struct smc_local *lp = (struct smc_local *)dev->priv;
	u_int 	ioaddr = dev->base_addr;
	int 	packet_number;
	word	status;
	word	packet_length;

	/* assume bank 2 */

	packet_number = smc_inw(ioaddr, FIFO_PORTS);

	if (packet_number & FP_RXEMPTY) {
		/* we got called , but nothing was on the FIFO */
		PRINTK(("%s: WARNING: smc_rcv with nothing on FIFO.\n",
			dev->name));
		/* don't need to restore anything */
		return;
	}

	/*  start reading from the start of the packet */
	smc_outw(PTR_READ | PTR_RCV | PTR_AUTOINC, ioaddr, POINTER);

	/* First two words are status and packet_length */
	status 		= smc_inw(ioaddr, DATA_1);
	packet_length 	= smc_inw(ioaddr, DATA_1);

	packet_length &= 0x07ff;  /* mask off top bits */

	PRINTK2(("RCV: STATUS %4x LENGTH %4x\n", status, packet_length));
	/*
	 . the packet length contains 3 extra words :
	 . status, length, and an extra word with an odd byte .
	*/
	packet_length -= 6;

	if (!(status & RS_ERRORS)){
		/* do stuff to make a new packet */
		struct sk_buff  * skb;
		byte		* data;

		/* read one extra byte */
		if (status & RS_ODDFRAME)
			packet_length++;

		/* set multicast stats */
		if (status & RS_MULTICAST)
			lp->stats.multicast++;

		skb = dev_alloc_skb(packet_length + 5);

		if (skb == NULL) {
			printk(KERN_NOTICE "%s: Low memory, packet dropped.\n",
				dev->name);
			lp->stats.rx_dropped++;
			goto done;
		}

		/*
		 ! This should work without alignment, but it could be
		 ! in the worse case
		*/

		skb_reserve(skb, 2);   /* 16 bit alignment */

		skb->dev = dev;
		data = skb_put(skb, packet_length);

		smc_ins(ioaddr, DATA_1, data, packet_length);
		print_packet(data, packet_length);

		skb->protocol = eth_type_trans(skb, dev);
		netif_rx(skb);
		dev->last_rx = jiffies;
		lp->stats.rx_packets++;
		lp->stats.rx_bytes += packet_length;
	} else {
		/* error ... */
		lp->stats.rx_errors++;

		if (status & RS_ALGNERR)
			lp->stats.rx_frame_errors++;
		if (status & (RS_TOOSHORT | RS_TOOLONG))
			lp->stats.rx_length_errors++;
		if (status & RS_BADCRC)
			lp->stats.rx_crc_errors++;
	}

done:
	/*  error or good, tell the card to get rid of this packet */
	smc_outw(MC_RELEASE, ioaddr, MMU_CMD);
}


/*************************************************************************
 . smc_tx
 .
 . Purpose:  Handle a transmit error message.   This will only be called
 .   when an error, because of the AUTO_RELEASE mode.
 .
 . Algorithm:
 .	Save pointer and packet no
 .	Get the packet no from the top of the queue
 .	check if it's valid (if not, is this an error???)
 .	read the status word
 .	record the error
 .	(resend?  Not really, since we don't want old packets around)
 .	Restore saved values
 ************************************************************************/
static void smc_tx(struct net_device * dev)
{
	u_int ioaddr = dev->base_addr;
	struct smc_local *lp = (struct smc_local *)dev->priv;
	byte saved_packet;
	byte packet_no;
	word tx_status;


	/* assume bank 2 */

	saved_packet = smc_inb(ioaddr, PNR_ARR);
	packet_no = smc_inw(ioaddr, FIFO_PORTS);
	packet_no &= 0x7F;

	/* select this as the packet to read from */
	smc_outb(packet_no, ioaddr, PNR_ARR);

	/* read the first word from this packet */
	smc_outw(PTR_AUTOINC | PTR_READ, ioaddr, POINTER);

	tx_status = smc_inw(ioaddr, DATA_1);
	PRINTK3(("%s: TX DONE STATUS: %4x\n", dev->name, tx_status));

	lp->stats.tx_errors++;
	if (tx_status & TS_LOSTCAR)
		lp->stats.tx_carrier_errors++;
	if (tx_status & TS_LATCOL) {
		printk(KERN_DEBUG "%s: Late collision occurred on "
			"last xmit.\n", dev->name);
		lp->stats.tx_window_errors++;
	}
#if 0
		if (tx_status & TS_16COL) { ... }
#endif

	if (tx_status & TS_SUCCESS) {
		printk("%s: Successful packet caused interrupt\n",
			dev->name);
	}
	/* re-enable transmit */
	SMC_SELECT_BANK(0);
	smc_outw(smc_inw(ioaddr, TCR) | TCR_ENABLE, ioaddr, TCR);

	/* kill the packet */
	SMC_SELECT_BANK(2);
	smc_outw(MC_FREEPKT, ioaddr, MMU_CMD);

	/* one less packet waiting for me */
	lp->packets_waiting--;

	smc_outb(saved_packet, ioaddr, PNR_ARR);
	return;
}

/*----------------------------------------------------
 . smc_close
 .
 . this makes the board clean up everything that it can
 . and not talk to the outside world.   Caused by
 . an 'ifconfig ethX down'
 .
 -----------------------------------------------------*/
static int smc_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	/* clear everything */
	smc_shutdown(dev);

	/* Update the statistics here. */
	return 0;
}

/*------------------------------------------------------------
 . Get the current statistics.
 . This may be called with the card open or closed.
 .-------------------------------------------------------------*/
static struct net_device_stats* smc_query_statistics(struct net_device *dev) {
	struct smc_local *lp = (struct smc_local *)dev->priv;

	return &lp->stats;
}

/*-----------------------------------------------------------
 . smc_set_multicast_list
 .
 . This routine will, depending on the values passed to it,
 . either make it accept multicast packets, go into
 . promiscuous mode (for TCPDUMP and cousins) or accept
 . a select set of multicast packets
*/
static void smc_set_multicast_list(struct net_device *dev)
{
	u_int ioaddr = dev->base_addr;

	SMC_SELECT_BANK(0);
	if (dev->flags & IFF_PROMISC)
		smc_outw(smc_inw(ioaddr, RCR) | RCR_PROMISC, ioaddr, RCR);

/* BUG?  I never disable promiscuous mode if multicasting was turned on.
   Now, I turn off promiscuous mode, but I don't do anything to multicasting
   when promiscuous mode is turned on.
*/

	/* Here, I am setting this to accept all multicast packets.
	   I don't need to zero the multicast table, because the flag is
	   checked before the table is
	*/
	else if (dev->flags & IFF_ALLMULTI)
		smc_outw(smc_inw(ioaddr, RCR) | RCR_ALMUL, ioaddr, RCR);

	/* We just get all multicast packets even if we only want them
	 . from one source.  This will be changed at some future
	 . point. */
	else if (dev->mc_count) {
		/* support hardware multicasting */

		/* be sure I get rid of flags I might have set */
		smc_outw(smc_inw(ioaddr, RCR) & ~(RCR_PROMISC | RCR_ALMUL),
			ioaddr, RCR);
		/* NOTE: this has to set the bank, so make sure it is the
		   last thing called.  The bank is set to zero at the top */
		smc_setmulticast(dev, dev->mc_count, dev->mc_list);
	}
	else {
		smc_outw(smc_inw(ioaddr, RCR) & ~(RCR_PROMISC | RCR_ALMUL),
			ioaddr, RCR);

		/*
		  since I'm disabling all multicast entirely, I need to
		  clear the multicast list
		*/
		SMC_SELECT_BANK(3);
		smc_outw(0, ioaddr, MULTICAST1);
		smc_outw(0, ioaddr, MULTICAST2);
		smc_outw(0, ioaddr, MULTICAST3);
		smc_outw(0, ioaddr, MULTICAST4);
	}
}

#ifdef MODULE

static struct net_device devSMC9194;
static int io;
static int irq;
static int ifport;
MODULE_LICENSE("GPL");

MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(ifport, "i");
MODULE_PARM_DESC(io, "SMC 99194 I/O base address");
MODULE_PARM_DESC(irq, "SMC 99194 IRQ number");
MODULE_PARM_DESC(ifport, "SMC 99194 interface port (0-default, 1-TP, 2-AUI)");

int init_module(void)
{
	if (io == 0)
		printk(KERN_WARNING CARDNAME
			": You shouldn't use auto-probing with insmod!\n");

	/*
	 * Note: dev->if_port has changed to be 2.4 compliant.
	 * We keep the ifport insmod parameter the same though.
	 */
	switch (ifport) {
	case 1: devSMC9194.if_port = IF_PORT_10BASET;	break;
	case 2: devSMC9194.if_port = IF_PORT_AUI;	break;
	default: devSMC9194.if_port = 0;		break;
	}

	/* copy the parameters from insmod into the device structure */
	devSMC9194.base_addr = io;
	devSMC9194.irq       = irq;
	devSMC9194.init      = smc_init;

	return register_netdev(&devSMC9194);
}

void cleanup_module(void)
{
	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	unregister_netdev(&devSMC9194);

	free_irq(devSMC9194.irq, &devSMC9194);
	release_region(devSMC9194.base_addr, SMC_IO_EXTENT);

	if (devSMC9194.priv)
		kfree(devSMC9194.priv);
}

#endif /* MODULE */
