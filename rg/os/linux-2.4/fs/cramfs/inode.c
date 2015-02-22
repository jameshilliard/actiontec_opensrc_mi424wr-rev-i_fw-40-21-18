/*
 * inode.c
 *
 * Copyright (C) 1999 Linus Torvalds
 * Copyright (C) 2000-2002 Transmeta Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * Compressed ROM filesystem for Linux.
 */

/*
 * These are the VFS interfaces to the compressed ROM filesystem.
 * The actual compression is based on zlib, see the other files.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <linux/cramfs_fs.h>
#include <asm/semaphore.h>

#include <asm/uaccess.h>

#define CRAMFS_SB_MAGIC u.cramfs_sb.magic
#define CRAMFS_SB_SIZE u.cramfs_sb.size
#define CRAMFS_SB_BLOCKS u.cramfs_sb.blocks
#define CRAMFS_SB_FILES u.cramfs_sb.files
#define CRAMFS_SB_FLAGS u.cramfs_sb.flags
#define CRAMFS_SB_SUB_TYPE u.cramfs_sb.sub_type
#define CRAMFS_SB_UNCOMP_BUFFER u.cramfs_sb.uncomp_buffer
#define CRAMFS_SB_UNCOMP_BLK_OFFSET u.cramfs_sb.uncomp_blk_offset
#define CRAMFS_SB_UNCOMP_BLK_DATA_SIZE u.cramfs_sb.uncomp_blk_data_size

#define SB_COMP_METHOD(sb) ((sb->CRAMFS_SB_FLAGS & \
	CRAMFS_FLAG_COMP_METHOD_MASK) >> CRAMFS_FLAG_COMP_METHOD_SHIFT)

#define CRAMFS_SUB_TYPE_NORMAL 0
#define CRAMFS_SUB_TYPE_INIT 1

static struct super_operations cramfs_ops;
static struct inode_operations cramfs_dir_inode_operations;
static struct file_operations cramfs_directory_operations;
static struct address_space_operations cramfs_aops;

static DECLARE_MUTEX(read_mutex);


/* These two macros may change in future, to provide better st_ino
   semantics. */
#define CRAMINO(x)	(CRAMFS_GET_OFFSET(x) ? CRAMFS_GET_OFFSET(x)<<2 : 1)
#define OFFSET(x)	((x)->i_ino)

static struct inode *get_cramfs_inode(struct super_block *sb, struct cramfs_inode * cramfs_inode)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
	        inode->i_atime = inode->i_mtime = inode->i_ctime =
		    CRAMFS_TIMESTAMP;
		inode->i_mode = CRAMFS_16(cramfs_inode->mode);
		inode->i_uid = CRAMFS_16(cramfs_inode->uid);
		inode->i_size = CRAMFS_24(cramfs_inode->size);
		inode->i_blocks = (CRAMFS_24(cramfs_inode->size) - 1)/512 + 1;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_gid = cramfs_inode->gid;
		inode->i_ino = CRAMINO(cramfs_inode);
		/* inode->i_nlink is left 1 - arguably wrong for directories,
		   but it's the best we can do without reading the directory
		   contents.  1 yields the right result in GNU find, even
		   without -noleaf option. */
		insert_inode_hash(inode);
		if (S_ISREG(inode->i_mode)) {
			inode->i_fop = &generic_ro_fops;
			inode->i_data.a_ops = &cramfs_aops;
		} else if (S_ISDIR(inode->i_mode)) {
			inode->i_op = &cramfs_dir_inode_operations;
			inode->i_fop = &cramfs_directory_operations;
		} else if (S_ISLNK(inode->i_mode)) {
			inode->i_op = &page_symlink_inode_operations;
			inode->i_data.a_ops = &cramfs_aops;
		} else {
			inode->i_size = 0;
			init_special_inode(inode, inode->i_mode, CRAMFS_24(cramfs_inode->size));
		}
	}
	return inode;
}

/*
 * We have our own block cache: don't fill up the buffer cache
 * with the ROM image, because the way the filesystem is set
 * up the accesses should be fairly regular and cached in the
 * page cache and dentry tree anyway..
 *
 * This also acts as a way to guarantee contiguous areas of up to
 * BLKS_PER_BUF*PAGE_CACHE_SIZE, so that the caller doesn't need to
 * worry about end-of-buffer issues even when decompressing a full
 * page cache.
 */
