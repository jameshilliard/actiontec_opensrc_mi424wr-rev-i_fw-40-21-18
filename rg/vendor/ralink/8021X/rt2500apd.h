#ifndef HOSTAPD_H
#define HOSTAPD_H

#include "common.h"
#include "ap.h"
#include <linux/wireless.h>

#define MAX_MSSID_NUM               8
#define WEP8021X_KEY_LEN            13

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#ifndef ETH_P_ALL
#define ETH_P_ALL 0x0003
#endif

#include "config.h"
#define NIC_DBG_STRING      (" ")

#define RT_DEBUG_HOSTAPD    0
#define RT_DEBUG_TEMP    1

#define RT_DEBUG_ERROR   1
#define RT_DEBUG_WARN   2
#define RT_DEBUG_TRACE   3
#define RT_DEBUG_INFO   4

#define CODE_QUERY        0
#define CODE_SET          1

#define RT_PRIV_IOCTL                                   SIOCIWFIRSTPRIV + 0x01
#define NIC_DEVICE_NAME            "RT2500AP"
// Ralink defined OIDs
#define OID_GET_SET_TOGGLE                              0x8000
#define OID_802_11_BSSID							0x0101
#define RT_OID_DEVICE_NAME                              0x0200
#define OID_802_11_ADD_KEY	                            0x011C
#define RT_OID_802_11_ADD_KEY							0x0222
#define RT_OID_802_11_PREAMBLE                          0x0201
#define RT_OID_802_11_LINK_STATUS                       0x0202
#define RT_OID_802_11_RESET_COUNTERS                    0x0203
#define RT_OID_802_11_AC_CAM                            0x0204
#define RT_OID_802_11_RADIUS_DATA                   0x0220

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#ifndef ETH_P_PAE
#define ETH_P_PAE 0x888E /* Port Access Entity (IEEE 802.1X) */
#endif /* ETH_P_PAE */
#define BIT(x) (1 << (x))
#define REAUTH_TIMER_DEFAULT_reAuthEnabled TRUE
#define REAUTH_TIMER_DEFAULT_reAuthPeriod 3600

#if DBG
u32    RTDebugLevel;
#define DBGPRINT(Level, fmt, args...) 					\
{                                   \
    if (Level <= RT_DEBUG_TRACE)      \
    {                               \
        printf(NIC_DBG_STRING);   \
		printf( fmt, ## args);			\
    }                               \
}
#else
#define DBGPRINT(Level, fmt, args...) 	
#endif

struct ieee8023_hdr {
	u8 dAddr[6];
	u8 sAddr[6];
	u16 eth_type;
} __attribute__ ((packed));

extern unsigned char rfc1042_header[6];

typedef struct apd_data {
	struct rtapd_config *conf;
	char *config_fname;

	int sock[MAX_MSSID_NUM]; /* raw packet socket for driver access */
	int ioctl_sock; /* socket for ioctl() use */
	u8 own_addr[6];

	int num_sta; /* number of entries in sta_list */
	struct sta_info *sta_list; /* STA info list head */
	struct sta_info *sta_hash[STA_HASH_SIZE];

	/* pointers to STA info; based on allocated AID or NULL if AID free
	 * AID is in the range 1-2007, so sta_aid[0] corresponders to AID 1
	 * and so on
	 */
	struct sta_info *sta_aid[MAX_AID_TABLE_SIZE];

	struct radius_client_data *radius;

} rtapd;

typedef struct recv_from_ra {
    u8 daddr[6];
    u8 saddr[6];
    u8 ethtype[2];
    u8 xframe[1];    
} priv_rec;

u16	RTMPCompareMemory(void *pSrc1,void *pSrc2, u16 Length);
void Handle_term(int sig, void *eloop_ctx, void *signal_ctx);
int OidQueryInformation(u16 OidQueryCode, int socket_id, char *DeviceName, void *ptr, u32 PtrLength);
int OidSetInformation(u16 OidQueryCode, int socket_id, char *DeviceName, void *ptr, u32 PtrLength);
int RT_ioctl(rtapd *rtapd, int code, int param, char  *data, int data_len, unsigned char apidx);

#endif /* HOSTAPD_H */
