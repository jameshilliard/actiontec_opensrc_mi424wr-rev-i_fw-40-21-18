/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/hostapd/main/hostapd.c
 *
 *  Developed by Jungo LTD.
 *  Residential Gateway Software Division
 *  www.jungo.com
 *  info@jungo.com
 *
 *  This file is part of the OpenRG Software and may not be distributed,
 *  sold, reproduced or copied in any way.
 *
 *  This copyright notice should not be removed
 *
 */

#include <unistd.h>
#include <stdio.h>

#include <process_funcs.h>
#include <rg_set_utils.h>
#include <main/mt_wsec_daemon.h>
#include <mgt/lib/mgt_utils.h>
#include <mgt/lib/mgt_route.h>
#include <mgt/lib/mgt_radius.h>
#include <web_mng/cgi/conn_settings.h>
#include <obscure.h>

#include <main/mt_wpa_common.h>
#include <main/mt_main.h>

#define CONF_PATH "/etc/hostapd/"

#define LEGACY_CONF_FILENAME "hostapd.conf"
#define LEGACY_HOSTAPD "hostapd_048"

#define WPA_CONF_MULTI_FILE "wpa-%s.conf"
#define TOPOLOGY_CONF_FILENAME "topology_ap.conf"
#ifdef ACTION_TEC_QUAD_SSID
#define TOPOLOGY_CONF_FILENAME_2 "topology_ap2.conf"
#define TOPOLOGY_CONF_FILENAME_3 "topology_ap3.conf"
#endif
#define HOSTAPD "hostapd"
#define EAP_USER_FILE "hostapd.eap_user"

#define ENT_STATUS ((wpa_stat_t *)e->ent_status)

typedef struct {
    pid_t pid;
    wpa_port_params_t wpa_params;
    int privacy_enabled;
    int cipher;
    struct {
	int enabled;
	struct in_addr ip;
	u16 port;
	u8 secret[RADIUS_SECRET_LEN]; /* RADIUS secret */
	int pre_auth;
    } radius;

    /* 
     * ACTION_TEC
	 * Added this parameter to distinguish between 
	 * old and new versions of hostapd
     */
    int use_new_hostapd;
} hostapd_wpa_stat_t;

#if defined(CONFIG_RG_WPS) && defined(ACTION_TEC)

/* 
 * ACTION_TEC
 * Add new macro define for new version of hostapd 
 */
typedef struct {
    char dev_name[IFNAMSIZ];
    char bridge_name[IFNAMSIZ];
    char ssid[MAX_SSID_LEN];
    int debug_level;
    /* Bitmask: 1 - WPA, 2 - WPA2. */
    int wpa;
    /* Bitmask: 1 - TKIP, 2 - CCMP. */
    int wpa_pairwise;
    char *wps_key;

    int auth_type;
    int encr_type;
} hostapd_wps_params_t;

/* 
wps_auth_type_flags : capabilities of network authentication
# Open    : 0x0001
# WPAPSK  : 0x0002
# Shared  : 0x0004
# WPA     : 0x0008
# WPA2    : 0x0010
# WPA2PSK : 0x0020
 */
#define WPS_AUTH_TYPE_OPEN		0x0001
#define WPS_AUTH_TYPE_WPAPSK	0x0002
#define WPS_AUTH_TYPE_SHARED	0x0004
#define WPS_AUTH_TYPE_WPA		0x0008
#define WPS_AUTH_TYPE_WPA2		0x0010
#define WPS_AUTH_TYPE_WPA2PSK	0x0020

/*
wps_encr_type_flags : capabilities of network encryption
# None : 0x0001
# WEP  : 0x0002
# TKIP : 0x0004
# AES  : 0x0008 
*/
#define WPS_ENCR_TYPE_NONE		0x0001
#define WPS_ENCR_TYPE_WEP		0x0002
#define WPS_ENCR_TYPE_TKIP		0x0004
#define WPS_ENCR_TYPE_AES		0x0008


static wl_station_type_t hostapd_sta_type_get(set_t **set)
{
    if (!set_get_path_flag(set, Sprivacy_enabled))
		return WL_STA_NONE;
    if (set_get_path_flag(set, Saccept_8021x_wep_stas))
		return WL_STA_8021X_WEP;
    if (set_get_path_flag(set, Saccept_non_8021x_wep_stas))
        return WL_STA_WEP_ONLY;
    if (set_get_path_flag(set, Saccept_wpa2_stas) && set_get_path_flag(set, Saccept_wpa_stas))
        return WL_STA_WPA1WPA2;
    else if (set_get_path_flag(set, Saccept_wpa2_stas))
        return WL_STA_WPA2;
    else if (set_get_path_flag(set, Saccept_wpa_stas))
        return WL_STA_WPA;

	return WL_STA_WPA2;
}

