/*
 * drivers/usb/host/m82xx-hcd.c
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
 *
 * HW interface based on  MPC8xx Host Controller Interface 
 * driver for USB by Brad Parker, brad@heeltoe.com
 *
 * Scheduling based on SL811HS HCD (Host Controller Driver)
 */

#include <linux/config.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>
#include <linux/pci.h>	/* yeah I know it's weird... */
#include <linux/dma-mapping.h>

#include "../core/hcd.h"

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/cpm2.h>

#include "m82xx-hcd.h"

#define DRIVER_VERSION "2005"
static const char hcd_name[] = "mpc82xx-hcd";

#ifdef VERBOSE_DEBUG
#undef VERBOSE_DEBUG
#endif

/* TODO: replace by actual DMA-API calls */
void consistent_sync(void *vaddr, size_t size, int direction)
{
}

/* DEBUG stuff */
#include "m82xx-dbg.c"
 
/* these defines should not be here */
#define SIU_INT_USB 11
#define MPC82xx_IRQ_USB		(SIU_INT_USB)

#define BCSR_USB_OFFSET		(0xc)
#define BCSR_USB_DISABLE	((uint)0x80000000)
#define BCSR_USB_LOW_SPEED	((uint)0x40000000)
#define BCSR_USB_POWER		((uint)0x20000000)

/********************************************************/

static void board_start_controller(struct m8xxhci_private *hp)
{
#if defined(CONFIG_NETROUTE)
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	volatile iop_cpm2_t *io = &immap->im_ioport;

	io->iop_pdatb &= ~(1 << (31 - 24));
	udelay(1000);
	io->iop_pdatb |=  (1 << (31 - 24));
#endif
}

/* nothing */
#define board_init_check(hp)		0

static int board_configure(struct m8xxhci_private *hp)
{
	hp->sof_timer = SOF_USB_CNTRLR;	/* internal SOF */
	hp->sof_flags = SOFTF_XCVER_ECHOES_SOF;
#if defined(CONFIG_NETROUTE)
	hp->sof_flags = SOFTF_XCVER_ECHOES_SOF | SOFTF_UCODE_PATCH;
	hp->usb_clock = USB_CLOCK_PC26_CLK6;
#elif defined(CONFIG_ADS8272) || defined(CONFIG_CMF82)
	hp->usb_clock = USB_CLOCK_PC24_CLK8;

	{
//#define BCSR_ADDR		((uint)0xf8000000)
	volatile uint* bcsr_usb = (volatile uint*)(BCSR_ADDR +
						   BCSR_USB_OFFSET);

	*(volatile uint *)(BCSR_ADDR + BCSR_USB_OFFSET) &=
		~BCSR_USB_LOW_SPEED; 
	
	*(volatile uint *)(BCSR_ADDR + BCSR_USB_OFFSET) &= 
		~BCSR_USB_DISABLE;

	*bcsr_usb |= BCSR_USB_POWER;
	}
#endif
	return 0;
}

#define board_start_force_disconnect(hp) do { (void)(hp); } while(0)
#define board_stop_force_disconnect(hp)	 do { (void)(hp); } while(0)
#define board_rh_power(hp, val)		 do { (void)(hp); (void)(val); } while(0)

/*******************************************************************/

#define	PENDING_SOF		0x00010000

/*******************************************************************/

static irqreturn_t hci_interrupt(int irq, void *hci_p, struct pt_regs *regs);
/* static irqreturn_t hci_interrupt(struct usb_hcd *hcd, struct pt_regs *regs); */

static int send_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe);
static int complete_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe, int status);
static int time_left_in_frame(struct m8xxhci_private *hp, int *bytes);

static void reset_tx_ring(struct m8xxhci_private *hp);
static void reset_rx_ring(struct m8xxhci_private *hp);
static void lock_tx_ring(struct m8xxhci_private *hp);
static void unlock_tx_ring(struct m8xxhci_private *hp);

static uint16_t crc5_table[1 << 11];

/*******************************************************************/

#ifdef CONFIG_CPM2

/* alloc cpm memory on a 32 byte boundary */
static int cpm_32b_dpalloc(int size)
{
	return cpm_dpalloc(size, 32);
}

static int cpm_8b_dpalloc(int size)
{
	return cpm_dpalloc(size, 8);
}

static int setup_usb_features(struct m8xxhci_private *hp)
{
	/* nothing */
	return 0;
}

static int setup_usb_clock(struct m8xxhci_private *hp)
{
	enum usb_clock usb_clock = hp->usb_clock;
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	volatile iop_cpm2_t *io = &immap->im_ioport;
	volatile cpmux_t *cm = &immap->im_cpmux;

	switch (usb_clock) {
		case USB_CLOCK_PC26_CLK6:
			io->iop_pdirc &= ~PC_CLK6_48MHz;
			io->iop_psorc &= ~PC_CLK6_48MHz;
			io->iop_pparc |=  PC_CLK6_48MHz;
			cm->cmx_scr &= ~0x0000ff00;
			cm->cmx_scr |=  0x00003f00;
			break;
		case USB_CLOCK_PC24_CLK8:
			io->iop_pdirc &= ~PC_CLK8_48MHz;
			io->iop_psorc &= ~PC_CLK8_48MHz;
			io->iop_pparc |=  PC_CLK8_48MHz;
			cm->cmx_scr &= ~0x0000ff00;
			cm->cmx_scr |=  0x00003f00;
			break;

		default:
			printk(KERN_ERR "m8xxhci: invalid usb_clock value %d!\n", usb_clock);
			return -1;
	}

	return 0;
}

static int setup_sof_timer(struct m8xxhci_private *hp)
{
	enum sof_timer timer = hp->sof_timer;
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;

	(void)immap;
	switch (timer) {
		case SOF_USB_CNTRLR:
			break;
		default:
			printk(KERN_WARNING "Illegal SOF timer %d\n", timer);
			return -1;
	}

	return 0;
}

static void start_sof_timer(struct m8xxhci_private *hp)
{
	volatile usbregs_t *usbregs = hp->usbregs;

	BUG_ON(hp->usbregs == NULL);

	mb();
	usbregs->usb_uscom = 0x80 | 0;
	mb();
}

static void stop_sof_timer(struct m8xxhci_private *hp)
{
}

static int sof_bytes_left(struct m8xxhci_private *hp)
{
	volatile usbregs_t *usbregs = hp->usbregs;
	uint ft;

	BUG_ON(hp->usbregs == NULL);

	ft = 11999 - usbregs->usb_ussft;

	return ft / 8;
}

static void advance_frame_number(struct m8xxhci_private *hp)
{
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	char *dprambase = (char *)&immap->im_dprambase;
	volatile usbpr_t *usbprmap = (volatile usbpr_t *)(dprambase + 0x8B00);

	hp->frame_no++;
	if (hp->frame_no > 0x7ff)
		hp->frame_no = 0;

	usbprmap->usb_frame_n = 
		((crc5_table[hp->frame_no]) << 11) | hp->frame_no;
}

static int get_bus_bits(struct m8xxhci_private *hp)
{
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	volatile iop_cpm2_t *io = &immap->im_ioport;
	uint v;

	v = io->iop_pdatc;

	return (!!(v & PC_USB_OE) << 2) |
		(!!(v & PC_USB_RP) << 1) |
	        !!(v & PC_USB_RN);
}

static void flush_xmit(struct m8xxhci_private *hp)
{
}

static void kick_xmit(struct m8xxhci_private *hp)
{
	volatile usbregs_t *usbregs = hp->usbregs;

	BUG_ON(hp->usbregs == NULL);

	eieio();
	usbregs->usb_uscom = 0x80 | 0;
	mb();
}

static void process_done_txbds(struct m8xxhci_private *hp);

static void tx_err(struct m8xxhci_private *hp, int ber)
{
	volatile usbregs_t *usbregs = hp->usbregs;
//	volatile cpm_cpm2_t *cp = cpmp;
   	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
        volatile cpm_cpm2_t *cp = &immap->im_cpm;

	if ( hp->active_qe )
		hp->stats.txe[hp->active_qe->qtype]++;
	else 
		hp->stats.txe[4]++;

	/* restart tx endpoint */
	cp->cp_cpcr = mk_cr_cmd(CPM_CR_USB_PAGE, CPM_CR_USB_SBLOCK, 0,
    				CPM_CR_USB_RESTART_TX) | CPM_CR_FLG;
	mb();

	while (cp->cp_cpcr & CPM_CR_FLG);

	hp->stats.tx_restart++;


	usbregs->usb_usbmr |= (BER_TXB | BER_TXE0 | BER_RXB | BER_SOF);
	usbregs->usb_uscom = 0x80 | 0;

	if ( (ber & BER_TXB) == 0 )
		process_done_txbds(hp);
}

static int setup_usb_pins(struct m8xxhci_private *hp)
{
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	volatile iop_cpm2_t *io = &immap->im_ioport;


/* TODO: make board_usb_setup_pins or something like that */
#ifdef CONFIG_CMF82

#define USB_MODE_PIN	((uint)(1 << (31 - 0)))
#define USB_SPEED_PIN	((uint)(1 << (31 - 23)))
#define USB_SUSPEND_PIN ((uint)(1 << (31 - 29)))


	io->iop_pdirc |= (USB_MODE_PIN | USB_SPEED_PIN | USB_SUSPEND_PIN);
	io->iop_pparc &= ~(USB_MODE_PIN | USB_SPEED_PIN | USB_SUSPEND_PIN);
	io->iop_pdatc |= (USB_MODE_PIN | USB_SPEED_PIN);
	io->iop_podrc |= USB_MODE_PIN;
	io->iop_pdatc &= ~(USB_SUSPEND_PIN);
#endif

	msleep(10);

	io->iop_pdirc &= ~PC_DIR0;
	io->iop_pdirc |= PC_DIR1;
	io->iop_psorc &= ~PC_MSK;
	io->iop_pparc |=  PC_MSK;

	io->iop_pdird &= ~PD_DIR0;
	io->iop_pdird |= PD_DIR1;
	io->iop_psord &= ~PD_MSK;
	io->iop_ppard |=  PD_MSK;

	return 0;
}

