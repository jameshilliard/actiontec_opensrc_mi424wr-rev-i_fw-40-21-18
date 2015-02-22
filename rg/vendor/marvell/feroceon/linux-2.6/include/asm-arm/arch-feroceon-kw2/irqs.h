/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/autoconf.h>

#include "../arch/arm/mach-feroceon-kw2/config/mvSysHwConfig.h"

/* for Asm only */
#define MV_ASM_BRIDGE_INT_CAUSE_REG	(INTER_REGS_BASE + 0x20110)
#define MV_ASM_BRIDGE_INT_MASK_REG    	(INTER_REGS_BASE + 0x20114)

#define MV_ASM_IRQ_CAUSE_LOW_REG	(INTER_REGS_BASE + 0x20200)
#define MV_ASM_IRQ_CAUSE_HIGH_REG	(INTER_REGS_BASE + 0x20210)

#define MV_ASM_IRQ_MASK_LOW_REG		(INTER_REGS_BASE + 0x20204)
#define MV_ASM_IRQ_MASK_HIGH_REG	(INTER_REGS_BASE + 0x20214)

#define MV_ASM_IRQ_CAUSE_ERROR_REG	(INTER_REGS_BASE + 0x20220)
#define MV_ASM_IRQ_MASK_ERROR_REG	(INTER_REGS_BASE + 0x20224)

#define MV_ASM_GPP_IRQ_CAUSE_REG	(INTER_REGS_BASE + 0x18110) /* use data in for cause in case of level interrupts */
#define MV_ASM_GPP_IRQ_MID_CAUSE_REG	(INTER_REGS_BASE + 0x18150) /* use mid data in for cause in case of level interrupts */ 
#define MV_ASM_GPP_IRQ_HIGH_CAUSE_REG	(INTER_REGS_BASE + 0x18190) /* use high data in for cause in case of level interrupts */ 
 
#define MV_ASM_GPP_IRQ_MASK_REG        	(INTER_REGS_BASE + 0x1811c)	/* level low mask */
#define MV_ASM_GPP_IRQ_MID_MASK_REG	(INTER_REGS_BASE + 0x1815c)	/* level mid mask */
#define MV_ASM_GPP_IRQ_HIGH_MASK_REG    (INTER_REGS_BASE + 0x1819c)	/* level high mask */

/* for c */
#define MV_IRQ_CAUSE_LOW_REG		0x20200
#define MV_IRQ_CAUSE_HIGH_REG		0x20210
#define MV_IRQ_CAUSE_ERROR_REG		0x20220

#define MV_IRQ_MASK_LOW_REG		0x20204
#define MV_IRQ_MASK_HIGH_REG		0x20214
#define MV_IRQ_MASK_ERROR_REG		0x20224

#define MV_GPP_IRQ_CAUSE_REG(id)	(0x18114 + (0x40 * (id)))

#define MV_GPP_IRQ_EDGE_REG(id)		(0x18118 + (0x40 * (id)))

#define MV_GPP_IRQ_MASK_REG(id)        	(0x1811c + (0x40 * (id)))

#define MV_GPP_IRQ_POLARITY(id)		(0x1810c + (0x40 * (id)))

#if 0
#define MV_AHBTOMBUS_IRQ_CAUSE_REG 	0x20114
#endif

#define MV_PCI_MASK_REG(unit)		((unit == 0) ? 0x41910 : 0x45910)
#define MV_PCI_IRQ_CAUSE_REG(pciIf)    (0x41900 + (pciIf * 0x4000))
#define MV_PCI_MASK_ABCD		(BIT24 | BIT25 | BIT26 | BIT27 )

/* Description for bit from PCI Express Interrupt Mask Register 
** BIT3 - Erroneous Write Attempt to Internal Register
** BIT4 - Hit Default Window Error
** BIT9 and BIT10 - Non Fatal and Fatal Error Detected
** BIT14 - Flow Control Protocol Error
** BIT23 - Link Failure Indication
*/
#define MV_PCI_MASK_ERR                        (BIT3 | BIT4 | BIT9 | BIT10 | BIT14 | BIT23)

#define GPP_IRQ_TYPE_LEVEL		0
#define GPP_IRQ_TYPE_CHANGE_LEVEL	1


/* 
 *  Interrupt numbers
 */
#define IRQ_HIGH(x)			(32 + (x))
#define IRQ_ERROR(x)			(64 + (x))
#define IRQ_START			0

