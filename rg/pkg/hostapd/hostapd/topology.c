
/*
 * Topology file format:
 * Contains lines of the following lexical types:
 *      empty lines
 *      comments lines:  #...
 *      labelled lines:  keyword ...
 *      section begin:  { ...
 *      section end:    } ...
 * Sections (between section begin and end) belong to previous labelled line.
 * We recognize the following:
 * bridge <name>
 * {
 *      interface <name>
 * }
 * radio <name>
 * {
 *      ap 
 *      {
 *              config <filepath>
 *              override <tag>=<value>
 *              bss <interface_name>
 *              {
 *                      config <filepath>
 *                      override <tag>=<value>
 *              }
 *      }
 * }
 *
 * radio, ap, bss and override may be repeated within their section,
 * although for best results there should be only one "ap" within
 * a radio section.
 * Only one "config" (for configuration file) may appear per section.
 * Tagged values in config files or in overrides will override
 * similarly tagged previous configuration lines for the object.
 *
 */

#include "includes.h"
#include "topology.h"

static char *word_first(char *s)
{
        while (*s && !isgraph(*s)) s++;
        return s;
}

static char *word_next(char *s)
{
        while (isgraph(*s)) s++;
        while (*s && !isgraph(*s)) s++;
        return s;
}

static int word_empty(char *s)
{
        return (*s == 0 || *s == '#');
}


static int word_eq(char *s, char *match)
{
        if (!isgraph(*s)) return 0;
        while (isgraph(*s)) {
                if (*s != *match) return 0;
                s++, match++;
        }
        if (isgraph(*match)) return 0;
        return 1;
}

static int word_len(char *s)
{
        int len = 0;
        while (isgraph(*s)) s++, len++;
        return len;
}

/* If it fits, copy word with null termination into out and return 0.
 * Else return 1.
 */
static int word_get(char *s, char *out, int out_size)
{
        int len = word_len(s);
        int i;
        if (len >= out_size) return 1;
        for (i = 0; i < len; i++) out[i] = s[i];
        out[i] = 0;
        return 0;
}


/* Information we carry around while parsing a topology file 
 * and the config files it refers to
 */
struct topology_bridge;
struct topology_iface;
struct topology_bridge  {
        char name[IFNAMSIZ+1];
        /* Sibling bridges (linked list) */
        struct topology_bridge *next;
        struct topology_bridge *prev;
        /* child interfaces */
        struct topology_iface *ifaces;
};
struct topology_iface {
        char name[IFNAMSIZ+1];
        struct topology_bridge *bridge; /* parent */
        /* Sibling interfaces (linked list) */
        struct topology_iface *next;
        struct topology_iface *prev;
        int used;
};
struct topology_parse {
        int errors;
	struct hapd_interfaces *interfaces;
        struct topology_bridge *bridges;
        char *filepath;
        int line;
        FILE *f;
        char buf[256];
        char bridge_name[IFNAMSIZ+1];
        char radio_name[IFNAMSIZ+1];
	struct hostapd_iface *hapd_iface;
        char ap_config_filepath[256];
        char bss_config_filepath[256];
        char ifname[IFNAMSIZ+1];
};


static char * topology_line_get(
        struct topology_parse *p
        )
{
        char *s;
        if (fgets(p->buf, sizeof(p->buf), p->f) == NULL) {
                return NULL;
        }
        /* Make sure we got a full line. Chop off trailing whitespace */
        s = strchr(p->buf,'\r');
        if (s == NULL) s = strchr(p->buf,'\n');
        if (s == NULL) {
                wpa_printf(MSG_ERROR, "File %s line %d is overlong!",
                        p->filepath, p->line);
                p->buf[0] = 0;
        } else {
                *s = 0;
                while (s > p->buf) {
                        int ch = *--s;
                        if (isgraph(ch)) break;
                        else *s = 0;
                }
        }
        p->line++;
        /* Chop off leading whitespace */
        s = word_first(p->buf);
        if (word_empty(s)) *s = 0;      /* chop comments */
        return s;
}

/* Skip lines until we get a closing brace (recursive)
 */
static int topology_skip_section(
        struct topology_parse *p
        )
{
        char *s;
        int depth = 1;
        while ((s = topology_line_get(p)) != NULL) {
               if (*s == '{') depth++;
               else
               if (*s == '}') depth--;
               if (depth <= 0) break;
        }
        if (depth != 0) {
                wpa_printf(MSG_ERROR, 
                        "Topology file %s line %d: unbalanced braces",
                        p->filepath, p->line);
                p->errors++;
                return 1;
        }
        return 0;
}

