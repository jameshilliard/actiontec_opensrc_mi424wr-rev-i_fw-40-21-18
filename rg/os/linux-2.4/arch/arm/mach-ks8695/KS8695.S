#define SDRAM_BANK0_SIZE (0x1000000)
#define SDRAM_BANK1_SIZE (0x1000000)

/* on chip regsters definitions*/	 	
#define REG_IO_BASE		        0x03FF0000 
#define REG_SYSTEN_CONFIG	        0x00000000
#define REG_SYSTEN_BUS_CLOCK            0x00000004
#define REG_IO_CTRL0	                0x00004000
#define REG_IO_CTRL1	                0x00004004
#define REG_IO_CTRL2	                0x00004008
#define REG_ROM_CTRL0	                0x00004010
#define REG_ROM_CTRL1	                0x00004014
#define REG_ROM_GENERAL	                0x00004020
#define REG_SDRAM_CTRL0	                0x00004030
#define REG_SDRAM_CTRL1	                0x00004034
#define REG_SDRAM_GENERAL               0x00004038
#define REG_SDRAM_BUFFER                0x0000403C
#define REG_SDRAM_REFRESH               0x00004040
#define REG_WAN_DMA_TX                  0x00006000
#define REG_WAN_DMA_RX                  0x00006004
#define REG_WAN_DMA_TX_START            0x00006008    
#define REG_WAN_DMA_RX_START            0x0000600C    
#define REG_WAN_TX_LIST                 0x00006010
#define REG_WAN_RX_LIST                 0x00006014
#define REG_WAN_MAC_LOW                 0x00006018
#define REG_WAN_MAC_HIGH                0x0000601C
#define REG_WAN_MAC_ELOW                0x00006080
#define REG_WAN_MAC_EHIGH               0x00006084
#define REG_LAN_DMA_TX                  0x00008000
#define REG_LAN_DMA_RX                  0x00008004
#define REG_LAN_DMA_TX_START            0x00008008    
#define REG_LAN_DMA_RX_START            0x0000800C    
#define REG_LAN_TX_LIST                 0x00008010
#define REG_LAN_RX_LIST                 0x00008014
#define REG_LAN_MAC_LOW                 0x00008018
#define REG_LAN_MAC_HIGH                0x0000801C
#define REG_LAN_MAC_ELOW                0x00008080
#define REG_LAN_MAC_EHIGH               0x00008084

/* EMAC control registers offset difinitions */
#ifdef KS8695
 #define REG_EMAC_DMA_TX                0x0000A000
 #define REG_EMAC_DMA_RX                0x0000A004
 #define REG_EMAC_DMA_TX_START          0x0000A008    
 #define REG_EMAC_DMA_RX_START          0x0000A00C    
 #define REG_EMAC_TX_LIST               0x0000A010
 #define REG_EMAC_RX_LIST               0x0000A014
 #define REG_EMAC_MAC_LOW               0x0000A018
 #define REG_EMAC_MAC_HIGH              0x0000A01C
 #define REG_EMAC_MAC_ELOW              0x0000A080
 #define REG_EMAC_MAC_EHIGH             0x0000A084
 #define REG_INT_EMAC_PRIORITY          0x0000E210
 #define REG_EMAC_CONTROL               0x0000EA08
#endif

/* UART control registers offsets */
#define REG_UART_RX_BUFFER              0x0000E000
#define REG_UART_TX_HOLDING             0x0000E004
#define REG_UART_FIFO_CTRL              0x0000E008
#define REG_UART_LINE_CTRL              0x0000E00C
#define REG_UART_MODEM_CTRL             0x0000E010
#define REG_UART_LINE_STATUS            0x0000E014
#define REG_UART_MODEM_STATUS           0x0000E018
#define REG_UART_DIVISOR                0x0000E01C
#define REG_UART_STATUS                 0x0000E020

