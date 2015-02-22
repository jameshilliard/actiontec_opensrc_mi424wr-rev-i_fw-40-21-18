#include <linux/interrupt.h>
#include <asm/mipsregs.h>

#define OP_MASK         0x3f
#define OP_SHIFT        26
#define R_MASK          0x1f
#define RS_SHIFT        21
#define RT_SHIFT        16
#define RD_SHIFT        11
#define OFFSET_MASK     0xffff

#define _OP_(x)         (OP_MASK & ((x) >> OP_SHIFT))
#define _OPS_(x)        (OP_MASK & (x))
#define _RS_(x)         (R_MASK & ((x) >> RS_SHIFT))
#define _RT_(x)         (R_MASK & ((x) >> RT_SHIFT))
#define _RD_(x)         (R_MASK & ((x) >> RD_SHIFT))
#define _OFFSET_(x)     (OFFSET_MASK & (x))

#ifdef CONFIG_LX4189_INSTRUMENT
static inline struct LX4189_Stats lx4189Stats;
#endif

extern void simulate_ll(struct pt_regs *regs, unsigned int opcode);
extern void simulate_sc(struct pt_regs *regs, unsigned int opcode);

typedef enum {
    BRANCH_T_NONE,
    BRANCH_T_OFFSET,
    BRANCH_T_TARGET,
    BRANCH_T_REGISTER
} Branch_t;

/* compute a * b */
static inline unsigned long __mulsi3( unsigned long a, unsigned long b )
{
    unsigned long result = 0;
#ifdef CONFIG_LX4189_INSTRUMENT
    lx4189Stats.__mulsi3_calls++;
#endif

    while ( 0 != a )
    {
        result += ( 0x01 & a ) ? b : 0;
        a = a >> 1;
        b = b << 1;
    }

    return result;
}

#if 0
static inline long long __muldi3( unsigned long long u, unsigned long long v )
{
    unsigned long long mask = 0x0000000000000001;
    unsigned long long r = 0;
    unsigned long      i;

#ifdef CONFIG_LX4189_INSTRUMENT
    lx4189Stats.__muldi3_calls++;
#endif
    
    /* This isn't the smartest approach, but it's functional.
       It's worth noting that the main user of this code appears to be
       NFS, so it shouldn't hurt us too much */
    for ( i = 0; i < 64; i++ )
    {
	if ( u & mask )
	    r += v;
	mask = mask << 1;
	v = v << 1;
    }

    return r;
}
#endif

static inline long get_mult_hi( long a, long b, long low )
{    
    unsigned int X = (unsigned int) a;
    unsigned int Y = (unsigned int) b;
    unsigned int i;
    unsigned int Mask = 0x80000000;
    unsigned int sign = (Mask & X) ^ (Mask & Y);
    unsigned int HI = 0;
    unsigned int CARRY;

    X = Mask&X ? -X : X;
    
    for (i=0; i<32; i++)
    {
	CARRY = 0;
	if ( low & 1 )
	{
	    CARRY = HI;
	    HI += X;
	    CARRY = (HI < CARRY) | (HI < X);
	}
	low >>= 1;
	HI >>= 1;
	if (CARRY)
	{
	    HI |= Mask;
	}
    }
    
    if (sign)
    {
	low = ~low;
	HI = ~HI;
	CARRY = low;
	low += 1;
	
	if ( low < CARRY)
	{
	    HI += 1;
	}
    }
    
    return HI;
}

static inline long get_umult_hi( long a, long b, long low )
{
    register unsigned int X = (unsigned int)a;
    register unsigned int i;
    register unsigned int Mask = 0x80000000;
    register unsigned int HI = 0;
    register unsigned int LO = (unsigned int)b;
    register unsigned int CARRY;

    for (i=0; i<32; i++)
    {
	CARRY = 0;
	if (LO&1)
	{
	    CARRY = HI;
	    HI += X;
	    CARRY = (HI < CARRY) | (HI < X);
	}
	LO >>= 1;
	if (HI&1)
	{
	    LO |= Mask;
	}
	HI >>= 1;
	if (CARRY)
	{
	    HI |= Mask;
	}
    }
    
    return HI;
}