/* skip lines until we get opening brace... error if non-empty
 * line found between.
 */
static int topology_find_section(
        struct topology_parse *p
        )
{
        char *s;
        int depth = 0;
        while ((s = topology_line_get(p)) != NULL) {
               if (*s == 0) continue;   /* allow empty lines */
               if (*s == '{') { depth++; break; }
               if (*s == '}') { depth--; break; }
               break;
        }
        if (depth <= 0) {
                wpa_printf(MSG_ERROR, 
                        "Topology file %s line %d: missing left brace",
                        p->filepath, p->line);
                p->errors++;
                return 1;
        }
        return 0;
}


static struct topology_bridge *topology_findbridge(
        struct topology_parse *p,
        char *name
        )
{
        struct topology_bridge *bridge = p->bridges;
        struct topology_bridge *first_bridge = bridge;
        if (bridge == NULL) return NULL;
        do {
                if (!strcmp(bridge->name, name)) {
                        return bridge;
                }
                bridge = bridge->next;
        } while (bridge != first_bridge);
        return NULL;
}

static struct topology_iface *topology_find_iface(
        struct topology_parse *p,
        char *name
        )
{
        struct topology_bridge *bridge = p->bridges;
        struct topology_bridge *first_bridge = bridge;
        if (bridge == NULL) return NULL;
        do {
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (iface != NULL) 
                do {
                        if (!strcmp(iface->name, name)) {
                                return iface;
                        }
                        iface = iface->next;
                } while (iface != first_iface);
                bridge = bridge->next;
        } while (bridge != first_bridge);
        return NULL;
}

static struct topology_bridge *topology_add_bridge(
        struct topology_parse *p
        )
{
        /* p->bridge_name contains bridge name */
        struct topology_bridge *bridge;
        bridge = topology_findbridge(p, p->bridge_name);
        if (bridge != NULL) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Duplicate bridge %s",
                        p->filepath, p->line, p->bridge_name);
                p->errors++;
                return NULL;
        }

        bridge = os_zalloc(sizeof(*bridge));
        if (bridge == NULL) {
                wpa_printf(MSG_ERROR, "Malloc error!");
                p->errors++;
                return NULL;
        }
        memcpy(bridge->name, p->bridge_name, sizeof(bridge->name));
        if (p->bridges) {
                bridge->next = p->bridges;
                bridge->prev = bridge->next->prev;
                bridge->next->prev = bridge;
                bridge->prev->next = bridge;
        } else {
                bridge->next = bridge->prev = bridge;
        }
        p->bridges = bridge;    /* point to "current" bridge */
        return bridge;
}

static struct topology_iface *topology_add_iface(
        struct topology_parse *p
        )
{
        /* p->ifname contains interface name; p->bridges points to bridge */
        struct topology_bridge *bridge = p->bridges;
        struct topology_iface *iface;
        iface = topology_find_iface(p, p->ifname);
        if (iface != NULL) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Duplicate iface %s",
                        p->filepath, p->line, p->ifname);
                p->errors++;
                return NULL;
        }
        iface = os_zalloc(sizeof(*iface));
        if (iface == NULL) {
                wpa_printf(MSG_ERROR, "Malloc error!");
                p->errors++;
                return NULL;
        }
        iface->bridge = bridge;
        memcpy(iface->name, p->ifname, sizeof(iface->name));
        if (bridge->ifaces) {
                iface->next = bridge->ifaces;
                iface->prev = iface->next->prev;
                iface->next->prev = iface;
                iface->prev->next = iface;
        } else {
                iface->next = iface->prev = iface;
        }
        bridge->ifaces = iface;    /* point to "current" iface */
        return iface;
}


static void topology_parse_bridge(
        struct topology_parse *p
        )
{
        char *s;
        struct topology_bridge *bridge;
        int error;

        /* Remember the bridge */
        bridge = topology_add_bridge(p);
        if (bridge == NULL) return;

        /* Find leading brace */
        if (topology_find_section(p)) return;

        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "interface")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->ifname, sizeof(p->ifname));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad interface name",
                                        p->filepath, p->line);
                                p->errors++;
                                strcpy(p->ifname, "?");
                        } else {
                                topology_add_iface(p);
                        }
                        continue;
                }
                /* skip unknown */
        }
        return;
}


