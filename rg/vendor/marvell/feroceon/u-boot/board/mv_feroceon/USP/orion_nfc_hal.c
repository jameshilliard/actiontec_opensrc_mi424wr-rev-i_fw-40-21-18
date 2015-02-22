/*
 * drivers/mtd/nand/orion_nfc.c
 *
 * Copyright c 2005 Intel Corporation
 * Copyright c 2006 Marvell International Ltd.
 *
 * This driver is based on the PXA drivers/mtd/nand/pxa3xx_nand.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <common.h>
#if defined(CONFIG_CMD_NAND)
#define UBOOT_CODE

#ifndef UBOOT_CODE
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <asm/dma.h>
#include <plat/orion_nfc.h>
#include <asm/hardware/pxa-dma.h>
#endif /* UBOOT_CODE */

#ifdef UBOOT_CODE
#include <common.h>
#if defined(CONFIG_CMD_NAND)

#define dma_alloc_coherent(w,x,y,z) malloc(x)
#define kmalloc(x,y) malloc(x)
#define PAGE_SIZE 4096
#define pxa_request_dma_intr(...) 0
#define dev_err(x,y) printf(y)
#define init_completion(...)
#define HZ 100
struct completion{};

#include <nand.h>
#include <asm-generic/errno.h>
#include "orion_nfc.h"
#endif /* UBOOT_CODE */

#include "mvCommon.h"
#include "mvOs.h"
#include "pdma/mvPdma.h"
#include "pdma/mvPdmaRegs.h"
#include "nfc/mvNfc.h"
#include "nfc/mvNfcRegs.h"

#define NFC_DPRINT(x) 		//printf x
#define PRINT_LVL		KERN_DEBUG


#define	CHIP_DELAY_TIMEOUT		(20 * HZ/10)
#define NFC_MAX_NUM_OF_DESCR	(33) /* worst case in 8K ganaged */
#define NFC_8BIT1K_ECC_SPARE	(32)

#define NFC_SR_MASK		(0xfff)
#define NFC_SR_BBD_MASK		(NFC_SR_CS0_BBD_MASK | NFC_SR_CS1_BBD_MASK)


char *cmd_text[]= {
	"MV_NFC_CMD_READ_ID",
	"MV_NFC_CMD_READ_STATUS",
	"MV_NFC_CMD_ERASE",
	"MV_NFC_CMD_MULTIPLANE_ERASE",
	"MV_NFC_CMD_RESET",

	"MV_NFC_CMD_CACHE_READ_SEQ",
	"MV_NFC_CMD_CACHE_READ_RAND",
	"MV_NFC_CMD_EXIT_CACHE_READ",
	"MV_NFC_CMD_CACHE_READ_START",
	"MV_NFC_CMD_READ_MONOLITHIC",
	"MV_NFC_CMD_READ_MULTIPLE",
	"MV_NFC_CMD_READ_NAKED",
	"MV_NFC_CMD_READ_LAST_NAKED",
	"MV_NFC_CMD_READ_DISPATCH",

	"MV_NFC_CMD_WRITE_MONOLITHIC",
	"MV_NFC_CMD_WRITE_MULTIPLE",
	"MV_NFC_CMD_WRITE_NAKED",
	"MV_NFC_CMD_WRITE_LAST_NAKED",
	"MV_NFC_CMD_WRITE_DISPATCH",
	"MV_NFC_CMD_WRITE_DISPATCH_START",
	"MV_NFC_CMD_WRITE_DISPATCH_END",

	"MV_NFC_CMD_COUNT"	/* This should be the last enum */

};

MV_U32 pg_sz[NFC_PAGE_SIZE_MAX_CNT] = {512, 2048, 4096, 8192, 16384};

/* error code and state */
enum {
	ERR_NONE	= 0,
	ERR_DMABUSERR	= -1,
	ERR_CMD_TO	= -2,
	ERR_DATA_TO	= -3,
	ERR_DBERR	= -4,
	ERR_BBD		= -5,
};

enum {
	STATE_READY	= 0,
	STATE_CMD_HANDLE,
	STATE_DMA_READING,
	STATE_DMA_WRITING,
	STATE_DMA_DONE,
	STATE_PIO_READING,
	STATE_PIO_WRITING,
};

struct orion_nfc_info {
	struct platform_device	 *pdev;

	struct clk		*clk;
	void __iomem		*mmio_base;
	unsigned int		mmio_phys_base;

	unsigned int 		buf_start;
	unsigned int		buf_count;

	unsigned char		*data_buff;
	dma_addr_t 		data_buff_phys;
	size_t			data_buff_size;

	/* saved column/page_addr during CMD_SEQIN */
	int			seqin_column;
	int			seqin_page_addr;

	/* relate to the command */
	unsigned int		state;

	unsigned int		use_dma;	/* use DMA ? */



	/* flash information */
	unsigned int		tclk;		/* Clock supplied to NFC */
	unsigned int		nfc_width;	/* Width of NFC 16/8 bits */
	unsigned int		num_devs;	/* Number of NAND devices 
						   (2 for ganged mode).   */
	unsigned int		num_cs;		/* Number of NAND devices 
						   chip-selects.	  */
	MV_NFC_ECC_MODE		ecc_type;

	enum nfc_page_size	page_size;
	uint32_t 		page_per_block;	/* Pages per block (PG_PER_BLK) */	
	uint32_t 		flash_width;	/* Width of Flash memory (DWIDTH_M) */
	size_t	 		read_id_bytes;

	size_t			data_size;	/* data size in FIFO */
	size_t			read_size;
	int 			retcode;
	uint32_t		dscr;		/* IRQ events - status */
	struct completion 	cmd_complete;

	int			chained_cmd;
	uint32_t		column;
	uint32_t		page_addr;
	MV_NFC_CMD_TYPE		cmd;
	MV_NFC_CTRL		nfcCtrl;

	/* RW buffer chunks config */
	MV_U32			sgBuffAddr[MV_NFC_RW_MAX_BUFF_NUM];
	MV_U32			sgBuffSize[MV_NFC_RW_MAX_BUFF_NUM];
	MV_U32			sgNumBuffs;

	/* suspend / resume data */
	MV_U32			nfcUnitData[128];
	MV_U32			nfcDataLen;
	MV_U32			pdmaUnitData[128];
	MV_U32			pdmaDataLen;
};

/*
 * ECC Layout
 */

static struct nand_ecclayout ecc_latout_512B_hamming = {
	.eccbytes = 6,
	.eccpos = {8, 9, 10, 11, 12, 13 },
	.oobfree = { {2, 6} }
};

static struct nand_ecclayout ecc_layout_2KB_hamming = {
	.eccbytes = 24,
	.eccpos = {
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = { {2, 38} }
};

static struct nand_ecclayout ecc_layout_2KB_bch4bit = {
	.eccbytes = 32,
	.eccpos = {
		32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = { {2, 30} }
};

static struct nand_ecclayout ecc_layout_4KB_bch4bit = {
	.eccbytes = 64,
	.eccpos = {
		32,  33,  34,  35,  36,  37,  38,  39,
		40,  41,  42,  43,  44,  45,  46,  47,
		48,  49,  50,  51,  52,  53,  54,  55,
		56,  57,  58,  59,  60,  61,  62,  63,
		96,  97,  98,  99,  100, 101, 102, 103,
		104, 105, 106, 107, 108, 109, 110, 111,
		112, 113, 114, 115, 116, 117, 118, 119,
		120, 121, 122, 123, 124, 125, 126, 127},
	/* Bootrom looks in bytes 0 & 5 for bad blocks */
	.oobfree = { {1, 4}, {6, 26}, { 64, 32} }
};

static struct nand_ecclayout ecc_layout_8KB_bch4bit = {
	.eccbytes = 128,
	.eccpos = {
		32,  33,  34,  35,  36,  37,  38,  39,
		40,  41,  42,  43,  44,  45,  46,  47,
		48,  49,  50,  51,  52,  53,  54,  55,
		56,  57,  58,  59,  60,  61,  62,  63,

		96,  97,  98,  99,  100, 101, 102, 103,
		104, 105, 106, 107, 108, 109, 110, 111,
		112, 113, 114, 115, 116, 117, 118, 119,
		120, 121, 122, 123, 124, 125, 126, 127,

		160, 161, 162, 163, 164, 165, 166, 167,
		168, 169, 170, 171, 172, 173, 174, 175,
		176, 177, 178, 179, 180, 181, 182, 183,
		184, 185, 186, 187, 188, 189, 190, 191,

		224, 225, 226, 227, 228, 229, 230, 231,
		232, 233, 234, 235, 236, 237, 238, 239,
		240, 241, 242, 243, 244, 245, 246, 247,
		248, 249, 250, 251, 252, 253, 254, 255},