/* Interrupt controlller registers offsets */
#define REG_INT_CONTL                   0x0000E200
#define REG_INT_ENABLE                  0x0000E204
#define REG_INT_STATUS                  0x0000E208
#define REG_INT_WAN_PRIORITY            0x0000E20C
#define REG_INT_LAN_PRIORITY            0x0000E214
#define REG_INT_TIMER_PRIORITY          0x0000E218
#define REG_INT_UART_PRIORITY           0x0000E21C
#define REG_INT_EXT_PRIORITY            0x0000E220
#define REG_INT_CHAN_PRIORITY           0x0000E224
#define REG_INT_BUSERROR_PRO            0x0000E228
#define REG_INT_MASK_STATUS             0x0000E22C
#define REG_FIQ_PEND_PRIORITY           0x0000E230
#define REG_IRQ_PEND_PRIORITY           0x0000E234

/* timer registers offsets */
#define REG_TIMER_CTRL                  0x0000E400
#define REG_TIMER1                      0x0000E404
#define REG_TIMER0                      0x0000E408
#define REG_TIMER1_PCOUNT               0x0000E40C
#define REG_TIMER0_PCOUNT               0x0000E410

/* GPIO register offsets */
#define REG_GPIO_MODE		        0x0000E600
#define REG_GPIO_CTRL		    	0x0000E604
#define REG_GPIO_DATA		    	0x0000E608

/* switch control register offsets */
#ifdef KS8695P
 #define REG_SWITCH_CTRL2               0x0000E808
 #define REG_SWITCH_PORT11              0x0000E80C
 #define REG_SWITCH_PORT12              0x0000E810
 #define REG_SWITCH_PORT13              0x0000E814
 #define REG_SWITCH_PORT21              0x0000E818
 #define REG_SWITCH_PORT22              0x0000E81C
 #define REG_SWITCH_PORT23              0x0000E820
 #define REG_SWITCH_PORT31              0x0000E824
 #define REG_SWITCH_PORT32              0x0000E828
 #define REG_SWITCH_PORT33              0x0000E82C
 #define REG_SWITCH_PORT41              0x0000E830
 #define REG_SWITCH_PORT42              0x0000E834
 #define REG_SWITCH_PORT43              0x0000E838
 #define REG_SWITCH_PORT51              0x0000E83C
 #define REG_SWITCH_PORT52              0x0000E840
 #define REG_SWITCH_PORT53              0x0000E844
 #define REG_SWITCH_AUTO0               0x0000E848
 #define REG_SWITCH_AUTO1               0x0000E84C
 #define REG_SWITCH_IACTRL              0x0000E850
 #define REG_SWITCH_IADH2               0x0000E854
 #define REG_SWITCH_IADH1               0x0000E858
 #define REG_SWITCH_IADL                0x0000E85C
 #define REG_SWITCH_ADVANCED            0x0000E860
 #define REG_TOS_HIGH                   0x0000E864
 #define REG_TOS_LOW                    0x0000E868
 #define REG_SWITCH_MAC_HIGH            0x0000E86C
 #define REG_SWITCH_MAC_LOW             0x0000E870
 #define REG_LAN12_POWERMAGR            0x0000E874
 #define REG_LAN34_POWERMAGR            0x0000E878
 #define REG_DIG_TEST                   0x0000E87C
 #define REG_ANA_TEST                   0x0000E880
 #define REG_WAN_POW_SAVE               0x0000EA1C
#else		
 #define REG_SWITCH_PORT1               0x0000E808
 #define REG_SWITCH_PORT2               0x0000E80C
 #define REG_SWITCH_PORT3               0x0000E810
 #define REG_SWITCH_PORT4               0x0000E814
 #define REG_SWITCH_PORT5               0x0000E818
 #define REG_SWITCH_AUTO0               0x0000E81C
 #define REG_SWITCH_AUTO1               0x0000E820
 #define REG_SWITCH_ADVANCED            0x0000E830
 #define REG_MANAGE_COUNTER             0x0000E844
 #define REG_MANAGE_DATA                0x0000E848
 #define REG_LAN12_POWERMAGR            0x0000E84C
 #define REG_LAN34_POWERMAGR            0x0000E850
 #define REG_DSCP_HIGH                  0x0000E834
 #define REG_DSCP_LOW                   0x0000E838
 #define REG_SWITCH_MAC_HIGH            0x0000E83C
 #define REG_SWITCH_MAC_LOW             0x0000E840
 #define REG_SWITCH_IACTRL              0x0000E824
 #define REG_SWITCH_IADH1               0x0000E828
 #define REG_SWITCH_IADL                0x0000E82C
