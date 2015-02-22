#include "sha.h"
#define SHA1_CTX SHA_CTX
typedef struct {
	SHA1_CTX ictx,octx;
} sha1_hmac_context;
#define SHA1_BLOCKSIZE 64
#define SHA1_HASHLEN   20

void sha1_hmac_hash(sha1_hmac_context *hctx, const __u8 * dat, int len, __u8 * hash, int hashlen);
void sha1_hmac_set_key(sha1_hmac_context *hctx, const __u8 * key, int keylen);
