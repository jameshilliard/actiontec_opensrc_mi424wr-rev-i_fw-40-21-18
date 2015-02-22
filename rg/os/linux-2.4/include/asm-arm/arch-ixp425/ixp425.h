/*
 * include/asm-arm/arch-ixp425/ixp425.h 
 *
 * Register definitions for IXP425 
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H__
#error "Do not include this directly, please #include <asm/hardware.h>"
#endif

#ifndef _ASM_ARM_IXP425_H_
#define _ASM_ARM_IXP425_H_

/* needed for CONFIG_ARCH_XXX */
#include <linux/autoconf.h>

#ifndef BIT    
#define BIT(bit)	(1 << (bit))
#endif

/*

IXP425 Memory map:

Phy		Phy Size	Map Size	Virt		Description
===========================================================================
	
0x00000000	0x10000000					SDRAM 1

0x10000000	0x10000000					SDRAM 2

0x20000000	0x10000000					SDRAM 3

0x30000000	0x10000000					SDRAM 4

0x48000000	 0x4000000					PCI Data

0x50000000	0x10000000					EXP BUS

0x60000000	 0x4000000	    0x4000	0xFF00F000	QMGR

0xC0000000	     0x100   	    0x1000	0xFF00C000	PCI CFG

0xC4000000	     0x100          0x1000	0xFF00D000	EXP CFG

0xC8000000	    0xC000          0xC000	0xFF000000	PERIPHERAL

0xCC000000	     0x100   	    0x1000	0xFF00E000	SDRAM CFG

*/


/*
 * SDRAM
 */
#define IXP425_SDRAM_BASE		(0x00000000)
#define IXP425_SDRAM_BASE_ALT		(0x10000000)

/* 
 * PCI Data
 */
#define IXP425_PCI_BASE_PHYS		(0x48000000)
#define IXP425_PCI_BASE_VIRT		(0xE8000000)
#define IXP425_PCI_REGION_SIZE		(0x04000000)

/* 
 * Q Manager space
 */
#define IXP425_QMGR_BASE_PHYS	(0x60000000)
#define IXP425_QMGR_BASE_VIRT	(0xFF00F000)
#define IXP425_QMGR_REGION_SIZE	(0x00004000)

/*
 * PCI Configuration space
 */
#define IXP425_PCI_CFG_BASE_PHYS	(0xC0000000)
#define IXP425_PCI_CFG_BASE_VIRT	(0xFF00C000)
#define IXP425_PCI_CFG_REGION_SIZE	(0x00001000)

/*
 * Expansion BUS Configuration space
 */
#define IXP425_EXP_CFG_BASE_PHYS	(0xC4000000)
#define IXP425_EXP_CFG_BASE_VIRT	(0xFF00D000)
#define IXP425_EXP_CFG_REGION_SIZE	(0x00001000)

/*
 * Peripheral space
 */
#define IXP425_PERIPHERAL_BASE_PHYS	(0xC8000000)
#define IXP425_PERIPHERAL_BASE_VIRT	(0xFF000000)
#define IXP425_PERIPHERAL_REGION_SIZE	(0x0000C000)

/*
 * SDRAM Config space
 */
#define IXP425_SDRAM_CFG_BASE_PHYS	(0xCC000000)

/* 
 * SDRAM Config registers
 */
#define IXP425_SDRAM_CFG_PHYS		(IXP425_SDRAM_CFG_BASE_PHYS + 0x0000)
#define IXP425_SDRAM_REFRESH_PHYS	(IXP425_SDRAM_CFG_BASE_PHYS + 0x0004)
#define IXP425_SDRAM_INSTRUCTION_PHYS	(IXP425_SDRAM_CFG_BASE_PHYS + 0x0008)

/* IXP425_SDRAM_CFG bits */
#define IXP425_SDRAM_CAS_3CLKS		(0x0008)
#define IXP425_SDRAM_CAS_2CLKS		(0x0000)

#define IXP425_SDRAM_32Meg_2Chip	(0x0000)
#define IXP425_SDRAM_64Meg_4Chip	(0x0001)
#define IXP425_SDRAM_64Meg_2Chip	(0x0002)
#define IXP425_SDRAM_128Meg_4Chip	(0x0003)
#define IXP425_SDRAM_128Meg_2Chip	(0x0004)
#define IXP425_SDRAM_256Meg_4Chip	(0x0005)

