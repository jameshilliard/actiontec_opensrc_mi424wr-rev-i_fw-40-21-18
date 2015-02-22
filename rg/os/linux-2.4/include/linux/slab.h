/*
 * linux/mm/slab.h
 * Written by Mark Hemment, 1996.
 * (markhe@nextd.demon.co.uk)
 */

#if	!defined(_LINUX_SLAB_H)
#define	_LINUX_SLAB_H

#ifdef __KERNEL__
#include <linux/autoconf.h>
#endif

/* The following definition of kmalloc_hdr, is included by user mode (ulibc).
 * there for it is out of ifdef __KERNEL__ scope 
 */
#ifdef CONFIG_RG_DEBUG_KMALLOC
#define KMALLOC_STACK_SIZE 64
struct kmalloc_debug_head
{
    struct kmalloc_debug_hdr *next, *prev;
    unsigned long qlen;		
};

typedef struct kmalloc_debug_hdr
{
    struct kmalloc_debug_hdr *next, *prev;
    struct kmalloc_debug_head *list;
    int magic;
    int size;
    char *src_file;
    char *allocator;
    int src_line;
    int was_released;
    int timestamp;
    int stack[KMALLOC_STACK_SIZE];

    int pid;
    int label;
    void *alloc_buf;
    int alloc_size;
    void *opaque;
} kmalloc_hdr;
#endif

#if	defined(__KERNEL__)

typedef struct kmem_cache_s kmem_cache_t;

#include	<linux/mm.h>
#include	<linux/cache.h>

/* flags for kmem_cache_alloc() */
#define	SLAB_NOFS		GFP_NOFS
#define	SLAB_NOIO		GFP_NOIO
#define SLAB_NOHIGHIO		GFP_NOHIGHIO
#define	SLAB_ATOMIC		GFP_ATOMIC
#define	SLAB_USER		GFP_USER
#define	SLAB_KERNEL		GFP_KERNEL
#define	SLAB_NFS		GFP_NFS
#define	SLAB_DMA		GFP_DMA

#define SLAB_LEVEL_MASK		(__GFP_WAIT|__GFP_HIGH|__GFP_IO|__GFP_HIGHIO|__GFP_FS)
#define	SLAB_NO_GROW		0x00001000UL	/* don't grow a cache */

/* flags to pass to kmem_cache_create().
 * The first 3 are only valid when the allocator as been build
 * SLAB_DEBUG_SUPPORT.
 */
#define	SLAB_DEBUG_FREE		0x00000100UL	/* Peform (expensive) checks on free */
#define	SLAB_DEBUG_INITIAL	0x00000200UL	/* Call constructor (as verifier) */
#define	SLAB_RED_ZONE		0x00000400UL	/* Red zone objs in a cache */
#define	SLAB_POISON		0x00000800UL	/* Poison objects */
#define	SLAB_NO_REAP		0x00001000UL	/* never reap from the cache */
#define	SLAB_HWCACHE_ALIGN	0x00002000UL	/* align objs on a h/w cache lines */
#define SLAB_CACHE_DMA		0x00004000UL	/* use GFP_DMA memory */
#define SLAB_MUST_HWCACHE_ALIGN	0x00008000UL	/* force alignment */

/* flags passed to a constructor func */
#define	SLAB_CTOR_CONSTRUCTOR	0x001UL		/* if not set, then deconstructor */
#define SLAB_CTOR_ATOMIC	0x002UL		/* tell constructor it can't sleep */
#define	SLAB_CTOR_VERIFY	0x004UL		/* tell constructor it's a verify call */

/* prototypes */
extern void kmem_cache_init(void);
extern void kmem_cache_sizes_init(void);

extern kmem_cache_t *kmem_find_general_cachep(size_t, int gfpflags);
extern kmem_cache_t *kmem_cache_create(const char *, size_t, size_t, unsigned long,
				       void (*)(void *, kmem_cache_t *, unsigned long),
				       void (*)(void *, kmem_cache_t *, unsigned long));
extern int kmem_cache_destroy(kmem_cache_t *);
extern int kmem_cache_shrink(kmem_cache_t *);
extern void *kmem_cache_alloc(kmem_cache_t *, int);
extern void kmem_cache_free(kmem_cache_t *, void *);
extern unsigned int kmem_cache_size(kmem_cache_t *);
extern int kmem_cache_has_free_space(kmem_cache_t *cachep);

extern unsigned int kmem_get_free_ram_size(void);

extern void *kmalloc(size_t, int);
extern void kfree(const void *);

