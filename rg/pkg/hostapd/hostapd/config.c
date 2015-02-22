/*
 * hostapd / Configuration file
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"
#ifndef CONFIG_NATIVE_WINDOWS
#include <grp.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "hostapd.h"
#include "driver.h"
#include "sha1.h"
#include "eap.h"
#include "radius_client.h"
#include "wpa_common.h"
#include "common.h"

#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
#include "wps_config.h"
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

#include "config_rewrite.h"

#define MAX_STA_COUNT 2007


static int hostapd_config_read_vlan_file(struct hostapd_bss_config *bss,
					 const char *fname)
{
	FILE *f;
	char buf[128], *pos, *pos2;
	int line = 0, vlan_id;
	struct hostapd_vlan *vlan;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "VLAN file '%s' not readable.\n", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

                pos = buf+strlen(buf);
                while (pos > buf && !isgraph(pos[-1])) *--pos = '\0';
		if (pos == buf || buf[0] == '#')
			continue;

		if (buf[0] == '*') {
			vlan_id = VLAN_ID_WILDCARD;
			pos = buf + 1;
		} else {
			vlan_id = strtol(buf, &pos, 10);
			if (buf == pos || vlan_id < 1 ||
			    vlan_id > MAX_VLAN_ID) {
				wpa_printf(MSG_ERROR,
                                        "Invalid VLAN ID at line %d in '%s'\n",
				       line, fname);
				fclose(f);
				return -1;
			}
		}

		while (*pos == ' ' || *pos == '\t')
			pos++;
		pos2 = pos;
		while (*pos2 != ' ' && *pos2 != '\t' && *pos2 != '\0')
			pos2++;
		*pos2 = '\0';
		if (*pos == '\0' || strlen(pos) > IFNAMSIZ) {
			wpa_printf(MSG_ERROR,
                                "Invalid VLAN ifname at line %d in '%s'\n",
			       line, fname);
			fclose(f);
			return -1;
		}

		vlan = malloc(sizeof(*vlan));
		if (vlan == NULL) {
			wpa_printf(MSG_ERROR,
                                "Out of memory while reading VLAN interfaces "
			       "from '%s'\n", fname);
			fclose(f);
			return -1;
		}

		memset(vlan, 0, sizeof(*vlan));
		vlan->vlan_id = vlan_id;
		strncpy(vlan->ifname, pos, sizeof(vlan->ifname));
		if (bss->vlan_tail)
			bss->vlan_tail->next = vlan;
		else
			bss->vlan = vlan;
		bss->vlan_tail = vlan;
	}

	fclose(f);

	return 0;
}


static void hostapd_config_free_vlan(struct hostapd_bss_config *bss)
{
	struct hostapd_vlan *vlan, *prev;

	vlan = bss->vlan;
	prev = NULL;
	while (vlan) {
		prev = vlan;
		vlan = vlan->next;
		free(prev);
	}

	bss->vlan = NULL;
}


/* convert floats with one decimal place to value*10 int, i.e.,
 * "1.5" will return 15 */
static int hostapd_config_read_int10(const char *value)
{
	int i, d;
	char *pos;

	i = atoi(value);
	pos = strchr(value, '.');
	d = 0;
	if (pos) {
		pos++;
		if (*pos >= '0' && *pos <= '9')
			d = *pos - '0';
	}

	return i * 10 + d;
}


static void hostapd_config_defaults_bss(struct hostapd_bss_config *bss)
{
	bss->logger_syslog_level = HOSTAPD_LEVEL_INFO;
	bss->logger_stdout_level = HOSTAPD_LEVEL_INFO;
	bss->logger_syslog = (unsigned int) -1;
	bss->logger_stdout = (unsigned int) -1;

	bss->auth_algs = HOSTAPD_AUTH_OPEN | HOSTAPD_AUTH_SHARED_KEY;

	bss->wep_rekeying_period = 300;
	/* use key0 in individual key and key1 in broadcast key */
	bss->broadcast_key_idx_min = 1;
	bss->broadcast_key_idx_max = 2;
	bss->eap_reauth_period = 3600;

	bss->wpa_group_rekey = 600;
	bss->wpa_gmk_rekey = 86400;
	bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
	bss->wpa_pairwise = WPA_CIPHER_TKIP;
	bss->wpa_group = WPA_CIPHER_TKIP;

	bss->max_num_sta = MAX_STA_COUNT;

	bss->dtim_period = 2;

	bss->radius_server_auth_port = 1812;
	bss->ap_max_inactivity = AP_MAX_INACTIVITY;
	bss->eapol_version = EAPOL_VERSION;
}



/* hostapd_radio_config_create -- allocate and initialize a new radio
 * control structure, with default values.
 * Returns NULL on error.
 */
struct hostapd_config *hostapd_radio_config_create(void)
{
	const int aCWmin = 15, aCWmax = 1024;
	const struct hostapd_wme_ac_params ac_bk =
		{ aCWmin, aCWmax, 7, 0, 0 }; /* background traffic */
	const struct hostapd_wme_ac_params ac_be =
		{ aCWmin, aCWmax, 3, 0, 0 }; /* best effort traffic */
	const struct hostapd_wme_ac_params ac_vi = /* video traffic */
		{ aCWmin >> 1, aCWmin, 2, 3000 / 32, 1 };
	const struct hostapd_wme_ac_params ac_vo = /* voice traffic */
		{ aCWmin >> 2, aCWmin >> 1, 2, 1500 / 32, 1 };

	struct hostapd_config *conf;
        int i;

	conf = wpa_zalloc(sizeof(*conf));
	if (conf == NULL) {
		wpa_printf(MSG_ERROR,
                        "Failed to allocate memory for configuration data.\n");
		return NULL;
	}

	/* set default driver based on configuration */
	conf->driver = driver_lookup("default");
	if (conf->driver == NULL) {
		wpa_printf(MSG_ERROR, "No default driver registered!\n");
		free(conf);
		return NULL;
	}

	conf->beacon_int = 100;
	conf->rts_threshold = -1; /* use driver default: 2347 */
	conf->fragm_threshold = -1; /* user driver default: 2346 */
	conf->send_probe_response = 1;
	conf->bridge_packets = INTERNAL_BRIDGE_DO_NOT_CONTROL;

	memcpy(conf->country, "US ", 3);

	for (i = 0; i < NUM_TX_QUEUES; i++)
		conf->tx_queue[i].aifs = -1; /* use hw default */

	conf->wme_ac_params[0] = ac_be;
	conf->wme_ac_params[1] = ac_bk;
	conf->wme_ac_params[2] = ac_vi;
	conf->wme_ac_params[3] = ac_vo;

	return conf;
}


#ifndef MODIFIED_BY_SONY /* move to hostapd.c to be global function */
static int hostapd_parse_ip_addr(const char *txt, struct hostapd_ip_addr *addr)
{
	if (inet_aton(txt, &addr->u.v4)) {
		addr->af = AF_INET;
		return 0;
	}

#ifdef CONFIG_IPV6
	if (inet_pton(AF_INET6, txt, &addr->u.v6) > 0) {
		addr->af = AF_INET6;
		return 0;
	}
#endif /* CONFIG_IPV6 */

	return -1;
}
#endif /* MODIFIED_BY_SONY */


int hostapd_mac_comp(const void *a, const void *b)
{
	return memcmp(a, b, sizeof(macaddr));
}


int hostapd_mac_comp_empty(const void *a)
{
	macaddr empty = { 0 };
	return memcmp(a, empty, sizeof(macaddr));
}


static int hostapd_config_read_maclist(const char *fname, macaddr **acl,
				       int *num)
{
	FILE *f;
	char buf[128], *pos;
	int line = 0;
	u8 addr[ETH_ALEN];
	macaddr *newacl;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "MAC list file '%s' not found.\n", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

                pos = buf+strlen(buf);
                while (pos > buf && !isgraph(pos[-1])) *--pos = '\0';
		if (pos == buf || buf[0] == '#')
			continue;

		if (hwaddr_aton(buf, addr)) {
			wpa_printf(MSG_ERROR, 
                                "Invalid MAC address '%s' at line %d in '%s'\n",
			       buf, line, fname);
			fclose(f);
			return -1;
		}

		newacl = (macaddr *) realloc(*acl, (*num + 1) * ETH_ALEN);
		if (newacl == NULL) {
			wpa_printf(MSG_ERROR, "MAC list reallocation failed\n");
			fclose(f);
			return -1;
		}

		*acl = newacl;
		memcpy((*acl)[*num], addr, ETH_ALEN);
		(*num)++;
	}

	fclose(f);

	qsort(*acl, *num, sizeof(macaddr), hostapd_mac_comp);

	return 0;
}


static int hostapd_config_read_wpa_psk(const char *fname,
				       struct hostapd_ssid *ssid)
{
	FILE *f;
	char buf[128], *pos;
	int line = 0, ret = 0, len, ok;
	u8 addr[ETH_ALEN];
	struct hostapd_wpa_psk *psk;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "WPA PSK file '%s' not found.\n", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

                pos = buf+strlen(buf);
                while (pos > buf && !isgraph(pos[-1])) *--pos = '\0';
		if (pos == buf || buf[0] == '#')
			continue;

		if (hwaddr_aton(buf, addr)) {
			wpa_printf(MSG_ERROR, "Invalid MAC address '%s' on line %d in '%s'\n",
			       buf, line, fname);
			ret = -1;
			break;
		}

		psk = wpa_zalloc(sizeof(*psk));
		if (psk == NULL) {
			wpa_printf(MSG_ERROR, "WPA PSK allocation failed\n");
			ret = -1;
			break;
		}
		if (memcmp(addr, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0)
			psk->group = 1;
		else
			memcpy(psk->addr, addr, ETH_ALEN);

		pos = buf + 17;
		if (pos == '\0') {
			wpa_printf(MSG_ERROR, "No PSK on line %d in '%s'\n", line, fname);
			free(psk);
			ret = -1;
			break;
		}
		pos++;

		ok = 0;
		len = strlen(pos);
		if (len == 64 && hexstr2bin(pos, psk->psk, PMK_LEN) == 0)
			ok = 1;
		else if (len >= 8 && len < 64) {
			pbkdf2_sha1(pos, ssid->ssid, ssid->ssid_len,
				    4096, psk->psk, PMK_LEN);
			ok = 1;
		}
		if (!ok) {
			wpa_printf(MSG_ERROR, "Invalid PSK '%s' on line %d in '%s'\n",
			       pos, line, fname);
			free(psk);
			ret = -1;
			break;
		}

		psk->next = ssid->wpa_psk;
		ssid->wpa_psk = psk;
	}

	fclose(f);

	return ret;
}