static int hostapd_get_wps_conf(dev_if_t *wl_dev, hostapd_wps_params_t *params)
{

    dev_if_t *bridge;
    set_t **wpa_set = set_get(dev_if_set(wl_dev), Swpa);
    set_t **wl_ap_set = set_get(dev_if_set(wl_dev), Swl_ap);
    set_t **wlan_set = set_get(dev_if_set(wl_dev), Swlan);
    wl_station_type_t wl_sta_type;

    if (!set_get_path_flag(wpa_set, Swps "/" Senabled))
	return -1;

    if (wpa_set == NULL || wl_ap_set == NULL 
	    || wlan_set == NULL || params == NULL){
	rg_error(LERR,"NULL POINTER\n");
	return (-1);
    }

    /* Get the station type */
    wl_sta_type = hostapd_sta_type_get(wpa_set);

    /* Get the wps parameters */
    strcpy(params->dev_name, wl_dev->name);
    if ((bridge = enslaving_default_bridge_get(wl_dev)))
	strcpy(params->bridge_name, bridge->name);
    strcpy(params->ssid, dev_if_ssid_get(wl_dev));
    params->wpa_pairwise = set_get_path_enum(wpa_set, Scipher,
	    cfg_wpa_cipher_t_str);
    switch (wl_sta_type) {
	case WL_STA_WPA:
	    {
		params->wpa = 1;
		if (!strcmp(set_get_path_strz(wpa_set, Scipher), "aes"))
		    params->encr_type = WPS_ENCR_TYPE_AES;
		else
		    params->encr_type = WPS_ENCR_TYPE_TKIP;
		params->auth_type = WPS_AUTH_TYPE_WPAPSK;
	    }
	    break;
	case WL_STA_WPA2:
	    {
		params->wpa |= 2;
		if (!strcmp(set_get_path_strz(wpa_set, Scipher), "aes"))
		    params->encr_type = WPS_ENCR_TYPE_AES;
		else
		    params->encr_type = WPS_ENCR_TYPE_TKIP;
		params->auth_type = WPS_AUTH_TYPE_WPA2PSK;
	    }
	    break;
	case WL_STA_WPA1WPA2:
	    {
		params->wpa |= 3;
		//			if (!strcmp(set_get_path_strz(wpa_set, Scipher), "aes"))
		params->encr_type = WPS_ENCR_TYPE_AES;
		//			else
		//				strcpy(params->encr_type, WPS_ENCR_TYPE_TKIP);
		params->auth_type = WPS_AUTH_TYPE_WPAPSK;
	    }
	    break;
	case WL_STA_WEP_ONLY:
	    {
		params->encr_type = WPS_ENCR_TYPE_WEP;
		if (set_get_path_int(wl_ap_set, Swl_auth))
		    params->auth_type = WPS_AUTH_TYPE_SHARED;
		else
		    params->auth_type = WPS_AUTH_TYPE_OPEN;
	    }
	    break;
	case WL_STA_NONE:
	    {
		params->auth_type = WPS_AUTH_TYPE_OPEN;
		params->encr_type = WPS_ENCR_TYPE_NONE;
	    }
	    break;
	case WL_STA_8021X_WEP:
	default:
	    rg_error(LERR,"The station type is error.\n");
	    return (-1);
    }

#if 0
    if (wps_key_flag == WSC_KEY_WPA) { /* get wpa key */
	wps_key = set_get_path_strz(wpa_set, Spreshared_key);
	if (!*wps_key && !set_get_path_flag(wpa_set, Swps "/" Smanual_create_key))
	    wps_key = mt_wps_generate_wps_key(wl_dev);
    } else if (wps_key_flag == WSC_KEY_WEP) { /* get wep key */
	int iactive;
	hex_ascii_t row_hex_ascii;
	iactive = set_get_path_int(wlan_set, Sactive_key);
	row_hex_ascii = set_get_path_int(wlan_set, 
		set_path_create(Skey, itoa(iactive), Sis_ascii));
	wps_key = set_get_path_strz(wlan_set, 
		set_path_create(Skey, itoa(iactive), Skey));
	if (row_hex_ascii == REPRESENT_ASCII)
	{
	    char key_as_ascii[MAX_8021X_KEY_CHARS];
	    int length;
	    length = set_get_path_int(wlan_set, 
		    set_path_create(Skey, itoa(iactive), Slength));
	    MZERO(key_as_ascii);
	    hex_2_bin(key_as_ascii, length, wps_key);
	    wps_key = key_as_ascii;
	}
    } else /* no key for none-security */
	wps_key = NULL;

#if 0
    if (!*wps_key)
	goto Error;
#endif
    params->wps_key = wps_key;

    /* params->debug_level is not yet implemented. */
    console_printf("[%s %d] to open wps task \n",__FILE__,__LINE__);
    e->t = wps_open(e, &mt_wps_task_cb, &params);
    if (e->t)
    {
	mt_wps_set_state(e, MT_WPS_INIT);
	event_timer_set(MT_WPS_INIT_WAIT_TIME, mt_wps_temporary_state_expire,
		e);
	return;
    }

Error:
    mt_wps_set_state(e, MT_WPS_STOPPED);
    /* If protected setup daemon isn't started, then the wireless device staying
     * in non secure mode. So, down it.
     */
    dev_if_notify(wl_dev, DEV_IF_ST_DOWN);
#endif


    return (0);
}