static void assert_reset(struct m8xxhci_private *hp)
{
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	volatile iop_cpm2_t *io = &immap->im_ioport;

	BUG_ON(hp->usbregs == NULL);

	lock_tx_ring(hp);

	/* assert reset */
	io->iop_pdird |=  (PD_USB_TP | PD_USB_TN);
	io->iop_ppard &= ~(PD_USB_TP | PD_USB_TN);
	io->iop_pdatd &= ~(PD_USB_TP | PD_USB_TN);

	io->iop_pdirc |=  PC_USB_OE;
	io->iop_pparc &= ~PC_USB_OE;
	io->iop_pdatc &= ~PC_USB_OE;

	msleep(10);

	io->iop_pdirc |= PC_USB_OE;
	io->iop_pparc |= PC_USB_OE;

	io->iop_pdird  = (io->iop_pdird & ~PD_DIR0) | PD_DIR1;
	io->iop_ppard |=  PD_MSK;

	unlock_tx_ring(hp);
}

static void stop_controller(struct m8xxhci_private *hp)
{
	volatile usbregs_t *usbregs = hp->usbregs;
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	char *dprambase = (char *)&immap->im_dprambase;
	volatile usbpr_t *usbprmap = (volatile usbpr_t *)(dprambase + 0x8B00);
	volatile epb_t *epb;
	int index;

	usbregs->usb_usmod = usbregs->usb_usmod & ~USMOD_EN;

	epb = hp->epbptr[0];

	/* free DPRAM */
	/* free rx bd ring */
	index = epb->epb_rbase;
	cpm_dpfree(index);

	/* free tx bd ring */
	index = epb->epb_tbase;
	cpm_dpfree(index);

	/* free ebp */
	index = usbprmap->usb_epbptr[0];
	cpm_dpfree(index);
}

static int start_controller(struct m8xxhci_private *hp)
{
	struct usb_hcd *hcd = m82xx_to_hcd(hp);
	volatile cpm2_map_t *immap = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	char *dprambase = (char *)&immap->im_dprambase;
	volatile usbpr_t *usbprmap = (volatile usbpr_t *)(dprambase + 0x8B00);
	volatile usbregs_t *usbregs = (volatile usbregs_t *)&immap->im_usb;
	volatile cbd_t *bdp;
	volatile epb_t *epb;
	unsigned long mem_addr;
	int k, index, r;

	hp->usbregs = usbregs;

	r = setup_usb_features(hp);
	BUG_ON(r != 0);

	r = setup_usb_clock(hp);
	BUG_ON(r != 0);

	r = setup_usb_pins(hp);
	BUG_ON(r != 0);

	/* set up EPxPTR's */

	/* these addresses need to be a on 32 byte boundary */
	index = cpm_32b_dpalloc(sizeof(epb_t));
	usbprmap->usb_epbptr[0] = index;
	hp->epbptr[0] = (epb_t *)(dprambase + index);
	epb = hp->epbptr[0];

	/* alloc rx bd ring */
	index = cpm_8b_dpalloc(sizeof(cbd_t) * RX_RING_SIZE);
	epb->epb_rbase = index;
	hp->rbase = (cbd_t *)(dprambase + index);

	/* alloc tx bd ring */
	index = cpm_8b_dpalloc(sizeof(cbd_t) * TX_RING_SIZE);
	epb->epb_tbase = index;
	hp->tbase = (cbd_t *)(dprambase + index);

	/* reset tx bd ring entries */
	reset_tx_ring(hp);

	/* set rx bd ring entries */
	bdp = hp->rbase;
	hp->rx_hostmem = kmalloc(PAGE_ALIGN(RX_RING_SIZE * MAX_RBE), GFP_KERNEL);

	if (hp->rx_hostmem == NULL)
		return -1;

	mem_addr = (unsigned long)hp->rx_hostmem;
	for (k = 0; k < RX_RING_SIZE; k++, mem_addr += MAX_RBE)
		hp->rx_va[k] = (void *)mem_addr;

	mem_addr = (unsigned long)hp->rx_hostmem;
	for (k = 0; k < RX_RING_SIZE; k++, mem_addr += MAX_RBE, bdp++) {
		bdp->cbd_sc = BD_SC_EMPTY | BD_SC_INTRPT;
		bdp->cbd_datlen = 0;
		bdp->cbd_bufaddr = iopa(mem_addr);
	}

	/* set the last buffer to wrap */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	epb->epb_rfcr = 0x30;
	epb->epb_tfcr = 0x30;

	epb->epb_mrblr = MAX_RBE;

	epb->epb_rbptr = epb->epb_rbase;
	epb->epb_tbptr = epb->epb_tbase;

	epb->epb_tstate = 0;

	epb->epb_himmr = (uint)(CPM_MAP_ADDR >> 16);

	usbregs->usb_usep[0] = USEP_TM_CONTROL | USEP_MF_ENABLED;

	usbprmap->usb_rstate = 0;
	usbprmap->usb_frame_n = ((crc5_table[hp->frame_no]) << 11) | hp->frame_no;

	/* set 12Mbps endpoint mode & disable usb */
	usbregs->usb_usmod = USMOD_HOST | USMOD_SFTE | USMOD_TEST;

	/* set address */
	usbregs->usb_usadr = 0;

	/* clear USCOM */
	usbregs->usb_uscom = 0;

	/* reset event register & interrupt mask */
	usbregs->usb_usber = 0xffff;
	usbregs->usb_usbmr = ~(BER_RESET | BER_IDLE | BER_SFT)/* 0xffff */;

	/* install our interrupt handler */
	r = request_irq(SIU_INT_USB, hci_interrupt, 0, "m82xxhci", hcd);
	BUG_ON(r != 0);

	/* turn on board specific bits */
	board_start_controller(hp);

	/* wait for powerup */
	msleep(200);
	assert_reset(hp);

#ifdef CONFIG_8xx
	/* make sure USB microcode patch is loaded */
	if (hp->sof_flags & SOFTF_UCODE_PATCH) {
		cpm_load_patch(immap);
		verify_patch(immap);
	}
#endif

#ifdef CONFIG_BSEIP
	if (hp->sof_timer == SOF_FPGA_CLK)
		load_fpga();
#endif

	/* enable USB controller */
	usbregs->usb_usmod &= ~USMOD_TEST;
	usbregs->usb_usmod |= USMOD_EN;

	/* setup the SOF timer */
	immap->im_cpm.cp_rccr |= RCCR_EIE;
	setup_sof_timer(hp);
	start_sof_timer(hp);

	return 0;
}

#endif

static int check_bus(struct m8xxhci_private *hp)
{
	int bits;

	/* if tranmitting, exit */
	bits = get_bus_bits(hp);

	if ((bits & 0x04) == 0)
		return -1;

	bits &= ~0x04;

	switch (bits) {
		case 0:					/* disconnected */
			return 0;

		case 1:					/* low speed */
			return 1;

		case 2:					/* high speed */
			return 2;

		case 3:					/* se0? */
			return 3;
	}

	return -2;
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*
  Universal Serial Bus Specification Revision 1.1 158

  8.3.5 Cyclic Redundancy Checks

  Cyclic redundancy checks (CRCs) are used to protect all non-PID fields
  in token and data packets. In this context, these fields are
  considered to be protected fields. The PID is not included in the CRC
  check of a packet containing a CRC. All CRCs are generated over their
  respective fields in the transmitter before bit stuffing is
  performed. Similarly, CRCs are decoded in the receiver after stuffed
  bits have been removed. Token and data packet CRCs provide 100%
  coverage for all single- and double-bit errors. A failed CRC is
  considered to indicate that one or more of the protected fields is
  corrupted and causes the receiver to ignore those fields, and, in most
  cases, the entire packet.

  For CRC generation and checking, the shift registers in the generator
  and checker are seeded with an all-ones pattern. For each data bit
  sent or received, the high order bit of the current remainder is XORed
  with the data bit and then the remainder is shifted left one bit and
  the low-order bit set to zero. If the result of that XOR is one, then
  the remainder is XORed with the generator polynomial.

  When the last bit of the checked field is sent, the CRC in the
  generator is inverted and sent to the checker MSb first. When the last
  bit of the CRC is received by the checker and no errors have occurred,
  the remainder will be equal to the polynomial residual.

  A CRC error exists if the computed checksum remainder at the end of a
  packet reception does not match the residual.

  Bit stuffing requirements must
  be met for the CRC, and this includes the need to insert a zero at the
  end of a CRC if the preceding six bits were all ones.

  8.3.5.1 Token CRCs

  A five-bit CRC field is provided for tokens and covers the ADDR and
  ENDP fields of IN, SETUP, and OUT tokens or the time stamp field of an
  SOF token. The generator polynomial is:

  G(X) = X 5 + X 2 + 1

  The binary bit pattern that represents this polynomial is 00101B. If
  all token bits are received without error, the five-bit residual at
  the receiver will be 01100B.

*/

static inline unsigned int calc_crc5_fast(unsigned int addr, unsigned int endpoint)
{
	unsigned int bytes, in, crc, temp;
	int i;

	bytes = in = ((endpoint & 0x0f) << 7) | (addr & 0x7F);
	crc = 0x1f;		/* initial CRC */
	for (i = 0; i < 11; i++) {
		temp = in ^ crc;	/* do next bit */
		crc >>= 1;
		if (temp & 0x01)	/* if LSB XOR == 1 */
			crc ^= 0x0014;	/* then XOR polynomial with CRC */
		in >>= 1;
	}
	return ((~crc & 0x1f) << 11) | bytes;
}

static void generate_crc5_table(void)
{
	unsigned int addr, endpoint;

	for (endpoint = 0; endpoint < 0x10; endpoint++)
		for (addr = 0; addr < 0x80; addr++)
			crc5_table[((endpoint & 0x0f) << 7) | (addr & 0x7F)] =
				calc_crc5_fast(addr, endpoint) >> 11;
}

static inline unsigned int calc_crc5(unsigned int addr, unsigned int endpoint)
{
	unsigned int bytes;
	bytes = (endpoint << 7) | addr;
	return ((unsigned int) crc5_table[bytes] << 11) | bytes;
}

/*******************************************************************************/

static inline int map_pipe_to_qtype(int pipe)
{
	switch (usb_pipetype(pipe)) {
		case PIPE_CONTROL:
			return Q_CTRL;
		case PIPE_INTERRUPT:
			return Q_INTR;
		case PIPE_BULK:
			return Q_BULK;
		case PIPE_ISOCHRONOUS:
			return Q_ISO;
	}

	return 0;
}

/*******************************************************************************/
/* allocate an internal queue entry for sending frames */
static struct m8xxhci_qe *allocate_qe(struct m8xxhci_private *hp, int qtype)
{
	struct m8xxhci_qe *qe;
	int inuse;

	qe = hp->queues[qtype];

	while ((inuse = test_and_set_bit(0, &qe->inuse)) != 0 && 
	       qe < &hp->queues[qtype][M8XXHCI_MAXQE])
		qe++;

	if (!inuse) {
		qe->hp = hp;
		qe->qtype = qtype;
		qe->qstate = 0;
		qe->retries = 0;
		qe->busys = 0;
		qe->recv_len = 0;
		qe->send_len = 0;
		qe->reschedule = 0;
		qe->shortread = 0;
		qe->dev = 0;
		qe->urb = 0;
		qe->iso_ptr = 0;
		qe->delta = 0;

		qe->on_frame_list = 0;

		qe->current_token = 0;
		qe->zero_length_data = 0;
		return qe;
	}

	return NULL;
}

static void deallocate_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	clear_bit(0, &qe->inuse);
}

