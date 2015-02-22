/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/vendor/marvell/feroceon/rd-88f6560-gw/flash_layout.c
 *
 *  Developed by Jungo LTD.
 *  Residential Gateway Software Division
 *  www.jungo.com
 *  info@jungo.com
 *
 *  This file is part of the OpenRG Software and may not be distributed,
 *  sold, reproduced or copied in any way.
 *
 *  This copyright notice should not be removed
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <perm_storage/flash.h>
#include <util/rg_error.h>

/* Flash layout for 16MB flash size, although we might have more available */
flash_section_layout_t flash_layout[] = {
#if 0
    {
    	/* uBoot */
        offset: 0x00000000,
	size:   0x00200000, /* 2MB */
	type:   FLASH_SECT_BOOT,
    },
#endif
    {
	/* Factory Settings */
        offset: 0x0,
	size:   0x00020000, /* 128k */
	type:   FLASH_SECT_FACTORY,
	flags:	FLASH_SECT_WRITE_WARNING,
	s: {
            type: FLASH_SEC_STRUCT_DEFAULT,
	    u: { def: { filename: CONFIG_RG_FFS_MNT_DIR "/rg_factory"}, },
	},
    },
    {
    	/* RG conf 1 */
        offset: 0x0,
	size:   0x00020000, /* 128k */
	type:   FLASH_SECT_CONF,
	s: {
            type: FLASH_SEC_STRUCT_DEFAULT,
	    u: { def: { filename: CONFIG_RG_FFS_MNT_DIR "/rg_conf1"}, },
	},
    },
    {
    	/* RG conf 2 */
        offset: 0x0,
	size:   0x00020000, /* 128k */
	type:   FLASH_SECT_CONF,
	s: {
            type: FLASH_SEC_STRUCT_DEFAULT,
	    u: { def: { filename: CONFIG_RG_FFS_MNT_DIR"/rg_conf2"}, },
	},
    },
    {
	/* Backup Config */
        offset: 0x0,
	size:   0x00020000, /* 128k */
	type:   FLASH_SECT_BACKUP_CONF,
	s: { 
            type: FLASH_SEC_STRUCT_DEFAULT,
            u: { def: { filename: CONFIG_RG_FFS_MNT_DIR"/backup_rg_conf"}, },
	},
    },
    {
    	/* OpenRG Image */
        offset: 0x0,
	size:   0x00F00000, /* 15MB */
	type:   FLASH_SECT_IMAGE,
    	flags:	FLASH_SECT_WRITE_WARNING | FLASH_SECT_UNLINK_ON_WRITE | 
	    FLASH_SECT_BACK_HEADER,
	s: { 
            type: FLASH_SEC_STRUCT_DEFAULT,
	    u: { def: { filename: CONFIG_RG_FFS_MNT_DIR "/openrg1.img"}, },
	},
    },
    {
    	/* OpenRG Image */
        offset: 0x0,
	size:   0x00F00000, /* 15MB */
	type:   FLASH_SECT_IMAGE,
    	flags:	FLASH_SECT_WRITE_WARNING | FLASH_SECT_UNLINK_ON_WRITE | 
	    FLASH_SECT_BACK_HEADER,
	s: { 
            type: FLASH_SEC_STRUCT_DEFAULT,
	    u: { def: { filename: CONFIG_RG_FFS_MNT_DIR "/openrg2.img"}, },
	},
    },
    {
	/* Persistent Log */
        offset: 0x0,
	size:   0x00040000, /* 256k */
	type:   FLASH_SECT_LOG,
	s: { 
            type: FLASH_SEC_STRUCT_DEFAULT,
	    u: { def: { filename: CONFIG_RG_FFS_MNT_DIR "/persistent_log"}, },
	},
    },
};

u32 flash_section_count = sizeof(flash_layout) / sizeof(flash_layout[0]);

