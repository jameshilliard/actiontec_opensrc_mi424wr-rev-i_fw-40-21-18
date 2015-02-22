/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for Strong Arm 1100 by Thilo Jeremias, Keith & Koep GmbH
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

/*
 * ARM port Copyright (c) 2002 MontaVista Software, Inc
 *
 * Authors:  George Davis <davis_g@mvista.com>
 *          Deepak Saxena <dsaxena@mvista.com>
 *
 *
 * See Documentation/ARM/kgdb for information on porting to a new board
 *
 * tabstop=3 to make this readable
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <asm/kgdb.h>

#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

// #ifdef DEBUG_DEBUGGER

#define BUFMAX 2048

static char initialized = 0;  

static char remote_debug = 0;

static const char hexchars[]="0123456789abcdef";

static int mem_err = 0;

unsigned long get_next_pc(struct pt_regs *);
/* Jump buffer for kgdb_setjmp/longjmp */
static int fault_jmp_buf[32];



// #define kgdb_debug(x, args...)  printk(x, ## args)

#define kgdb_debug(x, args...)


/*
 * GDB assumes that we're a user process being debugged, so 
 * it will send us an SWI command to write into memory as the
 * debug trap. This doesn't work for kernel debugging as 
 * in the kernel we're already in SVC mode, and the SWI instruction
 * would cause us to loose the kernel LR, which is a bad thing.
 * (see ARM ARM for reasoning)
 *
 * By doing this as an undefined instruction trap, we force a mode
 * switch from SVC which allows us to save full kernel state.
 * If we see GDB writing 4 bytes == GDB_BREAKINST to a memory 
 * location, we just replace it with KGDB_BREAKINST.
 *
 * We also define a KGDB_COMPILED_BREAK which can be used to compile
 * in breakpoints. This is important for things like sysrq-G and for
 * the initial breakpoint from trap_init(). 
 *
 * Note to ARM HW designers: Add real trap support like SH && PPC to 
 * make our lives much much simpler.
 */
#define	GDB_BREAKINST			0xef9f0001
#define	KGDB_BREAKINST			0xe7ffdefe
#define	KGDB_COMPILED_BREAK	0xe7ffdeff

/*
 * Various conversion functions 
 */