/*****************************************************************************/

static void make_active_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	hp->active_qe = qe;
	hp->active_ep = qe->ep;
}

static void make_inactive_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	hp->active_qe = 0;
	hp->active_ep = 0;
}

/* remove qe from any list it might be on and reset qe & driver state */
static void deactivate_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	qe->qstate = 0;

	/* if active, reset state */
	if (hp->active_qe == qe)
		make_inactive_qe(hp, qe);
}

static void unlink_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe);

static void finish_request(struct m8xxhci_private *hp,
			   struct m8xxhci_ep *ep,
			   struct urb *urb,
			   struct pt_regs *regs,
			   int status)
{
	unsigned i;
	struct m8xxhci_qe *qe = container_of(ep->qe_list.next, struct m8xxhci_qe, qe_list);

	BUG_ON(qe == NULL);

	complete_qe(hp, qe, status);
	hp->active_ep = NULL;
	hp->active_qe = NULL;

	list_del_init(&qe->qe_list);

	spin_lock(&urb->lock);
	if (urb->status == -EINPROGRESS)
		urb->status = status;
	spin_unlock(&urb->lock);

	DBG("%s: finishing ep %p, qe %p, urb %p, status %d\n", __FUNCTION__, ep, NULL /*ep->qe*/, urb, status);

	spin_unlock(&hp->lock);
	usb_hcd_giveback_urb(m82xx_to_hcd(hp), urb, regs);
	spin_lock(&hp->lock);

	/* leave active endpoints in the schedule */
	if (!list_empty(&ep->hep->urb_list))
		return;

	/* async deschedule? */
	if (!list_empty(&ep->schedule)) {
		list_del_init(&ep->schedule);
		if (ep == hp->next_async)
			hp->next_async = NULL;
		return;
	}

	/* periodic deschedule */
	DBG("deschedule qh%d/%p branch %d\n", ep->period, ep, ep->branch);
	for (i = ep->branch; i < PERIODIC_SIZE; i += ep->period) {
		struct m8xxhci_ep	*temp;
		struct m8xxhci_ep	**prev = &hp->periodic[i];

		while (*prev && ((temp = *prev) != ep))
			prev = &temp->next;
		if (*prev)
			*prev = ep->next;
		hp->load[i] -= ep->load;
	}	
	ep->branch = PERIODIC_SIZE;
	hp->periodic_count--;
	m82xx_to_hcd(hp)->self.bandwidth_allocated
		-= ep->load / ep->period;
	if (ep == hp->next_periodic)
		hp->next_periodic = ep->next;
}


static int complete_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe, int status)
{
	struct urb *urb = qe->urb;
	struct m8xxhci_ep *ep;
	volatile cbd_t *bdp;
	int i;

	qe->status = status;
	ep = qe->ep;

	make_inactive_qe(hp, qe);

	for ( i = 0; i < TX_RING_SIZE; i++ ) {
		if ( hp->tx_bd_qe[i] == qe ) {
			bdp = hp->tbase + i;
			bdp->cbd_sc &= BD_SC_WRAP;
			bdp->cbd_datlen = 0;
			bdp->cbd_bufaddr = 0;
		}
	}

	if (urb) {
		hp->stats.completes[qe->qtype]++;

		spin_lock(&urb->lock);
		urb->actual_length = usb_pipein(urb->pipe) ? qe->recv_len : qe->send_len;
		
		urb->hcpriv = NULL;
		spin_unlock(&urb->lock);
		unlink_qe(hp, qe);

		if (status == -1)
			status = -ETIMEDOUT;
	}
	return 0;
}

/* abort a qe; only works if it's active or just dequeued */
static void abort_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe, int status)
{
	finish_request(hp, qe->ep, qe->urb, NULL, status);
}

/* put current qe on next frame and reset active to nil */
static void pace_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	/* turn off active but don't mark ep idle - we're still using
	 * it */
	struct m8xxhci_ep *ep = qe->ep;

	ep->should_be_delayed = 1;
	if ( hp->frame_no == 0x7ff )
		ep->target_frame = 0;
	else
		ep->target_frame = hp->frame_no+1;
	make_inactive_qe(hp, qe);
}

static void advance_qe_state(struct m8xxhci_private *hp, struct m8xxhci_qe *qe);

static void nak_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe, char next_frame)
{
	switch (qe->qtype) {
	case Q_INTR:
		/* an interrupt transaction got a NAK, reset xmit machine */
		/* and try again next time */
		make_inactive_qe(hp, qe);
		return;
	case Q_ISO:
		/* nak an iso IN; retry IN at next frame */
		pace_qe(hp, qe);
		break;
	case Q_CTRL:
		/* NAK on control pipe in most cases means the
		   function is still processing the packet */
		if (usb_pipeout(qe->pipe)) 	/* OUT */
			qe->whichdata ^= 1;	/* fix PID */

		if (next_frame) {
			pace_qe(hp, qe);
		}
		break;
	default: /* BULK transfers */
		/* effectively reschedule for next frame */
		pace_qe(hp, qe);
		break;
	}
}


/* start the next pending qe, in transaction priority order */

static void pick_next_thing_to_send(struct m8xxhci_private *hp, int start_of_frame)
{
    struct m8xxhci_qe *qe;
    struct m8xxhci_ep *ep = 0;
    int send_status;

    int bytes;
  //  static int fff = 0;

    /* if tx bd list is locked, bail out */
    
    if (hp->txbd_list_busy)
	return;

    /* if actively working on qe, bail out */
    
    if ( hp->active_ep != 0 ) 
    {
	ep = hp->active_ep;
    }
    else 
    {
	if (hp->next_periodic) 
	{
	    ep = hp->next_periodic;
	    hp->next_periodic = ep->next;
	} 
	else 
	{
	    if (hp->next_async) 
	    {
		ep = hp->next_async;
	    }
	    else if (!list_empty(&hp->async)) 
	    {
		ep = container_of(hp->async.next,
		    struct m8xxhci_ep, schedule);
	    }
	    else 
	    {
		/* could set up the first fullspeed periodic
		 * transfer for the next frame ...
		 */
		
		make_inactive_qe(hp, qe);
		return;
	    }

	    if (ep->schedule.next == &hp->async)
		hp->next_async = NULL;
	    else
	    {
		hp->next_async = container_of(ep->schedule.next,
		    struct m8xxhci_ep, schedule);
	    }
	}		
    }

    if (ep->should_be_delayed) 
    {
	if ( (ep->target_frame >0  && ep->target_frame > hp->frame_no) ||
	    (ep->target_frame == 0 && hp->frame_no != 0) ) 
	{
	    make_inactive_qe(hp, qe);
	    return;
	}

	ep->should_be_delayed = 0;
	ep->target_frame = -1;
    }

    if (unlikely(list_empty(&ep->hep->urb_list))) 
    {
	DBG("empty %p queue?\n", ep);
	make_inactive_qe(hp, qe);
	return;
    }

    if (unlikely(list_empty(&ep->qe_list))) 
    {
	DBG("empty %p queue?\n", ep);
	make_inactive_qe(hp, qe);
	return;
    }

    /* if this frame doesn't have enough time left to transfer this
     * packet, wait till the next frame.  too-simple algorithm...
     */
    
    if (!time_left_in_frame(hp, &bytes)) 
    {
	make_inactive_qe(hp, qe);
	return;
    }

    qe = container_of(ep->qe_list.next, struct m8xxhci_qe, qe_list);
/*
    if (!fff)
    {
	fff = 1;
    }
    else
    {
	fff = 0;
	pace_qe(hp, qe);
	return;
    }
*/    

    send_status = send_qe(hp, qe);
    switch (send_status)
    {
    case -1:
	
	/* can't ever send this - free & exit */
	
	printk("send_qe returned -1: the qe cannot be sent (and should be aborted)\n");
	abort_qe(hp, qe, -1);

	break;

    case 1:
	udelay(250);
	break;

    case 2:		/* can't send this time - retry later */
	//printk("send_qe returned 2: no more time in frame or no bd's available\n");
	pace_qe(hp, qe);
	
	break;

    case 0:
	break;

    default:
	printk("send_qe returned %d: Unknown\n", send_status);
	break;
    } 
}

static inline cbd_t *next_bd(struct m8xxhci_private *hp)
{
    cbd_t *bdp;
    int index;

    index = hp->txnext;
    hp->tx_bd_qe[index] = 0;

    bdp = hp->tbase + hp->txnext++;
    if (bdp->cbd_sc & BD_SC_WRAP)
	hp->txnext = 0;

    bdp->cbd_sc &= BD_SC_WRAP;

    hp->txfree--;

    return bdp;
}

static inline cbd_t *next_bd_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	cbd_t *bdp;
	int index;

	index = hp->txnext;
	bdp = next_bd(hp);
	if (bdp)
		hp->tx_bd_qe[index] = qe;

	return bdp;
}

static void advance_rx_bd(struct m8xxhci_private *hp)
{
	cbd_t *bdp;

	bdp = hp->rbase + hp->rxnext;

	hp->rxnext++;
	if (bdp->cbd_sc & BD_SC_WRAP)
		hp->rxnext = 0;

	bdp->cbd_datlen = 0;
	bdp->cbd_sc &= BD_SC_WRAP;
	bdp->cbd_sc |= BD_SC_EMPTY | BD_SC_INTRPT;
}

