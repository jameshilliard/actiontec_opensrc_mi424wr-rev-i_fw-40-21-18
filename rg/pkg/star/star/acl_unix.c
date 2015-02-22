/* @(#)acl_unix.c	1.8 02/04/20 Copyright 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)acl_unix.c	1.8 02/04/20 Copyright 2001 J. Schilling";
#endif
/*
 *	ACL get and set routines for unix like operating systems.
 *
 *	Copyright (c) 2001 J. Schilling
 *
 *	This implementation currently supports POSIX.1e and Solaris ACLs.
 *	Thanks to Andreas Gruenbacher <ag@bestbits.at> for the first POSIX ACL
 *	implementation.
 *
 *	As True64 does not like ACL "mask" entries and this version of the
 * 	ACL code does not generate "mask" entries on True64, ACl support for
 *	True64 is currently broken. You cannot read back archives created
 *	on true64.
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <mconfig.h>
#ifdef	USE_ACL
/*
 * HAVE_ANY_ACL currently includes HAVE_POSIX_ACL and HAVE_SUN_ACL.
 * This definition must be in sync with the definition in star_unix.c
 * As USE_ACL is used in star.h, we are not allowed to change the
 * value of USE_ACL before we did include star.h or we may not include
 * star.h at all.
 * HAVE_HP_ACL is currently not included in HAVE_ANY_ACL.
 */
#	ifndef	HAVE_ANY_ACL
#	undef	USE_ACL		/* Do not try to get or set ACLs */
#	endif
#endif

#ifdef	USE_ACL
#include <stdio.h>
#include <errno.h>
#include "star.h"
#include "props.h"
#include "table.h"
#include <standard.h>
#include <stdxlib.h>	/* Needed for Solaris ACL code (malloc/free) */
#include <unixstd.h>
#include <dirdefs.h>
#include <strdefs.h>
#include <statdefs.h>
#include <schily.h>
#include "starsubs.h"

#ifdef	HAVE_SYS_ACL_H
#	include <sys/acl.h>
#endif

#define	ROOT_UID	0

extern	int	uid;
extern	BOOL	nochown;
extern	BOOL	numeric;

/*
 * XXX acl_access_text/acl_default_text are a bad idea. (see xheader.c)
 * XXX Note that in 'dirmode' dir ACLs get hosed because getinfo() is
 * XXX called for the directory before the directrory content is written
 * XXX and the directory itself is archived after the dir content.
 */
LOCAL char acl_access_text[PATH_MAX+1];
LOCAL char acl_default_text[PATH_MAX+1];

EXPORT	BOOL	get_acls	__PR((FINFO *info));
EXPORT	void	set_acls	__PR((FINFO *info));

#ifdef	HAVE_POSIX_ACL
LOCAL	BOOL	acl_to_info	__PR((char *name, int type, char *acltext));
LOCAL	BOOL	acl_add_ids	__PR((char *infotext, char *acltext));
#endif

#ifdef	HAVE_SUN_ACL
LOCAL	char	*acl_add_ids	__PR((char *dst, char *from, char *end, int *sizep));
#endif

LOCAL	char	*base_acl	__PR((int mode));
LOCAL	void	acl_check_ids	__PR((char *acltext, char *infotext));


#ifdef HAVE_POSIX_ACL

/*
 * Get the access control list for a file and convert it into the format used by star.
 */
EXPORT BOOL
get_acls(info)
	register FINFO	*info;
{
	info->f_acl_access = NULL;
	info->f_acl_default = NULL;

	/*
	 * Symlinks don't have ACLs
	 */
	if (is_symlink(info))
		return (TRUE);

	if (!acl_to_info(info->f_name, ACL_TYPE_ACCESS, acl_access_text))
		return (FALSE);
	if (*acl_access_text != '\0') {
		info->f_xflags |= XF_ACL_ACCESS;
		info->f_acl_access = acl_access_text;
	}
	if (!is_dir(info))
		return (TRUE);
	if (!acl_to_info(info->f_name, ACL_TYPE_DEFAULT, acl_default_text))
		return (FALSE);
	if (*acl_default_text != '\0') {
		info->f_xflags |= XF_ACL_DEFAULT;
		info->f_acl_default = acl_default_text;
	}
	return (TRUE);
}