/* IXP425_SDRAM_REFRESH bits */
#define IXP425_SDRAM_REFRESH_DISABLE 	(0x0)

/* IXP425_SDRAM_INSTRUCTION bits */
#define IXP425_SDRAM_IR_MODE_SET_CAS2_CMD 	(0x0000)
#define IXP425_SDRAM_IR_MODE_SET_CAS3_CMD 	(0x0001)
#define IXP425_SDRAM_IR_PRECHARGE_ALL_CMD 	(0x0002)
#define IXP425_SDRAM_IR_NOP_CMD 		(0x0003)
#define IXP425_SDRAM_IR_AUTOREFRESH_CMD 	(0x0004)
#define IXP425_SDRAM_IR_BURST_TERMINATE_CMD	(0x0005)
#define IXP425_SDRAM_IR_NORMAL_OPERATION_CMD	(0x0006)

#define IXP425_SDRAM_CFG_32MEG_2Chip (IXP425_SDRAM_CAS_3CLKS | IXP425_SDRAM_32Meg_2Chip)
#define IXP425_SDRAM_CFG_64MEG_2Chip (IXP425_SDRAM_CAS_3CLKS | IXP425_SDRAM_64Meg_2Chip)
#define IXP425_SDRAM_CFG_64MEG_4Chip (IXP425_SDRAM_CAS_3CLKS | IXP425_SDRAM_64Meg_4Chip)
#define IXP425_SDRAM_CFG_128MEG_2Chip (IXP425_SDRAM_CAS_3CLKS | IXP425_SDRAM_128Meg_2Chip)
#define IXP425_SDRAM_CFG_128MEG_4Chip (IXP425_SDRAM_CAS_3CLKS | IXP425_SDRAM_128Meg_4Chip)
#define IXP425_SDRAM_CFG_256MEG_4Chip (IXP425_SDRAM_CAS_3CLKS | IXP425_SDRAM_256Meg_4Chip)

#define IXP425_SDRAM_REFRESH_CNT	(0x081a)

/* 32MB platforms */
#if CONFIG_IXP425_SDRAM_SIZE == 32
#define IXP425_SDRAM_DEF_CFG IXP425_SDRAM_CFG_32MEG_2Chip
#endif

/* 64MB platforms */
#if CONFIG_IXP425_SDRAM_SIZE == 64
#if CONFIG_IXP425_NUMBER_OF_MEM_CHIPS == 2
#define IXP425_SDRAM_DEF_CFG IXP425_SDRAM_CFG_64MEG_2Chip
#endif
#if CONFIG_IXP425_NUMBER_OF_MEM_CHIPS == 4
#define IXP425_SDRAM_DEF_CFG IXP425_SDRAM_CFG_64MEG_4Chip
#endif
#endif

/* 128MB platforms */
#if CONFIG_IXP425_SDRAM_SIZE == 128
#if CONFIG_IXP425_NUMBER_OF_MEM_CHIPS  == 2
#define IXP425_SDRAM_DEF_CFG IXP425_SDRAM_CFG_128MEG_2Chip
#endif
#if CONFIG_IXP425_NUMBER_OF_MEM_CHIPS == 4
#define IXP425_SDRAM_DEF_CFG IXP425_SDRAM_CFG_128MEG_4Chip
#endif
#endif

/* 256MB platforms */
#if CONFIG_IXP425_SDRAM_SIZE == 256
#define IXP425_SDRAM_DEF_CFG IXP425_SDRAM_CFG_256MEG_4Chip
#endif

#ifndef IXP425_SDRAM_DEF_CFG
#error SDRAM size & number of chips is undefined.
#endif

/* On these boards console is connected to serial 1-UART2-ttyS1 */
#if defined(CONFIG_ARCH_IXP425_COYOTE) || \
    defined(CONFIG_ARCH_IXP425_BAMBOO) || \
    defined(CONFIG_ARCH_IXP425_JEEVES) || \
    defined(CONFIG_ARCH_IXP425_ROCKAWAYBEACH) || \
    defined(CONFIG_ARCH_IXP425_GTWX5800) || \
    defined(CONFIG_ARCH_IXP425_GTWX5711) || \
    defined(CONFIG_ARCH_IXP425_GTWX5715)
    