int hostapd_setup_wpa_psk(struct hostapd_bss_config *conf)
{
	struct hostapd_ssid *ssid = &conf->ssid;

	if (ssid->wpa_passphrase != NULL) {
		if (ssid->wpa_psk != NULL) {
			wpa_printf(MSG_ERROR, 
                                "Warning: both WPA PSK and passphrase set. "
			       "Using passphrase.\n");
			free(ssid->wpa_psk);
		}
		ssid->wpa_psk = wpa_zalloc(sizeof(struct hostapd_wpa_psk));
		if (ssid->wpa_psk == NULL) {
			wpa_printf(MSG_ERROR, "Unable to alloc space for PSK\n");
			return -1;
		}
		wpa_hexdump_ascii(MSG_DEBUG, "SSID",
				  (u8 *) ssid->ssid, ssid->ssid_len);
		wpa_hexdump_ascii(MSG_DEBUG, "PSK (ASCII passphrase)",
				  (u8 *) ssid->wpa_passphrase,
				  strlen(ssid->wpa_passphrase));
		pbkdf2_sha1(ssid->wpa_passphrase,
			    ssid->ssid, ssid->ssid_len,
			    4096, ssid->wpa_psk->psk, PMK_LEN);
		wpa_hexdump(MSG_DEBUG, "PSK (from passphrase)",
			    ssid->wpa_psk->psk, PMK_LEN);
		ssid->wpa_psk->group = 1;

#ifndef EAP_WPS
		memset(ssid->wpa_passphrase, 0,
		       os_strlen(ssid->wpa_passphrase));
		free(ssid->wpa_passphrase);
		ssid->wpa_passphrase = NULL;
#endif /* EAP_WPS */
	}

	if (ssid->wpa_psk_file) {
		if (hostapd_config_read_wpa_psk(ssid->wpa_psk_file,
						&conf->ssid))
			return -1;
		free(ssid->wpa_psk_file);
		ssid->wpa_psk_file = NULL;
	}

	return 0;
}


