// mv_nfp_sec.c

#include <asm/scatterlist.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <net/xfrm.h>

//#include "ctrlEnv/sys/mvSysCesa.h"
//#include "ctrlEnv/sys/mvSysGbe.h"
#include "mvDebug.h"
//#include "eth/mvEth.h"
//#include "eth-phy/mvEthPhy.h"
//#include "eth/nfp/mvNfpSec.h"
#include "../../../common/mv802_3.h"
//#include "../mv_ethernet/mv_netdev.h"

#include "mvTypes.h"
#include "mvOs.h"

#include "cesa/mvCesa.h"
#include "cesa/mvMD5.h"
#include "cesa/mvSHA1.h"
#include "cesa/mvCesaRegs.h"
#include "cesa/AES/mvAes.h"
#include "cesa/mvLru.h"
#include "nfp/mvNfpSec.h"

extern spinlock_t nfp_lock;
extern spinlock_t cesa_lock;
static spinlock_t nfp_sec_lock;
static unsigned long nfp_cesa_lock_flags;

struct nfp_cesa_alg_trans {
	const char	*name;
	unsigned int	algorithm;
};

static const struct nfp_cesa_alg_trans nfp_cesa_translation[] = {
	{ "ecb(cipher_null)",	MV_CESA_CRYPTO_NULL	},
	{ "cbc(des)",		MV_CESA_CRYPTO_DES	},
	{ "cbc(des3_ede)",	MV_CESA_CRYPTO_3DES	},
	{ "cbc(aes)",		MV_CESA_CRYPTO_AES	},
	{ "digest_null",	MV_CESA_MAC_NULL	},
	{ "hmac(md5)",		MV_CESA_MAC_HMAC_MD5	},
	{ "hmac(sha1)",		MV_CESA_MAC_HMAC_SHA1	},
	{ NULL }
};


static int nfp_alg_translate(const char *alg)
{
	const struct nfp_cesa_alg_trans *t = nfp_cesa_translation;

	while (t->name) {
		if (!strcmp(t->name, alg))
			return t->algorithm;

		t++;
	}

	return -1;
}


static int nfp_sec_copy_state(struct xfrm_state *state)
{
	int enc = MV_CESA_CRYPTO_NULL;
	int mac = MV_CESA_MAC_NULL;
	const char *ekey = NULL;
	const char *mkey = NULL;
	unsigned long flags;
	int eklen = 0;
	int mklen = 0;
	printk("in %s\n",__FUNCTION__);
	/* NFP supports only encryption and authentication */
	if (state->calg || state->aead)
		return -EINVAL;

	if (state->ealg) {
		enc = nfp_alg_translate(state->ealg->alg_name);
		ekey = state->ealg->alg_key;
		eklen = state->ealg->alg_key_len >> 3;
	}

	if (state->aalg) {
		mac = nfp_alg_translate(state->aalg->alg_name);
		mkey = state->aalg->alg_key;
		mklen = state->aalg->alg_key_len >> 3;
	}

	/* We have to support both algorithms */
	if (enc < 0 || mac < 0)
		return -EINVAL;

	xfrm_state_hold(state);

	spin_lock_irqsave(&nfp_sec_lock, flags);
	state->nfp_cookie = mvNfpSecStateCreate(enc, ekey, eklen,
		mac, mkey, mklen, state->id.proto, state->id.spi,
		state->replay.oseq, state);
	spin_unlock_irqrestore(&nfp_sec_lock, flags);

	if (!state->nfp_cookie) {
		xfrm_state_put(state);
		return -ENOMEM;
	}

	return 0;
}



void nfp_sec_rule_set(int family, struct sk_buff *skb, struct xfrm_state *state,
	int outgoing)
{
	unsigned long flags;
	struct iphdr *ip;

	/* IPSec are supported only on IPv4 */
	if (family != MV_INET)
		return;

	/* Create NFP copy of the state */
	if (!state->nfp_cookie)
		if (nfp_sec_copy_state(state))
			return;
	printk("after nfp_sec_copy_state() in %s\n",__FUNCTION__);
	ip = ip_hdr(skb);

	spin_lock_irqsave(&nfp_sec_lock, flags);
	mvNfpSecRuleInsert(MV_INET, (const MV_U8 *)&ip->daddr,
				(const MV_U8 *)&ip->saddr, state->nfp_cookie);
	spin_unlock_irqrestore(&nfp_sec_lock, flags);

	return;
}

static int __init nfp_sec_init(void)
{
	printk("in %s\n",__FUNCTION__);
	spin_lock_init(&nfp_sec_lock);
	mvNfpSecInit(1024);
	return 0;
}

module_init(nfp_sec_init);

MODULE_LICENSE("Marvell/GPL");
MODULE_AUTHOR("Rami Rosen");
MODULE_DESCRIPTION("NFP SEC for CESA crypto");
