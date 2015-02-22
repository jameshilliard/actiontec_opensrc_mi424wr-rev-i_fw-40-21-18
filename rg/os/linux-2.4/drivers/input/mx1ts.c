/*
 *  linux/drivers/misc/mx1ts.c
 *
 *  Copyright (C) 2003 Blue Mug, Inc. for Motorola, Inc.
 *
 *  Cloned from ucb1x00_ts.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/pm.h>

#include <asm/dma.h>

#include <linux/input.h>

#include "mx1ts.h"

#define DEV_IRQ_ID	"mx1-ts"

struct mx1_ts {
	struct input_dev	idev;
#ifdef CONFIG_PM
	struct pm_dev		*pmdev;
#endif

	wait_queue_head_t	irq_wait;
	struct completion	init_exit;
	int			use_count;
	u16			x_res;
	u16			y_res;

	int			restart:1;
};

static struct mx1_ts mx1ts;
static u8 mx1_performing_auto_calibration = 0;
static u16 mx1_cal_auto_zero = 0;
static u16 mx1_cal_range_x = 0;
static u16 mx1_cal_range_y = 0;

static int mx1_ts_startup(struct mx1_ts *ts);
static void mx1_ts_shutdown(struct mx1_ts *ts);

static void mx1_ts_pendata_int(int irq, void *dev_id, struct pt_regs *regs);
static void mx1_ts_touch_int(int irq, void *dev_id, struct pt_regs *regs);
static void mx1_ts_compare_int(int irq, void *dev_id, struct pt_regs *regs);

static void mx1_ts_enable_pen_touch_interrupt(void);
static void mx1_ts_disable_pen_touch_interrupt(void);
static void mx1_ts_enable_pen_up_interrupt(void);
static void mx1_ts_disable_pen_up_interrupt(void);
static void mx1_ts_enable_auto_sample(void);
static void mx1_ts_disable_auto_sample(void);
static void mx1_ts_start_auto_calibration(void);

static inline void mx1_reg_write(unsigned int reg, unsigned int val)
{
	*((volatile unsigned int *)reg) = val;
}

static inline unsigned int mx1_reg_read(unsigned int reg)
{
	return *((volatile unsigned int *)reg);
}

static inline void mx1_reg_clear_bit(unsigned int reg, unsigned int bit)
{
	*((volatile unsigned int *)reg) &= ~bit;
}

static inline void mx1_reg_set_bit(unsigned int reg, unsigned int bit)
{
	*((volatile unsigned int *)reg) |= bit;
}

static inline void mx1_ts_evt_add(struct mx1_ts *ts, u16 pressure, u16 x, u16 y)
{
	input_report_abs(&ts->idev, ABS_X, (int)x - 32768);
	input_report_abs(&ts->idev, ABS_Y, (int)y - 32768);
	input_report_abs(&ts->idev, ABS_PRESSURE, (int)pressure);
}

static inline void mx1_ts_flush_fifo(void)
{
	int i;
	for (i = 0; i < 12; i++)
		if (mx1_reg_read(ASP_ISTATR) & (ASP_PFF | ASP_PDR))
			mx1_reg_read(ASP_PADFIFO);
}

static int mx1_ts_open(struct input_dev *idev)
{
	struct mx1_ts *ts = (struct mx1_ts *)idev;

	mx1_performing_auto_calibration = 0;
	return mx1_ts_startup(ts);
}

static void mx1_ts_close(struct input_dev *idev)
{
	struct mx1_ts *ts = (struct mx1_ts *)idev;

	mx1_ts_shutdown(ts);
}

static inline int mx1_ts_enable_irqs(void)
{
	int result;

	result = request_irq(ASP_PENDATA_IRQ,
			     mx1_ts_pendata_int,
			     SA_INTERRUPT,
			     DEV_IRQ_ID,
			     DEV_IRQ_ID);
	if (result) {
		printk("Couldn't request pen data IRQ.\n");
		return result;
	}

	result = request_irq(ASP_TOUCH_IRQ,
			     mx1_ts_touch_int,
			     SA_INTERRUPT,
			     DEV_IRQ_ID,
			     DEV_IRQ_ID);
	if (result) {
		printk("Couldn't request pen touch IRQ.\n");
		free_irq(ASP_PENDATA_IRQ, DEV_IRQ_ID);
		return result;
	}

	return result;
}

static inline int mx1_ts_disable_irqs(void)
{
	free_irq(ASP_PENDATA_IRQ, DEV_IRQ_ID);
	free_irq(ASP_TOUCH_IRQ, DEV_IRQ_ID);

	return 0;
}

static inline int mx1_ts_register(struct mx1_ts *ts)
{
	ts->idev.name      = "Touchscreen panel";
	ts->idev.open      = mx1_ts_open;
	ts->idev.close     = mx1_ts_close;

	__set_bit(EV_ABS, ts->idev.evbit);
	__set_bit(ABS_X, ts->idev.absbit);
	__set_bit(ABS_Y, ts->idev.absbit);
	__set_bit(ABS_PRESSURE, ts->idev.absbit);

	ts->idev.absmin[ABS_X] = 0;
	ts->idev.absmax[ABS_X] = (u32)0x0000FFFF;
	ts->idev.absfuzz[ABS_X] = 50;
	ts->idev.absflat[ABS_X] = 0;

	ts->idev.absmin[ABS_Y] = 0;
	ts->idev.absmax[ABS_Y] = (u32)0x0000FFFF;
	ts->idev.absfuzz[ABS_Y] = 50;
	ts->idev.absflat[ABS_Y] = 0;

	input_register_device(&ts->idev);

	return 0;
}

static inline void mx1_ts_deregister(struct mx1_ts *ts)
{
	input_unregister_device(&ts->idev);
}

/*
 * Handle the touch interrupt, generated when the pen is pressed/
 * released.
 */
