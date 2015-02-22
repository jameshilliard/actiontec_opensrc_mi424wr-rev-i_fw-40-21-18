/*
 *  linux/drivers/ssi/clps711x_ssi1.c
 *
 * SSI bus driver for the CLPS711x SSI1 bus.  We support EP7212
 * extensions as well.
 *
 * Frame sizes can be between 4 and 24 bits.
 * Config sizes can be between 4 and 16 bits.
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/hardware/ep7212.h>

#include "ssi_bus.h"
#include "ssi_dev.h"

#define EP7212

/*
 * Port E on the P720T is used for the SPI bus chip selects
 *  0 - Keyboard
 *  1 - Touch screen
 *  2 - CS4224 ADC/DAC
 *  7 - idle
 */

#if 0
/*
 * we place in the transmit buffer:
 *  <control>
 * received data (binary):
 *  0xxxxxxxxxxxx000
 * where 'x' is the value
 */
struct ssi_dev ads7846_dev = {
	name:		"ADS7846",
	id:		1,
	proto:		SSI_SPI,
	cfglen:		8,
	framelen:	24,
	clkpol:		0,
	clkfreq:	2500000,
};

/*
 * we place in the transmit buffer:
 *  write: <20> <map> <data>...
 * received data discarded
 */
struct ssi_dev cs4224_dev = {
	name:		"CS4224",
	id:		2,
	proto:		SSI_SPI,
	cfglen:		8,
	framelen:	8,
	clkpol:		0,
	clkfreq:	6000000,
};
#endif

/*
 * Supplement with whatever method your board requires
 */
static void ssi1_select_id(int id)
{
	if (machine_is_p720t()) {
		clps_writel(7, PEDDR);
		clps_writel(id, PEDR);
	}
}

/*
 * Select the specified device.  The upper layers will have already
 * checked that the bus transmit queue is idle.  We need to make sure
 * that the interface itself is idle.
 */
static int ssi1_select(struct ssi_bus *bus, struct ssi_dev *dev)
{
	u_int id = dev ? dev->id : 7;
	u_int val;

	/*
	 * Make sure that the interface is idle
	 */
	do {
		val = clps_readl(SYSFLG1);
	} while (val & SYSFLG1_SSIBUSY);

	ssi1_select_id(7);

	if (dev) {
		/*
		 * Select clock frequency.  This is very rough,
		 * and assumes that we're operating in PLL mode.
		 */
		val = clps_readl(SYSCON1) & ~SYSCON1_ADCKSEL_MASK;
//		if (dev->clkfreq <= 16000)		/* <16kHz */
//			val |= SYSCON1_ADCKSEL(0);
//		else if (dev->clkfreq < 64000)		/* <64kHz */
//			val |= SYSCON1_ADCKSEL(1);
//		else if (dev->clkfreq < 128000)		/* <128kHz */
			val |= SYSCON1_ADCKSEL(2);
//		else					/* >= 128kHz */
//			val |= SYSCON1_ADCKSEL(3);
		clps_writel(val, SYSCON1);

		bus->cfglen	= dev->cfglen;
		bus->framelen	= dev->framelen;
		bus->clkpol	= dev->clkpol;
		bus->proto	= dev->proto;

#ifdef EP7212
		/*
		 * Set the clock edge according to the device.
		 * (set clkpol if the device reads data on the
		 * falling edge of the clock signal).
		 */
		val = ep_readl(SYSCON3) & ~SYSCON3_ADCCKNSEN;
		if (bus->clkpol && dev->proto != SSI_USAR)
			val |= SYSCON3_ADCCKNSEN;
		ep_writel(val, SYSCON3);
#endif

		/*
		 * Select the device
		 */
		ssi1_select_id(id);

#ifdef EP7212
		/*
		 * If we are doing USAR, wait 30us, then set
		 * the clock line low.
		 */
		if (dev->proto == SSI_USAR) {
			udelay(150);

			val |= SYSCON3_ADCCKNSEN;
			ep_writel(val, SYSCON3);
		}
#endif
	}

	return 0;
}

static void ssi1_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct ssi_bus *bus = (struct ssi_bus *)dev_id;

	/*
	 * Read the data word and queue it.
	 */
	ssi_core_rcv(bus, clps_readl(SYNCIO));
}

/*
 * Enable transmission and/or of some bytes
 */
static int ssi1_trans(struct ssi_bus *bus, u_int data)
{
	u_int syncio;

#ifdef EP7212
	data <<= 32 - bus->cfglen;
	syncio = bus->cfglen | bus->framelen << 7 | data;
#else
	syncio = data | bus->framelen << 8;
#endif

	clps_writel(syncio, SYNCIO);
	clps_writel(syncio | SYNCIO_TXFRMEN, SYNCIO);
	return 0;
}

/*
 * Initialise the SSI bus.
 */
static int ssi1_bus_init(struct ssi_bus *bus)
{
	int retval, val;

	retval = request_irq(IRQ_SSEOTI, ssi1_int, 0, "ssi1", bus);
	if (retval)
		return retval;

#ifdef EP7212
	/*
	 * EP7212 only!  Set the configuration command length.
	 * On the CLPS711x chips, it is fixed at 8 bits.
	 */
	val = ep_readl(SYSCON3);
	val |= SYSCON3_ADCCON;
	ep_writel(val, SYSCON3);
#endif

	ssi1_select(bus, NULL);

	PLD_SPI |= PLD_SPI_EN;

	return 0;
}

static void ssi1_bus_exit(struct ssi_bus *bus)
{
	ssi1_select(bus, NULL);

	PLD_SPI &= ~PLD_SPI_EN;

	free_irq(IRQ_SSEOTI, bus);
}

struct ssi_bus clps711x_ssi1_bus = {
	name:	"clps711x_ssi1",
	init:	ssi1_bus_init,
	exit:	ssi1_bus_exit,
	select:	ssi1_select,
	trans:	ssi1_trans,
};

static int __init clps711x_ssi1_init(void)
{
	return ssi_register_bus(&clps711x_ssi1_bus);
}

static void __exit clps711x_ssi1_exit(void)
{
	ssi_unregister_bus(&clps711x_ssi1_bus);
}

module_init(clps711x_ssi1_init);
module_exit(clps711x_ssi1_exit);
