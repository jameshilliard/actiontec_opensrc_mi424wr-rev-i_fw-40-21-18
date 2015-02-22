/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell Commercial License Option

If you received this File from Marvell and you have entered into a commercial
license agreement (a "Commercial License") with Marvell, the File is licensed
to you under the terms of the applicable Commercial License.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
********************************************************************************
Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
	    this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
        used to endorse or promote products derived from this software without
        specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
* mvSysHwCfg.h - Marvell system HW configuration file
*
* DESCRIPTION:
*       None.
*
* DEPENDENCIES:
*       None.
*
*******************************************************************************/

#ifndef __INCmvSysHwConfigh
#define __INCmvSysHwConfigh

#include "../../../../include/linux/autoconf.h"

#define CONFIG_MARVELL	1

/* includes */
#define _1K         0x00000400
#define _4K         0x00001000
#define _8K         0x00002000
#define _16K        0x00004000
#define _32K        0x00008000
#define _64K        0x00010000
#define _128K       0x00020000
#define _256K       0x00040000
#define _512K       0x00080000

#define _1M         0x00100000
#define _2M         0x00200000
#define _4M         0x00400000
#define _8M         0x00800000
#define _16M        0x01000000
#define _32M        0x02000000
#define _64M        0x04000000
#define _128M       0x08000000
#define _256M       0x10000000
#define _512M       0x20000000

#define _1G         0x40000000
#define _2G         0x80000000

/****************************************/
/* Soc supporeted Units definitions	*/
/****************************************/

#ifdef CONFIG_MV_INCLUDE_PEX
#define MV_INCLUDE_PEX
#endif
#ifdef CONFIG_MV_INCLUDE_TWSI
#define MV_INCLUDE_TWSI
#endif
#ifdef CONFIG_MV_INCLUDE_CESA
#define MV_INCLUDE_CESA
#endif
#ifdef CONFIG_MV_INCLUDE_GIG_ETH
#define MV_INCLUDE_GIG_ETH
#endif
#ifdef CONFIG_MV_INCLUDE_INTEG_SATA
#define MV_INCLUDE_INTEG_SATA
#define MV_INCLUDE_SATA
#endif
#ifdef CONFIG_MV_INCLUDE_USB
#define MV_INCLUDE_USB
//#define MV_USB_VOLTAGE_FIX
#endif
#ifdef CONFIG_MV_INCLUDE_NAND
#define MV_INCLUDE_NAND
#endif
#ifdef CONFIG_MV_INCLUDE_TDM
#define MV_INCLUDE_TDM
#endif
#ifdef CONFIG_MV_INCLUDE_XOR
#define MV_INCLUDE_XOR
#endif
#ifdef CONFIG_MV_INCLUDE_TWSI
#define MV_INCLUDE_TWSI
#endif
#ifdef CONFIG_MV_INCLUDE_UART
#define MV_INCLUDE_UART
#endif
#ifdef CONFIG_MV_INCLUDE_SPI
#define MV_INCLUDE_SPI
#endif
#ifdef CONFIG_MV_INCLUDE_AUDIO
#define MV_INCLUDE_AUDIO
#endif
#ifdef CONFIG_MV_INCLUDE_TS
#define MV_INCLUDE_TS
#endif
#ifdef CONFIG_MV_INCLUDE_SDIO
#define MV_INCLUDE_SDIO
#endif


/* NAND flash stuff */
#ifdef CONFIG_MV_NAND_BOOT
#define MV_NAND_BOOT
#endif
#ifdef CONFIG_MV_NAND
#define MV_NAND
#endif

/* SPI flash stuff */
#ifdef CONFIG_MV_SPI_BOOT
#define MV_SPI_BOOT
#endif
#ifdef CONFIG_MV_INCLUDE_SFLASH_MTD
#define MV_INCLUDE_SFLASH_MTD
#define MV_SPI
#endif

#ifdef CONFIG_MV_INCLUDE_NORFLASH_MTD
#define MV_NOR
#endif

/****************************************************************/
/************* General    configuration ********************/
/****************************************************************/

/* Enable Clock Power Control */
#define MV_INCLUDE_CLK_PWR_CNTRL

/* Disable the DEVICE BAR in the PEX */
#define MV_DISABLE_PEX_DEVICE_BAR

/* Allow the usage of early printings during initialization */
#define MV_INCLUDE_EARLY_PRINTK

#define WAN_SWAP_FEATURE

/****************************************************************/
/************* NFP configuration ********************************/
/****************************************************************/
#define MV_NFP_SEC_Q_SIZE		64
#define MV_NFP_SEC_REQ_Q_SIZE		1000



/****************************************************************/
/************* CESA configuration ********************/
/****************************************************************/

#ifdef MV_INCLUDE_CESA

/* Use 2K of SRAM */
#define MV_CESA_MAX_BUF_SIZE           1600

