/*
 * hostapd / Re-write configuration file
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2008, Atheros Communications
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

/* Description:
 * This supports dynamic editing of hostapd.conf files.
 * The old configuration file must be buffered, selected lines are
 * replaced (added as necessary), then the whole thing written out.
 * Old comments and old order of lines are retained to a large degree,
 * except that:
 * -- all fields of the same section are pulled together
 * -- redundant fields are replaced with last value, but at the
 *      location of the first value
 * -- missing fields are placed after the first apparently commented
 *      out instance of the field
 * -- comments on section begin/end are lost
 * -- leading whitespace is normalized
 * -- other?
 *
 * LIMITATIONS:
 * Does NOT properly support multiple-bssid case
 * (Atheros does not support multiple-bssid within single network
 * interface anyway).
 */

#include "includes.h"
#ifndef CONFIG_NATIVE_WINDOWS
#include <grp.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "hostapd.h"
// #include "driver.h"
// #include "sha1.h"
// #include "eap.h"
// #include "radius_client.h"
#include "wpa_common.h"

#ifdef EAP_WPS
#ifndef USE_INTEL_SDK
#include "wps_config.h"
#endif /* USE_INTEL_SDK */
#endif /* EAP_WPS */

#include "configedit.h"
#include "fileutil.h"
#include "config_rewrite.h"

#define EXTRA_BYTES 512 /* how much extra to allocate */
/* MARKER_LINE is used as a place holder so that a section is never
 * empty (which is not supported properly by configedit).
 */
#define MARKER_LINE "#auto-edited#\n"
#define INDENT_TEXT "        "
#define MAX_SECTIONS 20

struct config_rewrite_section {
    char *section_name;       /* NULL for section 0 */
    struct configedit *outer;       /* container for whole section */
    struct configedit *bound;       /* temp use within section */
    struct config_rewrite_section *next;        /* linked list */
};

struct config_rewrite {
    struct configedit *edit;    /* outer bound of whole file */
    int nsections;
    int nerrors;
    /* section 0 is the unnamed non-section.
     * Additional sections can be added.
     */
    struct config_rewrite_section section0;
};


/* tag_equal -- private helper to compare two tags or lines beginning
 * with tags.
 * Leading blanks/tabs skipped; tag ends with whitespace or equal sign.
 * Returns nonzero if tags are equal.
 * (missing tags always returns false == 0).
 */
static int tag_equal(const char *t1, const char *t2)
{
    /* skip leading whitespace */
    while (*t1 != '\n' && !isgraph(*t1)) t1++;    /* skip white */
    while (*t2 != '\n' && !isgraph(*t2)) t2++;    /* skip white */
    if (*t1 == '#' || *t1 == '=' || !isgraph(*t1)) 
            return 0; /* empty never matches */
    for (;;) {
        if (*t1 == '#' || *t1 == '=' || !isgraph(*t1)) {
            /* end of t1 */
            if (*t2 == '#' || *t2 == '=' || !isgraph(*t2)) return 1;  /* match */
        }
        if (*t1 != *t2) return 0;   /* no match */
        t1++; t2++;
    }
}

/* tag_value -- return part after tag=
 *      Returns NULL if no value (no equal sign or nothing after)
 */
static char *tag_value(char *line)
{
    while (*line != '\n' && !isgraph(*line)) line++;    /* skip white */
    while (*line != '=' && *line != '#' && isgraph(*line)) line++; /* tag */
    while (*line != '\n' && !isgraph(*line)) line++;    /* skip white */
    if (*line != '=') return NULL;      /* no equal sign */
    line++;
    while (*line != '\n' && !isgraph(*line)) line++;    /* skip white */
    if (!isgraph(*line)) return NULL;
    return line;
}

/* tag_dup -- return copy of tag, in allocated memory
 */