/* Main Low */
#define IRQ_MAIN_HIGH_SUM		0
#define BRIDGE_IRQ_NUM			1
#define IRQ_MAIN_ERROR_SUM		4
#define NET_MISC_IRQ_NUM(x)		(5 + ((x) * 4))
#define NET_RXTX_IRQ_NUM(x)		(6 + ((x) * 4))
#define NET_TH_RXTX_IRQ_NUM(x)		(7 + ((x) * 4))
#define NET_WAKEUP_IRQ_NUM(x)		(8 + ((x) * 4))
#define NETPON_MISC_IRQ_NUM		13
#define NETPON_RXTX_IRQ_NUM		14
#define NETPON_TH_RXTX_IRQ_NUM		15
#define NETPON_WAKEUP_IRQ_NUM		16
#define SDIO_IRQ_NUM			18
#define SPI_IRQ_NUM(x)			((x == 0) ? 19 : IRQ_HIGH(17))
#define TDM_2CH_IRQ_NUM			19
#define CESA_IRQ_NUM(chan)		((chan == 0) ? 21 : 17)
#define SATA_IRQ_NUM			22
#define USB_IRQ_NUM			23
#define PEX1_IRQ_NUM			24
#define PEX0_IRQ_NUM			25
#define XOR0_IRQ_NUM			26
#define XOR1_IRQ_NUM			27
#define BM_IRQ_NUM			28
#define ETH_COMP_IRQ_NUM		29
#define TDM_IRQ_NUM			30
#define GPON_MAC_IRQ_NUM		31

/* Main High */
#define RTC_IRQ_NUM			IRQ_HIGH(0)
#define UART_IRQ_NUM(x)			IRQ_HIGH(1 + x)
#define DYNGASP_IRQ_NUM			IRQ_HIGH(3)
#define GPP_LOW_0_7_IRQ_NUM		IRQ_HIGH(4)
#define GPP_LOW_8_15_IRQ_NUM		IRQ_HIGH(5)
#define GPP_LOW_16_23_IRQ_NUM		IRQ_HIGH(6)
#define GPP_LOW_24_31_IRQ_NUM		IRQ_HIGH(7)
#define GPP_MID_0_7_IRQ_NUM		IRQ_HIGH(8)
#define GPP_MID_8_15_IRQ_NUM		IRQ_HIGH(9)
#define GPP_MID_16_23_IRQ_NUM		IRQ_HIGH(10)
#define GPP_MID_24_31_IRQ_NUM		IRQ_HIGH(11)
#define GPP_HIGH_0_7_IRQ_NUM		IRQ_HIGH(12)
#define GPP_HIGH_8_15_IRQ_NUM		IRQ_HIGH(13)
#define GPP_HIGH_16_23_IRQ_NUM		IRQ_HIGH(14)
#define GPP_HIGH_24_IRQ_NUM		IRQ_HIGH(15)
#define NET_LEGACY_TX_IRQ_NUM(x)	IRQ_HIGH(19 + (3 * x))
#define NET_LEGACY_RX_IRQ_NUM(x)	IRQ_HIGH(20 + (3 * x))
#define NET_LEGACY_SUM_IRQ_NUM(x)	IRQ_HIGH(21 + (3 * x))
#define NF_IRQ_NUM			IRQ_HIGH(16)
#define TWSI_IRQ_NUM(x)			((x == 0) ? IRQ_HIGH(18) : IRQ_HIGH(25))

/* Main Error */
#define NET_ERR_IRQ_NUM(x)		IRQ_ERROR(0 + x)
#define PON_ERR_IRQ_NUM			IRQ_ERROR(2)
#define L2C_TAG_PAR_ERR_IRQ_NUM		IRQ_ERROR(3)
#define L2C_UNCOR_ERR_IRQ_NUM		IRQ_ERROR(4)
#define L2C_COR_ERR_IRQ_NUM		IRQ_ERROR(5)
#define TDM_ERROR			IRQ_ERROR(6)
#define DEVBUS_IRQ_NUM			IRQ_ERROR(7)
#define L1C_UNREC_PAR_IRQ_NUM		IRQ_ERROR(8)
#define XOR_ERR_IRQ_NUM			IRQ_ERROR(9)
#define BM_ERR_IRQ_NUM			IRQ_ERROR(10)
#define PNC_ERR_IRQ_NUM			IRQ_ERROR(11)
#define USB_ERR_IRQ_NUM			IRQ_ERROR(12)
#define PEX1_ERR_IRQ_NUM		IRQ_ERROR(13)
#define PEX0_ERR_IRQ_NUM		IRQ_ERROR(14)
#define CESA_ERR_IRQ_NUM		IRQ_ERROR(16)

#define PEX_ERR_IRQ_NUM(pciIf)             ((pciIf == 0) ? PEX0_ERR_IRQ_NUM : PEX1_ERR_IRQ_NUM)

#define IRQ_GPP_START			96
#define IRQ_ASM_GPP_START               96
#define IRQ_GPP_ID(idx)			(96 + (idx))

#define NR_IRQS                         192

/*********** timer **************/

#define TIME_IRQ        	IRQ_BRIDGE
#define BRIDGE_INT_CAUSE_REG	0x20110
#define BRIDGE_INT_MASK_REG    	0x20114
#define TIMER_BIT_MASK(x)	(1<<(x+1))

#define ETH_PORT_IRQ_NUM(dev)   NET_LEGACY_SUM_IRQ_NUM(dev) 

#define IRQ_USB_CTRL(dev)	USB_IRQ_NUM
#define CESA_IRQ(chan)		CESA_IRQ_NUM(chan)
