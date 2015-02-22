/****************************************************************************
 *
 * rg/pkg/build/create_config.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <util/str.h>
#include <util/alloc.h>
#include <rg_types.h>
#ifndef CONFIG_RG_GPL
#include <rg_version.h>
#include <util/lines.h>
#include <license/lic_features.h>
#endif

#include "config_opt.h"
#include "create_config.h"

char *hw, *os = "", *dist, *features;
char *mk_file_name = NULL, *h_file_name = NULL, *c_file_name = NULL;
FILE *mk_file = NULL, *c_file = NULL, *h_file = NULL;
#ifdef  CONFIG_RG_DO_DEVICES
char *dev_if_conf_file_name = NULL;
#endif
char *major_features_file = NULL, *major_features_cfile = NULL;
int is_evaluation, no_lic;
jpkg_dist_t *jpkg_dists;
char *config_strings = NULL;

static void add_config_string(char *token, char *value)
{
#ifndef CONFIG_RG_GPL
    char *s, *cvalue = NULL;

    str_cpy(&cvalue, "");
    for (s = value; s && *s; s++)
    {
	switch (*s)
	{
	case '\n': str_catprintf(&cvalue, "\\n"); break;
	case '\r': str_catprintf(&cvalue, "\\r"); break;
	case '\t': str_catprintf(&cvalue, "\\t"); break;
	case '\\': 
	case '"': str_catprintf(&cvalue, "\\%c", *s); break;
	default: str_catprintf(&cvalue, "%c", *s);
	}
    }
    str_catprintf(&config_strings, "    {\"%s\", \"%s\"},\n", token, cvalue);
    str_free(&cvalue);
#endif
}

static void token_set_internal(char *file, int line, set_prio_t set_prio, 
    char *token, const char *value_, ...);
static void vtoken_set_internal(char *file, int line, set_prio_t set_prio, 
    char *token, const char *value_, va_list ap);

static void add_feature_parse(char *arg)
{
    char *token, *value = NULL;
    char *opt = strdup(arg);
    option_t *o;

    if (!opt)
	conf_err("Can't strdup\n");

    token = strtok(opt, "=");
    value = strtok(NULL, "=");

    o = option_token_get(openrg_config_options, token);
    if (o->type & OPT_INT)
	conf_err("%s: Internal. Cannot be added to command line\n", token);

    if (value)
	token_set_internal(__FILE__, __LINE__, SET_PRIO_CMD_LINE, token, value);
}

static void usage(void)
{
    printf(
	"create_config [options]\n"
        "options:\n"
	"  -d [DIST]     - distribution\n"
	"  -c [file]     - C output file\n"
	"  -m [file]     - make output file\n"
	"  -i [file]     - C header output file\n"
	"  -f [FEATURE]  - add feature\n"
	"  -h [HARDWARE] - set CONFIG_RG_HW\n"
	"  -o [OS]       - set CONIFG_RG_OS\n"
#ifdef CONFIG_RG_DO_DEVICES
	"  -e [file]     - dev_if conf file name\n"
#endif
	"  -M [file]     - major features file\n"
	"  -F [file]     - major features C file\n"
	);
    exit(1);
}

static void handle_opt(int argc, char *argv[])
{
    while (1)
    {
	switch (getopt(argc, argv, "d:f:h:o:c:i:m:e:M:F:"))
	{
	case 'd':
	    dist = optarg;
	    break;
	case 'c':
	    c_file_name = optarg;
	    break;
	case 'm':
	    mk_file_name = optarg;
	    break;
	case 'i':
	    h_file_name = optarg;
	    break;
	case 'f':
	    features = optarg;
	    add_feature_parse(optarg);
	    break;
	case 'h':
	    hw = optarg;
	    break;
	case 'o':
	    os = optarg;
	    break;
#ifdef CONFIG_RG_DO_DEVICES
	case 'e':
	    dev_if_conf_file_name = optarg;
	    break;
#endif
	case 'M':
	    major_features_file = optarg;
	    break;
	case 'F':
	    major_features_cfile = optarg;
	    break;
	case -1:
	    return;
	case '?':
	case ':':
	    usage();
	    break;
	}
    }
}

void conf_err(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    exit(1);
}

char *sys_get(int *ret, char *command, ...)
{   
    FILE *fp;
    char *cmd = NULL;
    int c, pos = 0;
    va_list ap;
    char *output;
    int out_sz = 1024;
    int _ret = -1;

    output = malloc(out_sz);
    va_start(ap, command);
    str_vprintf(&cmd, command, ap);
    va_end(ap);
    if (ret)
	*ret = _ret;

    if (!(fp = popen(cmd, "r")))
    {
	printf("popen(%s): %s", cmd, strerror(errno));
	goto Exit;
    }

    while ((c = getc(fp)) != EOF)
    {
	if (pos+1>=out_sz)
	{
	    out_sz *= 2;
	    output = realloc(output, out_sz);
	}
	output[pos] = c;
	pos++;
    }
    
Exit:
    output[pos] = 0;
    _ret = pclose(fp);
    if (ret)
	*ret = _ret;

    return output;
}

static void options_print(option_t *array)
{
    int i;
    
    for (i = 0; array[i].token; i++)
    {
	fprintf(stderr, "%s", array[i].token);
	if (array[i + 1].token)
	    fprintf(stderr, ", ");
    }

    fprintf(stderr, "\n");
}

#define MODULE_EXPANSION "_MODULE"
#define FEATURE_EXPANSION "_FEATURE"

static void print_ref(int avoid_print, option_t *o)
{
    if (avoid_print)
	return;
    
    if (o->set_prio == SET_PRIO_CMD_LINE)
	fprintf(h_file, "/* Set by command line */\n");
    else if (o->line)
	fprintf(h_file, "/* %s:%d */\n", o->file, o->line);
    else
    {
	/* Empty line to separate from previous referrence section*/
	fprintf(h_file, "\n");
    }
}