static int hostapd_prepare_eap_user_file(void)
{
    char *conf_filename = NULL;
    FILE *conf_file;

    str_printf(&conf_filename, CONF_PATH EAP_USER_FILE);
    conf_file = fopen(conf_filename, "w");
    if (!conf_file){
	rg_error_f(LERR, "openning conf file:%s failed:%m", conf_filename);
	str_free(&conf_filename);
	return -1;
    }

    /*fprintf(conf_file, "\"WFA-SimpleConfig-Registrar-1-0\"      WPS\n");*/
    fprintf(conf_file, "\"WFA-SimpleConfig-Enrollee-1-0\"       WPS\n");
    fprintf(conf_file, "\n");

    fclose(conf_file);
    return 0;
}

#endif

static void hostapd_sigchild_handler(pid_t pid, void *data, int status)
{
    rg_error(LERR, "%s: nas killed unexpectedly, status = %d",
	((dev_if_t *)data)->name, WEXITSTATUS(status));
}

static int hostapd_prepare_conf_file(hostapd_wpa_stat_t *stat, dev_if_t *wl_dev,
    dev_if_t *listen_dev, struct in_addr nas_ip)
{
    char *conf_filename = NULL;
    FILE *conf_file;
    code2str_t pairwise_cipher[] = {
	{.code = CFG_WPA_CIPHER_TKIP, .str = "TKIP"},
	{.code = CFG_WPA_CIPHER_AES, .str = "CCMP"},
	{.code = CFG_WPA_CIPHER_TKIP_AES, .str = "TKIP CCMP"},
	{.code = -1}
    };
    wpa_port_params_t *wpa_params = &stat->wpa_params;
#if defined(CONFIG_RG_WPS) && defined(ACTION_TEC)
    hostapd_wps_params_t params;
#endif

    str_printf(&conf_filename, CONF_PATH WPA_CONF_MULTI_FILE, wl_dev->name);
    conf_file = fopen(conf_filename, "w");
    if (!conf_file)
    {
	rg_error_f(LERR, "openning conf file:%s failed:%m", conf_filename);
    str_free(&conf_filename);
	return -1;
    }
	rg_error_f(LERR, "openning conf file:%s success!", conf_filename);
    str_free(&conf_filename);
    fprintf(conf_file, "ignore_file_errors=1\n");
    fprintf(conf_file, "\n");

    fprintf(conf_file, "logger_syslog=-1\n");
    fprintf(conf_file, "logger_syslog_level=2\n");
    fprintf(conf_file, "logger_stdout=-1\n");
    fprintf(conf_file, "logger_stdout_level=2\n");
    fprintf(conf_file, "\n");

    fprintf(conf_file, "# Debugging: 0 = no, 1 = minimal, 2 = verbose, 3 = msg dumps, 4 = excessive\n");
    fprintf(conf_file, "debug=0\n");
    fprintf(conf_file, "\n");

    fprintf(conf_file, "ctrl_interface=/var/run/hostapd\n");
    fprintf(conf_file, "ctrl_interface_group=0\n");
    fprintf(conf_file, "\n");

    fprintf(conf_file, "##### IEEE 802.11 related configuration #######################################\n\n");

    fprintf(conf_file, "ssid=%s\n", wpa_params->ssid);
    fprintf(conf_file, "dtim_period=%d\n", wpa_params->dtim_period);
    fprintf(conf_file, "max_num_sta=255\n");
    fprintf(conf_file, "macaddr_acl=0\n");
    fprintf(conf_file, "auth_algs=%d\n", wpa_params->auth_alg);
    fprintf(conf_file, "ignore_broadcast_ssid=0\n");
    fprintf(conf_file, "wme_enabled=0\n");
    fprintf(conf_file, "#ap_max_inactivity=300\n");
    fprintf(conf_file, "\n");

#if 0
    fprintf(conf_file, "##### IEEE 802.1X-2004 related configuration ##################################\n\n");
    fprintf(conf_file, "ieee8021x=0\n");
    fprintf(conf_file, "eapol_version=2\n");
    fprintf(conf_file, "#eap_message=hello\0networkid=netw,nasid=foo,portid=0,NAIRealms=example.com\n");
    fprintf(conf_file, "#wep_key_len_broadcast=5\n");
    fprintf(conf_file, "#wep_key_len_unicast=5\n");
    fprintf(conf_file, "#wep_rekey_period=300\n");
    fprintf(conf_file, "eapol_key_index_workaround=0\n");
    fprintf(conf_file, "#eap_reauth_period=3600\n");
    fprintf(conf_file, "#use_pae_group_addr=1\n\n");

    fprintf(conf_file, "##### Integrated EAP server ###################################################\n\n");
    fprintf(conf_file, "eap_server=1\n");
    fprintf(conf_file, "eap_user_file=/etc/wpa2/hostapd.eap_user\n");
    fprintf(conf_file, "#ca_cert=/etc/wpa2/hostapd.ca.pem\n");
    fprintf(conf_file, "#server_cert=/etc/wpa2/hostapd.server.pem\n");
    fprintf(conf_file, "#private_key=/etc/wpa2/hostapd.server.prv\n");
    fprintf(conf_file, "#private_key_passwd=secret passphrase\n");
    fprintf(conf_file, "#check_crl=1\n");
    fprintf(conf_file, "#eap_sim_db=unix:/tmp/hlr_auc_gw.sock\n\n");

    fprintf(conf_file, "##### IEEE 802.11f - Inter-Access Point Protocol (IAPP) #######################\n\n");
    fprintf(conf_file, "#iapp_interface=eth0\n\n");
#endif

    /* RADIUS client configuration for 802.1x */
    if (stat->radius.enabled)
    {
	fprintf(conf_file, "##### RADIUS client configuration #############################################\n\n");

	/* 
	 * If there is no route to radius server, the NAS IP is 0.0.0.0 and
	 * authentication will not succeed. This is better then disabling radius
	 * since then not security is defined 
	 */
	fprintf(conf_file, "nas_identifier=%s\n", inet_ntoa(nas_ip)); 
	
	fprintf(conf_file, "\n# RADIUS authentication server\n\n");
	fprintf(conf_file, "auth_server_addr=%s\n", inet_ntoa(stat->radius.ip));
	fprintf(conf_file, "auth_server_port=%d\n", stat->radius.port);
	fprintf(conf_file, "auth_server_shared_secret=%s\n", stat->radius.secret);
	fprintf(conf_file, "\n");
    }

    if (stat->radius.enabled)
    {
	fprintf(conf_file, "##### IEEE 802.1X-2004 related configuration #################################\n\n");

	fprintf(conf_file, "ieee8021x=1\n");

	/* If 802.1x mode with dynamic WEP keys. */
	if ((wpa_params->allowed_sta_types & WPA_STA_TYPE_WEP_8021X) &&
	    wpa_params->rekeying_wep_cipher)
	{
	    int key_len;
	    key_type_t dummy;

	    cipher_to_key_type(wpa_params->rekeying_wep_cipher, &dummy, &key_len);
	    fprintf(conf_file, "wep_key_len_broadcast=%d\n", key_len);
	    fprintf(conf_file, "wep_key_len_unicast=%d\n", key_len);
	    fprintf(conf_file, "wep_rekey_period=%d\n", wpa_params->gtk_update_interval/1000);
	}
	fprintf(conf_file, "\n");
    }

    if (wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA_ANY)
    {
	char *key_mgmt = "WPA-PSK"; 
	
	fprintf(conf_file, "##### WPA/IEEE 802.11i configuration ##########################################\n\n");

	fprintf(conf_file, "wpa=%d\n",
	    (wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA1 ? 1 : 0) |
	    (wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA2 ? 2 : 0));

	switch (wpa_params->psk_param)
	{
	    case WPA_PSK_PARAM_HEX:
		{
		    char hex_key[2 * WPA_PSK_KEY_LEN + 1];

		    bin_2_hex(hex_key, wpa_params->psk.hex.data, WPA_PSK_KEY_LEN);
		    fprintf(conf_file, "wpa_psk=%s\n", hex_key);
		}
		break;
	    case WPA_PSK_PARAM_ASCII:
		fprintf(conf_file, "wpa_passphrase=%s\n", wpa_params->psk.ascii);
		break;
	    case WPA_PSK_PARAM_NONE:
		key_mgmt = "WPA-EAP";
		break;
	}
	fprintf(conf_file, "wpa_key_mgmt=%s\n", key_mgmt);
	fprintf(conf_file, "wpa_pairwise=%s\n", code2str(pairwise_cipher, stat->cipher));
	fprintf(conf_file, "wpa_group_rekey=%d\n", wpa_params->gtk_update_interval/1000);

	/* WPA2 pre-authentication */
	if ((wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA2) &&
	    stat->radius.enabled && stat->radius.pre_auth)
	{
	    fprintf(conf_file, "rsn_preauth=1\n");
	    fprintf(conf_file, "rsn_preauth_interfaces=%s\n", listen_dev->name);
	}

	//fprintf(conf_file, "#wpa_strict_rekey=1\n");
	//fprintf(conf_file, "#wpa_gmk_rekey=86400\n");
	//fprintf(conf_file, "#peerkey=1\n");
	//fprintf(conf_file, "#ieee80211w=0\n\n");
	fprintf(conf_file, "\n");
    }
    else
	fprintf(conf_file, "wpa=0\n");

    fprintf(conf_file, "##### wps_properties #####################################\n");
#if defined(CONFIG_RG_WPS) && defined(ACTION_TEC)
    MZERO(params);
    if ( hostapd_get_wps_conf(wl_dev,&params) < 0 ){
	fprintf(conf_file, "wps_disable=1\n");
	fprintf(conf_file, "wps_upnp_disable=1\n");
	fprintf(conf_file, "\n");

	fclose(conf_file);
	return 0;
    }

    fprintf(conf_file, "eap_server=1\n");
    fprintf(conf_file, "eap_user_file=" CONF_PATH EAP_USER_FILE "\n");

    fprintf(conf_file, "wps_disable=0\n");
    fprintf(conf_file, "wps_upnp_disable=1\n");
    fprintf(conf_file, "wps_uuid=01010202030304040505060607070808090900000a0b0c0d0e0f000100020005\n");
    fprintf(conf_file, "wps_version=0x10\n");
    fprintf(conf_file, "wps_dev_name=Wireless Broadband Router\n");
    fprintf(conf_file, "wps_dev_category=6\n");
    fprintf(conf_file, "wps_dev_oui=0050f204\n");
    fprintf(conf_file, "wps_dev_sub_category=1\n");
    fprintf(conf_file, "wps_manufacturer=Actiontec Electronics INC.\n");
    fprintf(conf_file, "wps_model_name=Wireless Broadband Router\n");
    fprintf(conf_file, "wps_model_number=0001\n");
    fprintf(conf_file, "wps_serial_number=0001\n");
    fprintf(conf_file, "wps_config_methods=0x0082\n");
    fprintf(conf_file, "wps_configured=1\n");

    /*
# wps_auth_type_flags : capabilities of network authentication
# Open    : 0x0001
# WPAPSK  : 0x0002
# Shared  : 0x0004
# WPA     : 0x0008
# WPA2    : 0x0010
# WPA2PSK : 0x0020
wps_auth_type_flags=0x0023 

# wps_encr_type_flags : capabilities of network encryption
# None : 0x0001
# WEP  : 0x0002
# TKIP : 0x0004
# AES  : 0x0008
wps_encr_type_flags=0x000f

# wps_conn_type_flags : capabilities of connection
# ESS  : 0x01
# IBSS : 0x02
wps_conn_type_flags=0x01
*/

    fprintf(conf_file, "wps_auth_type_flags=0x%04x\n",params.auth_type);
    fprintf(conf_file, "wps_encr_type_flags=0x%04x\n",params.encr_type);
    fprintf(conf_file, "wps_conn_type_flags=0x01\n");
    /*
# wps_rf_bands : supported RF bands
# 2.4GHz : 0x01
# 5.0GHz : 0x02
wps_rf_bands=0x03
*/
    fprintf(conf_file, "wps_rf_bands=0x03\n");
    fprintf(conf_file, "wps_os_version=0x00000001\n");
    fprintf(conf_file, "wps_atheros_extension=1\n");

#else
    fprintf(conf_file, "wps_disable=1\n");
    fprintf(conf_file, "wps_upnp_disable=1\n");
#endif
    fprintf(conf_file, "\n");

    fclose(conf_file);
    return 0;
}

