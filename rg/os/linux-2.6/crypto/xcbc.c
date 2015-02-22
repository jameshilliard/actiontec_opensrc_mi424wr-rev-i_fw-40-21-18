/*
 * Copyright (C)2005 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:
 * 	Kazunori Miyazawa <miyazawa@linux-ipv6.org>
 */

#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include "internal.h"

struct xcbc_ops {
	unsigned int len;
	u8 *prev;
};

const u_int32_t k1[4] = {0x01010101, 0x01010101, 0x01010101, 0x01010101}; 
const u_int32_t k2[4] = {0x02020202, 0x02020202, 0x02020202, 0x02020202}; 
const u_int32_t k3[4] = {0x03030303, 0x03030303, 0x03030303, 0x03030303}; 

int crypto_alloc_xcbc_block(struct crypto_tfm *tfm)
{
	struct xcbc_ops *ops;

	BUG_ON(!crypto_tfm_alg_blocksize(tfm));
	if (crypto_tfm_alg_blocksize(tfm) != 16)
		return 0;

	ops = (struct xcbc_ops*)kmalloc(sizeof(*ops) + 
				+ crypto_tfm_alg_blocksize(tfm), GFP_KERNEL);

	if (ops == NULL)
		return -ENOMEM;

	ops->len = 0;
	ops->prev = (u8*)(ops + 1);

	tfm->crt_cipher.cit_xcbc_block = ops;
	return 0;
}

void crypto_free_xcbc_block(struct crypto_tfm *tfm)
{
	if (tfm->crt_cipher.cit_xcbc_block)
		kfree(tfm->crt_cipher.cit_xcbc_block);
}

static int _crypto_xcbc_init(struct crypto_tfm *tfm, u8 *key, unsigned int keylen)
{
	const unsigned int bsize = crypto_tfm_alg_blocksize(tfm);
	u8 key1[bsize];
	int err;

	if (!(tfm->crt_cipher.cit_mode & CRYPTO_TFM_MODE_CBC))
		return -EINVAL;

	if (keylen != crypto_tfm_alg_blocksize(tfm))
		return -EINVAL;

	if ((err = crypto_cipher_setkey(tfm, key, keylen)))
	    return err;

	tfm->__crt_alg->cra_cipher.cia_encrypt(crypto_tfm_ctx(tfm), key1, (const u8*)k1);

	return crypto_cipher_setkey(tfm, key1, bsize);

}

int crypto_xcbc_init(struct crypto_tfm *tfm, u8 *key, unsigned int keylen)
{
	struct xcbc_ops *ops = (struct xcbc_ops*)tfm->crt_cipher.cit_xcbc_block;

	ops->len = 0;
	memset(ops->prev, 0, crypto_tfm_alg_blocksize(tfm));
	memset(tfm->crt_cipher.cit_iv, 0, crypto_tfm_alg_blocksize(tfm));
	return _crypto_xcbc_init(tfm, key, keylen);
}

void crypto_xcbc_update(struct crypto_tfm *tfm, struct scatterlist *sg, unsigned int nsg)
{
	struct xcbc_ops *ops = (struct xcbc_ops*)tfm->crt_cipher.cit_xcbc_block;
	const unsigned int bsize = crypto_tfm_alg_blocksize(tfm);
	unsigned int i;

	if (!(tfm->crt_cipher.cit_mode & CRYPTO_TFM_MODE_CBC))
		return;
 
	for(i = 0; i < nsg; i++) {

		struct page *pg = sg[i].page;
		unsigned int offset = sg[i].offset;
		unsigned int slen = sg[i].length;

		while (slen > 0) {
			unsigned int len = min(slen, ((unsigned int)(PAGE_SIZE)) - offset);
			char *p = crypto_kmap(pg, 0) + offset;

			/* checking the data can fill the block */
			if ((ops->len + len) <= bsize) {
				memcpy(ops->prev + ops->len, p, len);
				ops->len += len;
				slen -= len;

				/* checking the rest of the page */
				if (len + offset >= PAGE_SIZE) {
					offset = 0;
					pg++;
				} else
					offset += len;

				crypto_kunmap(p, 0);
				crypto_yield(tfm);
				continue;
			}

			/* filling ops->prev with new data and encrypting it */
			memcpy(ops->prev + ops->len, p, bsize - ops->len);
			len -= bsize - ops->len;
			p += bsize - ops->len;
			tfm->crt_u.cipher.cit_xor_block(tfm->crt_cipher.cit_iv,
							ops->prev);
			tfm->__crt_alg->cra_cipher.cia_encrypt(
				crypto_tfm_ctx(tfm), tfm->crt_cipher.cit_iv,
				tfm->crt_cipher.cit_iv);

			/* clearing the length */
			ops->len = 0;

			/* encrypting the rest of data */
			while (len > bsize) {
				tfm->crt_u.cipher.cit_xor_block(tfm->crt_cipher.cit_iv, p);
				tfm->__crt_alg->cra_cipher.cia_encrypt(
					crypto_tfm_ctx(tfm), tfm->crt_cipher.cit_iv,
					tfm->crt_cipher.cit_iv);
				p += bsize;
				len -= bsize;
			}

			/* keeping the surplus of blocksize */
			if (len) {
				memcpy(ops->prev, p, len);
				ops->len = len;
			}
			crypto_kunmap(p, 0);
			crypto_yield(tfm);
			slen -= min(slen, ((unsigned int)(PAGE_SIZE)) - offset);
			offset = 0;
			pg++;
		}
	}
}

