/* Copyright (c) 2005 freescale semiconductor
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _MPC8349_USB_EHCI_H
#define _MPC8349_USB_EHCI_H

/**************************************************************************/
/*@Description   t_USB_MPH_MAP - USB Multi-Port-Host internal memory map.*/
/***************************************************************************/
typedef   struct{
	volatile u32    id;			/* Identification register */
	volatile u32    hwgeneral;
	volatile u32    hwhost;
	volatile u8     RESERVED01[0x004];	/* Reserved area           */
	volatile u32    hwtxbuf;
	volatile u32    hwrxbuf;
	volatile u8     RESERVED02[0x0e8];	/* Reserved area           */
    
	/* Capability Registers */
	volatile u32    hc_capbase;
	volatile u32    hcs_params;		/* HCSPARAMS - offset 0x4 */
	volatile u32    hcc_params;		/* HCCPARAMS - offset 0x8 */
	volatile u8     portroute [8];		/* nibbles for routing - offset 0xC */
	volatile u8     RESERVED03[0x02c];	/* Reserved area           */

	/* Operational Registers */
	volatile u32    command;
	volatile u32    status;
	volatile u32    intr_enable;
	volatile u32    frame_index;		/* current microframe number */
	volatile u32    segment;		/* address bits 63:32 if needed */
	volatile u32    frame_list;		/* points to periodic list */
	volatile u32    async_next;		/* address of next async queue head */
	volatile u32    async_tt_status;	/* async queue status for embedded TT */
	volatile u32    burst_size;		/* programmable burst size */
	volatile u32    txfilltuning;		/* host transmit pre-buffer packet tuning */
	volatile u32    txttfilltuning;		/* host TT transmit pre-buffer packet tuning */
	volatile u8     RESERVED04[4];
	volatile u32    ulpi_view_port;		/* ULPI view port */
	volatile u32    endpoint_nack;		/* endpoint nack */
	volatile u32    endpoint_nack_en;	/* endpoint nack enable */
	volatile u8     RESERVED05[4];
	volatile u32    configured_flag;
	volatile u32    port_status [2];	/* up to N_PORTS */
   
	volatile u8     RESERVED06[0x01c];	/* Reserved area           */
	volatile u32    usbmode;
	volatile u8     RESERVED07[0x254];	/* Reserved area           */
	volatile u32    snoop1;
	volatile u32    snoop2;
	volatile u32    age_cnt_thresh;
	volatile u32    si_ctrl;
	volatile u32    pri_ctrl;
	volatile u8     RESERVED08[0x0ec];	/* Reserved area           */
	volatile u32    control;
	volatile u8     RESERVED09[0xaf8];	/* Reserved area           */
} t_USB_MPH_MAP;

/**************************************************************************/
/** @Description   t_USB_DR_MAP - USB Dual-Role internal memory map.      */
/***************************************************************************/
typedef   struct{
	volatile u32    id;			/* Identification register */
	volatile u32    hwgeneral;
	volatile u32    hwhost;
	volatile u32    hwdevice;
	volatile u32    hwtxbuf;
	volatile u32    hwrxbuf;
	volatile u8     RESERVED01[0x0e8];	/* Reserved area           */
   
	/* Capability Registers */
	volatile u32    hc_capbase;
	volatile u32    hcs_params;		/* HCSPARAMS - offset 0x4 */
	volatile u32    hcc_params;		/* HCCPARAMS - offset 0x8 */
	volatile u8     portroute [8];		/* nibbles for routing - offset 0xC */
	volatile u8     RESERVED02[0x00c];	/* Reserved area           */
	volatile u32    dciversion;
	volatile u32    dccparms;
	volatile u8     RESERVED03[0x018];	/* Reserved area           */
	   
	/* Operational Registers */
	volatile u32    command;
	volatile u32    status;
	volatile u32    intr_enable;
	volatile u32    frame_index;		/* current microframe number */
	union    t_host_slave_regs {
		struct         t_ehci_regs 
		{
			volatile u32    segment;	/* address bits 63:32 if needed */
			volatile u32    frame_list;	/* points to periodic list */
			volatile u32    async_next;	/* address of next async queue head */
		} host_regs;
		struct         t_slave_regs {
			volatile u8     RESERVED04 [0x04];
			volatile u32    deviceaddr;
			volatile u32    endpoint_list_addr;	/* points to periodic list */
		} slave_regs;
	} host_slave_regs;
	volatile u32    async_tt_status;	/* async queue status for embedded TT */
	volatile u32    burst_size;		/* programmable burst size */
	volatile u32    txfilltuning;		/* host transmit pre-buffer packet tuning */
	volatile u32    txttfilltuning;		/* host TT transmit pre-buffer packet tuning */
	volatile u8     RESERVED04[4];
	volatile u32    ulpi_view_port;		/* ULPI view port */
	volatile u32    endpoint_nack;		/* endpoint nack */
	volatile u32    endpoint_nack_en;	/* endpoint nack enable */
	volatile u8     RESERVED05[4];
	volatile u32    configured_flag;
	volatile u32    port_status [1];	/* up to N_PORTS */
	   
	volatile u8     RESERVED06[0x01c];	/* Reserved area           */
	volatile u32    otgsc;
	volatile u32    usbmode;
	volatile u32    endptsetupstat;
	volatile u32    endptprime;
	volatile u32    endptflush;
	volatile u32    endptstatus;
	volatile u32    endptcomplete;
	volatile u32    endptctrl[6];
	volatile u8     RESERVED07[0x228];	/* Reserved area           */
	volatile u32    snoop1;
	volatile u32    snoop2;
	volatile u32    age_cnt_thresh;
	volatile u32    si_ctrl;
	volatile u32    pri_ctrl;
	volatile u8     RESERVED08[0x0ec];	/* Reserved area           */
	volatile u32    control;
	volatile u8     RESERVED09[0xaf8];	/* Reserved area           */
} __attribute__ ((packed)) t_USB_DR_MAP, * t_pUSB_DR_MAP;