static void mx1_ts_touch_int(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Clear the interrupt. */
	mx1_reg_set_bit(ASP_ISTATR, ASP_PEN);

	mx1_ts_disable_pen_touch_interrupt();
	mx1_ts_start_auto_calibration();
	mx1_ts_enable_pen_up_interrupt();
}

/*
 * Handle the pen data ready interrupt, generated when pen data is
 * in the FIFO.
 */
static void mx1_ts_pendata_int(int irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned int auto_zero, pen_x, pen_y, pen_u;

	if (mx1_reg_read(ASP_ISTATR) & 0x400) {
		mx1_reg_set_bit(ASP_ISTATR, 0x400);

		mx1_ts_disable_auto_sample();
		mx1_ts_disable_pen_up_interrupt();
		mx1_ts_enable_pen_touch_interrupt();

		mx1_ts_evt_add(&mx1ts, 0, pen_x, pen_y);

		mx1_ts_flush_fifo();

		return;
	}

	if (mx1_performing_auto_calibration) {
		unsigned int value;

		mx1_cal_auto_zero = mx1_reg_read(ASP_PADFIFO) & 0xFFFF;
		mx1_cal_range_x = mx1_reg_read(ASP_PADFIFO) & 0xFFFF;
		mx1_cal_range_y = mx1_reg_read(ASP_PADFIFO) & 0xFFFF;

		if ((mx1_cal_auto_zero >= mx1_cal_range_x) ||
		    (mx1_cal_auto_zero >= mx1_cal_range_y)) {
			/* Invalid data. */
			mx1_ts_start_auto_calibration();
			return;
		}

		mx1_cal_range_x -= mx1_cal_auto_zero;
		mx1_cal_range_y -= mx1_cal_auto_zero;

		value = mx1_reg_read(ASP_ACNTLCR);
		value &= ~0x04000000; /* XXX Undocumented. */
		mx1_reg_write(ASP_ACNTLCR, value);

		mx1_performing_auto_calibration = 0;

		mx1_ts_enable_auto_sample();
	} else {
		/* There could be more than one sample in the FIFO, but we're
		 * only going to read one per call. The interrupt will be
		 * generated as long as there is data in the FIFO. */

		if ((mx1_reg_read(ASP_ISTATR) & ASP_PDR) != ASP_PDR) {
			return;
		}

		auto_zero = mx1_reg_read(ASP_PADFIFO);
		if (auto_zero > (mx1_cal_auto_zero + 0x200)) {
			return;
		}

		pen_x = mx1_reg_read(ASP_PADFIFO);
		pen_y = mx1_reg_read(ASP_PADFIFO);
		pen_u = mx1_reg_read(ASP_PADFIFO);

		pen_x = (u32)(((pen_x - mx1_cal_auto_zero) << 16) /
			      mx1_cal_range_x);
		pen_y = (u32)(((pen_y - mx1_cal_auto_zero) << 16) /
			      mx1_cal_range_y);

		mx1_ts_evt_add(&mx1ts, pen_u, pen_x, pen_y);
	}
}