#define IXP425_CONSOLE_UART_BASE_VIRT IXP425_UART2_BASE_VIRT
#define IXP425_CONSOLE_UART_BASE_PHYS IXP425_UART2_BASE_PHYS
    
/* On the rest of boards consloe is connected to serial 0-UART1-ttyS0 */
#else
    
#define IXP425_CONSOLE_UART_BASE_VIRT IXP425_UART1_BASE_VIRT
#define IXP425_CONSOLE_UART_BASE_PHYS IXP425_UART1_BASE_PHYS
    
#endif

/*
 * Expansion BUS
 */
/*
 * Expansion Bus 'lives' at either base1 or base 2 depending on the value of
 * Exp Bus config registers:
 * Setting bit 31 of IXP425_EXP_CFG0 puts SDRAM at zero,
 * and The expansion bus to IXP425_EXP_BUS_BASE2
 */
#define IXP425_EXP_BUS_BASE1_PHYS	(0x00000000)
#define IXP425_EXP_BUS_BASE2_PHYS	(0x50000000)
#define IXP425_EXP_BUS_BASE2_VIRT	(0xF0000000)

#define IXP425_EXP_BUS_BASE_PHYS	IXP425_EXP_BUS_BASE2_PHYS
#define IXP425_EXP_BUS_BASE_VIRT	IXP425_EXP_BUS_BASE2_VIRT

#define IXP425_EXP_BUS_REGION_SIZE	(0x08000000)
#define IXP425_EXP_BUS_CSX_REGION_SIZE	(0x01000000)

#define IXP425_EXP_BUS_CS0_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x00000000)
#define IXP425_EXP_BUS_CS1_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x01000000)
#define IXP425_EXP_BUS_CS2_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x02000000)
#define IXP425_EXP_BUS_CS3_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x03000000)
#define IXP425_EXP_BUS_CS4_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x04000000)
#define IXP425_EXP_BUS_CS5_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x05000000)
#define IXP425_EXP_BUS_CS6_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x06000000)
#define IXP425_EXP_BUS_CS7_BASE_PHYS	(IXP425_EXP_BUS_BASE2_PHYS + 0x07000000)

#define IXP425_EXP_BUS_CS0_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x00000000)
#define IXP425_EXP_BUS_CS1_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x01000000)
#define IXP425_EXP_BUS_CS2_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x02000000)
#define IXP425_EXP_BUS_CS3_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x03000000)
#define IXP425_EXP_BUS_CS4_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x04000000)
#define IXP425_EXP_BUS_CS5_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x05000000)
#define IXP425_EXP_BUS_CS6_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x06000000)
#define IXP425_EXP_BUS_CS7_BASE_VIRT	(IXP425_EXP_BUS_BASE2_VIRT + 0x07000000)

#define IXP425_FLASH_WRITABLE	(0x2)
#define IXP425_FLASH_DEFAULT	(0xbcd23c40)
#define IXP425_FLASH_WRITE	(0xbcd23c42)


#define IXP425_EXP_CS0_OFFSET	0x00
#define IXP425_EXP_CS1_OFFSET   0x04
#define IXP425_EXP_CS2_OFFSET   0x08
#define IXP425_EXP_CS3_OFFSET   0x0C
#define IXP425_EXP_CS4_OFFSET   0x10
#define IXP425_EXP_CS5_OFFSET   0x14
#define IXP425_EXP_CS6_OFFSET   0x18
#define IXP425_EXP_CS7_OFFSET   0x1C
#define IXP425_EXP_CFG0_OFFSET	0x20
#define IXP425_EXP_CFG1_OFFSET	0x24
#define IXP425_EXP_CFG2_OFFSET	0x28
#define IXP425_EXP_CFG3_OFFSET	0x2C

/*
 * Expansion Bus Controller registers.
 */
#define IXP425_EXP_REG(x) ((volatile u32 *)(IXP425_EXP_CFG_BASE_VIRT+(x)))