int crypto_xcbc_final(struct crypto_tfm *tfm, u8 *key, unsigned int keylen, u8 *out)
{
	struct xcbc_ops *ops = (struct xcbc_ops*)tfm->crt_cipher.cit_xcbc_block;
	const unsigned int bsize = crypto_tfm_alg_blocksize(tfm);
	int ret = 0;

	if (!(tfm->crt_cipher.cit_mode & CRYPTO_TFM_MODE_CBC))
		return -EINVAL;

	if (keylen != bsize)
		return -EINVAL;

	if (ops->len == bsize) {
		u8 key2[bsize];

		if ((ret = crypto_cipher_setkey(tfm, key, keylen)))
			return ret;

		tfm->__crt_alg->cra_cipher.cia_encrypt(crypto_tfm_ctx(tfm), key2, (const u8*)k2);
		tfm->crt_u.cipher.cit_xor_block(tfm->crt_cipher.cit_iv, ops->prev);
		tfm->crt_u.cipher.cit_xor_block(tfm->crt_cipher.cit_iv, key2);

		_crypto_xcbc_init(tfm, key, keylen);

		tfm->__crt_alg->cra_cipher.cia_encrypt(crypto_tfm_ctx(tfm), out, tfm->crt_cipher.cit_iv);
	} else {
		u8 key3[bsize];
		unsigned int rlen;
		u8 *p = ops->prev + ops->len;
		*p = 0x80;
		p++;

		rlen = bsize - ops->len -1;
		if (rlen)
			memset(p, 0, rlen);

		if ((ret = crypto_cipher_setkey(tfm, key, keylen)))
			return ret;

		tfm->__crt_alg->cra_cipher.cia_encrypt(crypto_tfm_ctx(tfm), key3, (const u8*)k3);

		tfm->crt_u.cipher.cit_xor_block(tfm->crt_cipher.cit_iv, ops->prev);
		tfm->crt_u.cipher.cit_xor_block(tfm->crt_cipher.cit_iv, key3);
		_crypto_xcbc_init(tfm, key, keylen);
		tfm->__crt_alg->cra_cipher.cia_encrypt(crypto_tfm_ctx(tfm), out, tfm->crt_cipher.cit_iv);
	}

	return ret;
}

int crypto_xcbc(struct crypto_tfm *tfm, u8 *key, unsigned int keylen,
		struct scatterlist *sg, unsigned int nsg, u8 *out)
{
	int ret = 0;

	ret = crypto_xcbc_init(tfm, key, keylen);
	if (ret)
		return ret;
	crypto_xcbc_update(tfm, sg, nsg);
	ret = crypto_xcbc_final(tfm, key, keylen, out);

	return ret;
}

EXPORT_SYMBOL_GPL(crypto_xcbc_init);
EXPORT_SYMBOL_GPL(crypto_xcbc_update);
EXPORT_SYMBOL_GPL(crypto_xcbc_final);
EXPORT_SYMBOL_GPL(crypto_xcbc);
