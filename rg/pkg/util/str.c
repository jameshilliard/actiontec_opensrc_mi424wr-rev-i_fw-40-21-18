/****************************************************************************
 *
 * rg/pkg/util/str.c
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
/* SYNC: rg/pkg/util/str.c <-> project/tools/util/str.c */

#define _GNU_SOURCE /* For Linux's string.h */
#define OS_INCLUDE_STRING
#define OS_INCLUDE_STDARG
#define OS_INCLUDE_STD
#include <os_includes.h>

#include "str.h"
#include "alloc.h"
#include "rg_error.h"
#include "util.h"

size_t str_nlen(const char *s, size_t maxlen)
{
    int i;
    for (i=0; i<maxlen && s[i]; i++);
    return i;
}

size_t strnlen(const char *s, size_t maxlen);
#if defined(__CYGWIN__) || defined(__SUNOS__)
size_t strnlen(const char *s, size_t maxlen)
{
    return str_nlen(s, maxlen);
}
#endif

char *str_init(void)
{
    return strdup("");
}

char **str_alloc(char **s)
{
    nfree(*s);
    *s = strdup("");
    return s;
}

int str_cmpsub(char *s, char *sub)
{
    return strncmp(s ? s : "", sub ? sub : "", strlen(sub ? sub : ""));
}

/* like strcmp, but also handles NULL pointers */
int str_cmp(char *s1, char *s2)
{
    return strcmp(s1 ? s1 : "", s2 ? s2 : "");
}

int str_cmpdelim(char *s, char *sub, char *delim)
{
    int i;
    for (i = 0; s[i] && !strchr(delim, s[i]); i++);
    if (!s[i])
	return strcmp(s, sub);
    if (i != strlen(sub))
	return 1;
    return strncmp(s, sub, i);
}

int str_casecmpdelim(char *s, char *sub, char *delim)
{
    int i;
    for (i = 0; s[i] && !strchr(delim, s[i]); i++);
    if (!s[i])
	return strcasecmp(s, sub);
    if (i != strlen(sub))
	return 1;
    return strncasecmp(s, sub, i);
}

static inline void ws_skip(char **p)
{
    while (isspace(**p))
	(*p)++;
}
	
/* increment *p skipping ws */
static inline void inc(char **p)
{
    (*p)++;
    ws_skip(p);
}

int str_wscmp(char *p, char *q)
{
    ws_skip(&p);
    ws_skip(&q);
    for (; *p && *p == *q; inc(&p), inc(&q));
    return *q - *p;
}

char *strncpyz(char *dst, char *src, int n)
{
    strncpy(dst, src, n);
    dst[n] = 0;
    return dst;
}

char *strnstr(char *haystack, char *needle, int n)
{
    int needle_len = strlen(needle);
    int pos;
    for (pos = 0; pos+needle_len<=n; pos++)
    {
	if (haystack[pos]==needle[0] &&
	    !memcmp(haystack+pos, needle, needle_len))
	{
	    return haystack+pos;
	}
    }
    return NULL;
}

char *itoa(int num)
{
    static char ret[16];

    *ret = 0;
    snprintf(ret, sizeof(ret), "%d", num);
    return ret;
}

char *utoa(unsigned int num)
{
    static char ret[16];

    *ret = 0;
    snprintf(ret, sizeof(ret), "%u", num);
    return ret;
}

char *ulltoa(unsigned long long num)
{
    static char ret[21];

    *ret = 0;
    snprintf(ret, sizeof(ret), "%llu", num);
    return ret;
}

char *ftoa(double f)
{
    static char s[80];
    snprintf(s, sizeof(s)-2, "%f", f);
    s[sizeof(s)-1] = 0;
    return s;
}

int str_isempty(char *s)
{
    return !s || !*s;
}

int str_isspace(char *s)
{
    if (!s)
	return 1;
    ws_skip(&s);
    return *s==0;
}

/* tests whether this is a valid number */
int str_isnumber(char *s, int allow_ws)
{
    int i;
    if (allow_ws)
	ws_skip(&s);
    for (i=0; isdigit((int)*s); s++, i++);
    if (i==0 || i>10)
	return 0;
    if (allow_ws)
	ws_skip(&s);
    if (*s)
	return 0;
    return 1;
}

int str_isxdigit(char *s)
{
    if (!s)
	return 0;
    for (; isxdigit((int)*s); s++);
    return !*s;
}

char **str_tolower(char **s)
{
    char *p;
    
    if (!*s)
	return str_alloc(s);
    for (p = *s; *p; p++)
        *p = tolower(*p);
    return s;
}

char **str_toupper(char **s)
{
    char *p;
    
    if (!*s)
	return str_alloc(s);
    for (p = *s; *p; p++)
        *p = toupper(*p);
    return s;
}

char **str_cpy(char **s, char *str)
{
    char *tmp = *s;
    *s = strdup_null(str);
    nfree(tmp);
    return s;
}

