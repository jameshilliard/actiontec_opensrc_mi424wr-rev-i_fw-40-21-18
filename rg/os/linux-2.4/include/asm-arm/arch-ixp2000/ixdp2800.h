/*
 * inclue/asm-arm/arch-ixp2000/ixdp2800.h
 *
 * Register and other defines for IXDP2800
 *
 * Author: Jeff Daly <jeffrey.daly@intel.com>
 *
 * Copyright 2002-2003 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#ifndef _IXDP2800_H_
#define _IXDP2800_H_

/* CPLD interrupt register definition */
#define IXDP2800_PHY_CPLD_BASE		0xc7000000
#define IXDP2800_VIRT_CPLD_BASE		0xd4000000
#define IXDP2800_CPLD_SIZE		0x01000000

#define IXDP2800_CPLD			IXDP2800_VIRT_CPLD_BASE

#define IXDP2800_CPLD_REG(reg)		(volatile u32 *)(IXDP2800_CPLD | reg)

#define IXDP2800_CPLD_INT		IXDP2800_CPLD_REG(0x0)
#define IXDP2800_CPLD_INT_MASK		IXDP2800_CPLD_REG(0x140)

/* 
 *  * Interrupt register mask bits on CPLD register 
 *   */
#define IXDP2800_MASK_EGRESS_NIC	(1<<0)	/*Master Ethernet */
#define IXDP2800_MASK_INGRESS		(1<<1)
#define IXDP2800_MASK_PMC		(1<<2)
#define IXDP2800_MASK_FABRIC_PCI	(1<<3)	/* Sw Fabric PCI intrpt */
#define IXDP2800_MASK_FABRIC_SP		(1<<4)	/* SW Fabric SlowPort intrpt */
#define IXDP2800_MASK_MEDIA		(1<<5)	/* media slow port interrupt */

/* LED defines */
#define IXDP2800_LED0_REG		IXDP2800_CPLD_REG(0x10c)
#define IXDP2800_LED1_REG		IXDP2800_CPLD_REG(0x108)
#define IXDP2800_LED2_REG		IXDP2800_CPLD_REG(0x104)
#define IXDP2800_LED3_REG		IXDP2800_CPLD_REG(0x100)

#define IXDP2800_GPIO_I2C_ENABLE	0x05
#define IXDP2800_GPIO_SCL		0x07
#define IXDP2800_GPIO_SDA		0x06

#endif /*_IXDP2800_H_ */