#ifdef EAP_SERVER
static int hostapd_config_read_eap_user(const char *fname,
					struct hostapd_bss_config *conf)
{
	FILE *f;
	char buf[512], *pos, *start, *pos2;
	int line = 0, ret = 0, num_methods;
	struct hostapd_eap_user *user, *tail = NULL;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "EAP user file '%s' not found.\n", fname);
		return -1;
	}

	/* Lines: "user" METHOD,METHOD2 "password" (password optional) */
	while (fgets(buf, sizeof(buf), f)) {
		line++;

                pos = buf+strlen(buf);
                while (pos > buf && !isgraph(pos[-1])) *--pos = '\0';
		if (pos == buf || buf[0] == '#')
			continue;

		user = NULL;

		if (buf[0] != '"' && buf[0] != '*') {
			wpa_printf(MSG_ERROR, 
                                "Invalid EAP identity (no \" in start) on "
			       "line %d in '%s'\n", line, fname);
			goto failed;
		}

		user = wpa_zalloc(sizeof(*user));
		if (user == NULL) {
			wpa_printf(MSG_ERROR, "EAP user allocation failed\n");
			goto failed;
		}
		user->force_version = -1;

		if (buf[0] == '*') {
			pos = buf;
		} else {
			pos = buf + 1;
			start = pos;
			while (*pos != '"' && *pos != '\0')
				pos++;
			if (*pos == '\0') {
				wpa_printf(MSG_ERROR,
                                        "Invalid EAP identity (no \" in end) on"
				       " line %d in '%s'\n", line, fname);
				goto failed;
			}

			user->identity = malloc(pos - start);
			if (user->identity == NULL) {
				wpa_printf(MSG_ERROR,
                                        "Failed to allocate memory for EAP "
				       "identity\n");
				goto failed;
			}
			memcpy(user->identity, start, pos - start);
			user->identity_len = pos - start;

			if (pos[0] == '"' && pos[1] == '*') {
				user->wildcard_prefix = 1;
				pos++;
			}
		}
		pos++;
		while (*pos == ' ' || *pos == '\t')
			pos++;

		if (*pos == '\0') {
			wpa_printf(MSG_ERROR,
                                "No EAP method on line %d in '%s'\n",
			       line, fname);
			goto failed;
		}

		start = pos;
		while (*pos != ' ' && *pos != '\t' && *pos != '\0')
			pos++;
		if (*pos == '\0') {
			pos = NULL;
		} else {
			*pos = '\0';
			pos++;
		}
		num_methods = 0;
		while (*start) {
			char *pos3 = strchr(start, ',');
			if (pos3) {
				*pos3++ = '\0';
			}
			user->methods[num_methods].method =
				eap_get_type(start, &user->methods[num_methods]
					     .vendor);
			if (user->methods[num_methods].vendor ==
			    EAP_VENDOR_IETF &&
			    user->methods[num_methods].method == EAP_TYPE_NONE)
			{
				wpa_printf(MSG_ERROR,
                                        "Unsupported EAP type '%s' on line %d "
				       "in '%s'\n", start, line, fname);
				goto failed;
			}

			num_methods++;
			if (num_methods >= EAP_USER_MAX_METHODS)
				break;
			if (pos3 == NULL)
				break;
			start = pos3;
		}
		if (num_methods == 0) {
			wpa_printf(MSG_ERROR,
                                "No EAP types configured on line %d in '%s'\n",
			       line, fname);
			goto failed;
		}

		if (pos == NULL)
			goto done;

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (*pos == '\0')
			goto done;

		if (strncmp(pos, "[ver=0]", 7) == 0) {
			user->force_version = 0;
			goto done;
		}

		if (strncmp(pos, "[ver=1]", 7) == 0) {
			user->force_version = 1;
			goto done;
		}

		if (strncmp(pos, "[2]", 3) == 0) {
			user->phase2 = 1;
			goto done;
		}

		if (*pos == '"') {
			pos++;
			start = pos;
			while (*pos != '"' && *pos != '\0')
				pos++;
			if (*pos == '\0') {
				wpa_printf(MSG_ERROR,
                                        "Invalid EAP password (no \" in end) "
				       "on line %d in '%s'\n", line, fname);
				goto failed;
			}

			user->password = malloc(pos - start);
			if (user->password == NULL) {
				wpa_printf(MSG_ERROR,
                                        "Failed to allocate memory for EAP "
				       "password\n");
				goto failed;
			}
			memcpy(user->password, start, pos - start);
			user->password_len = pos - start;

			pos++;
		} else if (strncmp(pos, "hash:", 5) == 0) {
			pos += 5;
			pos2 = pos;
			while (*pos2 != '\0' && *pos2 != ' ' &&
			       *pos2 != '\t' && *pos2 != '#')
				pos2++;
			if (pos2 - pos != 32) {
				wpa_printf(MSG_ERROR,
                                        "Invalid password hash on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password = malloc(16);
			if (user->password == NULL) {
				wpa_printf(MSG_ERROR,
                                        "Failed to allocate memory for EAP "
				       "password hash\n");
				goto failed;
			}
			if (hexstr2bin(pos, user->password, 16) < 0) {
				wpa_printf(MSG_ERROR,
                                        "Invalid hash password on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password_len = 16;
			user->password_hash = 1;
			pos = pos2;
		} else {
			pos2 = pos;
			while (*pos2 != '\0' && *pos2 != ' ' &&
			       *pos2 != '\t' && *pos2 != '#')
				pos2++;
			if ((pos2 - pos) & 1) {
				wpa_printf(MSG_ERROR,
                                        "Invalid hex password on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password = malloc((pos2 - pos) / 2);
			if (user->password == NULL) {
				wpa_printf(MSG_ERROR,
                                        "Failed to allocate memory for EAP "
				       "password\n");
				goto failed;
			}
			if (hexstr2bin(pos, user->password,
				       (pos2 - pos) / 2) < 0) {
				wpa_printf(MSG_ERROR,
                                        "Invalid hex password on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password_len = (pos2 - pos) / 2;
			pos = pos2;
		}

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (strncmp(pos, "[2]", 3) == 0) {
			user->phase2 = 1;
		}

	done:
		if (tail == NULL) {
			tail = conf->eap_user = user;
		} else {
			tail->next = user;
			tail = user;
		}
		continue;

	failed:
		if (user) {
			free(user->password);
			free(user->identity);
			free(user);
		}
		ret = -1;
		break;
	}

	fclose(f);

	return ret;
}
#endif /* EAP_SERVER */


static int
hostapd_config_read_radius_addr(struct hostapd_radius_server **server,
				int *num_server, const char *val, int def_port,
				struct hostapd_radius_server **curr_serv)
{
	struct hostapd_radius_server *nserv;
	int ret;
	static int server_index = 1;

	nserv = realloc(*server, (*num_server + 1) * sizeof(*nserv));
	if (nserv == NULL)
		return -1;

	*server = nserv;
	nserv = &nserv[*num_server];
	(*num_server)++;
	(*curr_serv) = nserv;

	memset(nserv, 0, sizeof(*nserv));
	nserv->port = def_port;
	ret = hostapd_parse_ip_addr(val, &nserv->addr);
	nserv->index = server_index++;

	return ret;
}


static int hostapd_config_parse_key_mgmt(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (strcmp(start, "WPA-PSK") == 0)
			val |= WPA_KEY_MGMT_PSK;
		else if (strcmp(start, "WPA-EAP") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X;
		else {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid key_mgmt '%s'\n",
			       line, start);
			free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}

	free(buf);
	if (val == 0) {
		wpa_printf(MSG_INFO,
                        "Line %d: no key_mgmt values configured.\n", line);
		/* WAS: return -1;  */
	}

	return val;
}


static int hostapd_config_parse_cipher(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (strcmp(start, "CCMP") == 0)
			val |= WPA_CIPHER_CCMP;
		else if (strcmp(start, "TKIP") == 0)
			val |= WPA_CIPHER_TKIP;
		else if (strcmp(start, "WEP104") == 0)
			val |= WPA_CIPHER_WEP104;
		else if (strcmp(start, "WEP40") == 0)
			val |= WPA_CIPHER_WEP40;
		else if (strcmp(start, "NONE") == 0)
			val |= WPA_CIPHER_NONE;
		else {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid cipher '%s'.", line, start);
			free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	if (val == 0) {
		wpa_printf(MSG_ERROR,
                        "Line %d: no cipher values configured.", line);
		return -1;
	}
	return val;
}


static int hostapd_config_check_bss(struct hostapd_bss_config *bss,
				    struct hostapd_config *conf)
{
	if (bss->ieee802_1x && !bss->eap_server &&
	    !bss->radius->auth_servers) {
		wpa_printf(MSG_ERROR,
                        "Invalid IEEE 802.1X configuration (no EAP "
		       "authenticator configured).\n");
		return -1;
	}

	if (bss->wpa && (bss->wpa_key_mgmt & WPA_KEY_MGMT_PSK) &&
	    bss->ssid.wpa_psk == NULL && bss->ssid.wpa_passphrase == NULL &&
	    bss->ssid.wpa_psk_file == NULL) {
		wpa_printf(MSG_ERROR,
                        "WPA-PSK enabled, but PSK or passphrase is not "
		       "configured.\n");
		return -1;
	}

	if (hostapd_mac_comp_empty(bss->bssid) != 0) {
		size_t i;

		for (i = 0; i < conf->num_bss; i++) {
			if ((&conf->bss[i] != bss) &&
			    (hostapd_mac_comp(conf->bss[i].bssid,
					      bss->bssid) == 0)) {
				wpa_printf(MSG_ERROR,
                                        "Duplicate BSSID " MACSTR
				       " on interface '%s' and '%s'.\n",
				       MAC2STR(bss->bssid),
				       conf->bss[i].iface, bss->iface);
				return -1;
			}
		}
	}

	return 0;
}


static int hostapd_config_check(struct hostapd_config *conf)
{
	size_t i;

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_config_check_bss(&conf->bss[i], conf))
			return -1;
	}

	return 0;
}


static int hostapd_config_read_wep(struct hostapd_wep_keys *wep, int keyidx,
				   char *val)
{
	size_t len = strlen(val);

	if (keyidx < 0 || keyidx > 3) 
		return -1;

        os_free(wep->key[keyidx]);
        wep->key[keyidx] = NULL;
        wep->len[keyidx] = 0;

	if (val[0] == '"') {
		if (len < 2 || val[len - 1] != '"')
			return -1;
		len -= 2;
                if (len > 0) {
		wep->key[keyidx] = malloc(len);
		if (wep->key[keyidx] == NULL)
			return -1;
		memcpy(wep->key[keyidx], val + 1, len);
                }
	} else {
		if (len & 1)
			return -1;
		len /= 2;
                if (len > 0) {
		wep->key[keyidx] = malloc(len);
		if (wep->key[keyidx] == NULL)
			return -1;
		        if (hexstr2bin(val, wep->key[keyidx], len) < 0) {
                                os_free(wep->key[keyidx]);
                                wep->key[keyidx] = NULL;
			return -1;
	}
                }
	}
	wep->len[keyidx] = len;

        /* Determine if any wep keys are set ... if none then
         * we may be in totally open mode...
         */
	wep->keys_set = 0;
        for (keyidx = 0; keyidx < 4; keyidx++) {
                if (wep->len[keyidx]) 
	                wep->keys_set = 1;
        }
	return 0;
}


static int hostapd_parse_rates(int **rate_list, char *val)
{
	int *list;
	int count;
	char *pos, *end;

	free(*rate_list);
	*rate_list = NULL;

	pos = val;
	count = 0;
	while (*pos != '\0') {
		if (*pos == ' ')
			count++;
		pos++;
	}

	list = malloc(sizeof(int) * (count + 2));
	if (list == NULL)
		return -1;
	pos = val;
	count = 0;
	while (*pos != '\0') {
		end = strchr(pos, ' ');
		if (end)
			*end = '\0';

		list[count++] = atoi(pos);
		if (!end)
			break;
		pos = end + 1;
	}
	list[count] = -1;

	*rate_list = list;
	return 0;
}


/* hostapd_config_bss -- add another BSS structure to radio configuration
 * and give it default values.
 */
int hostapd_config_bss(struct hostapd_config *conf, const char *ifname)
{
	struct hostapd_bss_config *bss;

	if (*ifname == '\0')
		return -1;

	bss = realloc(conf->bss, (conf->num_bss + 1) *
		      sizeof(struct hostapd_bss_config));
	if (bss == NULL) {
		wpa_printf(MSG_ERROR,
                        "Failed to allocate memory for multi-BSS entry\n");
		return -1;
	}
	conf->bss = bss;

	bss = &(conf->bss[conf->num_bss]);
	memset(bss, 0, sizeof(*bss));
	bss->radius = wpa_zalloc(sizeof(*bss->radius));
	if (bss->radius == NULL) {
		wpa_printf(MSG_ERROR,
                        "Failed to allocate memory for BSS RADIUS data\n");
		return -1;
	}

	conf->num_bss++;
	conf->last_bss = bss;

	hostapd_config_defaults_bss(bss);
	snprintf(bss->iface, sizeof(bss->iface), "%s", ifname);
	memcpy(bss->ssid.vlan, bss->iface, IFNAMSIZ + 1);

        #if defined(EAP_WPS) && !defined(USE_INTEL_SDK)
        bss->wps = wpa_zalloc(sizeof(*bss->wps));
        if (bss->wps == NULL) {
		wpa_printf(MSG_ERROR,
                        "Failed to allocate memory for WPS config\n");
		return -1;
        }
        #endif

	return 0;
}



static int valid_cw(int cw)
{
	return (cw == 1 || cw == 3 || cw == 7 || cw == 15 || cw == 31 ||
		cw == 63 || cw == 127 || cw == 255 || cw == 511 || cw == 1023);
}


enum {
	IEEE80211_TX_QUEUE_DATA0 = 0, /* used for EDCA AC_VO data */
	IEEE80211_TX_QUEUE_DATA1 = 1, /* used for EDCA AC_VI data */
	IEEE80211_TX_QUEUE_DATA2 = 2, /* used for EDCA AC_BE data */
	IEEE80211_TX_QUEUE_DATA3 = 3, /* used for EDCA AC_BK data */
	IEEE80211_TX_QUEUE_DATA4 = 4,
	IEEE80211_TX_QUEUE_AFTER_BEACON = 6,
	IEEE80211_TX_QUEUE_BEACON = 7
};

static int hostapd_config_tx_queue(struct hostapd_config *conf, char *name,
				   char *val)
{
	int num;
	char *pos;
	struct hostapd_tx_queue_params *queue;

	/* skip 'tx_queue_' prefix */
	pos = name + 9;
	if (strncmp(pos, "data", 4) == 0 &&
	    pos[4] >= '0' && pos[4] <= '9' && pos[5] == '_') {
		num = pos[4] - '0';
		pos += 6;
	} else if (strncmp(pos, "after_beacon_", 13) == 0) {
		num = IEEE80211_TX_QUEUE_AFTER_BEACON;
		pos += 13;
	} else if (strncmp(pos, "beacon_", 7) == 0) {
		num = IEEE80211_TX_QUEUE_BEACON;
		pos += 7;
	} else {
		wpa_printf(MSG_ERROR,
                        "Unknown tx_queue name '%s'\n", pos);
		return -1;
	}

	queue = &conf->tx_queue[num];

	if (strcmp(pos, "aifs") == 0) {
		queue->aifs = atoi(val);
		if (queue->aifs < 0 || queue->aifs > 255) {
			wpa_printf(MSG_ERROR,
                                "Invalid AIFS value %d\n", queue->aifs);
			return -1;
		}
	} else if (strcmp(pos, "cwmin") == 0) {
		queue->cwmin = atoi(val);
		if (!valid_cw(queue->cwmin)) {
			wpa_printf(MSG_ERROR,
                                "Invalid cwMin value %d\n", queue->cwmin);
			return -1;
		}
	} else if (strcmp(pos, "cwmax") == 0) {
		queue->cwmax = atoi(val);
		if (!valid_cw(queue->cwmax)) {
			wpa_printf(MSG_ERROR,
                                "Invalid cwMax value %d\n", queue->cwmax);
			return -1;
		}
	} else if (strcmp(pos, "burst") == 0) {
		queue->burst = hostapd_config_read_int10(val);
	} else {
		wpa_printf(MSG_ERROR,
                        "Unknown tx_queue field '%s'\n", pos);
		return -1;
	}

	queue->configured = 1;

	return 0;
}


static int hostapd_config_wme_ac(struct hostapd_config *conf, char *name,
				   char *val)
{
	int num, v;
	char *pos;
	struct hostapd_wme_ac_params *ac;

	/* skip 'wme_ac_' prefix */
	pos = name + 7;
	if (strncmp(pos, "be_", 3) == 0) {
		num = 0;
		pos += 3;
	} else if (strncmp(pos, "bk_", 3) == 0) {
		num = 1;
		pos += 3;
	} else if (strncmp(pos, "vi_", 3) == 0) {
		num = 2;
		pos += 3;
	} else if (strncmp(pos, "vo_", 3) == 0) {
		num = 3;
		pos += 3;
	} else {
		wpa_printf(MSG_ERROR,
                        "Unknown wme name '%s'\n", pos);
		return -1;
	}

	ac = &conf->wme_ac_params[num];

	if (strcmp(pos, "aifs") == 0) {
		v = atoi(val);
		if (v < 1 || v > 255) {
			wpa_printf(MSG_ERROR,
                                "Invalid AIFS value %d\n", v);
			return -1;
		}
		ac->aifs = v;
	} else if (strcmp(pos, "cwmin") == 0) {
		v = atoi(val);
		if (v < 0 || v > 12) {
			wpa_printf(MSG_ERROR,
                                "Invalid cwMin value %d\n", v);
			return -1;
		}
		ac->cwmin = v;
	} else if (strcmp(pos, "cwmax") == 0) {
		v = atoi(val);
		if (v < 0 || v > 12) {
			wpa_printf(MSG_ERROR,
                                "Invalid cwMax value %d\n", v);
			return -1;
		}
		ac->cwmax = v;
	} else if (strcmp(pos, "txop_limit") == 0) {
		v = atoi(val);
		if (v < 0 || v > 0xffff) {
			wpa_printf(MSG_ERROR,
                                "Invalid txop value %d\n", v);
			return -1;
		}
		ac->txopLimit = v;
	} else if (strcmp(pos, "acm") == 0) {
		v = atoi(val);
		if (v < 0 || v > 1) {
			wpa_printf(MSG_ERROR,
                                "Invalid acm value %d\n", v);
			return -1;
		}
		ac->admission_control_mandatory = v;
	} else {
		wpa_printf(MSG_ERROR,
                        "Unknown wme_ac_ field '%s'\n", pos);
		return -1;
	}

	return 0;
}


#ifdef EAP_WPS
#ifndef USE_INTEL_SDK


/**
 * hostapd_config_free_wps_config - Free wps configuration
 * @wps: Pointer to wps configuration to be freed
 */
void hostapd_config_free_wps_config(struct wps_config *wps)
{
	if (wps) {
                os_free(wps->default_pin);
		os_free(wps->manufacturer);
		os_free(wps->model_name);
		os_free(wps->model_number);
		os_free(wps->serial_number);
                os_free(wps->friendly_name);
                os_free(wps->manufacturer_url);
                os_free(wps->model_description);
                os_free(wps->model_url);
                os_free(wps->upc_string);
		os_free(wps->dev_name);
		wps_config_free_dh(&wps->dh_secret);
		os_free(wps->config);
		os_free(wps->upnp_root_dir);
		os_free(wps->upnp_desc_url);
		os_free(wps->upnp_iface);
		os_free(wps);
	}
}


#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */



/* hostapd_config_line_lex breaks up a configuration input line:
 *      -- empty lines, or with only a #... comment result in no error
 *              but result in return of empty string.
 *      -- lines of form tag=value are broken up; whitespace before
 *              and after tag and before and after value is discarded,
 *              but otherwise retained inside of value.
 *      -- other lines result in NULL return.
 *
 *      The tag pointer is the return value.
 */
char * hostapd_config_line_lex(
        char *buf,      /* input: modified as storage for results */
        char **value_out        /* output: pointer to value (null term) */
        )
{
        char *pos;
        char *value;

        /* Trim leading whitespace, including comment lines */
        for (pos = buf; ; pos++) {
                if (*pos == 0)  {
                        *value_out = pos;
                        return pos;
                }
                if (*pos == '\n' || *pos == '\r' || *pos == '#') {
                        *pos = 0;
                        *value_out = pos;
                        return pos;
                }
                buf = pos;
                if (isgraph(*pos)) break;
        }
        while (isgraph(*pos) && *pos != '=') pos++;
        if (*pos == '=') {
                *pos++ = 0;     /* null terminate the tag */
                *value_out = value = pos;
        } else {
                return NULL;
        }
        /* Trim trailing whitepace. Spaces inside of a value are allowed,
         * as are other arbitrary non-white text, thus no comments on
         * end of lines.
         */
        for (pos += strlen(pos); --pos >= value; ) {
                if (isgraph(*pos)) break;
                *pos = 0;
        }
        return buf;
}



/* hostapd_radio_config_apply_line -- apply a configuration line 
 * (e.g. from a configuration file) to a given radio configuration.
 * Later values override earlier ones, for the same field.
 */
int hostapd_radio_config_apply_line(
        struct hostapd_config /*radio config*/ *conf,
        char *tag,
        char *value,
        int line        /* for diagnostics */
        )
{
        int errors = 0;

        if (strcmp(tag, "ignore_file_errors") == 0) {
		conf->ignore_file_errors = atol(value);
	} else if (strcmp(tag, "driver") == 0) {
		conf->driver = driver_lookup(value);
		if (conf->driver == NULL) {
			wpa_printf(MSG_ERROR,
                                "invalid/unknown driver '%s'\n", value);
			errors++;
		}
	} else if (strcmp(tag, "country_code") == 0) {
		memcpy(conf->country, value, 2);
		/* FIX: make this configurable */
		conf->country[2] = ' ';
	} else if (strcmp(tag, "ieee80211d") == 0) {
		conf->ieee80211d = atoi(value);
	} else if (strcmp(tag, "ieee80211h") == 0) {
		conf->ieee80211h = atoi(value);
	} else if (strcmp(tag, "hw_mode") == 0) {
		if (strcmp(value, "a") == 0)
			conf->hw_mode = HOSTAPD_MODE_IEEE80211A;
		else if (strcmp(value, "b") == 0)
			conf->hw_mode = HOSTAPD_MODE_IEEE80211B;
		else if (strcmp(value, "g") == 0)
			conf->hw_mode = HOSTAPD_MODE_IEEE80211G;
		else {
			wpa_printf(MSG_ERROR,
                                "Line %d: unknown hw_mode '%s'\n",
			       line, value);
			errors++;
		}
	} else if (strcmp(tag, "channel") == 0) {
		conf->channel = atoi(value);
	} else if (strcmp(tag, "beacon_int") == 0) {
		int val = atoi(value);
		/* MIB defines range as 1..65535, but very small values
		 * cause problems with the current implementation.
		 * Since it is unlikely that this small numbers are
		 * useful in real life scenarios, do not allow beacon
		 * period to be set below 15 TU. */
		if (val < 15 || val > 65535) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid beacon_int %d "
			       "(expected 15..65535)\n",
			       line, val);
			errors++;
		} else
			conf->beacon_int = val;
	} else if (strcmp(tag, "rts_threshold") == 0) {
		conf->rts_threshold = atoi(value);
		if (conf->rts_threshold < 0 ||
		    conf->rts_threshold > 2347) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid rts_threshold %d\n",
			       line, conf->rts_threshold);
			errors++;
		}
	} else if (strcmp(tag, "fragm_threshold") == 0) {
		conf->fragm_threshold = atoi(value);
		if (conf->fragm_threshold < 256 ||
		    conf->fragm_threshold > 2346) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid fragm_threshold %d\n",
			       line, conf->fragm_threshold);
			errors++;
		}
	} else if (strcmp(tag, "send_probe_response") == 0) {
		int val = atoi(value);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid send_probe_response "
			       "%d (expected 0 or 1)\n", line, val);
		} else
			conf->send_probe_response = val;
	} else if (strcmp(tag, "supported_rates") == 0) {
		if (hostapd_parse_rates(&conf->supported_rates, value)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid rate list\n", line);
			errors++;
		}
	} else if (strcmp(tag, "basic_rates") == 0) {
		if (hostapd_parse_rates(&conf->basic_rates, value)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid rate list\n", line);
			errors++;
		}
	} else if (strcmp(tag, "bridge_packets") == 0) {
		conf->bridge_packets = atoi(value);
	} else if (strcmp(tag, "passive_scan_interval") == 0) {
		conf->passive_scan_interval = atoi(value);
	} else if (strcmp(tag, "passive_scan_listen") == 0) {
		conf->passive_scan_listen = atoi(value);
	} else if (strcmp(tag, "passive_scan_mode") == 0) {
		conf->passive_scan_mode = atoi(value);
	} else if (strcmp(tag, "ap_table_max_size") == 0) {
		conf->ap_table_max_size = atoi(value);
	} else if (strcmp(tag, "ap_table_expiration_time") == 0) {
		conf->ap_table_expiration_time = atoi(value);
	} else if (strncmp(tag, "tx_queue_", 9) == 0) {
		if (hostapd_config_tx_queue(conf, tag, value)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid TX queue item\n",
			       line);
			errors++;
		}
	} else if (strncmp(tag, "wme_ac_", 7) == 0) {
		if (hostapd_config_wme_ac(conf, tag, value)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid wme ac item\n",
			       line);
			errors++;
		}
	} else {
		wpa_printf(MSG_ERROR,
                        "Line %d: unknown radio configuration item '%s'\n",
		       line, tag);
                errors++;
	}
        return errors;
}


