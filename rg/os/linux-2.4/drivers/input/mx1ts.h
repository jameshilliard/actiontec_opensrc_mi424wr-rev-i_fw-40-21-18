/*
 *  linux/drivers/misc/mx1ts.h
 *
 *  Copyright (C) 2003 Blue Mug, Inc. for Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/* Interrupt numbers */
#define ASP_COMPARE_IRQ         5
#define ASP_PENDATA_IRQ         33
#define ASP_TOUCH_IRQ           46

/* Analog signal processor (ASP) control registers */
#define ASP_ACNTLCR		0xF0215010      /* Control register */
#define ASP_PSMPLRG		0xF0215014      /* Pen A/D sampe rate control */
#define ASP_CMPCNTL		0xF0215030      /* Compare control register */
#define ASP_ICNTLR		0xF0215018      /* Interrupt control register */
#define ASP_ISTATR		0xF021501C      /* Interrupt status register */
#define ASP_PADFIFO		0xF0215000      /* Pen sample FIFO */
#define ASP_CLKDIV		0xF021502C      /* Clock divide register */

/* ASP control register bits */
#define ASP_CLKEN               (1 << 25)       /* Clock enable */
#define ASP_SWRST               (1 << 23)       /* Software reset */
#define ASP_U_SEL               (1 << 21)       /* U-channel resistor select */
#define ASP_AZ_SEL              (1 << 20)       /* Auto-zero position select */
#define ASP_LVM                 (1 << 19)       /* Low voltage output */
#define ASP_NM                  (1 << 18)       /* Normal voltage output */
#define ASP_HPM                 (1 << 17)       /* High voltage output */
#define ASP_GLO                 (1 << 16)       /* Low gain enable */
#define ASP_AZE                 (1 << 15)       /* Auto-zero enable */
#define ASP_AUTO                (1 << 14)       /* Auto sampling */
#define ASP_SW8                 (1 << 11)       /* Switch control 8 */
#define ASP_SW7                 (1 << 10)
#define ASP_SW6                 (1 << 9)
#define ASP_SW5                 (1 << 8)
#define ASP_SW4                 (1 << 7)
#define ASP_SW3                 (1 << 6)
#define ASP_SW2                 (1 << 5)
#define ASP_SW1                 (1 << 4)        /* Switch control 1 */
#define ASP_VDAE                (1 << 3)        /* Voice D/A enable */
#define ASP_VADE                (1 << 2)        /* Voice A/D enable */
#define ASP_PADE                (1 << 1)        /* Pen A/D enable */
#define ASP_BGE                 (1 << 0)        /* Bandgap enable */

#define ASP_MODE_MASK           0x00003000
#define ASP_MODE_NONE           0x00000000
#define ASP_MODE_ONLY_X         0x00001000
#define ASP_MODE_ONLY_Y         0x00002000
#define ASP_MODE_ONLY_U         0x00003000

/* ASP Pen A/D sample rate control register */
#define ASP_DMCNT_MASK          (0x00007000)    /* Decimation ratio count */
#define ASP_DMCNT_SCALE         (12)
#define ASP_BIT_SELECT_MASK     (0x00000C00)    /* Bit select */
#define ASP_BIT_SELECT_SCALE    (10)
#define ASP_IDLECNT_MASK        (0x000003F0)    /* Idle count */
#define ASP_IDLECNT_SCALE       (4)
#define ASP_DSCNT_MASK          (0x0000000F)    /* Data setup count */
#define ASP_DSCNT_SCALE         (0)

/* ASP compare control register */
#define ASP_INT                 (1 << 19)       /* Interrupt status */
#define ASP_CC                  (1 << 18)       /* Trigger on greater than */
#define ASP_INSEL_MASK          (0x00030000)
#define ASP_INSEL_DISABLE       (0x00000000)
#define ASP_INSEL_X             (0x00010000)
#define ASP_INSEL_Y             (0x00020000)
#define ASP_INSEL_U             (0x00030000)
#define ASP_COMPARE_VAL_MASK    (0x0000FFFF)
#define ASP_COMPARE_VAL_SCALE   (0)

/* ASP interrupt control register bits */
#define ASP_PUPE                (1 << 10)       /* Pen up XXX undocumented */
#define ASP_VDDMAE              (1 << 8)        /* VDAC FIFO empty DMA */
#define ASP_VADMAE              (1 << 7)        /* VADC FIFO full DMA */
#define ASP_POL                 (1 << 6)        /* Pen interrupt polarity */
#define ASP_EDGE                (1 << 5)        /* Edge trigger enable */
#define ASP_PIRQE               (1 << 4)        /* Pen interrupt enable */
#define ASP_VDAFEE              (1 << 3)        /* VDAC FIFO empty interrupt */
#define ASP_VADFFE              (1 << 2)        /* VADC FIFO full interrupt */
#define ASP_PFFE                (1 << 1)        /* Pen FIFO full interrupt */
#define ASP_PDRE                (1 << 0)        /* Pen data ready interrupt */

/* ASP interrupt/error status register bits */
#define ASP_PUP                 (1 << 10)       /* Pen up XXX undocumented */
#define ASP_BGR                 (1 << 9)        /* Bandgap ready */
#define ASP_VOV                 (1 << 8)        /* Voice sample data overflow */
#define ASP_POV                 (1 << 7)        /* Pen sample data overflow */
#define ASP_PEN                 (1 << 6)        /* Pen interrupt */
#define ASP_VDAFF               (1 << 5)        /* VDAC FIFO full */
#define ASP_VDAFE               (1 << 4)        /* VDAC FIFO empty */
#define ASP_VADFF               (1 << 3)        /* VADC FIFO full */
#define ASP_VADDR               (1 << 2)        /* VADC data ready */
#define ASP_PFF                 (1 << 1)        /* Pen sample FIFO full */
#define ASP_PDR                 (1 << 0)        /* Pen data ready */

/* ASP Clock divide register */
#define ASP_PADC_CLK_MASK       (0x0000001F)
#define ASP_PADC_CLK_SCALE      (0)
#define ASP_VADC_CLK_MASK       (0x000003E0)
#define ASP_VADC_CLK_SCALE      (5)
#define ASP_VDAC_CLK_MASK       (0x00003C00)
#define ASP_VDAC_CLK_SCALE      (10)