/* compute a / b */
static inline unsigned long __udivsi3( unsigned long a, unsigned long b )
{
  unsigned long HI;
  unsigned long LO;
  unsigned long i;
  unsigned long Mask = 0x80000000;

#ifdef CONFIG_LX4189_INSTRUMENT
  lx4189stats.__udivsi3_calls++;
#endif

  HI = 0;
  LO = a;

  for (i=0; i<32; i++)
    {
      HI <<= 1;
      
      if (LO&Mask)
	HI |= 1;
      LO <<= 1;
      
      if ( b > HI)
	{
	  LO &= ~1;
	}
      else
	{
	  /* HI == Remainder */
	  HI -= b;
	  /* LO == Quotient */
	  LO |= 1;
	}
    }

    return LO;
}

/* compute a / b */
static inline long __divsi3( long a, long b )
{
  register unsigned int X = (unsigned int) a;
  register unsigned int Y = (unsigned int) b;
  register unsigned int i;
  register unsigned int Mask = 0x80000000;
  register unsigned int signHI = (Mask & X);
  register unsigned int sign = signHI ^ (Mask & Y);
  register unsigned int HI = 0;
  register unsigned int LO = Mask&X ? -X : X;
  
#ifdef CONFIG_LX4189_INSTRUMENT
  lx4189stats.__divsi3_calls++;
#endif

  Y = Mask&Y ? -Y : Y;
  
  for (i=0; i<32; i++)
    {
      HI <<= 1;
      if (LO&Mask)
	HI |= 1;
      LO <<= 1;
      
      if (Y > HI)
	{
	  LO &= ~1;
	}
      else
	{
	  HI -= Y;
	  LO |= 1;
	}
    }
  LO = sign ? -LO : LO;
  HI = signHI ? -HI : HI;
  
  return LO;
}

/* compute a % b */
static inline unsigned long __umodsi3( unsigned long a, unsigned long b )
{
  unsigned long HI;
  unsigned long LO;
  unsigned long i;
  unsigned long Mask = 0x80000000;

#ifdef CONFIG_LX4189_INSTRUMENT
  lx4189Stats.__umodsi3_calls++;
#endif

  HI = 0;
  LO = a;

  for (i=0; i<32; i++)
    {
      HI <<= 1;
      
      if (LO&Mask)
	HI |= 1;
      LO <<= 1;
      
      if ( b > HI)
	{
	  LO &= ~1;
	}
      else
	{
	  /* HI == Remainder */
	  HI -= b;
	  /* LO == Quotient */
	  LO |= 1;
	}
    }

    return HI;
}

/* compute a % b */
static inline long __modsi3( long a, long b )
{
  register unsigned int X = (unsigned int) a;
  register unsigned int Y = (unsigned int) b;
  register unsigned int i;
  register unsigned int Mask = 0x80000000;
  register unsigned int signHI = (Mask & X);
  register unsigned int sign = signHI ^ (Mask & Y);
  register unsigned int HI = 0;
  register unsigned int LO = Mask&X ? -X : X;
  
#ifdef CONFIG_LX4189_INSTRUMENT
  lx4189Stats.__modsi3_calls++;
#endif

  Y = Mask&Y ? -Y : Y;
  
  for (i=0; i<32; i++)
    {
      HI <<= 1;
      if (LO&Mask)
	HI |= 1;
      LO <<= 1;
      
      if (Y > HI)
	{
	  LO &= ~1;
	}
      else
	{
	  HI -= Y;
	  LO |= 1;
	}
    }
  LO = sign ? -LO : LO;
  HI = signHI ? -HI : HI;
  
  return HI;
}

static inline int isJumpInstruction( unsigned long epc )
{
    unsigned long inst;

    inst = *((unsigned long *)epc);

    switch (_OP_(inst))
    {
    case 0x00:                  /* Special */
        if ( (_OPS_(inst) == 0x08) ||        /* jr */
                (_OPS_(inst) == 0x09) )
        { /* jalr */
            return 1;
        }
        return 0;

    case 0x02:                  /* j */
    case 0x03:                  /* jal */
        return 1;

    default:
        return 0;
    }
}