static int hostapd_048_prepare_conf_file(hostapd_wpa_stat_t *stat, dev_if_t *wl_dev,
    dev_if_t *listen_dev, struct in_addr nas_ip)
{
    char *conf_filename = NULL;
    FILE *conf_file;
    code2str_t pairwise_cipher[] = {
	{.code = CFG_WPA_CIPHER_TKIP, .str = "TKIP"},
	{.code = CFG_WPA_CIPHER_AES, .str = "CCMP"},
	{.code = CFG_WPA_CIPHER_TKIP_AES, .str = "TKIP CCMP"},
	{.code = -1}
    };
    dev_if_t *br;
    wpa_port_params_t *wpa_params = &stat->wpa_params;

    str_printf(&conf_filename, CONF_PATH "%s_" LEGACY_CONF_FILENAME,
	wl_dev->name);
    conf_file = fopen(conf_filename, "w");
    str_free(&conf_filename);
    if (!conf_file)
    {
	rg_error_f(LERR, "openning conf file:%s failed:%m", LEGACY_CONF_FILENAME);
	return -1;
    }
    
    /* basic params */
    fprintf(conf_file, "interface=%s\n", wl_dev->name);
   
    /* Add bridge if exist 
     * TODO: Make sure we get notification for bridge change */
    if ((br = enslaving_default_bridge_get(wl_dev)))
	fprintf(conf_file, "bridge=%s\n", br->name);

    /* RADIUS client configuration for 802.1x */
    if (stat->radius.enabled)
    {
	/* If there is no route to radius server, the NAS IP is 0.0.0.0 and
	 * authentication will not succeed. This is better then disabling radius
	 * since then not security is defined */
	fprintf(conf_file, "own_ip_addr=%s\n", inet_ntoa(nas_ip));

	fprintf(conf_file, "auth_server_addr=%s\n", inet_ntoa(stat->radius.ip));
	fprintf(conf_file, "auth_server_port=%d\n", stat->radius.port);
	fprintf(conf_file, "auth_server_shared_secret=%s\n", stat->radius.secret);

	fprintf(conf_file, "ieee8021x=1\n");

	/* If 802.1x mode with dynamic WEP keys. */
	if ((wpa_params->allowed_sta_types & WPA_STA_TYPE_WEP_8021X) &&
	    wpa_params->rekeying_wep_cipher)
	{
	    int key_len;
	    key_type_t dummy;

	    cipher_to_key_type(wpa_params->rekeying_wep_cipher, &dummy,
		&key_len);
	    fprintf(conf_file, "wep_key_len_broadcast=%d\n", key_len);
	    fprintf(conf_file, "wep_key_len_unicast=%d\n", key_len);
	    fprintf(conf_file, "wep_rekey_period=%d\n",
		wpa_params->gtk_update_interval/1000);
	}
    }

    /* WPA/IEEE 802.11i configuration */
    if (wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA_ANY)
    {
	char *key_mgmt = "WPA-PSK";

	fprintf(conf_file, "wpa=%d\n",
	    (wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA1 ? 1 : 0) |
	    (wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA2 ? 2 : 0));

	/* WPA2 pre-authentication */
	if ((wpa_params->allowed_sta_types & WPA_STA_TYPE_WPA2) &&
	    stat->radius.enabled && stat->radius.pre_auth)
	{
	    fprintf(conf_file, "rsn_preauth=1\n");
	    fprintf(conf_file, "rsn_preauth_interfaces=%s\n", listen_dev->name);
	}

	switch (wpa_params->psk_param)
	{
	case WPA_PSK_PARAM_HEX:
	    {
		char hex_key[2 * WPA_PSK_KEY_LEN + 1];

		bin_2_hex(hex_key, wpa_params->psk.hex.data, WPA_PSK_KEY_LEN);
		fprintf(conf_file, "wpa_psk=%s\n", hex_key);
	    }
	    break;
	case WPA_PSK_PARAM_ASCII:
	    fprintf(conf_file, "wpa_passphrase=%s\n", wpa_params->psk.ascii);
	    break;
	case WPA_PSK_PARAM_NONE:
	    key_mgmt = "WPA-EAP";
	    break;
	}
	fprintf(conf_file, "wpa_key_mgmt=%s\n", key_mgmt);
	fprintf(conf_file, "wpa_pairwise=%s\n", code2str(pairwise_cipher,
	    stat->cipher));
	fprintf(conf_file, "wpa_group_rekey=%d\n",
	    wpa_params->gtk_update_interval/1000);

	/* XXX add the wpa2 preauth stuff */
    }
    else
	fprintf(conf_file, "wpa=0\n");

    fclose(conf_file);
    return 0;
}


