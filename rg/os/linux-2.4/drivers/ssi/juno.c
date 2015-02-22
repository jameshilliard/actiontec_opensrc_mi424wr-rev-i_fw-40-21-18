#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/syspld.h>

#include "ssi_bus.h"
#include "ssi_dev.h"

extern struct ssi_bus clps711x_ssi1_bus;

static u_int recvbuf[16];
static volatile u_int ptr, rxed;

static inline void juno_enable_irq(void)
{
	enable_irq(IRQ_EINT1);
}

static inline void juno_disable_irq(void)
{
	disable_irq(IRQ_EINT1);
}

static void juno_rcv(struct ssi_dev *dev, u_int data)
{
	if (ptr < 16) {
		recvbuf[ptr] = data;
		ptr++;
	} else
		printk("juno_rcv: %04x\n", data);
	rxed = 1;
}

static void juno_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct ssi_dev *dev = dev_id;

	printk("juno_irq\n");

	ssi_select_device(dev->bus, dev);

	ptr = 0;
	do {
		rxed = 0;
		ssi_transmit_data(dev, 0xff);
		while (rxed == 0);
		udelay(150);
	} while (PLD_INT & PLD_INT_KBD_ATN);

	ssi_select_device(dev->bus, NULL);

	{ int i;
	  printk("juno_rcv: ");
	  for (i = 0; i < ptr; i++)
	  	printk("%04x ", recvbuf[i]);
	  printk("\n");
	}
}

static void juno_command(struct ssi_dev *dev, int cmd, int data)
{
	ssi_transmit_data(dev, cmd);
	mdelay(1);
	ssi_transmit_data(dev, data);
	mdelay(1);
	ssi_transmit_data(dev, 0xa0 ^ 0xc0);
	mdelay(1);
}

static int juno_dev_init(struct ssi_dev *dev)
{
	int retval;

	PLD_KBD |= PLD_KBD_EN;
	ptr = 16;

	mdelay(20);

	retval = request_irq(IRQ_EINT1, juno_irq, 0, dev->name, dev);
	if (retval)
		return retval;

	juno_disable_irq();

	if (ssi_select_device(dev->bus, dev) != 0) {
		printk("juno: ssi_select_dev failed\n");
		return -EBUSY;
	}

	mdelay(1);

	juno_command(dev, 0x80, 0x20);

	ssi_select_device(dev->bus, NULL);

	juno_enable_irq();

	return 0;
}

static struct ssi_dev juno_dev = {
	name:		"Juno",
	id:		0,
	proto:		SSI_USAR,
	cfglen:		8,
	framelen:	8,
	clkpol:		1,
	clkfreq:	250000,
	rcv:		juno_rcv,
	init:		juno_dev_init,
};

static int __init juno_init(void)
{
	return ssi_register_device(&clps711x_ssi1_bus, &juno_dev);
}

static void __exit juno_exit(void)
{
	ssi_unregister_device(&juno_dev);
}

module_init(juno_init);
module_exit(juno_exit);

