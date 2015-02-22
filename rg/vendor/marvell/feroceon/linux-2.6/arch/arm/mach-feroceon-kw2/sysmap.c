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


#include "mvSysHwConfig.h"
#include "ctrlEnv/sys/mvCpuIf.h"
#include "boardEnv/mvBoardEnvLib.h"
#include <asm/mach/map.h>

/* for putstr */
/* #include <asm/arch/uncompress.h> */

MV_CPU_DEC_WIN* mv_sys_map(void);

#if defined(CONFIG_MV_INCLUDE_CESA)
u32 mv_crypto_base_get(u8 chan);
#endif

#if defined(CONFIG_MV_INTERNAL_REGS_SELECTIVE_MAPPING)

/* need to make sure it is big enough to hold all mapping entries */
#define MEM_TABLE_MAX_ENTRIES	30

/* default mapped entries */
#define MEM_TABLE_ENTRIES	6

/* number of entries to map */
volatile u32 entries = MEM_TABLE_ENTRIES;

struct _mv_internal_regs_map {
	MV_UNIT_ID id;
	u32 index;
	u32 offset;
	u32 size;
};

/* Internal registers mapping table */
struct _mv_internal_regs_map mv_internal_regs_map[] = {
	{DRAM_UNIT_ID,		0,	MV_DRAM_REGS_OFFSET,		SZ_64K},
	{CESA_UNIT_ID,		0,	MV_CESA_TDMA_REGS_OFFSET,	SZ_64K},
	{USB_UNIT_ID,		0,	MV_USB_REGS_OFFSET(0),		SZ_64K},
	{XOR_UNIT_ID,		0,	MV_XOR_REGS_OFFSET,		SZ_64K},
	{ETH_GIG_UNIT_ID,	0,	MV_ETH_REGS_OFFSET(0),		SZ_8K},  /* GbE port0 registers */
	{ETH_GIG_UNIT_ID,	1,	MV_ETH_REGS_OFFSET(1),		SZ_8K},  /* GbE port1 registers */
	{SATA_UNIT_ID,		0,	(MV_SATA_REGS_OFFSET + 0x2000),	SZ_8K }, /* SATA port0 registers */
	{SDIO_UNIT_ID,		0,	MV_SDIO_REGS_OFFSET,		SZ_64K},
	{TDM_UNIT_ID,		0,	MV_TDM_REGS_OFFSET,		SZ_64K},
	{XPON_UNIT_ID,		0,	MV_PON_REGS_OFFSET,		SZ_64K},
	{BM_UNIT_ID,		0,	MV_BM_REGS_OFFSET,		SZ_64K},
	{PNC_UNIT_ID,		0,	MV_PNC_REGS_OFFSET,		SZ_64K}
};

/* AHB to MBUS mapping entry */
struct map_desc AHB_TO_MBUS_MAP[] = {
	{(INTER_REGS_BASE + MAX_AHB_TO_MBUS_REG_BASE), __phys_to_pfn(INTER_REGS_BASE + MAX_AHB_TO_MBUS_REG_BASE),
		SZ_64K, MT_DEVICE},
};

struct map_desc  MEM_TABLE[MEM_TABLE_MAX_ENTRIES] = {
	{(INTER_REGS_BASE + MV_MPP_REGS_OFFSET),
		__phys_to_pfn(INTER_REGS_BASE + MV_MPP_REGS_OFFSET),  		SZ_64K,			MT_DEVICE},
	{(INTER_REGS_BASE + MV_SATA_REGS_OFFSET),
		__phys_to_pfn(INTER_REGS_BASE + MV_SATA_REGS_OFFSET),		SZ_8K,			MT_DEVICE},
	{(INTER_REGS_BASE + MV_PEX_IF_REGS_OFFSET(0)),
		__phys_to_pfn(INTER_REGS_BASE + MV_PEX_IF_REGS_OFFSET(0)),	SZ_64K,			MT_DEVICE},
	{ PEX0_IO_BASE,			__phys_to_pfn(PEX0_IO_BASE),		PEX0_IO_SIZE,		MT_DEVICE},
	{ PEX1_IO_BASE,			__phys_to_pfn(PEX1_IO_BASE),		PEX1_IO_SIZE,		MT_DEVICE},
	{ SPI_CS_BASE,			__phys_to_pfn(SPI_CS_BASE),		SPI_CS_SIZE,		MT_DEVICE},
	{ CRYPT_ENG_BASE(0),		__phys_to_pfn(CRYPT_ENG_BASE(0)),	CRYPT_ENG_SIZE,		MT_DEVICE},
	{ CRYPT_ENG_BASE(1),		__phys_to_pfn(CRYPT_ENG_BASE(1)),	CRYPT_ENG_SIZE,		MT_DEVICE},
	{ PNC_BM_PHYS_BASE,		__phys_to_pfn(PNC_BM_PHYS_BASE),	PNC_BM_SIZE,	        MV_DEVICE}
};