/* hostapd_bss_config_apply_line -- apply a configuration line 
 * (e.g. from a configuration file) to a given bss configuration.
 * Later values override earlier ones, for the same field.
 */
int hostapd_bss_config_apply_line(
        struct hostapd_bss_config *bss,
        char *tag,
        char *value,
        int line,       /* for diagnostics */
        int internal    /* if nonzero, avoid some side effects */
        )
{
        int errors = 0;

        if (strcmp(tag, "ignore_file_errors") == 0) {
		bss->ignore_file_errors = atol(value);
        } else if (strcmp(tag, "interface") == 0) {
		snprintf(bss->iface, sizeof(bss->iface), "%s", value);
	} else if (strcmp(tag, "bridge") == 0) {
                /* Special bridge name "none" used when no bridge! */
                if (!strcmp(value,"none")) {
                        memset(bss->bridge, 0, sizeof(bss->bridge));
                } else
		snprintf(bss->bridge, sizeof(bss->bridge), "%s", value);
	} else if (strcmp(tag, "debug") == 0) {
		bss->debug = atoi(value);
	} else if (strcmp(tag, "logger_syslog_level") == 0) {
		bss->logger_syslog_level = atoi(value);
	} else if (strcmp(tag, "logger_stdout_level") == 0) {
		bss->logger_stdout_level = atoi(value);
	} else if (strcmp(tag, "logger_syslog") == 0) {
		bss->logger_syslog = atoi(value);
	} else if (strcmp(tag, "logger_stdout") == 0) {
		bss->logger_stdout = atoi(value);
	} else if (strcmp(tag, "dump_file") == 0) {
                free(bss->dump_log_name);
		if ((bss->dump_log_name = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "ssid") == 0) {
		bss->ssid.ssid_len = strlen(value);
		if (bss->ssid.ssid_len > HOSTAPD_MAX_SSID_LEN ||
		    bss->ssid.ssid_len < 1) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid SSID '%s'\n", line,
			       value);
			errors++;
		} else {
			if (!strcasecmp(value,"any")) {
				bss->ssid.ssid_len = 1;
				bss->ssid.ssid[0] = '\0';
			} else {
				memcpy(bss->ssid.ssid, value,
					bss->ssid.ssid_len);
			}
			bss->ssid.ssid[bss->ssid.ssid_len] = '\0';
			bss->ssid.ssid_set = 1;
		}
	} else if (strcmp(tag, "macaddr_acl") == 0) {
		bss->macaddr_acl = atoi(value);
		if (bss->macaddr_acl != ACCEPT_UNLESS_DENIED &&
		    bss->macaddr_acl != DENY_UNLESS_ACCEPTED &&
		    bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
			wpa_printf(MSG_ERROR,
                                "Line %d: unknown macaddr_acl %d\n",
			       line, bss->macaddr_acl);
		}
	} else if (strcmp(tag, "accept_mac_file") == 0) {
		if (hostapd_config_read_maclist(value, &bss->accept_mac,
						&bss->num_accept_mac))
		{
			wpa_printf(MSG_ERROR,
                                "Line %d: Failed to read "
			       "accept_mac_file '%s'\n",
			       line, value);
			errors++;
		}
	} else if (strcmp(tag, "deny_mac_file") == 0) {
		if (hostapd_config_read_maclist(value, &bss->deny_mac,
						&bss->num_deny_mac))
		{
			wpa_printf(MSG_ERROR,
                                "Line %d: Failed to read "
			       "deny_mac_file '%s'\n",
			       line, value);
			errors++;
		}
	} else if (strcmp(tag, "ap_max_inactivity") == 0) {
		bss->ap_max_inactivity = atoi(value);
	} else if (strcmp(tag, "assoc_ap_addr") == 0) {
		if (hwaddr_aton(value, bss->assoc_ap_addr)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid MAC address '%s'\n",
			       line, value);
			errors++;
		}
		bss->assoc_ap = 1;
	} else if (strcmp(tag, "ieee8021x") == 0) {
		bss->ieee802_1x = atoi(value);
	} else if (strcmp(tag, "eapol_version") == 0) {
		bss->eapol_version = atoi(value);
		if (bss->eapol_version < 1 ||
		    bss->eapol_version > 2) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid EAPOL "
			       "version (%d): '%s'.\n",
			       line, bss->eapol_version, value);
			errors++;
		} else
			wpa_printf(MSG_DEBUG, "eapol_version=%d",
				   bss->eapol_version);
        #ifdef EAP_SERVER
	} else if (strcmp(tag, "eap_authenticator") == 0) {
		bss->eap_server = atoi(value);
		wpa_printf(MSG_ERROR,
                        "Line %d: obsolete eap_authenticator used; "
		       "this has been renamed to eap_server\n", line);
	} else if (strcmp(tag, "eap_server") == 0) {
		bss->eap_server = atoi(value);
	} else if (strcmp(tag, "eap_user_file") == 0) {
		if (hostapd_config_read_eap_user(value, bss))
			errors++;
	} else if (strcmp(tag, "ca_cert") == 0) {
		free(bss->ca_cert);
		if ((bss->ca_cert = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "server_cert") == 0) {
		free(bss->server_cert);
		if ((bss->server_cert = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "private_key") == 0) {
		free(bss->private_key);
		if ((bss->private_key = strdup(value)) == NULL) 
                        goto malloc_failure;
	} else if (strcmp(tag, "private_key_passwd") == 0) {
		free(bss->private_key_passwd);
		if ((bss->private_key_passwd = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "check_crl") == 0) {
		bss->check_crl = atoi(value);
        #ifdef EAP_SIM
	} else if (strcmp(tag, "eap_sim_db") == 0) {
		free(bss->eap_sim_db);
		if ((bss->eap_sim_db = strdup(value)) == NULL)
                        goto malloc_failure;
        #endif /* EAP_SIM */
        #endif /* EAP_SERVER */
	} else if (strcmp(tag, "eap_message") == 0) {
		char *term;
                free(bss->eap_req_id_text);
		if ((bss->eap_req_id_text = strdup(value)) == NULL)
                        goto malloc_failure;
		bss->eap_req_id_text_len =
			strlen(bss->eap_req_id_text);
		term = strstr(bss->eap_req_id_text, "\\0");
		if (term) {
			*term++ = '\0';
			memmove(term, term + 1,
				bss->eap_req_id_text_len -
				(term - bss->eap_req_id_text) - 1);
			bss->eap_req_id_text_len--;
		}
	} else if (strcmp(tag, "wep_key_len_broadcast") == 0) {
		bss->default_wep_key_len = atoi(value);
		if (bss->default_wep_key_len > 13) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid WEP key len %lu "
			       "(= %lu bits)\n", line,
			       (unsigned long)
			       bss->default_wep_key_len,
			       (unsigned long)
			       bss->default_wep_key_len * 8);
			errors++;
		}
	} else if (strcmp(tag, "wep_key_len_unicast") == 0) {
		bss->individual_wep_key_len = atoi(value);
		if (bss->individual_wep_key_len < 0 ||
		    bss->individual_wep_key_len > 13) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid WEP key len %d "
			       "(= %d bits)\n", line,
				       bss->individual_wep_key_len,
			       bss->individual_wep_key_len * 8);
			errors++;
		}
	} else if (strcmp(tag, "wep_rekey_period") == 0) {
		bss->wep_rekeying_period = atoi(value);
		if (bss->wep_rekeying_period < 0) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid period %d\n",
			       line, bss->wep_rekeying_period);
			errors++;
		}
	} else if (strcmp(tag, "eap_reauth_period") == 0) {
		bss->eap_reauth_period = atoi(value);
		if (bss->eap_reauth_period < 0) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid period %d\n",
			       line, bss->eap_reauth_period);
			errors++;
		}
	} else if (strcmp(tag, "eapol_key_index_workaround") == 0) {
		bss->eapol_key_index_workaround = atoi(value);
        #ifdef CONFIG_IAPP
	} else if (strcmp(tag, "iapp_interface") == 0) {
		bss->ieee802_11f = 1;
		snprintf(bss->iapp_iface, sizeof(bss->iapp_iface),
			 "%s", value);
        #endif /* CONFIG_IAPP */
	} else if (strcmp(tag, "own_ip_addr") == 0) {
		if (hostapd_parse_ip_addr(value, &bss->own_ip_addr)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid IP address '%s'\n",
			       line, value);
			errors++;
		}
	} else if (strcmp(tag, "nas_identifier") == 0) {
                free(bss->nas_identifier);
		if ((bss->nas_identifier = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "auth_server_addr") == 0) {
		if (hostapd_config_read_radius_addr(
			    &bss->radius->auth_servers,
			    &bss->radius->num_auth_servers, value, 1812,
			    &bss->radius->auth_server)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid IP address '%s'\n",
			       line, value);
			errors++;
		}
	} else if (bss->radius->auth_server &&
		   strcmp(tag, "auth_server_port") == 0) {
		bss->radius->auth_server->port = atoi(value);
	} else if (bss->radius->auth_server &&
		   strcmp(tag, "auth_server_shared_secret") == 0) {
		int len = strlen(value);
		if (len == 0) {
			/* RFC 2865, Ch. 3 */
			wpa_printf(MSG_ERROR,
                                "Line %d: empty shared secret is not "
			       "allowed.\n", line);
			errors++;
		}
                free(bss->radius->auth_server->shared_secret);
		if ((bss->radius->auth_server->shared_secret =
			(u8 *) strdup(value)) == NULL)
                                goto malloc_failure;
		bss->radius->auth_server->shared_secret_len = len;
	} else if (strcmp(tag, "acct_server_addr") == 0) {
		if (hostapd_config_read_radius_addr(
			    &bss->radius->acct_servers,
			    &bss->radius->num_acct_servers, value, 1813,
			    &bss->radius->acct_server)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid IP address '%s'\n",
			       line, value);
			errors++;
		}
	} else if (bss->radius->acct_server &&
		   strcmp(tag, "acct_server_port") == 0) {
		bss->radius->acct_server->port = atoi(value);
	} else if (bss->radius->acct_server &&
		   strcmp(tag, "acct_server_shared_secret") == 0) {
		int len = strlen(value);
		if (len == 0) {
			/* RFC 2865, Ch. 3 */
			wpa_printf(MSG_ERROR,
                                "Line %d: empty shared secret is not "
			       "allowed.\n", line);
			errors++;
		}
                free(bss->radius->acct_server->shared_secret);
		if ((bss->radius->acct_server->shared_secret =
			(u8 *) strdup(value)) == NULL)
                                goto malloc_failure;
		bss->radius->acct_server->shared_secret_len = len;
	} else if (strcmp(tag, "radius_retry_primary_interval") == 0) {
		bss->radius->retry_primary_interval = atoi(value);
	} else if (strcmp(tag, "radius_acct_interim_interval") == 0) {
		bss->radius->acct_interim_interval = atoi(value);
	} else if (strcmp(tag, "auth_algs") == 0) {
		bss->auth_algs = atoi(value);
	if (bss->auth_algs == 0) {
			wpa_printf(MSG_ERROR,
                                "Line %d: no authentication algorithms "
			       "allowed\n",
			       line);
			errors++;
		}
	} else if (strcmp(tag, "max_num_sta") == 0) {
		bss->max_num_sta = atoi(value);
		if (bss->max_num_sta < 0 ||
		    bss->max_num_sta > MAX_STA_COUNT) {
			wpa_printf(MSG_ERROR,
                                "Line %d: Invalid max_num_sta=%d; "
			       "allowed range 0..%d\n", line,
			       bss->max_num_sta, MAX_STA_COUNT);
			errors++;
		}
	} else if (strcmp(tag, "wpa") == 0) {
		bss->wpa = atoi(value);
	} else if (strcmp(tag, "wpa_group_rekey") == 0) {
		bss->wpa_group_rekey = atoi(value);
	} else if (strcmp(tag, "wpa_strict_rekey") == 0) {
		bss->wpa_strict_rekey = atoi(value);
	} else if (strcmp(tag, "wpa_gmk_rekey") == 0) {
		bss->wpa_gmk_rekey = atoi(value);
	} else if (strcmp(tag, "wpa_passphrase") == 0) {
		int len = strlen(value);
                /* psk and passphrase are contradictory */
	        free(bss->ssid.wpa_psk); bss->ssid.wpa_psk = NULL;
                free(bss->ssid.wpa_passphrase); bss->ssid.wpa_passphrase=NULL;
		if (len < 8 || len > 63) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid WPA passphrase length"
			       " %d (expected 8..63)\n", line, len);
			errors++;
		} else {
			if ((bss->ssid.wpa_passphrase = strdup(value)) == NULL)
                                goto malloc_failure;
		}
	} else if (strcmp(tag, "wpa_psk") == 0) {
                /* psk and passphrase are contradictory */
	        free(bss->ssid.wpa_psk); bss->ssid.wpa_psk = NULL;
                free(bss->ssid.wpa_passphrase); bss->ssid.wpa_passphrase=NULL;
		bss->ssid.wpa_psk =
			wpa_zalloc(sizeof(struct hostapd_wpa_psk));
		if (bss->ssid.wpa_psk == NULL)
			errors++;
		else if (hexstr2bin(value, bss->ssid.wpa_psk->psk,
				    PMK_LEN) ||
			 value[PMK_LEN * 2] != '\0') {
			wpa_printf(MSG_ERROR,
                                "Line %d: Invalid PSK '%s'.\n", line,
			       value);
	                free(bss->ssid.wpa_psk); bss->ssid.wpa_psk = NULL;
			errors++;
		} else {
			bss->ssid.wpa_psk->group = 1;
		}
	} else if (strcmp(tag, "wpa_psk_file") == 0) {
		free(bss->ssid.wpa_psk_file);
		if ((bss->ssid.wpa_psk_file = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "wpa_key_mgmt") == 0) {
		bss->wpa_key_mgmt =
			hostapd_config_parse_key_mgmt(line, value);
		if (bss->wpa_key_mgmt == -1) {
			errors++;
                        bss->wpa_key_mgmt = 0;
                }
	} else if (strcmp(tag, "wpa_pairwise") == 0) {
		bss->wpa_pairwise =
			hostapd_config_parse_cipher(line, value);
		if (bss->wpa_pairwise == -1 ||
		    bss->wpa_pairwise == 0)
			errors++;
		else if (bss->wpa_pairwise &
			 (WPA_CIPHER_NONE | WPA_CIPHER_WEP40 |
			  WPA_CIPHER_WEP104)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: unsupported pairwise "
			       "cipher suite '%s'\n",
			       bss->wpa_pairwise, value);
			errors++;
		} else {
			if (bss->wpa_pairwise & WPA_CIPHER_TKIP)
				bss->wpa_group = WPA_CIPHER_TKIP;
			else
				bss->wpa_group = WPA_CIPHER_CCMP;
		}
        #ifdef CONFIG_RSN_PREAUTH
	} else if (strcmp(tag, "rsn_preauth") == 0) {
		bss->rsn_preauth = atoi(value);
	} else if (strcmp(tag, "rsn_preauth_interfaces") == 0) {
                free(bss->rsn_preauth_interfaces);
		if ((bss->rsn_preauth_interfaces = strdup(value)) == NULL)
                        goto malloc_failure;
        #endif /* CONFIG_RSN_PREAUTH */
        #ifdef CONFIG_PEERKEY
	} else if (strcmp(tag, "peerkey") == 0) {
		bss->peerkey = atoi(value);
        #endif /* CONFIG_PEERKEY */
	} else if (strcmp(tag, "ctrl_interface") == 0) {
		free(bss->ctrl_interface);
		if ((bss->ctrl_interface = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "ctrl_interface_group") == 0) {
        #ifndef CONFIG_NATIVE_WINDOWS
		struct group *grp;
		char *endp;
		const char *group = value;

		grp = getgrnam(group);
		if (grp) {
			bss->ctrl_interface_gid = grp->gr_gid;
			bss->ctrl_interface_gid_set = 1;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d"
				   " (from group name '%s')",
				   bss->ctrl_interface_gid, group);
			return errors;
		}

		/* Group name not found - try to parse this as gid */
		bss->ctrl_interface_gid = strtol(group, &endp, 10);
		if (*group == '\0' || *endp != '\0') {
			wpa_printf(MSG_DEBUG, "Line %d: Invalid group "
				   "'%s'", line, group);
			return ++errors;
		}
		bss->ctrl_interface_gid_set = 1;
		wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
			   bss->ctrl_interface_gid);
        #endif /* CONFIG_NATIVE_WINDOWS */
        #ifdef RADIUS_SERVER
	} else if (strcmp(tag, "radius_server_clients") == 0) {
		free(bss->radius_server_clients);
		if ((bss->radius_server_clients = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "radius_server_auth_port") == 0) {
		bss->radius_server_auth_port = atoi(value);
	} else if (strcmp(tag, "radius_server_ipv6") == 0) {
		bss->radius_server_ipv6 = atoi(value);
        #endif /* RADIUS_SERVER */
	} else if (strcmp(tag, "test_socket") == 0) {
		free(bss->test_socket);
		if ((bss->test_socket = strdup(value)) == NULL)
                        goto malloc_failure;
	} else if (strcmp(tag, "use_pae_group_addr") == 0) {
		bss->use_pae_group_addr = atoi(value);
	} else if (strcmp(tag, "dtim_period") == 0) {
		bss->dtim_period = atoi(value);
		if (bss->dtim_period < 1 || bss->dtim_period > 255) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid dtim_period %d\n",
			       line, bss->dtim_period);
			errors++;
		}
	} else if (strcmp(tag, "ignore_broadcast_ssid") == 0) {
		bss->ignore_broadcast_ssid = atoi(value);
	} else if (strcmp(tag, "wep_default_key") == 0) {
		bss->ssid.wep.idx = atoi(value);
		if (bss->ssid.wep.idx > 3) {
			wpa_printf(MSG_ERROR,
                                "Invalid wep_default_key index %d\n",
			       bss->ssid.wep.idx);
			errors++;
		}
	} else if (strcmp(tag, "wep_key0") == 0 ||
		   strcmp(tag, "wep_key1") == 0 ||
		   strcmp(tag, "wep_key2") == 0 ||
		   strcmp(tag, "wep_key3") == 0) {
		if (hostapd_config_read_wep(&bss->ssid.wep,
					    tag[7] - '0', value)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid WEP key '%s'\n",
			       line, tag);
			errors++;
		}
	} else if (strcmp(tag, "dynamic_vlan") == 0) {
		bss->ssid.dynamic_vlan = atoi(value);
	} else if (strcmp(tag, "vlan_file") == 0) {
		if (hostapd_config_read_vlan_file(bss, value)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: failed to read VLAN file "
			       "'%s'\n", line, value);
			errors++;
		}
        #ifdef CONFIG_FULL_DYNAMIC_VLAN
	} else if (strcmp(tag, "vlan_tagged_interface") == 0) {
                free(bss->ssid.vlan_tagged_interface);
		if ((bss->ssid.vlan_tagged_interface = strdup(value)) == NULL)
                        goto malloc_failure;
        #endif /* CONFIG_FULL_DYNAMIC_VLAN */
	} else if (strcmp(tag, "wme_enabled") == 0) {
		bss->wme_enabled = atoi(value);
	} else if (strcmp(tag, "bssid") == 0) {
                #if 0   /* WAS */
		if (bss == conf->bss) {
			wpa_printf(MSG_ERROR,
                                "Line %d: bssid item not allowed "
			       "for the default interface\n", line);
			errors++;
		} else 
                #endif  /* WAS */
                if (hwaddr_aton(value, bss->bssid)) {
			wpa_printf(MSG_ERROR,
                                "Line %d: invalid bssid item\n", line);
			errors++;
		}
        #ifdef CONFIG_IEEE80211W
	} else if (strcmp(tag, "ieee80211w") == 0) {
		bss->ieee80211w = atoi(value);
        #endif /* CONFIG_IEEE80211W */
        #ifdef EAP_WPS
        #ifndef USE_INTEL_SDK
        } else if (strcmp(tag, "wps_disable") == 0) {
                bss->wps->wps_disable = atol(value);
        } else if (strcmp(tag, "wps_upnp_disable") == 0) {
                bss->wps->wps_upnp_disable = atol(value);
        } else if (strcmp(tag, "wps_version") == 0) {
                bss->wps->version = strtoul(value, NULL, 16);
        } else if (strcmp(tag, "wps_uuid") == 0) {
	        struct wps_config *wps = bss->wps;
	        if (hexstr2bin(value, wps->uuid, SIZE_16_BYTES) ||
	            value[SIZE_16_BYTES * 2] != '\0') {
		        wpa_printf(MSG_ERROR, "Line %d: Invalid UUID '%s'.",
			        line, value);
	                errors++;
	        }
	        wps->uuid_set = 1;
        } else if (strcmp(tag, "wps_auth_type_flags") == 0) {
                bss->wps->auth_type_flags = strtoul(value, NULL, 16);
        } else if (strcmp(tag, "wps_encr_type_flags") == 0) {
                bss->wps->encr_type_flags = strtoul(value, NULL, 16);
        } else if (strcmp(tag, "wps_conn_type_flags") == 0) {
                bss->wps->conn_type_flags = strtoul(value, NULL, 16);
        } else if (strcmp(tag, "wps_config_methods") == 0) {
                bss->wps->config_methods = strtoul(value, NULL, 16);
        } else if (strcmp(tag, "wps_configured") == 0) {
                bss->wps->wps_state = (atol(value) ? 
                        WPS_WPSSTATE_CONFIGURED : WPS_WPSSTATE_UNCONFIGURED);
        } else if (strcmp(tag, "wps_rf_bands") == 0) {
                bss->wps->rf_bands = strtoul(value, NULL, 16);
        } else if (strcmp(tag, "wps_default_pin") == 0) {
                free(bss->wps->default_pin);
                if ((bss->wps->default_pin = strdup(value)) == NULL) 
                        goto malloc_failure;
        } else if (strcmp(tag, "wps_default_timeout") == 0) {
                bss->wps->default_timeout = atol(value);
        } else if (strcmp(tag, "wps_atheros_extension") == 0) {
                bss->wps->atheros_extension = atol(value);
        } else if (strcmp(tag, "wps_manufacturer") == 0) {
                free(bss->wps->manufacturer);
                #if WPS_HACK_PADDING() /* a work around */
                bss->wps->manufacturer_len = 64;        /* wps spec */
                if (((bss->wps->manufacturer = os_zalloc(64+1))) == NULL)
                        goto malloc_failure;
                strncpy(bss->wps->manufacturer, value, 64);
                #else   /* original */
                if ((bss->wps->manufacturer = strdup(value)) == NULL) 
                        goto malloc_failure;
                bss->wps->manufacturer_len = strlen(bss->wps->manufacturer);
                #endif  /* WPS_HACK_PADDING */
        } else if (strcmp(tag, "wps_model_name") == 0) {
                free(bss->wps->model_name);
                #if WPS_HACK_PADDING() /* a work around */
                bss->wps->model_name_len = 32;  /* wps spec */
                if ((bss->wps->model_name = os_zalloc(32+1)) == NULL) 
                        goto malloc_failure;
                strncpy(bss->wps->model_name, value, 32);
                #else   /* original */
                if ((bss->wps->model_name = strdup(value)) == NULL) 
                        goto malloc_failure;
                bss->wps->model_name_len = strlen(bss->wps->model_name);
                #endif  /* WPS_HACK_PADDING */
        } else if (strcmp(tag, "wps_model_number") == 0) {
                free(bss->wps->model_number);
                #if WPS_HACK_PADDING() /* a work around */
                bss->wps->model_number_len = 32;        /* wps spec */
                if ((bss->wps->model_number = os_zalloc(32+1)) == NULL) 
                        goto malloc_failure;
                strncpy(bss->wps->model_number, value, 32);
                #else   /* original */
                if ((bss->wps->model_number = strdup(value)) == NULL) 
                        goto malloc_failure;
                bss->wps->model_number_len = strlen(bss->wps->model_number);
                #endif  /* WPS_HACK_PADDING */
        } else if (strcmp(tag, "wps_serial_number") == 0) {
                free(bss->wps->serial_number);
                #if WPS_HACK_PADDING() /* a work around */
                bss->wps->serial_number_len = 32;       /* wps spec */
                if ((bss->wps->serial_number = os_zalloc(32+1)) == NULL) 
                        goto malloc_failure;
                strncpy(bss->wps->serial_number, value, 32);
                #else   /* original */
                if ((bss->wps->serial_number = strdup(value)) == NULL) 
                        goto malloc_failure;
                bss->wps->serial_number_len = strlen(bss->wps->serial_number);
                #endif  /* WPS_HACK_PADDING */
        } else if (strcmp(tag, "wps_friendly_name") == 0) {
                free(bss->wps->friendly_name);
                if ((bss->wps->friendly_name = strdup(value)) == NULL) 
                        goto malloc_failure;
        } else if (strcmp(tag, "wps_manufacturer_url") == 0) {
                free(bss->wps->manufacturer_url);
                if ((bss->wps->manufacturer_url = strdup(value)) == NULL) 
                        goto malloc_failure;
        } else if (strcmp(tag, "wps_model_description") == 0) {
                free(bss->wps->model_description);
                if ((bss->wps->model_description = strdup(value)) == NULL) 
                        goto malloc_failure;
        } else if (strcmp(tag, "wps_model_url") == 0) {
                free(bss->wps->model_url);
                if ((bss->wps->model_url = strdup(value)) == NULL) 
                        goto malloc_failure;
        } else if (strcmp(tag, "wps_upc_string") == 0) {
                free(bss->wps->upc_string);
                if ((bss->wps->upc_string = strdup(value)) == NULL) 
                        goto malloc_failure;
        } else if (strcmp(tag, "wps_dev_category") == 0) {
                bss->wps->dev_category = atol(value);
        } else if (strcmp(tag, "wps_dev_sub_category") == 0) {
                bss->wps->dev_sub_category = atol(value);
        } else if (strcmp(tag, "wps_dev_oui") == 0) {
	        if (hexstr2bin(value, bss->wps->dev_oui, 4) ||
	                        value[4*2] != '\0') {
		        wpa_printf(MSG_ERROR, 
                                "Line %d: Invalid Device OUI '%s'.",
			        line, value);
		        errors++;
                }
	} else if (strcmp(tag, "wps_dev_name") == 0) {
                free(bss->wps->dev_name);
                #if WPS_HACK_PADDING() /* a work around */
                bss->wps->dev_name_len = 32;
                if ((bss->wps->dev_name = os_zalloc(32+1)) == NULL) 
                        goto malloc_failure;
                strncpy(bss->wps->dev_name, value, 32);
                #else   /* original */
                if ((bss->wps->dev_name = strdup(value)) == NULL) 
                        goto malloc_failure;
                bss->wps->dev_name_len = strlen(bss->wps->dev_name);
                #endif  /* WPS_HACK_PADDING */
        } else if (strcmp(tag, "wps_os_version") == 0) {
                bss->wps->os_version = strtoul(value, NULL, 16);
	} else if (strcmp(tag, "wps_upnp_root_dir") == 0) {
                free(bss->wps->upnp_root_dir);
                if ((bss->wps->upnp_root_dir = strdup(value)) == NULL) 
                        goto malloc_failure;
	} else if (strcmp(tag, "wps_upnp_desc_url") == 0) {
                free(bss->wps->upnp_desc_url);
                if ((bss->wps->upnp_desc_url = strdup(value)) == NULL)
                        goto malloc_failure;
        #endif /* USE_INTEL_SDK */
        #ifdef WPS_OPT_NFC
	} else if (os_strcmp(tag, "nfc") == 0) {
		if (bss->nfcname) {
			wpa_printf(MSG_ERROR, "Line %d: Failed to "
				   "set multiple NFC devices.", line);
			errors++;
		} else {
		        bss->nfcname = os_strdup(value);
                }
        #endif /* WPS_OPT_NFC */
        #endif /* EAP_WPS */
	} else {
		wpa_printf(MSG_ERROR,
                        "Line %d: unknown bss configuration item '%s'\n",
		       line, tag);
                errors++;
	}
        #ifdef EAP_WPS
        /* The WPS spec request that we mark ourselves as configured
         * on any change of SSID, authentication or encryption methods
         * or any key...  just to be on the safe side, we'll just
         * mark ourselves as configured on any change!
         */
        if (!internal && bss->wps->wps_state != WPS_WPSSTATE_CONFIGURED) {
            wpa_printf(MSG_INFO, "Marking WPS as configured.");
            bss->wps->wps_state = WPS_WPSSTATE_CONFIGURED;
            (void) hostapd_config_bss_set(bss, "wps_configured=1", 1/*internal*/);
        }
        #endif  /* EAP_WPS */
        return errors;

        malloc_failure:
        wpa_printf(MSG_ERROR, "Malloc failure");
        return ++errors;
}