static inline Branch_t emulateBranch( struct pt_regs *regs )
{
    register unsigned long rs;
    register unsigned long rt;
    unsigned int *pc;
    unsigned int inst;
    Branch_t branchStatus;

    pc = (unsigned int *)( regs->cp0_epc );

    inst = *pc;

    rs = regs->regs[_RS_(inst)];
    rt = regs->regs[_RT_(inst)];

    branchStatus = BRANCH_T_NONE;
    switch (_OP_(inst))
    {
    case 0x00:                  /* Special */
        if (_OPS_(inst) == 0x08)
        {       /* jr */
            branchStatus = BRANCH_T_REGISTER;
        }
        if (_OPS_(inst) == 0x09)
        {       /* jalr */
            regs->regs[_RD_(inst)] = (unsigned long) (pc + 2);
            branchStatus = BRANCH_T_REGISTER;
        }
        break;

    case 0x03:                  /* jal */
        regs->regs[31] = (unsigned long) (pc + 2);
    case 0x02:                  /* j */
        branchStatus = BRANCH_T_TARGET;
        break;

    case 0x04:                  /* beq */
        if (rs == rt) branchStatus = BRANCH_T_OFFSET;
        break;
    case 0x05:                  /* bne */
        if (rs != rt) branchStatus = BRANCH_T_OFFSET;
        break;

    case 0x06:                  /* blez */
        if ((signed long) rs <= (signed long) 0)
            branchStatus = BRANCH_T_OFFSET;
        break;

    case 0x07:                  /* bgtz */
        if ((signed long) rs > (signed long) 0)
            branchStatus = BRANCH_T_OFFSET;
        break;

    case 0x01:                  /* regimm */
        switch(_RT_(inst))
        {
        case 0x10:                      /* bltzal */
            regs->regs[31] = (unsigned long) (pc + 2);
        case 0x00:                      /* bltz */
            if ((signed long) rs < (signed long) 0)
                branchStatus = BRANCH_T_OFFSET;
            break;

        case 0x11:                      /* bgezal */
            regs->regs[31] = (unsigned long) (pc + 2);
        case 0x01:                      /* bgez */
            if ((signed long) rs >= (signed long) 0)
                branchStatus = BRANCH_T_OFFSET;
            break;
        }
        break;

        /* Todo: bcxf and bcxt */

    case 0x10:                  /* cop0 */
        branchStatus = BRANCH_T_OFFSET;
        break;

    case 0x11:                  /* cop1 */
        branchStatus = BRANCH_T_OFFSET;
        break;

    case 0x12:                  /* cop2 */
        branchStatus = BRANCH_T_OFFSET;
        break;

    case 0x13:                  /* cop3 */
        branchStatus = BRANCH_T_OFFSET;
        break;
    }

    return branchStatus;
}

static inline unsigned int *computeBranchAddress ( struct pt_regs *regs,
        Branch_t type)
{
    register unsigned long rs;
    unsigned int *pc;
    unsigned int inst;
    signed int offset;

    pc = (unsigned int *)(regs->cp0_epc);

    inst = *pc;

    switch (type)
    {
    case BRANCH_T_OFFSET:
        offset = (signed short) (inst & 0xffff);
        return (pc + 1 + offset);
    case BRANCH_T_TARGET:
        offset = inst & 0x3ffffff;
        return (unsigned int *)(((unsigned long)(pc + 1) & 0xf0000000) | (offset << 2));

    case BRANCH_T_REGISTER:
        rs = regs->regs[_RS_(inst)];
        return (unsigned int *)rs;

    case BRANCH_T_NONE:
        return (pc + 2);

    default:
        return (0);
    }
}