	/* Bootrom looks in bytes 0 & 5 for bad blocks */
	.oobfree = { {1, 4}, {6, 26}, { 64, 32}, {128, 32}, {192, 32} }
};

static struct nand_ecclayout ecc_layout_4KB_bch8bit = {
	.eccbytes = 64,
	.eccpos = {
		32,  33,  34,  35,  36,  37,  38,  39,
		40,  41,  42,  43,  44,  45,  46,  47,
		48,  49,  50,  51,  52,  53,  54,  55,
		56,  57,  58,  59,  60,  61,  62,  63},
	/* Bootrom looks in bytes 0 & 5 for bad blocks */
	.oobfree = { {1, 4}, {6, 26},  }
};

static struct nand_ecclayout ecc_layout_8KB_bch8bit = {
	.eccbytes = 160,
	.eccpos = {
		128, 129, 130, 131, 132, 133, 134, 135,
		136, 137, 138, 139, 140, 141, 142, 143,
		144, 145, 146, 147, 148, 149, 150, 151,
		152, 153, 154, 155, 156, 157, 158, 159},
	/* Bootrom looks in bytes 0 & 5 for bad blocks */
	.oobfree = { {1, 4}, {6, 122},  }
};

static struct nand_ecclayout ecc_layout_8KB_bch12bit = {
	.eccbytes = 0,
	.eccpos = { },
	/* Bootrom looks in bytes 0 & 5 for bad blocks */
	.oobfree = { {1, 4}, {6, 58}, }
};

static struct nand_ecclayout ecc_layout_16KB_bch12bit = {
	.eccbytes = 0,
	.eccpos = { },
	/* Bootrom looks in bytes 0 & 5 for bad blocks */
	.oobfree = { {1, 4}, {6, 122},  }
};

/*
 * Define bad block scan pattern when scanning a device for factory 
 * marked blocks.
 */
static uint8_t mv_scan_pattern[] = { 0xff, 0xff };

static struct nand_bbt_descr mv_sp_bb = {
	.options = NAND_BBT_SCANMVCUSTOM,
	.offs = 5,
	.len = 1,
	.pattern = mv_scan_pattern
};

static struct nand_bbt_descr mv_lp_bb = {
	.options = NAND_BBT_SCANMVCUSTOM,
	.offs = 0,
	.len = 2,
	.pattern = mv_scan_pattern
};

/*
 * Lookup Tables
 */

struct orion_nfc_naked_info {
	
	struct nand_ecclayout* 	ecc_layout;
	struct nand_bbt_descr*	bb_info;
	uint32_t		bb_bytepos;
	uint32_t		chunk_size;
	uint32_t		chunk_spare;
	uint32_t		chunk_cnt;
	uint32_t		last_chunk_size;
	uint32_t		last_chunk_spare;
};

			                     /* PageSize*/          /* ECc Type */
static struct orion_nfc_naked_info orion_nfc_naked_info_lkup[NFC_PAGE_SIZE_MAX_CNT][MV_NFC_ECC_MAX_CNT] = {
	/* 512B Pages */
	{{    	/* Hamming */
		&ecc_latout_512B_hamming, &mv_sp_bb, 512, 512, 16, 1, 0, 0
	}, { 	/* BCH 4bit */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}, { 	/* BCH 8bit */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}, { 	/* BCH 12bit */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}, { 	/* BCH 16bit */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}, { 	/* No ECC */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}},
	/* 2KB Pages */
	{{	/* Hamming */
		&ecc_layout_2KB_hamming, &mv_lp_bb, 2048, 2048, 40, 1, 0, 0
	}, { 	/* BCH 4bit */
		&ecc_layout_2KB_bch4bit, &mv_lp_bb, 2048, 2048, 32, 1, 0, 0
	}, { 	/* BCH 8bit */
		NULL, NULL, 2018, 1024, 0, 1, 1024, 32
	}, { 	/* BCH 12bit */
		NULL, NULL, 1988, 704, 0, 2, 640, 0
	}, { 	/* BCH 16bit */
		NULL, NULL, 1958, 512, 0, 4, 0, 32
	}, { 	/* No ECC */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}},
	/* 4KB Pages */
	{{	/* Hamming */
		NULL, 0, 0, 0, 0, 0, 0, 0
	}, { 	/* BCH 4bit */
		&ecc_layout_4KB_bch4bit, &mv_lp_bb, 4034, 2048, 32, 2, 0, 0
	}, { 	/* BCH 8bit */
		&ecc_layout_4KB_bch8bit, &mv_lp_bb, 4006, 1024, 0, 4, 0, 64
	}, { 	/* BCH 12bit */
		NULL, NULL, 3946, 704,  0, 5, 576, 32
	}, { 	/* BCH 16bit */
		NULL, NULL, 3886, 512, 0, 8, 0, 32
	}, { 	/* No ECC */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}},
	/* 8KB Pages */
	{{	/* Hamming */
		NULL, 0, 0, 0, 0, 0, 0, 0
	}, { 	/* BCH 4bit */
		&ecc_layout_8KB_bch4bit, &mv_lp_bb, 8102, 2048, 32, 4, 0, 0
	}, { 	/* BCH 8bit */
		&ecc_layout_8KB_bch8bit, &mv_lp_bb, 7982, 1024, 0, 8, 0, 160
	}, { 	/* BCH 12bit */
		&ecc_layout_8KB_bch12bit, &mv_lp_bb,7862, 704, 0, 11, 448, 64
	}, { 	/* BCH 16bit */
		NULL, NULL, 7742, 512, 0, 16, 0, 32
	}, { 	/* No ECC */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}},
	/* 16KB Pages */
	{{	/* Hamming */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}, { 	/* BCH 4bit */
		NULL, NULL, 15914, 2048, 32, 8, 0, 0
	}, { 	/* BCH 8bit */
		NULL, NULL, 15930, 1024, 0, 16, 0, 352
	}, { 	/* BCH 12bit */
		&ecc_layout_16KB_bch12bit, &mv_lp_bb, 15724, 704, 0, 23, 192, 128
	}, { 	/* BCH 16bit */
		NULL, NULL, 15484, 512, 0, 32, 0, 32
	}, { 	/* No ECC */
		NULL, NULL, 0, 0, 0, 0, 0, 0
	}}};
		

#define ECC_LAYOUT	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].ecc_layout)
#define BB_INFO		(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].bb_info)
#define	BB_BYTE_POS	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].bb_bytepos)
#define CHUNK_CNT	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].chunk_cnt)
#define CHUNK_SZ	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].chunk_size)
#define CHUNK_SPR	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].chunk_spare)
#define LST_CHUNK_SZ	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].last_chunk_size)
#define LST_CHUNK_SPR	(orion_nfc_naked_info_lkup[info->page_size][info->ecc_type].last_chunk_spare)

struct orion_nfc_cmd_info {
	