static void hostapd_start(void *context, dev_if_t *wl_dev,
    dev_if_t *listen_dev, struct in_addr nas_ip)
{
    hostapd_wpa_stat_t *stat = context;
    char *cmd = NULL;

    if (set_get_path_flag(dev_if_set(wl_dev), Sis_wifi_help))
        return;

    /* Don't activate hostapd for WEP or disabled mode. */
    if ((stat->wpa_params.allowed_sta_types & WPA_STA_TYPE_WEP_LEGACY) ||
	!stat->wpa_params.allowed_sta_types || !stat->privacy_enabled)
    {
	return;
    }

    if (stat->use_new_hostapd)
    {
	set_t **wpa_set = set_get(dev_if_set(wl_dev), Swpa);

	hostapd_prepare_conf_file(stat, wl_dev, listen_dev, nas_ip);
#if defined(CONFIG_RG_WPS) && defined(ACTION_TEC)
	if (set_get_path_flag(wpa_set, Swps "/" Senabled))
	{
	    if ( hostapd_prepare_eap_user_file() < 0)
		rg_error(LERR,"Hostapd eap_user file error to generate,no wps support now\n");
	}
#endif
#ifdef ACTION_TEC_QUAD_SSID
    if(!strcmp(wl_dev->name,ACTION_TEC_ATHEROS_PRIMARY_IFNAME))
	str_printf(&cmd, HOSTAPD " " CONF_PATH TOPOLOGY_CONF_FILENAME);
    else if(!strcmp(wl_dev->name,ACTION_TEC_ATHEROS_SECONDARY_IFNAME))
	str_printf(&cmd, HOSTAPD " " CONF_PATH TOPOLOGY_CONF_FILENAME_2);
    else if(!strcmp(wl_dev->name,ACTION_TEC_ATHEROS_PUBLIC_IFNAME))
	str_printf(&cmd, HOSTAPD " " CONF_PATH TOPOLOGY_CONF_FILENAME_3);
#else
	str_printf(&cmd, HOSTAPD " " CONF_PATH TOPOLOGY_CONF_FILENAME);
#endif
    }
    else
    {
	hostapd_048_prepare_conf_file(stat, wl_dev, listen_dev, nas_ip);
	str_printf(&cmd, LEGACY_HOSTAPD " " CONF_PATH "%s_" LEGACY_CONF_FILENAME, wl_dev->name);
    }

    console_printf("\n%s: hostapd_cmd=%s\n", __FUNCTION__, cmd);
    stat->pid = start_process(cmd, SYSTEM_DAEMON, hostapd_sigchild_handler, wl_dev);
    rg_error(LINFO, "%s:wl_dev(%s), listen_dev(%s), hostapd(pid = %d) started",
                __FUNCTION__, wl_dev->name, listen_dev->name, stat->pid);
    str_free(&cmd);
}