LOCAL BOOL
acl_to_info(name, type, acltext)
	char	*name;
	int	type;
	char	*acltext;
{
	acl_t	acl;
	char	*text, *c;
	int entries = 1;

	acltext[0] = '\0';
	if ((acl = acl_get_file(name, type)) == NULL) {
		register int err = geterrno();
#ifdef	ENOTSUP
		/*
		 * This FS does not support ACLs.
		 */
		if (err == ENOTSUP)
			return (TRUE);
#endif
#ifdef	ENOSYS
		if (err == ENOSYS)
			return (TRUE);
#endif
		errmsg("Cannot get %sACL for '%s'.\n",
			type==ACL_TYPE_DEFAULT?"default ":"", name);
		xstats.s_getaclerrs++;
		return (FALSE);
	}
	seterrno(0);
	text = acl_to_text(acl, NULL);
	acl_free(acl);
	if (text == NULL) {
		if (geterrno() == 0)
			seterrno(EX_BAD);
		errmsg("Cannot convert %sACL entries to text for '%s'.\n",
			type==ACL_TYPE_DEFAULT?"default ":"", name);
		xstats.s_badacl++;
		return (FALSE);
	}

	/* remove trailing newlines */
	c = strrchr(text, '\0');
	while (c > text && *(c-1) == '\n')
		*(--c) = '\0';
	
	/* count fields */
	for (c = text; *c != '\0'; c++) {
		if (*c == '\n') {
			*c = ',';
			entries++;
		}
	}
	if (entries > 3) { /* > 4 on Solaris? */
		if (!acl_add_ids(acltext, text)) {
			acl_free((acl_t)text);
			return (FALSE);
		}
	}
	/*
	 * XXX True64 prints a compile time warning when we use
	 * XXX acl_free(text) but it is standard...
	 * XXX we need to check whether we really have to call
	 * XXX free() instead of acl_free() if we are on True64.
	 * XXX Cast the string to acl_t to supress the warning.
	 */
/*	free(text);*/
	acl_free((acl_t)text);
	return (TRUE);
}

LOCAL BOOL
acl_add_ids(infotext, acltext)
	char	*infotext;
	char	*acltext;
{
	int	size = PATH_MAX;
	int	len;
	char	*token;
	Ulong	id;

	/*
	 * Add final nul to guarantee that the string is nul terminated.
	 */
	infotext[PATH_MAX] = '\0';

	token = strtok(acltext, ", \t\n\r");
	while (token) {
		strncpy(infotext, token, size);
		infotext += strlen(token);
		size -= strlen(token);
		if (size < 0)
			size = 0;

		if (!strncmp(token, "user:", 5) &&
		    !strchr(":, \t\n\r", token[5])) {
			char *username = &token[5], *c = username+1;

			while (!strchr(":, \t\n\r", *c))
				c++;
			*c = '\0';
			/* check for all-numeric user name */
			while (c > username && isdigit(*(c-1)))
				c--;
			if (c > username &&
			    uidname(username, c-username, &id)) {
				len = js_snprintf(infotext, size,
					":%ld", id);
				infotext += len;
				size -= len;
			}
		} else if (!strncmp(token, "group:", 6) &&
		           !strchr(":, \t\n\r", token[6])) {
			char *groupname = &token[6], *c = groupname+1;

			while (!strchr(":, \t\n\r", *c))
				c++;
			*c = '\0';
			/* check for all-numeric group name */
			while (c > groupname && isdigit(*(c-1)))
				c--;
			if (c > groupname &&
			    gidname(groupname, c-groupname, &id)) {
				len = js_snprintf(infotext, size,
					":%ld", id);
				infotext += len;
				size -= len;
			}
		}
		if (size > 0) {
			*infotext++ = ',';
			size--;
		}
		
		token = strtok(NULL, ", \t\n\r");
	}
	if (size >= 0) {
		*(--infotext) = '\0';
	} else {
		errmsgno(EX_BAD, "Cannot convert ACL entries (string too long).\n");
		xstats.s_badacl++;
		return (FALSE);
	}
	return (TRUE);
}