char **str_ncpy(char **s, char *str, size_t len)
{
    char *tmp = *s;
    *s = strndup_null(str, len);
    nfree(tmp);
    return s;
}

char **str_cat(char **s, char *str)
{
    char *news;
    int len;
    if (!*s)
    {
	*s = strdup(str);
	return s;
    }
    len = strlen(*s);
    news = malloc_e(len+strlen(str)+1);
    strcpy(news, *s);
    strcpy(news+len, str);
    free(*s);
    *s = news;
    return s;
}

char **str_left(char **s, int len)
{
    int n;
    if (!*s)
	return str_alloc(s);
    n = strlen(*s);
    if (n<=len)
	return s;
    (*s)[len] = 0;
    return s;
}

char **str_right(char **s, int len)
{
    int n;
    if (!*s)
	return str_alloc(s);
    n = strlen(*s);
    if (n<=len)
	return s;
    memmove(*s, *s+n-len, len+1);
    return s;
}

/* from is 0 based index */
char **str_mid(char **s, int from, int len)
{
    int n;
    if (!s)
	return str_alloc(s);
    n = strlen(*s);
    if (from>n)
    {
	(*s)[0] = 0;
	return s;
    }
    if (from+len>n)
	n = from+len;
    memmove(*s, *s+from, n+1);
    return s;
}

/* strip whitespace from end of string */
char **str_chomp(char **s)
{
    return str_rtrim(s);
}

char **str_rtrim(char **s)
{
    if (!*s)
	return str_alloc(s);
    strrtrim(*s);
    return s;
}

char *strrtrim(char *s)
{
    char *str;
    for (str=s+strlen(s); str>s && isspace((int)(str[-1])); str--);
    *str = 0;
    return s;
}

char *strchomp(char *s)
{
    return strrtrim(s);
}

/* strip whitespace from beginning of string */
char *strltrim(char *s)
{
    char *str = s;

    ws_skip(&str);
    memmove(s, str, strlen(str)+1);
    return s;
}

char **str_ltrim(char **s)
{
    if (!*s)
	return str_alloc(s);
    strltrim(*s);
    return s;
}

/* strip whitespace from beginning and end of string */
char **str_trim(char **s)
{
    return str_rtrim(str_ltrim(s));
}

char *strtrim(char *s)
{
    return strrtrim(strltrim(s));
}

char **str_cpytok(char **s, char *str, char *delim)
{
    str_cpy(s, str);
    strtok(*s, delim);
    return s;
}

void *strdup_log(rg_error_level_t level, char *s)
{
    char *str;

    if (!(str = strdup(s)))
	rg_error(level, "strdup() failed");
    return str;
}

char **str_vprintf_full(char **s, int flags, char *fmt, va_list ap)
{
    int rc, rc2;
    int cat_len = 0;
    char *old_s = NULL;
    va_list ap2;

    if ((flags & PRINTF_CAT) && *s)
	cat_len = strlen(*s);
    /* New version of vsnprintf returns the number of characters that would
     * have been written if the buffer was big enough, excluding the trailing
     * NULL.
     */
    va_copy(ap2, ap);
#if defined(__SUNOS__)
    /* a hack for Sun OS - vsnprintf(bufsize=0) returns -1, instead of
     * the expected formatted length. Giving it a buffer of 1 char solves
     * this problem.
     */
    {
        char b;
	rc = vsnprintf(&b, 1, fmt, ap2);
    }
#else
    rc = vsnprintf(NULL, 0, fmt, ap2);
#endif
    va_end(ap2);

    if (rc<0)
	rg_error(LEXIT, "Error in printf format");
    if (!cat_len)
    {
	old_s = *s;
	*s = NULL;
    }
    *s = realloc_e(*s, cat_len+rc+1);
    rc2 = vsnprintf(*s+cat_len, rc+1, fmt, ap);
    nfree(old_s);
    if (rc2 < 0)
	rg_error(LEXIT, "Failed vnsprintf"); /* this should never happen */
    return s;
}

char **str_catprintf(char **s, char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    str_vprintf_full(s, PRINTF_CAT, fmt, arg);
    va_end(arg);
    return s;
}

char **str_printf(char **s, char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    str_vprintf_full(s, 0, fmt, arg);
    va_end(arg);
    return s;
}

char **str_vprintf(char **s, char *fmt, va_list ap)
{
    va_list arg;
    char **ret;
    va_copy(arg, ap);
    ret = str_vprintf_full(s, 0, fmt, arg);
    va_end(arg);
    return ret;
    
}

char **str_valid_terminate_nl(char **s)
{
    if (!*s)
	return str_alloc(s);
    if (!(*s)[0] || (*s)[strlen(*s)-1]=='\n')
	return s;
    return str_cat(s, " ");
}

/* like strchr, but stops on multiple chars */
char *strchrs(char *s, char *chrs)
{
    for (; *s && !strchr(chrs, *s); s++);
    return *s ? s : NULL;
}