#define IXP425_EXP_CS0      IXP425_EXP_REG(IXP425_EXP_CS0_OFFSET)
#define IXP425_EXP_CS1      IXP425_EXP_REG(IXP425_EXP_CS1_OFFSET)
#define IXP425_EXP_CS2      IXP425_EXP_REG(IXP425_EXP_CS2_OFFSET) 
#define IXP425_EXP_CS3      IXP425_EXP_REG(IXP425_EXP_CS3_OFFSET)
#define IXP425_EXP_CS4      IXP425_EXP_REG(IXP425_EXP_CS4_OFFSET)
#define IXP425_EXP_CS5      IXP425_EXP_REG(IXP425_EXP_CS5_OFFSET)
#define IXP425_EXP_CS6      IXP425_EXP_REG(IXP425_EXP_CS6_OFFSET)     
#define IXP425_EXP_CS7      IXP425_EXP_REG(IXP425_EXP_CS7_OFFSET)

#define IXP425_EXP_CFG0     IXP425_EXP_REG(IXP425_EXP_CFG0_OFFSET) 
#define IXP425_EXP_CFG1     IXP425_EXP_REG(IXP425_EXP_CFG1_OFFSET) 
#define IXP425_EXP_CFG2     IXP425_EXP_REG(IXP425_EXP_CFG2_OFFSET) 
#define IXP425_EXP_CFG3     IXP425_EXP_REG(IXP425_EXP_CFG3_OFFSET)

/* Expansion Bus Configuration Register 0 Bit Definitions */
#define IXP425_EXP_CFG0_FLASH_8         BIT(0)
#define IXP425_EXP_CFG0_FLASH_16        0
#define IXP425_EXP_CFG0_PCI_HOST        BIT(1)
#define IXP425_EXP_CFG0_PCI_ARB         BIT(2)
#define IXP425_EXP_CFG0_PCI_CLK_66      BIT(4)	
#define IXP425_EXP_CFG0_PCI_CLK_33      0
#define IXP425_EXP_CFG0_MEM_MAP_BOOT    BIT(31)  
#define IXP425_EXP_CFG0_MEM_MAP_NORMAL  0    
#define IXP425_EXP_CFG0_INIT            0x00ffffee

/*
 * Peripheral Space Registers 
 */
#define IXP425_UART1_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x0000)
#define IXP425_UART2_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x1000)
#define IXP425_PMU_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x2000)
#define IXP425_INTC_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x3000)
#define IXP425_GPIO_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x4000)
#define IXP425_TIMER_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x5000)
#define IXP425_NPEA_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x6000)
#define IXP425_NPEB_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x7000)
#define IXP425_NPEC_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x8000)
#define IXP425_EthA_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0x9000)
#define IXP425_EthB_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0xA000)
#define IXP425_USB_BASE_PHYS	(IXP425_PERIPHERAL_BASE_PHYS + 0xB000)

#define IXP425_UART1_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x0000)
#define IXP425_UART2_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x1000)
#define IXP425_PMU_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x2000)
#define IXP425_INTC_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x3000)
#define IXP425_GPIO_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x4000)
#define IXP425_TIMER_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x5000)
#define IXP425_NPEA_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x6000)
#define IXP425_NPEB_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x7000)
#define IXP425_NPEC_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x8000)
#define IXP425_EthA_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0x9000)
#define IXP425_EthB_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0xA000)
#define IXP425_USB_BASE_VIRT	(IXP425_PERIPHERAL_BASE_VIRT + 0xB000)


/* 
 * UART Register Definitions , Offsets only as there are 2 UARTS.
 *   IXP425_UART1_BASE , IXP425_UART2_BASE.
 */

#undef  UART_NO_RX_INTERRUPT

#define IXP425_NUM_UARTS        2
#define IXP425_UART_XTAL        14745600
#define IXP425_DEF_UART_BAUD_RATE 115200
#define IXP425_DEF_UART_BAUD_DLL \
    ((IXP425_UART_XTAL/IXP425_DEF_UART_BAUD_RATE/16) & 0xff)
#define IXP425_DEF_UART_BAUD_DLM \
    ((IXP425_UART_XTAL/IXP425_DEF_UART_BAUD_RATE/16 >> 8) & 0x0f)
#define IXP425_UART_REG_DELTA   4 	/* Uart registers are spread out by 4 bytes */

