/*
 * inclue/asm-arm/arch-ixp2000/ixmb2400.h
 *
 * Register and other defines for IXDP2400
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#ifndef _IXDP2400_H_
#define _IXDP2400_H_


/*
 * On board CPLD memory map
 */
#define IXDP2400_PHY_CPLD_BASE	0xc7000000
#define IXDP2400_VIRT_CPLD_BASE	0xd4000000
#define IXDP2400_CPLD_SIZE	0x01000000


#define IXDP2400_CPLD_REG(reg)  	(volatile unsigned long *)(IXDP2400_VIRT_CPLD_BASE | reg)

#define IXDP2400_CPLD_SYSLED   		IXDP2400_CPLD_REG(0x0)  
#define IXDP2400_CPLD_DISP_DATA        	IXDP2400_CPLD_REG(0x4)
#define IXDP2400_CPLD_CLOCK_SPEED      	IXDP2400_CPLD_REG(0x8)
#define IXDP2400_CPLD_INT              	IXDP2400_CPLD_REG(0xc)
#define IXDP2400_CPLD_REV              	IXDP2400_CPLD_REG(0x10)
#define IXDP2400_CPLD_SYS_CLK_M        	IXDP2400_CPLD_REG(0x14)
#define IXDP2400_CPLD_SYS_CLK_N        	IXDP2400_CPLD_REG(0x18)
#define IXDP2400_CPLD_INT_MASK         	IXDP2400_CPLD_REG(0x48)

/* 
 * Interrupt register mask bits on CPLD register 
 */
#define IXDP2400_MASK_INGRESS		(1<<0)
#define IXDP2400_MASK_EGRESS_NIC	(1<<1)   /*Master Ethernet */
#define IXDP2400_MASK_MEDIA_PCI		(1<<2) 	 /* media PCI interrupt */
#define IXDP2400_MASK_MEDIA_SP		(1<<3)   /* media slow port interrupt*/
#define IXDP2400_MASK_SF_PCI		(1<<4)   /* Sw Fabric PCI intrpt */
#define IXDP2400_MASK_SF_SP		(1<<5)   /* SW Fabric SlowPort intrpt */
#define IXDP2400_MASK_PMC		(1<<6)
#define IXDP2400_MASK_TVM		(1<<7)

#define	IXDP2400_GPIO_I2C_ENABLE	0x02
#define	IXDP2400_GPIO_SCL		0x07
#define	IXDP2400_GPIO_SDA		0x06

#endif /*_IXDP2400_H_ */