#define PORT_OFF        0
#define PORT_ULPI       1
#define PORT_UTMI       2
#define PORT_SERIAL     3
#define PORT_SERIAL_OTG 4


#define PORT_TS          0xc0000000
#define PORT_TS_UTMI     0x00000000
#define PORT_TS_ULPI     0x80000000
#define PORT_TS_SERIAL   0xc0000000
#define PORT_TW          0x10000000
#define PORT_SPD         0x0c000000
#define PORT_FSC         0x01000000
#define PORT_PP          0x00001000



#define CFG_IMMR_BASE	        (0xfe000000)
#define MPC83xx_USB_MPH_BASE    (CFG_IMMR_BASE + 0x22000)
#define MPC83xx_USB_DR_BASE     (CFG_IMMR_BASE + 0x23000)
#define MPC83xx_USB_DR_IVEC	(38)
#define MPC83xx_USB_MPH_IVEC	(39)
#define CFG_BCSR_BASE 		(0xfe100000)
#define BCSR5_INT_USB		(0x02)

#define e_USB_MPH 0
#define e_USB_DR 1
#define e_ULPI          0
#define e_UTMI_8BIT     1 
#define e_UTMI_16BIT    2
#define e_SERIAL        3

#define SCCR_OFFS          0xA08
#define SCCR_USB_MPHCM_11  0x00c00000
#define SCCR_USB_MPHCM_01  0x00400000
#define SCCR_USB_MPHCM_10  0x00800000
#define SCCR_USB_DRCM_11   0x00300000
#define SCCR_USB_DRCM_01   0x00100000
#define SCCR_USB_DRCM_10   0x00200000

#define SICRL_OFFS         0x114
#define SICRL_USB1         0x40000000
#define SICRL_USB0         0x20000000

#define SICRH_OFFS         0x118
#define SICRH_USB_UTMI     0x00020000

#define SPCR_OFFS          0x00000110
#define SPCR_TBEN          0x00400000

#if 0

/* Very good memory allocation algorithm, but needless */

#define POWER_OF_2(n)           (!(n & (n-1)))

/*------------------------------------------------------*/

typedef struct
{
	char		Name[4];	/* this segment's name          */
	spinlock_t	lock;
	u16		Num;	/* number of blocks in segment  */
	int         Size;	/* size of blocks in segment    */
				/* in case of TMP_DEF -         */
				/* only the data                */
	u32    	GetFailures;	/* number of times get failed   */
	int	LocallyAllocated;	/* TRUE if memory was allocated */
	/* at MEM_Init.	  */
	u8   	*p_Base;	/* base address of segment      */
	void  	**p_First;	/* first block in segment       */
	void  	**p_Last;	/* last block in segment        */
	int 	(*f_MemPut)(void* Handle, void *p_Block );
				/* a routine for returning a memory block */

	u16    	PrefixSize;	/* replaces B_OFFSET - how many     */
				/* bytes to reserve before the data     */
	u16    	PostfixSize ;	/* replaces B_TRAILER - how many   */
				/* bytes to reserve after the data     */
				/* Trailer also includes a pad needed for */
				/* padding the entire block to 4 byte     */
				/* alignment for faster access to the     */
				/* control field                          */
	u16    	Alignment;	/* requested alignment for the data field */
	u16	AlignPad;	/* pad the offset field so that the data  */
				/* field shall have the proper alignment  */
	u16    	EndPad;		/* Pad to make entire block size a */
				/* multiple of Alignment	*/
} t_MemorySegment;

#define PAD_ALIGNMENT( align, x ) ( ((x)%(align)) ? ((align)-((x)%(align))) : 0 )