int str_count_str(char *haystack, char *needle)
{
    int count = 0;

    while ((haystack = strstr(haystack, needle)))
    {
	count++;
	haystack+=strlen(needle);
    }
    return count;
}

int str_count_chrs(char *s, char *chrs)
{
    int nchrs = 0;
    for (; *s; s++)
    {
	if (strchr(chrs, *s))
	    nchrs++;
    }
    return nchrs;
}

char **str_insert(char **s, int from, int nremove, char *replace)
{
    int src_len = str_len(*s);
    int rep_len = str_len(replace);
    char *str;

    if (!*s)
	*s = strdup("");
    from = MIN(src_len, from);
    nremove = MIN(src_len-from, nremove);
    str = (char *)malloc_e(src_len + rep_len - nremove + 1);
    memcpy(str, *s, from);
    memcpy(str+from, replace, rep_len);
    memcpy(str+from+rep_len, *s+from+nremove, src_len-nremove-from);
    str[src_len + rep_len - nremove] = 0;
    nfree(*s);
    *s = str;
    return s;
}

char **str_replace(char **s, char *search, char *replace)
{
    char *found;
    int i;
    int search_len = strlen(search);
    int replace_len = strlen(replace);

    if (!*s)
	return str_alloc(s);
    for (i = 0; (found = strstr(*s + i, search)); i += replace_len)
    {
	i = found-*s;
	str_insert(s, found-*s, search_len, replace);
    }
    return s;
}

int str2buf(char *buf, char *fmt, ...)
{
    va_list ap;
    char *s = NULL;
    int len;

    va_start(ap, fmt);
    str_vprintf_full(&s, 0, fmt, ap);
    va_end(ap);
    if (!s)
	return -1;
    len = strlen(s);
    memcpy(buf, s, len);
    free(s);

    return len;
}

void buf2str(char *str, char *buf, int buf_size)
{
    memcpy(str, buf, buf_size);
    str[buf_size] = 0;
}

/* searches for ch from endpos backwords, until beginning of str.
 * returns NULL if ch not found
 */
char *strrevchr(char *str, int ch, char *endpos)
{
    for (; endpos>=str && *endpos!=ch; endpos--);
    return endpos>=str ? endpos : NULL;
}

/* strlen should point on entry to the length of str, excluding the terminating
 * '\0'.
 */
char **str_catfast(char **s, char *str, int *str_slen)
{
    int slen;
    if (!*s)
	*s = strdup("");
    if (!str)
	return s;
    slen = strlen(str);
    *s = realloc_e(*s, *str_slen+slen+1);
    strcpy(*s + *str_slen, str);
    *str_slen += slen;
    return s;
}

/* for use with pre-allocated buffers */
char *strcat_printf(char *buf, char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = vsnprintf(buf+strlen(buf), 0x7fffffff, fmt, ap);
    va_end(ap);
    return buf;
}

char **str_use(char **s, char *str)
{
    nfree(*s);
    if (!str)
	*s = strdup("");
    else
	*s = str;
    return s;
}

char **str_free(char **s)
{
    nfree(*s);
    *s = NULL;
    return s;
}

char *strdup_n(char *s)
{
    return s ? strdup_null(s) : NULL;
}

char *strdup_null(char *s)
{
    return strdup(s ? s : "");
}

char *strndup_null(char *s, int n)
{
    int len;
    char *str;
    
    if (!s)
	return strdup("");
    len = str_nlen(s, n);
    str = malloc_e(len+1);
    memcpy(str, s, len);
    str[len] = 0;
    return str;
}

char *strtoupper(char *s)
{
    char *p;
    
    if (!s)
	return NULL;
    for (p = s; *p; p++)
        *p = toupper(*p);
    return s;
}

int char_is_valid(unsigned char c)
{
    return c>=32 && c<=126;
}

int charset_is_valid(char *s)
{
    for (; s && *s && char_is_valid(*s); s++);
    return !s || !*s;
}

int username_charset_is_valid(char *s)
{
    char reject[] = " :@\"|\\/=+<>[]*?,;()${}^\%&^";
    return !s || (charset_is_valid(s) && strcspn(s, reject)==strlen(s));
}

static int jnet_charset_is_valid(char *s)
{
    char reject[] = " :@\"|\\/=+<>[]*?,;_";

    return !s || (charset_is_valid(s) && strcspn(s, reject)==strlen(s));
}

int jnet_username_charset_is_valid(char *s)
{
    char *s1 = NULL;
    int rc;

    rc = jnet_charset_is_valid(s);
    if (!rc)
	goto Exit;
    /* We don't allow capital letters for jnet username */
    str_tolower(str_cpy(&s1, s));
    rc = !strcmp(s1, s);
Exit:
    str_free(&s1);
    return rc;
}

int jnet_password_charset_is_valid(char *s)
{
    return jnet_charset_is_valid(s);
}

int sip_username_charset_is_valid(char *s)
{
    char reject[] = " :@\"|\\=+<>[]?,;";

    return !s || (charset_is_valid(s) && strcspn(s, reject)==strlen(s));
}

