/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2005  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <linux/fuse.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/backing-dev.h>
#include <asm/semaphore.h>

/** Max number of pages that can be used in a single read request */
#define FUSE_MAX_PAGES_PER_REQ 32

/** If more requests are outstanding, then the operation will block */
#define FUSE_MAX_OUTSTANDING 10

/** It could be as large as PATH_MAX, but would that have any uses? */
#define FUSE_NAME_MAX 1024

/** If the FUSE_DEFAULT_PERMISSIONS flag is given, the filesystem
    module will check permissions based on the file mode.  Otherwise no
    permission checking is done in the kernel */
#define FUSE_DEFAULT_PERMISSIONS (1 << 0)

/** If the FUSE_ALLOW_OTHER flag is given, then not only the user
    doing the mount will be allowed to access the filesystem */
#define FUSE_ALLOW_OTHER         (1 << 1)


/** FUSE inode */
struct fuse_inode {
	/** Inode data */
	struct inode inode;

	/** Unique ID, which identifies the inode between userspace
	 * and kernel */
	u64 nodeid;

	/** Number of lookups on this inode */
	u64 nlookup;

	/** The request used for sending the FORGET message */
	struct fuse_req *forget_req;

	/** Time in jiffies until the file attributes are valid */
	unsigned long i_time;
};

/** FUSE specific file data */
struct fuse_file {
	/** Request reserved for flush and release */
	struct fuse_req *release_req;

	/** File handle used by userspace */
	u64 fh;
};

/** One input argument of a request */
struct fuse_in_arg {
	unsigned size;
	const void *value;
};

/** The request input */
struct fuse_in {
	/** The request header */
	struct fuse_in_header h;

	/** True if the data for the last argument is in req->pages */
	unsigned argpages:1;

	/** Number of arguments */
	unsigned numargs;

	/** Array of arguments */
	struct fuse_in_arg args[3];
};

/** One output argument of a request */
struct fuse_arg {
	unsigned size;
	void *value;
};

/** The request output */
struct fuse_out {
	/** Header returned from userspace */
	struct fuse_out_header h;

	/*
	 * The following bitfields are not changed during the request
	 * processing
	 */

	/** Last argument is variable length (can be shorter than
	    arg->size) */
	unsigned argvar:1;

	/** Last argument is a list of pages to copy data to */
	unsigned argpages:1;

	/** Zero partially or not copied pages */
	unsigned page_zeroing:1;

	/** Number or arguments */
	unsigned numargs;

	/** Array of arguments */
	struct fuse_arg args[3];
};

/** The request state */
enum fuse_req_state {
	FUSE_REQ_INIT = 0,
	FUSE_REQ_PENDING,
	FUSE_REQ_READING,
	FUSE_REQ_SENT,
	FUSE_REQ_FINISHED
};

struct fuse_conn;

/**
 * A request to the client
 */
struct fuse_req {
	/** This can be on either unused_list, pending processing or
	    io lists in fuse_conn */
	struct list_head list;

	/** Entry on the background list */
	struct list_head bg_entry;

	/** refcount */
	atomic_t count;

	/*
	 * The following bitfields are either set once before the
	 * request is queued or setting/clearing them is protected by
	 * fuse_lock
	 */

	/** True if the request has reply */
	unsigned isreply:1;

	/** The request is preallocated */
	unsigned preallocated:1;

	/** The request was interrupted */
	unsigned interrupted:1;

	/** Request is sent in the background */
	unsigned background:1;

	/** Data is being copied to/from the request */
	unsigned locked:1;

	/** State of the request */
	enum fuse_req_state state;

	/** The request input */
	struct fuse_in in;

	/** The request output */
	struct fuse_out out;

	/** Used to wake up the task waiting for completion of request*/
	wait_queue_head_t waitq;

	/** Data for asynchronous requests */
	union {
		struct fuse_forget_in forget_in;
		struct fuse_release_in release_in;
		struct fuse_init_in init_in;
		struct fuse_init_out init_out;
		struct fuse_read_in read_in;
	} misc;