#ifdef CONFIG_RG_DEBUG_KMALLOC
#ifdef CONFIG_DEBUG_KMALLOC
#error Cannot use both CONFIG_RG_DEBUG_KMALLOC and CONFIG_DEBUG_KMALLOC
#endif
extern spinlock_t kmalloc_queue_lock;
extern int kmalloc_was_init;
extern struct kmalloc_debug_head kmalloc_debug_list;
extern int kmalloc_label;
extern void kmalloc_queue_head_init(struct kmalloc_debug_head *list);
extern void kmalloc_queue_tail(struct kmalloc_debug_head *list,
    struct kmalloc_debug_hdr *buf);
extern void kmalloc_unlink(struct kmalloc_debug_hdr *buf);

extern __inline__ void kmalloc_queue_head_init(struct kmalloc_debug_head *list)
{
    list->prev = (struct kmalloc_debug_hdr *)list;
    list->next = (struct kmalloc_debug_hdr *)list;
    list->qlen = 0;
}

extern __inline__ void __kmalloc_queue_tail(struct kmalloc_debug_head *list,
    struct kmalloc_debug_hdr *newsk)
{
    struct kmalloc_debug_hdr *prev, *next;

    newsk->list = list;
    list->qlen++;
    next = (struct kmalloc_debug_hdr *)list;
    prev = next->prev;
    newsk->next = next;
    newsk->prev = prev;
    next->prev = newsk;
    prev->next = newsk;
}

extern __inline__ void kmalloc_queue_tail(struct kmalloc_debug_head *list, 
    struct kmalloc_debug_hdr *newsk)
{
    unsigned long flags;

    spin_lock_irqsave(&kmalloc_queue_lock, flags);
    __kmalloc_queue_tail(list, newsk);
    spin_unlock_irqrestore(&kmalloc_queue_lock, flags);
}

extern __inline__ void __kmalloc_unlink(struct kmalloc_debug_hdr *item,
    struct kmalloc_debug_head *list)
{
    struct kmalloc_debug_hdr *prev, *next;

    list->qlen--;
    next = item->next;
    prev = item->prev;
    item->next = NULL;
    item->prev = NULL;
    item->list = NULL;
    next->prev = prev;
    prev->next = next;
}

extern __inline__ void kmalloc_unlink(struct kmalloc_debug_hdr *item)
{
    unsigned long flags;

    spin_lock_irqsave(&kmalloc_queue_lock, flags);
    if(item->list)
	__kmalloc_unlink(item, item->list);
    spin_unlock_irqrestore(&kmalloc_queue_lock, flags);
}

extern void *kmalloc_org(size_t, int); /* Original kmalloc */
extern void *kmalloc_func( size_t, int , char *, char *, int );
extern void kfree_org(const void *); /* Original kfree */
extern void kfree_func(const void *, char *, int);
void kmalloc_show_history(int); 
extern void kmalloc_set_label(int);
extern int kmalloc_get_label(void);
extern void kmalloc_debug_test(int debug_level);

#define KMALLOC_DEBUG_MAGIC 0x12399321
#define kmalloc(size,flags) \
    kmalloc_func(size, flags, "kmalloc", __FILE__,__LINE__)
#define kfree(objp) kfree_func(objp, __FILE__,__LINE__)
#define kmalloc_hdr2ptr(hdr) (((void*)(hdr))+sizeof(kmalloc_hdr))
#define kmalloc_ptr2hdr(ptr) (((void*)(ptr))-sizeof(kmalloc_hdr))
#endif

#if CONFIG_DEBUG_KMALLOC

extern void *kmem_cache_alloc_debug(kmem_cache_t *, int, char *, int);
extern void kmem_cache_free_debug(kmem_cache_t *, void *, char *, int);
extern void *kmalloc_debug(size_t, int, char *, int);
extern void kfree_debug(const void *, char *, int);

extern void kmem_cache_debug_set_alloc(const void *, size_t, int,
	char *, char *, int);
extern void kmem_cache_debug_set_free(const void *, char *, char *, int);
    
#define kmem_cache_alloc(cache, flags) \
	kmem_cache_alloc_debug(cache, flags, __FILE__, __LINE__)

#define kmem_cache_free(cache, objp) \
	kmem_cache_free_debug(cache, objp, __FILE__, __LINE__)

#define kmalloc(size, flags) \
	kmalloc_debug(size, flags, __FILE__, __LINE__)

#define kfree(objp) \
	kfree_debug(objp, __FILE__, __LINE__)

#endif

extern int FASTCALL(kmem_cache_reap(int));

/* System wide caches */
extern kmem_cache_t	*vm_area_cachep;
extern kmem_cache_t	*mm_cachep;
extern kmem_cache_t	*names_cachep;
extern kmem_cache_t	*files_cachep;
extern kmem_cache_t	*filp_cachep;
extern kmem_cache_t	*dquot_cachep;
extern kmem_cache_t	*bh_cachep;
extern kmem_cache_t	*fs_cachep;
extern kmem_cache_t	*sigact_cachep;

#endif	/* __KERNEL__ */

#endif	/* _LINUX_SLAB_H */