/* reset a bd and advance the txlast ptr */
static inline void advance_tx_bd(struct m8xxhci_private *hp)
{
	cbd_t *bdp;

	bdp = hp->tbase + hp->txlast;
	hp->tx_bd_qe[hp->txlast] = 0;

	hp->txlast++;
	if (bdp->cbd_sc & BD_SC_WRAP)
		hp->txlast = 0;

	/* collect stats */
	if ((bdp->cbd_sc & (BD_USB_NAK | BD_USB_STAL | BD_USB_TO | BD_USB_UN)))
		hp->stats.tx_err++;

	if (bdp->cbd_sc & BD_USB_NAK)
		hp->stats.tx_nak++;
	if (bdp->cbd_sc & BD_USB_STAL)
		hp->stats.tx_stal++;
	if (bdp->cbd_sc & BD_USB_TO)
		hp->stats.tx_to++;
	if (bdp->cbd_sc & BD_USB_UN)
		hp->stats.tx_un++;

	hp->txfree++;

	/* I turned this off so I could see what had been sent */

	bdp->cbd_sc &= BD_SC_WRAP;
	bdp->cbd_datlen = 0;
	bdp->cbd_bufaddr = 0;
}

static inline int free_bds(struct m8xxhci_private *hp)
{
	return hp->txfree;
}

static void fixup_iso_urb(struct urb *urb, int frame_no)
{
	int i;

	urb->actual_length = 0;
	urb->status = -EINPROGRESS;
	if (frame_no >= 0)
		urb->start_frame = frame_no;

	for (i = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].actual_length = 0;
		urb->iso_frame_desc[i].status = -EXDEV;
	}
}

/*
 * got an iso rx; advance iso urb state
 *
 * we do this because there is no in,datax,ack for iso rx and we can't
 * advance the state in the tx completion like we do everything else
 */
static void process_iso_rx(struct m8xxhci_private *hp, struct m8xxhci_qe *qe, int status)
{
	struct urb *urb = qe->urb;

	urb->iso_frame_desc[qe->iso_ptr].status = status;
	urb->iso_frame_desc[qe->iso_ptr].actual_length = qe->recv_len;

	qe->frame_no = hp->frame_no;
	qe->iso_ptr++;

	qe->recv_len = 0;
	qe->shortread = 0;

	qe->data_len = urb->iso_frame_desc[qe->iso_ptr].length;
	qe->retries = 0;
	qe->busys = 0;
}

static void process_data(struct m8xxhci_private *hp, struct m8xxhci_qe *qe, int d01, u_char * data, int len)
{
	u_char *ptr;

	switch (qe->qstate) {
	case QS_ISO:
		hp->pending_iso_rx.dest = qe->iso_data + qe->recv_len;
		hp->pending_iso_rx.src = data;
		hp->pending_iso_rx.len = len;
		hp->pending_iso_rx.frame_no = qe->frame_no;
		hp->pending_iso_rx.urb = qe->urb;
		qe->recv_len += len;
		qe->data_len -= len;
		return;

	default:
		ptr = qe->data;

		if (d01 != qe->whichdata) {
			/* this probably means the function missed an ack */
			/* so we need to ignore this frame */
			hp->stats.rx_mismatch++;
			len = 0;
		} else
			/* toggle data0/1 on receive side */
			qe->whichdata ^= 1;

		break;
	}

	if (len > 0 && ptr) {
		memcpy(ptr + qe->recv_len, data, len);
		qe->recv_len += len;
		/* reduce how much we ask for in case we get a NAK and retry */
		qe->data_len -= len;
	}
}

static void flush_recv(struct m8xxhci_private *hp);

static void process_bsy(struct m8xxhci_private *hp)
{
	volatile usbregs_t *usbregs = hp->usbregs;

	BUG_ON(hp->usbregs == NULL);

	/* hack */
	usbregs->usb_usmod &= ~USMOD_EN;

	flush_recv(hp);
	flush_xmit(hp);

	usbregs->usb_usmod |= USMOD_EN;
}


/* run through completed rx bd's, matching them up with irq's */
static void process_done_rxbds(struct m8xxhci_private *hp)
{
	cbd_t *bdp;
	int status, bl, rx_err;
	u_char *bp;
	struct m8xxhci_qe *qe;
	struct urb *urb;

	BUG_ON(hp->usbregs == NULL);

	while (1) {
		bdp = hp->rbase + hp->rxnext;
		status = bdp->cbd_sc;
		bp = hp->rx_va[bdp - hp->rbase];
		bl = bdp->cbd_datlen - 2;

		if ((status & BD_SC_EMPTY) != 0)
			break;

		if ((rx_err = (status & 0x1e)) != 0) {
			hp->stats.rx_err++;

			if ((status & BD_USB_CRC))
				hp->stats.rx_crc++;
			if ((status & BD_USB_AB))
				hp->stats.rx_abort++;
			if ((status & BD_USB_NONOCT))
				hp->stats.rx_nonoct++;

			// XXX once this happens the audio & video stop...
			/* pretend we got no data to force a retry */
			bl = 0;
			status &= ~BD_USB_RX_PID;
		}

		if ((qe = hp->active_qe) != NULL) {
			/* copy the data */
			process_data(hp, qe, status & BD_USB_RX_DATA1 ? 1 : 0, bp, bl);

			/* function may be signaling read is done */
			if (bl < qe->maxpacketsize) {
				qe->shortread = 1;
				
				if ( qe->qstate == QS_SETUP3 )
					qe->zero_length_data = 1;

			}

			/* match rx bd to urb and update it */
			if ((urb = qe->urb) != NULL) {

				urb->actual_length = qe->recv_len;

				/*
				 * don't complete urbs here - do it in the
				 * xmit isr, as MOT assured me the xmit
				 * won't complete till after the ack...
				 */
			}

			/* advance iso state with each rx */
			if (qe->qstate == QS_ISO)
				process_iso_rx(hp, qe, rx_err ? -EILSEQ : 0);
		}
		
		advance_rx_bd(hp);
	}
}

/* move a queue element (pending transaction) to it's next state */
static void advance_qe_state(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
	struct urb *urb = qe->urb;
	int bytes;

	qe->retries = 0;

	switch (qe->qstate) {
	case QS_SETUP:
		qe->qstate = QS_SETUP2;

		bytes = calc_crc5(qe->devnum, qe->endpoint);
		qe->ph[0] = (usb_pipeout(qe->pipe) ? OUT : IN);
		qe->ph[1] = bytes & 0xff;
		qe->ph[2] = bytes >> 8;

		if ( qe->dev && qe->dev->speed == USB_SPEED_LOW ) {
			DBG("%s: pacing ep %p\n", __FUNCTION__, qe->ep);
			pace_qe(hp, qe);
			return;
		}
		break;
	case QS_SETUP2:
		if (qe->data_len > 0) {
			/* give the slow device time to setup after SETUP */
			if ( qe->dev && qe->dev->speed == USB_SPEED_LOW ) {
				pace_qe(hp, qe);
				return;
			}
			break;
		}

		qe->qstate = QS_SETUP3;

		if ( qe->dev && qe->dev->speed == USB_SPEED_LOW ) {
			pace_qe(hp, qe);
			return;
		}
		break;
	case QS_ISO:
		/* don't advance for IN's, we do that in rx code */
		if (usb_pipein(urb->pipe)) {
			if (qe->iso_ptr < urb->number_of_packets) {
				pace_qe(hp, qe);
				break;
			}
			make_inactive_qe(hp, qe);
			break;
		}

		urb->iso_frame_desc[qe->iso_ptr].status = 0;
		urb->iso_frame_desc[qe->iso_ptr].actual_length = qe->send_len;

		qe->send_len = 0;

		qe->iso_ptr++;
		if (qe->iso_ptr == urb->number_of_packets)
			goto finish_qe;

		qe->data_len = urb->iso_frame_desc[qe->iso_ptr].length;
		qe->retries = 0;
		qe->busys = 0;

		/* keep sending IN's until we get a nak; that will pace it */
		pace_qe(hp, qe);

		break;
	case QS_BULK:
		if (qe->data_len > 0)
			break;

		goto finish_qe;

	case QS_INTR:
		usb_settoggle(qe->dev, usb_pipeendpoint(qe->pipe), usb_pipeout(qe->pipe), qe->whichdata);
		/* fall through */

	case QS_SETUP3:
	  finish_qe:
		qe->qstate = 0;

		make_inactive_qe(hp, qe);
		finish_request(hp, qe->ep, qe->urb, NULL, 0);
		break;
	}
}

/* advance h/w tx pointer to match s/w tx ptr */
static void advance_hw_tx_ptr(struct m8xxhci_private *hp)
{
	volatile epb_t *epb = hp->epbptr[0];
	ushort new_tbptr;

	/* advance tx ring ptr to the right spot */
	new_tbptr = epb->epb_tbase + (hp->txlast * sizeof(cbd_t));
	if (epb->epb_tbptr != new_tbptr)
		epb->epb_tbptr = new_tbptr;
}