/* hostapd_radio_config_apply_file -- copy settings from file into
 * radio configuration. Later values with same tag replace earlier values.
 */
int hostapd_radio_config_apply_file(
        struct hostapd_config /*radio config*/ * conf,
        const char *fname
        )
{
	FILE *f;
	char buf[256];
        char *tag;
        char *value;
        int line = 0;
        int errors = 0;

        wpa_printf(MSG_INFO, "Reading radio configuration file %s ...\n", fname);

        /* Remember the configuration file for future use.
         * Can only remember or make use of one... so user should be
         * careful.
         */
        if (conf->config_fname == NULL) {
                conf->config_fname = strdup(fname);
        }

	f = fopen(fname, "r");
	if (f == NULL) {
		wpa_printf(MSG_ERROR,
                        "Could not open configuration file '%s' for reading.\n",
		        fname);
		return -1;
	}
        while (fgets(buf, sizeof(buf), f) != NULL) {
                line++;
                tag = hostapd_config_line_lex(buf, &value);
                if (tag == NULL) {
                        errors++;
                        continue;
                }
                if (*tag == 0) continue;        /* empty line */
                if (hostapd_radio_config_apply_line(conf, tag, value, line)) {
                        errors++;
                }
        }
        fclose(f);
	if (errors) {
		wpa_printf(MSG_ERROR,
                        "%d errors found in configuration file '%s'\n",
		        errors, fname);
                if (conf->ignore_file_errors) {
                        wpa_printf(MSG_ERROR, "IGNORING config file errors as directed.");
                        errors = 0;
                }
        }
        return (errors != 0);
}


