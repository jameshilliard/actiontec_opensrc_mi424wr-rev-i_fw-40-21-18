#include <linux/types.h>
#include <linux/string.h>
#include "hmac_generic.h"
#include "sha.h"
#include "hmac_sha1.h"

#if 0
#define SHA1_Final  SHA1Final
#define SHA1_Init   SHA1Init
#define SHA1_Update SHA1Update
#endif
void inline sha1_result(SHA1_CTX *ctx, __u8 * hash, int hashlen) {
	if (hashlen==SHA1_HASHLEN)
		SHA1_Final(hash, ctx);
	else {
		__u8 hash_buf[SHA1_HASHLEN];
		SHA1_Final(hash_buf, ctx);
		memcpy(hash, hash_buf, hashlen);
	}
}
HMAC_SET_KEY_IMPL (sha1_hmac_set_key, 
		sha1_hmac_context, SHA1_BLOCKSIZE, 
		SHA1_Init, SHA1_Update)
HMAC_HASH_IMPL (sha1_hmac_hash, 
		sha1_hmac_context, SHA1_CTX, SHA1_HASHLEN,
		SHA1_Update, sha1_result)