static int topology_parse_bss_config_override(
        struct topology_parse *p,
        char *s         /* beware, this points into p->buf */
        )
{
        char *tag = s;
        char *eq = strchr(s, '=');
        char *value;

        if (eq == NULL) {
                wpa_printf(MSG_ERROR, "File %s line %d bad override",
                        p->filepath, p->line);
                return 1;
        }
        *eq = 0;      /* null terminate tag */
        value = word_first(eq+1);
        if (hostapd_bss_config_apply_line(
                        p->hapd_iface->conf->last_bss,
                        tag, value, p->line, 1/*internal*/)) {
                return 1;
        }
        return 0;
}



static void topology_parse_bss(
        struct topology_parse *p
        )
{
        /* p->ifname contains bss name */
        char *s;
        int error;
        struct topology_iface *iface;

        iface = topology_find_iface(p, p->ifname);
        if (iface == NULL) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Undeclared iface %s",
                        p->filepath, p->line, p->ifname);
                p->errors++;
                return;
        }
        if (iface->used) {
                wpa_printf(MSG_ERROR,
                        "File %s line %d Already used iface %s",
                        p->filepath, p->line, p->ifname);
                p->errors++;
                return;
        }
        iface->used++;

        /* Add to radio configuration */
        error = hostapd_config_bss(p->hapd_iface->conf, iface->name);
        if (error) {
                p->errors++;
                return;
        }

        /* Find leading brace */
        if (topology_find_section(p)) return;
        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "config")) {
                    s = word_next(s);
                    if (word_get( s, p->bss_config_filepath, 
                                    sizeof(p->bss_config_filepath))) {
                            wpa_printf(MSG_ERROR, "File %s line %d"
                                    " config requires path",
                                    p->filepath, p->line);
                            p->errors++;
                    } else {
                            p->errors += hostapd_bss_config_apply_file(
                                p->hapd_iface->conf,
                                p->hapd_iface->conf->last_bss, 
                                p->bss_config_filepath);
                    }
                    continue;
                }
                if (word_eq(s, "override")) {
                    s = word_next(s);
                    p->errors += topology_parse_bss_config_override(p, s);
                    continue;
                }
                /* skip unknown */
        }
        /* Now override interface name and bridge */
        if (hostapd_bss_config_apply_line(
                        p->hapd_iface->conf->last_bss,
                        "interface", iface->name, p->line, 1/*internal*/)) {
                p->errors++;
        }
        if (hostapd_bss_config_apply_line(
                        p->hapd_iface->conf->last_bss,
                        "bridge", iface->bridge->name, p->line, 1/*internal*/)) {
                p->errors++;
        }
        return;
}




static void topology_parse_ap_config_override(
        struct topology_parse *p,
        char *s         /* beware, this points into p->buf */
        )
{
        char *tag = s;
        char *eq = strchr(s, '=');
        char *value;

        if (eq == NULL) {
                wpa_printf(MSG_ERROR, "File %s line %d bad override",
                        p->filepath, p->line);
                p->errors++;
                return;
        }
        *eq = 0;      /* null terminate tag */
        value = word_first(eq+1);
        if (hostapd_radio_config_apply_line(
                        p->hapd_iface->conf, tag, value, p->line)) {
                wpa_printf(MSG_ERROR, "File %s line %d override failure",
                        p->filepath, p->line);
                p->errors++;
                return;
        }
        return;
}



static void topology_parse_ap(
        struct topology_parse *p
        )
{
        char *s;

        /* Add to "interfaces" (really, this is radios) */
        p->interfaces->count++;
        p->interfaces->iface = realloc(p->interfaces->iface, 
                p->interfaces->count*sizeof(p->interfaces->iface[0]));
        if (p->interfaces->iface == NULL) {
                wpa_printf(MSG_ERROR, "FATAL: realloc error");
                exit(1);
                p->errors++;
                return;
        }
        p->interfaces->iface[p->interfaces->count-1] = p->hapd_iface =
	        wpa_zalloc(sizeof(*p->hapd_iface));
        if (p->hapd_iface == NULL) {
                wpa_printf(MSG_ERROR, "Malloc error");
                p->errors++;
                return;
        }
        p->hapd_iface->conf = hostapd_radio_config_create();
        if (p->hapd_iface->conf == NULL) {
                p->errors++;
                return;
        }

        /* Find leading brace */
        if (topology_find_section(p)) return;
        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == 0) continue;
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "config")) {
                    s = word_next(s);
                    if (word_get( s, p->ap_config_filepath, 
                                    sizeof(p->ap_config_filepath))) {
                            p->errors++;
                            wpa_printf(MSG_ERROR, "File %s line %d"
                                    " config requires path",
                                    p->filepath, p->line);
                    } else {
                            if (hostapd_radio_config_apply_file(p->hapd_iface->conf,
                                                p->ap_config_filepath)) {
                                    p->errors++;
                                    wpa_printf(MSG_ERROR, 
                                            "From file %s line %d",
                                            p->filepath, p->line);
                            }
                    }
                    continue;
                }
                if (word_eq(s, "override")) {
                    s = word_next(s);
                    topology_parse_ap_config_override(p, s);
                    continue;
                }
                if (word_eq(s, "bss")) {
                        s = word_next(s);
                        if (word_get(s, p->ifname, sizeof(p->ifname))) {
                                p->errors++;
                                wpa_printf(MSG_ERROR, "File %s line %d"
                                        " bss requires interface name",
                                        p->filepath, p->line);
                    }
                    topology_parse_bss(p);
                    continue;
                }
                /* skip unknown */
        }
        return;
}