#define IXP425_UART_RBR_OFFSET  0x00	/* Receive Buffer Register - read only */
#define IXP425_UART_THR_OFFSET  0x00	/* Transmit Hold Register(write buffer) - write only */
#define IXP425_UART_IER_OFFSET  0x04	/* Interrupt Enable */
#define IXP425_UART_IIR_OFFSET  0x08	/* Interrupt ID (read only)  */
#define IXP425_UART_FCR_OFFSET  0x08	/* FIFO Control - (write only)  */
#define IXP425_UART_LCR_OFFSET  0x0C	/* Line Control - r/w  */
#define IXP425_UART_MCR_OFFSET  0x10	/* Modem Control  - r/w */
#define IXP425_UART_LSR_OFFSET  0x14	/* Line Status - read only  */
#define IXP425_UART_MSR_OFFSET  0x18	/* Modem Status - read only  */
#define IXP425_UART_SPR_OFFSET  0x1C	/* Scratch register r/w  */
    

/* Note The Divisor can only be written to with DLAB bit of the Line
 * Control register is set, Note: for changes of baud rate the device 
 * should be disabled to prevent the s/w from reading the devisor latch 
 * as data 
 */

#define IXP425_UART_DLL_OFFSET (0x00)	/* Divisor Latch LSB  */
#define IXP425_UART_DLH_OFFSET (0x04)	/* Divisor Latch MSB  */

/* UART BIT DEFINITIONS. */

/* Line control register */
#define IXP425_UART_LCR_WS_5_BIT 	(0) /* 5 bit character */
#define IXP425_UART_LCR_WS_6_BIT 	(1) /* 6 bit character */
#define IXP425_UART_LCR_WS_7_BIT 	(2) /* 7 bit character */
#define IXP425_UART_LCR_WS_8_BIT 	(3) /* 8 bit character */
#define IXP425_UART_LCR_STB_BIT 	BIT(2) /* 0 = 1 Stop Bit,
						  1 = 2 Stop Bits  */ 
#define IXP425_UART_LCR_PEN_BIT 	BIT(3) /* Parity Enabled */
#define IXP425_UART_LCR_EPS_BIT 	BIT(4) /* Even Parity Select - 1 for even parity, 0 for odd */
#define IXP425_UART_LCR_STKYP_BIT 	BIT(5) /* Sticky Parity */
#define IXP425_UART_LCR_SB_BIT 		BIT(6) /* Set Break */
#define IXP425_UART_LCR_DLAB_BIT 	BIT(7) /* DLAB - Divisor Latch Bit  */

/*
 * Constants to make it easy to access  Interrupt Controller registers
 */
#define IXP425_ICPR_OFFSET	0x00 /* Interrupt Status */
#define IXP425_ICMR_OFFSET	0x04 /* Interrupt Enable */
#define IXP425_ICLR_OFFSET	0x08 /* Interrupt IRQ/FIQ Select */
#define IXP425_ICIP_OFFSET      0x0C /* IRQ Status */
#define IXP425_ICFP_OFFSET	0x10 /* FIQ Status */
#define IXP425_ICHR_OFFSET	0x14 /* Interrupt Priority */
#define IXP425_ICIH_OFFSET	0x18 /* IRQ Highest Pri Int */
#define IXP425_ICFH_OFFSET	0x1C /* FIQ Highest Pri Int */

/*
 * Interrupt Controller Register Definitions.
 */
#define IXP425_INTC_REG(x) ((volatile u32 *)(IXP425_INTC_BASE_VIRT+(x)))

#define IXP425_ICPR     IXP425_INTC_REG(IXP425_ICPR_OFFSET)
#define IXP425_ICMR     IXP425_INTC_REG(IXP425_ICMR_OFFSET)
#define IXP425_ICLR     IXP425_INTC_REG(IXP425_ICLR_OFFSET)
#define IXP425_ICIP     IXP425_INTC_REG(IXP425_ICIP_OFFSET)
#define IXP425_ICFP     IXP425_INTC_REG(IXP425_ICFP_OFFSET)
#define IXP425_ICHR     IXP425_INTC_REG(IXP425_ICHR_OFFSET)
#define IXP425_ICIH     IXP425_INTC_REG(IXP425_ICIH_OFFSET) 
#define IXP425_ICFH     IXP425_INTC_REG(IXP425_ICFH_OFFSET)
                                                                                
/*
 * Constants to make it easy to access GPIO registers
 */
#define IXP425_GPIO_GPOUTR_OFFSET       0x00
#define IXP425_GPIO_GPOER_OFFSET        0x04
#define IXP425_GPIO_GPINR_OFFSET        0x08
#define IXP425_GPIO_GPISR_OFFSET        0x0C
#define IXP425_GPIO_GPIT1R_OFFSET	0x10
#define IXP425_GPIO_GPIT2R_OFFSET	0x14
#define IXP425_GPIO_GPCLKR_OFFSET	0x18
#define IXP425_GPIO_GPDBSELR_OFFSET	0x1C

