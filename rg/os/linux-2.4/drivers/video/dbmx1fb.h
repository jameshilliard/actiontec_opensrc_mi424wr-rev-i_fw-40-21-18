/****************************************************************************
 *
 * Copyright (C) 2001, Chen Ning All Rights Reserved
 *
 * File Name:	dbmx1fb.h
 *
 * Progammers:	Chen Ning, Zhang Juan
 *
 * Date of Creations:	10 DEC,2001
 *
 * Synopsis:
 *
 * Descirption:
 *
 * Modification History:
 * 10 DEC, 2001, initialization version, frame work for frame buffer driver
 *
*******************************************************************************/

#ifndef LCDFB_H
#define LCDFB_H
#include "asm/arch/hardware.h"
#include "asm/arch/platform.h"

typedef signed char          	BOOLEAN;
typedef unsigned char        	UINT8;              		/* Unsigned  8 bit quantity 	*/
typedef signed char          	SINT8;              		/* Signed    8 bit quantity 		*/
typedef unsigned short       	UINT16;             		/* Unsigned 16 bit quantity 	*/
typedef signed short         	SINT16;            		/* Signed   16 bit quantity 		*/
typedef unsigned long        	UINT32;             		/* Unsigned 32 bit quantity 	*/
typedef signed long          	SINT32;             		/* Signed   32 bit quantity 		*/

typedef volatile BOOLEAN     	VBOOLEAN;
typedef volatile UINT8       	VUINT8;             		/* Unsigned  8 bit quantity 	*/
typedef volatile SINT8       	VSINT8;             		/* Signed    8 bit quantity 		*/
typedef volatile UINT16      	VUINT16;            	/* Unsigned 16 bit quantity 	*/
typedef volatile SINT16      	VSINT16;            	/* Signed   16 bit quantity 		*/
typedef volatile UINT32      	VUINT32;            	/* Unsigned 32 bit quantity 	*/
typedef volatile SINT32      	VSINT32;            	/* Signed   32 bit quantity 		*/

#define LCDBASE		0x00205000
#define LCD_REGIONSIZE	0xc00

#define DBMX1_LCD_SSA		0x00205000
#define DBMX1_LCD_XYMAX		0x00205004
#define DBMX1_LCD_VPW		0x00205008
#define DBMX1_LCD_LCXYP		0x0020500c
#define DBMX1_LCD_CURBLKCR	0x00205010
#define DBMX1_LCD_LCHCC		0x00205014
#define DBMX1_LCD_PANELCFG	0x00205018
#define DBMX1_LCD_HCFG		0x0020501c
#define DBMX1_LCD_VCFG		0x00205020
#define DBMX1_LCD_POS		0x00205024
#define DBMX1_LCD_LGPMR		0x00205028
#define DBMX1_LCD_PWMR		0x0020502c
#define DBMX1_LCD_DMACR		0x00205030
#define DBMX1_LCD_REFMCR	0x00205034
#define DBMX1_LCD_INTCR		0x00205038
#define DBMX1_LCD_INTSR		0x00205040
#define DBMX1_LCD_MAPRAM	0x00205800

#define MPCTL0			0x0021B004
#define PCDR			0X0021B020

#define SSA		DBMX1_LCD_SSA
#define XYMAX		DBMX1_LCD_XYMAX
#define VPW		DBMX1_LCD_VPW
#define LCXYP		DBMX1_LCD_LCXYP
#define CURBLKCR	DBMX1_LCD_CURBLKCR
#define LCHCC		DBMX1_LCD_LCHCC
#define PANELCFG	DBMX1_LCD_PANELCFG
#define VCFG		DBMX1_LCD_VCFG
#define HCFG		DBMX1_LCD_HCFG
#define POS		DBMX1_LCD_POS
#define LGPMR		DBMX1_LCD_LGPMR
#define PWMR		DBMX1_LCD_PWMR
#define DMACR		DBMX1_LCD_DMACR
#define REFMCR		DBMX1_LCD_REFMCR
#define INTCR		DBMX1_LCD_INTCR
#define INTSR		DBMX1_LCD_INTSR
#define MAPRAM		DBMX1_LCD_MAPRAM
// default value
#define SHARP_TFT_240x320

#ifdef SHARP_TFT_240x320
#define PANELCFG_VAL_12	0xf8008b48
#define HCFG_VAL_12	0x04000f06
#define VCFG_VAL_12	0x04000907
#define PWMR_VAL	0x0000008a
#define LCD_MAXX	240
#define LCD_MAXY	320
#else //SHARP_TFT_240x320

#define PANELCFG_VAL_12	0xf8088c6b
#define HCFG_VAL_12	0x04000f06
#define VCFG_VAL_12	0x04010c03
#define PWMR_VAL	0x00000200
#define LCD_MAXX	320
#define LCD_MAXY	240
#endif // SHARP_TFT_240x320