/* run through completed tx bd's, matching them up with qe's */
static void process_done_txbds(struct m8xxhci_private *hp)
{
	struct m8xxhci_qe *qe;
	cbd_t *bdp;
	int i, retry, nak, alldone, count, status;

	while (hp->txlast != hp->txnext) {
		bdp = hp->tbase + hp->txlast;

		if ((bdp->cbd_sc & BD_SC_READY))
			break;

		/* find the qe */
		qe = hp->tx_bd_qe[hp->txlast];

		/*
		 * if it's a SETUP, follow all the tx bd's
		 * if it's an IN, just one tx bd
		 * if it's an OUT, one tx bd + 'n' more for data
		 */
		if (!qe) {
			advance_tx_bd(hp);
			continue;
		}

		alldone = 1;
		retry = 0;
		nak = 0;
		count = 0;

		/* clean up the bd's for this qe */
		for (i = 0; i < TX_RING_SIZE; i++) {
			if (qe != hp->tx_bd_qe[hp->txlast])
				break;

			count++;

			status = bdp->cbd_sc;

			/* note errors */
			retry |= status & (BD_USB_TO | BD_USB_UN);
			nak |= status & (BD_USB_NAK | BD_USB_STAL);

			/* if not done and no errors, keep waiting */
			if ((status & BD_SC_READY)) {
				alldone = 0;
				if (retry == 0 && nak == 0)
					return;
			}

			/* if data out & ok, advance send */
			if ((status & (BD_USB_DATA0 | BD_USB_DATA1)) &&
				(qe->qstate == QS_SETUP2 || qe->qstate == QS_BULK || qe->qstate == QS_ISO) && nak == 0 && retry == 0) {
				qe->data_len -= bdp->cbd_datlen;
				qe->send_len += bdp->cbd_datlen;
			}

			advance_tx_bd(hp);
			bdp = hp->tbase + hp->txlast;
		}

		
		if ( nak & BD_USB_NAK )
			hp->stats.nak[qe->qtype]++;
		if ( nak & BD_USB_STAL )
			hp->stats.stall[qe->qtype]++;
		if ( retry & BD_USB_TO )
			hp->stats.to[qe->qtype]++;

		/* if we get a timeout on a slow interrupt transactions,
		   pretend it's a nak so we delay and retry later */
		if (retry && usb_pipeslow(qe->pipe) && qe->qtype == Q_INTR) {
			retry = 0;
			nak = BD_USB_NAK;
		}

		/* if error, retry transaction */
		if (retry) {
			hp->stats.retransmit++;
			if (++(qe->retries) > MAX_QE_RETRIES/*  && qe->qtype != Q_INTR */)
				abort_qe(hp, qe, -ETIMEDOUT);
			else {
				/* always retry in the next frame... */
				pace_qe(hp, qe);
			}
		} else {
			/* if short, we tried to read to much and we're done */
			/* if stalled and short, spec says no status phase */
			if (qe->shortread) {
				if ((nak & BD_USB_STAL)) {
					/* don't advance if protocol stall */
					/* (i.e. control pipe) */
					if (qe->qstate == QS_SETUP2 && qe->qtype != Q_CTRL)
						qe->qstate = QS_SETUP3;
				}

				/* finish up on short only or short+nak */
				qe->data_len = 0;
				nak = 0;
				alldone = 1;
			}

			/* if nak, resend IN's from where we left off */
			if (nak) {
				process_done_rxbds(hp);

				/* if stall, abort else retry later */
				if ((nak & BD_USB_STAL))
					abort_qe(hp, qe, -EPIPE);
				else
					nak_qe(hp, qe, 0);
			} else {
				/* if ok, and done, advance qe state */

				/* keep bulk up to date */
				if (qe->qstate == QS_BULK)
					usb_dotoggle(qe->dev, usb_pipeendpoint(qe->pipe), usb_pipeout(qe->pipe));

				if (alldone) {
					/* in case any rx's snuck in */
					process_done_rxbds(hp);
					advance_qe_state(hp, qe);
				}
			}
		}

		/* if we got a short read or tx timeout of some flavor
		 * we will have cleaned up the bds's and left a gap
		 * between where the hw things the next tx is and where
		 * we think it is.  so, we need to rectify this situation...
		 */
		advance_hw_tx_ptr(hp);
	}
}

static int controller_enabled(struct m8xxhci_private *hp)
{
	volatile usbregs_t *usbregs = hp->usbregs;

	BUG_ON(hp->usbregs == NULL);

	return (usbregs->usb_usmod & USMOD_EN) != 0;
}

static void reset_bus_history(struct m8xxhci_private *hp)
{
	hp->bus_history = 0;
}

static void idle_bus(struct m8xxhci_private *hp)
{
	int bus_state;

	bus_state = check_bus(hp);
	if (bus_state < 0)
		return;

	/* keep a history of the bus state */
	hp->bus_history = (hp->bus_history << 2) | bus_state;
	/* if controller not enabled, lines will float high on disconnect */
	if (!controller_enabled(hp) && ((hp->bus_history & 0x3f) == 0x3f))
		hp->bus_history = 0;

	if (hp->force_disconnect) {

		board_start_force_disconnect(hp);
		hp->force_disconnect = 0;
		hp->port_state = PS_DISCONNECTED;
		if (hp->rh.port_status & RH_PS_CCS) {
			stop_sof_timer(hp);
			hp->rh.port_status &= ~RH_PS_CCS;
			hp->rh.port_status |= RH_PS_CSC;
		}
	} else {
		switch (hp->bus_history & 0x3f) {
		case 0:
			hp->port_state = PS_DISCONNECTED;
			if (hp->rh.port_status & RH_PS_CCS) {

				/* require a little more time before we give up */
				if (hp->bus_history & /* 0xfffffff */ 0x0fff )
					return;

				stop_sof_timer(hp);

				hp->rh.port_status &= ~RH_PS_CCS;
				hp->rh.port_status |= RH_PS_CSC;
			}
			break;
		case 0x15:	/* 010101 */
			hp->port_state = PS_CONNECTED;
			hp->rh.port_status |= RH_PS_LSDA;

			if (!(hp->rh.port_status & RH_PS_CCS)) {

				start_sof_timer(hp);

				hp->rh.port_status |= RH_PS_CCS;
				hp->rh.port_status |= RH_PS_CSC;
			}
			break;
		case 0x2a:	/* 101010 */
			hp->port_state = PS_CONNECTED;
			hp->rh.port_status &= ~RH_PS_LSDA;

			if (!(hp->rh.port_status & RH_PS_CCS)) {

				start_sof_timer(hp);

				hp->rh.port_status |= RH_PS_CCS;
				hp->rh.port_status |= RH_PS_CSC;
			}
			break;
#if 0
		case 0x3f:	/* 111111 */
			/* Bus Reset ??? */
			printk("Force Bus Reset\n");
			break;
#endif

		default:
			break;
		}
	}
}

static void process_done_rxbds(struct m8xxhci_private *hp);

/* executed inside isr_lock */
static void service_pending(struct m8xxhci_private *hp, int ber)
{
	int start_of_frame = 0;
	BUG_ON(hp->usbregs == NULL);

	hp->stats.isrs++;

	if (ber & BER_SOF) {
		int index;
		hp->stats.sof++;
		mb();
		advance_frame_number(hp);

		index = hp->frame_no % (PERIODIC_SIZE - 1);
		
		/* be graceful about almost-inevitable periodic schedule
		 * overruns:  continue the previous frame's transfers iff
		 * this one has nothing scheduled.
		 */
		if (hp->next_periodic) {
			hp->stats.overrun++;
		}
		if (hp->periodic[index])
			hp->next_periodic = hp->periodic[index];
		start_of_frame = 1;
	}

	/* note: rx bd's must be processed before tx bds */
	/* (we depend on this) */
	if (ber & BER_RXB) {
		hp->stats.rxb++;
		process_done_rxbds(hp);
	}

	if (ber & BER_BSY) {
		hp->stats.bsy++;
		process_bsy(hp);
	}

	if (ber & BER_TXB) {
		hp->stats.txb++;
		process_done_txbds(hp);
	}

	if (ber & BER_TXE0) {
		tx_err(hp, ber);
	}


	if ( ber & (BER_TXB | BER_TXE0 | BER_SOF) ) 
		pick_next_thing_to_send(hp, start_of_frame);
}

/*
 * do the actual work of servicing pending isrs;
 * has a lock so only one thread can call service routine at a time
 * pending work is sampled with ints off; actual work is done with
 * interrupts enabled;
 *
 * pending work is added two by short simple isr's which call this
 * routine when they are done.
 */

static void service_isr(struct m8xxhci_private *hp)
{
    unsigned int ber;

    /* only one way into service routine, one at a time */
    if (test_and_set_bit(0, &hp->isr_state))
	return;

    while (1) 
    {
	/* sample pending work */
	mb();
	ber = hp->pending_isrs;
	hp->pending_isrs = 0;
	mb();

	/* any pending work? */
	if (ber == 0) 
	{
	    /* no, exit, allowing others to enter */
	    clear_bit(0, &hp->isr_state);
	    return;
	}
	/* pending work; we hold lock so enable isrs & service */

	service_pending(hp, ber);
    }
}


static void lock_tx_ring(struct m8xxhci_private *hp)
{
	unsigned long flags;
	/* change the lock (hp->lock) */
	spin_lock_irqsave(&hp->txbd_list_lock, flags);
	hp->txbd_list_busy++;
	spin_unlock_irqrestore(&hp->txbd_list_lock, flags);
}

static void unlock_tx_ring(struct m8xxhci_private *hp)
{
	unsigned long flags;
	spin_lock_irqsave(&hp->txbd_list_lock, flags);
	hp->txbd_list_busy--;
	spin_unlock_irqrestore(&hp->txbd_list_lock, flags);
}

/*
   add SOF frame to tx ring
   does NOT lock ring
*/
static void add_sof_to_tx_ring(struct m8xxhci_private *hp)
{
	volatile cbd_t *bdp;
	int bytes;

	/* always leave 2 bds for a control message */
	if (free_bds(hp) < 3)
		return;

	bytes = ((crc5_table[hp->frame_no]) << 11) | hp->frame_no;
	hp->frame_no++;
	if (hp->frame_no > 0x7ff)
		hp->frame_no = 0;

	hp->sof_pkt[0] = SOF;
	hp->sof_pkt[1] = bytes & 0xff;
	hp->sof_pkt[2] = bytes >> 8;

	consistent_sync(hp->sof_pkt, 3, PCI_DMA_BIDIRECTIONAL);

	bdp = next_bd(hp);
	bdp->cbd_datlen = 3;
	bdp->cbd_bufaddr = __pa(hp->sof_pkt);
	bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST | BD_SC_INTRPT;
}



/*
 * sample & reset pending usb interrupts in hardware;
 * add to pending isr work, call pending service routine
 */
static irqreturn_t hci_interrupt(int irq, void *_hcd, struct pt_regs *regs)
{
	struct usb_hcd* hcd = (struct usb_hcd*)_hcd;
	struct m8xxhci_private *hp = hcd_to_m82xx(hcd);
	volatile usbregs_t *usbregs = hp->usbregs;
	ushort ber;
	unsigned long flags;

	BUG_ON(hp->usbregs == NULL);

	spin_lock_irqsave(&hp->lock, flags);

	hp->stats.cpm_interrupts++;
	
	/* sample and reset the ber */
	ber = usbregs->usb_usber;
	usbregs->usb_usber = ber;

	/* clear this */
	if (ber & BER_SFT)
		ber &= ~BER_SFT;

	mb();
	hp->pending_isrs |= ber;
	mb();

	service_isr(hp);

	spin_unlock_irqrestore(&hp->lock, flags);

	return IRQ_HANDLED;
}


/*
   fill tx bd's

   unsigned long flags;

   spin_lock_irqsave(&hp->txbd_list_lock, flags);
   hp->txbd_list_busy++;
   spin_unlock_irqrestore(&hp->txbd_list_lock, flags);
   
   take from queue(s)
   fill in txbd(s)
   
   spin_lock_irqsave(&hp->txbd_list_lock, flags);
   hp->txbd_list_busy--;
   spin_unlock_irqrestore(&hp->txbd_list_lock, flags);
   
   spin_lock_irqsave(&txbd_sof_lock, flags);
   if (hp->need_sof) {
   ---> note; the sof should really be placed at the first descriptor
   add_sof_to_tx_ring(hp);
   hp->need_sof--;
   }
   spin_unlock_irqrestore(&txbd_sof_lock, flags);

   kick_xmit(hp);
*/ 

