#include "KS8695.S"

.section ".bootstrap",#alloc,#execinstr

/* 			
; ======================================================================
; put all the time sensetive initlizations at the beginning since the 
; clock is 25 mhz by default. Do all the register setting here such as
; program SDRAM controller, anything requires delay. Change the clock 
; only after these initlization have been done so you do not have to
; worry about calculate time delay for different clock settings.
;======================================================================
*/
# Enter ABT mode and set up the ABT stack pointer
        msr     cpsr_c, #MODE_ABT | I_BIT | F_BIT       
        ldr     sp, =ABT_STACK

# Enter UNDEF mode and set up the UNDEF stack pointer
        msr     cpsr_c, #MODE_UNDEF | I_BIT | F_BIT     
        ldr     sp, =UNDEF_STACK

# Enter FIQ mode and set up the FIQ stack pointer
        msr     cpsr_c, #MODE_FIQ | I_BIT | F_BIT   
        ldr     sp, =FIQ_STACK

# Enter IRQ mode and set up the IRQ stack pointer
        msr     cpsr_c, #MODE_IRQ | I_BIT | F_BIT 
        ldr     sp, =IRQ_STACK

# Enter SVC mode and set up the SVC stack pointer 
        msr     cpsr_c, #MODE_SVC | I_BIT | F_BIT
        ldr     sp, =SVC_STACK

# Enter SYS mode and set up the SYS stack pointer for temperary use
        msr     cpsr_c, #MODE_SYS | I_BIT | F_BIT 
        ldr     sp, =RAM_LIMIT_TMP

#   load register base address and never change it again
        ldr  r4, =REG_IO_BASE                            
        ldr  r5, =REG_IO_CTRL0           @set i/o space  bank 0
	add  r5, r5, r4
	ldr  r8, =EXTIO_REG0              @bank 0 0x03200000 -- 0x035FFFFF 50 to 54 MB
        str  r8, [r5]       
	ldr  r5, =REG_IO_CTRL1           @set i/o space bank 1
	add  r5, r5, r4
	ldr  r8, =EXTIO_REG1              @bank 1 0x03600000 -- 0x039FFFFF 54 to 58MB
        str  r8, [r5]       
	ldr  r5, =REG_IO_CTRL2           @set i/o space bank 2
	add  r5, r5, r4
	ldr  r8, =EXTIO_REG2              @bank 3 0x03A00000 -- 0x03CFFFFF 58 to 62MB
        str  r8, [r5]       

/*
;=====================================================
;Set memeory as following
;ROM bank 0 flash memory 8 bit, maximum size 8MB
;ROM bank 1 sram 32 bit, size 512KB or none
;EX I/O bank 0 32 bit
;EX I/O bank 1 16 bit
;EX I/O bank 2 8  bit
;======================================================
*/	

  	ldr  r5, =REG_ROM_CTRL0          
	add  r5, r5, r4                  @reset bank 0 to 0x00000000 -- ACTUAL FLASH SIZE
   	ldr  r8, =FLASH_REG              @to free sapce for SRAM and SDRAM
        str  r8, [r5] 

    	ldr  r5, =REG_SDRAM_CTRL0        @set sdram bank 0 at end of the flash 
	add  r5, r5, r4
	ldr  r8, =TMP_SDRAM_REG0                         
	str  r8, [r5]  
	ldr  r5, =REG_SDRAM_CTRL1        @set up SDRAM bank 1, size 8 MB, 4 bank device. 32bit
	add  r5, r5, r4
	ldr  r8, =TMP_SDRAM_REG1            
	str  r8, [r5]           

# set up SDRAM control
	ldr  r5, =REG_SDRAM_GENERAL        
	add  r5, r5, r4
	ldr  r8, =SDRAM_RASCAS
	str  r8, [r5]       

# set up SDRAM buffer control
        ldr  r5, =REG_SDRAM_BUFFER        
        add  r5, r5, r4
        ldr  r8, =SDRAM_NOP_COMD             @send NOP command                     
        str  r8, [r5]
        ldr  r7, =SDRAM_NOP_COMD

01:		
        ldr   r8, [r5]
        ands  r8, r8, r7
        bne   01b
        nop

        ldr  r8, =SDRAM_PRECHARGE_CMD        @send precharge command
        str  r8, [r5]