#endif /* MV_INCLUDE_CESA */

/* DRAM cache coherency configuration */
#define MV_CACHE_COHERENCY  MV_CACHE_COHER_SW



/****************************************************************/
/*************** Telephony configuration ************************/
/****************************************************************/
#if defined(CONFIG_MV_TDM_USE_EXTERNAL_PCLK_SOURCE)
 #define MV_TDM_USE_EXTERNAL_PCLK_SOURCE
#endif

/* We use the following registers to store DRAM interface pre configuration   */
/* auto-detection results													  */
/* IMPORTANT: We are using mask register for that purpose. Before writing     */
/* to units mask register, make sure main maks register is set to disable     */
/* all interrupts.                                                            */
#define DRAM_BUF_REG0   0x30810 /* sdram bank 0 size            */  
#define DRAM_BUF_REG1   0x30820 /* sdram config                 */
#define DRAM_BUF_REG2   0x30830 /* sdram mode                   */
#define DRAM_BUF_REG3   0x308c4 /* dunit control low            */          
#define DRAM_BUF_REG4   0x60a90 /* sdram address control        */
#define DRAM_BUF_REG5   0x60a94 /* sdram timing control low     */
#define DRAM_BUF_REG6   0x60a98 /* sdram timing control high    */
#define DRAM_BUF_REG7   0x60a9c /* sdram ODT control low        */
#define DRAM_BUF_REG8   0x60b90 /* sdram ODT control high       */
#define DRAM_BUF_REG9   0x60b94 /* sdram Dunit ODT control      */
#define DRAM_BUF_REG10  0x60b98 /* sdram Extended Mode          */
#define DRAM_BUF_REG11  0x60b9c /* sdram Ddr2 Time Low Reg      */
#define DRAM_BUF_REG12  0x60a00 /* sdram Ddr2 Time High Reg     */
#define DRAM_BUF_REG13  0x60a04 /* dunit Ctrl High              */
#define DRAM_BUF_REG14  0x60b00 /* sdram second DIMM exist      */

/* Following the pre-configuration registers default values restored after    */
/* auto-detection is done                                                     */
#define DRAM_BUF_REG_DV 0

/* System Mapping */
#define SDRAM_CS0_BASE  0x00000000
#define SDRAM_CS0_SIZE  _256M

#define SDRAM_CS1_BASE  0x10000000
#define SDRAM_CS1_SIZE  _256M

#define SDRAM_CS2_BASE  0x20000000
#define SDRAM_CS2_SIZE  _256M

#define SDRAM_CS3_BASE  0x30000000
#define SDRAM_CS3_SIZE  _256M

/* PEX */
#define PEX0_MEM_BASE		0xF3000000
#define PEX0_MEM_SIZE		_16M

#define PEX1_MEM_BASE		0xF4000000
#define PEX1_MEM_SIZE		_16M

#define CRYPT_ENG_BASE(chan) 		(0xF2200000 + (chan * 0x200000))
#define CRYPT_ENG_SIZE	_2M

/* Internal registers: size is defined in Controllerenvironment */
#define INTER_REGS_BASE	0xF1000000

#define DDR_OPERATION_BASE	(INTER_REGS_BASE | 0x1418)

#define PEX0_IO_BASE	0xF2000000
#define PEX0_IO_SIZE	_1M

#define PEX1_IO_BASE		0xF2100000
#define PEX1_IO_SIZE		_1M

#define SPI_CS_BASE		0xF0000000
#define SPI_CS_SIZE	_16M

#define PNC_BM_PHYS_BASE	0xF5000000
#define PNC_BM_SIZE		_1M

#define PMT_GIGA_PHYS_BASE      0xf5100000
#define PMT_PON_PHYS_BASE       0xf5200000
#define PMT_MEM_SIZE            _1M

/* Device Chip Selects */
#define BOOTDEV_CS_BASE	0xFF800000
#define BOOTDEV_CS_SIZE _8M

/* NAND/NOR Flash */
#define NAND_NOR_CS_BASE	0xF8000000
#define NAND_CS_SIZE		_2M
#define NOR_CS_SIZE		_128M

/* CS2 - BOOTROM */
#define DEVICE_CS2_BASE 0xFF900000
#define DEVICE_CS2_SIZE _1M


/* DRAM detection stuff */
#define MV_DRAM_AUTO_SIZE

/* Board clock detection */
#define TCLK_AUTO_DETECT    	/* Use Tclk auto detection   */
#define SYSCLK_AUTO_DETECT	/* Use SysClk auto detection */
#define PCLCK_AUTO_DETECT  	/* Use PClk auto detection   */
#define L2CLK_AUTO_DETECT 	/* Use L2Clk auto detection   */

/* PEX-PCI\PCI-PCI Bridge*/
#define PCI0_IF_PTP		0		/* Bridge exist on pciIf0*/

 

#endif /* __INCmvSysHwConfigh */

