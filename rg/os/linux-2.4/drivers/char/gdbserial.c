/*
 * Serial interface GDB stub
 *
 * Written (hacked together) by David Grothe (dave@gcom.com)
 *
 * Modified by Scott Foehner (sfoehner@engr.sgi.com) to allow connect
 * on boot-up
 *
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/serialP.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/gdb.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/atomic.h>

#undef	PRNT				/* define for debug printing */

#define	GDB_BUF_SIZE	512		/* power of 2, please */

static char	gdb_buf[GDB_BUF_SIZE] ;
static int	gdb_buf_in_inx ;
static atomic_t	gdb_buf_in_cnt ;
static int	gdb_buf_out_inx ;

extern void	set_debug_traps(void) ;		/* GDB routine */
extern struct serial_state *	gdb_serial_setup(int ttyS, int baud);
extern void	shutdown_for_gdb(struct async_struct * info) ;
						/* in serial.c */

int gdb_irq;
int gdb_port;
int gdb_ttyS = 1;	/* Default: ttyS1 */
int gdb_baud = 38400; 
int gdb_enter = 0;	/* Default: do not do gdb_hook on boot */
int gdb_initialized = 0;

static int initialized = -1;

/*
 * Get a byte from the hardware data buffer and return it
 */
static int	read_data_bfr(void)
{
    if (inb(gdb_port + UART_LSR) & UART_LSR_DR)
	return(inb(gdb_port + UART_RX));

    return( -1 ) ;

} /* read_data_bfr */


/*
 * Get a char if available, return -1 if nothing available.
 * Empty the receive buffer first, then look at the interface hardware.
 */
static int	read_char(void)
{
    if (atomic_read(&gdb_buf_in_cnt) != 0)	/* intr routine has q'd chars */
    {
	int		chr ;

	chr = gdb_buf[gdb_buf_out_inx++] ;
	gdb_buf_out_inx &= (GDB_BUF_SIZE - 1) ;
	atomic_dec(&gdb_buf_in_cnt) ;
	return(chr) ;
    }

    return(read_data_bfr()) ;	/* read from hardware */

} /* read_char */

/*
 * Wait until the interface can accept a char, then write it.
 */
static void	write_char(int chr)
{
    while ( !(inb(gdb_port + UART_LSR) & UART_LSR_THRE) ) ;

    outb(chr, gdb_port+UART_TX);

} /* write_char */

/*
 * This is the receiver interrupt routine for the GDB stub.
 * It will receive a limited number of characters of input
 * from the gdb  host machine and save them up in a buffer.
 *
 * When the gdb stub routine getDebugChar() is called it
 * draws characters out of the buffer until it is empty and
 * then reads directly from the serial port.
 *
 * We do not attempt to write chars from the interrupt routine
 * since the stubs do all of that via putDebugChar() which
 * writes one byte after waiting for the interface to become
 * ready.
 *
 * The debug stubs like to run with interrupts disabled since,
 * after all, they run as a consequence of a breakpoint in
 * the kernel.
 *
 * Perhaps someone who knows more about the tty driver than I
 * care to learn can make this work for any low level serial
 * driver.
 */
static void gdb_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
    int			 chr ;
    int			 iir ;

    do
    {
	chr = read_data_bfr() ;
	iir = inb(gdb_port + UART_IIR) ;
#ifdef PRNT
	printk("gdb_interrupt: chr=%02x '%c'  after read iir=%02x\n", chr,
		chr > ' ' && chr < 0x7F ? chr : ' ', iir) ;
#endif
	if (chr < 0) continue ;

        if (chr == 3)                   /* Ctrl-C means remote interrupt */
        {
            breakpoint();
            continue ;
        }

	if (atomic_read(&gdb_buf_in_cnt) >= GDB_BUF_SIZE)
	{				/* buffer overflow, clear it */
	    gdb_buf_in_inx = 0 ;
	    atomic_set(&gdb_buf_in_cnt, 0) ;
	    gdb_buf_out_inx = 0 ;
	    break ;
	}

	gdb_buf[gdb_buf_in_inx++] = chr ;
	gdb_buf_in_inx &= (GDB_BUF_SIZE - 1) ;
	atomic_inc(&gdb_buf_in_cnt) ;
    }
    while (iir & UART_IIR_RDI);

} /* gdb_interrupt */

/*
 * Just a NULL routine for testing.
 */
void gdb_null(void)
{
} /* gdb_null */


int     gdb_hook(void)
{
    int         retval ;
    struct serial_state *ser;

#ifdef CONFIG_SMP
    if (smp_num_cpus > KGDB_MAX_NO_CPUS) { 
        printk("kgdb: too manu cpus. Cannot enable debugger with more than 8 cpus\n");
	return (-1);
    }
#endif

    /*
     * Call first time just to get the ser ptr
     */
    if((ser = gdb_serial_setup(gdb_ttyS, gdb_baud)) == 0) {
        printk ("gdb_serial_setup() error");
        return(-1);
    }

    gdb_port = ser->port;
    gdb_irq = ser->irq;

    if (ser->info != NULL)
    {
	shutdown_for_gdb(ser->info) ;
	/*
	 * Call second time to do the setup now that we have
	 * shut down the previous user of the interface.
	 */
	gdb_serial_setup(gdb_ttyS, gdb_baud) ;
    }

    retval = request_irq(gdb_irq,
                         gdb_interrupt,
                         SA_INTERRUPT,
                         "GDB-stub", NULL);
    if (retval == 0)
        initialized = 1;
    else
    {
        initialized = 0;
	printk("gdb_hook: request_irq(irq=%d) failed: %d\n", gdb_irq, retval);
    }

    /*
     * Call GDB routine to setup the exception vectors for the debugger
     */
    set_debug_traps() ;

    /*
     * Call the breakpoint() routine in GDB to start the debugging
     * session.
     */
#ifndef CONFIG_X86_REMOTE_DEBUG
    printk("Waiting for connection from remote gdb... ") ;
    breakpoint() ;
    gdb_null() ;

    printk("Connected.\n");
#endif

    gdb_initialized = 1;
    return(0) ;

} /* gdb_hook_interrupt2 */

/*
 * getDebugChar
 *
 * This is a GDB stub routine.  It waits for a character from the
 * serial interface and then returns it.  If there is no serial
 * interface connection then it returns a bogus value which will
 * almost certainly cause the system to hang.
 */
int	getDebugChar(void)
{
    volatile int	chr ;

#ifdef PRNT
    printk("getDebugChar: ") ;
#endif

    while ( (chr = read_char()) < 0 ) ;


#ifdef PRNT
    printk("%c\n", chr > ' ' && chr < 0x7F ? chr : ' ') ;
#endif
    return(chr) ;

} /* getDebugChar */

/*
 * putDebugChar
 *
 * This is a GDB stub routine.  It waits until the interface is ready
 * to transmit a char and then sends it.  If there is no serial
 * interface connection then it simply returns to its caller, having
 * pretended to send the char.
 */
void	putDebugChar(int chr)
{
#ifdef PRNT
    printk("putDebugChar: chr=%02x '%c'\n", chr,
		chr > ' ' && chr < 0x7F ? chr : ' ') ;
#endif

    write_char(chr) ;	/* this routine will wait */

} /* putDebugChar */

