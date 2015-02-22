/*
 * c-lx4189.c: Lexra LX4189 Cache routines
 * Copyright (C) 2005 Analog Devices
 *
 * with a lot of changes to make this thing work for R3000s
 * Tx39XX R4k style caches added. HK
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1998, 1999, 2000 Harald Koerfgen
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 * Copyright (C) 2001  Maciej W. Rozycki
 */
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/system.h>
#include <asm/isadep.h>
#include <asm/io.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>

static unsigned long icache_size, dcache_size;		/* Size in bytes */
static unsigned long icache_lsize, dcache_lsize;	/* Size in bytes */

#undef DEBUG_CACHE
#define C0_CCTL $20
#define C0_IvIC 0x02
#define C0_IvDC 0x01

void wbflush(void)
{
	int tmp;
	tmp = *((int*)0xa0000000);
}

static void __init lx4189_probe_cache(void)
{
	dcache_size = 8 * 1024;
	dcache_lsize = 32;

	icache_size = 16 * 1024;
	icache_lsize = 32;
}

static void lx4189_flush_icache_range(unsigned long start, unsigned long end)
{
	/* We don't get the luxury of flushing part of the cache at a time.
	 * However, we do get the luxury of a hardware cache flush routine.
	 * So what we do is read from C0_CCTL, set C0_IvIC, write back to
	 * C0_CCTL, and then wait for the CPU to work it's magic.
	 */
	unsigned long temp;

	temp = read_c0_xcontext();
	temp |= C0_IvIC;
	write_c0_xcontext(temp);
	for ( temp = 0; temp < 20; temp++ );

	/* The flush only happens on a 0->1 transition. And C0_IvIC doesn't
	 * clear when the invalidation is done. So, we have to be sure that 
	 * the bit has been cleared.
	 */
	temp = read_c0_xcontext();
	temp |= C0_IvIC;
	temp ^= C0_IvIC;
	write_c0_xcontext(temp);

	return;
}

static void lx4189_flush_dcache_range(unsigned long start, unsigned long end)
{
	/* We don't get the luxury of flushing part of the cache at a time.
	 * However, we do get the luxury of a hardware cache flush routine.
	 * So what we do is read from C0_CCTL, set C0_IvDC, write back to
	 * C0_CCTL, and then wait for the CPU to work it's magic.
	 */
	unsigned long temp;

	temp = read_c0_xcontext();
	temp |= C0_IvDC;
	write_c0_xcontext(temp);
	for ( temp = 0; temp < 20; temp++ );

	/* The flush only happens on a 0->1 transition. And C0_IvIC doesn't
	 * clear when the invalidation is done. So, we have to be sure that 
	 * the bit has been cleared.
	 */
	temp = read_c0_xcontext();
	temp |= C0_IvDC;
	temp ^= C0_IvDC;
	write_c0_xcontext(temp);

	return;
}

static inline unsigned long get_phys_page (unsigned long addr,
					   struct mm_struct *mm)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long physpage;

	pgd = pgd_offset(mm, addr);
	pmd = pmd_offset(pgd, addr);
	pte = pte_offset(pmd, addr);

	if ((physpage = pte_val(*pte)) & _PAGE_VALID)
		return KSEG0ADDR(physpage & PAGE_MASK);

	return 0;
}

static inline void lx4189_flush_cache_all(void)
{
}

static inline void lx4189___flush_cache_all(void)
{
	/* Data cache is write-through cache, no need to flush it. */
	lx4189_flush_icache_range(KSEG0, KSEG0 + icache_size);
}

static void lx4189_flush_cache_mm(struct mm_struct *mm)
{
	if (mm->context)
		lx4189___flush_cache_all();
}

static void lx4189_flush_cache_range(struct vm_area_struct *vma, unsigned long start,
				  unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;
	if (mm->context)
		lx4189_flush_icache_range(0, 0);
}

static void lx4189_flush_cache_page(struct vm_area_struct *vma,
				   unsigned long page,unsigned long pfn)
{
	struct mm_struct *mm = vma->vm_mm;

	if (mm->context && (vma->vm_flags & VM_EXEC) &&
				get_phys_page(page, vma->vm_mm))
		lx4189_flush_icache_range(0, 0);
}

static void lx4189_flush_data_cache_page(unsigned long addr)
{
	/* Write-through cache, no need to flush it. */
}

static void lx4189_flush_icache_page(struct vm_area_struct *vma, struct page *page)
{
	struct mm_struct *mm = vma->vm_mm;

	if (mm->context && (vma->vm_flags & VM_EXEC) &&
				get_phys_page((unsigned long)page, vma->vm_mm))
		lx4189_flush_icache_range((unsigned long)0, (unsigned long)0);
}

static void lx4189_flush_cache_sigtramp(unsigned long addr)
{
	lx4189___flush_cache_all();
}

static void lx4189_dma_cache_wback(unsigned long start, unsigned long size)
{
	wbflush();
	/* Write-through data cache, no need to flush it. */
}

static void lx4189_dma_cache_wback_inv(unsigned long start, unsigned long size)
{
	wbflush();
	lx4189_flush_dcache_range(start, start + size);
}

void __init ld_mmu_lx4189(void)
{
	extern void build_clear_page(void);
	extern void build_copy_page(void);

	lx4189_probe_cache();

	flush_cache_all = lx4189_flush_cache_all;
	__flush_cache_all = lx4189___flush_cache_all;
	flush_cache_mm = lx4189_flush_cache_mm;
	flush_cache_range = lx4189_flush_cache_range;
	flush_cache_page = lx4189_flush_cache_page;
	flush_icache_page = lx4189_flush_icache_page;
	flush_icache_range = lx4189_flush_icache_range;
	flush_cache_sigtramp = lx4189_flush_cache_sigtramp;
	flush_data_cache_page = lx4189_flush_data_cache_page;

	_dma_cache_wback_inv = lx4189_dma_cache_wback_inv;
	_dma_cache_wback = lx4189_dma_cache_wback;
	_dma_cache_inv = lx4189_dma_cache_wback_inv;

	printk("Primary instruction cache %ldkB, linesize %ld bytes.\n",
		icache_size >> 10, icache_lsize);
	printk("Primary data cache %ldkB, linesize %ld bytes.\n",
		dcache_size >> 10, dcache_lsize);

	build_clear_page();
	build_copy_page();
}