#endif	

/* miscellaneours register offsets */
#define REG_SWITCH_CTRL0                0x0000E800
#define REG_SWITCH_CTRL1                0x0000E804
#define REG_WAN_CONTROL                 0x0000EA0C
#define REG_WAN_POWERMAGR               0x0000EA10
#define REG_WAN_PHY_CONTROL             0x0000EA14
#define REG_WAN_PHY_STATUS              0x0000EA18

/* register definitions for PCI interface */
#define REG_PCI_CONFID          	0x00002000
#define REG_PCI_CMD             	0x00002004
#define REG_PCI_REV             	0x00002008
#define REG_PCI_LTYTIMER        	0x0000200C
#define REG_PCI_BASEMEM         	0x00002010
#define REG_PCI_BASEFST         	0x00002014
#define REG_PCI_BASELST         	0x00002028
#define REG_PCI_SYSID           	0x0000202C
#define REG_PCI_INT             	0x0000203C
#define REG_PCI_CNFADD          	0x00002100
#define REG_PCI_CNGDATA         	0x00002104
#define REG_PCI_MODE            	0x00002200
#define REG_PCI_CONTROL         	0x00002204
#define REG_PCI_MEMBASE         	0x00002208
#define REG_PCI_MEMBCTRL        	0x0000220C
#define REG_PCI_MBMARSK         	0x00002210
#define REG_PCI_MBTRANS         	0x00002214
#define REG_PCI_IOBASE          	0x00002218
#define REG_PCI_IOBCTRL         	0x0000221C
#define REG_PCI_IOBMARSK        	0x00002220
#define REG_PCI_IOBTRANS        	0x00002224

/* device ID */
#define REG_DEVICE_ID                   0x0000EA00
#define REG_REVISION_ID                 0x0000EA04

/* following are parameters to registers */
#ifdef KS8695P
 #define INIT_GPIO_MODE                0xFFFF
#else
 #define INIT_GPIO_MODE                0xFF
#endif

#define INIT_GPIO_DATA                 0
#define INIT_GPIO_CTRL                 0

/* clock indices */
#define SYSTEN_BUS_CLOCK_125        0x0 
#define SYSTEN_BUS_CLOCK_100        0x1
#define SYSTEN_BUS_CLOCK_625        0x2
#define SYSTEN_BUS_CLOCK_050        0x3
#define SYSTEN_BUS_CLOCK_417        0x4
#define SYSTEN_BUS_CLOCK_333        0x5
#define SYSTEN_BUS_CLOCK_313        0x6
#define SYSTEN_BUS_CLOCK_025        0x7

/* cpu parameters */
#define MODE_USR                    0x10
#define MODE_FIQ                    0x11
#define MODE_IRQ                    0x12
#define MODE_SVC                    0x13
#define MODE_ABT                    0x17
#define MODE_UNDEF                  0x1B	
#define MODE_SYS                    0x1F  /* available on ARM Arch 4 and later */
#define I_BIT                       0x80  /* when I bit is set, IRQ is disabled */
#define F_BIT                       0x40  /* when F bit is set, FIQ is disabled */
   
/* other parameters to set up FLASH/ROM/SRAM configuration registers */
#define ROM_BANK_ACCTM2             0x0
#define ROM_BANK_ACCTM3             (0x1 << 4)
#define ROM_BANK_ACCTM4             (0x2 << 4)
#define ROM_BANK_ACCTM5             (0x3 << 4)
#define ROM_BANK_ACCTM6             (0x4 << 4)
#define ROM_BANK_ACCTM7             (0x5 << 4)
#define ROM_BANK_ACCTM8             (0x6 << 4)
#define ROM_BANK_ACCTM9             (0x7 << 4)
#define ROM_BANK_PACTM2             0x0
#define ROM_BANK_PACTM3             (0x1 << 2)
#define ROM_BANK_PACTM4             (0x2 << 2)
#define ROM_BANK_PACTM5             (0x3 << 2)
#define ROM_BANK_PMOD0              0x0
#define ROM_BANK_PMOD1              0x1
#define ROM_BANK_PMOD2              0x2
#define ROM_BANK_PMOD3              0x3