static char *tag_dup(const char *line) 
{
    int nbytes = 0;
    char *copy;
    while (*line != '\n' && !isgraph(*line)) line++;    /* skip white */
    for (;;) {
        if (line[nbytes] == '#' || line[nbytes] == '=' || 
                !isgraph(line[nbytes])) {
            break;
        }
        nbytes++;
    }
    if (nbytes == 0) return NULL;
    copy = os_malloc(nbytes+1);
    if (copy == NULL) return NULL;
    strncpy(copy, line, nbytes+1);
    copy[nbytes] = 0;
    return copy;
}

/* config_rewrite_section_find -- find or allocate new section info
 */
static struct config_rewrite_section *config_rewrite_section_find(
        struct config_rewrite *rewrite,
        const char *name_line  /* terminated by #, =, whitespace */
        )
{
    struct config_rewrite_section *section = &rewrite->section0;
    struct config_rewrite_section *last_section = NULL;
    char *section_name = NULL;
    struct configedit *outer = NULL;
    struct configedit *bound = NULL;

    for (;;) {
        last_section = section;
        section = section->next;
        if (section == NULL) break;
        if (tag_equal(name_line, section->section_name)) 
            return section;
    }
    section_name = tag_dup(name_line);
    if (section_name == NULL) {
        goto fail;
    }
    section = os_malloc(sizeof(*section));
    if (section == NULL) {
        os_free(section_name);
        goto fail;
    }
    memset(section, 0, sizeof(*section));
    last_section->next = section;
    section->section_name = section_name;
    /* put anonymous outer wrapper to hold outer text */
    if ((outer = configedit_begin(rewrite->edit)) == NULL) goto fail;
    configedit_seekend(outer);
    if ((bound = configedit_begin(outer)) == NULL) goto fail;
    configedit_text_append_string(bound, section_name);
    configedit_text_append_string(bound, "={\n");
    /* We can't have empty sections because configedit will fail */
    configedit_next(bound);
    configedit_text_append_string(bound, INDENT_TEXT);
    if (configedit_text_append_string(bound, MARKER_LINE)) goto fail;
    if ((section->outer = configedit_dup(bound)) == NULL) {
        goto fail;
    }
    if (section_name) {
        configedit_text_append_string(bound, "}\n");
    }
    configedit_free(bound);
    if ((section->bound = configedit_begin(section->outer)) == NULL) {
        goto fail;
    }
    return section;

    fail:
    if (section_name) os_free(section_name);
    if (section) os_free(section);
    /* leave bounds dangling, will be cleaned up when main edit is ... */
    return NULL;
}


/* config_rewrite_create
 */
