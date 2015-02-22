/*
 * fileutil -- file manipulation utility functions
 * Copyright (c) 2008, Atheros Communications
 * Author: Ted Merrill
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
 * Miscellaneous utility functions for accessing regular files.
 */

#ifndef fileutil__h
#define fileutil__h     /* once only */

/* filesize -- returns size of a regular file.
 * Returns -1 if file does not exist or is not a regular file
 * or information cannot be obtained.
 */
int filesize(const char *filepath);

/* fileread -- returns malloc'd buffer holding content of regular file.
 *      Returns NULL if file cannot be read or memory allocated.
 *      Extra bytes of allocation may be obtained; anyway, at least 1 
 *      extra byte is allocated (and zeroed).
 */
char *fileread(
        const char *filepath,
        int extra,              /* how many bytes extra to allocate */
        int *buf_size_p,        /* NULL or out: size of allocated buffer */
        int *file_size_p        /* NULL or out: size of content in buffer */
        );

/* filewrite -- write buffer to file
 * Returns nonzero if error.
 */
int filewrite(
        const char *filepath,
        const char *buf,
        int buf_size
        );

#endif  /* fileutil__h */