/*
 * Use ACL info from archive to set access control list for the file if needed.
 */
EXPORT void
set_acls(info)
	register FINFO	*info;
{
	char	acltext[PATH_MAX+1];
	acl_t	acl;

	if (info->f_xflags & XF_ACL_ACCESS) {
		acl_check_ids(acltext, info->f_acl_access);
	} else {
		/*
		 * We may need to delete an inherited ACL.
		 */
		strcpy(acltext,  base_acl(info->f_mode));
	}
	if ((acl = acl_from_text(acltext)) == NULL) {
		errmsg("Cannot convert ACL '%s' to internal format for '%s'.\n",
		       acltext, info->f_name);
		xstats.s_badacl++;
	} else {
		if (acl_set_file(info->f_name, ACL_TYPE_ACCESS, acl) < 0) {
			/*
			 * XXX What should we do if errno is ENOTSUP/ENOSYS?
			 */ 
			errmsg("Cannot set ACL '%s' for '%s'.\n",
				acltext, info->f_name);
			xstats.s_setacl++;

			/* Fall back to chmod */
/* XXX chmod has already been done! */
/*			chmod(info->f_name, (int)info->f_mode);*/
		}
		acl_free(acl);
	}
	
	/*
	 * Only directories can have Default ACLs
	 */
	if (!is_dir(info))
		return;

	if (info->f_xflags & XF_ACL_DEFAULT) {
		acl_check_ids(acltext, info->f_acl_default);
	} else {
		acltext[0] = '\0';
#ifdef	HAVE_ACL_DELETE_DEF_FILE
		/*
		 * FreeBSD does not like acl_from_text("")
		 */
		if (acl_delete_def_file(info->f_name) < 0) {
			/*
			 * XXX What should we do if errno is ENOTSUP/ENOSYS?
			 */ 
			errmsg("Cannot remove default ACL from '%s'.\n", info->f_name);
			xstats.s_setacl++;
		}
		return;
#endif
	}
	if ((acl = acl_from_text(acltext)) == NULL) {
		errmsg("Cannot convert default ACL '%s' to internal format for '%s'.\n",
		       acltext, info->f_name);
		xstats.s_badacl++;
	} else {
		if (acl_set_file(info->f_name, ACL_TYPE_DEFAULT, acl) < 0) {
			/*
			 * XXX What should we do if errno is ENOTSUP/ENOSYS?
			 */ 
			errmsg("Cannot set default ACL '%s' for '%s'.\n",
				acltext, info->f_name);
			xstats.s_setacl++;
		}
		acl_free(acl);
	}
}

#endif  /* HAVE_POSIX_ACL */

#ifdef	HAVE_SUN_ACL	/* Solaris */

/*
 * Get the access control list for a file and convert it into the format used by star.
 */
