/****************************************************************************
#	 	spcadecoder: Generic decoder for various input stream yyuv  #
# yuyv yuvy jpeg411 jpeg422 bayer rggb with gamma correct                   #
# and various output palette rgb16 rgb24 rgb32 yuv420p                      #
# various output size with crop feature                                     #
# 		Copyright (C) 2003 2004 2005 Michel Xhaard                  #
# 		mxhaard@magic.fr                                            #
# 		Sonix Decompressor by B.S. (C) 2004                         #
# This program is free software; you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by      #
# the Free Software Foundation; either version 2 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program; if not, write to the Free Software               #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA #
#                                                                           #
****************************************************************************/


#ifndef __KERNEL__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else /* __KERNEL__ */
#include <linux/string.h>
#endif /* __KERNEL__ */


#include "spcadecoder.h"
#include "jpeg_header.h"
#include "spcagamma.h"

#define ISHIFT 11

#define IFIX(a) ((long)((a) * (1 << ISHIFT) + .5))
#define IMULT(a, b) (((a) * (b)) >> ISHIFT)
#define ITOINT(a) ((a) >> ISHIFT)

/* special markers */
#define M_BADHUFF	-1




/* Sonix decompressor struct B.S.(2004) */
typedef struct {
	int is_abs;
	int len;
	int val;
} code_table_t;
static code_table_t table[256];


#define ERR_NO_SOI 1
#define ERR_NOT_8BIT 2
#define ERR_HEIGHT_MISMATCH 3
#define ERR_WIDTH_MISMATCH 4
#define ERR_BAD_WIDTH_OR_HEIGHT 5
#define ERR_TOO_MANY_COMPPS 6
#define ERR_ILLEGAL_HV 7
#define ERR_QUANT_TABLE_SELECTOR 8
#define ERR_NOT_YCBCR_221111 9
#define ERR_UNKNOWN_CID_IN_SCAN 10
#define ERR_NOT_SEQUENTIAL_DCT 11
#define ERR_WRONG_MARKER 12
#define ERR_NO_EOI 13
#define ERR_BAD_TABLES 14
#define ERR_DEPTH_MISMATCH 15
#define ERR_CORRUPTFRAME 16

#define JPEGHEADER_LENGTH 589

const unsigned char JPEGHeader[JPEGHEADER_LENGTH] =
{
 0xff, 0xd8, 0xff, 0xdb, 0x00, 0x84, 0x00, 0x06, 0x04, 0x05, 0x06, 0x05, 0x04, 0x06, 0x06, 0x05,
 0x06, 0x07, 0x07, 0x06, 0x08, 0x0a, 0x10, 0x0a, 0x0a, 0x09, 0x09, 0x0a, 0x14, 0x0e, 0x0f, 0x0c,
 0x10, 0x17, 0x14, 0x18, 0x18, 0x17, 0x14, 0x16, 0x16, 0x1a, 0x1d, 0x25, 0x1f, 0x1a, 0x1b, 0x23,
 0x1c, 0x16, 0x16, 0x20, 0x2c, 0x20, 0x23, 0x26, 0x27, 0x29, 0x2a, 0x29, 0x19, 0x1f, 0x2d, 0x30,
 0x2d, 0x28, 0x30, 0x25, 0x28, 0x29, 0x28, 0x01, 0x07, 0x07, 0x07, 0x0a, 0x08, 0x0a, 0x13, 0x0a,
 0x0a, 0x13, 0x28, 0x1a, 0x16, 0x1a, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0xff, 0xc4, 0x01, 0xa2, 0x00, 0x00, 0x01, 0x05,
 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01,
 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23,
 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17,
 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5,
 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1,
 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04,
 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04,
 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14,
 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16,
 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36,
 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76,
 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2,
 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xc0, 0x00, 0x11,
 0x08, 0x01, 0xe0, 0x02, 0x80, 0x03, 0x01, 0x21, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff,
 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00
 };

int spca50x_outpicture ( struct spca50x_frame *myframe );

static int jpeg_decode411(struct spca50x_frame *myframe, int force_rgb);
static int jpeg_decode422(struct spca50x_frame *myframe, int force_rgb);
static int yuv_decode(struct spca50x_frame *myframe, int force_rgb);
static int bayer_decode(struct spca50x_frame *myframe, int force_rgb);
static int make_jpeg ( struct spca50x_frame *myframe );
static int make_jpeg_conexant (struct spca50x_frame *myframe);


#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))
/****************************************************************/
/**************      Sonix  huffman decoder      ****************/
/****************************************************************/
void init_sonix_decoder(void)
{
	int i;
	int is_abs, val, len;

	for (i = 0; i < 256; i++) {
		is_abs = 0;
		val = 0;
		len = 0;
		if ((i & 0x80) == 0) {
			/* code 0 */
			val = 0;
			len = 1;
		}
		else if ((i & 0xE0) == 0x80) {
			/* code 100 */
			val = +4;
			len = 3;
		}
		else if ((i & 0xE0) == 0xA0) {
			/* code 101 */
			val = -4;
			len = 3;
		}
		else if ((i & 0xF0) == 0xD0) {
			/* code 1101 */
			val = +11;
			len = 4;
		}
		else if ((i & 0xF0) == 0xF0) {
			/* code 1111 */
			val = -11;
			len = 4;
		}
		else if ((i & 0xF8) == 0xC8) {
			/* code 11001 */
			val = +20;
			len = 5;
		}
		else if ((i & 0xFC) == 0xC0) {
			/* code 110000 */
			val = -20;
			len = 6;
		}
		else if ((i & 0xFC) == 0xC4) {
			/* code 110001xx: unknown */
			val = 0;
			len = 8;
		}
		else if ((i & 0xF0) == 0xE0) {
			/* code 1110xxxx */
			is_abs = 1;
			val = (i & 0x0F) << 4;
			len = 8;
		}
		table[i].is_abs = is_abs;
		table[i].val = val;
		table[i].len = len;
	}
}

static void sonix_decompress(int width, int height, unsigned char *inp, unsigned char *outp)
{
	int row, col;
	int val;
	int bitpos;
	unsigned char code;
	unsigned char *addr;

	bitpos = 0;
	for (row = 0; row < height; row++) {

		col = 0;

		/* first two pixels in first two rows are stored as raw 8-bit */
		if (row < 2) {
			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
			bitpos += 8;
			*outp++ = code;

			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
			bitpos += 8;
			*outp++ = code;

			col += 2;
		}

		while (col < width) {
			/* get bitcode from bitstream */
			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));

			/* update bit position */
			bitpos += table[code].len;

			/* calculate pixel value */
			val = table[code].val;
			if (!table[code].is_abs) {
				/* value is relative to top and left pixel */
				if (col < 2) {
					/* left column: relative to top pixel */
					val += outp[-2*width];
				}
				else if (row < 2) {
					/* top row: relative to left pixel */
					val += outp[-2];
				}
				else {
					/* main area: average of left pixel and top pixel */
					val += (outp[-2] + outp[-2*width]) / 2;
					
				}
			}

			/* store pixel */
			*outp++ = CLIP(val);
			col++;
		}
	}
}
static void tv8532_preprocess(struct spca50x_frame *myframe)
{
/* we should received a whole frame with header and EOL marker
in myframe->data and return a GBRG pattern in frame->tmpbuffer
 sequence 2bytes header the Alternate pixels bayer GB 4 bytes
 Alternate pixels bayer RG 4 bytes EOL */
 int width = myframe->hdrwidth;
 int height =myframe->hdrheight;
 int src = 0;
 unsigned char *dst = myframe->tmpbuffer;
 unsigned char *data = myframe->data;
 int i;
 int seq1,seq2;
 
 /* precompute where is the good bayer line */
 if((((data[src+3]+ data[src+width+7]) >>1 )+(data[src+4]>>2)+(data[src+width+6]>>1)) >= 
    (((data[src+2]+data[src+width+6]) >>1)+(data[src+3]>>2)+(data[src+width+5]>>1))){
 	seq1 = 3;
 	seq2 = 4;
 } else {
 	seq1 = 2;
 	seq2 = 5;
 }
 for(i=0; i < height/2; i++){
 	src += seq1;
	memcpy(dst,&myframe->data[src],width);
	src += (width + 3);
	dst += width;
	memcpy(dst,&myframe->data[src],width);
	src += (width + seq2);
	dst += width;	
 }
}
/****************************************************************/
/**************       huffman decoder             ***************/
/****************************************************************/

/*need to be on init jpeg */
static struct comp comp_template[MAXCOMP] = {
	{0x01, 0x22, 0x00},
	{0x02, 0x11, 0x01},
	{0x03, 0x11, 0x01},
	{0x00, 0x00, 0x00}
};
/* deprecated set by webcam now in spca50x */
//static struct scan dscans[MAXCOMP];
//static unsigned char quant[3][64];
//static struct in in;
//int dquant[3][64];
//static struct jpginfo info;
/* table de Huffman global for all */
static struct dec_hufftbl dhuff[4];
#define dec_huffdc (dhuff + 0)
#define dec_huffac (dhuff + 2)
#define M_RST0	0xd0


static int fillbits (struct in *, int, unsigned int);
static int dec_rec2 (struct in *, struct dec_hufftbl *, int *, int, int);

static int
fillbits (struct in *in, int le, unsigned int bi)
{
	int b;
	int m;
	if (in->marker) {
		if (le <= 16)
			in->bits = bi << 16, le += 16;
		return le;
	}
	while (le <= 24) {
		b = *in->p++;
		if(in->omitescape){
			if (b == 0xff && (m = *in->p++) != 0) {
				in->marker = m;
				if (le <= 16)
					bi = bi << 16, le += 16;
				break;
			}
		}
		bi = bi << 8 | b;
		le += 8;
	}
	in->bits = bi;		/* tmp... 2 return values needed */
	return le;
}

#define LEBI_GET(in)	(le = in->left, bi = in->bits)
#define LEBI_PUT(in)	(in->left = le, in->bits = bi)

