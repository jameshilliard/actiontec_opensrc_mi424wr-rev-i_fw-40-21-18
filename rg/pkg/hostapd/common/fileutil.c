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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

/* filesize -- returns size of a regular file.
 * Returns -1 if file does not exist or is not a regular file
 * or information cannot be obtained.
 */
int filesize(const char *filepath)
{
    struct stat Stat;
    if (stat(filepath, &Stat)) return -1;
    if (!S_ISREG(Stat.st_mode)) return -1;
    return Stat.st_size;
}


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
        )
{
    int file_size;
    int fd = -1;
    char *buf = NULL;
    int buf_size;

    if (buf_size_p) *buf_size_p = 0;
    if (file_size_p) *file_size_p = -1;
    file_size = filesize(filepath);
    if (file_size < 0) return NULL;
    if (file_size_p) *file_size_p = file_size;
    if (extra <= 0) extra = 1;
    buf_size = file_size + 1;
    buf = malloc(buf_size);
    if (buf == NULL) goto fail;
    if (buf_size_p) *buf_size_p = buf_size;
    fd = open(filepath, O_RDONLY);
    if (fd < 0) goto fail;
    if (read(fd, buf, file_size) != file_size) goto fail;
    close(fd);
    return buf;
    
    fail:
    if (fd >= 0) close(fd);
    if (buf) free(buf);
    return NULL;
}

/* filewrite -- write buffer to file
 * Returns nonzero if error.
 */
int filewrite(
        const char *filepath,
        const char *buf,
        int buf_size
        )
{
    int fd = -1;

    fd = open(filepath, O_CREAT|O_TRUNC|O_WRONLY, 0666);
    if (fd < 0) return -1;
    if (write(fd, buf, buf_size) != buf_size) {
        close(fd);
        (void) unlink(filepath);
        return -1;
    }
    close(fd);
    return 0;
}


