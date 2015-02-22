/*
 * ds1307.h
 *
 * Copyright (C) 2002 Intrinsyc Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef DS1307_H
#define DS1307_H

#if defined(CONFIG_PXA_EMERSON_SBC) || defined(CONFIG_PXA_CERF_BOARD)
	#define DS1307_I2C_SLAVE_ADDR	0x68
#else
	#define DS1307_I2C_SLAVE_ADDR	0xffff
#endif

#define DS1307_RAM_ADDR_START	0x08
#define DS1307_RAM_ADDR_END	0x3F
#define DS1307_RAM_SIZE 0x40

#define PROC_DS1307_NAME	"driver/ds1307"

struct rtc_mem {
	unsigned int	loc;
	unsigned int	nr;
	unsigned char	*data;
};

#define DS1307_GETDATETIME	0
#define DS1307_SETTIME		1
#define DS1307_SETDATETIME	2
#define DS1307_GETCTRL		3
#define DS1307_SETCTRL		4
#define DS1307_MEM_READ		5
#define DS1307_MEM_WRITE	6

#define SQW_ENABLE	0x10	/* Square Wave Enable */
#define SQW_DISABLE	0x00	/* Square Wave disable */

#define RATE_32768HZ	0x03	/* Rate Select 32.768KHz */
#define RATE_8192HZ	0x02	/* Rate Select 8.192KHz */
#define RATE_4096HZ	0x01	/* Rate Select 4.096KHz */
#define RATE_1HZ	0x00	/* Rate Select 1Hz */

#define CLOCK_HALT	0x80	/* Clock Halt */

#define BCD_TO_BIN(val) (((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val) ((((val)/10)<<4) + (val)%10)

#define TWELVE_HOUR_MODE(n)	(((n)>>6)&1)
#define HOURS_AP(n)		(((n)>>5)&1)
#define HOURS_12(n)		BCD_TO_BIN((n)&0x1F)
#define HOURS_24(n)		BCD_TO_BIN((n)&0x3F)

#endif