EXPORT BOOL
get_acls(info)
	register FINFO	*info;
{
		 int		aclcount;
		 aclent_t	*aclp;
	register char		*acltext;
	register char		*ap;
	register char		*dp;
	register char		*cp;
	register char		*ep;
		 int		asize;
		 int		dsize;

	info->f_acl_access = NULL;
	info->f_acl_default = NULL;

	/*
	 * Symlinks don't have ACLs
	 */
	if (is_symlink(info))
		return (TRUE);

	if ((aclcount = acl(info->f_name, GETACLCNT, 0, NULL)) < 0) {
		errmsg("Cannot get ACL count for '%s'.\n", info->f_name);
		xstats.s_getaclerrs++;
		return (FALSE);
	}
#ifdef	ACL_DEBUG
	error("'%s' acl count %d\n", info->f_name, aclcount);
#endif
	if (aclcount <= MIN_ACL_ENTRIES) {
		/*
		 * This file has only the traditional UNIX access list.
		 * This case includes a filesystem that does not support ACLs
		 * like the tmpfs.
		 */
		return (TRUE);
	}
	if ((aclp = (aclent_t *)malloc(sizeof(aclent_t) * aclcount)) == NULL) {
		errmsg("Cannot malloc ACL buffer for '%s'.\n", info->f_name);
		xstats.s_getaclerrs++;
		return (FALSE);
	}
	if (acl(info->f_name, GETACL, aclcount, aclp) < 0) {
		errmsg("Cannot get ACL entries for '%s'.\n", info->f_name);
		xstats.s_getaclerrs++;
		return (FALSE);
	}
	seterrno(0);
	acltext = acltotext(aclp, aclcount);
	free(aclp);
	if (acltext == NULL) {
		if (geterrno() == 0)
			seterrno(EX_BAD);
		errmsg("Cannot convert ACL entries to text for '%s'.\n", info->f_name);
		xstats.s_badacl++;
		return (FALSE);
	}
#ifdef	ACL_DEBUG
	error("acltext '%s'\n", acltext);
#endif

	ap = acl_access_text;
	dp = acl_default_text;
	asize = PATH_MAX;
	dsize = PATH_MAX;

	for (cp = acltext; *cp; cp = ep) {
		if (*cp == ',')
			cp++;
		ep = strchr(cp, ',');
		if (ep == NULL)
			ep = strchr(cp, '\0');
			
		if (*cp == 'd' && strncmp(cp, "default", 7) == 0) {
			cp += 7;
			dp = acl_add_ids(dp, cp, ep, &dsize);
			if (dp == NULL)
				break;
		} else {
			ap = acl_add_ids(ap, cp, ep, &asize);
			if (ap == NULL)
				break;
		}
	}
	if (ap == NULL || dp == NULL) {
		acl_access_text[0] = '\0';
		acl_default_text[0] = '\0';
		errmsgno(EX_BAD, "Cannot convert ACL entries (string too long).\n");
		xstats.s_badacl++;
		return (FALSE);
	}

	if (ap > acl_access_text && ap[-1] == ',')
		--ap;
	*ap = '\0';
	if (dp > acl_default_text && dp[-1] == ',')
		--dp;
	*dp = '\0';

	if (*acl_access_text != '\0') {
		info->f_xflags |= XF_ACL_ACCESS;
		info->f_acl_access = acl_access_text;
	}
	if (*acl_default_text != '\0') {
		info->f_xflags |= XF_ACL_DEFAULT;
		info->f_acl_default = acl_default_text;
	}

#ifdef	ACL_DEBUG
error("access:  '%s'\n", acl_access_text);
error("default: '%s'\n", acl_default_text);
#endif

	return (TRUE);
}

/*
 * Convert Solaris ACL text into POSIX ACL text and add numerical user/group
 * ids.
 *
 * Solaris uses only one colon in the ACL text format for "other" and "mask".
 * Solaris ACL text is:	"user::rwx,group::rwx,mask:rwx,other:rwx"
 * while POSIX text is:	"user::rwx,group::rwx,mask::rwx,other::rwx"
 */