#define READ_BUFFERS (2)
/* NEXT_BUFFER(): Loop over [0..(READ_BUFFERS-1)]. */
#define NEXT_BUFFER(_ix) ((_ix) ^ 1)

/*
 * BLKS_PER_BUF_SHIFT should be at least 2 to allow for "compressed"
 * data that takes up more space than the original and with unlucky
 * alignment.
 */
#define BLKS_PER_BUF_SHIFT	(2)
#define BLKS_PER_BUF		(1 << BLKS_PER_BUF_SHIFT)
#define BUFFER_SIZE		(BLKS_PER_BUF*PAGE_CACHE_SIZE)

#ifndef CONFIG_COPY_CRAMFS_TO_RAM
static unsigned char read_buffers[READ_BUFFERS][BUFFER_SIZE];
static unsigned buffer_blocknr[READ_BUFFERS];
static struct super_block * buffer_dev[READ_BUFFERS];
static int next_buffer;
#else
extern char __crd_start, __crd_end, __crd_init_start, __crd_init_end;
#endif

static int curr_comp_method = -1;
 
static u32 cramfs_get_blksz(struct super_block *sb)
{
#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
    	u32 blksz_shift;

	blksz_shift = (sb->CRAMFS_SB_FLAGS & CRAMFS_FLAG_BLKSZ_MASK) >>
	    CRAMFS_FLAG_BLKSZ_SHIFT;
    	return PAGE_CACHE_SIZE << blksz_shift;
#endif
	return PAGE_CACHE_SIZE;
}

static u32 cramfs_get_blk_page_ratio(struct super_block *sb)
{
	return cramfs_get_blksz(sb) >> PAGE_CACHE_SHIFT;
}

#ifdef CONFIG_COPY_CRAMFS_TO_RAM
static void *cramfs_start_address(struct super_block *sb)
{
    if (sb->CRAMFS_SB_SUB_TYPE == CRAMFS_SUB_TYPE_NORMAL)
	return &__crd_start;
    else return &__crd_init_start;
}
#endif

/*
 * Returns a pointer to a buffer containing at least LEN bytes of
 * filesystem starting at byte offset OFFSET into the filesystem.
 */
static void *cramfs_read(struct super_block *sb, unsigned int offset, unsigned int len)
{
#ifdef CONFIG_COPY_CRAMFS_TO_RAM
        return cramfs_start_address(sb) + offset;
#else
	struct buffer_head * bh_array[BLKS_PER_BUF];
	struct buffer_head * read_array[BLKS_PER_BUF];
	unsigned i, blocknr, buffer, unread;
	unsigned long devsize;
	int major, minor;

	char *data;

	if (!len)
		return NULL;
	blocknr = offset >> PAGE_CACHE_SHIFT;
	offset &= PAGE_CACHE_SIZE - 1;

	/* Check if an existing buffer already has the data.. */
	for (i = 0; i < READ_BUFFERS; i++) {
		unsigned int blk_offset;

		if (buffer_dev[i] != sb)
			continue;
		if (blocknr < buffer_blocknr[i])
			continue;
		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_CACHE_SHIFT;
		blk_offset += offset;
		if (blk_offset + len > BUFFER_SIZE)
			continue;
		return read_buffers[i] + blk_offset;
	}

	devsize = ~0UL;
	major = MAJOR(sb->s_dev);
	minor = MINOR(sb->s_dev);

	if (blk_size[major])
		devsize = blk_size[major][minor] >> 2;

	/* Ok, read in BLKS_PER_BUF pages completely first. */
	unread = 0;
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct buffer_head *bh;

		bh = NULL;
		if (blocknr + i < devsize) {
			bh = sb_getblk(sb, blocknr + i);
			if (!buffer_uptodate(bh))
				read_array[unread++] = bh;
		}
		bh_array[i] = bh;
	}

	if (unread) {
		ll_rw_block(READ, unread, read_array);
		do {
			unread--;
			wait_on_buffer(read_array[unread]);
		} while (unread);
	}

	/* Ok, copy them to the staging area without sleeping. */
	buffer = next_buffer;
	next_buffer = NEXT_BUFFER(buffer);
	buffer_blocknr[buffer] = blocknr;
	buffer_dev[buffer] = sb;

	data = read_buffers[buffer];
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct buffer_head * bh = bh_array[i];
		if (bh) {
			memcpy(data, bh->b_data, PAGE_CACHE_SIZE);
			brelse(bh);
		} else
			memset(data, 0, PAGE_CACHE_SIZE);
		data += PAGE_CACHE_SIZE;
	}
	return read_buffers[buffer] + offset;
