/* from iabg ipv6_main.c - mk */
#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>

#ifdef __KERNEL__
void zero_out_for_ah(struct inet6_skb_parm *parm, char* packet);

int ipsec6_out_get_ahsize(struct ipsec_sp *policy);
int ipsec6_out_get_espsize(struct ipsec_sp *policy);
static inline int ipsec6_out_get_hdrsize(struct ipsec_sp *policy)
{
	return ipsec6_out_get_ahsize(policy) + ipsec6_out_get_espsize(policy);
}

struct ipv6_txoptions *ipsec6_out_get_newopt(struct ipv6_txoptions *opt, struct ipsec_sp *policy);
int ipsec6_ah_calc(const void *data, unsigned length, 
		inet_getfrag_t getfrag, struct sk_buff *skb, 
		struct ipv6_auth_hdr *authhdr, struct ipsec_sp *policy);
void ipsec6_enc(const void *data, unsigned length, u8 proto, struct ipv6_txoptions *opt,
		void **newdata, unsigned *newlength, struct ipsec_sp *policy);
void ipsec6_out_finish(struct ipv6_txoptions *opt, struct ipsec_sp *policy_ptr);
int ipsec6_input_check_ah(struct sk_buff **skb, struct ipv6_auth_hdr* authhdr);
int ipsec6_input_check_esp(struct sk_buff **skb, struct ipv6_esp_hdr *esphdr, u8 *nexthdr);
#endif /* __KERNEL__ */