/* hostapd_bss_config_apply_file -- copy settings from file into
 * bss configuration. Later values with same tag replace earlier values.
 */
int hostapd_bss_config_apply_file(
        struct hostapd_config /*radio config*/ * conf,
        struct hostapd_bss_config * bss,
        const char *fname
        )
{
	FILE *f;
	char buf[256];
        int line = 0;
        int errors = 0;

        wpa_printf(MSG_INFO, "Reading bss configuration file %s ...\n", fname);

        /* Remember the configuration file for future use.
         * Can only remember or make use of one... so user should be
         * careful.
         */
        if (bss->config_fname == NULL) {
                bss->config_fname = strdup(fname);
        }

	f = fopen(fname, "r");
	if (f == NULL) {
		wpa_printf(MSG_ERROR,
                        "Could not open configuration file '%s' for reading.\n",
		        fname);
		return -1;
	}
        while (fgets(buf, sizeof(buf), f) != NULL) {
                char *tag;
                char *value;

                line++;
                tag = hostapd_config_line_lex(buf, &value);
                if (tag == NULL) {
                        errors++;
                        continue;
                }
                if (*tag == 0) continue;        /* empty line */
                if (hostapd_bss_config_apply_line(bss, tag, value, line, 1)) {
                        errors++;
                }
        }
        fclose(f);
	if (errors) {
		wpa_printf(MSG_ERROR,
                        "%d errors found in configuration file '%s'\n",
		       errors, fname);
                if (bss->ignore_file_errors) {
                        wpa_printf(MSG_ERROR, "IGNORING config file errors as directed.");
                        errors = 0;
                }
        }
        return (errors != 0);
}