	uint32_t		events_p1;	/* post command events */
	uint32_t		events_p2;	/* post data events */
	MV_NFC_PIO_RW_MODE	rw;
};

static struct orion_nfc_cmd_info orion_nfc_cmd_info_lkup[MV_NFC_CMD_COUNT] = {
	/* Phase 1 interrupts */			/* Phase 2 interrupts */			/* Read/Write */  /* MV_NFC_CMD_xxxxxx */
	{(NFC_SR_RDDREQ_MASK), 				(0),						MV_NFC_PIO_READ}, /* READ_ID */
	{(NFC_SR_RDDREQ_MASK), 				(0),						MV_NFC_PIO_READ}, /* READ_STATUS */
	{(0), 						(MV_NFC_STATUS_RDY | MV_NFC_STATUS_BBD),	MV_NFC_PIO_NONE}, /* ERASE */
	{(0), 						(0), 						MV_NFC_PIO_NONE}, /* MULTIPLANE_ERASE */
	{(0), 						(MV_NFC_STATUS_RDY), 				MV_NFC_PIO_NONE}, /* RESET */
	{(0), 						(0), 						MV_NFC_PIO_READ}, /* CACHE_READ_SEQ */
	{(0), 						(0), 						MV_NFC_PIO_READ}, /* CACHE_READ_RAND */
	{(0), 						(0), 						MV_NFC_PIO_NONE}, /* EXIT_CACHE_READ */
	{(0), 						(0), 						MV_NFC_PIO_READ}, /* CACHE_READ_START */
	{(NFC_SR_RDDREQ_MASK | NFC_SR_UNCERR_MASK), 	(0), 						MV_NFC_PIO_READ}, /* READ_MONOLITHIC */
	{(0), 						(0),						MV_NFC_PIO_READ}, /* READ_MULTIPLE */
	{(NFC_SR_RDDREQ_MASK | NFC_SR_UNCERR_MASK), 	(0), 						MV_NFC_PIO_READ}, /* READ_NAKED */
	{(NFC_SR_RDDREQ_MASK | NFC_SR_UNCERR_MASK), 	(0), 						MV_NFC_PIO_READ}, /* READ_LAST_NAKED */
	{(0), 						(0), 						MV_NFC_PIO_NONE}, /* READ_DISPATCH */
	{(MV_NFC_STATUS_WRD_REQ), 			(MV_NFC_STATUS_RDY | MV_NFC_STATUS_BBD),	MV_NFC_PIO_WRITE},/* WRITE_MONOLITHIC */
	{(0), 						(0), 						MV_NFC_PIO_WRITE},/* WRITE_MULTIPLE */
	{(MV_NFC_STATUS_WRD_REQ),			(MV_NFC_STATUS_PAGED),				MV_NFC_PIO_WRITE},/* WRITE_NAKED */
	{(0), 						(0), 						MV_NFC_PIO_WRITE},/* WRITE_LAST_NAKED */
	{(0), 						(0), 						MV_NFC_PIO_NONE}, /* WRITE_DISPATCH */
	{(MV_NFC_STATUS_CMDD),				(0),						MV_NFC_PIO_NONE}, /* WRITE_DISPATCH_START */
	{(0),						(MV_NFC_STATUS_RDY | MV_NFC_STATUS_BBD), 	MV_NFC_PIO_NONE}, /* WRITE_DISPATCH_END */
};

static int prepare_read_prog_cmd(struct orion_nfc_info *info,
			int column, int page_addr)
{
	MV_U32 size;

	if (mvNfcFlashPageSizeGet(&info->nfcCtrl, &size, &info->data_size) 
	    != MV_OK)
		return -EINVAL;

	return 0;
}
int orion_nfc_wait_for_completion_timeout(struct orion_nfc_info *info, int timeout)
{
#ifndef UBOOT_CODE
	return wait_for_completion_timeout(&info->cmd_complete, timeout);
#else /* defined UBOOT_CODE */
	MV_U32 mask;
	ulong time;
	int i;
	
	/* Clear the interrupt and pass the status UP */
	mask = ~MV_REG_READ(NFC_CONTROL_REG) & 0xFFF;
	if (mask & 0x800)
		mask |= 0x1000;

	info->dscr = MV_REG_READ(NFC_STATUS_REG);
	time = get_timer(0);
	while ((info->dscr & mask) == 0) {
		if (get_timer (time) > timeout) {
			printf(">>> orion_nfc_wait_for_completion_timeout command timed out!\n");
			return 0;
		}
		udelay(10);
		info->dscr = MV_REG_READ(NFC_STATUS_REG);
	}

	/* Disable all interrupts */
	mvNfcIntrSet(&info->nfcCtrl, 0xFFF, MV_FALSE);

	NFC_DPRINT((PRINT_LVL ">>> orion_nfc_wait_for_completion_timeout(0x%x)\n", info->dscr));
	MV_REG_WRITE(NFC_STATUS_REG, info->dscr);

	return 1;
#endif /* UBOOT_CODE */
}
#ifndef UBOOT_CODE
static void orion_nfc_data_dma_irq(int irq, void *data)
{
	struct orion_nfc_info *info = data;
	uint32_t dcsr, intr;
	int channel = info->nfcCtrl.dataChanHndl.chanNumber;

	intr = MV_REG_READ(PDMA_INTR_CAUSE_REG);
	dcsr = MV_REG_READ(PDMA_CTRL_STATUS_REG(channel));
	MV_REG_WRITE(PDMA_CTRL_STATUS_REG(channel), dcsr);

	NFC_DPRINT((PRINT_LVL "orion_nfc_data_dma_irq(0x%x, 0x%x) - 1.\n", dcsr, intr));

	if(info->chained_cmd) {
		if (dcsr & DCSR_BUSERRINTR) {
			info->retcode = ERR_DMABUSERR;
			complete(&info->cmd_complete);
		}
		if ((info->state == STATE_DMA_READING) && (dcsr & DCSR_ENDINTR)) {
			info->state = STATE_READY;
			complete(&info->cmd_complete);
		}
		return;
	}

	if (dcsr & DCSR_BUSERRINTR) {
		info->retcode = ERR_DMABUSERR;
		complete(&info->cmd_complete);
	}

	if (info->state == STATE_DMA_WRITING) {
		info->state = STATE_DMA_DONE;
		mvNfcIntrSet(&info->nfcCtrl,  MV_NFC_STATUS_BBD | MV_NFC_STATUS_RDY , MV_TRUE);
	} else {
		info->state = STATE_READY;
		complete(&info->cmd_complete);
	}

	return;
}

static irqreturn_t orion_nfc_irq_pio(int irq, void *devid)
{
	struct orion_nfc_info *info = devid;

	/* Disable all interrupts */
	mvNfcIntrSet(&info->nfcCtrl, 0xFFF, MV_FALSE);

	/* Clear the interrupt and pass the status UP */
	info->dscr = MV_REG_READ(NFC_STATUS_REG);
	NFC_DPRINT((PRINT_LVL ">>> orion_nfc_irq_pio(0x%x)\n", info->dscr));
	MV_REG_WRITE(NFC_STATUS_REG, info->dscr);
	complete(&info->cmd_complete);

	return IRQ_HANDLED;
}


static irqreturn_t orion_nfc_irq_dma(int irq, void *devid)
{
	struct orion_nfc_info *info = devid;
	unsigned int status;

	status = MV_REG_READ(NFC_STATUS_REG);

	NFC_DPRINT((PRINT_LVL "orion_nfc_irq_dma(0x%x) - 1.\n", status));

	if(!info->chained_cmd) {
		if (status & (NFC_SR_RDDREQ_MASK | NFC_SR_UNCERR_MASK)) {
			if (status & NFC_SR_UNCERR_MASK)
				info->retcode = ERR_DBERR;
			mvNfcIntrSet(&info->nfcCtrl, NFC_SR_RDDREQ_MASK | NFC_SR_UNCERR_MASK, MV_FALSE);
			if (info->use_dma) {
				info->state = STATE_DMA_READING;
				mvNfcReadWrite(&info->nfcCtrl, info->cmd, (MV_U32*)info->data_buff, info->data_buff_phys);
			} else {
				info->state = STATE_PIO_READING;
				complete(&info->cmd_complete);
			}
		} else if (status & NFC_SR_WRDREQ_MASK) {
			mvNfcIntrSet(&info->nfcCtrl, NFC_SR_WRDREQ_MASK, MV_FALSE);
			if (info->use_dma) {
				info->state = STATE_DMA_WRITING;
				NFC_DPRINT((PRINT_LVL "Calling mvNfcReadWrite().\n"));
				if (mvNfcReadWrite(&info->nfcCtrl, info->cmd,
						   (MV_U32 *)info->data_buff,
						   info->data_buff_phys) 
				    != MV_OK)
					printk(KERN_ERR "mvNfcReadWrite() failed.\n");
			} else {
				info->state = STATE_PIO_WRITING;
				complete(&info->cmd_complete);
			}
		} else if (status & (NFC_SR_BBD_MASK | MV_NFC_CS0_CMD_DONE_INT |
				     NFC_SR_RDY0_MASK | MV_NFC_CS1_CMD_DONE_INT |
				     NFC_SR_RDY1_MASK)) {
			if (status & NFC_SR_BBD_MASK)
				info->retcode = ERR_BBD;
			mvNfcIntrSet(&info->nfcCtrl,  MV_NFC_STATUS_BBD |
					MV_NFC_STATUS_CMDD | MV_NFC_STATUS_RDY,
					MV_FALSE);
			info->state = STATE_READY;
			complete(&info->cmd_complete);
		}
	} else if (status & (NFC_SR_BBD_MASK | NFC_SR_RDY0_MASK |
				NFC_SR_RDY1_MASK | NFC_SR_UNCERR_MASK)) {
		if (status & (NFC_SR_BBD_MASK | NFC_SR_UNCERR_MASK))
			info->retcode = ERR_DBERR;
		mvNfcIntrSet(&info->nfcCtrl, MV_NFC_STATUS_BBD |
				MV_NFC_STATUS_RDY | MV_NFC_STATUS_CMDD,
				MV_FALSE);
		if ((info->state != STATE_DMA_READING) ||
		    (info->retcode == ERR_DBERR)) {
			info->state = STATE_READY;
			complete(&info->cmd_complete);
		}
	}
	MV_REG_WRITE(NFC_STATUS_REG, status);
	return IRQ_HANDLED;
}
#endif /* UBOOT_CODE */

static int orion_nfc_cmd_prepare(struct orion_nfc_info *info,
		MV_NFC_MULTI_CMD *descInfo, u32 *numCmds)
{
	MV_U32	i;
	MV_NFC_MULTI_CMD *currDesc;	

	currDesc = descInfo;
	if (info->cmd == MV_NFC_CMD_READ_MONOLITHIC) {
		/* Main Chunks */
		for (i=0; i<CHUNK_CNT; i++)
		{
			if (i == 0)
				currDesc->cmd = MV_NFC_CMD_READ_MONOLITHIC;
			else if ((i == (CHUNK_CNT-1)) && (LST_CHUNK_SZ == 0) && (LST_CHUNK_SPR == 0))
				currDesc->cmd = MV_NFC_CMD_READ_LAST_NAKED;
			else
				currDesc->cmd = MV_NFC_CMD_READ_NAKED;

			currDesc->pageAddr = info->page_addr;
			currDesc->pageCount = 1;
			currDesc->virtAddr = (MV_U32 *)(info->data_buff + (i * CHUNK_SZ));
			currDesc->physAddr = info->data_buff_phys + (i * CHUNK_SZ);
			currDesc->length = (CHUNK_SZ + CHUNK_SPR);

			if (CHUNK_SPR == 0)
				currDesc->numSgBuffs = 1;
			else
			{
				currDesc->numSgBuffs = 2;
				currDesc->sgBuffAddr[0] = (info->data_buff_phys + (i * CHUNK_SZ));
				currDesc->sgBuffAddrVirt[0] = (MV_U32 *)(info->data_buff + (i * CHUNK_SZ));
				currDesc->sgBuffSize[0] = CHUNK_SZ;
				currDesc->sgBuffAddr[1] = (info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (i * CHUNK_SPR));
				currDesc->sgBuffAddrVirt[1] = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (i * CHUNK_SPR));
				currDesc->sgBuffSize[1] = CHUNK_SPR;
			}

			currDesc++;
		}
		
		/* Last chunk if existing */
		if ((LST_CHUNK_SZ != 0) || (LST_CHUNK_SPR != 0))
		{
			currDesc->cmd = MV_NFC_CMD_READ_LAST_NAKED;
			currDesc->pageAddr = info->page_addr;
			currDesc->pageCount = 1;				
			currDesc->length = (LST_CHUNK_SPR + LST_CHUNK_SZ);

			if ((LST_CHUNK_SZ == 0) && (LST_CHUNK_SPR != 0))	/* Spare only */
			{
				currDesc->virtAddr = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT));
				currDesc->physAddr = info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT);
				currDesc->numSgBuffs = 1;
				currDesc->length = LST_CHUNK_SPR;
			}
			else if ((LST_CHUNK_SZ != 0) && (LST_CHUNK_SPR == 0))	/* Data only */
			{
				currDesc->virtAddr = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT));
				currDesc->physAddr = info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT);
				currDesc->numSgBuffs = 1;
				currDesc->length = LST_CHUNK_SZ;
			}
			else /* Both spare and data */
			{
				currDesc->numSgBuffs = 2;
				currDesc->sgBuffAddr[0] = (info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT));
				currDesc->sgBuffAddrVirt[0] = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT));
				currDesc->sgBuffSize[0] = LST_CHUNK_SZ;
				currDesc->sgBuffAddr[1] = (info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT));
				currDesc->sgBuffAddrVirt[1] =  (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT));
				currDesc->sgBuffSize[1] = LST_CHUNK_SPR;
			}
			currDesc++;
		}

		*numCmds = CHUNK_CNT + (((LST_CHUNK_SZ) || (LST_CHUNK_SPR)) ? 1 : 0);
	} else if (info->cmd == MV_NFC_CMD_WRITE_MONOLITHIC) {
		/* Write Dispatch */
		currDesc->cmd = MV_NFC_CMD_WRITE_DISPATCH_START;
		currDesc->pageAddr = info->page_addr;
		currDesc->pageCount = 1;
		currDesc->numSgBuffs = 1;
		currDesc->length = 0;
		currDesc++;

		/* Main Chunks */
		for (i=0; i<CHUNK_CNT; i++)
		{
			currDesc->cmd = MV_NFC_CMD_WRITE_NAKED;
			currDesc->pageAddr = info->page_addr;
			currDesc->pageCount = 1;
			currDesc->virtAddr = (MV_U32 *)(info->data_buff + (i * CHUNK_SZ));
			currDesc->physAddr = info->data_buff_phys + (i * CHUNK_SZ);
			currDesc->length = (CHUNK_SZ + CHUNK_SPR);

			if (CHUNK_SPR == 0)
				currDesc->numSgBuffs = 1;
			else
			{
				currDesc->numSgBuffs = 2;
				currDesc->sgBuffAddr[0] = (info->data_buff_phys + (i * CHUNK_SZ));
				currDesc->sgBuffAddrVirt[0] = (MV_U32 *)(info->data_buff + (i * CHUNK_SZ));
				currDesc->sgBuffSize[0] = CHUNK_SZ;
				currDesc->sgBuffAddr[1] = (info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (i * CHUNK_SPR));
				currDesc->sgBuffAddrVirt[1] = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (i * CHUNK_SPR));
				currDesc->sgBuffSize[1] = CHUNK_SPR;
			}

			currDesc++;
		}
		
		/* Last chunk if existing */
		if ((LST_CHUNK_SZ != 0) || (LST_CHUNK_SPR != 0))
		{
			currDesc->cmd = MV_NFC_CMD_WRITE_NAKED;
			currDesc->pageAddr = info->page_addr;
			currDesc->pageCount = 1;
			currDesc->length = (LST_CHUNK_SZ + LST_CHUNK_SPR);

			if ((LST_CHUNK_SZ == 0) && (LST_CHUNK_SPR != 0))	/* Spare only */
			{
				currDesc->virtAddr = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT));
				currDesc->physAddr = info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT);
				currDesc->numSgBuffs = 1;
			}
			else if ((LST_CHUNK_SZ != 0) && (LST_CHUNK_SPR == 0))	/* Data only */
			{
				currDesc->virtAddr = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT));
				currDesc->physAddr = info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT);
				currDesc->numSgBuffs = 1;
			}
			else /* Both spare and data */
			{
				currDesc->numSgBuffs = 2;
				currDesc->sgBuffAddr[0] = (info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT));
				currDesc->sgBuffAddrVirt[0] = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT));
				currDesc->sgBuffSize[0] = LST_CHUNK_SZ;
				currDesc->sgBuffAddr[1] = (info->data_buff_phys + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT));
				currDesc->sgBuffAddrVirt[1] = (MV_U32 *)(info->data_buff + (CHUNK_SZ * CHUNK_CNT) + LST_CHUNK_SZ + (CHUNK_SPR * CHUNK_CNT));
				currDesc->sgBuffSize[1] = LST_CHUNK_SPR;
			}
			currDesc++;
		}

		/* Write Dispatch END */
		currDesc->cmd = MV_NFC_CMD_WRITE_DISPATCH_END;
		currDesc->pageAddr = info->page_addr;
		currDesc->pageCount = 1;
		currDesc->numSgBuffs = 1;
		currDesc->length = 0;

		*numCmds = CHUNK_CNT + (((LST_CHUNK_SZ) || (LST_CHUNK_SPR)) ? 1 : 0) + 2;
	} else {
		descInfo[0].cmd = info->cmd;
		descInfo[0].pageAddr = info->page_addr;
		descInfo[0].pageCount = 1;
		descInfo[0].virtAddr = (MV_U32 *)info->data_buff;
		descInfo[0].physAddr = info->data_buff_phys;
		descInfo[0].numSgBuffs = 1;
		descInfo[0].length = info->data_size;
		*numCmds = 1;
	}

	return 0;
}