static void print_h_option(option_t *o)
{
    char *v = o->value, *t = o->token;
    int module_expanded = o->type & OPT_MODULE_EXPAND;
    int is_y_m, avoid_print_ref = 0;
    char *quote = o->type & OPT_C_STR ? "\"" : "";

    is_y_m = (o->type & (OPT_MODULE | OPT_MODULE_EXPAND) || 
	!(o->type & (OPT_NUMBER | OPT_STR | OPT_C_STR)));

    /* Print the cCONFIG_XXX and vcCONFIG_XXX */
    if (o->type & OPT_HC)
    {
	int is_int = 1;
	char *v_str = strdup("");
	char *vc_type;
	
	if (o->type & (OPT_STR | OPT_C_STR))
	{
	    is_int = 0;
	    if (v)
		str_printf(&v_str, "%s%s%s", quote, v, quote);
	    else
		str_printf(&v_str, "NULL");
	}
	else if (o->type & OPT_NUMBER)
	    str_printf(&v_str, "%s", v ?: "0");
	else if (is_y_m)
	{
	    str_printf(&v_str, "%s",
		(v && (!strcmp(v ,"y") || !strcmp(v ,"m"))) ? "1" : "0");
	}
	else
	    str_cpy(&v_str, v ? "1" : "0");
	vc_type = is_int ? "int " : "char *";

	print_ref(avoid_print_ref++, o);
	fprintf(h_file, "#define c%s %s\n", t, v_str);
	fprintf(h_file, "extern %svc%s;\n", vc_type, t);
	fprintf(c_file, "%svc%s = %s;\n",vc_type, t, v_str);
	str_free(&v_str);
    }

    if (!module_expanded)
    {
	/* The CONFIG_XXX isn't module expanded. So, we print the value
	 * (if exists) and nothing more.
	 */
	if (v)
	{
	    char *v_str = NULL;

	    if (is_y_m)
	    {
		if (!strcmp(v ,"y") || !strcmp(v ,"m"))
		    v_str = "1";
	    }
	    else
		v_str = v;
	    
	    if (v_str)
	    {
		print_ref(avoid_print_ref++, o);
		fprintf(h_file, "#define %s %s%s%s\n", t, quote, v_str, quote);
	    }
	}
	return;
    }

    /* Print the cCONFIG_XXX_FEATURE */
    print_ref(avoid_print_ref++, o);
    fprintf(h_file, "#define c%s%s %d\n", t, FEATURE_EXPANSION,
	(v && strcmp(v, "n")) ? 1 : 0);
    fprintf(h_file, "extern int vc%s%s;\n", t, FEATURE_EXPANSION);
    fprintf(c_file, "int vc%s%s = %d;\n", t, FEATURE_EXPANSION,
	(v && strcmp(v, "n")) ? 1 : 0);
    
    if (!v || !strcmp(v, "n"))
	return;

    /* If the config is on (no matter 'm' or 'y')
     *   turn on CONFIG_XXX_FEATURE
     */
    fprintf(h_file, "#define %s%s 1\n", t, FEATURE_EXPANSION);
    
    /* If the config is 'm'
     *   turn on CONFIG_XXX_MODULE
     * else
     *   turn on CONFIG_XXX
     */
    fprintf(h_file, "#define %s%s 1\n", t, (*v == 'm') ? MODULE_EXPANSION : "");
}