/* 
 * GPIO Register Definitions.
 * [Only perform 32bit reads/writes]
 */
#define IXP425_GPIO_REG(x) ((volatile u32 *)(IXP425_GPIO_BASE_VIRT+(x)))

#define IXP425_GPIO_GPOUTR	IXP425_GPIO_REG(IXP425_GPIO_GPOUTR_OFFSET)
#define IXP425_GPIO_GPOER       IXP425_GPIO_REG(IXP425_GPIO_GPOER_OFFSET)
#define IXP425_GPIO_GPINR       IXP425_GPIO_REG(IXP425_GPIO_GPINR_OFFSET)
#define IXP425_GPIO_GPISR       IXP425_GPIO_REG(IXP425_GPIO_GPISR_OFFSET)
#define IXP425_GPIO_GPIT1R      IXP425_GPIO_REG(IXP425_GPIO_GPIT1R_OFFSET)
#define IXP425_GPIO_GPIT2R      IXP425_GPIO_REG(IXP425_GPIO_GPIT2R_OFFSET)
#define IXP425_GPIO_GPCLKR      IXP425_GPIO_REG(IXP425_GPIO_GPCLKR_OFFSET)
#define IXP425_GPIO_GPDBSELR    IXP425_GPIO_REG(IXP425_GPIO_GPDBSELR_OFFSET)

/*
 * Constants to make it easy to access Timer Control/Status registers
 */
#define IXP425_OSTS_OFFSET	0x00  /* Continious TimeStamp */
#define IXP425_OST1_OFFSET	0x04  /* Timer 1 Timestamp */
#define IXP425_OSRT1_OFFSET	0x08  /* Timer 1 Reload */
#define IXP425_OST2_OFFSET	0x0C  /* Timer 2 Timestamp */
#define IXP425_OSRT2_OFFSET	0x10  /* Timer 2 Reload */
#define IXP425_OSWT_OFFSET	0x14  /* Watchdog Timer */
#define IXP425_OSWE_OFFSET	0x18  /* Watchdog Enable */
#define IXP425_OSWK_OFFSET	0x1C  /* Watchdog Key */
#define IXP425_OSST_OFFSET	0x20  /* Timer Status */

/*
 * Operating System Timer Register Definitions.
 */

#define IXP425_TIMER_REG(x) ((volatile u32 *)(IXP425_TIMER_BASE_VIRT+(x)))

#define IXP425_OSTS	IXP425_TIMER_REG(IXP425_OSTS_OFFSET)
#define IXP425_OST1	IXP425_TIMER_REG(IXP425_OST1_OFFSET)
#define IXP425_OSRT1	IXP425_TIMER_REG(IXP425_OSRT1_OFFSET)
#define IXP425_OST2	IXP425_TIMER_REG(IXP425_OST2_OFFSET)
#define IXP425_OSRT2	IXP425_TIMER_REG(IXP425_OSRT2_OFFSET)
#define IXP425_OSWT	IXP425_TIMER_REG(IXP425_OSWT_OFFSET)
#define IXP425_OSWE	IXP425_TIMER_REG(IXP425_OSWE_OFFSET)
#define IXP425_OSWK	IXP425_TIMER_REG(IXP425_OSWK_OFFSET)
#define IXP425_OSST	IXP425_TIMER_REG(IXP425_OSST_OFFSET)

/*
 * Timer register values and bit definitions 
 */

#define IXP425_OST_ENABLE              BIT(0)
#define IXP425_OST_ONE_SHOT            BIT(1)
/* Low order bits of reload value ignored */
#define IXP425_OST_RELOAD_MASK         (0x3)    
#define IXP425_OST_DISABLED            (0x0)

#define IXP425_OSST_TIMER_1_PEND       BIT(0)
#define IXP425_OSST_TIMER_2_PEND       BIT(1)
#define IXP425_OSST_TIMER_TS_PEND      BIT(2)
#define IXP425_OSST_TIMER_WDOG_PEND    BIT(3)
#define IXP425_OSST_TIMER_WARM_RESET   BIT(4)