static int orion_nfc_do_cmd_dma(struct orion_nfc_info *info,
		uint32_t event)
{
	uint32_t ndcr;
	int ret, timeout = CHIP_DELAY_TIMEOUT;
	MV_STATUS status;
	MV_U32	numCmds;

	/* static allocation to avoid stack overflow*/
	static MV_NFC_MULTI_CMD descInfo[NFC_MAX_NUM_OF_DESCR];

	/* Clear all status bits. */
	MV_REG_WRITE(NFC_STATUS_REG, NFC_SR_MASK);

	mvNfcIntrSet(&info->nfcCtrl, event, MV_TRUE);

	NFC_DPRINT((PRINT_LVL "\nAbout to issue dma cmd %d (cs %d) - 0x%x.\n",
				info->cmd, info->nfcCtrl.currCs,
				MV_REG_READ(NFC_CONTROL_REG)));
	if ((info->cmd == MV_NFC_CMD_READ_MONOLITHIC) ||
	    (info->cmd == MV_NFC_CMD_READ_ID) ||
	    (info->cmd == MV_NFC_CMD_READ_STATUS))
		info->state = STATE_DMA_READING;
	else
		info->state = STATE_CMD_HANDLE;
	info->chained_cmd = 1;

	orion_nfc_cmd_prepare(info, descInfo, &numCmds);

	status = mvNfcCommandMultiple(&info->nfcCtrl,descInfo, numCmds);
	if (status != MV_OK) {
		printk(KERN_ERR "nfcCmdMultiple() failed for cmd %d (%d).\n",
				info->cmd, status);
		goto fail;
	}

	NFC_DPRINT((PRINT_LVL "After issue command %d - 0x%x.\n",
				info->cmd, MV_REG_READ(NFC_STATUS_REG)));

	ret = orion_nfc_wait_for_completion_timeout(info, timeout);
	if (!ret) {
		printk(KERN_ERR "Cmd %d execution timed out (0x%x) - cs %d.\n",
				info->cmd, MV_REG_READ(NFC_STATUS_REG),
				info->nfcCtrl.currCs);
		info->retcode = ERR_CMD_TO;
		goto fail_stop;
	}

	mvNfcIntrSet(&info->nfcCtrl, event | MV_NFC_STATUS_CMDD, MV_FALSE);

	while (MV_PDMA_CHANNEL_STOPPED !=
			mvPdmaChannelStateGet(&info->nfcCtrl.dataChanHndl)) {
		if (info->retcode == ERR_NONE)
			BUG();

	}

	return 0;

fail_stop:
	ndcr = MV_REG_READ(NFC_CONTROL_REG);
	MV_REG_WRITE(NFC_CONTROL_REG, ndcr & ~NFC_CTRL_ND_RUN_MASK);
	udelay(10);
fail:
	return -ETIMEDOUT;
}

static int orion_nfc_error_check(struct orion_nfc_info *info)
{
	switch (info->cmd) {
		case MV_NFC_CMD_ERASE:
		case MV_NFC_CMD_MULTIPLANE_ERASE:
		case MV_NFC_CMD_WRITE_MONOLITHIC:
		case MV_NFC_CMD_WRITE_MULTIPLE:
		case MV_NFC_CMD_WRITE_NAKED:
		case MV_NFC_CMD_WRITE_LAST_NAKED:
		case MV_NFC_CMD_WRITE_DISPATCH:
		case MV_NFC_CMD_WRITE_DISPATCH_START:
		case MV_NFC_CMD_WRITE_DISPATCH_END:
			if (info->dscr & (MV_NFC_CS0_BAD_BLK_DETECT_INT | MV_NFC_CS1_BAD_BLK_DETECT_INT)) {
				info->retcode = ERR_BBD;
				return 1;
			}
			break;
		
		case MV_NFC_CMD_CACHE_READ_SEQ:
		case MV_NFC_CMD_CACHE_READ_RAND:
		case MV_NFC_CMD_EXIT_CACHE_READ:
		case MV_NFC_CMD_CACHE_READ_START:
		case MV_NFC_CMD_READ_MONOLITHIC:
		case MV_NFC_CMD_READ_MULTIPLE:
		case MV_NFC_CMD_READ_NAKED:
		case MV_NFC_CMD_READ_LAST_NAKED:
		case MV_NFC_CMD_READ_DISPATCH:
			if (info->dscr & MV_NFC_UNCORR_ERR_INT) {
				info->dscr = ERR_DBERR;
				return 1;
			}
			break;

		default:
			break;
	}

	info->retcode = ERR_NONE;
	return 0;
}