static void check_option_validity(option_t *o)
{
    char *v = o->value, *t = o->token;
    int is_y = 0, is_y_m = 0;
    char *s = NULL;
    
    if (!v)
	return;
    if (o->set_prio == SET_PRIO_CMD_LINE)
	str_printf(&s, "%s=%s (Set by command line)", t, v);
    else if (o->file)
	str_printf(&s, "%s=%s (%s:%d)", t, v, o->file, o->line);
    else
	str_printf(&s, "%s=%s (config_opt.c)", t, v);
    if (o->type & OPT_NUMBER)
    {
	if (!str_is_number_value(v))
	    conf_err("%s must be numeric\n", s);
    }
    is_y_m = o->type & (OPT_MODULE | OPT_MODULE_EXPAND);
    /* default case: if we did not mark anything, then this is a y/n option */
    is_y = !(o->type & (OPT_NUMBER | OPT_STR | OPT_C_STR |
	OPT_MODULE| OPT_MODULE_EXPAND));
    if (is_y && !(!strcmp(v, "") || !strcmp(v, "n") || !strcmp(v, "y")))
	conf_err("%s must be 'y' or 'n'\n", s);
    if (is_y_m && !(!strcmp(v, "") || !strcmp(v, "n") || !strcmp(v, "y") ||
	!strcmp(v, "m")))
    {
	conf_err("%s must be 'y' 'm' or 'n'\n", s);
    }
    str_free(&s);
}

static void print_mk_option(option_t *o)
{
    char *v = o->value, *t = o->token;
    int is_y_m;

    is_y_m = v && (!strcmp(v, "y") || !strcmp(v, "m"));

    if (!is_y_m && !(o->type & (OPT_STR | OPT_C_STR | OPT_NUMBER)))
    {
	/* do not print 'CONFIG_xxx=n', unless it was set by command line */
	if (o->set_prio == SET_PRIO_CMD_LINE)
	{
	    fprintf(mk_file, "# Disabled by command line\n");
	    fprintf(mk_file, "override %s=\n", t);
	    if (o->type & OPT_EXPORT)
		fprintf(mk_file, "export %s\n", t);
	}
	return;
    }

    if (!v)
	return;
    
    if (o->set_prio == SET_PRIO_CMD_LINE)
	fprintf(mk_file, "# Set by command line:\n");
    else if (o->file)
	fprintf(mk_file, "# %s:%d\n", o->file, o->line); /* refference */
    else
	fprintf(mk_file, "# config_opt.c (default):\n");

    fprintf(mk_file, "%s%s=%s\n",
	(o->type & OPT_EXPORT) ? "export " : "", t, v);
}

static void print_option(option_t *o)
{
    /* If none of the print options is set we print everything */
    int print_all = !(o->type & (OPT_H | OPT_MK));
    int print_h = (o->type & OPT_H) || (o->type & OPT_HC) || print_all;
    int print_mk = (o->type & OPT_MK) || print_all;
    
    /* check the option is a valid value! */
    check_option_validity(o);

    /* Print CONFIG_XXX to rg_config.mk */
    if (print_mk)
	print_mk_option(o);
    
    /* Print to rg_config.h */
    if (print_h)
	print_h_option(o);
    if (!str_isempty(o->value) && strcmp(o->value, "0"))
	add_config_string(o->token, o->value);
}