static void hostapd_stop(void *context)
{
    hostapd_wpa_stat_t *stat = context;

    if (stat->pid == -1)
	return;

    rg_error(LINFO, "%s: hostapd(pid = %d) stop", __FUNCTION__, stat->pid);
    stop_process(stat->pid);
    stat->pid = -1;
}

static void hostapd_wpa_stat_fill(hostapd_wpa_stat_t *stat, dev_if_t *dev)
{
    set_t **wpa_set = set_get(dev_if_set(dev), Swpa);

    wpa_port_params_fill(dev, NULL, &stat->wpa_params);
    
    if (stat->wpa_params.psk_param == WPA_PSK_PARAM_ASCII)
	stat->wpa_params.psk.ascii = strdup(stat->wpa_params.psk.ascii);
    //stat->wpa_params.ssid = NULL; /* not used */
    stat->privacy_enabled = wlan_is_privacy_enabled(dev_if_set(dev));
    stat->cipher = set_get_path_enum(wpa_set, Scipher,
	cfg_wpa_cipher_t_str);
    stat->radius.enabled = (set_get_path_flag(wpa_set, Sprivacy_enabled) &&
        (stat->wpa_params.allowed_sta_types & WPA_STA_TYPE_WEP_8021X ||
	(stat->wpa_params.allowed_sta_types & WPA_STA_TYPE_WPA_ANY &&
	stat->wpa_params.psk_param == WPA_PSK_PARAM_NONE)));

    if (stat->radius.enabled)
    {
	set_t **radius = set_get(wpa_set, Sradius);

	if (radius)
	{
	    stat->radius.ip = set_get_path_ip(radius, Sip);
	    stat->radius.port = set_get_path_int(radius, Sport);
	    strncpy(stat->radius.secret,
		set_get_path_strz(radius, Sshared_secret), RADIUS_SECRET_LEN);
	    unobscure_str(stat->radius.secret);
	    stat->radius.pre_auth = set_get_path_flag(wpa_set,
  	                 S8021x "/" Spre_auth);
	}
    }
}

