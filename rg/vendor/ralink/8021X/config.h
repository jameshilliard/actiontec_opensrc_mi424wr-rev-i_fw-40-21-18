#ifndef CONFIG_H
#define CONFIG_H

typedef u8 macaddr[ETH_ALEN];

struct hostapd_radius_server {
	struct in_addr addr;
	int port;
	u8 *shared_secret;
	size_t shared_secret_len;
};

struct rtapd_config {
	char iface[MAX_MSSID_NUM][IFNAMSIZ + 1];

	int SsidNum;
	char IEEE8021X_ikey[WEP8021X_KEY_LEN];
	int individual_wep_key_len;
#define HOSTAPD_MODULE_IEEE80211 BIT(0)
#define HOSTAPD_MODULE_IEEE8021X BIT(1)
#define HOSTAPD_MODULE_RADIUS BIT(2)

	enum { HOSTAPD_DEBUG_NO = 0, HOSTAPD_DEBUG_MINIMAL = 1,
	       HOSTAPD_DEBUG_VERBOSE = 2,
	       HOSTAPD_DEBUG_MSGDUMPS = 3 } debug; /* debug verbosity level */
	int daemonize; /* fork into background */

	struct in_addr own_ip_addr;
	/* RADIUS Authentication and Accounting servers in priority order */
	struct hostapd_radius_server *auth_servers, *auth_server;
	int num_auth_servers;

	int radius_retry_primary_interval;

#define HOSTAPD_AUTH_OPEN BIT(0)
#define HOSTAPD_AUTH_SHARED_KEY BIT(1)
	int session_timeout_set;
	int session_timeout_interval;
};


struct rtapd_config * Config_read(const char *fname, int pid);
void Config_free(struct rtapd_config *conf);


#endif /* CONFIG_H */
