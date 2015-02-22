/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/openssl/crypto/bio/bio_lcl.h
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

#include <openssl/bio.h>

#if BIO_FLAGS_UPLINK==0
/* Shortcut UPLINK calls on most platforms... */
#define	UP_stdin	stdin
#define	UP_stdout	stdout
#define	UP_stderr	stderr
#define	UP_fprintf	fprintf
#define	UP_fgets	fgets
#define	UP_fread	fread
#define	UP_fwrite	fwrite
#undef	UP_fsetmod
#define	UP_feof		feof
#define	UP_fclose	fclose

#define	UP_fopen	fopen
#define	UP_fseek	fseek
#define	UP_ftell	ftell
#define	UP_fflush	fflush
#define	UP_ferror	ferror
#define	UP_fileno	fileno

#define	UP_open		open
#define	UP_read		read
#define	UP_write	write
#define	UP_lseek	lseek
#define	UP_close	close
#endif