/*
  This driver uses a microcode patch which requires a 1Khz signal be fed to
  DREQ0.  There are issues around syncing the internal code to the external
  1khz signal.  The software must be in-sync with the external signal
  since it delineates the start-of-frame.

  Because the Sipex 5310 USB transceiver does not echo tranmitted data
  the way the Philips PDIUSBP11A transceiver does (and the Philips is
  used on the FADS board so the MOT people assume thats what you
  have), you will NOT get SOF interrupts if you use Sipex part (which
  some RPXLite board do, since it claims to be pin compat).

  So, if you have a Sipex USB transceiver you need a way to figure out
  when the SOF interrupts occur.  One way is to connect DREC0 to an
  interrupt line, but this requires a h/w change.

  A s/w only fix is to run a h/w timer and to 'sync' it to the BRG1
  output.  This is what timer4 is used for.  A linux timer can't be used
  because it won't stay in sync.  The timer can not drift - it must be
  exactly in sync with the BRG1' timer'.

  [If you are designing hardware or can change it I would use timer1 (or
  any timer which has an output pin) and connect the timer output to
  DREC0.  This frees up BRG1 and is the simplest way to make the microcode
  patch work.]

  RPX_LITE DW board - The DW rev of the RPXLite board has circuitry
  which will connect DREQ0 (a.k.a. PC15) to BRG1O (a.k.a. PA7).  So,
  the code outputs BRG1 on PA7 and makes PC15 an input.

  BSE IPenging - On the BSE ipengine I generate the 1khz via timer2
  and feed this to the fpga which feeds it to dreq0*.  There's also
  another option to have the FPGA generate the clock signal with an
  internal timer if Timer 2 is needed for something else.

  Other - One some hardware PA6/TOUT1 is connected to PC15/DREQ0 to
  make the microcode patch work.  This requires only timer1 to work.
*/

static void reset_tx_ring(struct m8xxhci_private *hp)
{
	volatile cbd_t *bdp;
	int i;

	/* reset tx bd ring entries */
	bdp = hp->tbase;
	for (i = 0; i < TX_RING_SIZE; i++) {
		hp->tx_bd_qe[i] = 0;
		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* set the last buffer to wrap */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	hp->txnext = 0;
	hp->txlast = 0;
	hp->txfree = TX_RING_SIZE;
}

static void reset_rx_ring(struct m8xxhci_private *hp)
{
	volatile cbd_t *bdp;
	int i;

	bdp = hp->rbase;
	for (i = 0; i < RX_RING_SIZE; i++) {
		bdp->cbd_sc = BD_SC_EMPTY | BD_SC_INTRPT;
		bdp->cbd_datlen = 0;
		bdp++;
	}

	/* set the last buffer to wrap */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	hp->rxnext = 0;
}

static void flush_recv(struct m8xxhci_private *hp)
{
	volatile epb_t *epb;

	epb = hp->epbptr[0];
	epb->epb_rbptr = epb->epb_rbase;

	reset_rx_ring(hp);
}

static int time_left_in_frame(struct m8xxhci_private *hp, int *bytes)
{
	*bytes = sof_bytes_left(hp);

	/*
	 * Be careful! if we crash into the SOF send, the transmit
	 * will lock up...
	 */
 	if (*bytes < 100)
        /* if (*bytes < 50) */
		return 0;

	return 1;
}

/*
   send the next frame for a queue element, depending on it's state
   if the qe is a SETUP, multiple frames are actually send

   SETUP
        [setup stage]
        ->      setup   3 bytes
        ->      data0   8 bytes
                                        <- ack
        [optional data stage]
        ->      out/in  3 bytes
        ->      datax   n bytes
                                        <- ack
        [status stage]
        ->      out/in  3 bytes
        ->      data1   0 bytes
                                        <- ack

   example:
     get descriptor
        -> setup(3), data0(8) 0x80, 0x06, 0, 1, 0, 0, 8, 0
                                        <- ack
        -> in(3)
                                        <- data1(8)
        -> ack
        -> out(3), data1(0)
                                        <- ack

     set address
        -> setup(3), data0(8) 0x80, 0x05, 0, 1, 0, 0, 0, 0
                                        <- ack
        -> in(3)
                                        <- data1(0)
        -> ack

   return value:
       -1 - the qe cannot be sent (and should be aborted)
        1 - sending, but no more bd's should be added
        2 - no more time in frame or no bd's available
	0 - sent, more packets can be added
*/
static int send_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
    volatile cbd_t *bdp, *first_bdp;
    int bytes, len, data_bd_count, ret, maxsze;
    unsigned char token, *data;
    unsigned long flags;
    struct urb *urb = qe->urb;
    struct usb_iso_packet_descriptor *ipd;
    volatile usbregs_t *usbregs = hp->usbregs;
    
    BUG_ON(hp->usbregs == NULL);

    bdp = (volatile cbd_t *)0; /* init to 0 so if used unitialized
				  (as gcc says) we get PANIC */

    maxsze = qe->dev ? usb_maxpacket(qe->dev, qe->pipe, usb_pipeout(qe->pipe)) : 8;

    if (maxsze < 8)
	maxsze = 8;

    qe->maxpacketsize = maxsze;

    spin_lock_irqsave(&hp->txbd_list_lock, flags);
    
    /* time check */
    
    if (!time_left_in_frame(hp, &bytes)) 
    {
	spin_unlock_irqrestore(&hp->txbd_list_lock, flags);
	return 2;
    }

    /* paranoid check */
    
    if (hp->active_qe && hp->active_qe != qe) 
    {

	if (hp->active_qe->busys == MAX_QE_STALLED) 
	{
	    kick_xmit(hp);
	}

	ret = 2;

	if (++hp->active_qe->busys > MAX_QE_BUSYS)
	    ret = -1;

	spin_unlock_irqrestore(&hp->txbd_list_lock, flags);
	return ret;
    }

    make_active_qe(hp, qe);

    /* setup for building tx bds */
    
    ret = 0;
    first_bdp = 0;

    /* lock the tx bd ring */
    
    lock_tx_ring(hp);

    switch (qe->qstate) 
    {
    case QS_SETUP:
	usbregs->usb_usep[0] = (usbregs->usb_usep[0] & ~USEP_TM_MASK) | USEP_TM_CONTROL;

	qe->whichdata = 1;
	
	if (free_bds(hp) < 2) 
	{
	    ret = 2;
	    break;
	}

	/* setup stage transaction (SETUP + DATA0) */
	
	bdp = next_bd_qe(hp, qe);
	bdp->cbd_datlen = 3;
	bdp->cbd_bufaddr = __pa(qe->ph);
	bdp->cbd_sc |= BD_SC_LAST;

	qe->current_token = bdp;

	if (qe->dev && qe->dev->speed == USB_SPEED_LOW)
	    bdp->cbd_sc |= BD_USB_LSP;

	/* don't set ready in BD */
	
	first_bdp = bdp;

	consistent_sync(qe->cmd, 8, PCI_DMA_BIDIRECTIONAL);

	bdp = next_bd_qe(hp, qe);
	bdp->cbd_datlen = 8;
	bdp->cbd_bufaddr = __pa(qe->cmd);
	bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST | BD_USB_DATA0 | BD_USB_TC | BD_USB_CNF | BD_SC_INTRPT;

	/* more to do - send these and stop adding bd's for now */
	ret = 1;
	
	break;

    case QS_ISO:
	usbregs->usb_usep[0] = (usbregs->usb_usep[0] & ~USEP_TM_MASK) | USEP_TM_ISOCHRONOUS;

	maxsze = 1024;

	ipd = &urb->iso_frame_desc[qe->iso_ptr];
	qe->iso_data = qe->data + ipd->offset;

	token = usb_pipeout(qe->pipe) ? OUT : IN;
	len = ipd->length;
	data = qe->data + ipd->offset + ipd->actual_length;

	/* if no space in frame, wait... */

	first_bdp = 0;
	
	while (len > 0) 
	{
	    if (len < maxsze)
		maxsze = len;

	    /* data stage (OUT+DATAx or IN) */
	    
	    bdp = next_bd_qe(hp, qe);
	    bdp->cbd_datlen = 3;
	    bdp->cbd_bufaddr = __pa(qe->ph);

	    /* don't set ready in first BD */
	    
	    if (first_bdp == 0) 
	    {
		first_bdp = bdp;
		bdp->cbd_sc |= BD_SC_LAST;
	    } 
	    else
		bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST;

	    switch (token) 
	    {
	    case OUT:
		consistent_sync(data, maxsze, PCI_DMA_BIDIRECTIONAL);

		bdp = next_bd_qe(hp, qe);
		bdp->cbd_datlen = maxsze;
		bdp->cbd_bufaddr = __pa(data);
		bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST | BD_USB_DATA0 | BD_USB_TC /* | BD_USB_CNF */ ;
		
		break;
		
	    case IN:
		bdp->cbd_sc |= BD_USB_CNF;
		break;
	    }

	    data += maxsze;
	    len -= maxsze;
	}

	/* set interrupt on last bd */
	if (first_bdp) 
	{
	    bdp->cbd_sc |= BD_SC_INTRPT;

	    /* more to do - stop sending bd's */
	    
	    ret = 1;
	    
	    break;
	}
	
	break;

    case QS_BULK:
	qe->whichdata = usb_gettoggle(qe->dev, usb_pipeendpoint(qe->pipe), usb_pipeout(qe->pipe)) ? 1 : 0;
	/* fall through */

    case QS_SETUP2:
	
	/* calc how many bd's we need to send this transaction */
	
	data_bd_count = (qe->data_len + maxsze - 1) / maxsze;
	len = qe->data_len;

	usbregs->usb_usep[0] = (usbregs->usb_usep[0] & ~USEP_TM_MASK) | USEP_TM_CONTROL;

	/* only one IN per frame */
	if (data_bd_count > 1) 
	{
	    if (len > maxsze)
		len = maxsze;
	    
	    data_bd_count = 1;
	}

	if (free_bds(hp) < 4 + data_bd_count) 
	{
	    /* requeue, we don't have enough bd's  */
	    
	    ret = 2;
	    
	    break;
	}

	/*
	 * If direction is "send", change the frame from SETUP (0x2D)
	 * to OUT (0xE1). Else change it from SETUP to IN (0x69)
	 */
	
	token = usb_pipeout(qe->pipe) ? OUT : IN;
	data = qe->data + qe->send_len;

	/* if we're sending more, fix up the data0/1 marker */

	/* experiment - trim down to available bytes left in frame */
	if (len > bytes) 
	{
	    bytes = (bytes * 9) / 10;
	    len = (bytes / maxsze) * maxsze;
	    
	    if (len == 0) 
	    {
		ret = 2;
		break;
	    }
	}

	first_bdp = 0;

	while (len > 0) 
	{
	    if (len < maxsze)
		maxsze = len;

	    /* data stage (OUT+DATAx or IN) */
	    
	    bdp = next_bd_qe(hp, qe);
	    bdp->cbd_datlen = 3;
	    bdp->cbd_bufaddr = __pa(qe->ph);

	    /* don't set ready in first BD */
	    if (first_bdp == 0) 
	    {
		first_bdp = bdp;
		bdp->cbd_sc |= BD_SC_LAST;
	    } 
	    else
		bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST;

	    if (qe->dev && qe->dev->speed == USB_SPEED_LOW)
		bdp->cbd_sc |= BD_USB_LSP;

	    switch (token) 
	    {
	    case OUT:
		
		/* follow OUT with DATAx */
		
		consistent_sync(data, maxsze, PCI_DMA_BIDIRECTIONAL);

		bdp = next_bd_qe(hp, qe);
		bdp->cbd_datlen = maxsze;
		bdp->cbd_bufaddr = __pa(data);
		bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST | (qe->whichdata ? BD_USB_DATA1 : BD_USB_DATA0) | BD_USB_TC | BD_USB_CNF;
		
		/* toggle data0/1 on send side */
		
		qe->whichdata ^= 1;
		
		break;
		
	    case IN:
		bdp->cbd_sc |= BD_USB_CNF;

		if (qe->dev && qe->dev->speed == USB_SPEED_LOW)
		    len = 0;

		break;
	    }

	    data += maxsze;
	    len -= maxsze;
	}

	/* set interrupt on last bd */
	
	if (first_bdp) 
	{
	    bdp->cbd_sc |= BD_SC_INTRPT;

	    /* more to do - stop sending bd's */
	    
	    ret = 1;
	    
	    break;
	}

	/* fall through if no bd's (i.e. len == 0) */
	
	qe->qstate = QS_SETUP3;

    case QS_SETUP3:
	
	/* status stage transaction (IN or OUT w/zero-len data) */
	
	token = usb_pipeout(qe->pipe) ? IN : OUT;

	if (free_bds(hp) < 2) 
	{
	    ret = 2;
	    break;
	}

	bytes = calc_crc5(qe->devnum, qe->endpoint);
	qe->ph[0] = token;
	qe->ph[1] = bytes & 0xff;
	qe->ph[2] = bytes >> 8;

	bdp = next_bd_qe(hp, qe);
	bdp->cbd_datlen = 3;
	bdp->cbd_bufaddr = __pa(qe->ph);
	bdp->cbd_sc |= BD_SC_LAST;
	
	/* don't set ready in BD */
	
	first_bdp = bdp;

	qe->current_token = bdp;

	if (qe->dev && qe->dev->speed == USB_SPEED_LOW)
	    bdp->cbd_sc |= BD_USB_LSP;

	switch (token) 
	{
	case OUT:
	    
	    /* send STATUS stage empty DATA1 packet */
	    
	    bdp = next_bd_qe(hp, qe);
	    bdp->cbd_datlen = 0;
	    bdp->cbd_bufaddr = 0;
	    bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST | BD_USB_DATA1 | BD_USB_TC | BD_USB_CNF | BD_SC_INTRPT;
	    
	    break;
	    
	case IN:
	    
	    /* get STATUS stage empty DATA1 packet */
	    
	    qe->whichdata = 1;
	    bdp->cbd_sc |= BD_USB_CNF | BD_SC_INTRPT;
	    
	    break;
	}

	/* done */
	
	ret = 0;
	
	break;
	
    case QS_INTR:
	usbregs->usb_usep[0] = (usbregs->usb_usep[0] & ~USEP_TM_MASK) | USEP_TM_CONTROL;

	token = IN;

	qe->whichdata = usb_gettoggle(qe->dev, usb_pipeendpoint(qe->pipe), usb_pipeout(qe->pipe)) ? 1 : 0;

	if (free_bds(hp) < 2) 
	{
	    ret = 2;
	    break;
	}

	bdp = next_bd_qe(hp, qe);
	bdp->cbd_datlen = 3;
	bdp->cbd_bufaddr = __pa(qe->ph);
	bdp->cbd_sc |= BD_SC_LAST | BD_USB_CNF | BD_SC_INTRPT;
	
	/* don't set ready in BD */
	
	first_bdp = bdp;

	if (qe->dev && qe->dev->speed == USB_SPEED_LOW)
	    bdp->cbd_sc |= BD_USB_LSP;

	/* done */
	
	ret = 0;
	
	break;
    }

    /* now allow the whole shabang to go by setting the first BD ready */
    
    if (first_bdp)
	first_bdp->cbd_sc |= BD_SC_READY;

    consistent_sync(qe->ph, 3, PCI_DMA_BIDIRECTIONAL);

    if (!(hp->sof_flags & SOFTF_UCODE_PATCH)) 
    {
	/* check if we need an SOF */
	
	if (hp->need_sof) 
	{
	    /* note; the sof should really put in the first descriptor */
	    
	    add_sof_to_tx_ring(hp);
	    hp->need_sof--;
	}
    }

    unlock_tx_ring(hp);
    kick_xmit(hp);

    /* if we changed our mind, turn off active qe */
    
    if (ret == 2) 
    {
	make_inactive_qe(hp, qe);
    }

    spin_unlock_irqrestore(&hp->txbd_list_lock, flags);

    return ret;
}