struct config_rewrite *config_rewrite_create(
        const char *old_file_path       /* file to modify from */
        )
{
    struct config_rewrite *rewrite = NULL; /*return value */
    struct configedit *old = NULL;      /*temp*/
    struct configedit *old_bound = NULL;            /*temp*/
    struct configedit *bound = NULL;            /*temp*/
    int buf_size = 0;
    int file_size = -1;
    char *readbuf = NULL;
    char *editbuf = NULL;
    struct config_rewrite_section *section;
    char *section_name = NULL;
    char *tagvalue;


    /* To make our job easier, we rewrite all of the sections.
     * This allows us to combined duplicated fields and sections.
     * This has the danger of scrambling some comments that
     * appeared immediately before a section...
     * We use "old" to read from the old.
     */

    readbuf = fileread(old_file_path, 2/*extra*/, &buf_size, &file_size);
    if (readbuf == NULL) goto fail;
    readbuf[file_size] = '\n';  /* ensure newline termination */
    old = configedit_create(readbuf, buf_size, file_size);
    if (old == NULL) goto fail;
    old_bound = configedit_begin(old);
    if (old_bound == NULL) goto fail;

    buf_size += EXTRA_BYTES;
    editbuf = os_malloc(buf_size);
    if (editbuf == NULL) goto fail;
    rewrite = os_malloc(sizeof(*rewrite));
    if (rewrite == NULL) return NULL;
    memset(rewrite, 0, sizeof(*rewrite));
    rewrite->edit = configedit_create(editbuf, buf_size, 0);
    if (rewrite->edit == NULL) {
        goto fail;
    }

    /* Establish the unnamed non-section */
    if ((rewrite->section0.outer = configedit_begin(
                rewrite->edit)) == NULL) {
        goto fail;
    }
    if ((rewrite->section0.bound = configedit_begin(
                rewrite->section0.outer)) == NULL) {
        goto fail;
    }
    /* We can't have empty sections because configedit will fail */
    if (configedit_text_append_string(rewrite->section0.bound,
            MARKER_LINE)) goto fail;

    /* Now go through line by line and add to the appropriate section.
     * This will also kill any duplicates that have snuck in
     * (hostapd would keep the last one, so we do too).
     */
    bound = rewrite->section0.bound;
    section_name = NULL;
    for (;;) {
        char *ptr;
        int nbytes = configedit_next_line(old_bound);
        if (nbytes == 0) {
            break; /* EOF */
        }
        configedit_seekend(bound);
        ptr = configedit_ptr(old_bound);
        if (ptr[nbytes-1] != '\n') {
            /* fix lack of newline on last old line */
            ptr[nbytes++] = '\n'; /* we allocated enough room! */
        }
        /* skip indentation */
        while (!isgraph(*ptr) && *ptr != '\n') ptr++, nbytes--;
        /* Skip any old marker lines */
        if (!memcmp(ptr, MARKER_LINE, strlen(MARKER_LINE))) {
            continue;
        }
        if (*ptr == '#' || *ptr == '\n') {
            /* copy over decoration lines */
            if (section_name) {
                configedit_text_append_string(bound, INDENT_TEXT);
            }
            configedit_text_append(bound, ptr, nbytes);
            continue;
        }
        if (*ptr == '=') {
            /* comment illegal line */
            if (section_name) {
                configedit_text_append_string(bound, INDENT_TEXT);
            }
            configedit_text_append(bound, "# ", 2);
            configedit_text_append(bound, ptr, nbytes);
            continue;
        }
        if (*ptr == '}') {
            /* end of section */
            if (section_name == NULL) {
                /* we're not in a section ... ignore */
                continue;
            }
            /* note: any comment on this line will be lost */
            bound = rewrite->section0.bound;
            section_name = NULL;
            continue;
        }
        tagvalue = tag_value(ptr);
        if (! tagvalue) {
            /* empty values are not supported... they break some code */
            continue;   /* skip */
        }
        if (*tagvalue == '{') {
            /* named section */
            if (section_name) {
                /* section within section, not allowed */
                configedit_text_append_string(bound, "#illegal# ");
                configedit_text_append(bound, ptr, nbytes);
                continue;
            }
            section = config_rewrite_section_find(rewrite, ptr);
            if (section == NULL) goto fail;
            /* any comments on this line are lost */
            bound = section->bound;
            section_name = section->section_name;
            continue;
        }
        /* use generic function to write the value... this will 
         * replace previous, otherwise append to end
         */
        if (config_rewrite_line(rewrite, ptr, section_name)) goto fail;
    }

    /* Free of old also frees child bounds and readbuf */
    configedit_free(old);
    return rewrite;

    fail:
    config_rewrite_free(rewrite);
    /* Free of old also frees child bounds and readbuf */
    configedit_free(old);
    if (readbuf) os_free(readbuf);
    if (editbuf) os_free(editbuf);
    return NULL;
}


/* config_rewrite_remove_others -- remove tagged line(s)
 * within parent bound starting at line after current bound
 * (private helper function)
 */
static void config_rewrite_remove_others(
        struct configedit *bound,
        const char *tag
        )
{
    for (;;) {
        int nbytes = configedit_next_line(bound);
        if (nbytes == 0) break; /* end of file */
        if (tag_equal(tag, configedit_ptr(bound))) {
            configedit_text_resize(bound, 0);   /* should not fail */
        }
    }
}

