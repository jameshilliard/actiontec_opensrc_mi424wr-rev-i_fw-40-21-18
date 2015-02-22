/* Generic skb cache. the cache stores both the skb and its (fixed size) data
 * buffer to allow performance enhancement.
 *
 * Written by Jungo ltd.
 */

#include <linux/skbuff.h>
#include <linux/skb_cache.h>

#ifdef CONFIG_RG_SKB_CACHE

#define SKB_CACHE_TIMEOUT HZ /* 1 second */
#define SKB_LOWER_LIMIT 10 /* amount of skb's to leave in the cache */

typedef struct _skb_cache_t {
    struct _skb_cache_t *next; /* list of skb caches */
    struct sk_buff *skb_list; /* list of skbs in this cache */
    unsigned int alloc_size; /* size of the data buffer */
    struct timer_list timer; /* cleanup timer */
    
    unsigned int count; /* ammount of skbs in this cache */
    /* statistics */
    unsigned int hits;
    unsigned int misses;
    unsigned int free_by_timer;
    unsigned int free_invalid;
} skb_cache_t;

static skb_cache_t *skb_cache_list;

static void skb_cache_timer_cb(unsigned long arg)
{
    skb_cache_t *skb_cache = (skb_cache_t *)arg;
    struct sk_buff *skb;

    if (skb_cache->count <= SKB_LOWER_LIMIT)
	goto Exit;

    local_irq_disable();
    skb = skb_cache->skb_list;
    if (skb)
	skb_cache->skb_list = skb->next;
    local_irq_enable();
    if (skb)
    {
	skb_cache->count--;
	skb->retfreeq_cb = NULL;
	kfree_skbmem(skb);
	skb_cache->free_by_timer++;
    }

Exit:
    /* re-set timer */
    init_timer(&skb_cache->timer);
    skb_cache->timer.expires = SKB_CACHE_TIMEOUT + jiffies;
    /* following info stays the same
    skb_cache->timer.data = (unsigned long) skb_cache;
    skb_cache->timer.function = skb_cache_timer_cb;
    */
    add_timer(&skb_cache->timer);
}

void *skb_cache_start(unsigned int alloc_size)
{
    skb_cache_t *skb_cache = kmalloc(sizeof(skb_cache_t), GFP_ATOMIC);

    if (!skb_cache)
	return NULL;
    memset(skb_cache, 0, sizeof(skb_cache_t));
    skb_cache->alloc_size = SKB_DATA_ALIGN(alloc_size);

    /* add clean up timer */
    init_timer(&skb_cache->timer);
    skb_cache->timer.expires = SKB_CACHE_TIMEOUT + jiffies;
    skb_cache->timer.data = (unsigned long) skb_cache;
    skb_cache->timer.function = skb_cache_timer_cb;
    add_timer(&skb_cache->timer);

    /* add to global list of cache lists */
    local_irq_disable();
    skb_cache->next = skb_cache_list;
    skb_cache_list = skb_cache;
    local_irq_enable();

    return skb_cache;
}

void skb_cache_end(void *skb_cache_handle)
{
    skb_cache_t *skb_cache = skb_cache_handle;
    skb_cache_t **p;
    struct sk_buff *skb, *next;

    if (!skb_cache)
	return;

    del_timer(&skb_cache->timer);
    skb_cache->timer.expires = 0;
    
    /* take off global list */
    local_irq_disable();
    for (p = &skb_cache_list; *p; p = &(*p)->next)
    {
	if (*p == skb_cache)
	{
	    *p = skb_cache->next;
	    break;
	}
    }
    local_irq_enable();

    /* free the skb in the list */
    for (skb = skb_cache->skb_list; skb; skb = next)
    {
	next = skb->next;
	skb->retfreeq_cb = NULL;
	kfree_skbmem(skb);
    }

    kfree(skb_cache);
}

static void skb_cache_free(void *context, void *obj, int flag)
{
    struct sk_buff *skb = obj;
    skb_cache_t *skb_cache = context;

    if (FREE_SKB != flag)
	return;

    /* the cache saves the skb struct together with its data buffer,
     * if the data buffer is used by other skb, we can not store this skb
     */
    if (atomic_read(&skb_shinfo(skb)->dataref) > 1)
    {
	skb_cache->free_invalid++;
	skb->retfreeq_cb = NULL;
	kfree_skbmem(skb);
	return;
    }
    
    local_irq_disable();
    skb->next = skb_cache->skb_list;
    skb_cache->skb_list = skb;
    skb_cache->count++;
    local_irq_enable();
}

struct sk_buff *skb_cache_alloc(void *skb_cache_handle)
{
    struct sk_buff *skb;
    struct skb_shared_info *shinfo;
    skb_cache_t *skb_cache = skb_cache_handle;

    /* try first to get skb from the cache */
    local_irq_disable();
    skb = skb_cache->skb_list;
    if (skb)
    {
	skb_cache->skb_list = skb->next;
	skb_cache->count--;
	local_irq_enable();
    }
    else
    {
	local_irq_enable();

	/* alloc a new skb if the cache was empty */
	skb = __alloc_skb(skb_cache->alloc_size, GFP_ATOMIC, 0);

	/* set this skb to use be freed to the cache */
	skb->retfreeq_cb = skb_cache_free;
	skb->retfreeq_context = skb_cache;
	skb->retfreeq_skb_prealloc = 1;
	skb->retfreeq_data_prealloc = 1;

	skb_cache->misses++;
	return skb;
    }

    memset(skb, 0, offsetof(struct sk_buff, truesize));
    atomic_set(&skb->users, 1);
    skb->data = skb->tail = skb->head;
    
    /* re set this info due to the memset above */
    skb->retfreeq_cb = skb_cache_free;
    skb->retfreeq_context = skb_cache;
    skb->retfreeq_skb_prealloc = 1;
    skb->retfreeq_data_prealloc = 1;

    /* make sure we initialize shinfo sequentially */
    shinfo = skb_shinfo(skb);
    atomic_set(&shinfo->dataref, 1);
    shinfo->tso_size = 0;
    shinfo->tso_segs = 0;
    shinfo->ufo_size = 0;
    shinfo->ip6_frag_id = 0;
    shinfo->frag_list = NULL;
    skb_cache->hits++;
    return skb;
}

void skb_cache_print(char *buf, unsigned int size)
{
    skb_cache_t *p;
    int i, buf_size;

    for (i = 0, p = skb_cache_list, buf_size = 0; p; p = p->next, i++)
    {
	if (buf_size + 200 < size)
	{
	    sprintf(buf + buf_size, "%d. skb size: %d, count: %d, "
		"hits: %d, misses: %d, free by timer: %d, free invalid: %d\n",
		i, p->alloc_size, p->count, p->hits, p->misses,
		p->free_by_timer, p->free_invalid);
	    buf_size += strlen(buf + buf_size);
	}
    }
    sprintf(buf + buf_size, "total: %d skb cache lists\n", i);
}

EXPORT_SYMBOL(skb_cache_start);
EXPORT_SYMBOL(skb_cache_end);
EXPORT_SYMBOL(skb_cache_alloc);
EXPORT_SYMBOL(skb_cache_print);

#endif