void do_ri_lx4189( struct pt_regs *regs )
{
    register unsigned long rs;
    register unsigned long rt;
    unsigned long va;
    unsigned long mem;
    unsigned long newPC = 0;
    unsigned long inst;
    unsigned int byte;
    int status;
    int branchDelay;

    /*
      =========================================
      If this exception occurred in a branch
      delay slot (Cause(BD)) then the victim
      instruction is at EPC + 4:

      If it occurred in a jump (j, jr, jalr,
      or jal), then the instruction location
      must be adjusted according to the branch
      type (offset, target, register, or none):
      =========================================
    */

    branchDelay = ( ( 0 != ( regs->cp0_cause & CAUSEF_BD ) ) ||
                    ( isJumpInstruction( regs->cp0_epc ) ) );

    if (branchDelay)
    {
        newPC =
            (unsigned long) computeBranchAddress ( regs,
                                                   emulateBranch( regs ));
    }

    /*
      =====================================
      Get the instruction that caused this
      interrupt:
      =====================================
    */
    inst = *((unsigned long *)(regs->cp0_epc) + ( branchDelay ? 1 : 0 ));

    /* In case the emulated zero register is somehow trashed. */
    regs->regs[ 0 ] = 0;

    /*
      =================================
      Isolate the two source registers:
      =================================
    */
    rs = regs->regs[ _RS_(inst) ];
    rt = regs->regs[ _RT_(inst) ];

    /*
      =======================================
      Calculate the offset and alignment
      for lwl, lwr, swl, or swr instructions.
      For these instructions, 'rs' represents
      the base to which the offset is added:
      =======================================
    */
    va = rs + (unsigned long)((short)_OFFSET_(inst));
    byte = va & 3;


    status = 0;

    /*
    =============================================================
    Three types of instructions deserve special consideration
    in the Lexra ESB:

    lwl, lwr, swl, and swr are unaligned load and store inst-
    ructions.  These four instructions are _always_ implement-
    ed in software.

    mult, multu, div, and divu instructions may be either soft-
    ware emulated or placed in the optional MAC-DIV module.
    When these instructions are implemented in the MAC-DIC module
    they do not generate Reserved Instruction (RI) traps.

    The mthi, mtlo, mfhi, and mflo instructions are implemented
    in hardware, no software emulation is necessary, EXCEPT for
    "Pass Zero" Test Chips.  The "Pass Zero" Test Chips require
    software emulation for these instructions.

    The 12 instructions described above are decoded by the foll-
    owing switch statement.  They may be uniquely identified by
    bits 26 through 31 (the primary opcode field) and bits 0
    through 5 (the subcode field).
    =============================================================
    */
    switch (_OP_(inst))
    {
        /*
	  ================================================================
	  Load Word Left:  lwl rt, offset + rs
	  Add the sign-extended offset to base register 'rs'; this is the
	  source address.  Copy the byte at this address to the leftmost
	  unwritten byte in 'rt', proceeding from left to right.
	  
	  When the rightmost byte of the source is copied, the operation
	  is complete.
	  ================================================================
	*/
    case 0x22:                  /* lwl */
#ifdef CONFIG_LX4189_INSTRUMENT
        lx4189Stats.lwl_instructions++;
#endif
        mem = *(unsigned long *)(va - byte);
        mem = mem << byte*8;

        rt = (rt & ~(-1UL << byte*8)) | mem;

        regs->regs[_RT_(inst)] = rt;
        status = 1;
        break;

        /*
          ================================================================
          Load Word Right:  lwr rt, offset + rs
          Add the sign-extended offset to base register 'rs'; this is 
          the source address.  Copy the byte at this address to the 
          rightmost unwritten byte in 'rt', proceeding from right to left.

          When the leftmost byte of the source is copied, the operation 
          is complete.
          ================================================================
        */
    case 0x26:                  /* lwr */
#ifdef CONFIG_LX4189_INSTRUMENT
        lx4189Stats.lwr_instructions++;
#endif
        mem = *(unsigned long *)(va - byte);
        mem = mem >> (3-byte)*8;

        rt = (rt & ~(-1UL >> (3-byte)*8)) | mem;

        regs->regs[_RT_(inst)] = rt;
        status = 1;
        break;

        /*
          ================================================================
          Store Word Left:  swl rt, offset + rs

          Add the sign-extended offset to base register 'rs'; this is the
          destination address.  Proceeding from left to right, copy bytes
          from the register specified by 'rt' to bytes starting at the
          destination address.

          When the rightmost byte of the destination is written, the 
          operation is complete.
          ================================================================
        */
    case 0x2A:                  /* swl */
#ifdef CONFIG_LX4189_INSTRUMENT
        lx4189Stats.swl_instructions++;
#endif
        mem = *(unsigned long *)(va - byte);
        mem = mem & ~(-1UL >> byte*8);

        rt = (rt >> byte*8) | mem;

        *(unsigned long *)(va - byte) = rt;
        status = 1;
        break;

        /*
          ================================================================
          Store Word Right:  swr rt, offset + rs

          Add the sign-extended offset to base register 'rs'; this is the
          destination address.  Proceeding from right to left, copy bytes
          from the register specified by 'rt' to bytes starting at the
          destination address.

          When the leftmost byte of the destination is written, the
          operation is complete.
          ================================================================
        */
    case 0x2E:                  /* swr */
#ifdef CONFIG_LX4189_INSTRUMENT
        lx4189Stats.swr_instructions++;
#endif
        mem = *(unsigned long *)(va - byte);
        mem = mem & ~(-1UL << (3-byte)*8);

        rt = (rt << (3-byte)*8) | mem;

        *(unsigned long *)(va - byte) = rt;
        status = 1;
        break;
    case 0x00:                  /* Special */

        switch (_OPS_(inst))
        {

            /*
                ===================================
                The move from HI-result instruction
                must be emulated in software for
                the Pass Zero Test Chip only.
                Other LX4xxx devices implement mfhi
                in hardware.
                ===================================
            */
        case 0x10:                      /* mfhi */
#ifdef CONFIG_LX4189_INSTRUMENT
            lx4189Stats.mfhi_instructions++;
#endif
            regs->regs[_RD_(inst)] = regs->hi;

            status = 1;
            break;
            /*
                ===================================
                The move to HI-result instruction
                must be emulated in software for
                the Pass Zero Test Chip only.
                Other LX4xxx devices implement mthi
                in hardware.
                ===================================
            */
        case 0x11:                      /* mthi */
#ifdef CONFIG_LX4189_INSTRUMENT
            lx4189Stats.mthi_instructions++;
#endif
            regs->hi = regs->regs[_RS_(inst) ];

            status = 1;
            break;
            /*
                ===================================
                The move from LO-result instruction
                must be emulated in software for
                the Pass Zero Test Chip only.
                Other LX4xxx devices implement mflo
                in hardware.
                ===================================
            */
        case 0x12:                      /* mflo */
#ifdef CONFIG_LX4189_INSTRUMENT
            lx4189Stats.mflo_instructions++;
#endif
            regs->regs[_RD_(inst)] = regs->lo;

            status = 1;
            break;
            /*
                ===================================
                The move to LO-result instruction
                must be emulated in software for
                the Pass Zero Test Chip only.
                Other LX4xxx devices implement mtlo
                in hardware.
                ===================================
            */
        case 0x13:                      /* mtlo */
#ifdef CONFIG_LX4189_INSTRUMENT
            lx4189Stats.mtlo_instructions++;
#endif
            regs->lo = regs->regs[_RS_(inst) ];

            status = 1;
            break;
            /*
                ===================================
                The signed multiply instruction
                may be emulated in software or
                implemented in the optional MAC-DIV
                Module.
                ===================================
            */
        case 0x18:                      /* mult */
#ifdef CONFIG_LX4189_INSTRUMENT
	    lx4189Stats.mult_instructions++;
#endif
	    regs->lo = __mulsi3( rt, rs );
	    regs->hi = get_mult_hi( rt, rs, 0 );

            status = 1;
            break;
            /*
                ===================================
                The unsigned multiply instruction
                may be emulated in software or
                implemented in the optional MAC-DIV
                Module.
                ===================================
            */
        case 0x19:                      /* multu */
#ifdef CONFIG_LX4189_INSTRUMENT
	    lx4189Stats.multu_instructions++;
#endif
	    regs->hi = get_umult_hi( rs, rt, 0 );
	    regs->lo = __mulsi3( rs, rt );

            status = 1;
            break;
        case 0x1a:                      /* div */
            
	    /* If the LX4189 was incorporated without the optional MAC 
	       unit, we have to simulate this instruction in software. 
	       This is insanely painful. Please consider algorithms that 
	       don't need divides */
#ifdef CONFIG_LX4189_INSTRUMENT
	    lx4189Stats.div_instructions++;
#endif
	    regs->hi = __modsi3( rs, rt );
	    regs->lo = __divsi3( rs, rt );

            status = 1;
            break;

        case 0x1b:                      /* divu */
	    /* If the LX4189 was incorporated without the optional MAC 
	       unit, we have to simulate this instruction in software. 
	       This is insanely painful. Please consider algorithms that 
	       don't need divides */
#ifdef CONFIG_LX4189_INSTRUMENT
            lx4189Stats.divu_instructions++;
#endif
            regs->lo = __udivsi3( rs, rt );
            regs->hi = __umodsi3( rs, rt );

            status = 1;
            break;
        default:                        /* special */
            printk("unknown instruction executed at %08lx\n",
                   (regs->cp0_epc) + ( branchDelay ? 4 : 0 ) );
        }
        break;
    case 0x30:
		/* LL */
		simulate_ll(regs, inst);
		status = 0;
		break;
    case 0x38:
		/* SC */
		simulate_sc(regs, inst);
		status = 0;
		break;
    default:
        printk("unknown instruction executed at %08lx\n",
               (regs->cp0_epc) + ( branchDelay ? 4 : 0 ) );
    }

    if (status)
    {
        if (branchDelay)
        {
            regs->cp0_epc = newPC;
        }
        else
        {
            regs->cp0_epc += 4;
        }
    }
}