static void configure_print(void)
{
    option_t *c_opt;
    
    if (!h_file_name || !mk_file_name || !c_file_name)
	conf_err("MUST define mk_file, h_file and c_file");
    
    if (!(c_file = fopen(c_file_name, "w")))
	conf_err("Can't open c_file: %s", c_file_name);
   
    if (!(mk_file = fopen(mk_file_name, "w")))
	conf_err("Can't open mk_file: %s", mk_file_name);
   
    if (!(h_file = fopen(h_file_name, "w")))
	conf_err("Can't open h_file: %s", h_file_name);
    
    fprintf(mk_file, "#OpenRG: This file was automatically generated by "
	"pkg/build/create_config\n"
	"#See %s for configuration log.\n\n", getenv("CONFIG_LOG"));

    fprintf(c_file, "/* OpenRG: This file was automatically generated by "
	"pkg/build/create_config \n * See %s for configuration log.\n */\n\n"
	"#include <stdlib.h>\n", getenv("CONFIG_LOG"));
    if (token_get("CONFIG_RG_CONFIG_STRINGS"))
	fprintf(c_file, "#include <build/config_strings.h>\n");
    fprintf(c_file, "\n");

    fprintf(h_file, "/* OpenRG: This file was automatically generated by "
	"pkg/build/create_config \n * See %s for configuration log.\n */\n"
	"#ifndef _RG_CONFIG_H_\n"
	"#define _RG_CONFIG_H_\n\n"
	"#define AUTOCONF_INCLUDED\n", getenv("CONFIG_LOG"));
    
    for (c_opt = openrg_config_options; c_opt->token; c_opt++)
	print_option(c_opt);

    if (token_get("CONFIG_RG_CONFIG_STRINGS"))
    {
	fprintf(c_file, "\nconfig_value_t config_strings[] = {\n%s"
	    "    {NULL, NULL}\n};\n\n", config_strings ? : "");
    }
    
    fprintf(h_file, "#endif\n");

    fclose(mk_file);
    fclose(c_file);
    fclose(h_file);
    str_free(&config_strings);
}

#ifndef CONFIG_RG_GPL