#define PANELCFG_VAL_4	0x20008c09
#define PANELCFG_VAL_4C	0x60008c09

#define HCFG_VAL_4	0x04000f07

#define VCFG_VAL_4	0x04010c03


#define REFMCR_VAL_4	0x00000003
#define REFMCR_VAL_12	0x00000003
#define DISABLELCD_VAL	0x00000000

#define DMACR_VAL_4	0x800c0003	// 12 & 3 TRIGGER
#define DMACR_VAL_12	0x00020008

#define INTCR_VAL_4	0x00000000
#define INTCR_VAL_12	0x00000000

#define INTSR_UDRERR	0x00000008
#define INTSR_ERRRESP	0x00000004
#define INTSR_EOF	0x00000002
#define INTSR_BOF	0x00000001

#define MIN_XRES        64
#define MIN_YRES        64

#define LCD_MAX_BPP	16

#if 0
#define READREG(r)	\
	inl(IO_ADDRESS(r))

#define WRITEREG(r, val) \
	outl(val, IO_ADDRESS(r))
#else
#endif // 0

#define MAX_PALETTE_NUM_ENTRIES         256
#define MAX_PIXEL_MEM_SIZE \
        ((current_par.max_xres * current_par.max_yres * current_par.max_bpp)/8)
#define MAX_FRAMEBUFFER_MEM_SIZE \
	        (MAX_PIXEL_MEM_SIZE + 32)
#define ALLOCATED_FB_MEM_SIZE \
	        (PAGE_ALIGN(MAX_FRAMEBUFFER_MEM_SIZE + PAGE_SIZE * 2))

	// TODO:
#define FBCON_HAS_CFB4
#define FBCON_HAS_CFB8
#define FBCON_HAS_CFB16

#define IRQ_LCD			12		// TODO: which irq?
#define DBMX1_NAME 		"DBMX1FB"
#define DEV_NAME		DBMX1_NAME
#define MAX_PALETTE_NUM_ENTRIES 256
#define DEFAULT_CURSOR_BLINK_RATE (20)
#define CURSOR_DRAW_DELAY  (2)
/*cursor status*/
#define LCD_CURSOR_OFF	            0
#define LCD_CURSOR_ON               1

#ifdef FBCON_HAS_CFB4
	#define LCD_CURSOR_REVERSED     2
   	#define LCD_CURSOR_ON_WHITE     3
#elif defined(FBCON_HAS_CFB8) || defined(FBCON_HAS_CFB16)
   	#define LCD_CURSOR_INVERT_BGD	2
   	#define LCD_CURSOR_AND_BGD	3
	#define LCD_CURSOR_OR_BGD       4
	#define LCD_CURSOR_XOR_BGD      5
#endif //FBCON_HAS_CFB4

/* MASK use for caculating MCDUPLLCLK */
#define MFI_MASK		0x00003C00
#define MFN_MASK		0x000003FF
#define PD_MASK			0x3C000000
#define MFD_MASK		0x03FF0000
#define PCLKDIV2_MASK		0x000000F0
#define PCD_MASK		0x0000003F
#define XMAX_MASK                   0xF3F00000
#define YMAX_MASK                   0x000001FF
#define HWAIT1_MASK                 0x0000FF00
#define HWAIT2_MASK                 0x000000FF
#define HWIDTH_MASK                 0xFC000000
#define PASSDIV_MASK		0x00FF0000
#define VWAIT1_MASK                 0x0000FF00
#define VWAIT2_MASK                 0x000000FF
#define VWIDTH_MASK                 0xFC000000
#define CURSORBLINK_DIS_MASK	0x80000000

#define DISPLAY_MODE_MASK		0x80000000

#define MCU_FREQUENCE               32768
#define COLOR_MASK	0x40000000

/*  MASK use for indicating the cursor status  */
#define CURSOR_ON_MASK              0x40000000
#define CURSOR_OFF_MASK		   0x0FFFFFFF
#define MAX_CURSOR_WIDTH            31
#define MAX_CURSOR_HEIGHT           31
#define CURSORBLINK_EN_MASK		0x80000000

#ifdef FBCON_HAS_CFB4
#define CURSOR_REVERSED_MASK        0x80000000
#define CURSOR_WHITE_MASK           0xC0000000
#else
#define CURSOR_INVERT_MASK		0x80000000
#define CURSOR_AND_BGD_MASK         0xC0000000
#define CURSOR_OR_BGD_MASK          0x50000000
#define CURSOR_XOR_BGD_MASK         0x90000000
#endif // FBCON_HAS_CFB4

#endif //LCDFB_H