static void topology_parse_radio(
        struct topology_parse *p
        )
{
        char *s;
        /* Find leading brace */
        if (topology_find_section(p)) return;
        /* Now process lines within */
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (*s == '}') break;
                if (word_eq(s, "ap")) {
                    topology_parse_ap(p);
                    continue;
                }
                /* skip unknown */
        }
        return;
}


static void topology_find_radios(
        struct topology_parse *p
        )
{
        char *s;
        int error;
        while ((s = topology_line_get(p)) != NULL) {
                if (*s == '{') {
                        topology_skip_section(p);
                        continue;
                }
                if (word_eq(s, "bridge")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->bridge_name, sizeof(p->bridge_name));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad bridge name",
                                        p->filepath, p->line);
                                p->errors++;
                                strcpy(p->bridge_name, "?");
                        } else {
                                topology_parse_bridge(p);
                        }
                        continue;
                }
                if (word_eq(s, "radio")) {
                        s = word_next(s);
                        error = word_get(
                                s, p->radio_name, sizeof(p->radio_name));
                        if (error) {
                                wpa_printf(MSG_ERROR, 
                                        "File %s line %d Bad radio name",
                                        p->filepath, p->line);
                                p->errors++;
                                strcpy(p->radio_name, "?");
                        } else {
                                topology_parse_radio(p);
                        }
                        continue;
                }
                /* skip unknown */
        }
        return;
}


static void topology_clean(
        struct topology_parse *p
        )
{
        /* Only do normal cleaning (in case of no errors) */
        struct topology_bridge *bridge = p->bridges;
        struct topology_bridge *first_bridge = bridge;
        if (bridge) 
        do {
                struct topology_bridge *next_bridge = bridge->next;
                struct topology_iface *iface = bridge->ifaces;
                struct topology_iface *first_iface = iface;
                if (iface)
                do {
                        struct topology_iface *next_iface = iface->next;
                        free(iface);
                        iface = next_iface;
                } while (iface != first_iface);
                free(bridge);
                bridge = next_bridge;
        } while (bridge != first_bridge);
        free(p);
        return;
}

/* hostapd_config_read_topology_file reads a topology file,
 * which defines radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int hostapd_config_read_topology_file(
	struct hapd_interfaces *interfaces,
        char *filepath
        )
{
        struct topology_parse *p;
        int errors;

        printf("Reading topology file %s ...\n", filepath);
        p = malloc(sizeof(*p));
        if (p == NULL) {
                wpa_printf(MSG_ERROR, "Malloc failure.");
                return 1;
        }
        memset(p, 0, sizeof(*p));
        p->interfaces = interfaces;
        p->filepath = filepath;
        p->f = fopen(p->filepath, "r");
        if (p->f == NULL) {
                wpa_printf(MSG_ERROR, "Failed to open topology file: %s",
                        filepath);
                errors = 1;
        } else {
                topology_find_radios(p);
                fclose(p->f);
                errors = p->errors;
        }

        /* Don't bother to clean up if errors... we'll abort anyway */
        if (errors) return errors;
        topology_clean(p);
        return 0;
}


/* hostapd_config_read_topology_files reads one or more topology files,
 * which define radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 *
 * BUG: sanity checking between topology files is missing.
 */
int hostapd_config_read_topology_files(
	struct hapd_interfaces *interfaces,
        char **filepaths
        )
{
        int errors = 0;
        char *filepath;
        while ((filepath = *filepaths++) != NULL) {
                errors += hostapd_config_read_topology_file(interfaces, filepath);
        }
        return errors;
}


