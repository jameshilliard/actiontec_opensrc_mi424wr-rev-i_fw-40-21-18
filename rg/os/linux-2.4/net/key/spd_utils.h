/* $USAGI: spd_utils.h,v 1.2 2002/08/12 00:38:40 miyazawa Exp $ */
/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __SPD_UTILS_H
#define __SPD_UTILS_H

#include <linux/skbuff.h>
#include <net/spd.h>

int compare_selector(struct selector *selector1, struct selector *selector2);

#endif /* __SPD_UTIILS_H */

