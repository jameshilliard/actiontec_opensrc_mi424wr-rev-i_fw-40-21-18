/* Low-level polled-mode parallel port routines for the Accelent IDP
 *
 * Author: Rich Dulabahn <rich@accelent.com>
 *
 * Inspiration taken from parport_amiga.c and parport_atari.c.
 *
 * To use, under menuconfig:
 *   1)  Turn on <*> Accelent IDP under Parallel port setup
 *   2)  Turn on <*> Parallel printer support under Character devices
 *
 * This will give you parport0 configured as /dev/lp0
 *
 * To make the correct /dev/lp* entries, enter /dev and type this:
 *
 * mknod lp0 c 6 0
 * mknod lp1 c 6 1
 * mknod lp2 c 6 2
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <asm/hardware.h>

/*
 * Parallel data port is port H, data
 * Parallel data direction is port H, direction
 * Control port is port I, data, lowest 4 bits
 * Status port is port G, data, upper 5 bits
 */

#define INPUTPOWERHANDLER		0
/* masks */
#define CONTROL_MASK			0x0f
#define STATUS_MASK			0xf8

#undef DEBUG

#ifdef DEBUG
#define DPRINTK  printk
#else
#define DPRINTK(stuff...)
#endif

static struct parport *this_port = NULL;

static unsigned char
parport_idp_read_data(struct parport *p)
{
	unsigned char c;

	c = IDP_FPGA_PORTH_DATA;
	DPRINTK("read_data:0x%x\n",c);
	return c;
}

static void
parport_idp_write_data(struct parport *p, unsigned char data)
{
	IDP_FPGA_PORTH_DATA = data;
	DPRINTK("write_data:0x%x\n",data);
}

static unsigned char
parport_idp_read_control(struct parport *p)
{
	unsigned char c;

	c = IDP_FPGA_PORTI_DATA & CONTROL_MASK;
	DPRINTK("read_control:0x%x\n",c);
	return c;
}

static void
parport_idp_write_control(struct parport *p, unsigned char control)
{
	unsigned int temp;

	temp = IDP_FPGA_PORTH_DATA;
	temp &= ~CONTROL_MASK;
	IDP_FPGA_PORTI_DATA = (temp | (control & CONTROL_MASK));
DPRINTK("write_control:0x%x\n",control);
}

static unsigned char
parport_idp_frob_control(struct parport *p, unsigned char mask,
			   unsigned char val)
{
	unsigned char c;

/* From the parport-lowlevel.txt file...*/
/* This is equivalent to reading from the control register, masking out
the bits in mask, exclusive-or'ing with the bits in val, and writing
the result to the control register. */

/* Easy enough, right? */

	c = parport_idp_read_control(p);
	parport_idp_write_control(p, (c & ~mask) ^ val);
	DPRINTK("frob_control:0x%x\n",c);
	return c;
}

static unsigned char
parport_idp_read_status(struct parport *p)
{
	unsigned char c;

	c = IDP_FPGA_PORTG_DATA & STATUS_MASK;
	c ^= 0x80;  /* toggle S7 bit, active low */
	DPRINTK("read_status:0x%x\n",c);
	return c;
}

static void
parport_idp_init_state(struct pardevice *d, struct parport_state *s)
{
}

static void
parport_idp_save_state(struct parport *p, struct parport_state *s)
{
}

static void
parport_idp_restore_state(struct parport *p, struct parport_state *s)
{
}

static void
parport_idp_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
}

static void
parport_idp_enable_irq(struct parport *p)
{
}

static void
parport_idp_disable_irq(struct parport *p)
{
}

static void
parport_idp_data_forward(struct parport *p)
{
	IDP_FPGA_PORTH_DIR = 0x00; /* 0 sets to output */
	DPRINTK("data_forward:0x%x\n",0);
}

static void
parport_idp_data_reverse(struct parport *p)
{
	IDP_FPGA_PORTH_DIR = 0xff; /* and 1 sets to input */
	DPRINTK("data_reverse:0x%x\n",0xff);
}

static void
parport_idp_inc_use_count(void)
{
	MOD_INC_USE_COUNT;
}

static void
parport_idp_dec_use_count(void)
{
	MOD_DEC_USE_COUNT;
}

static struct parport_operations parport_idp_ops = {
	parport_idp_write_data,
	parport_idp_read_data,

	parport_idp_write_control,
	parport_idp_read_control,
	parport_idp_frob_control,

	parport_idp_read_status,

	parport_idp_enable_irq,
	parport_idp_disable_irq,

	parport_idp_data_forward,
	parport_idp_data_reverse,

	parport_idp_init_state,
	parport_idp_save_state,
	parport_idp_restore_state,

	parport_idp_inc_use_count,
	parport_idp_dec_use_count,

	parport_ieee1284_epp_write_data,
	parport_ieee1284_epp_read_data,
	parport_ieee1284_epp_write_addr,
	parport_ieee1284_epp_read_addr,

	parport_ieee1284_ecp_write_data,
	parport_ieee1284_ecp_read_data,
	parport_ieee1284_ecp_write_addr,

	parport_ieee1284_write_compat,
	parport_ieee1284_read_nibble,
	parport_ieee1284_read_byte,
};


int __init
parport_idp_init(void)
{
	struct parport *p;

	p = parport_register_port((unsigned long)0,PARPORT_IRQ_NONE,PARPORT_DMA_NONE,&parport_idp_ops);

	if (!p) return 0;  /* return 0 on failure */

	this_port=p;
	printk("%s: Accelent IDP parallel port registered.\n", p->name);
	parport_proc_register(p);
	parport_announce_port(p);

	return 1;
}

#ifdef MODULE

MODULE_AUTHOR("Rich Dulabahn");
MODULE_DESCRIPTION("Parport Driver for Accelent IDP");
MODULE_SUPPORTED_DEVICE("Accelent IDP builtin Parallel Port");
MODULE_LICENSE("GPL");

int
init_module(void)
{
	return parport_idp_init() ? 0 : -ENODEV;
}

void
cleanup_module(void)
{
	parport_proc_unregister(this_port);
	parport_unregister_port(this_port);
}
#endif