#define GETBITS(in, n) (					\
  (le < (n) ? le = fillbits(in, le, bi), bi = in->bits : 0),	\
  (le -= (n)),							\
  bi >> le & ((1 << (n)) - 1)					\
)

#define UNGETBITS(in, n) (	\
  le += (n)			\
)

static void dec_initscans(struct dec_data * decode)
{
	struct jpginfo *info = &decode->info;
	struct scan *dscans = decode->dscans;
	int i;
	info->ns = 3; // HARDCODED  here
	info->nm = info->dri + 1; // macroblock count
	info->rm = M_RST0;
	for (i = 0; i < info->ns; i++)
		dscans[i].dc = 0;
}
static int dec_readmarker(struct in *in)

{
	int m;

	in->left = fillbits(in, in->left, in->bits);
	if ((m = in->marker) == 0)
		return 0;
	in->left = 0;
	in->marker = 0;
	return m;
}

static int dec_checkmarker(struct dec_data * decode)
{
	struct jpginfo *info = &decode->info;
	struct scan *dscans = decode->dscans;
	struct in *in = &decode->in;
	int i;

	if (dec_readmarker(in) != info->rm)
		return -1;
	info->nm = info->dri;
	info->rm = (info->rm + 1) & ~0x08;
	for (i = 0; i < info->ns; i++)
		dscans[i].dc = 0;
	return 0;
}
void
jpeg_reset_input_context (struct dec_data *decode, unsigned char *buf,int oescap)
{
	/* set input context */
	struct in *in = &decode->in;
	in->p = buf;
	in->omitescape = oescap;
	in->left = 0;
	in->bits = 0;
	in->marker = 0;
}
static int
dec_rec2 (struct in *in, struct dec_hufftbl *hu, int *runp, int c, int i)
{
	int le, bi;

	le = in->left;
	bi = in->bits;
	if (i) {
		UNGETBITS (in, i & 127);
		*runp = i >> 8 & 15;
		i >>= 16;
	} else {
		for (i = DECBITS;
		     (c = ((c << 1) | GETBITS (in, 1))) >= (hu->maxcode[i]);
		     i++) ;
		if (i >= 16) {
			in->marker = M_BADHUFF;
			return 0;
		}
		i = hu->vals[hu->valptr[i] + c - hu->maxcode[i - 1] * 2];
		*runp = i >> 4;
		i &= 15;
	}
	if (i == 0) {		/* sigh, 0xf0 is 11 bit */
		LEBI_PUT (in);
		return 0;
	}
	/* receive part */
	c = GETBITS (in, i);
	if (c < (1 << (i - 1)))
		c += (-1 << i) + 1;
	LEBI_PUT (in);
	return c;
}

#define DEC_REC(in, hu, r, i)	 (	\
  r = GETBITS(in, DECBITS),		\
  i = hu->llvals[r],			\
  i & 128 ?				\
    (					\
      UNGETBITS(in, i & 127),		\
      r = i >> 8 & 15,			\
      i >> 16				\
    )					\
  :					\
    (					\
      LEBI_PUT(in),			\
      i = dec_rec2(in, hu, &r, r, i),	\
      LEBI_GET(in),			\
      i					\
    )					\
)

inline static void
decode_mcus (struct in *in, int *dct, int n, struct scan *sc, int *maxp)
{
	struct dec_hufftbl *hu;
	int i, r, t;
	int le, bi;

	memset (dct, 0, n * 64 * sizeof (*dct));
	le = in->left;
	bi = in->bits;

	while (n-- > 0) {
		hu = sc->hudc.dhuff;
		*dct++ = (sc->dc += DEC_REC (in, hu, r, t));

		hu = sc->huac.dhuff;
		i = 63;
		while (i > 0) {
			t = DEC_REC (in, hu, r, t);
			if (t == 0 && r == 0) {
				dct += i;
				break;
			}
			dct += r;
			*dct++ = t;
			i -= r + 1;
		}
		*maxp++ = 64 - i;
		if (n == sc->next)
			sc++;
	}
	LEBI_PUT (in);
}

static void
dec_makehuff (struct dec_hufftbl *hu, int *hufflen, unsigned char *huffvals)
{
	int code, k, i, j, d, x, c, v;

	for (i = 0; i < (1 << DECBITS); i++)
		hu->llvals[i] = 0;

/*
 * llvals layout:
 *
 * value v already known, run r, backup u bits:
 *  vvvvvvvvvvvvvvvv 0000 rrrr 1 uuuuuuu
 * value unknown, size b bits, run r, backup u bits:
 *  000000000000bbbb 0000 rrrr 0 uuuuuuu
 * value and size unknown:
 *  0000000000000000 0000 0000 0 0000000
 */
	code = 0;
	k = 0;
	for (i = 0; i < 16; i++, code <<= 1) {	/* sizes */
		hu->valptr[i] = k;
		for (j = 0; j < hufflen[i]; j++) {
			hu->vals[k] = *huffvals++;
			if (i < DECBITS) {
				c = code << (DECBITS - 1 - i);
				v = hu->vals[k] & 0x0f;	/* size */
				for (d = 1 << (DECBITS - 1 - i); --d >= 0;) {
					if (v + i < DECBITS) {	/* both fit in table */
						x = d >> (DECBITS - 1 - v - i);
						if (v && x < (1 << (v - 1)))
							x += (-1 << v) + 1;
						x = x << 16 | (hu->
							       vals[k] & 0xf0)
							<< 4 | (DECBITS -
								(i + 1 +
								 v)) | 128;
					} else
						x = v << 16 | (hu->
							       vals[k] & 0xf0)
							<< 4 | (DECBITS -
								(i + 1));
					hu->llvals[c | d] = x;
				}
			}
			code++;
			k++;
		}
		hu->maxcode[i] = code;
	}
	hu->maxcode[16] = 0x20000;	/* always terminate decode */
}

/****************************************************************/
/**************             idct                  ***************/
/****************************************************************/


#define S22 ((long)IFIX(2 * 0.382683432))
#define C22 ((long)IFIX(2 * 0.923879532))
#define IC4 ((long)IFIX(1 / 0.707106781))

static unsigned char zig2[64] = {
	0, 2, 3, 9, 10, 20, 21, 35,
	14, 16, 25, 31, 39, 46, 50, 57,
	5, 7, 12, 18, 23, 33, 37, 48,
	27, 29, 41, 44, 52, 55, 59, 62,
	15, 26, 30, 40, 45, 51, 56, 58,
	1, 4, 8, 11, 19, 22, 34, 36,
	28, 42, 43, 53, 54, 60, 61, 63,
	6, 13, 17, 24, 32, 38, 47, 49
};

inline static void
idct (int *in, int *out, int *quant, long off, int max)
{
	long t0, t1, t2, t3, t4, t5, t6, t7;	// t ;
	long tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	long tmp[64], *tmpp;
	int i, j, te;
	unsigned char *zig2p;

	t0 = off;
	if (max == 1) {
		t0 += in[0] * quant[0];
		for (i = 0; i < 64; i++)
			out[i] = ITOINT (t0);
		return;
	}
	zig2p = zig2;
	tmpp = tmp;
	for (i = 0; i < 8; i++) {
		j = *zig2p++;
		t0 += in[j] * (long) quant[j];
		j = *zig2p++;
		t5 = in[j] * (long) quant[j];
		j = *zig2p++;
		t2 = in[j] * (long) quant[j];
		j = *zig2p++;
		t7 = in[j] * (long) quant[j];
		j = *zig2p++;
		t1 = in[j] * (long) quant[j];
		j = *zig2p++;
		t4 = in[j] * (long) quant[j];
		j = *zig2p++;
		t3 = in[j] * (long) quant[j];
		j = *zig2p++;
		t6 = in[j] * (long) quant[j];


		if ((t1 | t2 | t3 | t4 | t5 | t6 | t7) == 0) {

			tmpp[0 * 8] = t0;
			tmpp[1 * 8] = t0;
			tmpp[2 * 8] = t0;
			tmpp[3 * 8] = t0;
			tmpp[4 * 8] = t0;
			tmpp[5 * 8] = t0;
			tmpp[6 * 8] = t0;
			tmpp[7 * 8] = t0;

			tmpp++;
			t0 = 0;
			continue;
		}
		//IDCT;
		tmp0 = t0 + t1;
		t1 = t0 - t1;
		tmp2 = t2 - t3;
		t3 = t2 + t3;
		tmp2 = IMULT (tmp2, IC4) - t3;
		tmp3 = tmp0 + t3;
		t3 = tmp0 - t3;
		tmp1 = t1 + tmp2;
		tmp2 = t1 - tmp2;
		tmp4 = t4 - t7;
		t7 = t4 + t7;
		tmp5 = t5 + t6;
		t6 = t5 - t6;
		tmp6 = tmp5 - t7;
		t7 = tmp5 + t7;
		tmp5 = IMULT (tmp6, IC4);
		tmp6 = IMULT ((tmp4 + t6), S22);
		tmp4 = IMULT (tmp4, (C22 - S22)) + tmp6;
		t6 = IMULT (t6, (C22 + S22)) - tmp6;
		t6 = t6 - t7;
		t5 = tmp5 - t6;
		t4 = tmp4 - t5;

		tmpp[0 * 8] = tmp3 + t7;	//t0;
		tmpp[1 * 8] = tmp1 + t6;	//t1;
		tmpp[2 * 8] = tmp2 + t5;	//t2;
		tmpp[3 * 8] = t3 + t4;	//t3;
		tmpp[4 * 8] = t3 - t4;	//t4;
		tmpp[5 * 8] = tmp2 - t5;	//t5;
		tmpp[6 * 8] = tmp1 - t6;	//t6;
		tmpp[7 * 8] = tmp3 - t7;	//t7;
		tmpp++;
		t0 = 0;
	}
	for (i = 0, j = 0; i < 8; i++) {
		t0 = tmp[j + 0];
		t1 = tmp[j + 1];
		t2 = tmp[j + 2];
		t3 = tmp[j + 3];
		t4 = tmp[j + 4];
		t5 = tmp[j + 5];
		t6 = tmp[j + 6];
		t7 = tmp[j + 7];
		if ((t1 | t2 | t3 | t4 | t5 | t6 | t7) == 0) {
			te = ITOINT (t0);
			out[j + 0] = te;
			out[j + 1] = te;
			out[j + 2] = te;
			out[j + 3] = te;
			out[j + 4] = te;
			out[j + 5] = te;
			out[j + 6] = te;
			out[j + 7] = te;
			j += 8;
			continue;
		}
		//IDCT;
		tmp0 = t0 + t1;
		t1 = t0 - t1;
		tmp2 = t2 - t3;
		t3 = t2 + t3;
		tmp2 = IMULT (tmp2, IC4) - t3;
		tmp3 = tmp0 + t3;
		t3 = tmp0 - t3;
		tmp1 = t1 + tmp2;
		tmp2 = t1 - tmp2;
		tmp4 = t4 - t7;
		t7 = t4 + t7;
		tmp5 = t5 + t6;
		t6 = t5 - t6;
		tmp6 = tmp5 - t7;
		t7 = tmp5 + t7;
		tmp5 = IMULT (tmp6, IC4);
		tmp6 = IMULT ((tmp4 + t6), S22);
		tmp4 = IMULT (tmp4, (C22 - S22)) + tmp6;
		t6 = IMULT (t6, (C22 + S22)) - tmp6;
		t6 = t6 - t7;
		t5 = tmp5 - t6;
		t4 = tmp4 - t5;

		out[j + 0] = ITOINT (tmp3 + t7);
		out[j + 1] = ITOINT (tmp1 + t6);
		out[j + 2] = ITOINT (tmp2 + t5);
		out[j + 3] = ITOINT (t3 + t4);
		out[j + 4] = ITOINT (t3 - t4);
		out[j + 5] = ITOINT (tmp2 - t5);
		out[j + 6] = ITOINT (tmp1 - t6);
		out[j + 7] = ITOINT (tmp3 - t7);
		j += 8;
	}

}