#else

struct map_desc  MEM_TABLE[] =	{
	/* no use for pex mem remap */	
	/*{ PEX0_MEM_BASE,  		__phys_to_pfn(PEX0_MEM_BASE),   	PEX0_MEM_SIZE,  	MT_DEVICE},*/
	{ INTER_REGS_BASE, 		__phys_to_pfn(INTER_REGS_BASE), 	SZ_1M,  	     	MT_DEVICE},
	{ PEX0_IO_BASE,   		__phys_to_pfn(PEX0_IO_BASE),   	 	PEX0_IO_SIZE,  		MT_DEVICE},
	{ PEX1_IO_BASE,			__phys_to_pfn(PEX1_IO_BASE),		PEX1_IO_SIZE,		MT_DEVICE},
	{ SPI_CS_BASE,			__phys_to_pfn(SPI_CS_BASE),		SPI_CS_SIZE,		MT_DEVICE},
	{ CRYPT_ENG_BASE(0),		__phys_to_pfn(CRYPT_ENG_BASE(0)),	CRYPT_ENG_SIZE,		MT_DEVICE},
	{ CRYPT_ENG_BASE(1),		__phys_to_pfn(CRYPT_ENG_BASE(1)),	CRYPT_ENG_SIZE,		MT_DEVICE},
	{ PNC_BM_PHYS_BASE,		__phys_to_pfn(PNC_BM_PHYS_BASE),	PNC_BM_SIZE,		MT_DEVICE}
};

#endif /* CONFIG_MV_INTERNAL_REGS_SELECTIVE_MAPPING */