/* SDRAM banks */
#define BOOT_START                  0x00400000   /*linux bootloader C code start address*/
#define DIAG_START                  0x00080000   /*diagnostic program C code start address*/
#define LEVEL1TABLE                 0x00040000   /*where the MMU table is stored */

#define SDRAM_NOP_COMD              0x30000
#define SDRAM_PRECHARGE_CMD         0x10000
#define SDRAM_REFRESH_TIMER         360                               
#define SDRAM_MODE_COMD             0x20033    
#define SDRAM_RASCAS                0x0000000A

#ifdef USE_SRAM
 #define ROM_GENERAL_SETTING        0x0000000D   
#else
 #define ROM_GENERAL_SETTING        0x00000001  
#endif	

#define FLASH_BANK_SIZE		    0x00800000   
#define SRAM_BANK_SIZE		    0x00080000       

/* memory configuration */
#define FLASH_BANK		    0x00000000
#define REMAPPED_FLASH_BANK	    0x02800000   

#ifdef USE_SRAM
 #define SRAM_BANK		    FLASH_BANK_SIZE  
 #define REMAPPED_SRAM_BANK         0x00000000   	
 #define SDRAM_BANK_0		    (SRAM_BANK + SRAM_BANK_SIZE)           
 #define REMAPPED_SDRAM_BANK_0      (REMAPPED_SRAM_BANK + SRAM_BANK_SIZE)       
#else
 #define SDRAM_BANK_0		    FLASH_BANK_SIZE  
 #define REMAPPED_SDRAM_BANK_0      0x0
#endif

#define SDRAM_BANK_1		    (SDRAM_BANK_0 + SDRAM_BANK0_SIZE)	
#define SDRAM_BANK_END              (SDRAM_BANK_1 + SDRAM_BANK1_SIZE)
#define REMAPPED_SDRAM_BANK_1       (REMAPPED_SDRAM_BANK_0 + SDRAM_BANK0_SIZE)     
#define REMAPPED_SDRAM_BANK_END     (REMAPPED_SDRAM_BANK_1 + SDRAM_BANK1_SIZE)	
		
#define SDRAM_BANK_COLAB8           0x0
#define SDRAM_BANK_COLAB9           (0x1 << 8)
#define SDRAM_BANK_COLAB10          (0x2 << 8)
#define SDRAM_BANK_COLAB11          (0x3 << 8)
#define SDRAM_UNM_BANKS2            0x0
#define SDRAM_UNM_BANKS4            (0x1 << 3)
#define SDRAM_BANKS_DBW0            0x0
#define SDRAM_BANKS_DBW8            (0x1 << 1)
#define SDRAM_BANKS_DBW16           (0x2 << 1)
#define SDRAM_BANKS_DBW32           (0x3 << 1)

/* External I/O banks difintions */
#define EXT_IO_BANK_SIZE	    0x00400000    
#define EXT_IO_BANK_0		    0x03200000    
#define EXT_IO_BANK_1		    (EXT_IO_BANK_0 + EXT_IO_BANK_SIZE)  
#define EXT_IO_BANK_2		    (EXT_IO_BANK_1 + EXT_IO_BANK_SIZE)   
#define EXT_IOBANK_CLOCK0           0x0
#define EXT_IOBANK_CLOCK1           0x1
#define EXT_IOBANK_CLOCK2           0x2
#define EXT_IOBANK_CLOCK3           0x3
#define EXT_IOBANK_CLOCK4           0x4
#define EXT_IOBANK_CLOCK5           0x5
#define EXT_IOBANK_CLOCK6           0x6
#define EXT_IOBANK_CLOCK7           0x7
	
/* --- System memory locations */
#ifdef ROM_RAM_REMAP
 #define RAM_LIMIT                  REMAPPED_SDRAM_BANK_END  
#else
 #define RAM_LIMIT                  SDRAM_BANK_END  
#endif  

#define ABT_STACK                   RAM_LIMIT 
#define UNDEF_STACK                 ABT_STACK - 1024 
#define SVC_STACK                   UNDEF_STACK - 1024    
#define IRQ_STACK                   SVC_STACK - 2048      
#define FIQ_STACK                   IRQ_STACK - 4096
#define SYS_STACK                   FIQ_STACK - 4096
#define USR_STACK                   SYS_STACK - 4096
#define RAM_LIMIT_TMP               SDRAM_BANK_END - 2048
#define FLASH_ROM_START             REMAPPED_FLASH_BANK   
#define SDRAM_START                 SDRAM_BANK_0 