	/** page vector */
	struct page *pages[FUSE_MAX_PAGES_PER_REQ];

	/** number of pages in vector */
	unsigned num_pages;

	/** offset of data on first page */
	unsigned page_offset;

	/** Inode used in the request */
	struct inode *inode;

	/** Second inode used in the request (or NULL) */
	struct inode *inode2;

	/** File used in the request (or NULL) */
	struct file *file;

	/** Request completion callback */
	void (*end)(struct fuse_conn *, struct fuse_req *);
};

/**
 * A Fuse connection.
 *
 * This structure is created, when the filesystem is mounted, and is
 * destroyed, when the client device is closed and the filesystem is
 * unmounted.
 */
struct fuse_conn {
	/** The user id for this mount */
	uid_t user_id;

	/** The group id for this mount */
	gid_t group_id;

	/** The fuse mount flags for this mount */
	unsigned flags;

	/** Maximum read size */
	unsigned max_read;

	/** Maximum write size */
	unsigned max_write;

	/** Readers of the connection are waiting on this */
	wait_queue_head_t waitq;

	/** The list of pending requests */
	struct list_head pending;

	/** The list of requests being processed */
	struct list_head processing;

	/** The list of requests under I/O */
	struct list_head io;

	/** Requests put in the background (RELEASE or any other
	    interrupted request) */
	struct list_head background;

	/** Controls the maximum number of outstanding requests */
	struct semaphore outstanding_sem;

	/** This counts the number of outstanding requests if
	    outstanding_sem would go negative */
	unsigned outstanding_debt;

	/** RW semaphore for exclusion with fuse_put_super() */
	struct rw_semaphore sbput_sem;

	/** The list of unused requests */
	struct list_head unused_list;

	/** The next unique request id */
	u64 reqctr;

	/** Mount is active */
	unsigned mounted;

	/** Connection established, cleared on umount, connection
	    abort and device release */
	unsigned connected;

	/** Connection failed (version mismatch).  Cannot race with
	    setting other bitfields since it is only set once in INIT
	    reply, before any other request, and never cleared */
	unsigned conn_error : 1;

	/** Do readpages asynchronously?  Only set in INIT */
	unsigned async_read : 1;

	/*
	 * The following bitfields are only for optimization purposes
	 * and hence races in setting them will not cause malfunction
	 */

	/** Is fsync not implemented by fs? */
	unsigned no_fsync : 1;

	/** Is fsyncdir not implemented by fs? */
	unsigned no_fsyncdir : 1;

	/** Is flush not implemented by fs? */
	unsigned no_flush : 1;

	/** Is setxattr not implemented by fs? */
	unsigned no_setxattr : 1;

	/** Is getxattr not implemented by fs? */
	unsigned no_getxattr : 1;

	/** Is listxattr not implemented by fs? */
	unsigned no_listxattr : 1;

	/** Is removexattr not implemented by fs? */
	unsigned no_removexattr : 1;

	/** Is access not implemented by fs? */
	unsigned no_access : 1;

	/** Is create not implemented by fs? */
	unsigned no_create : 1;

	/** The number of requests waiting for completion */
	atomic_t num_waiting;

	/** Negotiated minor version */
	unsigned minor;

	/** Backing dev info */
	struct backing_dev_info bdi;

	/** kobject */
	struct kobject kobj;
};