static u8 enabled_features[FEATURE_LAST];
static code2str_t modules[] = {
   	{FEATURE_USB_RNDIS, "CONFIG_HW_USB_RNDIS"},
	{FEATURE_80211, "CONFIG_HW_80211G_BCM43XX"},
	{FEATURE_80211, "CONFIG_HW_80211G_ISL38XX"},
	{FEATURE_80211, "CONFIG_HW_80211G_ISL_SOFTMAC"},
	{FEATURE_80211, "CONFIG_HW_80211B_PRISM2"},
	{FEATURE_80211, "CONFIG_HW_80211G_AR531X"},
	{FEATURE_80211, "CONFIG_HW_80211A_AR531X"},
	{FEATURE_80211, "CONFIG_HW_80211G_RALINK_RT2560"},
	{FEATURE_80211, "CONFIG_HW_80211G_RALINK_RT2561"},
	{FEATURE_80211, "CONFIG_RG_ATHEROS_HW_AR5212"},
	{FEATURE_80211, "CONFIG_RG_ATHEROS_HW_AR5416"},
	{FEATURE_80211, "CONFIG_HW_80211N_AIRGO_AGN100"},
	{FEATURE_80211, "CONFIG_HW_80211G_UML_WLAN"},
	{FEATURE_DSP_VOICE, "CONFIG_HW_DSP"},
	{FEATURE_RG_FOUNDATION, "MODULE_RG_FOUNDATION"},
	{FEATURE_ADV_MANAGEMENT, "MODULE_RG_ADVANCED_MANAGEMENT"},
	{FEATURE_SNMP, "MODULE_RG_SNMP"},
	{FEATURE_DSL, "MODULE_RG_DSL"},
	{FEATURE_PPP, "MODULE_RG_PPP"},
	{FEATURE_IPV6, "MODULE_RG_IPV6"},
	{FEATURE_VLAN, "MODULE_RG_VLAN"},
	{FEATURE_UPNP, "MODULE_RG_UPNP"},
	{FEATURE_ADV_ROUTING, "MODULE_RG_ADVANCED_ROUTING"},
	{FEATURE_SECURITY, "MODULE_RG_FIREWALL_AND_SECURITY"},
	{FEATURE_VPN_L2TP, "MODULE_RG_L2TP"},
	{FEATURE_VPN_IPSEC, "MODULE_RG_IPSEC"},
	{FEATURE_VPN_IPSEC, "CONFIG_HW_ENCRYPTION"},
	{FEATURE_VPN_PPTP, "MODULE_RG_PPTP"},
	{FEATURE_FILE_SERVER, "MODULE_RG_FILESERVER"},
	{FEATURE_PRINT_SERVER, "MODULE_RG_PRINTSERVER"},
	{FEATURE_PRINT_SERVER, "CONFIG_RG_LPD"},
	{FEATURE_WLAN_ADV_SECURITY, "MODULE_RG_WLAN_AND_ADVANCED_WLAN"},
	{FEATURE_CABLEHOME, "MODULE_RG_CABLEHOME"},
	{FEATURE_VODSL, "MODULE_RG_VODSL"},
	{FEATURE_RV_SIP, "MODULE_RG_VOIP_RV_SIP"},
	{FEATURE_RV_MGCP, "MODULE_RG_VOIP_RV_MGCP" }, 
	{FEATURE_RV_H323, "MODULE_RG_VOIP_RV_H323"},
	{FEATURE_OSIP, "MODULE_RG_VOIP_OSIP"},
	{FEATURE_URL_FILTERING, "MODULE_RG_URL_FILTERING"},
	{FEATURE_QOS, "MODULE_RG_QOS"},
	{FEATURE_DSLHOME, "MODULE_RG_DSLHOME"},
	{FEATURE_MAIL_FILTER, "MODULE_RG_MAIL_FILTER"},
	{FEATURE_MAIL_SERVER, "MODULE_RG_MAIL_SERVER"},
	{FEATURE_ZERO_CONFIG_NET, "MODULE_RG_ZERO_CONFIGURATION_NETWORKING"},
	{FEATURE_WEB_SERVER, "MODULE_RG_WEB_SERVER"},
	{FEATURE_FTP_SERVER, "MODULE_RG_FTP_SERVER"},
	{FEATURE_ROUTE_MULTIWAN, "MODULE_RG_ROUTE_MULTIWAN"},
	{FEATURE_ASTERISK_SIP, "MODULE_RG_VOIP_ASTERISK_SIP"},
	{FEATURE_ASTERISK_H323, "MODULE_RG_VOIP_ASTERISK_H323"},
	{FEATURE_BLUETOOTH_PAN, "MODULE_RG_BLUETOOTH"},
	{FEATURE_TR_064, "MODULE_RG_TR_064"},
	{FEATURE_JVM, "MODULE_RG_JVM"},
	{FEATURE_SSL_VPN, "MODULE_RG_SSL_VPN"},
	{FEATURE_ASTERISK_MGCP_CALL_AGENT,
	    "MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT"},
	{FEATURE_ATA, "MODULE_RG_ATA"},
	{FEATURE_PBX, "MODULE_RG_PBX"},
	{FEATURE_AV_LAN, "MODULE_RG_ANTIVIRUS_LAN_PROXY"},
	{FEATURE_AV_NAC, "MODULE_RG_ANTIVIRUS_NAC"},
	{FEATURE_UPNP_AV, "MODULE_RG_UPNP_AV"},
	{FEATURE_RADIUS_SERVER, "MODULE_RG_RADIUS_SERVER"},
	{FEATURE_REDUCE_SUPPORT, "MODULE_RG_REDUCE_SUPPORT"},
	{FEATURE_USB_HOST, "CONFIG_HW_USB_STORAGE"},
	{FEATURE_USB_HOST, "CONFIG_HW_USB_HOST_UHCI"},
	{FEATURE_USB_HOST, "CONFIG_HW_USB_HOST_OHCI"},
	{FEATURE_USB_HOST, "CONFIG_HW_USB_HOST_EHCI"},
	{FEATURE_FIREWIRE_1394, "CONFIG_HW_FIREWIRE"},
	{FEATURE_FIREWIRE_1394, "CONFIG_HW_FIREWIRE_STORAGE"},
	{FEATURE_PSE, "MODULE_RG_PSE"},
	{FEATURE_WPS, "MODULE_RG_WPS"},
	{-1}
    };

static int is_valid_module(char *token)
{
    int feature_id;
    feature_id = str2code(modules, token);
    return (feature_id >= 0);
}

static int is_feature_enabled(char *module)
{
    int feature_id;

    if (no_lic)
	return 1;
    feature_id = str2code(modules, module);
    if (feature_id < 0)
	return 1; /* not checked by license => always enabled */
    return enabled_features[feature_id];
}
#else
static int is_valid_module(char *token)
{
    return 0;
}

static int is_feature_enabled(char *feature_name)
{
    return 1;
}
#endif


static int is_token_set_by_cmdline(char *token)
{
    option_t *o;
    
    o = option_token_get(openrg_config_options, token);
    return o->set_prio == SET_PRIO_CMD_LINE;
}

#define BAD_IMAGE_WARN "The created image will not run properly.\n"
#define BAD_IMAGE_INFO "Attention: " BAD_IMAGE_WARN

