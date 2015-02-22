/****************************************************************************
 *
 * rg/os/linux-2.4/include/asm-arm/arch-ixp425/ixp425-pci.h
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

#ifndef _ASM_ARCH_IXP425_PCI_H_
#define _ASM_ARCH_IXP425_PCI_H_

#define NP_CMD_IOREAD                           0x2
#define NP_CMD_IOWRITE                          0x3
#define NP_CMD_MEMREAD				0x6
#define	NP_CMD_MEMWRITE				0x7
#define NP_CMD_CONFIGREAD                       0xa
#define NP_CMD_CONFIGWRITE                      0xb

#define IXP425_PCI_NP_CBE_BESL                  4

extern int (*ixp425_pci_read)(u32 addr, u32 cmd, u32* data);
extern int ixp425_pci_write(u32 addr, u32 cmd, u32 data);
extern void ixp425_pci_init(void *sysdata);
extern int ixp425_pci_is_host(void);

#endif