static void hostapd_wpa_stat_free(hostapd_wpa_stat_t *stat)
{
    if (stat->wpa_params.psk_param == WPA_PSK_PARAM_ASCII)
	free(stat->wpa_params.psk.ascii);
}

static void hostapd_wpa_reconf(void *context, dev_if_t *dev)
{
    hostapd_wpa_stat_t *stat = context;

    if (set_get_path_flag(dev_if_set(dev), Sis_wifi_help))
        return;

    hostapd_stop(stat);
    hostapd_wpa_stat_free(stat);
    hostapd_wpa_stat_fill(stat, dev);
}

static reconf_type_t hostapd_wpa_changed(void *context, dev_if_t *dev)
{
    set_t **old, **new;
    
    old = dev_if_set_get(&saved_rg_conf, dev);
    new = dev_if_set_get(rg_conf, dev);

    if (COMP_SET(old, new, Swpa) || 
	COMP_SET(old, new, S8021x) ||
	COMP_SET(old, new, Swlan))
    {
	return NEED_RECONF;
    }
	
    return NO_RECONF;
}

struct in_addr hostapd_get_radius_ip(void *context)
{
    hostapd_wpa_stat_t *stat = context;
    struct in_addr ip = { 0 };

    if (stat->radius.enabled)
	return stat->radius.ip;