static void vtoken_set_internal(char *file, int line, set_prio_t set_prio, 
    char *token, const char *value_, va_list ap)
{
    option_t *opt;
    char value[4096];

    vsnprintf(value, sizeof(value), value_, ap);

    if (strlen(value) > sizeof(value) - 2)
    {
	fprintf(stderr, 
	    "In %s:%d Error setting token \"%s\" - value too long\n",
	    file, line, token);
	exit(-1);
    }

    opt = option_token_get(openrg_config_options, token);

    if (set_prio < opt->set_prio)
    {
	/* The option was already set with higher set priority than 'set_prio',
	 * so don't do anything */
	return;
    }

    if (!strcmp(value, "m"))
    {
	if (!opt->type & OPT_MODULE_EXPAND)
	{
	    conf_err("ERROR: Can't set %s to \"m\". %s is not defined"
		" as OPT_MODULE_EXPAND\n", token, token);
	}
	/* replace m to y when developing. */
	if (opt->type & OPT_STATIC_ON_DEVELOP && token_get("CONFIG_RG_DEV"))
	    sprintf(value, "y");
    }

    if (set_prio == opt->set_prio && opt->value && strcmp(opt->value, value))
    {
	/* Attempt to change a token value that has already been set before,
	 * with the same set priority as 'set_prio' */
	fprintf(stderr, "Replacing %s=%s (from %s:%d) with %s=%s from "
	    "(%s:%d)\n", token, opt->value, opt->file, opt->line, token,
	    value, file, line);
    }

    opt->file = file;
    opt->line = line;
    
    if (opt->value)
	free(opt->value); /* Free old value */
    opt->value = strdup(value);
    opt->set_prio = set_prio;
}

static void token_set_internal(char *file, int line, set_prio_t set_prio, 
    char *token, const char *value_, ...)
{
    va_list ap;

    va_start(ap, value_);
    vtoken_set_internal(file, line, set_prio, token, value_, ap);
    va_end(ap);
}

void _token_set(char *file, int line, set_prio_t set_prio, char *token, 
    const char *value_, ...)
{
    va_list ap;

    if (is_valid_module(token))
    {
	conf_err("ERROR: %s:%d: %s is a module (mapped to license features), "
	    "use enable_module instead of token_set*\n", file, line, token);
    }

    va_start(ap, value_);
    vtoken_set_internal(file, line, set_prio, token, value_, ap);
    va_end(ap);
}


void _enable_module(char *file, int line, char *name)
{
#ifndef CONFIG_RG_GPL    
    if (!is_valid_module(name))
    {
	conf_err("ERROR: %s:%d: %s is not a module (i.e. is not mapped to any " 
	    "license feature), use token_set* instead of enable_module\n", 
	    file, line, name);
    }
#endif

    if (token_is_y("LIC_AUTOCONF") && token_get("LIC_FORCE"))
	conf_err("ERROR: LIC_FORCE and LIC_AUTOCONF flags cannot co-exist\n");

    if (is_token_set_by_cmdline(name))
    {
	if (token_get(name))
	{
	    /* User has requested the module in the command line, by specifying 
	     * MODULE_...=y
	     */
	    if (!is_feature_enabled(name))
	    {
		if (token_get("LIC_FORCE"))
		{
		    /* Overriding license engine. This will most probably cause
		     * a halting image.
		     */
		    fprintf(stderr, 
			"WARNING: enabling %s although there no license. "
			BAD_IMAGE_WARN, name);
		}
		else
		{
		    /* No license and no override specified, halt build */
		    conf_err("ERROR: There is no valid license for %s. "
			"use LIC_FORCE=y with 'make config' comamnd line "
			"to override license enforcement. " BAD_IMAGE_INFO,
			name);
		}
	    }
	    token_set_internal(file, line, SET_PRIO_TOKEN_SET, name, "y");
	}
    }
    else
    {
	/* No requests for module in the command line */
	if (is_feature_enabled(name))
	    token_set_internal(file, line, SET_PRIO_TOKEN_SET, name, "y");
	else
	{
	    /* The user did not specify LIC_AUTOCONF command-line flag, and
	     * there is no valid license for this module.
	     */
	    if (token_get("LIC_FORCE"))
	    {
	        fprintf(stderr, 
	            "WARNING: %s:%d enabling %s although there is no "
		    "license. " BAD_IMAGE_WARN, file, line, name);
		_token_set_y(file, line, name);
            }	    
	    else
	    {
	        /* we do not have a license for a feature that was turned on
	         * indirectly.
	         */		
                if (token_is_y("LIC_AUTOCONF"))
	    	{
		    fprintf(stderr, 
		        "NOTICE: %s:%d: disabling %s (no license). use "
		        "LIC_AUTOCONF=n LIC_FORCE=y in 'make config' command "
		        "line to override license enforcement. "
			BAD_IMAGE_INFO, file, line, name);
	        }		
		else
		{
		    conf_err("ERROR: %s:%d: There is no valid license for %s.\n"
		        "Add %s=n, to the 'make config' command line to disable"
			"the module or add LIC_AUTOCONF=y to the 'make config' "
			"command line to automatically disable features you do "
			"not have a license for.\n", file, line, name, name);
		}
	    }
	}
    }
}

