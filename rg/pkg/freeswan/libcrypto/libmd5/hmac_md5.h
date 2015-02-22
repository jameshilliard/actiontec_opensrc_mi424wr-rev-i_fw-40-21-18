#include "md5.h"
typedef struct {
	MD5_CTX ictx,octx;
} md5_hmac_context;
#define MD5_BLOCKSIZE 64
#define MD5_HASHLEN   16

void md5_hmac_hash(md5_hmac_context *hctx, const __u8 * dat, int len, __u8 * hash, int hashlen);
void md5_hmac_set_key(md5_hmac_context *hctx, const __u8 * key, int keylen);