/* ==================================================================================================
 *           STEP  1		|   STEP  2   |   STEP  3   |   STEP  4   |   STEP  5   |   STEP 6
 *           COMMAND		|   WAIT FOR  |   CHK ERRS  |     PIO     |   WAIT FOR  |   CHK ERRS
 * =========================|=============|=============|=============|=============|============
 *   READ MONOLITHIC		|   RDDREQ    |   UNCERR    |    READ     |     NONE    |    NONE
 *   READ NAKED				|   RDDREQ    |   UNCERR    |    READ     |     NONE    |    NONE
 *   READ LAST NAKED		|   RDDREQ    |   UNCERR    |    READ     |     NONE    |    NONE
 *   WRITE MONOLITHIC		|   WRDREQ    |    NONE     |    WRITE    |     RDY     |    BBD
 *   WRITE DISPATCH START	|   CMDD      |    NONE     |    NONE     |     NONE    |    NONE
 *   WRITE NAKED			|   WRDREQ    |    NONE     |    WRITE    |     PAGED   |    NONE
 *   WRITE DISPATCH END		|   NONE      |    NONE     |    NONE     |     RDY     |    BBD
 *   ERASE					|   NONE      |    NONE     |    NONE     |     RDY     |    BBD
 *   READ ID				|   RDDREQ    |    NONE     |    READ     |     NONE    |    NONE
 *   READ STAT				|   RDDREQ    |    NONE     |    READ     |     NONE    |    NONE
 *   RESET					|   NONE      |    NONE     |    NONE     |     RDY     |    NONE
 */
static int orion_nfc_do_cmd_pio(struct orion_nfc_info *info)
{
	int timeout = CHIP_DELAY_TIMEOUT;
	MV_STATUS status;
	MV_U32	i, j, numCmds;
	MV_U32 ndcr;

	/* static allocation to avoid stack overflow */
	static MV_NFC_MULTI_CMD descInfo[NFC_MAX_NUM_OF_DESCR];

	/* Clear all status bits */
	MV_REG_WRITE(NFC_STATUS_REG, NFC_SR_MASK);	
	
	NFC_DPRINT((PRINT_LVL "\nStarting PIO command %d (cs %d) - NDCR=0x%08x\n",
				info->cmd, info->nfcCtrl.currCs, MV_REG_READ(NFC_CONTROL_REG)));

	/* Build the chain of commands */
	orion_nfc_cmd_prepare(info, descInfo, &numCmds);
	NFC_DPRINT((PRINT_LVL "Prepared %d commands in sequence\n", numCmds));

	/* Execute the commands */
	for (i=0; i < numCmds; i++) {
		/* Verify that command is supported in PIO mode */
		if ((orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p1 == 0) &&
		    (orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p2 == 0)) {
			goto fail_stop;
		}
		
		/* clear the return code */
		info->dscr = 0;

		/* STEP1: Initiate the command */
		if ((status = mvNfcCommandPio(&info->nfcCtrl, &descInfo[i], MV_FALSE)) != MV_OK) {
			printk(KERN_ERR "mvNfcCommandPio() failed for command %d (%d).\n", descInfo[i].cmd, status);
			goto fail_stop;
		}
		NFC_DPRINT((PRINT_LVL "After issue command %d (NDSR=0x%x)\n", descInfo[i].cmd, MV_REG_READ(NFC_STATUS_REG)));
	
		/* Check if command phase interrupts events are needed */
		if (orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p1) {
			/* Enable necessary interrupts for command phase */
			NFC_DPRINT((PRINT_LVL "Enabling part1 interrupts (IRQs 0x%x)\n", orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p1));
			mvNfcIntrSet(&info->nfcCtrl, orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p1, MV_TRUE);	
			
			/* STEP2: wait for interrupt */
			
			if (!orion_nfc_wait_for_completion_timeout(info, timeout)) {
				printk(KERN_ERR "command %d execution timed out (CS %d, NDCR=0x%x, NDSR=0x%x).\n",
				       descInfo[i].cmd, info->nfcCtrl.currCs, MV_REG_READ(NFC_CONTROL_REG), MV_REG_READ(NFC_STATUS_REG));
				info->retcode = ERR_CMD_TO;
				goto fail_stop;
			}
		
			/* STEP3: Check for errors */
			if (orion_nfc_error_check(info)) {
				NFC_DPRINT((PRINT_LVL "Command level errors (DSCR=%08x, retcode=%d)\n", info->dscr, info->retcode));
				goto fail_stop;
			}
		}		
				
		/* STEP4: PIO Read/Write data if needed */
		if (descInfo[i].numSgBuffs > 1)
		{
			for (j=0; j< descInfo[i].numSgBuffs; j++) {
				NFC_DPRINT((PRINT_LVL "Starting SG#%d PIO Read/Write (%d bytes, R/W mode %d)\n", j, 
					    descInfo[i].sgBuffSize[j], orion_nfc_cmd_info_lkup[descInfo[i].cmd].rw));
				mvNfcReadWritePio(&info->nfcCtrl, descInfo[i].sgBuffAddrVirt[j], 
						  descInfo[i].sgBuffSize[j], orion_nfc_cmd_info_lkup[descInfo[i].cmd].rw);
			}
		}
		else {
			NFC_DPRINT((PRINT_LVL "Starting nonSG PIO Read/Write (%d bytes, R/W mode %d)\n", 
				    descInfo[i].length, orion_nfc_cmd_info_lkup[descInfo[i].cmd].rw));
			mvNfcReadWritePio(&info->nfcCtrl, descInfo[i].virtAddr, 
					  descInfo[i].length, orion_nfc_cmd_info_lkup[descInfo[i].cmd].rw);
		}

		/* check if data phase events are needed */
		if (orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p2) {
			/* Enable the RDY interrupt to close the transaction */
			NFC_DPRINT((PRINT_LVL "Enabling part2 interrupts (IRQs 0x%x)\n", orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p2));
			mvNfcIntrSet(&info->nfcCtrl, orion_nfc_cmd_info_lkup[descInfo[i].cmd].events_p2, MV_TRUE);			

			/* STEP5: Wait for transaction to finish */
			
			if (!orion_nfc_wait_for_completion_timeout(info, timeout)) {
				printk(KERN_ERR "command %d execution timed out (NDCR=0x%08x, NDSR=0x%08x, NDECCCTRL=0x%08x)\n", descInfo[i].cmd, 
						MV_REG_READ(NFC_CONTROL_REG), MV_REG_READ(NFC_STATUS_REG), MV_REG_READ(NFC_ECC_CONTROL_REG));
				info->retcode = ERR_DATA_TO;
				goto fail_stop;
			}
		
			/* STEP6: Check for errors BB errors (in erase) */
			if (orion_nfc_error_check(info)) {
				NFC_DPRINT((PRINT_LVL "Data level errors (DSCR=0x%08x, retcode=%d)\n", info->dscr, info->retcode));
				goto fail_stop;
			}
		}
	
		/* Fallback - in case the NFC did not reach the idle state */
		ndcr = MV_REG_READ(NFC_CONTROL_REG);
		if (ndcr & NFC_CTRL_ND_RUN_MASK) {
			//printk(KERN_DEBUG "WRONG NFC STAUS: command %d, NDCR=0x%08x, NDSR=0x%08x, NDECCCTRL=0x%08x)\n", 
		    //   	info->cmd, MV_REG_READ(NFC_CONTROL_REG), MV_REG_READ(NFC_STATUS_REG), MV_REG_READ(NFC_ECC_CONTROL_REG));
			MV_REG_WRITE(NFC_CONTROL_REG, (ndcr & ~NFC_CTRL_ND_RUN_MASK));
		}
	}

	NFC_DPRINT((PRINT_LVL "Command done (NDCR=0x%08x, NDSR=0x%08x)\n", MV_REG_READ(NFC_CONTROL_REG), MV_REG_READ(NFC_STATUS_REG)));
	info->retcode = ERR_NONE;	
	
	return 0;

fail_stop:
	ndcr = MV_REG_READ(NFC_CONTROL_REG);
	if (ndcr & NFC_CTRL_ND_RUN_MASK) {
		printk(KERN_ERR "WRONG NFC STAUS: command %d, NDCR=0x%08x, NDSR=0x%08x, NDECCCTRL=0x%08x)\n", 
		       info->cmd, MV_REG_READ(NFC_CONTROL_REG), MV_REG_READ(NFC_STATUS_REG), MV_REG_READ(NFC_ECC_CONTROL_REG));
		MV_REG_WRITE(NFC_CONTROL_REG, (ndcr & ~NFC_CTRL_ND_RUN_MASK));
	}
	mvNfcIntrSet(&info->nfcCtrl, 0xFFF, MV_FALSE);
	udelay(10);
	return -ETIMEDOUT;
}

