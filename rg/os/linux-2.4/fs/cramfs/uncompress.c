/*
 * uncompress.c
 *
 * (C) Copyright 1999 Linus Torvalds
 *
 * cramfs interfaces to the uncompression library. There's really just
 * three entrypoints:
 *
 *  - cramfs_uncompress_init() - called to initialize the thing.
 *  - cramfs_uncompress_exit() - tell me when you're done
 *  - cramfs_uncompress_block() - uncompress a block.
 *
 * NOTE NOTE NOTE! The uncompression is entirely single-threaded. We
 * only have one stream, and we'll initialize it only once even if it
 * then is used by multiple filesystems.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>
#include <linux/cramfs_fs.h>

static z_stream stream;
static int gzip_initialized, lzma_initialized;

int lzma_decode(void *dst, int dstlen, void *src, int srclen);
void lzma_decode_uninit(void);

/* Returns length of decompressed data. */
int cramfs_uncompress_block_gzip(void *dst, int dstlen, void *src, int srclen)
{
	int err;

	stream.next_in = src;
	stream.avail_in = srclen;

	stream.next_out = dst;
	stream.avail_out = dstlen;

	err = zlib_inflateReset(&stream);
	if (err != Z_OK) {
		printk("zlib_inflateReset error %d\n", err);
		zlib_inflateEnd(&stream);
		zlib_inflateInit(&stream);
	}

	err = zlib_inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END)
		goto err;
	return stream.total_out;

err:
	printk("Error %d while decompressing!\n", err);
	printk("%p(%d)->%p(%d)\n", src, srclen, dst, dstlen);
	return 0;
}

int cramfs_uncompress_init_gzip(void)
{
	if (!gzip_initialized++) {
		stream.workspace = vmalloc(zlib_inflate_workspacesize());
		if ( !stream.workspace ) {
			gzip_initialized = 0;
			return -ENOMEM;
		}
		stream.next_in = NULL;
		stream.avail_in = 0;
		zlib_inflateInit(&stream);
	}
	return 0;
}

int cramfs_uncompress_init_lzma(void)
{
       lzma_initialized++;
       return 0;
}

int cramfs_uncompress_exit_gzip(void)
{
	if (!--gzip_initialized) {
		zlib_inflateEnd(&stream);
		vfree(stream.workspace);
	}
	return 0;
}

int cramfs_uncompress_exit_lzma(void)
{
	if (!--lzma_initialized)
		lzma_decode_uninit();
	return 0;
}

int cramfs_uncompress_block(void *dst, int dstlen, void *src, int srclen,
    int comp_method)
{
    	switch (comp_method)
	{
	case CRAMFS_FLAG_COMP_METHOD_GZIP:
		return cramfs_uncompress_block_gzip(dst, dstlen, src, srclen);
	case CRAMFS_FLAG_COMP_METHOD_LZMA:
		return lzma_decode(dst, dstlen, src, srclen);
	case CRAMFS_FLAG_COMP_METHOD_NONE:
		memcpy(dst, src, srclen);
		return srclen;
	}

	return 0;
}

int cramfs_uncompress_init(int comp_method)
{
	switch (comp_method)
	{
	case CRAMFS_FLAG_COMP_METHOD_GZIP:
		return cramfs_uncompress_init_gzip();
	case CRAMFS_FLAG_COMP_METHOD_LZMA:
		return cramfs_uncompress_init_lzma();
	case CRAMFS_FLAG_COMP_METHOD_NONE:
		return 0;
	}

	return 0;
}

int cramfs_uncompress_exit(int comp_method)
{
     	switch (comp_method)
	{
	case CRAMFS_FLAG_COMP_METHOD_GZIP:
		return cramfs_uncompress_exit_gzip();
	case CRAMFS_FLAG_COMP_METHOD_LZMA:
		return cramfs_uncompress_exit_lzma();
	case CRAMFS_FLAG_COMP_METHOD_NONE:
		return 0;
	}

	return 0;
}
