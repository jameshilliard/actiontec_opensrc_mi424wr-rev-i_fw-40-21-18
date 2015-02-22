/*
 * linux/include/asm-arm/arch-ixp2000/irqs.h
 *
 * Original Author:	Naeem Afzal <naeem.m.afzal@intel.com>
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright:	(C) 2002 Intel Corp.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IRQS_H
#define _IRQS_H

/*
 * Some interrupt numbers go unused b/c the IRQ mask/ummask/status
 * register has those bit reserved. We just mark those interrupts
 * as invalid and this allows us to do mask/unmask with a single
 * shit operation instead of having to map the IRQ number to
 * a HW IRQ number.
 */
#define	IRQ_IXP2000_SWI			0 /* soft interrupt */
#define	IRQ_IXP2000_ERRSUM		1 /* OR of all bits in ErrorStatus reg*/
#define	IRQ_IXP2000_UART		2
#define	IRQ_IXP2000_GPIO		3
#define	IRQ_IXP2000_TIMER1     		4
#define	IRQ_IXP2000_TIMER2     		5
#define	IRQ_IXP2000_TIMER3     		6
#define	IRQ_IXP2000_TIMER4     		7
#define	IRQ_IXP2000_PMU        		8               
#define	IRQ_IXP2000_SPF        		9  /* Slow port framer IRQ */
#define	IRQ_IXP2000_DMA1      		10
#define	IRQ_IXP2000_DMA2      		11
#define	IRQ_IXP2000_DMA3      		12
#define	IRQ_IXP2000_PCI_DOORBELL	13
#define	IRQ_IXP2000_ME_ATTN       	14
#define IRQ_IXP2000_PCI			15
#define IRQ_IXP2000_THDA0		16
#define IRQ_IXP2000_THDA1		17
#define IRQ_IXP2000_THDA2		18
#define IRQ_IXP2000_THDA3		19
#define IRQ_IXP2000_THDB0		24
#define IRQ_IXP2000_THDB1		25
#define IRQ_IXP2000_THDB2		26
#define IRQ_IXP2000_THDB3		27

/* define generic GPIOs */
#define IRQ_IXP2000_GPIO0               32
#define IRQ_IXP2000_GPIO1               33
#define IRQ_IXP2000_GPIO2               34
#define IRQ_IXP2000_GPIO3               35
#define IRQ_IXP2000_GPIO4               36
#define IRQ_IXP2000_GPIO5               37
#define IRQ_IXP2000_GPIO6               38
#define IRQ_IXP2000_GPIO7               39

/* split off the 2 PCI sources */
#define IRQ_IXP2000_PCIA                40
#define IRQ_IXP2000_PCIB                41

/* Int sources from IRQ_ERROR_STATUS */
#define IRQ_IXP2000_DRAM0_MIN_ERR       42
#define IRQ_IXP2000_DRAM0_MAJ_ERR       43
#define IRQ_IXP2000_DRAM1_MIN_ERR       44
#define IRQ_IXP2000_DRAM1_MAJ_ERR       45
#define IRQ_IXP2000_DRAM2_MIN_ERR       46
#define IRQ_IXP2000_DRAM2_MAJ_ERR       47
#define IRQ_IXP2000_SRAM0_ERR           48
#define IRQ_IXP2000_SRAM1_ERR           49
#define IRQ_IXP2000_SRAM2_ERR           50
#define IRQ_IXP2000_SRAM3_ERR           51
#define IRQ_IXP2000_MEDIA_ERR           52
#define IRQ_IXP2000_PCI_ERR             53
#define IRQ_IXP2000_SP_INT              54

#define NR_IXP2000_IRQS                 55

#if defined(CONFIG_ARCH_IXDP2400)
/* 
 * IXDP2400 specific IRQs
 */
#define	IRQ_IXDP2400(x)			(NR_IXP2000_IRQS + (x))

//#define	IRQ_IXDP2400_SETHERNET		IRQ_IXDP2400(0) /* Slave NPU NIC irq */
#define	IRQ_IXDP2400_INGRESS_NPU	IRQ_IXDP2400(0) /* Slave NPU irq */
#define	IRQ_IXDP2400_METHERNET		IRQ_IXDP2400(1) /* Master NPU NIC irq */
#define	IRQ_IXDP2400_MEDIA_PCI		IRQ_IXDP2400(2) /* Media on PCI irq */
#define	IRQ_IXDP2400_MEDIA_SP		IRQ_IXDP2400(3) /* Media on SlowPort */
#define	IRQ_IXDP2400_SF_PCI		IRQ_IXDP2400(4) /* Sw Fab. on PCI */
#define	IRQ_IXDP2400_SF_SP		IRQ_IXDP2400(5) /* Switch Fab on SP */
#define	IRQ_IXDP2400_PMC		IRQ_IXDP2400(6) /* PMC slot ineterrupt*/
#define	IRQ_IXDP2400_TVM		IRQ_IXDP2400(7) /* Temp & Voltage */

#define	IRQ_IXP2000_INTERRUPT   ((IRQ_IXDP2400_TVM)+1)  
#define NR_IXDP2400_IRQS	(IRQ_IXDP2400_TVM+1)

#elif defined(CONFIG_ARCH_IXDP2800)
/* IXDP2800 specific IRQs */
#define IRQ_IXDP2800(x)			(NR_IXP2000_IRQS + (x))
#define IRQ_IXDP2800_EGRESS_NIC		IRQ_IXDP2800(0)
#define IRQ_IXDP2800_INGRESS_NPU	IRQ_IXDP2800(1)
#define IRQ_IXDP2800_PMC_PCI		IRQ_IXDP2800(2)
#define IRQ_IXDP2800_FABRIC_PCI		IRQ_IXDP2800(3)
#define IRQ_IXDP2800_FABRIC		IRQ_IXDP2800(4)
#define IRQ_IXDP2800_MEDIA		IRQ_IXDP2800(5)

#define IRQ_IXP2000_INTERRUPT	(IRQ_IXDP2800_MEDIA)
#define NR_IXDP2800_IRQS	(IRQ_IXDP2800_MEDIA+1)
#endif /* CONFIG_ARCH_XXX */

#undef NR_IRQS
#define NR_IRQS ((IRQ_IXP2000_INTERRUPT)+1)

#endif /*_IRQS_H*/