static void license_init(void)
{
#ifndef CONFIG_RG_GPL    
    char **features = NULL;
    char *output, *alloced_key = NULL;
    char *key = token_get_str("LICSTR");
    char *lic_file = token_get_str("LIC");
    int rc, i;

    if (key && lic_file)
	conf_err("LIC and LICSTR make-flags can't be used together\n");

    if (!key)
    {
	char *eoln;

	if (!lic_file && !(lic_file = set_dist_license()))
	{
	    char *hwid = sys_get(&rc, LIC_RG_APP " getid");
	    if (rc || !*hwid)
		str_cpy(&hwid, "0042aa");
	    conf_err(
		"\n"
		"A license key must be specified with:\n"
		"  LIC=/usr/local/openrg/etc/licenses/mycompany.lic\n"
		"file name (which includes the license string), or directly:\n"
		"  'LICSTR=1234567890.My Company'\n"
		"If you don't have a license - contact rg_support@jungo.com,\n"
		"to receive an evaluation or permanent license.\n"
		"Evaluation licenses may also be obtained directly from the\n"
		"web site: www.jungo.com/openrg\n"
		"If you own a license for an older version of the product -\n"
		"send the license file to rg_support@jungo.com to obtain a\n"
		"new license.\n"
		"Please send the following code when contacting Jungo: %s\n",
		hwid);
	    str_free(&hwid); /* won't get here */
	}

	if (!strcasecmp(lic_file, "n"))
	{
	    no_lic = 1;
	    return;
	}

	/* get license string from license file */
	alloced_key = key = sys_get(&rc, LIC_RG_APP " readlic %s", lic_file);
	if (!key || rc)
	    conf_err("invalid license file %s\n", lic_file);
	if ((eoln = strchr(key, '\n')))
	    *eoln = 0;
	token_set("LICSTR", key);
    }

    /* get list of features enabled by the given license */
    output = sys_get(&rc, LIC_RG_APP " info --features_only '%s'", key);
    if (!output || rc)
	conf_err("invalid license string %s\n", key);
    lines_str_split(&features, output, " ", 1);
    str_free(&output);

    for (i = 0; features[i]; i++)
	enabled_features[atoi(features[i])] = 1;
    lines_free(&features);

    /* check that the license is valid for this version */
    output = sys_get(&rc, LIC_RG_APP " ver %s '%s'", RG_VERSION, key);
    if (rc)
	conf_err("license is invalid for this version\n");
    str_free(&output);

    /* check if this is an evaluation license */
    output = sys_get(&rc, LIC_RG_APP " eval '%s'", key);
    if (rc)
	conf_err("failed to check evaluation mode of the license\n");
    is_evaluation = !str_cmpsub(output, "evaluation license");
    str_free(&output);

    if (!token_get("LIC_AUTOCONF"))
	token_set_y("LIC_AUTOCONF");

    str_free(&alloced_key);
#endif
}

/* Make sure CONFIGs doesn't exist more than once */
static void _validate_unique_configs(option_t *array)
{
    int i, j;
    
    for (i = 0; array[i].token; i++)
    {
	for (j = i+1; array[j].token; j++)
	{
	    if (!strcmp(array[i].token, array[j].token))
		rg_error(LEXIT, "%s is duplicated", array[j].token);
	}
    }
}

static void validate_unique_configs(void)
{
    _validate_unique_configs(openrg_hw_options);
    _validate_unique_configs(openrg_os_options);
    _validate_unique_configs(openrg_distribution_options);
    _validate_unique_configs(openrg_config_options);
}

static void _init_configs(option_t *array)
{
    option_t *p;
    
    for (p = array; p->token; p++)
    {
	/* If the token has a value, then change the value to a dynamically
	 * allocated one */
	if (p->value)
	{
	    p->set_prio = SET_PRIO_INIT;
	    p->value = strdup(p->value);
	}
    }
}

static void _uninit_configs(option_t *array)
{
    option_t *p;
    
    for (p = array; p->token; p++)
	nfree(p->value);
}

