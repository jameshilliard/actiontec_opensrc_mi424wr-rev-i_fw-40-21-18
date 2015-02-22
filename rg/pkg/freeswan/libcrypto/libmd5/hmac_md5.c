#include <linux/types.h>
#include <linux/string.h>
#include <hmac_generic.h>
#include "md5.h"
#include "hmac_md5.h"

void inline md5_result(MD5_CTX *ctx, __u8 * hash, int hashlen) {
	if (hashlen==MD5_HASHLEN)
		MD5_Final(hash, ctx);
	else {
		__u8 hash_buf[MD5_HASHLEN];
		MD5_Final(hash_buf, ctx);
		memcpy(hash, hash_buf, hashlen);
	}
}
HMAC_SET_KEY_IMPL (md5_hmac_set_key, 
		md5_hmac_context, MD5_BLOCKSIZE, 
		MD5_Init, MD5_Update)
HMAC_HASH_IMPL (md5_hmac_hash, 
		md5_hmac_context, MD5_CTX, MD5_HASHLEN,
		MD5_Update, md5_result)