static unsigned char zig[64] = {
	0, 1, 5, 6, 14, 15, 27, 28,
	2, 4, 7, 13, 16, 26, 29, 42,
	3, 8, 12, 17, 25, 30, 41, 43,
	9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63
};

static int aaidct[8] = {
	IFIX (0.3535533906), IFIX (0.4903926402),
	IFIX (0.4619397663), IFIX (0.4157348062),
	IFIX (0.3535533906), IFIX (0.2777851165),
	IFIX (0.1913417162), IFIX (0.0975451610)
};


inline static void
idctqtab (unsigned char *qin, int *qout)
{
	int i, j;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			qout[zig[i * 8 + j]] = qin[zig[i * 8 + j]] *
				IMULT (aaidct[i], aaidct[j]);
}

inline static void
scaleidctqtab (int *q, int sc)
{
	int i;

	for (i = 0; i < 64; i++)
		q[i] = IMULT (q[i], sc);
}

/* Reduce to the necessary minimum. FIXME */
void
init_jpeg_decoder (struct usb_spca50x *spca50x, unsigned int qIndex)
{
	unsigned int i, j, k, l;
	//unsigned int qIndex = 1; // 2;
	int tc, th, tt, tac, tdc;
	unsigned char *ptr;
	memcpy(spca50x->maindecode.comps,comp_template, MAXCOMP*sizeof(struct comp));	
	/* set up the huffman table */
	ptr = (unsigned char *) GsmartJPEGHuffmanTable;
	l = GSMART_JPG_HUFFMAN_TABLE_LENGTH;
	while (l > 0) {
		int hufflen[16];
		unsigned char huffvals[256];

		tc = *ptr++;
		th = tc & 15;
		tc >>= 4;
		tt = tc * 2 + th;
		if (tc > 1 || th > 1) {
			//printf("died whilst setting up huffman table.\n");
			//abort();
		}
		for (i = 0; i < 16; i++)
			hufflen[i] = *ptr++;
		l -= 1 + 16;
		k = 0;
		for (i = 0; i < 16; i++) {
			for (j = 0; j < (unsigned int) hufflen[i]; j++)
				huffvals[k++] = *ptr++;
			l -= hufflen[i];
		}
		dec_makehuff (dhuff + tt, hufflen, huffvals);
	}

	/* set up the scan table */
	ptr = (unsigned char *) GsmartJPEGScanTable;
	for (i = 0; i < 3; i++) {
		spca50x->maindecode.dscans[i].cid = *ptr++;
		tdc = *ptr++;
		tac = tdc & 15;
		tdc >>= 4;
		if (tdc > 1 || tac > 1) {
			//printf("died whilst setting up scan table.\n");
			//abort();
		}
		/* for each component */
		for (j = 0; j < 3; j++)
			if (spca50x->maindecode.comps[j].cid == spca50x->maindecode.dscans[i].cid)
				break;

		spca50x->maindecode.dscans[i].hv = spca50x->maindecode.comps[j].hv;
		spca50x->maindecode.dscans[i].tq = spca50x->maindecode.comps[j].tq;
		spca50x->maindecode.dscans[i].hudc.dhuff = dec_huffdc + tdc;
		spca50x->maindecode.dscans[i].huac.dhuff = dec_huffac + tac;
	}

	if (spca50x->maindecode.dscans[0].cid != 1 ||
		spca50x->maindecode.dscans[1].cid != 2 ||
		spca50x->maindecode.dscans[2].cid != 3) {
		//printf("invalid cid found.\n");
		//abort();
	}

	if (spca50x->maindecode.dscans[0].hv != 0x22 ||
	spca50x->maindecode.dscans[1].hv != 0x11 ||
	spca50x->maindecode.dscans[2].hv != 0x11) {
		//printf("invalid hv found.\n");
		//abort();
	}
	spca50x->maindecode.dscans[0].next = 6 - 4;
	spca50x->maindecode.dscans[1].next = 6 - 4 - 1;
	spca50x->maindecode.dscans[2].next = 6 - 4 - 1 - 1;	/* 411 encoding */
	
	/* set up a quantization table */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 64; j++) {
			spca50x->maindecode.quant[i][j] = GsmartQTable[qIndex * 2 + i][j];
		}
	}
	idctqtab (spca50x->maindecode.quant[spca50x->maindecode.dscans[0].tq], spca50x->maindecode.dquant[0]);
	idctqtab (spca50x->maindecode.quant[spca50x->maindecode.dscans[1].tq], spca50x->maindecode.dquant[1]);
	idctqtab (spca50x->maindecode.quant[spca50x->maindecode.dscans[2].tq], spca50x->maindecode.dquant[2]);
	/* rescale qtab */
	scaleidctqtab (spca50x->maindecode.dquant[0], IFIX (0.7));
	scaleidctqtab (spca50x->maindecode.dquant[1], IFIX (0.7));
	scaleidctqtab (spca50x->maindecode.dquant[2], IFIX (0.7));	

}



static int bgr = 0;

 



/* Gamma correction setting */
/*	Gtable[0][n] -> 2.2
*	Gtable[1][n] -> 1.7
*	Gtable[2][n] -> 1.45
*	Gtable[3][n] -> 1
*	Gtable[4][n] -> 0.6896
*	Gtable[5][n] -> 0.5882
*	Gtable[6][n] -> 0.4545
*	gCor coeff 0..6
*/