static void mx1_ts_reset_asp(void)
{
	unsigned int value;

	mx1_ts_flush_fifo();

	/* Soft reset the ASP module */
        mx1_reg_write(ASP_ACNTLCR, ASP_SWRST);

	/* Read back the reset value of the control register */
	value = mx1_reg_read(ASP_ACNTLCR);

	/* Enable the clock and wait for a short while */
	value |= ASP_CLKEN;
        mx1_reg_write(ASP_ACNTLCR, value);
	udelay(100);

	/* Set the value of the conrtol register. */
	value = ASP_CLKEN | ASP_NM | ASP_SW6 | ASP_BGE;
        mx1_reg_write(ASP_ACNTLCR, value);

	/* Set the clock divide ratio to 2. */
	mx1_reg_write(ASP_CLKDIV, 0x01);

	/* Set the sample rate control register. These values should yield
         * about 150 samples per second, which seems to give good smooth
         * lines. */
	value = (0x2 << ASP_DMCNT_SCALE) | 	/* Decimation ratio is 3 */
		(0x1 << ASP_IDLECNT_SCALE) | 	/* Idle count is 1 clock */
		(0x2 << ASP_DSCNT_SCALE);	/* Data setup is 2 clocks */
	mx1_reg_write(ASP_PSMPLRG, value);

	/* Disable the compare function. */
	mx1_reg_write(ASP_CMPCNTL, 0);
}

static void mx1_ts_enable_auto_sample(void)
{
	unsigned int value;

	mx1_ts_flush_fifo();

	value = mx1_reg_read(ASP_ACNTLCR);

	/* Set the mode to X then Y */
	value &= ~ASP_MODE_MASK;
	value |= ASP_MODE_ONLY_Y;

	/* Enable auto zero. */
	value |= ASP_AZE;

	/* Enable auto sample. */
	value |= ASP_AUTO;

	/* Enable pen A/D. */
	value |= ASP_PADE;
	mx1_reg_write(ASP_ACNTLCR, value);

	/* Enable pen data ready and full interrupt. */
	value = mx1_reg_read(ASP_ICNTLR);
	value |= ASP_PFFE | ASP_PDRE;
	mx1_reg_write(ASP_ICNTLR, value);
}

static void mx1_ts_disable_auto_sample(void)
{
	unsigned int value;

	value = mx1_reg_read(ASP_ACNTLCR);

	/* Set the mode to none */
	value &= ~ASP_MODE_MASK;

	/* Disable auto zero. */
	value &= ~ASP_AZE;

	/* Disable auto sample. */
	value &= ~ASP_AUTO;

	/* Disable pen A/D. */
	value &= ~ASP_PADE;
	mx1_reg_write(ASP_ACNTLCR, value);

	/* Disable pen data ready and full interrupt. */
	value = mx1_reg_read(ASP_ICNTLR);
	value &= ~(ASP_PFFE | ASP_PDRE);
	mx1_reg_write(ASP_ICNTLR, value);
}