static inline struct fuse_conn *get_fuse_conn_super(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct fuse_conn *get_fuse_conn(struct inode *inode)
{
	return get_fuse_conn_super(inode->i_sb);
}

static inline struct fuse_conn *get_fuse_conn_kobj(struct kobject *obj)
{
	return container_of(obj, struct fuse_conn, kobj);
}

static inline struct fuse_inode *get_fuse_inode(struct inode *inode)
{
	return container_of(inode, struct fuse_inode, inode);
}

static inline u64 get_node_id(struct inode *inode)
{
	return get_fuse_inode(inode)->nodeid;
}

/** Device operations */
extern struct file_operations fuse_dev_operations;

/**
 * This is the single global spinlock which protects FUSE's structures
 *
 * The following data is protected by this lock:
 *
 *  - the private_data field of the device file
 *  - the s_fs_info field of the super block
 *  - unused_list, pending, processing lists in fuse_conn
 *  - background list in fuse_conn
 *  - the unique request ID counter reqctr in fuse_conn
 *  - the sb (super_block) field in fuse_conn
 *  - the file (device file) field in fuse_conn
 */
extern spinlock_t fuse_lock;

/**
 * Get a filled in inode
 */
struct inode *fuse_iget(struct super_block *sb, unsigned long nodeid,
			int generation, struct fuse_attr *attr);

/**
 * Send FORGET command
 */
void fuse_send_forget(struct fuse_conn *fc, struct fuse_req *req,
		      unsigned long nodeid, u64 nlookup);

/**
 * Initialize READ or READDIR request
 */
void fuse_read_fill(struct fuse_req *req, struct file *file,
		    struct inode *inode, loff_t pos, size_t count, int opcode);

/**
 * Send OPEN or OPENDIR request
 */
int fuse_open_common(struct inode *inode, struct file *file, int isdir);

struct fuse_file *fuse_file_alloc(void);
void fuse_file_free(struct fuse_file *ff);
void fuse_finish_open(struct inode *inode, struct file *file,
		      struct fuse_file *ff, struct fuse_open_out *outarg);

/**
 * Send a RELEASE request
 */
void fuse_send_release(struct fuse_conn *fc, struct fuse_file *ff,
		       u64 nodeid, struct inode *inode, int flags, int isdir);

/**
 * Send RELEASE or RELEASEDIR request
 */
int fuse_release_common(struct inode *inode, struct file *file, int isdir);

/**
 * Send FSYNC or FSYNCDIR request
 */
int fuse_fsync_common(struct file *file, struct dentry *de, int datasync,
		      int isdir);

/**
 * Initialize file operations on a regular file
 */
void fuse_init_file_inode(struct inode *inode);

/**
 * Initialize inode operations on regular files and special files
 */
void fuse_init_common(struct inode *inode);

/**
 * Initialize inode and file operations on a directory
 */
void fuse_init_dir(struct inode *inode);

/**
 * Initialize inode operations on a symlink
 */
void fuse_init_symlink(struct inode *inode);

/**
 * Change attributes of an inode
 */
void fuse_change_attributes(struct inode *inode, struct fuse_attr *attr);

/**
 * Initialize the client device
 */
int fuse_dev_init(void);

/**
 * Cleanup the client device
 */
void fuse_dev_cleanup(void);

/**
 * Allocate a request
 */
struct fuse_req *fuse_request_alloc(void);

/**
 * Free a request
 */
void fuse_request_free(struct fuse_req *req);

/**
 * Reinitialize a request, the preallocated flag is left unmodified
 */
void fuse_reset_request(struct fuse_req *req);

/**
 * Reserve a preallocated request
 */
struct fuse_req *fuse_get_request(struct fuse_conn *fc);

/**
 * Decrement reference count of a request.  If count goes to zero put
 * on unused list (preallocated) or free request (not preallocated).
 */
void fuse_put_request(struct fuse_conn *fc, struct fuse_req *req);

/**
 * Send a request (synchronous)
 */
void request_send(struct fuse_conn *fc, struct fuse_req *req);

/**
 * Send a request with no reply
 */
void request_send_noreply(struct fuse_conn *fc, struct fuse_req *req);

/**
 * Send a request in the background
 */
void request_send_background(struct fuse_conn *fc, struct fuse_req *req);

/**
 * Release inodes and file associated with background request
 */
void fuse_release_background(struct fuse_req *req);

/* Abort all requests */
void fuse_abort_conn(struct fuse_conn *fc);

/**
 * Get the attributes of a file
 */
int fuse_do_getattr(struct inode *inode);

/**
 * Invalidate inode attributes
 */
void fuse_invalidate_attr(struct inode *inode);