#endif
}


static struct super_block * cramfs_read_super(struct super_block *sb, void *data, int silent)
{
#ifndef CONFIG_COPY_CRAMFS_TO_RAM
	int i;
#endif
	struct cramfs_super super;
	unsigned long root_offset;
	struct super_block * retval = NULL;

	set_blocksize(sb->s_dev, PAGE_CACHE_SIZE);
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	if ((int)data == 0)
	    sb->CRAMFS_SB_SUB_TYPE = CRAMFS_SUB_TYPE_NORMAL;
	else
	    sb->CRAMFS_SB_SUB_TYPE = CRAMFS_SUB_TYPE_INIT;

#ifndef CONFIG_COPY_CRAMFS_TO_RAM
	/* Invalidate the read buffers on mount: think disk change.. */
	for (i = 0; i < READ_BUFFERS; i++)
		buffer_blocknr[i] = -1;
#endif

	down(&read_mutex);
	/* Read the first block and get the superblock from it */
	memcpy(&super, cramfs_read(sb, 0, sizeof(super)), sizeof(super));
	up(&read_mutex);

	/* Do sanity checks on the superblock */
	if (super.magic != CRAMFS_32(CRAMFS_MAGIC)) {
		/* check at 512 byte offset */
		memcpy(&super, cramfs_read(sb, 512, sizeof(super)), sizeof(super));
		if (super.magic != CRAMFS_32(CRAMFS_MAGIC)) {
			printk(KERN_ERR "cramfs: wrong magic. "
			    "found %x, mus be %x\n",
			    super.magic, CRAMFS_32(CRAMFS_MAGIC));
			goto out;
		}
	}

	/* flags is reused several times, so swab it once */
	super.flags = CRAMFS_32(super.flags);

	/* get feature flags first */
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS) {
		printk(KERN_ERR "cramfs: unsupported filesystem features\n");
		goto out;
	}

	/* Check that the root inode is in a sane state */
	if (!S_ISDIR(CRAMFS_16(super.root.mode))) {
		printk(KERN_ERR "cramfs: root is not a directory\n");
		goto out;
	}
	root_offset = CRAMFS_GET_OFFSET(&(super.root)) << 2;
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		sb->CRAMFS_SB_SIZE = CRAMFS_32(super.size);
		sb->CRAMFS_SB_BLOCKS = CRAMFS_32(super.fsid.blocks);
		sb->CRAMFS_SB_FILES = CRAMFS_32(super.fsid.files);
	} else {
		sb->CRAMFS_SB_SIZE = 1 << 28;
		sb->CRAMFS_SB_BLOCKS = 0;
		sb->CRAMFS_SB_FILES = 0;
	}
	sb->CRAMFS_SB_MAGIC = CRAMFS_MAGIC;
	sb->CRAMFS_SB_FLAGS = super.flags;
	if (root_offset == 0)
		printk(KERN_INFO "cramfs: empty filesystem");
	else if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
		 ((root_offset != sizeof(struct cramfs_super)) &&
		  (root_offset != 512 + sizeof(struct cramfs_super))))
	{
		printk(KERN_ERR "cramfs: bad root offset %lu\n", root_offset);
		goto out;
	}

	/* Set it all up.. */
	sb->CRAMFS_SB_UNCOMP_BUFFER = NULL;
	sb->CRAMFS_SB_UNCOMP_BLK_OFFSET = -1;
	if (cramfs_uncompress_init(SB_COMP_METHOD(sb)))
	{
	        printk(KERN_ERR "cramfs: error initializing compression\n");
		goto out;
	}
	sb->s_op = &cramfs_ops;
	sb->s_root = d_alloc_root(get_cramfs_inode(sb, &super.root));
	retval = sb;
out:
	return retval;
}

static int cramfs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = CRAMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_blocks = sb->CRAMFS_SB_BLOCKS;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = sb->CRAMFS_SB_FILES;
	buf->f_ffree = 0;
	buf->f_namelen = CRAMFS_MAXPATHLEN;
	return 0;
}