static void init_configs(void)
{
    _init_configs(openrg_hw_options);
    _init_configs(openrg_os_options);
    _init_configs(openrg_distribution_options);
    _init_configs(config_options_base);
}

static void uninit_configs(void)
{
    _uninit_configs(openrg_hw_options);
    _uninit_configs(openrg_os_options);
    _uninit_configs(openrg_distribution_options);
    _uninit_configs(config_options_base);
}

static option_t *options_dup(option_t *o)
{
    int i;
    option_t *n;

    for (i = 0; o[i].token; i++);

    n = zalloc((i+1)*sizeof(option_t));

    for (i = 0; o[i].token; i++)
    {
	n[i].token = strdup_n(o[i].token);
	n[i].value = strdup_n(o[i].value);
	n[i].type = o[i].type;
	n[i].description = strdup_n(o[i].description);
	n[i].file = strdup_n(o[i].file);
	n[i].line = o[i].line;
	n[i].set_prio = o[i].set_prio;
    }

    return n;
}

void jpkg_dist_add(char *dist)
{
    jpkg_dist_t *new = zalloc_e(sizeof(jpkg_dist_t));
    jpkg_dist_t **p;
    
    new->options = options_dup(config_options_base);
    new->dist = strdup(dist);

    /* Add to end of list, this is important if an added dist calls
     * jpkg_dist_add */
    for (p = &jpkg_dists; *p; p = &((*p)->next));
    *p = new;
}

static void configure_features(void)
{
    hardware_features();

    /* Adding common features */
    openrg_features();
    general_features();
}

static void jpkg_configuration(void)
{
    jpkg_dist_t *p;
    int i;

    char *orig_dist = dist;
    option_t *orig_options = openrg_config_options;
    
    for (p = jpkg_dists; p; p=p->next)
    {
	int i;

	openrg_config_options = p->options;

	token_set_y("CONFIG_RG_JPKG");

	/* Copy cmd-line preset values */
        for (i = 0; orig_options[i].token; i++)
	{
	    if (orig_options[i].set_prio == SET_PRIO_CMD_LINE)
	    {
                option_t *o;

	        o = option_token_get(openrg_config_options, 
		    orig_options[i].token);
		token_set(orig_options[i].token, orig_options[i].value);
		o->set_prio = SET_PRIO_CMD_LINE;
	    }
	}
	dist = p->dist;
	os = "";
	
	distribution_features();

	target_os_features(os);

	configure_features();
    }

    dist = orig_dist;
    openrg_config_options = orig_options;
    
    /* XXX for now we do not override defaults from config_opt.c */
    for (i = 0; openrg_config_options[i].token; i++)
    {
	if (openrg_config_options[i].value)
	    continue;
	for (p = jpkg_dists; p; p=p->next)
	{
	    if (p->options[i].value)
		openrg_config_options[i] = p->options[i];
	}   
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    init_configs();
    
    openrg_config_options = options_dup(config_options_base);
    validate_unique_configs();

    handle_opt(argc, argv);
    if (optind!=argc)
	usage();
#ifdef  CONFIG_RG_DO_DEVICES
    if (!dev_if_conf_file_name)
	conf_err("you must set dev_if conf file name (-e <path>)\n");
    dev_open_conf_file(dev_if_conf_file_name);
#endif

    /* Initialize the license info before initializing the dist */
    license_init();

    distribution_features();

    /* Check OS */
    if (*os && !option_token_get_nofail(openrg_os_options, os))
    {
	fprintf(stderr, "OS isn't legal: OS=%s\nLegal options are: ", os);
	options_print(openrg_os_options);
	exit(1);
    }

    if (!hw && *os)
	conf_err("Can't determine hw\n");

    /* Check dist and hw */

    if (dist && !option_token_get_nofail(openrg_distribution_options, dist))
    {
	fprintf(stderr, "Distribution isn't legal: DIST=%s\n"
	    "Legal options are: ", dist);
	options_print(openrg_distribution_options);
	exit(1);
    }

    if (hw && !option_token_get_nofail(openrg_hw_options, hw))
    {
	fprintf(stderr, "Hardware isn't legal: HW=%s\n", hw);
	options_print(openrg_hw_options);
	exit(1);
    }
    
    if (*os)
	target_os_features(os);

    configure_features();

    jpkg_configuration();

    target_primary_os();

    /* Host configuration */
    config_host();

    /* Save device set to file. */
    dev_close_conf_file();

    configure_print();
    print_major_features();

    uninit_configs();
    
    return 0;
}
