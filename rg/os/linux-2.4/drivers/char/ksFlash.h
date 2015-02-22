#ifndef KSFLASH_H
#define KSFLASH_H

#include <linux/delay.h>

typedef enum { 
        FL_UNKNOWN,
	FL_READY,
	FL_STATUS,
	FL_CFI_QUERY,
	FL_JEDEC_QUERY,
	FL_ERASING,
	FL_ERASE_SUSPENDING,
	FL_ERASE_SUSPENDED,
	FL_WRITING,
	FL_WRITING_TO_BUFFER,
	FL_WRITE_SUSPENDING,
	FL_WRITE_SUSPENDED,
	FL_PM_SUSPENDED,
	FL_SYNCING,
	FL_UNLOADING,
	FL_LOCKING,
	FL_UNLOCKING,
} flash_state;

static __u8 armflash_read8(unsigned long addr)
{
	return readb(addr);
}

static __u16 armflash_read16(unsigned long addr)
{
	return readw(addr);
}

static __u32 armflash_read32(unsigned long addr)
{
	return readl(addr);
}

static void armflash_write8(__u8 d, unsigned long adr)
{
	writeb(d, adr);
}

static void armflash_write16(__u16 d, unsigned long adr)
{
	writew(d, adr);
}

static void armflash_write32(__u32 d, unsigned long adr)
{
	writel(d, adr);
}

static inline __u8 cfi_read_query(__u32 addr)
{
   if (cfi_buswidth_is_1())
       return armflash_read8(addr);
   else if (cfi_buswidth_is_2()) 
       return (armflash_read16(addr));
   else if (cfi_buswidth_is_4()) 
       return (armflash_read32(addr));
   else 
       return 0;
}

static inline void cfi_write(__u32 val, __u32 addr)
{
    if (cfi_buswidth_is_1()) 
	armflash_write8(val, addr);
    else if (cfi_buswidth_is_2()) 
	armflash_write16(val, addr);
    else if (cfi_buswidth_is_4()) 
	armflash_write32(val, addr);
}

static inline __u32 cfi_read(__u32 addr)
{
   if (cfi_buswidth_is_1()) 
	return armflash_read8(addr);
   else if (cfi_buswidth_is_2()) 
	return armflash_read16(addr);
   else if (cfi_buswidth_is_4()) 
	return armflash_read32(addr);
   else 
	return 0;
}

/*
 * Returns the command address according to the given geometry.
 */
static inline __u32 cfi_build_cmd_addr(__u32 cmd_ofs, int interleave, int type)
{
   return (cmd_ofs * type) * interleave;
}


/*
 * Transforms the CFI command for the given geometry (bus width & interleave.
 */
static inline __u32 cfi_build_cmd(u_char cmd)
{
   __u32 val = 0;

   if (cfi_buswidth_is_1()) 
   {
	/* 1 x8 device */
	val = cmd;
   } else if (cfi_buswidth_is_2()) 
   {
      if (cfi_interleave_is_1())
      {
	/* 1 x16 device in x16 mode */
	val = cmd;
      } 
      else if (cfi_interleave_is_2()) 
      {
	/* 2 (x8, x16 or x32) devices in x8 mode */
        val = (cmd << 8) | cmd;
      }
    }
    else if (cfi_buswidth_is_4())
    {
	if (cfi_interleave_is_1())
       	{
  	   /* 1 x32 device in x32 mode */
	   val = cmd;
	} 
	else if (cfi_interleave_is_2()) 
	{
	    /* 2 x16 device in x16 mode */
	    val = (cmd << 16) | cmd;
	}
       	else if (cfi_interleave_is_4()) 
	{
	   /* 4 (x8, x16 or x32) devices in x8 mode */
	   val = (cmd << 16) | cmd;
	   val = (val << 8) | val;
	}
    }
    return val;
}

static inline __u32 cfi_send_gen_cmd(u_char cmd, __u32 cmd_addr, __u32 base,
  				     int type, int interleave)
{
	__u32 val;
	__u32 addr = base + cfi_build_cmd_addr(cmd_addr, interleave, type);

	val = cfi_build_cmd(cmd);
	cfi_write(val, addr);
	return addr - base;
}

static inline void cfi_udelay(int us)
{
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	if (current->need_resched) {
		unsigned long t = us * HZ / 1000000;
		if (t < 1)
			t = 1;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(t);
	}
	else
//#endif
        udelay(us);
}

#endif