static int orion_nfc_dev_ready(struct mtd_info *mtd)
{
	return (MV_REG_READ(NFC_STATUS_REG) & (NFC_SR_RDY0_MASK | NFC_SR_RDY1_MASK)) ? 1 : 0;
}

static inline int is_buf_blank(uint8_t *buf, size_t len)
{
	for (; len > 0; len--)
		if (*buf++ != 0xff)
			return 0;
	return 1;
}

static void orion_nfc_cmdfunc(struct mtd_info *mtd, unsigned command,
				int column, int page_addr)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	int ret;

	info->data_size = 0;
	info->state = STATE_READY;
	info->chained_cmd = 0;
	info->retcode = ERR_NONE;

	init_completion(&info->cmd_complete);

	switch (command) {
	case NAND_CMD_READOOB:
		info->buf_count = mtd->writesize + mtd->oobsize;
		info->buf_start = mtd->writesize + column;
		info->cmd = MV_NFC_CMD_READ_MONOLITHIC;
		info->column = column;
		info->page_addr = page_addr;
		if (prepare_read_prog_cmd(info, column, page_addr))
			break;

		if (info->use_dma)
			orion_nfc_do_cmd_dma(info, MV_NFC_STATUS_RDY | NFC_SR_UNCERR_MASK);
		else
			orion_nfc_do_cmd_pio(info);

		/* We only are OOB, so if the data has error, does not matter */
		if (info->retcode == ERR_DBERR)
			info->retcode = ERR_NONE;
		break;

	case NAND_CMD_READ0:
		info->buf_start = column;
		info->buf_count = mtd->writesize + mtd->oobsize;
		memset(info->data_buff, 0xff, info->buf_count);
		info->cmd = MV_NFC_CMD_READ_MONOLITHIC;
		info->column = column;
		info->page_addr = page_addr;

		if (prepare_read_prog_cmd(info, column, page_addr))
			break;

		if (info->use_dma)
			orion_nfc_do_cmd_dma(info, MV_NFC_STATUS_RDY | NFC_SR_UNCERR_MASK);
		else
			orion_nfc_do_cmd_pio(info);

		if (info->retcode == ERR_DBERR) {
			/* for blank page (all 0xff), HW will calculate its ECC as
			 * 0, which is different from the ECC information within
			 * OOB, ignore such double bit errors
			 */
			if (is_buf_blank(info->data_buff, mtd->writesize))
				info->retcode = ERR_NONE;
			else
				printk(PRINT_LVL "%s: retCode == ERR_DBERR\n", __FUNCTION__);
		}
		break;
	case NAND_CMD_SEQIN:
		info->buf_start = column;
		info->buf_count = mtd->writesize + mtd->oobsize;
		memset(info->data_buff + mtd->writesize, 0xff, mtd->oobsize);

		/* save column/page_addr for next CMD_PAGEPROG */
		info->seqin_column = column;
		info->seqin_page_addr = page_addr;
		break;
	case NAND_CMD_PAGEPROG:
		info->column = info->seqin_column;
		info->page_addr = info->seqin_page_addr;
		info->cmd = MV_NFC_CMD_WRITE_MONOLITHIC;
		if (prepare_read_prog_cmd(info,
				info->seqin_column, info->seqin_page_addr)) {
			printk(KERN_ERR "prepare_read_prog_cmd() failed.\n");
			break;
		}
	
		if (info->use_dma)
			orion_nfc_do_cmd_dma(info, MV_NFC_STATUS_RDY);
		else
			orion_nfc_do_cmd_pio(info);

		break;
	case NAND_CMD_ERASE1:
		info->column = 0;
		info->page_addr = page_addr;
		info->cmd = MV_NFC_CMD_ERASE;

		if (info->use_dma)
			orion_nfc_do_cmd_dma(info, MV_NFC_STATUS_BBD | MV_NFC_STATUS_RDY);
		else
			orion_nfc_do_cmd_pio(info);

		break;
	case NAND_CMD_ERASE2:
		break;
	case NAND_CMD_READID:
	case NAND_CMD_STATUS:
		info->buf_start = 0;
		info->buf_count = (command == NAND_CMD_READID) ?
				info->read_id_bytes : 1;
		info->data_size = 8;
		info->column = 0;
		info->page_addr = 0;
		info->cmd = (command == NAND_CMD_READID) ?
			MV_NFC_CMD_READ_ID : MV_NFC_CMD_READ_STATUS;

		if (info->use_dma)
			orion_nfc_do_cmd_dma(info,MV_NFC_STATUS_RDY);
		else
			orion_nfc_do_cmd_pio(info);

		break;
	case NAND_CMD_RESET:
		info->column = 0;
		info->page_addr = 0;
		info->cmd = MV_NFC_CMD_RESET;

		if (info->use_dma)
			ret = orion_nfc_do_cmd_dma(info, MV_NFC_STATUS_CMDD);
		else
			ret = orion_nfc_do_cmd_pio(info);

		if (ret == 0) {
			int timeout = 2;
			uint32_t ndcr;

			while (timeout--) {
				if (MV_REG_READ(NFC_STATUS_REG) & (NFC_SR_RDY0_MASK | NFC_SR_RDY1_MASK))
					break;
				udelay(10000);
			}

			ndcr = MV_REG_READ(NFC_CONTROL_REG);
			MV_REG_WRITE(NFC_CONTROL_REG, ndcr & ~NFC_CTRL_ND_RUN_MASK);
		}
		break;
	default:
		printk(KERN_ERR "non-supported command.\n");
		break;
	}

	if (info->retcode == ERR_DBERR) {
		printk(KERN_ERR "double bit error @ page %08x (%d)\n",
				page_addr, info->cmd);
		info->retcode = ERR_NONE;
	}
}

static uint8_t orion_nfc_read_byte(struct mtd_info *mtd)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	char retval = 0xFF;

	if (info->buf_start < info->buf_count)
		/* Has just send a new command? */
		retval = info->data_buff[info->buf_start++];
	return retval;
}

static u16 orion_nfc_read_word(struct mtd_info *mtd)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	u16 retval = 0xFFFF;

	if (!(info->buf_start & 0x01) && info->buf_start < info->buf_count) {
		retval = *((u16 *)(info->data_buff+info->buf_start));
		info->buf_start += 2;
	}
	else
		printk(KERN_ERR "\n%s: returning 0xFFFF (%d, %d).\n",__FUNCTION__, info->buf_start,info->buf_count);

	return retval;
}

static void orion_nfc_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(buf, info->data_buff + info->buf_start, real_len);
	info->buf_start += real_len;
}

static void orion_nfc_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(info->data_buff + info->buf_start, buf, real_len);
	info->buf_start += real_len;
}

static int orion_nfc_verify_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	return 0;
}

static void orion_nfc_select_chip(struct mtd_info *mtd, int chip)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_0 + chip);
	return;
}

static int orion_nfc_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;

	/* orion_nfc_send_command has waited for command complete */
	if (this->state == FL_WRITING || this->state == FL_ERASING) {
		if (info->retcode == ERR_NONE)
			return 0;
		else {
			/*
			 * any error make it return 0x01 which will tell
			 * the caller the erase and write fail
			 */
			return 0x01;
		}
	}

	return 0;
}

static void orion_nfc_ecc_hwctl(struct mtd_info *mtd, int mode)
{
	return;
}

static int orion_nfc_ecc_calculate(struct mtd_info *mtd,
		const uint8_t *dat, uint8_t *ecc_code)
{
	return 0;
}

static int orion_nfc_ecc_correct(struct mtd_info *mtd,
		uint8_t *dat, uint8_t *read_ecc, uint8_t *calc_ecc)
{
	struct orion_nfc_info *info = (struct orion_nfc_info *)((struct nand_chip *)mtd->priv)->priv;
	/*
	 * Any error include ERR_SEND_CMD, ERR_DBERR, ERR_BUSERR, we
	 * consider it as a ecc error which will tell the caller the
	 * read fail We have distinguish all the errors, but the
	 * nand_read_ecc only check this function return value
	 */
	if (info->retcode != ERR_NONE)
		return -1;

	return 0;
}

static int orion_nfc_detect_flash(struct orion_nfc_info *info)
{
	MV_U32 my_page_size;

	mvNfcFlashPageSizeGet(&info->nfcCtrl, &my_page_size, NULL);

	/* Translate page size to enum */
	switch (my_page_size)
	{
		case 512:
			info->page_size = NFC_PAGE_512B;
			break;

		case 2048:
			info->page_size = NFC_PAGE_2KB;
			break;

		case 4096:
			info->page_size = NFC_PAGE_4KB;
			break;
		
		case 8192:
			info->page_size = NFC_PAGE_8KB;
			break;

		case 16384:
			info->page_size = NFC_PAGE_16KB;
			break;

		default:
			return -EINVAL;
	}

	info->flash_width = info->nfc_width;
	if (info->flash_width != 16 && info->flash_width != 8)
		return -EINVAL;

	/* calculate flash information */
	info->read_id_bytes = (pg_sz[info->page_size] >= 2048) ? 4 : 2;

	return 0;
}

