#define PCISR	0x0000		/* PC Card Interrupt Status Register		*/
#define PCIMR	0x0400		/* PC Card Interrupt Mask Register		*/
#define PCICR	0x0800		/* PC Card Interrupt Clear Register		*/
#define PCIOSR	0x0c00		/* PC Card Interrupt Output Select Regsiter	*/
#define PCIRR1	0x1000		/* PC Card Interrupt Reserved Register 1	*/
#define PCIRR2	0x1400		/* PC Card Interrupt Reserved Register 2	*/
#define PCIRR3	0x1800		/* PC Card Interrupt Reserved Register 3	*/
#define PCIILR	0x1c00		/* PC Card Interrupt Input Level Register	*/
#define SICR	0x2000		/* System Interface Configuration Register	*/
#define CICR	0x2400		/* Card Interface Configuration Register	*/
#define PMR	0x2800		/* Power Management Register			*/
#define CPCR	0x2c00		/* Card Power Control Register			*/
#define CITR0A	0x3000		/* Card Interface Timing Register 0A		*/
#define CITR0B	0x3400		/* Card Interface Timing Register 0B		*/
#define CITR1A	0x3800		/* Card Interface Timing Register 1A		*/
#define CITR1B	0x3c00		/* Card Interface Timing Register 1B		*/
#define DMACR	0x4000		/* DMA Control Register				*/
#define DIR	0x4400		/* Device Information Register			*/

#define CLPS6700_ATTRIB_BASE	0x00000000
#define CLPS6700_IO_BASE	0x04000000
#define CLPS6700_MEM_BASE	0x08000000
#define CLPS6700_REG_BASE	0x0c000000
#define CLPS6700_REG_SIZE	0x00005000


#define PMR_AUTOIDLE	(1 << 0)	/* auto idle mode			*/
#define PMR_FORCEIDLE	(1 << 1)	/* force idle mode			*/
#define PMR_PDCS	(1 << 2)	/* Power down card on standby		*/
#define PMR_PDCR	(1 << 3)	/* Power down card on removal		*/
#define PMR_DCAR	(1 << 4)	/* Disable card access on removal	*/
#define PMR_CPE		(1 << 5)	/* Card power enable			*/
#define PMR_MCPE	(1 << 6)	/* Monitor card power enable		*/
#define PMR_PDREQLSEL	(1 << 7)	/* If set, PDREQL is a GPIO pin		*/
#define PMR_DISSTBY	(1 << 8)	/* Disable standby			*/
#define PMR_ACCSTBY	(1 << 9)	/* Complete card accesses before standby*/
#define PMR_CDUNPROT	(0 << 10)	/* Card detect inputs unprotected	*/
#define PMR_CDPROT	(1 << 10)	/* Card detect inputs protected		*/
#define PMR_CDWEAK	(2 << 10)	/* Weak pullup except in standby	*/
#define PMR_CDWEAKAL	(3 << 10)	/* Weak pullup				*/

#define CPCR_PON(x)	((x)&7)		/* PCTL[2:0] value when PMR_CPE = 1	*/
#define CPCR_POFF(x)	(((x)&7)<<3)	/* PCTL[2:0] value when PMR_CPE = 0	*/
#define CPCR_PDIR(x)	(((x)&7)<<6)	/* PCTL[2:0] direction			*/
#define CPCR_CON(x)	(((x)&1)<<9)	/* GPIO value when PMR_CPE = 1		*/
#define CPCR_COFF(x)	(((x)&1)<<10)	/* GPIO value when PMR_CPE = 0		*/
#define CPCR_CDIR(x)	(((x)&1)<<11)	/* GPIO direction (PMR_PDREQLSEL = 1)	*/
#define CPCR_VS(x)	(((x)&3)<<12)	/* VS[2:1] output value			*/
#define CPCR_VSDIR(x)	(((x)&3)<<14)	/* VS[2:1] direction			*/

#define PCTL0		(1 << 0)
#define PCTL1		(1 << 1)
#define PCTL2		(1 << 2)

#define CICR_ASRTMR1	(1 << 0)	/* Timer 1 select for attribute read	*/
#define CICR_ASWTMR1	(1 << 1)	/* Timer 1 select for attribute write	*/
#define CICR_IOSRTMR1	(1 << 2)	/* Timer 1 select for IO read		*/
#define CICR_IOSWTMR1	(1 << 3)	/* Timer 1 select for IO write		*/
#define CICR_MEMSRTMR1	(1 << 4)	/* Timer 1 select for memory read	*/
#define CICR_MEMSWTMR1	(1 << 5)	/* Timer 1 select for memory write	*/
#define CICR_AUTOIOSZ	(1 << 6)	/* Auto size I/O accesses		*/
#define CICR_CAW	(1 << 7)	/* Card access width			*/
#define CICR_IOMODE	(1 << 8)	/* IO mode select			*/
#define CICR_ENABLE	(1 << 10)	/* Card enable				*/
#define CICR_RESETOE	(1 << 11)	/* Card reset output enable		*/
#define CICR_RESET	(1 << 12)	/* Card reset				*/


#define RD_FAIL		(1 << 14)
#define WR_FAIL		(1 << 13)
#define IDLE		(1 << 12)

#define FFOTHLD		(1 << 11)
#define PCM_RDYL	(1 << 10)
#define PCM_WP		(1 << 9)
#define PCTL		(1 << 8)

#define PDREQ_L		(1 << 6)
#define PCM_VS2		(1 << 5)
#define PCM_VS1		(1 << 4)

#define PCM_CD2		(1 << 3)
#define PCM_CD1		(1 << 2)
#define PCM_BVD2	(1 << 1)
#define PCM_BVD1	(1 << 0)