#ifdef CONFIG_COPY_CRAMFS_TO_RAM
static void cramfs_putsuper(struct super_block *sb)
{
    extern void free_area(unsigned long addr, unsigned long end, char *s);

    /* Cleanup */
    cramfs_uncompress_exit(SB_COMP_METHOD(sb));
    if (sb->CRAMFS_SB_UNCOMP_BUFFER)
	kfree(sb->CRAMFS_SB_UNCOMP_BUFFER);

    if (sb->CRAMFS_SB_SUB_TYPE == CRAMFS_SUB_TYPE_INIT)
	free_area(&__crd_init_start, &__crd_init_end, "init cramfs");
}
#endif

/*
 * Read a cramfs directory entry.
 */
static int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	unsigned int offset;
	int copied;

	/* Offset within the thing. */
	offset = filp->f_pos;
	if (offset >= inode->i_size)
		return 0;
	/* Directory entries are always 4-byte aligned */
	if (offset & 3)
		return -EINVAL;

	copied = 0;
	while (offset < inode->i_size) {
		struct cramfs_inode *de;
		unsigned long nextoffset;
		char *name;
		int namelen, error;

		down(&read_mutex);
		de = cramfs_read(sb, OFFSET(inode) + offset, sizeof(*de)+256);
		up(&read_mutex);
		name = (char *)(de+1);

		/*
		 * Namelengths on disk are shifted by two
		 * and the name padded out to 4-byte boundaries
		 * with zeroes.
		 */
		namelen = CRAMFS_GET_NAMELEN(de) << 2;
		nextoffset = offset + sizeof(*de) + namelen;
		for (;;) {
			if (!namelen)
				return -EIO;
			if (name[namelen-1])
				break;
			namelen--;
		}
		error = filldir(dirent, name, namelen, offset, CRAMINO(de), CRAMFS_16(de->mode) >> 12);
		if (error)
			break;

		offset = nextoffset;
		filp->f_pos = offset;
		copied++;
	}
	return 0;
}

/*
 * Lookup and fill in the inode data..
 */
static struct dentry * cramfs_lookup(struct inode *dir, struct dentry *dentry)
{
	unsigned int offset = 0;
	int sorted = dir->i_sb->CRAMFS_SB_FLAGS & CRAMFS_FLAG_SORTED_DIRS;

	while (offset < dir->i_size) {
		struct cramfs_inode *de;
		char *name;
		int namelen, retval;

		down(&read_mutex);
		de = cramfs_read(dir->i_sb, OFFSET(dir) + offset, sizeof(*de)+256);
		up(&read_mutex);
		name = (char *)(de+1);

		/* Try to take advantage of sorted directories */
		if (sorted && (dentry->d_name.name[0] < name[0]))
			break;

		namelen = CRAMFS_GET_NAMELEN(de) << 2;
		offset += sizeof(*de) + namelen;

		/* Quick check that the name is roughly the right length */
		if (((dentry->d_name.len + 3) & ~3) != namelen)
			continue;

		for (;;) {
			if (!namelen)
				return ERR_PTR(-EIO);
			if (name[namelen-1])
				break;
			namelen--;
		}
		if (namelen != dentry->d_name.len)
			continue;
		retval = memcmp(dentry->d_name.name, name, namelen);
		if (retval > 0)
			continue;
		if (!retval) {
			d_add(dentry, get_cramfs_inode(dir->i_sb, de));
			return NULL;
		}
		/* else (retval < 0) */
		if (sorted)
			break;
	}
	d_add(dentry, NULL);
	return NULL;
}

#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
/* first_page_in_block is the index of data's first page in the uncompressed
 * block */
static int cramfs_get_uncomp_data(struct super_block *sb, void *dst, int dstlen,
    int first_page_in_block, int blk_start_offset, int blk_comp_len)
{
	int copy_bytes;
	int start = first_page_in_block << PAGE_CACHE_SHIFT;
	u32 blksz = cramfs_get_blksz(sb);
	
	if (!sb->CRAMFS_SB_UNCOMP_BUFFER &&
	        !(sb->CRAMFS_SB_UNCOMP_BUFFER =	kmalloc(blksz, GFP_KERNEL)))
	{
		printk(KERN_ERR "Error allocating CRAMFS block cache buffer\n");
		return 0;
	}

	if (blk_start_offset != sb->CRAMFS_SB_UNCOMP_BLK_OFFSET)
	{
		
		sb->CRAMFS_SB_UNCOMP_BLK_OFFSET = blk_start_offset;
		sb->CRAMFS_SB_UNCOMP_BLK_DATA_SIZE = cramfs_uncompress_block(
			sb->CRAMFS_SB_UNCOMP_BUFFER, blksz,
			cramfs_read(sb, blk_start_offset, blk_comp_len),
			blk_comp_len, SB_COMP_METHOD(sb));
	}
	
	copy_bytes = min(sb->CRAMFS_SB_UNCOMP_BLK_DATA_SIZE - start, dstlen);
	
	memcpy(dst, sb->CRAMFS_SB_UNCOMP_BUFFER + start, copy_bytes);
	return copy_bytes;
}
#endif