/*
 * Constants to make it easy to access PCI Control/Status registers
 */
#define PCI_NP_AD_OFFSET            0x00
#define PCI_NP_CBE_OFFSET           0x04
#define PCI_NP_WDATA_OFFSET         0x08
#define PCI_NP_RDATA_OFFSET         0x0c
#define PCI_CRP_AD_CBE_OFFSET       0x10
#define PCI_CRP_WDATA_OFFSET        0x14
#define PCI_CRP_RDATA_OFFSET        0x18
#define PCI_CSR_OFFSET              0x1c
#define PCI_ISR_OFFSET              0x20
#define PCI_INTEN_OFFSET            0x24
#define PCI_DMACTRL_OFFSET          0x28
#define PCI_AHBMEMBASE_OFFSET       0x2c
#define PCI_AHBIOBASE_OFFSET        0x30
#define PCI_PCIMEMBASE_OFFSET       0x34
#define PCI_AHBDOORBELL_OFFSET      0x38
#define PCI_PCIDOORBELL_OFFSET      0x3C
#define PCI_ATPDMA0_AHBADDR_OFFSET  0x40
#define PCI_ATPDMA0_PCIADDR_OFFSET  0x44
#define PCI_ATPDMA0_LENADDR_OFFSET  0x48
#define PCI_ATPDMA1_AHBADDR_OFFSET  0x4C
#define PCI_ATPDMA1_PCIADDR_OFFSET  0x50
#define PCI_ATPDMA1_LENADDR_OFFSET  0x54

/*
 * PCI Control/Status Registers
 */
#define IXP425_PCI_CSR(x) ((volatile u32 *)(IXP425_PCI_CFG_BASE_VIRT+(x)))

#define PCI_NP_AD               IXP425_PCI_CSR(PCI_NP_AD_OFFSET)
#define PCI_NP_CBE              IXP425_PCI_CSR(PCI_NP_CBE_OFFSET)
#define PCI_NP_WDATA            IXP425_PCI_CSR(PCI_NP_WDATA_OFFSET)
#define PCI_NP_RDATA            IXP425_PCI_CSR(PCI_NP_RDATA_OFFSET)
#define PCI_CRP_AD_CBE          IXP425_PCI_CSR(PCI_CRP_AD_CBE_OFFSET)
#define PCI_CRP_WDATA           IXP425_PCI_CSR(PCI_CRP_WDATA_OFFSET)
#define PCI_CRP_RDATA           IXP425_PCI_CSR(PCI_CRP_RDATA_OFFSET)
#define PCI_CSR                 IXP425_PCI_CSR(PCI_CSR_OFFSET) 
#define PCI_ISR                 IXP425_PCI_CSR(PCI_ISR_OFFSET)
#define PCI_INTEN               IXP425_PCI_CSR(PCI_INTEN_OFFSET)
#define PCI_DMACTRL             IXP425_PCI_CSR(PCI_DMACTRL_OFFSET)
#define PCI_AHBMEMBASE          IXP425_PCI_CSR(PCI_AHBMEMBASE_OFFSET)
#define PCI_AHBIOBASE           IXP425_PCI_CSR(PCI_AHBIOBASE_OFFSET)
#define PCI_PCIMEMBASE          IXP425_PCI_CSR(PCI_PCIMEMBASE_OFFSET)
#define PCI_AHBDOORBELL         IXP425_PCI_CSR(PCI_AHBDOORBELL_OFFSET)
#define PCI_PCIDOORBELL         IXP425_PCI_CSR(PCI_PCIDOORBELL_OFFSET)
#define PCI_ATPDMA0_AHBADDR     IXP425_PCI_CSR(PCI_ATPDMA0_AHBADDR_OFFSET)
#define PCI_ATPDMA0_PCIADDR     IXP425_PCI_CSR(PCI_ATPDMA0_PCIADDR_OFFSET)
#define PCI_ATPDMA0_LENADDR     IXP425_PCI_CSR(PCI_ATPDMA0_LENADDR_OFFSET)
#define PCI_ATPDMA1_AHBADDR     IXP425_PCI_CSR(PCI_ATPDMA1_AHBADDR_OFFSET)
#define PCI_ATPDMA1_PCIADDR     IXP425_PCI_CSR(PCI_ATPDMA1_PCIADDR_OFFSET)
#define PCI_ATPDMA1_LENADDR     IXP425_PCI_CSR(PCI_ATPDMA1_LENADDR_OFFSET)