02:	
	ldr   r8, [r5]
        ldr   r7, =SDRAM_NOP_COMD
        ands  r8, r8, r7
        bne   02b
        nop

# set up SDRAM refresh timer
        ldr  r5, =REG_SDRAM_REFRESH
        add  r5, r5, r4  
        mov  r8, #20                         @quick refresh at least twice                 
        str  r8, [r5]                        @before sending Mode command 

#wait for two refresh to complete
    
	mov   r8, #10
05:		
	subs  r8, r8, #1
	bne   05b   
	nop       

        ldr  r8, =SDRAM_REFRESH_TIMER        @set back normal refresh timer                 
        str  r8, [r5]       

	ldr  r5, =REG_SDRAM_BUFFER        
	add  r5, r5, r4
	ldr  r8, =SDRAM_MODE_COMD            @send Mode command                     
	str  r8, [r5]       

03:	
	ldr   r8, [r5]
        ands  r8, r8, r7
        bne   03b
        nop

/*	
;========================================================
; Here is memory remapping code,
; First copy two instuctions to SDRAM and jump to the 
; first, the second intruction will jump the pc back to
; remapped flash.
;========================================================
*/	

  	ldr  r5, =REG_ROM_CTRL0          
	add  r5, r5, r4                   @remapp rom bank 0 start at 0x02400000
	ldr  r8, =REM_FLASH_REG          

        stmfd r13!, {r3-r6}
        adr   r3, MAP_INSTRUC
	ldr   r4, =SDRAM_START
	ldmia r3, {r5-r6}
	stmia r4, {r5-r6}
	ldmfd r13!, {r3-r6}
	adr   r10, JUMP_TO_HERE 
	ldr   r0, =FLASH_ROM_START
	add   r10, r10, r0
	ldr   pc, =SDRAM_START
MAP_INSTRUC:	
        str  r8, [r5]    
	mov  pc, r10
JUMP_TO_HERE:		
        ldr  r5, =REG_SDRAM_BUFFER        
        add  r5, r5, r4
        ldr  r8, =SDRAM_PRECHARGE_CMD         @send precharge command
        str  r8, [r5]
107:	
        ldr   r8, [r5]
        ldr   r7, =SDRAM_NOP_COMD
        ands  r8, r8, r7
        bne   107b    
        nop
     
	ldr  r5, =REG_SDRAM_CTRL0              @set sdram bank 0 at end of the sram 0x080000
	add  r5, r5, r4
	ldr  r8, =REM_SDRAM_REG0                         
	str  r8, [r5]  
	ldr  r5, =REG_SDRAM_CTRL1              @set up SDRAM bank 1, size 8 MB, 4 bank device. 32bit
	add  r5, r5, r4
	ldr  r8, =REM_SDRAM_REG1            
        str  r8, [r5]         
	
# enable all the ext i/o flash and rom banks

	ldr  r5, =REG_ROM_GENERAL        
	add  r5, r5, r4
	ldr  r8, =ROM_GENERAL_SETTING              
	str  r8, [r5]       

#   change system clock here if you need more speed.
	ldr  r5, =REG_SYSTEN_BUS_CLOCK
	add  r5, r5, r4
	ldr  r8, =SYSTEN_BUS_CLOCK_125
        str  r8, [r5]       
    
        ldr  r5, =REG_GPIO_DATA
        add  r5, r5, r4
        ldr  r1, =INIT_GPIO_DATA
        str  r1, [r5] 

        ldr  r5, =REG_GPIO_CTRL
        add  r5, r5, r4
        ldr  r1, =INIT_GPIO_CTRL
        str  r1, [r5] 
	 
# set the MODE register last to
	ldr  r5, =REG_GPIO_MODE 
	add  r5, r5, r4
        ldr  r1, =INIT_GPIO_MODE
        str  r1, [r5]
	mov  r0, r0

# enable FIFO write and read
	ldr  r5, =REG_SDRAM_BUFFER        
	add  r5, r5, r4
        ldr  r8, =0x1F00000                           @enable FIFOs                     
        str  r8, [r5]       

# reset the SVC stack point after the memory remap
        msr     CPSR_c, #MODE_SYS | I_BIT | F_BIT    @disable interrupts*/
        ldr     SP, =SYS_STACK
           
        nop
        nop
        nop

    b 666f
    .pool
666:
