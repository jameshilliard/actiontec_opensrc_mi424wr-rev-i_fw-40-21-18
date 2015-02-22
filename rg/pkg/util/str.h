/****************************************************************************
 *
 * rg/pkg/util/str.h
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
/* SYNC: rg/pkg/util/str.h <-> project/tools/util/str.h */

#ifndef _STR_H_
#define _STR_H_

#include <string.h>
#include <stdarg.h>
#include <sys/types.h>

#include "rg_error.h"

int str_isempty(char *s);
int str_isspace(char *s);
int str_isnumber(char *s, int allow_ws);
int str_isxdigit(char *s);
int str_cmpsub(char *s, char *sub);
/* Compare each chars in s with string in sub until find 'delim' in s */
int str_cmpdelim(char *s, char *sub, char *delim);
/* like strcmp, but also handles NULL pointers */
int str_cmp(char *s1, char *s2);
int str_casecmpdelim(char *s, char *sub, char *delim);
/* like strcmp, but ignore ws */
int str_wscmp(char *s1, char *s2);

/* Copy at most 'size' characters from 'src' to 'dst'. 'size' is the size
 * of 'dst' MINUS 1 for the trailing '\0'.
 * Therefore, even if 'size' is 0, 'dst' must be at least 1 byte long in
 * order to accomodate the trailing '\0'.
 */
char *strncpyz(char *dst, char *src, int size);

char *itoa(int i);
char *utoa(unsigned int u);
char *ulltoa(unsigned long long num);
char *ftoa(double f);

char *strnstr(char *haystack, char *needle, int n);

char **str_tolower(char **s);
char **str_toupper(char **s);
char **str_cpytok(char **s, char *str, char *delim);
char **str_cpy(char **s, char *str);
char **str_ncpy(char **s, char *str, size_t len);
char **str_cat(char **s, char *str);
char **str_strcpytok(char **s, char *str, char *delim);
char **str_chomp(char **s);
char **str_ltrim(char **s);
char **str_rtrim(char **s);
char **str_trim(char **s);

#define PRINTF_CAT    1
#define PRINTF_ESCAPE 2

char **str_catprintf(char **s, char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));
char **str_printf(char **s, char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));
char **str_vprintf(char **s, char *fmt, va_list ap)
    __attribute__((nonnull(3)));
char **str_valid_terminate_nl(char **s);
char **str_vprintf_full(char **s, int flags, char *fmt, va_list ap)
    __attribute__((nonnull(4)));

/* 'from' is 0 based index */
char **str_mid(char **s, int from, int len);
char **str_left(char **s, int len);
char **str_right(char **s, int len);

void *strdup_log(rg_error_level_t level, char *s);

static inline char *strdup_e(char *s)
{
    return strdup_log(LEXIT, s);
}

static inline char *strdup_l(char *s)
{
    return strdup_log(LERR, s);
}

static inline int str_len(char *s)
{
    return s ? strlen(s) : 0;
}

size_t str_nlen(const char *s, size_t maxlen);

char *str_init(void);
char **str_alloc(char **s);

/* like strchr, but stops on multiple chars */
char *strchrs(char *s, char *chrs);
/* count the number of occurences of needle in haystack (no overlapping) */
int str_count_str(char *haystack, char *needle);
int str_count_chrs(char *s, char *chrs);

/* insert the replace string to str starting from "from". nremove chars are
 * removed from str */
char **str_insert(char **s, int from, int nremove, char *replace);
/* replace multiple sub-string search from str with replace */
char **str_replace(char **s, char *search, char *replace);

/* buf is a non null terminated string */
int str2buf(char *buf, char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));
void buf2str(char *str, char *buf, int buf_size);
char *strrevchr(char *s, int ch, char *endpos);
char **str_catfast(char **s, char *str, int *str_slen);

static inline char *str_nonull(char *s)
{
    return s ? s : "";
}

/* if s is null the function will return null */
char *strdup_n(char *s);

/* if s is null the function will return an empty string */
char *strdup_null(char *s);
char *strndup_null(char *s, int n);

char *strtoupper(char *s);

/* for use with pre-allocated buffers */
char *strcat_printf(char *buf, char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));
char *strchomp(char *s);
char *strltrim(char *s);
char *strrtrim(char *s);
char *strtrim(char *s);

/* use a string's memory, instead of duplicating it */
char **str_use(char **s, char *str);
char **str_free(char **s);

/* Return 1 if the char 'c' is acceptable in CLI */
int char_is_valid(unsigned char c);

/* Return 1 if all the characters of 's' are acceptable in CLI */
int charset_is_valid(char *s);

/* Return 1 if all the characters of 's' are valid for a username */
int username_charset_is_valid(char *s);

/* Return 1 if all the characters of 's' are valid for a Jnet username */
int jnet_username_charset_is_valid(char *s);
int jnet_password_charset_is_valid(char *s);

/* Return 1 if all the characters of 's' are valid for a SIP username */
int sip_username_charset_is_valid(char *s);

#endif

