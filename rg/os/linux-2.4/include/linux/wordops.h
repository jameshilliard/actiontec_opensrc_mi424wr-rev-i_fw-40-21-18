/* wordops.h - Linux CryptoAPI <cryptoapi-devel@kerneli.org>
 *
 */

#ifndef _LINUX_WORDOPS_H_
#define _LINUX_WORDOPS_H_

#include <linux/types.h>

#if 0
#include <asm/wordops.h>
#endif

#ifdef WORDOP_WANT_DEFINE
#define rotl(reg, val) ((reg << val) | (reg >> (32 - val)))
#define rotr(reg, val) ((reg >> val) | (reg << (32 - val)))
#endif /* WORDOP_WANT_DEFINE */

static inline 
u32 generic_rotr32 (const u32 x, const unsigned bits)
{
  const unsigned n = bits % 32;
  return (x >> n) | (x << (32 - n));
}

static inline 
u32 generic_rotl32 (const u32 x, const unsigned bits)
{
  const unsigned n = bits % 32;
  return (x << n) | (x >> (32 - n));
}

/* 64bit variants */

static inline 
u64 generic_rotr64 (const u64 x, const unsigned bits)
{
  const unsigned n = bits % 64;
  return (x >> n) | (x << (64 - n));
}

static inline 
u64 generic_rotl64 (const u64 x, const unsigned bits)
{
  const unsigned n = bits % 64;
  return (x << n) | (x >> (64 - n));
}


#endif /* _LINUX_WORDOPS_H_ */