/* forcably remove a qe */
static void unlink_qe(struct m8xxhci_private *hp, struct m8xxhci_qe *qe)
{
    int i;

    /* we're active, clean up any tx ring ptrs */
    for (i = 0; i < TX_RING_SIZE; i++) {
	if (hp->tx_bd_qe[i] == qe)
	    hp->tx_bd_qe[i] = 0;
    }

    deactivate_qe(hp, qe);
    deallocate_qe(hp, qe);
}


/*------------------------------------------------------------------*/
/* Virtual root hub */
#include "m82xx-rh.c"
/*------------------------------------------------------------------*/

/* usb 1.1 says max 90% of a frame is available for periodic transfers.
 * this driver doesn't promise that much since it's got to handle an
 * IRQ per packet; irq handling latencies also use up that time.
 */
#define	MAX_PERIODIC_LOAD	300	/* out of 1000 usec */

static int balance(struct m8xxhci_private *hp, u16 period, u16 load)
{
    int	i, branch = -ENOSPC;

    /* search for the least loaded schedule branch of that period
     * which has enough bandwidth left unreserved.
     */
    
    for (i = 0; i < period ; i++) 
    {
	if (branch < 0 || hp->load[branch] > hp->load[i]) 
	{
	    int	j;

	    for (j = i; j < PERIODIC_SIZE; j += period) 
	    {
		if ((hp->load[j] + load) > MAX_PERIODIC_LOAD)
		    break;
	    }
	    
	    if (j < PERIODIC_SIZE)
		continue;
	    
	    branch = i; 
	}
    }
    
    return branch;
}

static int
m82xx_urb_enqueue(struct usb_hcd *hcd, struct usb_host_endpoint *hep,
		  struct urb *urb, int mem_flags)
{
	struct m8xxhci_private *m82xx = hcd_to_m82xx(hcd);
	int devnum, endpoint, bytes;
	unsigned long flags;
	struct m8xxhci_qe *qe;
	int qtype = map_pipe_to_qtype(urb->pipe);
	struct m8xxhci_ep *ep;
	struct usb_device *udev = urb->dev;

	int is_out = !usb_pipein(urb->pipe);
	int retval;
	int i;

	m82xx->stats.enqueues[qtype]++;

	devnum = usb_pipedevice(urb->pipe);
	endpoint = usb_pipeendpoint(urb->pipe);

	ep = kcalloc(1, sizeof(*ep), mem_flags);
	if (!ep) {
		return -ENOMEM;
	}

	/* avoid allocations withing spin_lock as in sl811 */
	spin_lock_irqsave(&m82xx->lock, flags);
	if (!hep->hcpriv) {
		INIT_LIST_HEAD(&ep->schedule);
		INIT_LIST_HEAD(&ep->qe_list);
		ep->udev = usb_get_dev(udev);

		if ( qtype == Q_ISO || qtype == Q_INTR ) {
			ep->load = usb_calc_bus_time(ep->udev->speed, !is_out,
						     (qtype == Q_ISO),
						     usb_maxpacket(ep->udev, urb->pipe, is_out))
				/ 1000;
			DBG("%s: ep %p, urb %p, period %d, load %d\n",
			     __FUNCTION__, ep, urb, urb->interval, ep->load);

			if (urb->interval > PERIODIC_SIZE)
				urb->interval = PERIODIC_SIZE;
			ep->period = urb->interval;
			ep->branch = PERIODIC_SIZE;
		}
	}
	else {
		kfree(ep);
		ep = hep->hcpriv;
	}

	/* build queue element */
	qe = allocate_qe(m82xx, qtype);
	if (qe == 0) {
		spin_unlock_irqrestore(&m82xx->lock, flags);
		if ( !hep->hcpriv)
			kfree(ep);
		return -1;
	}

	hep->hcpriv = ep;

	list_add_tail(&qe->qe_list, &ep->qe_list);
	ep->hep = hep;

	ep->target_frame = -1;
	ep->should_be_delayed = 0;

	qe->ep = ep;
	qe->hp = m82xx;
	qe->dev = udev;
	qe->pipe = urb->pipe;
	qe->devnum = devnum;
	qe->endpoint = endpoint;
	qe->data_len = urb->transfer_buffer_length;
	qe->cmd = urb->setup_packet;
	qe->data = urb->transfer_buffer;
	qe->status = 1;
	qe->urb = urb;
	
	bytes = calc_crc5(qe->devnum, qe->endpoint);
	qe->ph[1] = bytes & 0xff;
	qe->ph[2] = bytes >> 8;
		
