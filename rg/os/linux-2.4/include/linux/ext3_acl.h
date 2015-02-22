/*
  File: linux/ext3_acl.h

  (C) 2001 Andreas Gruenbacher, <a.gruenbacher@computer.org>
*/

#include <linux/init.h>
#include <linux/posix_acl.h>
#include <linux/xattr_acl.h>

#define EXT3_ACL_VERSION	0x0001
#define EXT3_ACL_MAX_ENTRIES	32

typedef struct {
	__u16		e_tag;
	__u16		e_perm;
	__u32		e_id;
} ext3_acl_entry;

typedef struct {
	__u16		e_tag;
	__u16		e_perm;
} ext3_acl_entry_short;

typedef struct {
	__u32		a_version;
} ext3_acl_header;

static inline size_t ext3_acl_size(int count)
{
	if (count <= 4) {
		return sizeof(ext3_acl_header) +
		       count * sizeof(ext3_acl_entry_short);
	} else {
		return sizeof(ext3_acl_header) +
		       4 * sizeof(ext3_acl_entry_short) +
		       (count - 4) * sizeof(ext3_acl_entry);
	}
}

static inline int ext3_acl_count(size_t size)
{
	ssize_t s;
	size -= sizeof(ext3_acl_header);
	s = size - 4 * sizeof(ext3_acl_entry_short);
	if (s < 0) {
		if (size % sizeof(ext3_acl_entry_short))
			return -1;
		return size / sizeof(ext3_acl_entry_short);
	} else {
		if (s % sizeof(ext3_acl_entry))
			return -1;
		return s / sizeof(ext3_acl_entry) + 4;
	}
}

#ifdef __KERNEL__
# ifdef CONFIG_EXT3_FS_POSIX_ACL

/* Value for inode->u.ext3_i.i_acl and inode->u.ext3_i.i_default_acl
   if the ACL has not been cached */
# define EXT3_ACL_NOT_CACHED ((void *)-1)

/* acl.c */
extern int ext3_permission (struct inode *, int);
extern struct posix_acl *ext3_get_acl (struct inode *, int);
extern int ext3_set_acl (struct inode *, int, struct posix_acl *);
extern int ext3_acl_chmod (handle_t *, struct inode *);
extern int ext3_init_acl (handle_t *, struct inode *, struct inode *);
extern int ext3_get_acl_xattr (struct inode *, int, void *, size_t);
extern int ext3_set_acl_xattr (struct inode *, int, void *, size_t);

extern int init_ext3_acl(void) __init;
extern void exit_ext3_acl(void);

# else  /* CONFIG_EXT3_FS_POSIX_ACL */
#  include <linux/sched.h>
#  define ext3_permission NULL
#  define ext3_get_acl	NULL
#  define ext3_set_acl	NULL

static inline int
ext3_acl_chmod(handle_t *handle, struct inode *inode)
{
	return 0;
}

static inline int
ext3_init_acl(handle_t *handle, struct inode *inode, struct inode *dir)
{
	inode->i_mode &= ~current->fs->umask;
	ext3_mark_inode_dirty(handle, inode);
	return 0;
}

static inline int
init_ext3_acl(void)
{
	return 0;
}

static inline void
exit_ext3_acl(void)
{
}

# endif  /* CONFIG_EXT3_FS_POSIX_ACL */
#endif  /* __KERNEL__ */
