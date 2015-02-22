#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <freeswan.h>

#include <constants.h>
#include <defs.h>
#include <log.h>
#include <crypto/des.h>
#include <alg_info.h>
#include <ike_alg.h>

#ifndef DES_CBC_BLOCK_SIZE
#define  DES_CBC_BLOCK_SIZE	8  	/* block size */
#endif

static void __attribute__ ((unused))
do_des(u_int8_t *buf, size_t buf_len, u_int8_t *key, __attribute__ ((unused)) size_t key_size, u_int8_t *iv, bool enc)
{
    des_key_schedule ks;

    (void) des_set_key((des_cblock *)key, ks);

    des_ncbc_encrypt((des_cblock *)buf, (des_cblock *)buf, buf_len,
	ks, (des_cblock *)iv, enc);
}

struct encrypt_desc algo_1des =
{
	algo_type: IKE_ALG_ENCRYPT,
	algo_id:   OAKLEY_DES_CBC,
	algo_next: NULL, 
	enc_ctxsize: 	sizeof(des_key_schedule),
	enc_blocksize: 	DES_CBC_BLOCK_SIZE, 
	keydeflen: 	DES_CBC_BLOCK_SIZE * BITS_PER_BYTE,
	keyminlen: 	DES_CBC_BLOCK_SIZE * BITS_PER_BYTE,
	keymaxlen: 	DES_CBC_BLOCK_SIZE * BITS_PER_BYTE,
	do_crypt: do_des,
};
int ike_alg_1des_init(void);
int
ike_alg_1des_init(void)
{
	int ret = ike_alg_register_enc(&algo_1des);
	return ret;
}
/*
IKE_ALG_INIT_NAME: ike_alg_1des_init
*/