static char hostapd_ssid_fix_buf[200];  /* use same value for multiple VAPs */

/* If no SSID provided (e.g. out-of-box configuration), then
* invent one.
* NOTE that this is called in very early stages when we don't even
* know what our bssid is.
*/
/*static*/ int hostapd_bss_config_ssid_fix(struct hostapd_bss_config *bss)
{
    int ret;

    if (bss->ssid.ssid_len > 0) {
        return 0;
    }
    /* Use same value for ALL VAPs in order to avoid conflicts
    *   when multiple VAPs use the same file.
    *   It also is somewhat "nice" that the all have the same SSID
    *   depending on the circumstances...
    */
    if (!hostapd_ssid_fix_buf[0]) {
        /* We COULD base the new ssid on the bssid but there are two
        *       problems:
        *       -- At this stage of bringing up hostapd, we don't know
        *               what the bssid is yet.
        *       -- WPS testing requires that yet another ssid is invented
        *               when a VAP with wps_configured==0 configures
        *               a station... the two ssids MUST differ.
        */
        /* Caution, total ssid must be <= 32 chars */
        unsigned char random_bytes[8];
        int ibyte;
        os_get_random(random_bytes, sizeof(random_bytes));
        strcpy(hostapd_ssid_fix_buf, "ssid=Network-");
        for (ibyte=0; ibyte < sizeof(random_bytes); ibyte++) {
            sprintf(hostapd_ssid_fix_buf+strlen(hostapd_ssid_fix_buf), 
                "%02x", random_bytes[ibyte]);
        }
    }
    wpa_printf(MSG_INFO, "Defaulting: %s", hostapd_ssid_fix_buf);
    /* Write to file. In case multiple VAPs share the same file,
    *   they will get the same result 
    */
    ret = hostapd_config_bss_set(bss, hostapd_ssid_fix_buf, 1/*internal*/);
    return ret;
}

static char hostapd_psk_fix_buf[200];
/* If no PSK provided (e.g. out-of-box configuration), then
*  invent one... but ONLY for WPA[2]-PSK type authentication.
*/
static int hostapd_bss_config_psk_fix(struct hostapd_bss_config *bss)
{
    int ret;

    if (bss->ssid.wpa_passphrase != NULL || bss->ssid.wpa_psk != NULL) {
        /* already set? */
        return 0;
    }
    if (bss->wpa == 0) {
        /* Not WPA[2] ? */
        return 0;
    }
    if ((bss->wpa_key_mgmt & WPA_KEY_MGMT_PSK) == 0) {
        /* Not pre-shared key? */
        return 0;
    }
    /* Use same value for ALL VAPs in order to avoid conflicts
    *   when multiple VAPs use the same file.
    *   It also is somewhat "nice" that the all have the same SSID
    *   depending on the circumstances...
    */
    if (!hostapd_psk_fix_buf[0]) {
        unsigned char psk[PMK_LEN];
        int ikey;
        os_get_random(psk, PMK_LEN);
        strcpy(hostapd_psk_fix_buf, "wpa_psk=");
        for (ikey=0; ikey < PMK_LEN; ikey++) {
            sprintf(hostapd_psk_fix_buf+strlen(hostapd_psk_fix_buf), "%02x", psk[ikey]);
        }
    }
    wpa_printf(MSG_INFO, "Defaulting %s", hostapd_psk_fix_buf);
    ret = hostapd_config_bss_set(bss, hostapd_psk_fix_buf, 1/*internal*/);
    return ret;
}

/* hostapd_bss_config_finish -- final checks and fixups on bss config
 * Returns nonzero on error.
 */
static int hostapd_bss_config_finish(
        struct hostapd_bss_config *bss
        )
{
        int errors = 0;

        /* If no SSID provided (e.g. out-of-box configuration), then
         * invent one.
         */
        errors |= hostapd_bss_config_ssid_fix(bss);
        /* If no PSK provided (e.g. out-of-box configuration), then
         * invent one.
         * This helps meet requirements for WPS testing.
         */
        errors |= hostapd_bss_config_psk_fix(bss);
        /* ... could also provide default WEP keys if this was useful.
         */

	bss->radius->auth_server = bss->radius->auth_servers;
	bss->radius->acct_server = bss->radius->acct_servers;

        /* Ignore wpa_key_mgmt if wpa is 0 (including missing).
         * Or if wpa is set, default wpa_key_mgmt to PSK.
         */
	if (!bss->wpa)
		bss->wpa_key_mgmt = 0;
        else
        if (!bss->wpa_key_mgmt)
	        bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK;

	if (bss->wpa && bss->ieee802_1x) {
		bss->ssid.security_policy = SECURITY_WPA;
	} else if (bss->wpa) {
		bss->ssid.security_policy = SECURITY_WPA_PSK;
	} else if (bss->ieee802_1x) {
		bss->ssid.security_policy = SECURITY_IEEE_802_1X;
		bss->ssid.wep.default_len = bss->default_wep_key_len;
	} else if (bss->ssid.wep.keys_set)
		bss->ssid.security_policy = SECURITY_STATIC_WEP;
	else
		bss->ssid.security_policy = SECURITY_PLAINTEXT;
                
#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
        WPA_PUT_BE16(&bss->wps->prim_dev_type[0], bss->wps->dev_category);
        memcpy(&bss->wps->prim_dev_type[2], bss->wps->dev_oui, 4);
        WPA_PUT_BE16(&bss->wps->prim_dev_type[6], bss->wps->dev_sub_category);
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

        return errors;
}

/* hostapd_radio_config_finish -- final checks and fixups on radio config
 * Returns nonzero on error.
 */
int hostapd_radio_config_finish(
        struct hostapd_config /*radio config*/ * conf
        )
{
        int errors = 0;
        int i;

	if (conf->last_bss->individual_wep_key_len == 0) {
		/* individual keys are not use; can use key idx0 for broadcast
		 * keys */
		conf->last_bss->broadcast_key_idx_min = 0;
	}

	for (i = 0; i < conf->num_bss; i++) {
	        struct hostapd_bss_config *bss;

		bss = &conf->bss[i];
                if (hostapd_bss_config_finish(bss)) errors++;
	}

	if (hostapd_config_check(conf))
		errors++;

        return (errors != 0);
}


int hostapd_wep_key_cmp(struct hostapd_wep_keys *a, struct hostapd_wep_keys *b)
{
	int i;

	if (a->idx != b->idx || a->default_len != b->default_len)
		return 1;
	for (i = 0; i < NUM_WEP_KEYS; i++)
		if (a->len[i] != b->len[i] ||
		    memcmp(a->key[i], b->key[i], a->len[i]) != 0)
			return 1;
	return 0;
}


static void hostapd_config_free_radius(struct hostapd_radius_server *servers,
				       int num_servers)
{
	int i;

	for (i = 0; i < num_servers; i++) {
		free(servers[i].shared_secret);
	}
	free(servers);
}


static void hostapd_config_free_eap_user(struct hostapd_eap_user *user)
{
	free(user->identity);
	free(user->password);
	free(user);
}


static void hostapd_config_free_wep(struct hostapd_wep_keys *keys)
{
	int i;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
		free(keys->key[i]);
		keys->key[i] = NULL;
	}
}