int spca50x_outpicture ( struct spca50x_frame *myframe )
{	/* general idea keep a frame in the temporary buffer from the tasklet*/
	/* decode with native format at input and asked format at output */
	/* myframe->cameratype is the native input format */
	/* myframe->format is the asked format */
	
	struct pictparam *gCorrect = &myframe->pictsetting;
	unsigned char *red = myframe->decoder->Red;
	unsigned char *green = myframe->decoder->Green;
	unsigned char * blue = myframe->decoder->Blue;
	int width = 0;
	int height = 0;
	int done = 0;
	int i;
	if (gCorrect->change) {
		if ( gCorrect->change == 0x01) {
		/* Gamma setting change compute all case */
			memcpy (red,&GTable[gCorrect->gamma],256);
			memcpy (green,&GTable[gCorrect->gamma],256);
			memcpy (blue,&GTable[gCorrect->gamma],256);
			for (i =0; i < 256 ; i++){
				red[i] = CLIP(((red[i] + gCorrect->OffRed) * gCorrect->GRed) >> 8);
				green[i] = CLIP(((green[i] + gCorrect->OffGreen) * gCorrect->GGreen) >> 8);
				blue[i] = CLIP(((blue[i] + gCorrect->OffBlue) * gCorrect->GBlue) >> 8);
			
			}
			bgr = gCorrect->force_rgb;
			gCorrect->change = 0x00;
		}
		if ( gCorrect->change == 0x02) {
		/* Red setting change compute Red Value */
			memcpy (red,&GTable[gCorrect->gamma],256);
			for (i =0; i < 256 ; i++){
				red[i] = CLIP(((red[i] + gCorrect->OffRed) * gCorrect->GRed) >> 8);
			}
			gCorrect->change &= ~0x02;
		}
		if ( gCorrect->change == 0x04) {
		/* Green setting change compute Green Value */
			memcpy (green,&GTable[gCorrect->gamma],256);
			for (i =0; i < 256 ; i++){	
				green[i] = CLIP(((green[i] + gCorrect->OffGreen) * gCorrect->GGreen) >> 8);	
			}
			gCorrect->change &= ~0x04;
		}
		if ( gCorrect->change == 0x08) {
		/* Blue setting change compute Blue Value */
			memcpy (blue,&GTable[gCorrect->gamma],256);
			for (i =0; i < 256 ; i++){
				blue[i] = CLIP(((blue[i] + gCorrect->OffBlue) * gCorrect->GBlue) >> 8);
			}
			gCorrect->change &= ~0x08; 
		}
		if ( gCorrect->change == 0x10) {
		/* force_rgb setting change   */
			bgr = gCorrect->force_rgb;
			gCorrect->change &= ~0x10; 
		}
	}
	switch(myframe->cameratype){
		case JPGC:
			
			height = (myframe->data[11] << 8) | myframe->data[12];
			width = (myframe->data[13] << 8) | myframe->data[14];
			if (myframe->hdrheight != height || myframe->hdrwidth != width){	
				done = ERR_CORRUPTFRAME;
			} else {
			//set info.dri struct should be kmalloc with the
			// instance camera
			myframe->decoder->info.dri = myframe->data[5];
			if (myframe->format == VIDEO_PALETTE_JPEG){
				memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength);
			 	done = make_jpeg_conexant ( myframe );
			} else {
				memcpy(myframe->tmpbuffer,myframe->data + 39,myframe->scanlength - 39);
			 	done = jpeg_decode422( myframe, bgr);
			}
			}
		break;
		case JPGH:
			width = (myframe->data[10] << 8) | myframe->data[11];
			height = (myframe->data[12] << 8) | myframe->data[13];
			/* some camera did not respond with the good height ie:Labtec Pro 240 -> 232 */
			if (myframe->hdrwidth != width){	
				done = ERR_CORRUPTFRAME;
			} else {
			// reset info.dri
			myframe->decoder->info.dri = 0;
			memcpy(myframe->tmpbuffer,myframe->data+16,myframe->scanlength - 16);
			if (myframe->format == VIDEO_PALETTE_JPEG){
				done = make_jpeg ( myframe );
			} else {
				done = jpeg_decode422( myframe, bgr);
			}
			}
		break;
		case JPGM:
		case JPGS:
		// reset info.dri
			myframe->decoder->info.dri = 0;
			memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength );
			if (myframe->format == VIDEO_PALETTE_JPEG){
				done = make_jpeg ( myframe );
			} else {
				done = jpeg_decode422( myframe, bgr);
			}
		break;
		case JPEG: 
			memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength);
			if (myframe->format == VIDEO_PALETTE_JPEG){
			 done = make_jpeg ( myframe );
			} else {
			 done = jpeg_decode411( myframe, bgr);
			}
		break;
		case YUVY: 
		case YUYV: 
		case YYUV: 
			memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength);
			done = yuv_decode( myframe, bgr);
		break;
		case GBGR:
			/* translate the tv8532 stream into GBRG stream */
			tv8532_preprocess(myframe);
			done = bayer_decode( myframe, bgr);
		break;
		case GBRG:
			memcpy(myframe->tmpbuffer,myframe->data,myframe->scanlength);
			done = bayer_decode( myframe, bgr);
		break;
		case SN9C:
			sonix_decompress(myframe->hdrwidth,myframe->hdrheight,myframe->data,myframe->tmpbuffer); 
			done = bayer_decode( myframe, bgr);
		break;
	default : done = -1;
	break;
	}
return done;	
}

static int 
yuv_decode(struct spca50x_frame *myframe, int force_rgb )
{
	
	int r_offset, g_offset, b_offset;
	int my , mx; /* scan input surface */
	unsigned char *pic1; /* output surface */
	__u16 *pix1,*pix2; /* same for 16 bits output */
	
	unsigned char *U, *V; /* chroma output pointer */
	int inuv, inv, pocx; /* offset chroma input */
	int iny,iny1; /* offset luma input */
	int nextinline, nextoutline;
	int u1,v1,rg;
	unsigned char y,y1;
	char u,v;
	unsigned char *pic = myframe->data; /* output surface */
	unsigned char *buf = myframe->tmpbuffer; /* input surface */
	int width = myframe->hdrwidth; 
	int height = myframe->hdrheight;
	int softwidth = myframe->width;
	int softheight = myframe->height;
	//int method = myframe->method;
	int format = myframe->format;
	int cropx1 = myframe->cropx1;
	int cropx2 = myframe->cropx2;
	int cropy1 = myframe->cropy1;
	int cropy2 = myframe->cropy2;
	unsigned char *red = myframe->decoder->Red;
	unsigned char *green = myframe->decoder->Green;
	unsigned char * blue = myframe->decoder->Blue;
	int bpp;
	int framesize, frameUsize;
	
	framesize = softwidth * softheight;
	frameUsize = framesize >> 2;
	/* rgb or bgr like U or V that's the question */
	if (force_rgb) {
		U = pic + framesize;
		V = U + frameUsize;
		r_offset = 2;
		g_offset = 1;
		b_offset = 0;
	} else {
		V = pic + framesize;
		U = V + frameUsize;
		r_offset = 0;
		g_offset = 1;
		b_offset = 2;
	}
	switch (myframe->cameratype) {
		case YUVY: {
			iny = 0;	   /********* iny **********/
			inuv = width;	   /*** inuv **** inv ******/
			nextinline = 3 * width;
			inv = ( nextinline >> 1);
			iny1 = width << 1; /********* iny1 *********/
			}
			break;
		case YUYV: {
			iny = 0;	   /********* iny **********/
			inuv = width;	   /*** inuv **** iny1 *****/
			nextinline = 3 * width;
			iny1 = ( nextinline >> 1);
			inv = iny1 + width ;/*** iny1 **** inv ******/
			}
			break;
		case YYUV: {
			iny = 0;	   /********* iny **********/
			iny1 = width;	   /********* iny1 *********/
			inuv = width << 1; /*** inuv **** inv ******/
			inv = inuv +(width >>1);
			nextinline = 3 * width;
			}
			break;
	default:	{
			iny = 0 ;	   /* make compiler happy */
			iny1 = 0;
			inuv = 0;
			inv = 0 ;
			nextinline = 0;
			}
		 break;
	}
	
	/* Decode to the correct format. */
	switch (format) {
		case VIDEO_PALETTE_RGB565:
			{	bpp = 2;
			/* initialize */
				
				pix1 = (__u16*) pic;
				pix2 = pix1 + softwidth;
				
				
				for ( my =0; my < height; my += 2){
					for ( mx = 0, pocx = 0; mx < width ; mx += 2, pocx++){
					/* test if we need to decode */
					  if ((my >= cropy1)
						    && (my < height - cropy2)
						    && (mx >= cropx1)
						    && (mx < width - cropx2)) {
						    /* yes decode */
						    if ( force_rgb ){
						    	u = buf [inuv + pocx] ;
						    	v = buf [inv + pocx] ;
						    } else {
						    	v = buf [inuv + pocx] ;
						    	u = buf [inv + pocx] ;
						    }
						    v1 = ((v << 10) + (v << 9)) >> 10;
						    rg = ((u << 8) + (u << 7) + (v << 9) + (v << 4)) >> 10;
						    u1 = ((u << 11) + (u << 4)) >> 10;
						    
						   
						    /* top pixel Right */
						    y1 = 128 +buf [iny + mx];		
							*pix1++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
								  ((green[CLIP((y1 - rg))] & 0xFC) << 3) |
								  ((blue[CLIP((y1 + u1))] & 0xF8) << 8)) ;		
						    /* top pixel Left */
						    y1 = 128 +buf [iny + mx +1];
							*pix1++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
								  ((green[CLIP((y1 - rg))] & 0xFC) << 3) |
								  ((blue[CLIP((y1 + u1))] & 0xF8) << 8)) ;		
						    /* bottom pixel Right */
						    y1 = 128 + buf [iny1 + mx];
							*pix2++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
								  ((green[CLIP((y1 - rg))] & 0xFC) << 3) |
								  ((blue[CLIP((y1 + u1))] & 0xF8) << 8)) ;		
						    /* bottom pixel Left */
						    y1 = 128 + buf [iny1 + mx + 1];
							*pix2++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
								  ((green[CLIP((y1 - rg))] & 0xFC) << 3) |
								  ((blue[CLIP((y1 + u1))] & 0xF8) << 8)) ;		
						    
						    
						    
						    
					  } // end test decode
					} // end mx loop
					iny += nextinline;
					inuv += nextinline ;
					inv += nextinline ;
					iny1 += nextinline;
					if (my >= cropy1){
						/* are we in a decode surface move the output pointer */
						pix1 += softwidth ;
						pix2 += softwidth ;
					}
					
				} // end my loop
			
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_RGB32:
		case VIDEO_PALETTE_RGB24:
			{	bpp = (format == VIDEO_PALETTE_RGB32) ? 4 : 3;
				/* initialize */
				nextoutline  = bpp * softwidth;
				pic1 = pic + nextoutline;
				
				
				for ( my =0; my < height; my += 2){
					for ( mx = 0, pocx = 0; mx < width ; mx += 2, pocx++){
					/* test if we need to decode */
					  if ((my >= cropy1)
						    && (my < height - cropy2)
						    && (mx >= cropx1)
						    && (mx < width - cropx2)) {
						    /* yes decode */
						    v = buf [inuv + pocx] ;
						    u = buf [inv + pocx] ;
						    
						    v1 = ((v << 10) + (v << 9)) >> 10;
						    rg = ((u << 8) + (u << 7) + (v << 9) + (v << 4)) >> 10;
						    u1 = ((u << 11) + (u << 4)) >> 10;
						    
						    y = 128 +buf [iny + mx];
						    /* top pixel Right */
									
									pic[r_offset] = red[CLIP ((y + v1))];
									pic[g_offset] = green[CLIP ((y - rg))];
									pic[b_offset] = blue[CLIP ((y + u1))];
									pic += bpp;
						    /* top pixel Left */
						    y = 128 +buf [iny + mx +1];
									pic[r_offset] = red[CLIP ((y + v1))];
									pic[g_offset] = green[CLIP ((y - rg))];
									pic[b_offset] = blue[CLIP ((y + u1))];
									pic += bpp;
						    /* bottom pixel Right */
						    y1 = 128 + buf [iny1 + mx];
									pic1[r_offset] = red[CLIP ((y1 + v1))];
									pic1[g_offset] = green[CLIP ((y1 - rg))];
									pic1[b_offset] = blue[CLIP ((y1 + u1))];
									pic1 += bpp;
						    /* bottom pixel Left */
						    y1 = 128 + buf [iny1 + mx + 1];
									pic1[r_offset] = red[CLIP ((y1 + v1))];
									pic1[g_offset] = green[CLIP ((y1 - rg))];
									pic1[b_offset] = blue[CLIP ((y1 + u1))];
									pic1 += bpp;
						    
						    
						    
						    
					  } // end test decode
					} // end mx loop
					iny += nextinline;
					inuv += nextinline ;
					inv += nextinline ;
					iny1 += nextinline;
					if (my >= cropy1){
						/* are we in a decode surface move the output pointer */
						pic += nextoutline;
						pic1 += nextoutline;
					}
					
				} // end my loop
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_YUV420P:
			{
				/* initialize */
				pic1 = pic + softwidth;
				
				for ( my =0; my < height; my += 2){
					for ( mx = 0, pocx=0; mx < width ; mx +=2, pocx++){
					/* test if we need to decode */
					  if ((my >= cropy1)
						    && (my < height - cropy2)
						    && (mx >= cropx1)
						    && (mx < width - cropx2)) {
						    /* yes decode */
						    *V++ = 128 + buf [inuv + pocx];
						    *U++ = 128 + buf [inv + pocx] ;
						    *pic++ = 128 +buf [iny + mx];
						    *pic++ = 128 +buf [iny + mx+1];
						    *pic1++ = 128 + buf [iny1 + mx];
						    *pic1++ = 128 + buf [iny1 + mx +1];
						    
					  } // end test decode
					} // end mx loop
					iny += nextinline;
					inuv += nextinline;
					inv += nextinline;
					iny1 += nextinline;
				
					if (my >= cropy1){
						/* are we in a decode surface move the output pointer */
						pic += softwidth;
						pic1 += softwidth;
					}
					
				} // end my loop
				
				
			}
			myframe->scanlength = (long)(softwidth*softheight*3)>>1;
			break;
		default:
			break;
	}// end case
	return 0;
}
/*
 *    linux/drivers/video/fbcon-jpegdec.c - a tiny jpeg decoder.
 *      
 *      (w) August 2001 by Michael Schroeder, <mls@suse.de>
 *
 *    I severly gutted this beast and hardcoded it to the palette and subset
 *    of jpeg needed for the spca50x driver. Also converted it from K&R style
 *    C to a more modern form ;). Michael can't be blamed for what is left.
 *    All nice features are his, all bugs are mine. - till
 *
 *    Change color space converter for YUVP and RGB -  
 *    Rework the IDCT implementation for best speed, cut test in the loop but instead
 *	more copy and paste code :)
 *    For more details about idct look at :
 *    http://rnvs.informatik.tu-chemnitz.de/~jan/MPEG/HTML/IDCT.html 
 *    12/12/2003 mxhaard@magic.fr
 *	add make jpeg from header (mxhaard 20/09/2004)
 *	add jpeg_decode for 422 stream (mxhaard 01/10/2004)       
 */