/*
 *  calculating all the memory bank configration register value which used to take me a lot of 
 *  my time and often ends up with errors and now it will be done automatioally.
 */	

#define FLASH_REG       ((((FLASH_BANK+FLASH_BANK_SIZE-1)>>16)<<22)|((FLASH_BANK>>16)<<12)|ROM_BANK_PMOD0|ROM_BANK_ACCTM9)

#ifdef USE_SRAM
 #define SRAM_REG       (((SRAM_BANK+SRAM_BANK_SIZE-1)>>16)<<22)|((SRAM_BANK>>16)<<12)|ROM_BANK_PMOD0|ROM_BANK_ACCTM5
 #define REM_SRAM_REG   (((REMAPPED_SRAM_BANK+SRAM_BANK_SIZE-1)>>16)<<22)|((REMAPPED_SRAM_BANK>>16)<<12)|ROM_BANK_PMOD0|ROM_BANK_ACCTM5
#endif
 
#define TMP_SDRAM_REG0  (((SDRAM_BANK_0+SDRAM_BANK0_SIZE-1)>>16)<<22)|((SDRAM_BANK_0>>16)<<12)|SDRAM_UNM_BANKS4|SDRAM_BANKS_DBW32

#if SDRAM_BANK1_SIZE == 0
 #define TMP_SDRAM_REG1    0
 #define SDRAM_REG1        0
 #define REM_SDRAM_REG1    0	
#else  
 #define TMP_SDRAM_REG1    (((SDRAM_BANK_1+SDRAM_BANK1_SIZE-1)>>16)<<22)|((SDRAM_BANK_1>>16)<<12)|SDRAM_UNM_BANKS4|SDRAM_BANKS_DBW32
 #define SDRAM_REG1        (((SDRAM_BANK_1+SDRAM_BANK1_SIZE-1)>>16)<<22)|((SDRAM_BANK_1>>16)<<12)|SDRAM_UNM_BANKS4|SDRAM_BANKS_DBW32
 #define REM_SDRAM_REG1    (((REMAPPED_SDRAM_BANK_1+SDRAM_BANK1_SIZE-1)>>16)<<22)|((REMAPPED_SDRAM_BANK_1>>16)<<12)|SDRAM_UNM_BANKS4|SDRAM_BANKS_DBW32	
#endif

#define SDRAM_REG0         (((SDRAM_BANK_0+SDRAM_BANK0_SIZE-1)>>16)<<22)|((SDRAM_BANK_0>>16)<<12)|SDRAM_UNM_BANKS4|SDRAM_BANKS_DBW32
#define REM_FLASH_REG      (((REMAPPED_FLASH_BANK+FLASH_BANK_SIZE-1)>>16)<<22)|((REMAPPED_FLASH_BANK>>16)<<12)|ROM_BANK_PMOD0|ROM_BANK_ACCTM9
#define REM_SDRAM_REG0     (((REMAPPED_SDRAM_BANK_0+SDRAM_BANK0_SIZE-1)>>16)<<22)|((REMAPPED_SDRAM_BANK_0>>16)<<12)|SDRAM_UNM_BANKS4|SDRAM_BANKS_DBW32
#define EXTIO_REG0         (((EXT_IO_BANK_0+EXT_IO_BANK_SIZE-1)>>16)<<22)|((EXT_IO_BANK_0>>16)<<12)|(EXT_IOBANK_CLOCK1<<3)
#define EXTIO_REG1         (((EXT_IO_BANK_1+EXT_IO_BANK_SIZE-1)>>16)<<22)|((EXT_IO_BANK_1>>16)<<12)|(EXT_IOBANK_CLOCK1<<3)
#define EXTIO_REG2         (((EXT_IO_BANK_2+EXT_IO_BANK_SIZE-1)>>16)<<22)|((EXT_IO_BANK_2>>16)<<12)|(EXT_IOBANK_CLOCK1<<3)
