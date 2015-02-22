/*
 * Here is the original comment from the distribution:

SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain

 * Adapted for use by the IPSEC code by John Ioannidis
 */


#ifndef _IPSEC_SHA1_H_
#define _IPSEC_SHA1_H_

typedef struct
{
	__u32	state[5];
	__u32	count[2];
	__u8	buffer[64];
} SHA1_CTX;

void SHA1Transform(__u32 state[5], __u8 buffer[64]);
void SHA1Init(SHA1_CTX *context);
void SHA1Update(SHA1_CTX *context, unsigned char *data, __u32 len);
void SHA1Final(unsigned char digest[20], SHA1_CTX *context);

 
#endif /* _IPSEC_SHA1_H_ */

/*
 * $Log: ipsec_sha1.h,v $
 * Revision 1.5  2003/09/21 20:24:00  igork
 * merge branch-dev-2421 into dev
 *
 * Revision 1.3.120.1  2003/09/16 13:34:20  ron
 * merge from branch-3_1 to merge-dev-branch-dev-2421 into branch-dev-2421
 *
 * Revision 1.4  2003/09/11 11:37:56  yoavp
 * AUTO MERGE: 1 <- branch-3_2
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 * NOTIFY: automerge
 *
 * Revision 1.3.122.1  2003/09/11 11:34:41  yoavp
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 *
 * Revision 1.3  2001/02/10 11:52:07  aidan
 * freeswan-1.8 first pcbox version
 *
 * Revision 1.5  1999/12/13 13:59:13  rgb
 * Quick fix to argument size to Update bugs.
 *
 * Revision 1.4  1999/12/07 18:16:23  rgb
 * Fixed comments at end of #endif lines.
 *
 * Revision 1.3  1999/04/06 04:54:27  rgb
 * Fix/Add RCSID Id: and Log: bits to make PHMDs happy.  This includes
 * patch shell fixes.
 *
 * Revision 1.2  1998/11/30 13:22:54  rgb
 * Rationalised all the klips kernel file headers.  They are much shorter
 * now and won't conflict under RH5.2.
 *
 * Revision 1.1  1998/06/18 21:27:50  henry
 * move sources from klips/src to klips/net/ipsec, to keep stupid
 * kernel-build scripts happier in the presence of symlinks
 *
 * Revision 1.2  1998/04/23 20:54:05  rgb
 * Fixed md5 and sha1 include file nesting issues, to be cleaned up when
 * verified.
 *
 * Revision 1.1  1998/04/09 03:04:21  henry
 * sources moved up from linux/net/ipsec
 * these two include files modified not to include others except in kernel
 *
 * Revision 1.1.1.1  1998/04/08 05:35:04  henry
 * RGB's ipsec-0.8pre2.tar.gz ipsec-0.8
 *
 * Revision 0.4  1997/01/15 01:28:15  ji
 * New transform
 *
 */