LOCAL char *
acl_add_ids(dst, from, end, sizep)
	char	*dst;
	char	*from;
	char	*end;
	int	*sizep;
{
	register char	*cp = from;
	register char	*ep = end;
	register char	*np = dst;
	register int	size = *sizep;
	register int	amt;
		 Ulong	id;

	if (cp[0] == 'u' &&
	    strncmp(cp, "user:", 5) == 0) {
		if (size <= (ep - cp +1)) {
			*sizep = 0;
			return (NULL);
		}
		size -= ep - cp +1;
		strncpy(np, cp, ep - cp +1);
		np += ep - cp + 1;

		cp += 5;
		ep = strchr(cp, ':');
		if (ep)
			*ep = '\0';
		if (*cp) {
			if (uidname(cp, 1000, &id)) {
				if (np[-1] == ',') {
					--np;
					size++;
				}
				amt = js_snprintf(np, size,
					":%ld,", id);
				np += amt;
				size -= amt;
			}
		}
		if (ep)
			*ep = ':';

	} else if (cp[0] == 'g' &&
	     strncmp(cp, "group:", 6) == 0) {
		if (size <= (ep - cp +1)) {
			*sizep = 0;
			return (NULL);
		}
		size -= ep - cp +1;
		strncpy(np, cp, ep - cp + 1);
		np += ep - cp + 1;

		cp += 6;
		ep = strchr(cp, ':');
		if (ep)
			*ep = '\0';
		if (*cp) {
			if (gidname(cp, 1000, &id)) {
				if (np[-1] == ',') {
					--np;
					size++;
				}
				amt = js_snprintf(np, size,
					":%ld,", id);
				np += amt;
				size -= amt;
			}
		}
		if (ep)
			*ep = ':';

	} else if (cp[0] == 'm' &&
	    strncmp(cp, "mask:", 5) == 0) {
		cp += 4;
		if (size < 5) {
			*sizep = 0;
			return (NULL);
		}
		/*
		 * Add one additional ':' to the string for POSIX compliance.
		 */
		strcpy(np, "mask:");
		np += 5;
		if (size <= (ep - cp +1)) {
			*sizep = 0;
			return (NULL);
		}
		strncpy(np, cp, ep - cp + 1);
		np += ep - cp + 1;

	} else if (cp[0] == 'o' &&
	    strncmp(cp, "other:", 6) == 0) {
		cp += 5;
		if (size < 6) {
			*sizep = 0;
			return (NULL);
		}
		/*
		 * Add one additional ':' to the string for POSIX compliance.
		 */
		strcpy(np, "other:");
		np += 6;
		if (size <= (ep - cp +1)) {
			*sizep = 0;
			return (NULL);
		}
		strncpy(np, cp, ep - cp + 1);
		np += ep - cp + 1;
	}
	if (size <= 0) {
		size = 0;
		np = NULL;
	}
	*sizep = size;
	return (np);
}

/*
 * Convert ACL info from archive into Sun's format and set access control list
 * for the file if needed.
 */
EXPORT void
set_acls(info)
	register FINFO	*info;
{
	int		aclcount;
	aclent_t	*aclp;
	char		acltext[PATH_MAX+1];
	char		aclbuf[8192];

	aclbuf[0] = '\0';
	if (info->f_xflags & XF_ACL_ACCESS) {
		acl_check_ids(aclbuf, info->f_acl_access);
	}
	if (info->f_xflags & XF_ACL_DEFAULT) {
		register char *cp;
		register char *dp;
		register char *ep;

		acl_check_ids(acltext, info->f_acl_default);

		dp = aclbuf + strlen(aclbuf);
		if (dp > aclbuf)
			*dp++ = ',';
		for (cp = acltext; *cp; cp = ep) {
			/*
			 * XXX Eigentlich muesste man hier bei den Eintraegen
			 * XXX "mask" und "other" jeweils ein ':' beseitigten
			 * XXX aber es sieht so aus, als ob es bei Solaris 9
			 * XXX auch funktionert wenn man das nicht tut.
			 * XXX Nach Solaris 7 "libsec" Source kann es nicht
			 * XXX mehr funktionieren wenn man das ':' beseitigt.
			 * XXX Moeglicherweise ist das der Grund warum
			 * XXX Solaris immer Probleme mit den ACLs hatte.
			 */
			if (*cp == ',')
				cp++;
			ep = strchr(cp, ',');
			if (ep == NULL)
				ep = strchr(cp, '\0');
			strcpy(dp, "default");
			dp += 7;
			strncpy(dp, cp, ep - cp + 1);
			dp += ep - cp + 1;
		}
	}
#ifdef	ACL_DEBUG
	error("aclbuf: '%s'\n", aclbuf);
#endif

	if (aclbuf[0] == '\0') {
		/*
		 * We may need to delete an inherited ACL.
		 */
		strcpy(aclbuf, base_acl(info->f_mode));
	}

	seterrno(0);
	if ((aclp = aclfromtext(aclbuf, &aclcount)) == NULL) {
		if (geterrno() == 0)
			seterrno(EX_BAD);
		errmsg("Cannot convert ACL '%s' to internal format for '%s'.\n",
		       aclbuf, info->f_name);
		xstats.s_badacl++;
	} else {
		if (acl(info->f_name, SETACL, aclcount, aclp) < 0) {
			/*
			 * XXX What should we do if errno is ENOSYS?
			 */ 
			errmsg("Cannot set ACL '%s' for '%s'.\n",
				aclbuf, info->f_name);
			xstats.s_setacl++;
		}
		free(aclp);
	}
}