/* the maximum possible buffer size for ganaged 8K page with OOB data
 * is: 2 * (8K + Spare) ==> to be alligned allocate 5 MMU (4K) pages
 */
#define MAX_BUFF_SIZE	(PAGE_SIZE * 5)

static int orion_nfc_init_buff(struct orion_nfc_info *info)
{
	struct platform_device *pdev = info->pdev;
	int ret;
	if (info->use_dma == 0) {
		info->data_buff = kmalloc(MAX_BUFF_SIZE, GFP_KERNEL);
		if (info->data_buff == NULL)
			return -ENOMEM;
		return 0;
	}

	info->data_buff =  dma_alloc_coherent(&pdev->dev, MAX_BUFF_SIZE,
				&info->data_buff_phys, GFP_KERNEL);
	if (info->data_buff == NULL) {
		dev_err(&pdev->dev, "failed to allocate dma buffer\n");
		return -ENOMEM;
	}
	memset(info->data_buff, 0xff, MAX_BUFF_SIZE);

	ret = pxa_request_dma_intr ("nand-data", info->nfcCtrl.dataChanHndl.chanNumber,
			orion_nfc_data_dma_irq, info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request PDMA IRQ\n");
		return -ENOMEM;
	}
	return 0;
}

static uint8_t mv_bbt_pattern[] = {'M', 'V', 'B', 'b', 't', '0' };
static uint8_t mv_mirror_pattern[] = {'1', 't', 'b', 'B', 'V', 'M' };

static struct nand_bbt_descr mvbbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	8,
	.len = 6,
	.veroffs = 14,
	.maxblocks = 8,		/* Last 8 blocks in each chip */
	.pattern = mv_bbt_pattern
};

static struct nand_bbt_descr mvbbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	8,
	.len = 6,
	.veroffs = 14,
	.maxblocks = 8,		/* Last 8 blocks in each chip */
	.pattern = mv_mirror_pattern
};


static int orion_nfc_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	uint8_t buf[6] = {0, 0, 0, 0, 0, 0};
	int block, ret = 0;
	loff_t page_addr;

	/* Get block number */
	block = (int)(ofs >> chip->bbt_erase_shift);
	if (chip->bbt)
		chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);
	ret = nand_update_bbt(mtd, ofs);

	if (ret == 0) {
		/* Get address of the next block */
		ofs += mtd->erasesize;
		ofs &= ~(mtd->erasesize - 1);

		/* Get start of oob in last page */
		ofs -= mtd->oobsize;

		page_addr = ofs;
		do_div(page_addr, mtd->writesize);

		orion_nfc_cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize,
				page_addr);
		orion_nfc_write_buf(mtd, buf, 6);
		orion_nfc_cmdfunc(mtd, NAND_CMD_PAGEPROG, 0, page_addr);
	}

	return ret;
}


static void orion_nfc_init_nand(struct nand_chip *nand, struct orion_nfc_info *info)
{
	if (info->nfc_width == 16)
		nand->options 	= (NAND_USE_FLASH_BBT | NAND_BUSWIDTH_16);
	else
		nand->options 	= NAND_USE_FLASH_BBT;
	nand->num_devs		= info->num_devs;
	nand->oobsize_ovrd	= ((CHUNK_SPR * CHUNK_CNT) + LST_CHUNK_SPR);
	nand->bb_location	= BB_BYTE_POS;
	nand->bb_page		= mvNfcBadBlockPageNumber(&info->nfcCtrl);
	nand->waitfunc		= orion_nfc_waitfunc;
	nand->select_chip	= orion_nfc_select_chip;
	nand->dev_ready		= orion_nfc_dev_ready;
	nand->cmdfunc		= orion_nfc_cmdfunc;
	nand->read_word		= orion_nfc_read_word;
	nand->read_byte		= orion_nfc_read_byte;
	nand->read_buf		= orion_nfc_read_buf;
	nand->write_buf		= orion_nfc_write_buf;
	nand->verify_buf	= orion_nfc_verify_buf;
	nand->block_markbad	= orion_nfc_markbad;
	nand->ecc.mode		= NAND_ECC_HW;
	nand->ecc.hwctl		= orion_nfc_ecc_hwctl;
	nand->ecc.calculate	= orion_nfc_ecc_calculate;
	nand->ecc.correct	= orion_nfc_ecc_correct;
	nand->ecc.size		= pg_sz[info->page_size];
	nand->ecc.layout	= ECC_LAYOUT;
	nand->bbt_td 		= &mvbbt_main_descr;
	nand->bbt_md 		= &mvbbt_mirror_descr;
	nand->badblock_pattern	= BB_INFO;
	nand->chip_delay 	= 25;
}

#ifndef UBOOT_CODE
static int orion_nfc_probe(struct platform_device *pdev)
{
	struct nfc_platform_data *pdata;
	struct orion_nfc_info *info;
	struct nand_chip *this;
	struct mtd_info *mtd;
	struct resource *r;
	int ret = 0, irq;
	char * stat[2] = {"Disabled", "Enabled"};
	char * ecc_stat[] = {"Hamming", "BCH 4bit", "BCH 8bit", "BCH 12bit", "BCH 16bit", "No"};
	MV_NFC_INFO nfcInfo;

	pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -ENODEV;
	}

	/* Set global parameters based on platform data */
	//if (pdata->use_dma) use_dma = 1;

	dev_info(&pdev->dev, "Initialize HAL based NFC in %dbit mode with DMA %s using %s ECC\n",
			pdata->nfc_width, stat[pdata->use_dma], ecc_stat[pdata->ecc_type]);

	mtd = kzalloc(sizeof(struct mtd_info) + sizeof(struct orion_nfc_info),
			GFP_KERNEL);
	if (!mtd) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	info = (struct orion_nfc_info *)(&mtd[1]);
	info->pdev = pdev;

	this = &info->nand_chip;
	mtd->priv = info;
	mtd->owner = THIS_MODULE;

	/* Copy all necessary information from platform data */
	info->use_dma = pdata->use_dma;
	info->ecc_type = pdata->ecc_type;

        info->clk = clk_get_sys("dove-nand", NULL);
        if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to get nand clock\n");
		ret = PTR_ERR(info->clk);
		goto fail_free_mtd;
        }
        clk_enable(info->clk);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		ret = -ENXIO;
		goto fail_put_clk;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		ret = -ENODEV;
		goto fail_put_clk;
	}

	r = devm_request_mem_region(&pdev->dev, r->start, r->end - r->start + 1,
				    pdev->name);
	if (r == NULL) {
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto fail_put_clk;
	}

	info->mmio_base = devm_ioremap(&pdev->dev, r->start,
				       r->end - r->start + 1);
	if (info->mmio_base == NULL) {
		dev_err(&pdev->dev, "ioremap() failed\n");
		ret = -ENODEV;
		goto fail_put_clk;
	}

	info->mmio_phys_base = r->start;

#if 0
	if (mvPdmaHalInit(MV_PDMA_MAX_CHANNELS_NUM) != MV_OK) {
		dev_err(&pdev->dev, "mvPdmaHalInit() failed.\n");
		goto fail_put_clk;
	}
#endif
	/* Initialize NFC HAL */
	nfcInfo.ioMode = (pdata->use_dma ? MV_NFC_PDMA_ACCESS : MV_NFC_PIO_ACCESS);
	nfcInfo.eccMode = pdata->ecc_type;
		
	if(pdata->num_devs == 1)
		nfcInfo.ifMode = ((pdata->nfc_width == 8) ? MV_NFC_IF_1X8 : MV_NFC_IF_1X16);
	else
		nfcInfo.ifMode = MV_NFC_IF_2X8;
	nfcInfo.autoStatusRead = MV_FALSE;
	nfcInfo.tclk = pdata->tclk;
	nfcInfo.readyBypass = MV_FALSE;
	nfcInfo.osHandle = NULL;
	nfcInfo.regsPhysAddr = DOVE_SB_REGS_PHYS_BASE;
	nfcInfo.dataPdmaIntMask = MV_PDMA_END_OF_RX_INTR_EN | MV_PDMA_END_INTR_EN;
	nfcInfo.cmdPdmaIntMask = 0x0;
	if (mvNfcInit(&nfcInfo, &info->nfcCtrl) != MV_OK) {
		dev_err(&pdev->dev, "mvNfcInit() failed.\n");
		goto fail_put_clk;
	}

	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_0);
	mvNfcIntrSet(&info->nfcCtrl,  0xFFF, MV_FALSE);
	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_1);
	mvNfcIntrSet(&info->nfcCtrl,  0xFFF, MV_FALSE);
	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_NONE);

	ret = orion_nfc_init_buff(info);
	if (ret)
		goto fail_put_clk;

	/* Clear all old events on the status register */
	MV_REG_WRITE(NFC_STATUS_REG, MV_REG_READ(NFC_STATUS_REG));
	if (info->use_dma)
		ret = request_irq(IRQ_NAND, orion_nfc_irq_dma, IRQF_DISABLED,
				pdev->name, info);
	else
		ret = request_irq(IRQ_NAND, orion_nfc_irq_pio, IRQF_DISABLED,
				pdev->name, info);

	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto fail_free_buf;
	}	

	ret = orion_nfc_detect_flash(info);
	if (ret) {
		dev_err(&pdev->dev, "failed to detect flash\n");
		ret = -ENODEV;
		goto fail_free_irq;
	}

	orion_nfc_init_mtd(mtd, info);

	if (info->nand_chip.ecc.layout == NULL) {
		dev_err(&pdev->dev, "Undefined ECC layout for selected nand device\n");
		ret = -ENXIO;
		goto fail_free_irq;
	}

	platform_set_drvdata(pdev, mtd);

	if (nand_scan(mtd, pdata->num_cs)) {
		dev_err(&pdev->dev, "failed to scan nand\n");
		ret = -ENXIO;
		goto fail_free_irq;
	}

	if (mtd_has_partitions()) {
		struct mtd_partition	*parts = NULL;
		int			i, nr_parts = 0;

		if (mtd_has_cmdlinepart()) {
			static const char *part_probes[]
					= { "cmdlinepart", NULL, };
			mtd->name = "dove-nand";
			nr_parts = parse_mtd_partitions(mtd,
					part_probes, &parts, 0);
		}

		if (nr_parts <= 0 && pdata && pdata->parts) {
			parts = pdata->parts;
			nr_parts = pdata->nr_parts;
		}

		if (nr_parts > 0) {
			for (i = 0; i < nr_parts; i++) {
				DEBUG(MTD_DEBUG_LEVEL2, "partitions[%d] = "
					"{.name = %s, .offset = 0x%llx, "
						".size = 0x%llx (%lldKiB) }\n",
					i, parts[i].name,
					(long long)parts[i].offset,
					(long long)parts[i].size,
					(long long)(parts[i].size >> 10));
			}
			return add_mtd_partitions(mtd, parts, nr_parts);
		}
	} else if (pdata && pdata->nr_parts)
		dev_warn(&info->pdev->dev, "ignoring %d default partitions on %s\n",
				pdata->nr_parts, info->pdev->name);

	return add_mtd_device(mtd) == 1 ? -ENODEV : 0;

