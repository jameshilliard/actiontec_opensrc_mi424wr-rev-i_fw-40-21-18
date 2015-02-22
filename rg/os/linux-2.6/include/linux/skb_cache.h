/* Generic skb cache. the cache stores both the skb and its (fixed size) data
 * buffer to allow performance enhancement.
 *
 * Written by Jungo ltd.
 */

#ifndef _SKB_CACHE_H_
#define _SKB_CACHE_H_
#ifdef CONFIG_RG_SKB_CACHE

#include <linux/skbuff.h>
#include <linux/timer.h>

void *skb_cache_start(unsigned int alloc_size);
void skb_cache_end(void *skb_cache_handle);
struct sk_buff *skb_cache_alloc(void *skb_cache_handle);
void skb_cache_print(char *buf, unsigned int size);

#endif
#endif

