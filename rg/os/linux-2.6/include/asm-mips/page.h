/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999, 2000, 03 Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

#include <linux/config.h>

#ifdef __KERNEL__

#include <spaces.h>

#endif

/*
 * PAGE_SHIFT determines the page size
 */
#ifdef CONFIG_PAGE_SIZE_4KB
#define PAGE_SHIFT	12
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define PAGE_SHIFT	13
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define PAGE_SHIFT	14
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PAGE_SHIFT	16
#endif
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK       (~((1 << PAGE_SHIFT) - 1))

#ifdef CONFIG_HUGETLB_PAGE
#define HPAGE_SHIFT	21
#define HPAGE_SIZE	((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK	(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#endif

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

extern void clear_page(void * page);
extern void copy_page(void * to, void * from);

extern unsigned long shm_align_mask;

static inline unsigned long pages_do_alias(unsigned long addr1,
	unsigned long addr2)
{
	return (addr1 ^ addr2) & shm_align_mask;
}

struct page;

static inline void clear_user_page(void *addr, unsigned long vaddr,
	struct page *page)
{
	extern void (*flush_data_cache_page)(unsigned long addr);

	clear_page(addr);
	if (pages_do_alias((unsigned long) addr, vaddr))
		flush_data_cache_page((unsigned long)addr);
}

static inline void copy_user_page(void *vto, void *vfrom, unsigned long vaddr,
	struct page *to)
{
	extern void (*flush_data_cache_page)(unsigned long addr);

	copy_page(vto, vfrom);
	if (pages_do_alias((unsigned long)vto, vaddr))
		flush_data_cache_page((unsigned long)vto);
}

/*
 * These are used to make use of C type-checking..
 */
#ifdef CONFIG_64BIT_PHYS_ADDR
  #ifdef CONFIG_CPU_MIPS32
    typedef struct { unsigned long pte_low, pte_high; } pte_t;
    #define pte_val(x)    ((x).pte_low | ((unsigned long long)(x).pte_high << 32))
  #else
     typedef struct { unsigned long long pte; } pte_t;
     #define pte_val(x)	((x).pte)
  #endif
#else
typedef struct { unsigned long pte; } pte_t;
#define pte_val(x)	((x).pte)
#endif
#define __pte(x)	((pte_t) { (x) } )

/*
 * For 3-level pagetables we defines these ourselves, for 2-level the
 * definitions are supplied by <asm-generic/pgtable-nopmd.h>.
 */
#ifdef CONFIG_64BIT

typedef struct { unsigned long pmd; } pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) } )

#endif

/*
 * Right now we don't support 4-level pagetables, so all pud-related
 * definitions come from <asm-generic/pgtable-nopud.h>.
 */

/*
 * Finall the top of the hierarchy, the pgd
 */
typedef struct { unsigned long pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) } )

/*
 * Manipulate page protection bits
 */
typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) } )

/*
 * On R4000-style MMUs where a TLB entry is mapping a adjacent even / odd
 * pair of pages we only have a single global bit per pair of pages.  When
 * writing to the TLB make sure we always have the bit set for both pages
 * or none.  This macro is used to access the `buddy' of the pte we're just
 * working on.
 */
#define ptep_buddy(x)	((pte_t *)((unsigned long)(x) ^ sizeof(pte_t)))

#endif /* !__ASSEMBLY__ */

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr) + PAGE_SIZE - 1) & PAGE_MASK)

#define __pa(x)			((unsigned long) (x) - PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long) (x) + PAGE_OFFSET))

#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

#ifndef CONFIG_SPARSEMEM
#ifndef CONFIG_NEED_MULTIPLE_NODES
#define pfn_to_page(pfn)	(mem_map + (pfn))
#define page_to_pfn(page)	((unsigned long)((page) - mem_map))
#define pfn_valid(pfn)		((pfn) < max_mapnr)
#endif
#endif

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define UNCAC_ADDR(addr)	((addr) - PAGE_OFFSET + UNCAC_BASE)
#define CAC_ADDR(addr)		((addr) - UNCAC_BASE + PAGE_OFFSET)

#endif /* defined (__KERNEL__) */

#ifdef CONFIG_LIMITED_DMA
#define WANT_PAGE_VIRTUAL
#endif

#include <asm-generic/page.h>

#endif /* _ASM_PAGE_H */