fail_free_irq:
	free_irq(IRQ_NAND, info);
fail_free_buf:
	if (pdata->use_dma) {
		dma_free_coherent(&pdev->dev, info->data_buff_size,
			info->data_buff, info->data_buff_phys);
	} else
		kfree(info->data_buff);
fail_put_clk:
	clk_disable(info->clk);
	clk_put(info->clk);
fail_free_mtd:
	kfree(mtd);
	return ret;
}
#else /* defined UBOOT_CODE */
int board_nand_init(struct nand_chip *nand)
{
	struct orion_nfc_info *info;
	int ret = 0;
	MV_NFC_INFO nfcInfo;

	info = malloc(sizeof(struct orion_nfc_info));
	if (info == NULL) {
		dev_err(&pdev->dev, "orion_nfc_info malloc failed!\n");
		return -ENOMEM;
	}

	info->tclk = CONFIG_SYS_TCLK;
#ifdef MV_NAND_GANG_MODE
	info->num_devs       = 2;
	info->nfc_width      = 16;
#else /* NON-GANG */
	info->num_devs       = 1;
	info->nfc_width      = 8;
#endif

#if defined(MV_NAND_2CS_MODE)
	info->num_cs         = 2;
#elif defined(MV_NAND_1CS_MODE)
	info->num_cs         = 1;
#else
	#error "1CS or 2CS must be defined!"
#endif

#ifdef MV_NAND_DMA_MODE
	info->use_dma	= MV_NFC_PDMA_ACCESS;
#elif defined(MV_NAND_PIO_MODE)
	info->use_dma	= MV_NFC_PIO_ACCESS;
#else
	#error "no access mode defined! (DMA/PIO)"
#endif

#if defined(MV_NAND_4BIT_MODE)
	info->ecc_type	= MV_NFC_ECC_BCH_2K;
#elif defined(MV_NAND_8BIT_MODE)
	info->ecc_type	= MV_NFC_ECC_BCH_1K;
#elif defined(MV_NAND_12BIT_MODE)
	info->ecc_type	= MV_NFC_ECC_BCH_704B;
#else
	#error ECC Mode must be defined!
#endif

	info->mmio_phys_base = MV_NFC_REGS_BASE;

	/* Initialize NFC HAL */
	nfcInfo.ioMode = (info->use_dma ? MV_NFC_PDMA_ACCESS : MV_NFC_PIO_ACCESS);
	nfcInfo.eccMode = info->ecc_type;
		
	if(info->num_devs == 1)
		nfcInfo.ifMode = ((info->nfc_width == 8) ? MV_NFC_IF_1X8 : MV_NFC_IF_1X16);
	else
		nfcInfo.ifMode = MV_NFC_IF_2X8;
	nfcInfo.autoStatusRead = MV_FALSE;
	nfcInfo.tclk = info->tclk;
	nfcInfo.readyBypass = MV_FALSE;
	nfcInfo.osHandle = NULL;
	nfcInfo.regsPhysAddr = DOVE_SB_REGS_PHYS_BASE;
	nfcInfo.dataPdmaIntMask = MV_PDMA_END_OF_RX_INTR_EN | MV_PDMA_END_INTR_EN;
	nfcInfo.cmdPdmaIntMask = 0x0;
	if (mvNfcInit(&nfcInfo, &info->nfcCtrl) != MV_OK) {
		dev_err(&pdev->dev, "mvNfcInit() failed.\n");
		goto fail_free_orion;
	}

	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_0);
	mvNfcIntrSet(&info->nfcCtrl,  0xFFF, MV_FALSE);
	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_1);
	mvNfcIntrSet(&info->nfcCtrl,  0xFFF, MV_FALSE);
	mvNfcSelectChip(&info->nfcCtrl, MV_NFC_CS_NONE);

	ret = orion_nfc_init_buff(info);
	if (ret)
		goto fail_free_buf;

	/* Clear all old events on the status register */
	MV_REG_WRITE(NFC_STATUS_REG, MV_REG_READ(NFC_STATUS_REG));

	ret = orion_nfc_detect_flash(info);
	if (ret) {
		dev_err(&pdev->dev, "failed to detect flash\n");
		ret = -ENODEV;
		goto fail_free_buf;
	}

	orion_nfc_init_nand(nand, info);

	nand->priv = info;
	return 0;

fail_free_buf:
	free(info->data_buff);
fail_free_orion:
	free(info);
	return ret;
}
#endif /* UBOOT_CODE */
#ifndef UBOOT_CODE
static int orion_nfc_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct orion_nfc_info *info = mtd->priv;

	platform_set_drvdata(pdev, NULL);

	del_mtd_device(mtd);
	del_mtd_partitions(mtd);
	free_irq(IRQ_NAND, info);
	if (info->use_dma) {
		dma_free_writecombine(&pdev->dev, info->data_buff_size,
				info->data_buff, info->data_buff_phys);
	} else
		kfree(info->data_buff);

	clk_disable(info->clk);
	clk_put(info->clk);
	kfree(mtd);
	return 0;
}
#endif /* UBOOT_CODE */
#ifdef CONFIG_PM
static int orion_nfc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mtd_info *mtd = (struct mtd_info *)platform_get_drvdata(pdev);
	struct orion_nfc_info *info = mtd->priv;

	if (info->state != STATE_READY) {
		dev_err(&pdev->dev, "driver busy, state = %d\n", info->state);
		return -EAGAIN;
	}

	/* Store PDMA registers.	*/
	info->pdmaDataLen = 128;
	mvPdmaUnitStateStore(info->pdmaUnitData, &info->pdmaDataLen);

	/* Store NFC registers.	*/
	info->nfcDataLen = 128;
	mvNfcUnitStateStore(info->nfcUnitData, &info->nfcDataLen);

	clk_disable(info->clk);
	return 0;
}

static int orion_nfc_resume(struct platform_device *pdev)
{
	struct mtd_info *mtd = (struct mtd_info *)platform_get_drvdata(pdev);
	struct orion_nfc_info *info = mtd->priv;
	MV_U32	i;

	clk_enable(info->clk);

	/* restore PDMA registers */
	for(i = 0; i < info->pdmaDataLen; i+=2)
		MV_REG_WRITE(info->pdmaUnitData[i], info->pdmaUnitData[i+1]);

	/* Clear all NAND interrupts */
	MV_REG_WRITE(NFC_STATUS_REG, MV_REG_READ(NFC_STATUS_REG));

	/* restore NAND registers */
	for(i = 0; i < info->nfcDataLen; i+=2)
		MV_REG_WRITE(info->nfcUnitData[i], info->nfcUnitData[i+1]);

	return 0;
}
#else
#define orion_nfc_suspend	NULL
#define orion_nfc_resume	NULL
#endif

#ifndef UBOOT_CODE
static struct platform_driver orion_nfc_driver = {
	.driver = {
		.name	= "orion-nfc-hal",
		.owner	= THIS_MODULE,
	},
	.probe		= orion_nfc_probe,
	.remove		= orion_nfc_remove,
	.suspend	= orion_nfc_suspend,
	.resume		= orion_nfc_resume,
};

static int __init orion_nfc_init(void)
{
	return platform_driver_register(&orion_nfc_driver);
}
module_init(orion_nfc_init);

static void __exit orion_nfc_exit(void)
{
	platform_driver_unregister(&orion_nfc_driver);
}

module_exit(orion_nfc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dove NAND controller driver");
#endif /* UBOOT_CODE */

#ifdef UBOOT_CODE
#endif /* defined(CONFIG_CMD_NAND)*/
#endif /* UBOOT_CODE */
#endif /* defined(MV_NAND) */