static void hostapd_config_free_bss(struct hostapd_bss_config *conf)
{
	struct hostapd_wpa_psk *psk, *prev;
	struct hostapd_eap_user *user, *prev_user;

	if (conf == NULL)
		return;

        free(conf->config_fname);
	psk = conf->ssid.wpa_psk;
	while (psk) {
		prev = psk;
		psk = psk->next;
		free(prev);
	}

	free(conf->ssid.wpa_passphrase);
	free(conf->ssid.wpa_psk_file);
#ifdef CONFIG_FULL_DYNAMIC_VLAN
	free(conf->ssid.vlan_tagged_interface);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

	user = conf->eap_user;
	while (user) {
		prev_user = user;
		user = user->next;
		hostapd_config_free_eap_user(prev_user);
	}

	free(conf->dump_log_name);
	free(conf->eap_req_id_text);
	free(conf->accept_mac);
	free(conf->deny_mac);
	free(conf->nas_identifier);
	hostapd_config_free_radius(conf->radius->auth_servers,
				   conf->radius->num_auth_servers);
	hostapd_config_free_radius(conf->radius->acct_servers,
				   conf->radius->num_acct_servers);
	free(conf->rsn_preauth_interfaces);
	free(conf->ctrl_interface);
	free(conf->ca_cert);
	free(conf->server_cert);
	free(conf->private_key);
	free(conf->private_key_passwd);
	free(conf->eap_sim_db);
	free(conf->radius_server_clients);
	free(conf->test_socket);
	free(conf->radius);
	hostapd_config_free_vlan(conf);
	if (conf->ssid.dyn_vlan_keys) {
		struct hostapd_ssid *ssid = &conf->ssid;
		size_t i;
		for (i = 0; i <= ssid->max_dyn_vlan_keys; i++) {
			if (ssid->dyn_vlan_keys[i] == NULL)
				continue;
			hostapd_config_free_wep(ssid->dyn_vlan_keys[i]);
			free(ssid->dyn_vlan_keys[i]);
		}
		free(ssid->dyn_vlan_keys);
		ssid->dyn_vlan_keys = NULL;
	}
#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
	hostapd_config_free_wps_config(conf->wps);
#endif /* USE_INTEL_SDK */
#ifdef WPS_OPT_NFC
	if (conf->nfcname) {
		free(conf->nfcname);
		conf->nfcname = 0;
	}
#endif /* WPS_OPT_NFC */
#endif /* EAP_WPS */

}


void hostapd_config_free(struct hostapd_config *conf)
{
	size_t i;

	if (conf == NULL)
		return;

	for (i = 0; i < conf->num_bss; i++)
		hostapd_config_free_bss(&conf->bss[i]);
	free(conf->bss);
        free(conf->config_fname);

	free(conf);
}


/* Perform a binary search for given MAC address from a pre-sorted list.
 * Returns 1 if address is in the list or 0 if not. */
int hostapd_maclist_found(macaddr *list, int num_entries, const u8 *addr)
{
	int start, end, middle, res;

	start = 0;
	end = num_entries - 1;

	while (start <= end) {
		middle = (start + end) / 2;
		res = memcmp(list[middle], addr, ETH_ALEN);
		if (res == 0)
			return 1;
		if (res < 0)
			start = middle + 1;
		else
			end = middle - 1;
	}

	return 0;
}


int hostapd_rate_found(int *list, int rate)
{
	int i;

	if (list == NULL)
		return 0;

	for (i = 0; list[i] >= 0; i++)
		if (list[i] == rate)
			return 1;

	return 0;
}


const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan, int vlan_id)
{
	struct hostapd_vlan *v = vlan;
	while (v) {
		if (v->vlan_id == vlan_id || v->vlan_id == VLAN_ID_WILDCARD)
			return v->ifname;
		v = v->next;
	}
	return NULL;
}


const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
			   const u8 *addr, const u8 *prev_psk)
{
	struct hostapd_wpa_psk *psk;
	int next_ok = prev_psk == NULL;

	for (psk = conf->ssid.wpa_psk; psk != NULL; psk = psk->next) {
		if (next_ok &&
		    (psk->group || memcmp(psk->addr, addr, ETH_ALEN) == 0))
			return psk->psk;

		if (psk->psk == prev_psk)
			next_ok = 1;
	}

	return NULL;
}


const struct hostapd_eap_user *
hostapd_get_eap_user(const struct hostapd_bss_config *conf, const u8 *identity,
		     size_t identity_len, int phase2)
{
	struct hostapd_eap_user *user = conf->eap_user;

	while (user) {
		if (!phase2 && user->identity == NULL) {
			/* Wildcard match */
			break;
		}

		if (!phase2 && user->wildcard_prefix &&
		    identity_len >= user->identity_len &&
		    memcmp(user->identity, identity, user->identity_len) == 0)
		{
			/* Wildcard prefix match */
			break;
		}

		if (user->phase2 == !!phase2 &&
		    user->identity_len == identity_len &&
		    memcmp(user->identity, identity, identity_len) == 0)
			break;
		user = user->next;
	}

	return user;
}



/* 
 * Set a per-BSS config variable into running program and then into config file.
 * Input is of form:   [-nosave] <tag>=<value>
 * where <tag>=<value> is as in the per-BSS configuration file.
 * Succesful changes are saved to the original configuration file! unless
 * -nosave was specified.
 *
 * NOTE that affects of reconfiguration are NOT necessarily propogated 
 * correctly...
 * to correctly propogate, it >may< be necessary to "reconfigure" 
 * (re-read from the configuration files), which won't work if -nosave
 * was used.
 *
 * NOTE that this is a relatively inefficient operation, especially as
 * the entire configuration file is rewritten (unless -nosave is used,
 * in which case there is no way to get the change written to the configuration
 * file short of repeating the command without -nosave).
 * A better solution might be to write code to format each field individually
 * based on the binary representation, and require a "COMMIT" command to
 * save to file...
 */
int hostapd_config_bss_set(struct hostapd_bss_config *conf, char *cmd,
        int internal)   /* if nonzero, avoid some side effects */
{
	int ret = -1;
        char buf[512];
        char *tag;
        char *value;
        struct config_rewrite *rewrite = NULL;
        char *filepath = NULL;
        int nosave = 0;

	do {
                if (strncmp(cmd, "-nosave ", 8) == 0) {
                        nosave = 1;
                        cmd += 8;
                }
                while (*cmd && !isgraph(*cmd)) cmd++;
                if (strlen(cmd) >= sizeof(buf)-1) {
                        wpa_printf(MSG_ERROR, "SETBSS: line too long");
                        break;
                }
                strcpy(buf, cmd);
                tag = hostapd_config_line_lex(buf, &value);
                if (tag == NULL || value == NULL) {
                        break;
                }
                if (hostapd_bss_config_apply_line(conf, tag, value, 0, internal)) {
                        wpa_printf(MSG_ERROR, "Invalid bss config line rejected");
                        break;
                }
                /* If it survived that test, it is worth to write to
                 * the file. First we write a temp file, then rename it
                 * so as to make it a clean atomic operation.
                 */
                filepath = wps_config_temp_filepath_make(
                        conf->config_fname, "SETBSS.temp");
                if (filepath == NULL) {
                        break;
                }
                rewrite = config_rewrite_create(
                        conf->config_fname   /* original file as input */
                        );
                if (rewrite == NULL) {
                        break;
                }
                strcpy(buf, cmd);
                strcat(buf, "\n");      /* required for config_rewrite_line */
                if (config_rewrite_line(rewrite, buf, NULL/*section*/)) {
                        break;
                }
                (void) unlink(filepath);
                if (config_rewrite_write(rewrite, filepath)) {
                        break;
                }
                if (rename(filepath, conf->config_fname)) {
                        wpa_printf(MSG_ERROR, "Failed to rename %s to %s",
                                filepath, conf->config_fname);
                        break;
                }

		ret = 0;
	} while (0);

        os_free(filepath);
        config_rewrite_free(rewrite);
        if (ret) {
                wpa_printf(MSG_ERROR, "SETBSS: Failed");
        } else {
                wpa_printf(MSG_INFO, "SETBSS: May need RECONFIGURE to to fully change");
        }
	return ret;
}



/* 
 * Set a per-RADIO config variable into running program and then into config file.
 * Input is of form:   [-nosave] <tag>=<value>
 * where <tag>=<value> is as in the per-RADIO configuration file.
 * Succesful changes are saved to the original configuration file! unless
 * -nosave was specified.
 *
 * NOTE that affects of reconfiguration are NOT necessarily propogated 
 * correctly...
 * to correctly propogate, it >may< be necessary to "reconfigure" 
 * (re-read from the configuration files), which won't work if -nosave
 * was used.
 *
 * NOTE that this is a relatively inefficient operation, especially as
 * the entire configuration file is rewritten (unless -nosave is used,
 * in which case there is no way to get the change written to the configuration
 * file short of repeating the command without -nosave).
 * A better solution might be to write code to format each field individually
 * based on the binary representation, and require a "COMMIT" command to
 * save to file...
 */
int hostapd_config_radio_set(
        struct hostapd_config /*radio config*/ *iconf, 
        char *cmd)
{
	int ret = -1;
        char buf[512];
        char *tag;
        char *value;
        struct config_rewrite *rewrite = NULL;
        char *filepath = NULL;
        int nosave = 0;

	do {
                if (strncmp(cmd, "-nosave ", 8) == 0) {
                        nosave = 1;
                        cmd += 8;
                }
                while (*cmd && !isgraph(*cmd)) cmd++;
                if (strlen(cmd) >= sizeof(buf)-1) {
                        wpa_printf(MSG_ERROR, "SETRADIO: line too long");
                        break;
                }
                strcpy(buf, cmd);
                tag = hostapd_config_line_lex(buf, &value);
                if (tag == NULL || value == NULL) {
                        break;
                }
                if (hostapd_radio_config_apply_line(iconf,
                                tag, value, 0)) {
                        wpa_printf(MSG_ERROR, "Invalid radio config line rejected");
                        break;
                }
                /* If it survived that test, it is worth to write to
                 * the file. First we write a temp file, then rename it
                 * so as to make it a clean atomic operation.
                 */
                filepath = wps_config_temp_filepath_make(
                        iconf->config_fname, "SETRADIO.temp");
                if (filepath == NULL) {
                        break;
                }
                rewrite = config_rewrite_create(
                        iconf->config_fname   /* original file as input */
                        );
                if (rewrite == NULL) {
                        break;
                }
                strcpy(buf, cmd);
                strcat(buf, "\n");      /* required for config_rewrite_line */
                if (config_rewrite_line(rewrite, buf, NULL/*section*/)) {
                        break;
                }
                (void) unlink(filepath);
                if (config_rewrite_write(rewrite, filepath)) {
                        break;
                }
                if (rename(filepath, iconf->config_fname)) {
                        wpa_printf(MSG_ERROR, "Failed to rename %s to %s",
                                filepath, iconf->config_fname);
                        break;
                }

		ret = 0;
	} while (0);

        os_free(filepath);
        config_rewrite_free(rewrite);
        if (ret) {
                wpa_printf(MSG_ERROR, "SETRADIO: Failed");
        } else {
                wpa_printf(MSG_INFO, "SETRADIO: May need RECONFIGURE to to fully change");
        }
	return ret;
}