static int cramfs_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;
	u32 maxpage, bytes_filled;
	void *pgdata;

	/* maxpage is the number of 4K pages in the file */
	maxpage = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	if (page->index < maxpage) {
		struct super_block *sb = inode->i_sb;
		u32 ratio = cramfs_get_blk_page_ratio(sb);
		u32 page_block = page->index / ratio;
		u32 blkptr_offset = OFFSET(inode) + page_block * 4;
		u32 start_offset, compr_len, maxblock;

		/* maxblock is the number of blocks in the file */
		maxblock = (maxpage + ratio - 1) / ratio;

		/* start_offset is the offset of the requested page's
		 * compressed block from the beginning of the cramfs image.
		 * At the beginning of the file there is an array of "block
		 * pointers" and then the compressed blocks themselves.
		 * A "block pointer" means the offset of the compressed block's
		 * end from the beginning of cramfs image.
		 * So, start_offset of block n is calculated in the following
		 * way:
		 * - If n == 0 then immediatly after the array of block
		 *   pointers.
		 * - Else, the end of block n-1 taken from the array of block
		 *   pointers. */
		
		start_offset = OFFSET(inode) + maxblock*4;
		down(&read_mutex);
		if (page_block)
			start_offset = CRAMFS_32(*(u32 *) cramfs_read(sb, blkptr_offset-4, 4));

		/* Compressed length is block n's end minus block n-1's end */
		compr_len = CRAMFS_32(*(u32 *) cramfs_read(sb, blkptr_offset, 4)) - start_offset;
		up(&read_mutex);
		pgdata = kmap(page);
		if (compr_len == 0)
			; /* hole */
		else {
			down(&read_mutex);
#ifdef CONFIG_CRAMFS_DYN_BLOCKSIZE
			bytes_filled = cramfs_get_uncomp_data(sb, pgdata,
				PAGE_CACHE_SIZE,
				page->index - page_block * ratio,
				start_offset, compr_len);
#else
			bytes_filled = cramfs_uncompress_block(pgdata,
				 PAGE_CACHE_SIZE,
				 cramfs_read(sb, start_offset, compr_len),
				 compr_len, SB_COMP_METHOD(sb));
#endif
			up(&read_mutex);
		}
	} else
		pgdata = kmap(page);
	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	kunmap(page);
	flush_dcache_page(page);
	SetPageUptodate(page);
	UnlockPage(page);
	return 0;
}

static struct address_space_operations cramfs_aops = {
	readpage: cramfs_readpage
};

/*
 * Our operations:
 */

/*
 * A directory can only readdir
 */
static struct file_operations cramfs_directory_operations = {
	read:		generic_read_dir,
	readdir:	cramfs_readdir,
};

static struct inode_operations cramfs_dir_inode_operations = {
	lookup:		cramfs_lookup,
};

static struct super_operations cramfs_ops = {
	statfs:		cramfs_statfs,
#ifdef CONFIG_COPY_CRAMFS_TO_RAM
	put_super:	cramfs_putsuper,
#endif
};

#ifdef CONFIG_COPY_CRAMFS_TO_RAM
/* Do not require device on mount operation */
static DECLARE_FSTYPE(cramfs_fs_type, "cramfs", cramfs_read_super, 0);
#else
static DECLARE_FSTYPE_DEV(cramfs_fs_type, "cramfs", cramfs_read_super);
#endif

static int __init init_cramfs_fs(void)
{
	return register_filesystem(&cramfs_fs_type);
}

static void __exit exit_cramfs_fs(void)
{
	unregister_filesystem(&cramfs_fs_type);
}

module_init(init_cramfs_fs)
module_exit(exit_cramfs_fs)
MODULE_LICENSE("GPL");
