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
 * The old configuration file must be buffered, selected lines are
 * replaced (added as necessary), then the whole thing written out.
 *
 * BUGS:
 * Does NOT properly support multiple-bssid case
 * (Atheros does not support multiple-bssid within single network
 * interface anywa).
 */

#ifndef config_rewrite__h
#define config_rewrite__h


/* config_rewrite -- opaque data structure */
struct config_rewrite;

/* config_rewrite_create -- start new editing session
 */
struct config_rewrite *config_rewrite_create(
        const char *old_file_path       /* file to modify from */
        );

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
        );

/* config_rewrite_write -- write result to a file
 * Returns nonzero if error.
 */
int config_rewrite_write(
        struct config_rewrite *rewrite,
        const char *out_file_path
        );

/* config_rewrite_free -- free resources from config_rewrite_create
 */
void config_rewrite_free(
        struct config_rewrite *rewrite
        );

#endif  /* config_rewrite__h */