static int hex(unsigned char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

static unsigned char *
mem2hex(const char *mem, char *buf, int count)
{
	unsigned char ch;

	/* Accessing 16 bit and 32 bit objects in a single
	 * load instruction is required to avoid bad side
	 * effects for some IO registers.
	 */

	if ((count == 2) && (((long)mem & 1) == 0)) 
	{
		unsigned short tmp_s = *(unsigned short *)mem;
		mem += 2;
#ifdef CONFIG_CPU_BIG_ENDIAN
		*buf++ = hexchars[(tmp_s >> 12) & 0xf];
		*buf++ = hexchars[(tmp_s >> 8) & 0xf];
		*buf++ = hexchars[(tmp_s >> 4) & 0xf];
		*buf++ = hexchars[tmp_s & 0xf];
#else
		*buf++ = hexchars[(tmp_s >> 4) & 0xf];
		*buf++ = hexchars[tmp_s & 0xf];
		*buf++ = hexchars[(tmp_s >> 12) & 0xf];
		*buf++ = hexchars[(tmp_s >> 8) & 0xf];
#endif
	} 
	else if ((count == 4) && (((long)mem & 3) == 0)) 
	{
		unsigned long tmp_l = *(unsigned int *)mem;
		mem += 4;
#ifdef CONFIG_CPU_BIG_ENDIAN
		*buf++ = hexchars[(tmp_l >> 28) & 0xf];
		*buf++ = hexchars[(tmp_l >> 24) & 0xf];
		*buf++ = hexchars[(tmp_l >> 20) & 0xf];
		*buf++ = hexchars[(tmp_l >> 16) & 0xf];
		*buf++ = hexchars[(tmp_l >> 12) & 0xf];
		*buf++ = hexchars[(tmp_l >> 8) & 0xf];
		*buf++ = hexchars[(tmp_l >> 4) & 0xf];
		*buf++ = hexchars[tmp_l & 0xf];
#else
		*buf++ = hexchars[(tmp_l >> 4) & 0xf];
		*buf++ = hexchars[tmp_l & 0xf];
		*buf++ = hexchars[(tmp_l >> 12) & 0xf];
		*buf++ = hexchars[(tmp_l >> 8) & 0xf];
		*buf++ = hexchars[(tmp_l >> 20) & 0xf];
		*buf++ = hexchars[(tmp_l >> 16) & 0xf];
		*buf++ = hexchars[(tmp_l >> 28) & 0xf];
		*buf++ = hexchars[(tmp_l >> 24) & 0xf];
#endif
	} 
	else 
	{
		while (count-- > 0) 
		{
			ch = *mem++;
			*buf++ = hexchars[ch >> 4];
			*buf++ = hexchars[ch & 0xf];
		}
	}

	*buf = 0;
	return buf;
}

/* 
 * convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written.  
 */
static char *
hex2mem(char *buf, char *mem, int count)
{
	unsigned char ch;
	int i;
	char *orig_mem;

	orig_mem = mem;

/*
	if (kgdb_setjmp((long*)fault_jmp_buf) == 0) {
		debugger_fault_handler = kgdb_fault_handler;
*/

		/* Accessing 16 bit and 32 bit objects in a single
		** store instruction is required to avoid bad side
		** effects for some IO registers.
		*/

		if ((count == 2) && (((long)mem & 1) == 0)) {
			unsigned short tmp_s = 0;
			
#ifdef CONFIG_CPU_BIG_ENDIAN
			tmp_s |= hex(*buf++) << 12;
			tmp_s |= hex(*buf++) << 8;
			tmp_s |= hex(*buf++) << 4;
			tmp_s |= hex(*buf++);
#else
			tmp_s |= hex(*buf++) << 4;
			tmp_s |= hex(*buf++);
			tmp_s |= hex(*buf++) << 12;
			tmp_s |= hex(*buf++) << 8;
#endif
			*(unsigned short *)mem = tmp_s;
			mem += 2;
		} else if ((count == 4) && (((long)mem & 3) == 0)) {
			unsigned long tmp_l = 0;

#ifdef CONFIG_CPU_BIG_ENDIAN
			tmp_l |= hex(*buf++) << 28;
			tmp_l |= hex(*buf++) << 24;
			tmp_l |= hex(*buf++) << 20;
			tmp_l |= hex(*buf++) << 16;
			tmp_l |= hex(*buf++) << 12;
			tmp_l |= hex(*buf++) << 8;
			tmp_l |= hex(*buf++) << 4;
			tmp_l |= hex(*buf++);
#else
			tmp_l |= hex(*buf++) << 4;
			tmp_l |= hex(*buf++);
			tmp_l |= hex(*buf++) << 12;
			tmp_l |= hex(*buf++) << 8;
			tmp_l |= hex(*buf++) << 20;
			tmp_l |= hex(*buf++) << 16;
			tmp_l |= hex(*buf++) << 28;
			tmp_l |= hex(*buf++) << 24;
#endif
			*(unsigned long *)mem = tmp_l;
			mem += 4;

		} else {
			for (i=0; i<count; i++) {
				ch = hex(*buf++) << 4;
				ch |= hex(*buf++);
				*mem++ = ch;
			}
		}


/*
	} else {
	}
*/
	return mem;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int
hexToInt(char **ptr, int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;
/*
	if (kgdb_setjmp((long*)fault_jmp_buf) == 0) {
		debugger_fault_handler = kgdb_fault_handler;
*/
		while (**ptr) {
			hexValue = hex(**ptr);
			if (hexValue < 0)
				break;

			*intValue = (*intValue << 4) | hexValue;
			numChars ++;

			(*ptr)++;
		}
#if 0
	} 
	else {
		/* error condition */
	}
	debugger_fault_handler = 0;
#endif

	return (numChars);
}

char  remcomInBuffer[BUFMAX];
char  remcomOutBuffer[BUFMAX];
static short error;

/* Address of a routine to RTE to if we get a memory fault.  */
static volatile void (*_mem_fault_routine)(void) = NULL;


int connected=0;

void kgdb_msg(char *msg)
{
	char *emsg; 

	for(emsg=msg;*emsg;emsg++); 
	
	remcomOutBuffer[0] = 'O';

	mem2hex(msg, remcomOutBuffer+1, emsg-msg); 

	if(kgdb_active()) kgdb_put_packet(remcomOutBuffer);
}

#define PC_REGNUM	0x0f
#define	LR_REGNUM	0x0e
#define	SP_REGNUM	0x0d

static int kgdb_initialized = 0;
static int gdb_connected = 0;
int kgdb_fault_expected = 0;	/* Boolean to ignore bus errs (i.e. in GDB) */

/* A bus error has occurred - perform a longjmp to return execution and
 *    allow handling of the error */
void kgdb_handle_bus_error(void)
{
	kgdb_longjmp(fault_jmp_buf, 1);
}

/* Reply that all was well */
static void send_ok_msg(void)
{
	kgdb_put_packet("OK");
}
 
/* Reply that an error occurred */
static void send_err_msg(void)
{
	kgdb_put_packet("E01");
}

/* Read memory due to 'm' message */
static void read_mem_msg(void)
{
	char *ptr;
	int addr;
	int length;

	/* Jmp, disable bus error handler */
	if (kgdb_setjmp(fault_jmp_buf) == 0) {

		kgdb_fault_expected = 1;

		/* Walk through, have m<addr>,<length> */
		ptr = &remcomInBuffer[1];
		if (hexToInt(&ptr, &addr) && (*ptr++ == ','))
			if (hexToInt(&ptr, &length)) {
				ptr = 0;
				if (length * 2 > BUFMAX)
					length = BUFMAX / 2;
				mem2hex((char *) addr, remcomOutBuffer, length);
			}
		if (ptr)
			send_err_msg();
		else
			kgdb_put_packet(remcomOutBuffer);
	} else
		send_err_msg();

	/* Restore bus error handler */
	kgdb_fault_expected = 0;
}

/* Write memory due to 'M' or 'X' message */
static void write_mem_msg(int binary)
{
	char *ptr;
	int addr;
	int length;

	if (kgdb_setjmp(fault_jmp_buf) == 0) {

		kgdb_fault_expected = 1;

		/* Walk through, have M<addr>,<length>:<data> */
		ptr = &remcomInBuffer[1];
		if (hexToInt(&ptr, &addr) && (*ptr++ == ','))
			if (hexToInt(&ptr, &length) && (*ptr++ == ':')) {
#if	0	/* Not yet... */
				if (binary)
					ebin2mem(ptr, (char*)addr, length);
				else
#endif
					hex2mem(ptr, (char*)addr, length);
				ptr = 0;
			
				/*
				 * Trap breakpoints
				 */
				if(length == 4 && !(addr & 0x3) &&
					*((unsigned *)addr) == GDB_BREAKINST)
					*((unsigned *)addr) = KGDB_BREAKINST;

				cpu_cache_clean_invalidate_all();

				send_ok_msg();
			}
		if (ptr)
			send_err_msg();
	} else
		send_err_msg();

	/* Restore bus error handler */
	kgdb_fault_expected = 0;
}

static unsigned int saved_instruction = 0;
static unsigned int *saved_addr = 0;


/*
 * This function does all command procesing for interfacing to gdb.
 */
void do_kgdb(struct pt_regs *regs, unsigned char sigval)
{
	static int addr, length;
	static char* ptr;
	static unsigned char zero[96] = { 0 };	/* zero buffer for fp0-fp7 */
	unsigned int *PC; 
	unsigned int flags;
	int i = 0;
	extern void dump_all(struct task_struct *tsk, struct pt_regs *regs);
 
  	save_flags_cli(flags);
	
	/* dump stack before entering KGDB to have some debug information 
	 * without having to connect gdb 
	 */
	dump_all(current, regs);

	kgdb_debug("Entered KGDB\n");

	/*
	 * Check to see if this is a compiled in breakpoint
	 * (sysrq-G or initial breakpoint).  If so, we
	 * need to increment the PC to the next instruction
	 * so that we don't infinite loop.
	 *
	 * NOTE: THIS COULD BE BAD.  We're reading the PC and
	 * if the cause of the fault is a bad PC, we're going 
	 * to suffer massive death. Need to find some way to
	 * validate the PC address or use our setjmp/longjmp.
	 */
	if(sigval == SIGTRAP)
	{
		PC = (unsigned *)regs->ARM_pc;
		if(*PC == KGDB_COMPILED_BREAK) 
			regs->ARM_pc += 4;
	}	

	/* 
	 * reply to host that an exception has occurred 
	 *
	 * We don't do this on the first call as it would cause a sync problem.
	 */
	if(kgdb_initialized)
	{
		remcomOutBuffer[0] = 'S';
		remcomOutBuffer[1] =  hexchars[sigval >> 4];
		remcomOutBuffer[2] =  hexchars[sigval % 0xf];
		remcomOutBuffer[3] = 0;
	
		kgdb_put_packet(remcomOutBuffer);
	}
	else
	{
		/*
		 * This is the first breakpoint, called from 
		 * start_kernel or elsewhere. We need to 
		 * (re-)initialize the I/O subsystem.
		 */

		printk("Breaking into KGDB\n");

		if(kgdb_io_init())
		{
			kgdb_debug("KGB I/O INIT FAILED...HALTING!");
			while(1){ };
		}
	
		kgdb_initialized = 1;
	}

	/*
	 * If we had a step or stepi, restore the original instruction
	 */
	if(saved_instruction)
	{
		*saved_addr = saved_instruction;

		saved_instruction = saved_addr = 0;
	}

	while (1) { 
		remcomOutBuffer[0] = 0;

		kgdb_get_packet(remcomInBuffer, BUFMAX);

		/*
		 * We're not connected until we've received something
		 * from the client.
		 */
		gdb_connected = 1;

		switch (remcomInBuffer[0]) 
		{
		case '?':   /* Report most recent signal */
			remcomOutBuffer[0] = 'S';
               		remcomOutBuffer[1] =  hexchars[sigval >> 4]; 
			remcomOutBuffer[2] =  hexchars[sigval % 16];
			remcomOutBuffer[3] = 0;
    			kgdb_put_packet(remcomOutBuffer);
			break;

		case 'd': /* toggle debug flag */
			remote_debug = !(remote_debug);  
               		break;

		case 'g': /* return the value of the CPU registers */

			ptr = remcomOutBuffer;

			/*
			 * Get r0->r15 first
			 */
			ptr = mem2hex((char *)regs, ptr, 16*4);

			/*
			 * GDB expects f0-f7 (96 bits each...)
			 */
			ptr = mem2hex((char*)zero, ptr, 8 * 12);	

			/*
			 * FPS always == 0 in kernel
			 */
			ptr = mem2hex((char *)&zero, ptr, 4);

			/*
			 * CPSR
			 */
			ptr = mem2hex((char*)&regs->ARM_cpsr, ptr, 4);
    			kgdb_put_packet(remcomOutBuffer);

               		break;

     		case 'G' : /* set the value of the CPU registers - return OK */
			ptr = &remcomInBuffer[1];
		
			/*
			 * r0->r15
			 */
			ptr = hex2mem(ptr, (char*)regs, 16 * 4);

			/*
			 * Skip FP
			 */
			ptr += 8 * 12;

			/*
			 * Skip FPS
			 */
			ptr += 4;

			/*
			 * CPSR
			 */
			ptr = hex2mem(ptr, (char*)&regs->ARM_cpsr, 4);

			strcpy(remcomOutBuffer, "OK");
    			kgdb_put_packet(remcomOutBuffer);

			break;

		case 'H':
			/* Just ack */
			strcpy(remcomOutBuffer, "OK");
    			kgdb_put_packet(remcomOutBuffer);
			break;

		/* 
		 * Kill...no sense in kernel...so we just ack and 
		 * leave
		 */
		case 'k':
			gdb_connected = 0;
			return;	

		/* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
		case 'm' :
			read_mem_msg();
			restore_flags(flags);
	          	break;

		/* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
		case 'M' :
			write_mem_msg(0);
                	break;
     
		/* 
		 * sAA..AA    Single step at address AA..AA(optional) 
		 * 
		 */ 
		case 's':
			addr = get_next_pc(regs);

			kgdb_debug("Current PC: %#010x Next PC: %#010x\n",
				regs->ARM_pc, addr);

			saved_instruction = *((unsigned long *)addr);
			saved_addr = (unsigned int*)addr;

			*((unsigned long*)addr) = KGDB_BREAKINST;

			strcpy(remcomOutBuffer, "OK");
			kgdb_put_packet(remcomOutBuffer);

			cpu_cache_clean_invalidate_all();

			restore_flags(flags);
			return;

		/* cAA..AA    Continue at address AA..AA(optional) */ 
		case 'c':
			ptr = &remcomInBuffer[1];

			if (hexToInt(&ptr,&addr))
				regs->ARM_pc = addr;

			strcpy(remcomOutBuffer, "OK");
			kgdb_put_packet(remcomOutBuffer);
    
			cpu_cache_clean_invalidate_all();

			restore_flags(flags);
			return;
		default:
    			kgdb_put_packet(remcomOutBuffer);
		}
    	}
}

/*
 * TODO: If remote GDB disconnects from us, we need to return
 * 0 as we're no longer active.  Does GDB send us a disconnect
 * message??
 */
extern int panic_timeout;

/* 
 * In agreement with the kernel panic function
 * zero timeout->no reboot->kgdb
 */
int 
kgdb_active(void)
{
	return panic_timeout == 0;
}

int kgdb_connected(void)
{
	return gdb_connected;
}


/*
 * Code to determine next PC based on current PC address.
 * Taken from GDB source code.  Open Source is good. :)
 */
#define read_register(x) regs->uregs[x]

#define addr_bits_remove(x) (x & 0xfffffffc)

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((unsigned) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))