/* config_rewrite_line -- replace etc. tagged line.
 * Input line has format tag=value...<newline>
 * section_name is terminated by #, =, whitespace or null.
 *
 * Return value: nonzero if error (e.g. allocation failure)
 *
 * BUGS: 
 * -- This must do special things for some tags but doesn't know all
 *  such special cases.
 */
int config_rewrite_line(
        struct config_rewrite *rewrite,
        const char *line,       /* newline terminated */
        const char *section_name     /* NULL or name of special section */
        )
{
    struct config_rewrite_section *section;
    int line_len;
    struct configedit *bound;

    for (line_len = 0; line[line_len] != '\n'; line_len++) {;}
    if (line_len == 0) return 0;        /* silently drop empty lines */
    line_len++; /* count newline also */

    /* Section 0 (unnamed non-section) is pre-setup */
    if (section_name == NULL) {
        section = &rewrite->section0;
    } else {
        section = config_rewrite_section_find(rewrite, section_name);
        if (section == NULL) goto fail;
    }

    bound = section->bound;
    /* Look for the line directly and replace it */
    configedit_rewind(bound);
    for (;;) {
        int nbytes = configedit_next_line(bound);
        if (nbytes == 0) break; /* end of file */
        if (tag_equal(line, configedit_ptr(bound))) {
            if (section_name) {
                if (configedit_text_replace_string(bound, INDENT_TEXT)) 
                    goto fail;
                if (configedit_text_append(bound, line, line_len)) 
                    goto fail;
            } else {
                if (configedit_text_replace(bound, line, line_len)) 
                    goto fail;
            }
            /* Remove any other occurrences! */
            config_rewrite_remove_others(bound, line);
            goto found;
        }
    }
    /* Look for commented line, and add after */
    configedit_rewind(bound);
    for (;;) {
        char *ptr;
        int nbytes = configedit_next_line(bound);
        if (nbytes == 0) break; /* end of file */
        ptr = configedit_ptr(bound);
        while (*ptr == '#') ptr++;
        if (tag_equal(line, ptr) && tag_value(ptr) != NULL ) {
            if (section_name) {
                if (configedit_text_append_string(bound, INDENT_TEXT))
                    goto fail;
            }
            if (configedit_text_append(bound, line, line_len)) goto fail;
            goto found;
        }
    }
    /* Put at end of file */
    configedit_seekend(bound);
    if (section_name) {
        if (configedit_text_append_string(bound, INDENT_TEXT))
            goto fail;
    }
    if (configedit_text_append(bound, line, line_len)) goto fail;
    goto found;

    found:
    /* Remove conflicting tags */
    if (section_name == NULL) {
        if (tag_equal(line, "wpa_passphrase")) {
            configedit_rewind(bound);
            config_rewrite_remove_others(bound, "wpa_psk");
        } else
        if (tag_equal(line, "wpa_psk")) {
            configedit_rewind(bound);
            config_rewrite_remove_others(bound, "wpa_passphrase");
        }
    }

    return 0;   /* success */

    fail:
    return ++rewrite->nerrors;
}


/* config_rewrite_write -- write result to a file
 * Returns nonzero if error.
 */
int config_rewrite_write(
        struct config_rewrite *rewrite,
        const char *out_file_path
        )
{
    if (rewrite->nerrors) return 1;
    return filewrite(out_file_path, configedit_ptr(rewrite->edit), 
        configedit_size(rewrite->edit));
}


/* config_rewrite_free -- free resources from config_rewrite_create
 */
void config_rewrite_free(
        struct config_rewrite *rewrite
        )
{
    int section;
    struct config_rewrite_section *next_section;
    if (rewrite == NULL) return;
    next_section = rewrite->section0.next;
    for (section = 1; section < rewrite->nsections; section++) {
        struct config_rewrite_section *section = next_section;
        next_section = section->next;
        os_free(section->section_name);
        os_free(section);
    }
    /* Note! configedit_free frees all bounds! */
    if (rewrite->edit) configedit_free(rewrite->edit);
    os_free(rewrite);
    return;
}



