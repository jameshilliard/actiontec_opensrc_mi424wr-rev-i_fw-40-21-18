/****************************************************************************
 *
 * rg/pkg/include/rg_version.h
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
/* SYNC: rg/pkg/include/rg_version.h <-> project/tools/util/rg_version.h */

#ifndef _RG_VERSION_H_
#define _RG_VERSION_H_

#include "rg_version_data.h"
#include "external_version_data.h"

#define RG_MAJOR_NUM RG_VERSION_1
#define RG_MINOR_NUM RG_VERSION_2
#define RG_REVISION_NUM RG_VERSION_3

/* Get a version in rg format and convert it to a string format in a static
 * buffer
 */

/* Get full formatted version string in a static buffer */
char *rg_full_version_to_str(char *rg_ver, char *ext_ver);

/* Return an allocated int array terminated by -1 which contains 'version'
 * numbers. For example: "3.10.1.4.1" -> {3,10,1,4,1,-1}
 * Note that 'version' must be in dotted numbers format.
 */
int *rg_version_str2num(char *version);

/* returns 1 if version is of valid format:
 * - of form <number>.<number>.<number> 
 * - the number of numbers is odd (i.e. CVS tag, not branch)
 * - canonical form: no leading zeros in numbers
 */
int rg_version_is_valid(char *p);

/* return 1 if version is old style integer e.g. "30921" */
int rg_version_is_old(char *p);

/* returns -1, 0 , 1 if p <, ==, > q */
int rg_version_compare(char *p, char *q);

/* converts version into old style int, using only first three numbers */
int rg_version_new2old(char *p);

/* convert 30921 to "3.9.21" (stored in static buffer) */
char *rg_version_old2new(int v);

/* return 1 if v1 and v2 are the same, or only differ in the last digit */
int rg_version_is_same_branch(char *p, char *q); 

/* "3.10.1.4.1" -> 5 */
int rg_version_length(char *p);

extern char *rg_version;
extern char *external_version;

#endif