/*
 * PCI register values and bit definitions 
 */

/* CSR bit definitions */
#define PCI_CSR_HOST    	BIT(0)
#define PCI_CSR_ARBEN   	BIT(1)
#define PCI_CSR_ADS     	BIT(2)
#define PCI_CSR_PDS     	BIT(3)
#define PCI_CSR_ABE     	BIT(4)
#define PCI_CSR_DBT     	BIT(5)
#define PCI_CSR_ASE     	BIT(8)
#define PCI_CSR_IC      	BIT(15)

/* ISR (Interrupt status) Register bit definitions */
#define PCI_ISR_PSE     	BIT(0)
#define PCI_ISR_PFE     	BIT(1)
#define PCI_ISR_PPE     	BIT(2)
#define PCI_ISR_AHBE    	BIT(3)
#define PCI_ISR_APDC    	BIT(4)
#define PCI_ISR_PADC    	BIT(5)
#define PCI_ISR_ADB     	BIT(6)
#define PCI_ISR_PDB     	BIT(7)

/* INTEN (Interrupt Enable) Register bit definitions */
#define PCI_INTEN_PSE   	BIT(0)
#define PCI_INTEN_PFE   	BIT(1)
#define PCI_INTEN_PPE   	BIT(2)
#define PCI_INTEN_AHBE  	BIT(3)
#define PCI_INTEN_APDC  	BIT(4)
#define PCI_INTEN_PADC  	BIT(5)
#define PCI_INTEN_ADB   	BIT(6)
#define PCI_INTEN_PDB   	BIT(7)

#define CRP_AD_CBE_BESL         20
#define CRP_AD_CBE_WRITE        BIT(16)

/*
 * Clock Speed Definitions.
 */
#define IXP425_PERIPHERAL_BUS_CLOCK (66) /* 66Mhzi APB BUS   */ 

/*
 * NPE s/w build ID. Use with IxNpeDl to select which version of
 * NPE microcode to download to the NPEs.  The buildID indicates
 * the functionality of that version.  See also IxNpeDl.h
 */

/* Functionality: Ethernet w/o FastPath, No Crypto */
#define IX_ETH_NPE_BUILD_ID_ETH                  0x00

/* Functionality: Ethernet w/ FastPath, No Crypto  */
#define IX_ETH_NPE_BUILD_ID_ETH_FPATH            0x01

/* Functionality: Crypto w/o AES, No Ethernet      */
#define IX_ETH_NPE_BUILD_ID_CRYPTO               0x02

/* Functionality: Crypto w/o AES, No Ethernet      */
#define IX_ETH_NPE_BUILD_ID_CRYPTO_AES           0x03

/* Functionality: Ethernet w/o FastPath, Crypto w/o AES */
#define IX_ETH_NPE_BUILD_ID_ETH_CRYPTO           0x04

/* Functionality: Ethernet w/o FastPath, Crypto w/ AES */
#define IX_ETH_NPE_BUILD_ID_ETH_CRYPTO_AES       0x05

/* Functionality: Ethernet w/ FastPath, Crypto w/o AES */
#define IX_ETH_NPE_BUILD_ID_ETH_FPATH_CRYPTO     0x06

/* Functionality: Ethernet w/ FastPath, Crypto w/ AES */
#define IX_ETH_NPE_BUILD_ID_ETH_FPATH_CRYPTO_AES 0x07

/* Functionality: ATM, using multy port multy PHY */
#define IX_ATM_NPE_BUILD_ID_MULTI_PORT_MPHY 0x4

#define IX_ATM_NPE_BUILD_ID_SINGLE_PORT_MPHY 0x3

#define IX_ATM_NPE_BUILD_ID_SINGLE_PORT_SPHY 0x2

#if defined(CONFIG_IXP425_ADSL_USE_MPHY)
#  define IX_ATM_NPEA_BUILD_ID IX_ATM_NPE_BUILD_ID_SINGLE_PORT_MPHY 
#elif defined(CONFIG_IXP425_ADSL_USE_SPHY)
#  define IX_ATM_NPEA_BUILD_ID IX_ATM_NPE_BUILD_ID_SINGLE_PORT_SPHY 
#endif

#define IX_ETHACC_NPE_LIST 32

#endif