static int
jpeg_decode411 (struct spca50x_frame *myframe, int force_rgb)
{
	int mcusx, mcusy, mx, my;	
	int *dcts = myframe->dcts;	
	int *out =myframe->out;	
	int *max=myframe->max;
//	int i;
	int bpp;
	int framesize, frameUsize;
	int k, j;
	int nextline, nextuv, nextblk, nextnewline;
	unsigned char *pic0, *pic1, *outv, *outu;
	__u16 *pix1,*pix2;
	int picy, picx, pocx, pocy;
	unsigned char *U, *V;
	int *outy, *inv, *inu;
	int outy1, outy2;
	int v, u, y1, v1, u1, u2;
	int r_offset, g_offset, b_offset;
	
	unsigned char *pic = myframe->data; /* output surface */
	unsigned char *buf = myframe->tmpbuffer; /* input surface */
	int width = myframe->hdrwidth; 
	int height = myframe->hdrheight;
	int softwidth = myframe->width;
	int softheight = myframe->height;
	//int method = myframe->method;
	int format = myframe->format;
	int cropx1 = myframe->cropx1;
	int cropx2 = myframe->cropx2;
	int cropy1 = myframe->cropy1;
	int cropy2 = myframe->cropy2;
	unsigned char *red = myframe->decoder->Red;
	unsigned char *green = myframe->decoder->Green;
	unsigned char * blue = myframe->decoder->Blue;
	struct dec_data *decode= myframe->decoder;
	
	if ((height & 15) || (width & 15))
		return 1;
	if (width < softwidth || height < softheight)
		return 1;
	
	mcusx = width >> 4;
	mcusy = height >> 4;
	framesize = softwidth * softheight;
	frameUsize = framesize >> 2;
	jpeg_reset_input_context (decode,buf,0);

	/* for each component. Reset dc values. */
	//for (i = 0; i < 3; i++)
		//dscans[i].dc = 0;
	dec_initscans(decode);
	/* rgb or bgr like U or V that's the question */
	if (force_rgb) {
		U = pic + framesize;
		V = U + frameUsize;
		r_offset = 2;
		g_offset = 1;
		b_offset = 0;
	} else {
		V = pic + framesize;
		U = V + frameUsize;
		r_offset = 0;
		g_offset = 1;
		b_offset = 2;
	}

	/* Decode to the correct format. */
	switch (format) {
		case VIDEO_PALETTE_RGB565:
			{	bpp = 2;
				nextline = ((softwidth << 1) - 16);// *bpp;
				nextblk = bpp * (softwidth << 4);
				nextnewline = softwidth ; // *bpp;
				for (my = 0, picy = 0; my < mcusy; my++) {
					for (mx = 0, picx = 0; mx < mcusx; mx++) {

						decode_mcus (&decode->in, dcts, 6,
							     decode->dscans, max);
						if ((my >= cropy1)
						    && (my < mcusy - cropy2)
						    && (mx >= cropx1)
						    && (mx < mcusx - cropx2)) {
							idct (dcts, out,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[0]);
							idct (dcts + 64,
							      out + 64,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[1]);
							idct (dcts + 128,
							      out + 128,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[2]);
							idct (dcts + 192,
							      out + 192,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[3]);
							idct (dcts + 256,
							      out + 256,
							      decode->dquant[1],
							      IFIX (0.5),
							      max[4]);
							idct (dcts + 320,
							      out + 320,
							      decode->dquant[2],
							      IFIX (0.5),
							      max[5]);
							pix1 = (__u16 *)(pic + picx + picy);
							pix2 = pix1 + nextnewline;
							outy = out;
							outy1 = 0;
							outy2 = 8;
							inv = out + 64 * 4;
							inu = out + 64 * 5;
							for (j = 0; j < 8; j++) {
								for (k = 0;
								     k < 8;
								     k++) {
									if (k ==
									    4) {
										outy1 += 56;
										outy2 += 56;
									}
									/* outup 4 pixels */
									/* get the UV colors need to change UV order for force rgb? */
									if ( force_rgb){
										u = *inv++;
										v = *inu++;
									} else {
										v = *inv++;
										u = *inu++;
									}
									/* MX color space why not? */
									v1 = ((v << 10) + (v << 9)) >> 10;
									u1 = ((u << 8) + (u << 7) + (v << 9) + (v << 4)) >> 10;
									u2 = ((u << 11) + (u << 4)) >> 10;
									/* top pixel Right */
									y1 = outy[outy1++];
									*pix1++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
									 	  ((green[CLIP((y1 - u1))] & 0xFC) << 3) |
									  	  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;
									/* top pixel Left */
									y1 = outy[outy1++];	  
									*pix1++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 | 
										  ((green[CLIP((y1 - u1))] & 0xFC) << 3) | 
										  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;
									
									/* bottom pixel Right */
									y1 = outy[outy2++];
									*pix2++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
									          ((green[CLIP((y1 - u1))] & 0xFC) << 3) | 
										  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;
									/* bottom pixel Left */
									y1 = outy[outy2++];	  
									*pix2++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3| 
									          ((green[CLIP((y1 - u1))] & 0xFC) << 3) | 
										  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;	

								}
								if (j == 3) {
									outy = out + 128;
								} else {
									outy += 16;
								}
								outy1 = 0;
								outy2 = 8;
								pix1 += nextline;
								pix2 += nextline;

							}
							picx += 16 * bpp;
						} 
					}
					if (my >= cropy1)
						picy += nextblk;

				}
			
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_RGB32:
		case VIDEO_PALETTE_RGB24:
			{	bpp = (format == VIDEO_PALETTE_RGB32) ? 4 : 3;
				nextline = bpp * ((softwidth << 1) - 16);
				nextblk = bpp * (softwidth << 4);
				nextnewline = bpp * softwidth;
				for (my = 0, picy = 0; my < mcusy; my++) {
					for (mx = 0, picx = 0; mx < mcusx; mx++) {

						decode_mcus (&decode->in, dcts, 6,
							     decode->dscans, max);
						if ((my >= cropy1)
						    && (my < mcusy - cropy2)
						    && (mx >= cropx1)
						    && (mx < mcusx - cropx2)) {
							idct (dcts, out,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[0]);
							idct (dcts + 64,
							      out + 64,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[1]);
							idct (dcts + 128,
							      out + 128,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[2]);
							idct (dcts + 192,
							      out + 192,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[3]);
							idct (dcts + 256,
							      out + 256,
							      decode->dquant[1],
							      IFIX (0.5),
							      max[4]);
							idct (dcts + 320,
							      out + 320,
							      decode->dquant[2],
							      IFIX (0.5),
							      max[5]);
							pic0 = pic + picx +
								picy;
							pic1 = pic0 + nextnewline;
							outy = out;
							outy1 = 0;
							outy2 = 8;
							inv = out + 64 * 4;
							inu = out + 64 * 5;
							for (j = 0; j < 8; j++) {
								for (k = 0;
								     k < 8;
								     k++) {
									if (k ==
									    4) {
										outy1 += 56;
										outy2 += 56;
									}
									/* outup 4 pixels */
									/* get the UV colors need to change UV order for force rgb? */
									v = *inv++;
									u = *inu++;
									/* MX color space why not? */
									v1 = ((v << 10) + (v << 9)) >> 10;
									u1 = ((u << 8) + (u << 7) + (v << 9) + (v << 4)) >> 10;
									u2 = ((u << 11) + (u << 4)) >> 10;
									/* top pixel Right */
									y1 = outy[outy1++];
									pic0[r_offset] = red[CLIP ((y1 + v1))];
									pic0[g_offset] = green[CLIP ((y1 - u1))];
									pic0[b_offset] = blue[CLIP ((y1 + u2))];
									pic0 += bpp;
									/* top pixel Left */
									y1 = outy[outy1++];
									pic0[r_offset] = red[CLIP ((y1 + v1))];
									pic0[g_offset] = green[CLIP ((y1 - u1))];
									pic0[b_offset] = blue[CLIP ((y1 + u2))];
									pic0 += bpp;
									/* bottom pixel Right */
									y1 = outy[outy2++];
									pic1[r_offset] = red[CLIP ((y1 + v1))];
									pic1[g_offset] = green[CLIP ((y1 - u1))];
									pic1[b_offset] = blue[CLIP ((y1 + u2))];
									pic1 += bpp;
									/* bottom pixel Left */
									y1 = outy[outy2++];
									pic1[r_offset] = red[CLIP ((y1 + v1))];
									pic1[g_offset] = green[CLIP ((y1 - u1))];
									pic1[b_offset] = blue[CLIP ((y1 + u2))];
									pic1 += bpp;

								}
								if (j == 3) {
									outy = out + 128;
								} else {
									outy += 16;
								}
								outy1 = 0;
								outy2 = 8;
								pic0 += nextline;
								pic1 += nextline;

							}
							picx += 16 * bpp;
						} 
					}
					if (my >= cropy1)
						picy += nextblk;

				}
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_YUV420P:
			{
				nextline = (softwidth << 1) - 16;
				nextuv = (softwidth >> 1) - 8;
				nextblk = softwidth << 4;
				nextnewline = softwidth << 2;
				for (my = 0, picy = 0, pocy = 0; my < mcusy;
				     my++) {
					for (mx = 0, picx = 0, pocx = 0;
					     mx < mcusx; mx++) {
						decode_mcus (&decode->in, dcts, 6,
							     decode->dscans, max);
						if ((my >= cropy1)
						    && (my < mcusy - cropy2)
						    && (mx >= cropx1)
						    && (mx < mcusx - cropx2)) {
							idct (dcts, out,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[0]);
							idct (dcts + 64,
							      out + 64,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[1]);
							idct (dcts + 128,
							      out + 128,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[2]);
							idct (dcts + 192,
							      out + 192,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[3]);
							idct (dcts + 256,
							      out + 256,
							      decode->dquant[1],
							      IFIX (0.5),
							      max[4]);
							idct (dcts + 320,
							      out + 320,
							      decode->dquant[2],
							      IFIX (0.5),
							      max[5]);

							pic0 = pic + picx +
								picy;
							pic1 = pic0 + softwidth;
							outv = V + (pocx +
								    pocy);
							outu = U + (pocx +
								    pocy);
							outy = out;
							outy1 = 0;
							outy2 = 8;
							inv = out + 64 * 4;
							inu = out + 64 * 5;
							for (j = 0; j < 8; j++) {
								for (k = 0;
								     k < 8;
								     k++) {
									if (k ==
									    4) {
										outy1 += 56;
										outy2 += 56;
									}
									/* outup 4 pixels */

									*pic0++ = outy[outy1++];
									*pic0++ = outy[outy1++];
									*pic1++ = outy[outy2++];
									*pic1++ = outy[outy2++];
									*outv++ = 128 + *inv++;
									*outu++ = 128 + *inu++;
								}
								if (j == 3) {
									outy = out + 128;
								} else {
									outy += 16;
								}
								outy1 = 0;
								outy2 = 8;
								pic0 += nextline;
								pic1 += nextline;
								outv += nextuv;
								outu += nextuv;
							}
							picx += 16;
							pocx += 8;
						} 
					}
					if (my >= cropy1) {
						picy += nextblk;
						pocy += nextnewline;
					}
				}
			}
			myframe->scanlength = (long)((softwidth*softheight*3)>>1);
			break;
		default:
			break;
	}			// end case
	return 0;
}

static int
jpeg_decode422 (struct spca50x_frame *myframe, int force_rgb)
{
	int mcusx, mcusy, mx, my;	
	int *dcts = myframe->dcts;	
	int *out =myframe->out;
	int *max=myframe->max;
	int bpp;
	int framesize, frameUsize;
	int k, j;
	int nextline, nextuv, nextblk, nextnewline;
	unsigned char *pic0, *pic1, *outv, *outu;
	__u16 *pix1,*pix2;
	int picy, picx, pocx, pocy;
	unsigned char *U, *V;
	int *outy, *inv, *inu;
	int outy1, outy2;
	int v, u, y1, v1, u1, u2;
	int r_offset, g_offset, b_offset;
	
	unsigned char *pic = myframe->data; /* output surface */
	unsigned char *buf = myframe->tmpbuffer; /* input surface */
	int width = myframe->hdrwidth; 
	int height = myframe->hdrheight;
	int softwidth = myframe->width;
	int softheight = myframe->height;
	//int method = myframe->method;
	int format = myframe->format;
	int cropx1 = myframe->cropx1;
	int cropx2 = myframe->cropx2;
	int cropy1 = myframe->cropy1;
	int cropy2 = myframe->cropy2;
	unsigned char *red = myframe->decoder->Red;
	unsigned char *green = myframe->decoder->Green;
	unsigned char * blue = myframe->decoder->Blue;
	struct dec_data *decode= myframe->decoder;
	if ((height & 15) || (width & 7))
		return 1;
	if (width < softwidth || height < softheight)
		return 1;
	
	mcusx = width >> 4;
	mcusy = height >> 3;
	framesize = softwidth * softheight;
	frameUsize = framesize >> 2;
	jpeg_reset_input_context (decode,buf,1);

	/* for each component. Reset dc values. */
	dec_initscans(decode);
	/* rgb or bgr like U or V that's the question */
	if (force_rgb) {
		U = pic + framesize;
		V = U + frameUsize;
		r_offset = 2;
		g_offset = 1;
		b_offset = 0;
	} else {
		V = pic + framesize;
		U = V + frameUsize;
		r_offset = 0;
		g_offset = 1;
		b_offset = 2;
	}

	/* Decode to the correct format. */
	switch (format) {
		case VIDEO_PALETTE_RGB565:
			{	bpp = 2;
				nextline = ((softwidth << 1) - 16);// *bpp;
				nextblk = bpp * (softwidth << 3);
				nextnewline = softwidth ; // *bpp;
				for (my = 0, picy = 0; my < mcusy; my++) {
					for (mx = 0, picx = 0; mx < mcusx; mx++) {
						if (decode->info.dri && !--decode->info.nm)
							if (dec_checkmarker(decode))
								return ERR_WRONG_MARKER;
						decode_mcus (&decode->in, dcts, 4,
							     decode->dscans, max);
						if ((my >= cropy1)
						    && (my < mcusy - cropy2)
						    && (mx >= cropx1)
						    && (mx < mcusx - cropx2)) {
							idct (dcts, out,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[0]);
							idct (dcts + 64,
							      out + 64,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[1]);
							idct (dcts + 128,
							      out + 256,
							      decode->dquant[1],
							      IFIX (0.5),
							      max[2]);
							idct (dcts + 192,
							      out + 320,
							      decode->dquant[2],
							      IFIX (0.5),
							      max[3]);
							
							pix1 = (__u16 *)(pic + picx + picy);
							pix2 = pix1 + nextnewline;
							outy = out;
							outy1 = 0;
							outy2 = 8;
							inv = out + 64 * 4;
							inu = out + 64 * 5;
							for (j = 0; j < 4; j++) {
								for (k = 0;
								     k < 8;
								     k++) {
									if (k ==
									    4) {
										outy1 += 56;
										outy2 += 56;
									}
									/* outup 4 pixels Colors are treated as 411 */
									/* get the UV colors need to change UV order for force rgb? */
									if ( force_rgb){
										
										u = *inv++;
										v = *inu++;	
									} else {
										
										v = *inv++;
										u = *inu++;	
									}
									/* MX color space why not? */
									v1 = ((v << 10) + (v << 9)) >> 10;
									u1 = ((u << 8) + (u << 7) + (v << 9) + (v << 4)) >> 10;
									u2 = ((u << 11) + (u << 4)) >> 10;
									/* top pixel Right */
									y1 = outy[outy1++];
									*pix1++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
									 	  ((green[CLIP((y1 - u1))] & 0xFC) << 3) |
									  	  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;
									/* top pixel Left */
									y1 = outy[outy1++];	  
									*pix1++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 | 
										  ((green[CLIP((y1 - u1))] & 0xFC) << 3) | 
										  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;
									
									/* bottom pixel Right */
									y1 = outy[outy2++];
									*pix2++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3 |
									          ((green[CLIP((y1 - u1))] & 0xFC) << 3) | 
										  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;
									/* bottom pixel Left */
									y1 = outy[outy2++];	  
									*pix2++ = ((red[CLIP((y1 + v1))] & 0xF8) >> 3| 
									          ((green[CLIP((y1 - u1))] & 0xFC) << 3) | 
										  ((blue[CLIP((y1 + u2))] & 0xF8) << 8)) ;	

								}
								
								outy += 16;
								outy1 = 0;
								outy2 = 8;
								pix1 += nextline;
								pix2 += nextline;

							}
							picx += 16 * bpp;
						} 
					}
					if (my >= cropy1)
						picy += nextblk;

				}
			
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_RGB32:
		case VIDEO_PALETTE_RGB24:
			{	bpp = (format == VIDEO_PALETTE_RGB32) ? 4 : 3;
				nextline = bpp * ((softwidth << 1) - 16);
				nextblk = bpp * (softwidth << 3);
				nextnewline = bpp * softwidth;

				for (my = 0, picy = 0; my < mcusy; my++) {
					for (mx = 0, picx = 0; mx < mcusx; mx++) {
						if (decode->info.dri && !--decode->info.nm)
							if (dec_checkmarker(decode))
								return ERR_WRONG_MARKER;
						decode_mcus (&decode->in, dcts, 4,
							     decode->dscans, max);
						if ((my >= cropy1)
						    && (my < mcusy - cropy2)
						    && (mx >= cropx1)
						    && (mx < mcusx - cropx2)) {
							idct (dcts, out,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[0]);
							idct (dcts + 64,
							      out + 64,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[1]);
							idct (dcts + 128,
							      out + 256,
							      decode->dquant[1],
							      IFIX (0.5),
							      max[2]);
							idct (dcts + 192,
							      out + 320,
							      decode->dquant[2],
							      IFIX (0.5),
							      max[3]);
							
							pic0 = pic + picx +
								picy;
							pic1 = pic0 + nextnewline;
							outy = out;
							outy1 = 0;
							outy2 = 8;
							inv = out + 64 * 4;
							inu = out + 64 * 5;

							for (j = 0; j < 4; j++) {
								for (k = 0;
								     k < 8;
								     k++) {
									if (k ==
									    4) {
										outy1 += 56;
										outy2 += 56;
									}
									/* outup 4 pixels Colors are treated as 411 */
									
									v = *inv++;
									u = *inu++;
									
									/* MX color space why not? */
									v1 = ((v << 10) + (v << 9)) >> 10;
									u1 = ((u << 8) + (u << 7) + (v << 9) + (v << 4)) >> 10;
									u2 = ((u << 11) + (u << 4)) >> 10;
									/* top pixel Right */
									y1 = outy[outy1++];
									pic0[r_offset] = red[CLIP ((y1 + v1))];
									pic0[g_offset] = green[CLIP ((y1 - u1))];
									pic0[b_offset] = blue[CLIP ((y1 + u2))];
									pic0 += bpp;
									/* top pixel Left */
									y1 = outy[outy1++];
									pic0[r_offset] = red[CLIP ((y1 + v1))];
									pic0[g_offset] = green[CLIP ((y1 - u1))];
									pic0[b_offset] = blue[CLIP ((y1 + u2))];
									pic0 += bpp;
									/* bottom pixel Right */
									y1 = outy[outy2++];
									pic1[r_offset] = red[CLIP ((y1 + v1))];
									pic1[g_offset] = green[CLIP ((y1 - u1))];
									pic1[b_offset] = blue[CLIP ((y1 + u2))];
									pic1 += bpp;
									/* bottom pixel Left */
									y1 = outy[outy2++];
									pic1[r_offset] = red[CLIP ((y1 + v1))];
									pic1[g_offset] = green[CLIP ((y1 - u1))];
									pic1[b_offset] = blue[CLIP ((y1 + u2))];
									pic1 += bpp;

								}
								
								outy += 16;
								outy1 = 0;
								outy2 = 8;
								pic0 += nextline;
								pic1 += nextline;

							}

							picx += 16 * bpp;
						} 
					}
					if (my >= cropy1)
						picy += nextblk;

				}

			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_YUV420P:
			{
				nextline = (softwidth << 1) - 16;
				nextuv = (softwidth >> 1) - 8;
				nextblk = softwidth << 3;
				nextnewline = softwidth << 1;//2
				for (my = 0, picy = 0, pocy = 0; my < mcusy;
				     my++) {
					for (mx = 0, picx = 0, pocx = 0;
					     mx < mcusx; mx++) {
					     	if (decode->info.dri && !--decode->info.nm)
							if (dec_checkmarker(decode))
								return ERR_WRONG_MARKER;
						decode_mcus (&decode->in, dcts, 4,
							     decode->dscans, max);
						if ((my >= cropy1)
						    && (my < mcusy - cropy2)
						    && (mx >= cropx1)
						    && (mx < mcusx - cropx2)) {
							idct (dcts, out,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[0]);
							idct (dcts + 64,
							      out + 64,
							      decode->dquant[0],
							      IFIX (128.5),
							      max[1]);
							idct (dcts + 128,
							      out + 256,
							      decode->dquant[1],
							      IFIX (0.5),
							      max[2]);
							idct (dcts + 192,
							      out + 320,
							      decode->dquant[2],
							      IFIX (0.5),
							      max[3]);
							
							pic0 = pic + picx +
								picy;
							pic1 = pic0 + softwidth;
							outv = V + (pocx +
								    pocy);
							outu = U + (pocx +
								    pocy);
							outy = out;
							outy1 = 0;
							outy2 = 8;
							inv = out + 64 * 4;
							inu = out + 64 * 5;
							for (j = 0; j < 4; j++) {
								for (k = 0;
								     k < 8;
								     k++) {
									if (k ==
									    4) {
										outy1 += 56;
										outy2 += 56;
									}
									/* outup 4 pixels */

									*pic0++ = CLIP(outy[outy1]);outy1++;
									*pic0++ = CLIP(outy[outy1]);outy1++;
									*pic1++ = CLIP(outy[outy2]);outy2++;
									*pic1++ = CLIP(outy[outy2]);outy2++; 
									/* maybe one day yuv422P */
									 *outv++ = CLIP(128 + *inv);inv++; 
									 *outu++ = CLIP(128 + *inu);inu++; 
								}
								
								outy += 16;
								outy1 = 0;
								outy2 = 8;
								pic0 += nextline;
								pic1 += nextline;
								outv += nextuv;
								outu += nextuv;
							}
							picx += 16;
							pocx += 8;
						} 
					}
					if (my >= cropy1) {
						picy += nextblk;
						pocy += nextnewline;
					}
				}
			}
			myframe->scanlength = (long)((softwidth*softheight*3)>>1);	
			break;
		default:
			break;
	}			// end case
	return 0;
}


static int 
bayer_decode(struct spca50x_frame *myframe, int force_rgb )
{
	
	int r_offset, g_offset, b_offset;
	int my , mx; /* scan input surface */
	unsigned char *pic1; /* output surface */
	__u16 *pix1,*pix2; /* same for 16 bits output */
	unsigned char *U, *V; /* chroma output pointer */
	unsigned char inr, ing1, ing2, inb, ing; /* srgb input */
	int inl,inl1; /* offset line input */
	int nextinline, nextoutline;
	unsigned char r,b,y1,y2,y3,y4;
	int u,v;
	int bpp;
	
	unsigned char *pic = myframe->data; /* output surface */
	unsigned char *buf = myframe->tmpbuffer; /* input surface */
	int width = myframe->hdrwidth; 
	int height = myframe->hdrheight;
	int softwidth = myframe->width;
	int softheight = myframe->height;
	//int method = myframe->method;
	int format = myframe->format;
	int cropx1 = myframe->cropx1;
	int cropx2 = myframe->cropx2;
	int cropy1 = myframe->cropy1;
	int cropy2 = myframe->cropy2;
	unsigned char *red = myframe->decoder->Red;
	unsigned char *green = myframe->decoder->Green;
	unsigned char * blue = myframe->decoder->Blue;
	int framesize, frameUsize;
	inr=ing1=ing2=ing=inb=r=b=0; //compiler maybe happy !!
	framesize = softwidth * softheight;
	frameUsize = framesize >> 2;
	/* rgb or bgr like U or V that's the question */
	if (force_rgb) {
		V = pic + framesize;
		U = V + frameUsize;		
		r_offset = 0;
		g_offset = 1;
		b_offset = 2;	
	} else {
		U = pic + framesize;
		V = U + frameUsize;		
		r_offset = 2;
		g_offset = 1;
		b_offset = 0;
	}
	/* initialize input pointer */
	inl = 0;
	inl1 = width ;
	nextinline = width << 1;
	/* Decode to the correct format. */
	switch (format) {
		case VIDEO_PALETTE_RGB565:
			{	bpp = 2;
			/* initialize */				
				pix1 = (__u16*) pic;
				pix2 = pix1 + softwidth;				
				for ( my =0; my < height; my += 2){
					for ( mx = 0 ; mx < width ; mx += 2 ){
					/* test if we need to decode */
					  if ((my >= cropy1)
						    && (my < height - cropy2)
						    && (mx >= cropx1)
						    && (mx < width - cropx2)) {
						    /* yes decode  
						    ing1 = buf [inl + mx] ;
						if(force_rgb){	
						    inb = buf [inl + 1 + mx] ;
						    inr = buf [inl1 + mx];					    
						    } else {
						    inr = buf [inl + 1 + mx] ;
						    inb = buf [inl1 + mx];
						    }
						    ing2 = buf [inl1 + 1 + mx];
						    ing = (ing1 + ing2) >> 1;
						    */
						   
						    	/* yes decode GBRG */
						   	 ing1 = buf [inl + mx] ;
							if(force_rgb){	
						    		inb = buf [inl + 1 + mx] ;
						    		inr = buf [inl1 + mx];					    
						    	} else {
						    		inr = buf [inl + 1 + mx] ;
						    		inb = buf [inl1 + mx];
						    		} 
						    	ing2 = buf [inl1 +1 + mx];
						    	ing = (ing1 + ing2) >> 1;
						   
						    /* top pixel Right */
						    		
							*pix1++ = ((red[inr] & 0xF8) >> 3 |
								  ((green[ing1] & 0xFC) << 3) |
								  ((blue[inb] & 0xF8) << 8)) ;		
						    /* top pixel Left */
						    
							*pix1++ = ((red[inr] & 0xF8) >> 3 |
								  ((green[ing] & 0xFC) << 3) |
								  ((blue[inb] & 0xF8) << 8)) ;		
						    /* bottom pixel Right */
						   
							*pix2++ = ((red[inr] & 0xF8) >> 3 |
								  ((green[ing] & 0xFC) << 3) |
								  ((blue[inb] & 0xF8) << 8)) ;		
						    /* bottom pixel Left */
						    
							*pix2++ = ((red[inr] & 0xF8) >> 3 |
								  ((green[ing2] & 0xFC) << 3) |
								  ((blue[inb] & 0xF8) << 8)) ;								    
						    
					  } // end test decode
					} // end mx loop
					inl += nextinline;
					inl1 += nextinline ;
					
					if (my >= cropy1){
						/* are we in a decode surface move the output pointer */
						pix1 += (softwidth);
						pix2 += (softwidth);
					}
					
				} // end my loop
			
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_RGB32:
		case VIDEO_PALETTE_RGB24:
			{	bpp = (format == VIDEO_PALETTE_RGB32) ? 4 : 3;
				/* initialize */
				nextoutline  = bpp * softwidth;
				pic1 = pic + nextoutline;				
				for ( my =0; my < height; my += 2){
					for ( mx = 0 ; mx < width ; mx += 2 ){
					/* test if we need to decode */
					  if ((my >= cropy1)
						    && (my < height - cropy2)
						    && (mx >= cropx1)
						    && (mx < width - cropx2)) {
						    
						  
						    	/* yes decode GBRG */
						   	 ing1 = buf [inl + mx] ;
						    	inb = buf [inl+ 1 + mx] ;
						    	inr = buf [inl1 + mx];
						    	ing2 = buf [inl1 +1 + mx];
						    	ing = (ing1 + ing2) >> 1;
						
						   /* yes decode GBGR 
						    ing1 = buf [inl + mx] ;
						    inb = buf [inl+ 1 + mx] ;
						    ing2 = buf [inl1 + mx];
						    inr = buf [inl1 +1 + mx];
						    ing = (ing1 + ing2) >> 1;
						    */
						   /* yes decode RGGB 
						    inr = buf [inl + mx] ;
						    ing1 = buf [inl+ 1 + mx] ;
						    ing2 = buf [inl1 + mx];
						    inb = buf [inl1 +1 + mx];
						    ing = (ing1 + ing2) >> 1;
						   */
						   /* yes decode GGBR 
						    ing1 = buf [inl + mx] ;
						    ing2 = buf [inl+ 1 + mx] ;
						    inb = buf [inl1 + mx];
						    inr = buf [inl1 +1 + mx];
						    ing = (ing1 + ing2) >> 1;
						   */
						   /* yes decode BRGG 
						    inr = buf [inl + mx] ;
						    inb = buf [inl+ 1 + mx] ;
						    ing1 = buf [inl1 + mx];
						    ing2 = buf [inl1 +1 + mx];
						    ing = (ing1 + ing2) >> 1;
						   */				    
						    /* top pixel Right */
									
									pic[r_offset] = red[inr];
									pic[g_offset] = green[ing1];
									pic[b_offset] = blue[inb];
									pic += bpp;
						    /* top pixel Left */
						   
									pic[r_offset] = red[inr];
									pic[g_offset] = green[ing];
									pic[b_offset] = blue[inb];
									pic += bpp;
						    /* bottom pixel Right */
						    
									pic1[r_offset] = red[inr];
									pic1[g_offset] = green[ing];
									pic1[b_offset] = blue[inb];
									pic1 += bpp;
						    /* bottom pixel Left */
						    
									pic1[r_offset] = red[inr];
									pic1[g_offset] = green[ing2];
									pic1[b_offset] = blue[inb];
									pic1 += bpp;						    
						    
					  } // end test decode
					} // end mx loop
					inl += nextinline;
					inl1 += nextinline ;
					
					if (my >= cropy1){
						/* are we in a decode surface move the output pointer */
						pic += (nextoutline);
						pic1 += (nextoutline);
					}
					
				} // end my loop
			}
			myframe->scanlength = (long)(softwidth*softheight*bpp);
			break;
		case VIDEO_PALETTE_YUV420P:
			{ /* Not yet implemented */
				nextoutline  = softwidth;
				pic1 = pic + nextoutline;

				for ( my =0; my < height; my += 2){
					for ( mx = 0; mx < width ; mx +=2 ){
					/* test if we need to decode */
					  if ((my >= cropy1)
						    && (my < height - cropy2)
						    && (mx >= cropx1)
						    && (mx < width - cropx2)) {
						    /* yes decode 
						    ing1 = buf [inl + mx] ;
						    b = buf [inl + 1 + mx] ;
						    r = buf [inl1 + mx];
						    ing2 = buf [inl1 + 1 + mx];
						    ing = (ing1 + ing2) >> 1;
						    */
							  
						    	/* yes decode GBRG 
						   	ing1 = buf [inl + mx] ;
						    	b = buf [inl+ 1 + mx] ;
						    	r = buf [inl1 + mx];
						    	ing2 = buf [inl1 +1 + mx];
						    	ing = (ing1 + ing2) >> 1;
							*/
							/* yes decode GBRG */
						   	ing1 = green[buf [inl + mx]] ;
						    	b = blue[buf [inl+ 1 + mx]] ;
						    	r = red[buf [inl1 + mx]];
						    	ing2 = green[buf [inl1 +1 + mx]];
						    	ing = (ing1 + ing2) >> 1;
							
							inr = ((r << 8)-( r << 4) -( r << 3)) >> 10;
							inb = (( b << 7) >>10);
							ing1 = ((ing1 <<9)+(ing1 << 7)+(ing1 << 5)) >> 10;
							ing2 = ((ing2 <<9)+(ing2 << 7)+(ing2 << 5)) >> 10;
							ing = ((ing <<9)+(ing << 7)+(ing << 5)) >> 10;

						    /* top pixel Right */
							y1= CLIP((inr+ing1+inb));
							*pic++ = y1;
						    /* top pixel Left */
						   	y2= CLIP((inr+ing+inb));
							*pic++ = y2;
						    /* bottom pixel Right */
							y3= CLIP((inr+ing+inb));
							*pic1++ = y3;
						    /* bottom pixel Left */
							y4= CLIP((inr+ing2+inb));
							*pic1++ = y4;
						/* U V plane */
							v = r - ((y1+y2+y3+y4) >> 2);
							u = ((v << 9) + (v <<7) + (v << 5)) >> 10;
							v = (b - ((y1+y2+y3+y4) >> 2)) >> 1;
							// *U++ = 128 + u;
							// *V++ = 128 + v;

							*U++ = 128 + v;
							*V++ = 128 + u;

					  } // end test decode
					} // end mx loop
					inl += nextinline;
					inl1 += nextinline ;

					if (my >= cropy1){
						/* are we in a decode surface move the output pointer */
						pic += softwidth ;
						pic1 += softwidth ;
					}

				} // end my loop
		
			}
			myframe->scanlength = (long)((softwidth*softheight*3)>>1);
			break;
		default:
			break;
	}// end case
	return 0;
} // end bayer_decode

/* this function restore the missing header for the jpeg camera */
/* adapted from Till Adam create_jpeg_from_data() */
static int make_jpeg (struct spca50x_frame *myframe)
{
 int width;
 int height;
 int inputsize;
 int i;
 __u8 value;
 __u8 *buf;
 __u8 *dst;
 __u8 *start;
	 dst = myframe->data;
	 start = dst;
	buf = myframe->tmpbuffer;
	width = myframe->hdrwidth;
	height = myframe->hdrheight;	
	inputsize = width*height;
	/* set up the default header */
	memcpy(dst,JPEGHeader,JPEGHEADER_LENGTH);
	/* setup quantization table */
	*(dst+6) = 0;
	memcpy(dst+7,myframe->decoder->quant[0],64);
	*(dst+7+64) = 1;
	memcpy(dst+8+64,myframe->decoder->quant[1],64);
	
	*(dst + 564) = width & 0xFF;	//Image width low byte
	*(dst + 563) = width >> 8 & 0xFF;	//Image width high byte
	*(dst + 562) = height & 0xFF;	//Image height low byte
	*(dst + 561) = height >> 8 & 0xFF;	//Image height high byte
	/* set the format */
	if(myframe->cameratype == JPEG){
	 *(dst + 567) = 0x22;
	 dst += JPEGHEADER_LENGTH;
	 for (i=0 ; i < inputsize; i++){
		value = *(buf + i) & 0xFF;
		*dst = value;
		dst++;
		if ((*(buf+i)+*(buf+i+1)+ *(buf+i+2)+*(buf+i+3)) == 0)
		break;
		if (value == 0xFF){
			*dst = 0;
			dst++;
		}
		
	 }
	 /* Add end of image marker */
	 *(dst++) = 0xFF; 	
	 *(dst++) = 0xD9; 
	} else {
	 *(dst + 567) = 0x21;
	 dst += JPEGHEADER_LENGTH;
	 for (i=0 ; i < inputsize; i++){
		value = *(buf + i) & 0xFF;
		*dst = value;
		dst++;
		if((value == 0XFF) && (*(buf+i+1) == 0xD9)){
			*dst = *(buf+i+1);
			dst++;
			break;
		}
	 }
	}
	
	
	myframe->scanlength = (long)(dst - start);
return 0;
}

static int make_jpeg_conexant (struct spca50x_frame *myframe)
{

 __u8 *buf;
 __u8 *dst;

	dst = myframe->data;
	buf = myframe->tmpbuffer;
	memcpy(dst,JPEGHeader,JPEGHEADER_LENGTH-33);
	*(dst+6) = 0;
	memcpy(dst+7,myframe->decoder->quant[0],64);
	*(dst+7+64) = 1;
	memcpy(dst+8+64,myframe->decoder->quant[1],64);
	dst += (JPEGHEADER_LENGTH-33);
	memcpy(dst,buf,myframe->scanlength);
	myframe->scanlength +=(JPEGHEADER_LENGTH-33);
return 0;
}