MV_CPU_DEC_WIN SYSMAP_88F6500[] = {
	/* base low        base high    size       		WinNum     	enable */
	{{SDRAM_CS0_BASE,	0,	SDRAM_CS0_SIZE	},	0xFFFFFFFF,	DIS},	/* SDRAM_CS0 */
	{{SDRAM_CS1_BASE,	0,	SDRAM_CS1_SIZE	},	0xFFFFFFFF,	DIS},	/* SDRAM_CS1 */
	{{SDRAM_CS2_BASE,	0,	SDRAM_CS2_SIZE	},	0xFFFFFFFF,	DIS},	/* SDRAM_CS2 */
	{{SDRAM_CS3_BASE,	0,	SDRAM_CS3_SIZE	},	0xFFFFFFFF,	DIS},	/* SDRAM_CS3 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* DEVICE_CS0 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* DEVICE_CS1 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* DEVICE_CS2 */
	{{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* DEVICE_CS3 */
	{{PEX0_MEM_BASE,	0,	PEX0_MEM_SIZE	},	0x1,		EN},	/* PEX0_MEM */
	{{PEX0_IO_BASE,		0,	PEX0_IO_SIZE	},	0x0,		EN},	/* PEX0_IO */
	{{PEX1_MEM_BASE,	0,	PEX1_MEM_SIZE	},	0x3,		EN},	/* PEX1_MEM */
	{{PEX1_IO_BASE,		0,	PEX1_IO_SIZE	},	0x2,		EN},	/* PEX1_IO */
	{{INTER_REGS_BASE,	0,	INTER_REGS_SIZE	},	0xe,		EN},	/* INTER_REGS */
#ifdef CONFIG_MV_INCLUDE_NORFLASH_MTD
       	{{NAND_NOR_CS_BASE,	0,	NOR_CS_SIZE	},	0x4,		EN},	/* NOR_CS */
#else
	{{NAND_NOR_CS_BASE,	0,	NAND_CS_SIZE	},	0x4,		EN},	/* NAND_CS */
#endif
	{{SPI_CS_BASE,		0,	SPI_CS_SIZE	},	0x5,		EN},	/* SPI_CS0 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS1 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS2 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS3 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS4 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS5 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS6 */
        {{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_CS7 */
	{{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* SPI_B_CS0 */
	{{DEVICE_CS2_BASE,	0,	DEVICE_CS2_SIZE	},	0x6,		DIS},	/* BOOT_ROM_CS */
 	{{BOOTDEV_CS_BASE,	0,	BOOTDEV_CS_SIZE	},	0x7,		DIS},	/* DEV_BOOCS */
	{{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* CRYPT1_ENG */
	{{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* CRYPT2_ENG */
	{{PNC_BM_PHYS_BASE,	0,	PNC_BM_SIZE	},	0xa,		EN},	/* PNC_BM */
	{{PMT_GIGA_PHYS_BASE,	0,	PMT_MEM_SIZE	},	0xb,		EN},	/* PMT_GIGA */
	{{PMT_PON_PHYS_BASE,	0,	PMT_MEM_SIZE	},	0xc,		EN},	/* PMT_PON */
	{{TBL_UNUSED,		0,	TBL_UNUSED,	},	TBL_UNUSED,	DIS},	/* NFC_CTRL */
	{{TBL_TERM,		TBL_TERM, TBL_TERM	},	TBL_TERM,	TBL_TERM}
};

#if defined(CONFIG_MV_INTERNAL_REGS_SELECTIVE_MAPPING)

void __init mv_build_map_table(void)
{
	u32 unit;
	
	/* prepare consecutive mapping table */
	for(unit = 0; unit < ARRAY_SIZE(mv_internal_regs_map); unit++) {
		if(MV_TRUE == mvCtrlPwrClckGet(mv_internal_regs_map[unit].id, mv_internal_regs_map[unit].index)) {
			MEM_TABLE[entries].virtual = (INTER_REGS_BASE + mv_internal_regs_map[unit].offset);
			MEM_TABLE[entries].pfn = __phys_to_pfn(INTER_REGS_BASE + mv_internal_regs_map[unit].offset);
			MEM_TABLE[entries].length = mv_internal_regs_map[unit].size;
			MEM_TABLE[entries].type = MT_DEVICE;
			entries++;
		}
	}
}

#endif /* CONFIG_MV_INTERNAL_REGS_SELECTIVE_MAPPING */

MV_CPU_DEC_WIN* mv_sys_map(void)
{
	switch(mvBoardIdGet()) {
		case DB_88F6535_BP_ID:
		case RD_88F6510_SFU_ID:
		case RD_88F6560_GW_ID:
		case RD_88F6530_MDU_ID:
		case MI424WR_I_ID:
			return SYSMAP_88F6500;
		default:
			printk("ERROR: can't find system address map\n");
			return NULL;
        }
}


#if defined(CONFIG_MV_INCLUDE_CESA)
u32 mv_crypto_base_get(u8 chan)
{
	return CRYPT_ENG_BASE(chan);
}
#endif

void __init mv_map_io(void)
{
#if defined(CONFIG_MV_INTERNAL_REGS_SELECTIVE_MAPPING)
  	/* first, mappping AHB to MBUS entry for mvCtrlPwrClckGet access */
	iotable_init(AHB_TO_MBUS_MAP, ARRAY_SIZE(AHB_TO_MBUS_MAP));

	/* build dynamic mapping table  */
	mv_build_map_table();

	iotable_init(MEM_TABLE, entries);
#else
        iotable_init(MEM_TABLE, ARRAY_SIZE(MEM_TABLE));
#endif /* CONFIG_MV_INTERNAL_REGS_SELECTIVE_MAPPING */	
}


