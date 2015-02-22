/*
 * L3 SA1111 algorithm/adapter module.
 *
 *  By Russell King,
 *    gratuitously ripped from sa1111-uda1341.c by John Dorsey.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/l3/l3.h>

#include <asm/hardware.h>
#include <asm/semaphore.h>
#include <asm/mach-types.h>
#include <asm/arch/assabet.h>
#include <asm/hardware/sa1111.h>

static inline unsigned char l3_sa1111_recv_byte(unsigned char addr)
{
	unsigned char dat;

	L3_CAR = addr;
	while ((SASR0 & SASR0_L3RD) == 0)
		mdelay(1);
	dat = L3_CDR;
	SASCR = SASCR_RDD;
	return dat;
}

static void l3_sa1111_recv_msg(struct l3_msg *msg)
{
	int len = msg->len;
	char *p = msg->buf;

	if (len > 1) {
		SACR1 |= SACR1_L3MB;
		while ((len--) > 1)
			*p++ = l3_sa1111_recv_byte(msg->addr);
	}
	SACR1 &= ~SACR1_L3MB;
	*p = l3_sa1111_recv_byte(msg->addr);
}

static inline void l3_sa1111_send_byte(unsigned char addr, unsigned char dat)
{
	L3_CAR = addr;
	L3_CDR = dat;
	while ((SASR0 & SASR0_L3WD) == 0)
		mdelay(1);
	SASCR = SASCR_DTS;
}

static void l3_sa1111_send_msg(struct l3_msg *msg)
{
	int len = msg->len;
	char *p = msg->buf;

	if (len > 1) {
		SACR1 |= SACR1_L3MB;
		while ((len--) > 1)
			l3_sa1111_send_byte(msg->addr, *p++);
	}
	SACR1 &= ~SACR1_L3MB;
	l3_sa1111_send_byte(msg->addr, *p);
}

static int l3_sa1111_xfer(struct l3_adapter *adap, struct l3_msg msgs[], int num)
{
	int i;

	for (i = 0; i < num; i++) {
		struct l3_msg *pmsg = &msgs[i];

		if (pmsg->flags & L3_M_RD)
			l3_sa1111_recv_msg(pmsg);
		else
			l3_sa1111_send_msg(pmsg);
	}

	return num;
}

static struct l3_algorithm l3_sa1111_algo = {
	name:		"L3 SA1111 algorithm",
	xfer:		l3_sa1111_xfer,
};

static DECLARE_MUTEX(sa1111_lock);

static struct l3_adapter l3_sa1111_adapter = {
	owner:		THIS_MODULE,
	name:		"l3-sa1111",
	algo:		&l3_sa1111_algo,
	lock:		&sa1111_lock,
};

static int __init l3_sa1111_init(void)
{
	int ret = -ENODEV;
	if ((machine_is_assabet() && machine_has_neponset()) ||
	    machine_is_jornada720() || machine_is_accelent_sa() ||
	    machine_is_badge4())
		ret = l3_add_adapter(&l3_sa1111_adapter);
	return ret;
}

static void __exit l3_sa1111_exit(void)
{
	l3_del_adapter(&l3_sa1111_adapter);
}

module_init(l3_sa1111_init);
module_exit(l3_sa1111_exit);