/* Instruction condition field values.  */
#define INST_EQ         0x0
#define INST_NE         0x1
#define INST_CS         0x2
#define INST_CC         0x3
#define INST_MI         0x4
#define INST_PL         0x5
#define INST_VS         0x6
#define INST_VC         0x7
#define INST_HI         0x8
#define INST_LS         0x9
#define INST_GE         0xa
#define INST_LT         0xb
#define INST_GT         0xc
#define INST_LE         0xd
#define INST_AL         0xe
#define INST_NV         0xf

#define FLAG_N          0x80000000
#define FLAG_Z          0x40000000
#define FLAG_C          0x20000000
#define FLAG_V          0x10000000

#define error(x)	

static unsigned long
shifted_reg_val (unsigned long inst, int carry, unsigned long pc_val,
		 unsigned long status_reg, struct pt_regs* regs)
{
  unsigned long res = 0, shift = 0;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);

  if (bit (inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);

  res = (rm == 15
	 ? ((pc_val | (1 ? 0 : status_reg))
	    + (bit (inst, 4) ? 12 : 8)) : read_register (rm));

  switch (shifttype)
    {
    case 0:			/* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;

    case 1:			/* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2:			/* ASR */
      if (shift >= 32)
	shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3:			/* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32 - shift));
      break;
    }

  return res & 0xffffffff;
}

/* Return number of 1-bits in VAL.  */

static int
bitcount (unsigned long val)
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;		/* delete rightmost 1-bit in val */
  return nbits;
}

static int
condition_true (unsigned long cond, unsigned long status_reg)
{
	if (cond == INST_AL || cond == INST_NV)
		return 1;

	switch (cond)
	{
		case INST_EQ:
		  return ((status_reg & FLAG_Z) != 0);
		case INST_NE:
		  return ((status_reg & FLAG_Z) == 0);
		case INST_CS:
		  return ((status_reg & FLAG_C) != 0);
		case INST_CC:
		  return ((status_reg & FLAG_C) == 0);
		case INST_MI:
		  return ((status_reg & FLAG_N) != 0);
		case INST_PL:
		  return ((status_reg & FLAG_N) == 0);
		case INST_VS:
		  return ((status_reg & FLAG_V) != 0);
		case INST_VC:
		  return ((status_reg & FLAG_V) == 0);
		case INST_HI:
		  return ((status_reg & (FLAG_C | FLAG_Z)) == FLAG_C);
		case INST_LS:
		  return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
		case INST_GE:
		  return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
		case INST_LT:
		  return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
		case INST_GT:
		  return (((status_reg & FLAG_Z) == 0) &&
					(((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
		case INST_LE:
		  return (((status_reg & FLAG_Z) != 0) ||
					(((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
	} 
	return 1;
}

unsigned long
get_next_pc(struct pt_regs *regs)
{
  unsigned long pc_val;
  unsigned long this_instr;
  unsigned long status;
  unsigned long nextpc;

  pc_val = regs->ARM_pc;
  this_instr = *((unsigned long *)regs->ARM_pc);
  status = regs->ARM_cpsr;
  nextpc = pc_val + 4;	/* Default case */

  if (condition_true (bits (this_instr, 28, 31), status))
  { 
	  switch (bits (this_instr, 24, 27))
	  {
		case 0x0:
		case 0x1:		/* data processing */
		case 0x2:
		case 0x3:
		{
			unsigned long operand1, operand2, result = 0;
		 	unsigned long rn;
			int c;

			if (bits (this_instr, 12, 15) != 15)
				break;

			if (bits (this_instr, 22, 25) == 0
					&& bits (this_instr, 4, 7) == 9)
				/* multiply */
				error ("Illegal update to pc in instruction");

			/* Multiply into PC */
			c = (status & FLAG_C) ? 1 : 0;
			rn = bits (this_instr, 16, 19);
			operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);

			if (bit (this_instr, 25))
			{
				unsigned long immval = bits (this_instr, 0, 7);
				unsigned long rotate = 2 * bits (this_instr, 8, 11);
				operand2 = ((immval >> rotate) | (immval << (32 - rotate)))
									& 0xffffffff;
			}
			else		/* operand 2 is a shifted register */
				operand2 = shifted_reg_val (this_instr, c, pc_val, status, regs);

			switch (bits (this_instr, 21, 24))
			{
				case 0x0:	/*and */
					result = operand1 & operand2; 
					break;

				case 0x1:	/*eor */
					result = operand1 ^ operand2; 
					break;

				case 0x2:	/*sub */
					result = operand1 - operand2;
					break;

				case 0x3:	/*rsb */
					result = operand2 - operand1;
					break;

				case 0x4:	/*add */
					result = operand1 + operand2;
					break;

				case 0x5:	/*adc */
					result = operand1 + operand2 + c;
					break;

				case 0x6:	/*sbc */
					result = operand1 - operand2 + c;
					break;

				case 0x7:	/*rsc */
					result = operand2 - operand1 + c;
					break;

				case 0x8:
				case 0x9:
				case 0xa:
				case 0xb:	/* tst, teq, cmp, cmn */
					result = (unsigned long) nextpc;
					break;

				case 0xc:	/*orr */
					result = operand1 | operand2;
					break;

				case 0xd:	/*mov */
					/* Always step into a function.  */
					result = operand2;
					break;

				case 0xe:	/*bic */
					result = operand1 & ~operand2;
					break;

				case 0xf:	/*mvn */
					result = ~operand2;
					break;
			}
			nextpc = addr_bits_remove(result);

			break;
	  }

		case 0x4:
		case 0x5:		/* data transfer */
		case 0x6:
		case 0x7:
		if (bit (this_instr, 20))
		{
			/* load */
			if (bits (this_instr, 12, 15) == 15)
			{
				/* rd == pc */
				unsigned long rn;
				unsigned long base;

				if (bit (this_instr, 22))
					error ("Illegal update to pc in instruction");

				/* byte write to PC */
				rn = bits (this_instr, 16, 19);
				base = (rn == 15) ? pc_val + 8 : read_register (rn);
				if (bit (this_instr, 24))
				{
					/* pre-indexed */
					int c = (status & FLAG_C) ? 1 : 0;
					unsigned long offset =
							  (bit (this_instr, 25)
								? shifted_reg_val (this_instr, c, pc_val, status, regs)
								: bits (this_instr, 0, 11));

					if (bit (this_instr, 23))
						base += offset;
					else
						base -= offset;
				}
				nextpc = *((unsigned long *) base);
		  
				nextpc = addr_bits_remove (nextpc);
		  
				if (nextpc == regs->ARM_pc) 
						  error ("Infinite loop detected");
			}
		}
		break;

		case 0x8:
		case 0x9:		/* block transfer */
		if (bit (this_instr, 20))
		{
			/* LDM */
			if (bit (this_instr, 15))
			{
				/* loading pc */
				int offset = 0;

				if (bit (this_instr, 23)) 
				{
					/* up */
					unsigned long reglist = bits (this_instr, 0, 14);
					offset = bitcount (reglist) * 4;
					if (bit (this_instr, 24))		/* pre */
						offset += 4;
				}
				else if (bit (this_instr, 24))
					offset = - 4;

				{
					unsigned long rn_val =
						read_register (bits (this_instr, 16, 19));
					nextpc = *((unsigned int *) (rn_val + offset)); 
				}
				
				nextpc = addr_bits_remove (nextpc);
				if (nextpc == regs->ARM_pc)
						  error ("Infinite loop detected");
			}
		}
		break;

		case 0xb:		/* branch & link */
		case 0xa:		/* branch */
		{
			nextpc = BranchDest (regs->ARM_pc, this_instr);
			nextpc = addr_bits_remove (nextpc);

			if (nextpc == regs->ARM_pc)
				error ("Infinite loop detected");
			
			break;
		}

		case 0xc:
		case 0xd:
		case 0xe:		/* coproc ops */
		case 0xf:		/* SWI */
			break;

		default:
			error("Bad bit-field extraction");
			return (regs->ARM_pc);
	  }
  } 
  
  return nextpc;
}


