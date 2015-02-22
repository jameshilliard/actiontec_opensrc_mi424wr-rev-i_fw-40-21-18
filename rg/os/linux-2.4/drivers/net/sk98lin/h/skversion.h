/******************************************************************************
 *
 * Name:	version.h
 * Project:	GEnesis, PCI Gigabit Ethernet Adapter
 * Version:	$Revision: 1.1.1.1 $
 * Date:	$Date: 2007/05/07 23:29:26 $
 * Purpose:	SK specific Error log support
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	(C)Copyright 1998-2002 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * History:
 *	$Log: skversion.h,v $
 *	Revision 1.1.1.1  2007/05/07 23:29:26  jungo
 *	Import of Jungo RG SDK 4.6.2
 *	
 *	Revision 1.3  2003/09/21 20:11:26  igork
 *	merge branch-dev-2421 into dev
 *	
 *	Revision 1.2.66.1  2003/09/18 21:10:08  igork
 *	linux-2.4.21-rmk1 integration
 *	
 *	Revision 1.1.2.1  2001/09/05 13:38:30  mlindner
 *	Removed FILE description
 *	
 *	Revision 1.1  2001/03/06 09:25:00  mlindner
 *	first version
 *	
 *	
 *
 ******************************************************************************/
 
 
static const char SysKonnectFileId[] = "@(#) (C) SysKonnect GmbH.";
static const char SysKonnectBuildNumber[] =
	"@(#)SK-BUILD: 6.02 PL: 01"; 

#define BOOT_STRING	"sk98lin: Network Device Driver v6.02\n" \
			"Copyright (C) 2000-2002 SysKonnect GmbH."

#define VER_STRING	"6.02"