static void mx1_ts_enable_pen_touch_interrupt(void)
{
	unsigned int value;

	/* Enable pen touch interrupt. */
	value = mx1_reg_read(ASP_ICNTLR);
	value |= ASP_EDGE | ASP_PIRQE;
	mx1_reg_write(ASP_ICNTLR, value);
}

static void mx1_ts_disable_pen_touch_interrupt(void)
{
	unsigned int value;

	/* Enable pen touch interrupt. */
	value = mx1_reg_read(ASP_ICNTLR);
	value &= ~ASP_PIRQE;
	mx1_reg_write(ASP_ICNTLR, value);
}

static void mx1_ts_enable_pen_up_interrupt(void)
{
	unsigned int value;

	/* Enable pen up interrupt. XXX: This feature is undocumented. */
	value = mx1_reg_read(ASP_ICNTLR);
	value |= ASP_PUPE;
	mx1_reg_write(ASP_ICNTLR, value);
}

static void mx1_ts_disable_pen_up_interrupt(void)
{
	unsigned int value;

	/* Enable pen up interrupt. XXX: This feature is undocumented. */
	value = mx1_reg_read(ASP_ICNTLR);
	value &= ~ASP_PUPE;
	mx1_reg_write(ASP_ICNTLR, value);
}

static void mx1_ts_start_auto_calibration(void)
{
	unsigned int value;

	mx1_performing_auto_calibration = 1;

	value = mx1_reg_read(ASP_ACNTLCR);

	/* Set the mode to X then Y */
	value &= ~ASP_MODE_MASK;
	value |= ASP_MODE_ONLY_X;

	/* Enable auto zero. */
	value |= ASP_AZE;

	/* Enable auto calibrate. XXX: Undocumented bitfield. */
	value |= 0x04000000;

	/* Enable auto sample. */
	value |= ASP_AUTO;

	/* Enable pen A/D. */
	value |= ASP_PADE;
	mx1_reg_write(ASP_ACNTLCR, value);

	/* Enable pen data ready and full interrupt. */
	value = mx1_reg_read(ASP_ICNTLR);
	value |= ASP_PFFE | ASP_PDRE | ASP_PUPE;
	mx1_reg_write(ASP_ICNTLR, value);
}

static int mx1_ts_startup(struct mx1_ts *ts)
{
	int ret = 0;

	if (ts->use_count++ != 0)
		goto out;

        /*
         * Reset the ASP.
         */
        mx1_ts_reset_asp();


	/*
	 * XXX: Figure out if we need this...
	 * If we do this at all, we should allow the user to
	 * measure and read the X and Y resistance at any time.
	 */
	//ts->x_res = mx1_ts_read_xres(ts);
	//ts->y_res = mx1_ts_read_yres(ts);

	mx1_ts_enable_pen_touch_interrupt();

 out:
	if (ret)
		ts->use_count--;
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static void mx1_ts_shutdown(struct mx1_ts *ts)
{
	if (--ts->use_count == 0) {
		unsigned int value;

		/* Turn off the ADC and associated circuitry. */
		value = mx1_reg_read(ASP_ACNTLCR);
		value &= !(ASP_CLKEN | ASP_PADE | ASP_BGE);
		mx1_reg_write(ASP_ACNTLCR, value);
	}
}

/*
 * Initialization.
 */
static int __init mx1_ts_init(void)
{
	int ret = 0;
	struct mx1_ts *ts = &mx1ts;

	mx1_ts_reset_asp();

	/*
	 * Enable the IRQ's
	 */
	if ((ret = mx1_ts_enable_irqs()))
		return ret;

	return mx1_ts_register(ts);
}

static void __exit mx1_ts_exit(void)
{
	struct mx1_ts *ts = &mx1ts;

	mx1_ts_disable_irqs();
	mx1_ts_deregister(ts);
}

module_init(mx1_ts_init);
module_exit(mx1_ts_exit);

MODULE_AUTHOR("Jon McClintock <jonm@bluemug.com>");
MODULE_DESCRIPTION("MX1 touchscreen driver");
MODULE_LICENSE("GPL");