#endif	/* HAVE_SUN_ACL Solaris */


/*
 * Convert UNIX sdtandard mode bits into base ACL
 */
LOCAL char *
base_acl(mode)
	int	mode;
{
	static char _acltxt[] = "user::***,group::***,other::***";

	_acltxt[ 6] = (mode & 0400) ? 'r' : '-';
	_acltxt[ 7] = (mode & 0200) ? 'w' : '-';
	_acltxt[ 8] = (mode & 0100) ? 'x' : '-';
	_acltxt[17] = (mode & 0040) ? 'r' : '-';
	_acltxt[18] = (mode & 0020) ? 'w' : '-';
	_acltxt[19] = (mode & 0010) ? 'x' : '-';
	_acltxt[28] = (mode & 0004) ? 'r' : '-';
	_acltxt[29] = (mode & 0002) ? 'w' : '-';
	_acltxt[30] = (mode & 0001) ? 'x' : '-';

	return (_acltxt);
}

/*
 * If we are in -numeric mode, we replace the user and groups names by the
 * user and group numbers from our internal format.
 *
 * If we are non non numeric mode, we check whether a user name or group name
 * is present on our current system. It the user/group name is known, then we
 * remove the numeric value from out internal format. If the user/group name
 * is not known, then we replace the name by the numeric value.
 */
LOCAL void
acl_check_ids(acltext, infotext)
	char	*acltext;
	char	*infotext;
{
	char	entry_buffer[PATH_MAX];
	char	*token = strtok(infotext, ", \t\n\r");

	if (!token)
		return;

	while (token) {

		if (!strncmp(token, "user:", 5) &&
		    !strchr(":, \t\n\r", token[5])) {
			char *username = &token[5], *c = username+1;
			char *perms, *auid;
			Ulong dummy;
			/* uidname does not check for NULL! */

			/* user name */
			while (!strchr(":, \t\n\r", *c))
				c++;
			if (*c)
				*c++ = '\0';

			/* permissions */
			perms = c;
			while (!strchr(":, \t\n\r", *c))
				c++;
			if (*c)
				*c++ = '\0';

			/* identifier */
			auid = c;
			while (!strchr(":, \t\n\r", *c))
				c++;
			if (*c)
				*c++ = '\0';

			/*
			 * XXX We use strlen(username)+1 to tell uidname not
			 * XXX to stop comparing before the end of the
			 * XXX username has been reached. Otherwise "joe" and
			 * XXX "joeuser" would be compared as identical.
			 */
			if (*auid && (numeric ||
			    !uidname(username, strlen(username)+1, &dummy)))
				username = auid;
			js_snprintf(entry_buffer, PATH_MAX, "user:%s:%s",
				username, perms);
			token = entry_buffer;

		} else if (!strncmp(token, "group:", 6) &&
		    !strchr(":, \t\n\r", token[6])) {
			char *groupname = &token[6], *c = groupname+1;
			char *perms, *agid;
			Ulong dummy;
			/* gidname does not check for NULL! */

			/* group name */
			while (!strchr(":, \t\n\r", *c))
				c++;
			if (*c)
				*c++ = '\0';

			/* permissions */
			perms = c;
			while (!strchr(":, \t\n\r", *c))
				c++;
			if (*c)
				*c++ = '\0';

			/* identifier */
			agid = c;
			while (!strchr(":, \t\n\r", *c))
				c++;
			if (*c)
				*c++ = '\0';
			
			/*
			 * XXX We use strlen(groupname)+1 to tell gidname not
			 * XXX to stop comparing before the end of the
			 * XXX groupname has been reached. Otherwise "joe" and
			 * XXX "joeuser" would be compared as identical.
			 */
			if (*agid && (numeric ||
			    !gidname(groupname, strlen(groupname)+1, &dummy)))
				groupname = agid;
			js_snprintf(entry_buffer, PATH_MAX, "group:%s:%s",
				groupname, perms);
			token = entry_buffer;
		}
		strcpy(acltext, token);
		acltext += strlen(token);

		token = strtok(NULL, ", \t\n\r");
	}
	*(--acltext) = '\0';
}

#endif  /* USE_ACL */