	switch (qtype) {
	case Q_CTRL:
		qe->qstate = QS_SETUP;
		qe->ph[0] = SETUP;
		break;
	case Q_INTR:
		qe->qstate = QS_INTR;
		qe->ph[0] = (usb_pipeout(qe->pipe) ? OUT : IN);
		break;
	case Q_BULK:
		qe->qstate = QS_BULK;
		qe->whichdata = -1;
		qe->ph[0] = (usb_pipeout(qe->pipe) ? OUT : IN);
		break;
	case Q_ISO:
		qe->qstate = QS_ISO;
		qe->data_len = urb->iso_frame_desc[0].length;
		fixup_iso_urb(urb, -1);
		qe->ph[0] = (usb_pipeout(qe->pipe) ? OUT : IN);
		break;
	}

	qe->hep = hep;

	switch(qtype) {
	case Q_CTRL:
	case Q_BULK:
		if (list_empty(&ep->schedule))
			list_add_tail(&ep->schedule, &m82xx->async);
		break;
	case Q_INTR:
	case Q_ISO:
		urb->interval = ep->period;
		if (ep->branch < PERIODIC_SIZE)
			break;
		
		retval = balance(m82xx, ep->period, ep->load);
		if (retval < 0) {
			spin_unlock_irqrestore(&m82xx->lock, flags);
			return retval;
		}
		ep->branch = retval;
		retval = 0;
		urb->start_frame = (m82xx->frame_no & (PERIODIC_SIZE - 1))
			+ ep->branch;
		
		/* sort each schedule branch by period (slow before fast)
		 * to share the faster parts of the tree without needing
		 * dummy/placeholder nodes
		 */
		DBG("schedule qh%d/%p branch %d\n", ep->period, ep, ep->branch);
		for (i = ep->branch; i < PERIODIC_SIZE; i += ep->period) {
			struct m8xxhci_ep	**prev = &m82xx->periodic[i];
			struct m8xxhci_ep	*here = *prev;
			
			while (here && ep != here) {
				if (ep->period > here->period)
					break;
				prev = &here->next;
				here = *prev;
			}
			if (ep != here) {
				ep->next = here;
				*prev = ep;
			}
			m82xx->load[i] += ep->load;
		}
		m82xx->periodic_count++;
		hcd->self.bandwidth_allocated += ep->load / ep->period;
		break;
	}

	spin_lock(&urb->lock);
	if (urb->status != -EINPROGRESS) {
		spin_unlock(&urb->lock);
		finish_request(m82xx, ep, urb, NULL, 0);
		spin_unlock_irqrestore(&m82xx->lock, flags);
		return 0;
	}
	urb->hcpriv = hep;
	spin_unlock(&urb->lock);


	spin_unlock_irqrestore(&m82xx->lock, flags);

	return 0;
}

static int
m82xx_urb_dequeue(struct usb_hcd *hcd, struct urb *urb)
{
	struct m8xxhci_private *m82xx = hcd_to_m82xx(hcd);
	struct usb_host_endpoint *hep;
	struct m8xxhci_ep *ep;

	unsigned long flags;

	spin_lock_irqsave(&urb->lock, flags);
	hep = urb->hcpriv;
	spin_unlock_irqrestore(&urb->lock, flags);

	if (!hep)
		return -EINVAL;

	spin_lock_irqsave(&m82xx->lock, flags);
	ep = hep->hcpriv;
	
	if ( ep ) {
		if ( ep == m82xx->active_ep ) {
			make_inactive_qe(m82xx, 0);
		}
		finish_request(m82xx, ep, urb, NULL, -1);
	}
	spin_unlock_irqrestore(&m82xx->lock, flags);

	return 0;
}

static void m82xx_stop(struct usb_hcd *hcd)
{
}

static int m82xx_start(struct usb_hcd *hcd)
{
    struct usb_device	*udev;


    /* chip has been reset, VBUS power is off */

    udev = usb_alloc_dev(NULL, &hcd->self, 0);
    if (!udev)
	return -ENOMEM;

    udev->speed = USB_SPEED_FULL;
    hcd->state = HC_STATE_RUNNING;

    return 0;
}

static int m82xx_get_frame_number(struct usb_hcd *hcd)
{
	struct m8xxhci_private* m82xx = hcd_to_m82xx(hcd);
	
	return m82xx->frame_no;
}

static void
m82xx_endpoint_disable(struct usb_hcd *hcd, struct usb_host_endpoint *hep)
{
	struct m8xxhci_ep *ep = hep->hcpriv;

	if (!ep)
		return;

	/* assume we'd just wait for the irq */
	if (!list_empty(&hep->urb_list))
		msleep(3);
	if (!list_empty(&hep->urb_list))
		WARN("ep %p not empty?\n", ep);

	usb_put_dev(ep->udev);
	kfree(ep);
	hep->hcpriv = NULL;
}

/* called to init HCD and root hub */
static int m82xx_reset(struct usb_hcd *hcd)
{
	return 0;
}

/* NOTE:  these suspend/resume calls relate to the HC as
 * a whole, not just the root hub; they're for bus glue.
 */
/* called after all devices were suspended */
static int m82xx_suspend(struct usb_hcd *hcd, pm_message_t message)
{
	return 0;
}

/* called before any devices get resumed */
static int m82xx_resume(struct usb_hcd *hcd)
{
	return 0;
}

static struct hc_driver m82xx_hc_driver = {
	.description =		hcd_name,
	.hcd_priv_size =	sizeof(struct m8xxhci_private),

	.irq   =		0,
	.flags =		HCD_USB11 | HCD_MEMORY,

	.reset = m82xx_reset,
	.start = m82xx_start,
	.stop = m82xx_stop,

	.suspend = m82xx_suspend,
	.resume = m82xx_resume,

	.urb_enqueue =		m82xx_urb_enqueue,
	.urb_dequeue =		m82xx_urb_dequeue,
	.endpoint_disable =	m82xx_endpoint_disable,

	.get_frame_number =	m82xx_get_frame_number,

	.hub_status_data =	m82xx_hub_status_data,
	.hub_control =		m82xx_hub_control,
	.suspend =		m82xx_hub_suspend,
	.resume =		m82xx_hub_resume,
};


static int __init m82xxhci_probe(struct device *dev)
{
	struct platform_device	*pdev;
	struct usb_hcd		*hcd;
	struct m8xxhci_private	*m82xx;
	int			irq;
	int			retval = 0;

	pdev = container_of(dev, struct platform_device, dev);

	if (dev->dma_mask) {
		DBG("no we won't dma\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	/* allocate and initialize hcd */
	hcd = usb_create_hcd(&m82xx_hc_driver, dev, dev->bus_id);
	if (!hcd) {
		retval = 0;
		goto err1;
	}

	m82xx = hcd_to_m82xx(hcd);
	dev_set_drvdata(dev, m82xx);

	/* init driver stuctures */
	spin_lock_init(&m82xx->lock);
	spin_lock_init(&m82xx->txbd_list_lock);
    
	retval = board_configure(m82xx);
	if (retval != 0) {
		goto err1;
	}
    
	retval = board_init_check(m82xx);
	if (retval != 0) {
		goto err1;
	}
    
	generate_crc5_table();
    
	m82xx->pending_iso_rx.urb = 0;
    
	m82xx->disabled = 1;
    
	INIT_LIST_HEAD(&m82xx->async);
    
	m82xx->port_state = PS_INIT;

	hcd->product_desc = "PQ2 intergrated USB controller v0.1";

	/* init controller */
	retval = start_controller(m82xx);
	if ( retval < 0 )
		goto err2;

	m82xx->port_state = PS_DISCONNECTED;

	retval = usb_add_hcd(hcd, irq, SA_INTERRUPT);
	if (retval < 0)
		goto err2;

	create_debug_file(m82xx);

	m82xx->rh.port_status |= RH_PS_CSC;

	return 0;

  err2:
	usb_remove_hcd(hcd);
	free_irq(irq, dev);
  err1:
	usb_put_hcd(hcd);
	return retval;
}

static int __init_or_module m82xxhci_remove(struct device *dev)
{
	struct m8xxhci_private	*m82xx = dev_get_drvdata(dev);
	struct usb_hcd		*hcd = m82xx_to_hcd(m82xx);
	struct platform_device	*pdev;

	pdev = container_of(dev, struct platform_device, dev);

	stop_controller(m82xx);

	remove_debug_file(m82xx);

	free_irq(MPC82xx_IRQ_USB, hcd);

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM
static int m82xxhci_suspend(struct device *dev, u32 state, u32 phase)
{
	return 0;
}

static int m82xxhci_resume(struct device *dev, u32 phase)
{
	
	return 0;
}
#else
#define	m82xxhci_suspend	NULL
#define m82xxhci_resume		NULL
#endif /* CONFIG_PM */


static struct device_driver m82xxhci_driver = {
	.name =		(char *) hcd_name,
	.bus =		&platform_bus_type,

	.probe =	m82xxhci_probe,
	.remove =	m82xxhci_remove,

	.suspend =	m82xxhci_suspend,
	.resume =	m82xxhci_resume,
};

/*-------------------------------------------------------------------------*/
/* FIXME: This platform_device stuff is  definetly does not belong here */
#define MPC82xx_USB_OFFSET	(0x11B60)
#define MPC82xx_USB_SIZE	(0x22)

void mpc8272_hcd_release(struct device * dev)
{
}

struct platform_device mpc8272_hcd_device = {
	.name = "mpc82xx-hcd",
	.id	= 3,
	.num_resources	 = 2,
	.resource = (struct resource[]) 
	{
		{
			.name	= "USB mem",
			.start	= MPC82xx_USB_OFFSET,
			.end	= MPC82xx_USB_OFFSET + MPC82xx_USB_SIZE - 1,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "USB interrupt",
			.start	= MPC82xx_IRQ_USB,
			.end	= MPC82xx_IRQ_USB,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev.release = mpc8272_hcd_release
};


static int __init m82xxhci_init(void) 
{
	if (usb_disabled())
		return -ENODEV;

	platform_device_register(&mpc8272_hcd_device);
	INFO("driver %s, %s\n", hcd_name, DRIVER_VERSION);
	return driver_register(&m82xxhci_driver);
}

module_init(m82xxhci_init);

static void __exit m82xxhci_cleanup(void) 
{	
	driver_unregister(&m82xxhci_driver);
	platform_device_unregister(&mpc8272_hcd_device);
}

module_exit(m82xxhci_cleanup);

MODULE_AUTHOR("Brad Parker");
MODULE_DESCRIPTION("MPC8xx USB Host Controller Interface driver");
MODULE_LICENSE("GPL");