    return ip;
}

static wsec_daemon_cb_t hostapd_wpa_daemon_cb = {
    .start_daemon = hostapd_start,
    .stop_daemon = hostapd_stop,
    .get_radius_ip = hostapd_get_radius_ip,
    .changed = hostapd_wpa_changed,
    .reconf = hostapd_wpa_reconf,
};

void __hostapd_wpa_open(void *ctx)
{
    dev_if_t *dev = ctx;
    hostapd_wpa_stat_t *stat = zalloc_e(sizeof(hostapd_wpa_stat_t));

#ifdef CONFIG_HW_AUTODETECT
    int actiontec_is_new_hostapd(void);
    stat->use_new_hostapd = actiontec_is_new_hostapd();
#endif /* CONFIG_HW_AUTODETECT */
    stat->pid = -1;

    console_printf("Starting HOSTAPD on dev '%s'\n", dev->name);
    mt_wsec_daemon_open(dev, &hostapd_wpa_daemon_cb, stat);
}

void hostapd_wpa_close(dev_if_t *dev)
{
    if (set_get_path_flag(dev_if_set(dev), Sis_wifi_help))
        return;

    event_timer_del(__hostapd_wpa_open, dev);

    rg_entity_t *e = dev->context_wsec_daemon;
    hostapd_wpa_stat_t *stat = mt_wsec_daemon_get_context(e);

    mt_wsec_daemon_close(e);
    hostapd_wpa_stat_free(stat);
    free(stat);
}

void hostapd_wpa_open(dev_if_t *dev)
{
    if (set_get_path_flag(dev_if_set(dev), Sis_wifi_help))
        return;

    console_printf("Scheduling HOSTAPD on dev '%s'\n", dev->name);
    event_timer_set(10000, __hostapd_wpa_open, dev);
}