void *MEM_Get( void* Handle )
{
	unsigned long    	flags;	
	u8            	*p_F;
	t_MemorySegment 	*p ;
	
	p = (t_MemorySegment *)Handle;
  
	spin_lock_irqsave(&p->lock,flags);

	/* check if the chain is not empty */
	if( !(*(p->p_First)) )
	{
		p->GetFailures++;
		spin_unlock_irqrestore(p->lock,flags);
		return 0;
	}
	/* advance first pointer and return the old head of chain */
	p_F = ((u8 *)p->p_First) + ( 4 + p->AlignPad + p->PrefixSize );   
	p->p_First = (void **) *(p->p_First); /* skip the next pointer */

	spin_unlock_irqrestore(&p->lock,flags);

	return (void *)p_F;
}


int MEM_Put_Default( void * Handle, void *p_Block )
{
	unsigned long	flags;	
	t_MemorySegment 	*p = (t_MemorySegment *)Handle;
	u8            	*p_B = (u8 *)p_Block;
   
	/* if handle is NULL, use user's free routine */
	if( Handle == 0 )
	{
		kfree( p_Block );
		return 0;
	}

	spin_lock_irqsave(&p->lock,flags);

	/* get the pointer to the start of the memory */
	p_B -= ( 4 + p->AlignPad + p->PrefixSize );    /* skip back over next pointer */ 
	/* chain to end and advance last pointer */
	*((void **)p_B) = 0;
	*(p->p_Last) = (void *)p_B;
	p->p_Last = (void **)p_B;

	spin_unlock_irqrestore(&p->lock,flags);
	return 0;
}

int MEM_Init( char Name[], 
	void* *p_Handle, 
	u16 Num, 
	u16 Size , 
	u16 PrefixSize, 
	u16 PostfixSize, 
	u16 Alignment )
{
	t_MemorySegment 	*p;
	u8            	*p_Blocks;
	int              	i ;
	int		     	blockSize;

	/* always allocate a dummy block at the end */
	Num++;

	/* make sure size is always a multiple of 4 */
	if( Size & 3 )
	{
		Size &= ~3;
		Size += 4;
	}

	if (Alignment < 4 ) 
		Alignment = 4;

	/** make sure that the alignment is a power of two */
	if( !POWER_OF_2(Alignment) ) 
	{
		printk("MEM_Init: requested alignment is not a power of two.\n");
		return -EINVAL;
	}                          

	/* prepare in case of error */
	*p_Handle = 0;

	/* first allocate the segment descriptor */
	p = (t_MemorySegment *)kmalloc( sizeof(t_MemorySegment),GFP_KERNEL );

	if( !p )
		return -ENOMEM;
	/* calculate blockSize */
  
	/* store info about this segment */
	spin_lock_init (&p->lock);
	p->Num = (u16)(Num - 1);  
	p->Size = Size;
	p->GetFailures = 0L;
	p->f_MemPut = MEM_Put_Default;
	p->LocallyAllocated = 1;
	p->PrefixSize = PrefixSize;
	p->Alignment = Alignment; 
	p->AlignPad  = (u16)PAD_ALIGNMENT((u16)4, (u16)PrefixSize+4); 
	p->PostfixSize = PostfixSize;
	/* Make sure the entire size is a multiple of Alignment */
	p->EndPad = (u16)PAD_ALIGNMENT((u16)Alignment, 4 + p->AlignPad + PrefixSize + Size + PostfixSize); 

	blockSize = 4 + p->AlignPad + PrefixSize + Size + PostfixSize + p->EndPad;

	p_Blocks = (u8 *)kmalloc(( Alignment +  Num * blockSize ),GFP_KERNEL);

	if( !p_Blocks )
	{
		kfree( p );
		return -ENOMEM;
	}
	/* store the memory segment address */
	p->p_Base = p_Blocks;
	
	p_Blocks += (PrefixSize+4);
	p_Blocks += (PAD_ALIGNMENT( Alignment, (u32)p_Blocks)); 
	p_Blocks -= (PrefixSize+4+p->AlignPad);

	/* store name */
	strncpy( p->Name, Name, 4 );

	/* finally, initialize the blocks */
	p->p_Last = p->p_First = (void **)p_Blocks;
	for(i = 0; i < (Num-1); i++)
	{
	/* get next block */
		p_Blocks += blockSize;

	/* attach to end of chain */
		if( p->p_Last )
			*(p->p_Last) = (void *)p_Blocks;

	/* advance last pointer */
		p->p_Last = (void **)p_Blocks;
	}

	/* zero next pointer in last block */
	*(p->p_Last) = 0;

	/* return handle to caller */
	*p_Handle = (void *)p;

	return 0;
}



void  MEM_Free( void* p_Handle)
{
	t_MemorySegment *p = (t_MemorySegment*)p_Handle;

	if ( p && p->LocallyAllocated)
		kfree (p->p_Base);
	kfree(p);
}

#define MEM_Put(Handle, p_Block)    \
	((t_MemorySegment *)Handle)->f_MemPut( Handle, p_Block)

#endif

#endif /* __STD_EXT_H */
