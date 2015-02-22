/*
 *  linux/fs/nfs/nfs3proc.c
 *
 *  Client-side NFSv3 procedures stubs.
 *
 *  Copyright (C) 1997, Olaf Kirch
 */

#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs3.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>

#define NFSDBG_FACILITY		NFSDBG_PROC

/* A wrapper to handle the EJUKEBOX error message */
static int
nfs3_rpc_wrapper(struct rpc_clnt *clnt, struct rpc_message *msg, int flags)
{
	sigset_t oldset;
	int res;
	rpc_clnt_sigmask(clnt, &oldset);
	do {
		res = rpc_call_sync(clnt, msg, flags);
		if (res != -EJUKEBOX)
			break;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(NFS_JUKEBOX_RETRY_TIME);
		res = -ERESTARTSYS;
	} while (!signalled());
	rpc_clnt_sigunmask(clnt, &oldset);
	return res;
}

static inline int
nfs3_rpc_call_wrapper(struct rpc_clnt *clnt, u32 proc, void *argp, void *resp, int flags)
{
	struct rpc_message msg = { proc, argp, resp, NULL };
	return nfs3_rpc_wrapper(clnt, &msg, flags);
}

#define rpc_call(clnt, proc, argp, resp, flags) \
		nfs3_rpc_call_wrapper(clnt, proc, argp, resp, flags)
#define rpc_call_sync(clnt, msg, flags) \
		nfs3_rpc_wrapper(clnt, msg, flags)

/*
 * Bare-bones access to getattr: this is for nfs_read_super.
 */
static int
nfs3_proc_get_root(struct nfs_server *server, struct nfs_fh *fhandle,
		   struct nfs_fattr *fattr)
{
	int	status;

	dprintk("NFS call  getroot\n");
	fattr->valid = 0;
	status = rpc_call(server->client, NFS3PROC_GETATTR, fhandle, fattr, 0);
	dprintk("NFS reply getroot\n");
	return status;
}

/*
 * One function for each procedure in the NFS protocol.
 */
static int
nfs3_proc_getattr(struct inode *inode, struct nfs_fattr *fattr)
{
	int	status;

	dprintk("NFS call  getattr\n");
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(inode), NFS3PROC_GETATTR,
			  NFS_FH(inode), fattr, 0);
	dprintk("NFS reply getattr\n");
	return status;
}

static int
nfs3_proc_setattr(struct inode *inode, struct nfs_fattr *fattr,
			struct iattr *sattr)
{
	struct nfs3_sattrargs	arg = { NFS_FH(inode), sattr, 0, 0 };
	int	status;

	dprintk("NFS call  setattr\n");
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(inode), NFS3PROC_SETATTR, &arg, fattr, 0);
	dprintk("NFS reply setattr\n");
	return status;
}

static int
nfs3_proc_lookup(struct inode *dir, struct qstr *name,
		 struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_diropargs	arg = { NFS_FH(dir), name->name, name->len };
	struct nfs3_diropres	res = { &dir_attr, fhandle, fattr };
	int			status;

	dprintk("NFS call  lookup %s\n", name->name);
	dir_attr.valid = 0;
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(dir), NFS3PROC_LOOKUP, &arg, &res, 0);
	if (status >= 0 && !(fattr->valid & NFS_ATTR_FATTR))
		status = rpc_call(NFS_CLIENT(dir), NFS3PROC_GETATTR,
			 fhandle, fattr, 0);
	dprintk("NFS reply lookup: %d\n", status);
	if (status >= 0)
		status = nfs_refresh_inode(dir, &dir_attr);
	return status;
}

static int
nfs3_proc_access(struct inode *inode, struct rpc_cred *cred, int mode)
{
	struct nfs_fattr	fattr;
	struct nfs3_accessargs	arg = { NFS_FH(inode), 0 };
	struct nfs3_accessres	res = { &fattr, 0 };
	struct rpc_message msg = { NFS3PROC_ACCESS, &arg, &res, cred };
	int	status;

	dprintk("NFS call  access\n");
	fattr.valid = 0;

	if (mode & MAY_READ)
		arg.access |= NFS3_ACCESS_READ;
	if (S_ISDIR(inode->i_mode)) {
		if (mode & MAY_WRITE)
			arg.access |= NFS3_ACCESS_MODIFY | NFS3_ACCESS_EXTEND | NFS3_ACCESS_DELETE;
		if (mode & MAY_EXEC)
			arg.access |= NFS3_ACCESS_LOOKUP;
	} else {
		if (mode & MAY_WRITE)
			arg.access |= NFS3_ACCESS_MODIFY | NFS3_ACCESS_EXTEND;
		if (mode & MAY_EXEC)
			arg.access |= NFS3_ACCESS_EXECUTE;
	}
	status = rpc_call_sync(NFS_CLIENT(inode), &msg, 0);
	nfs_refresh_inode(inode, &fattr);
	dprintk("NFS reply access\n");

	if (status == 0 && (arg.access & res.access) != arg.access)
		status = -EACCES;
	return status;
}

static int
nfs3_proc_readlink(struct inode *inode, struct page *page)
{
	struct nfs_fattr	fattr;
	struct nfs3_readlinkargs args = { NFS_FH(inode), PAGE_CACHE_SIZE, &page };
	int			status;

	dprintk("NFS call  readlink\n");
	fattr.valid = 0;
	status = rpc_call(NFS_CLIENT(inode), NFS3PROC_READLINK,
			&args, &fattr, 0);
	nfs_refresh_inode(inode, &fattr);
	dprintk("NFS reply readlink: %d\n", status);
	return status;
}

static int
nfs3_proc_read(struct inode *inode, struct rpc_cred *cred,
	       struct nfs_fattr *fattr, int flags,
	       unsigned int base, unsigned int count, struct page *page,
	       int *eofp)
{
	u64			offset = page_offset(page) + base;
	struct nfs_readargs	arg = { NFS_FH(inode), offset, count,
					base, &page };
	struct nfs_readres	res = { fattr, count, 0 };
	struct rpc_message	msg = { NFS3PROC_READ, &arg, &res, cred };
	int			status;

	dprintk("NFS call  read %d @ %Ld\n", count, (long long)offset);
	fattr->valid = 0;
	status = rpc_call_sync(NFS_CLIENT(inode), &msg, flags);
	dprintk("NFS reply read: %d\n", status);
	*eofp = res.eof;
	return status;
}

static int
nfs3_proc_write(struct inode *inode, struct rpc_cred *cred,
		struct nfs_fattr *fattr, int flags,
		unsigned int base, unsigned int count,
		struct page *page, struct nfs_writeverf *verf)
{
	u64			offset = page_offset(page) + base;
	struct nfs_writeargs	arg = { NFS_FH(inode), offset, count,
					NFS_FILE_SYNC, base, &page };
	struct nfs_writeres	res = { fattr, verf, 0 };
	struct rpc_message	msg = { NFS3PROC_WRITE, &arg, &res, cred };
	int			status, rpcflags = 0;

	dprintk("NFS call  write %d @ %Ld\n", count, (long long)offset);
	fattr->valid = 0;
	if (flags & NFS_RW_SWAP)
		rpcflags |= NFS_RPC_SWAPFLAGS;
	arg.stable = (flags & NFS_RW_SYNC) ? NFS_FILE_SYNC : NFS_UNSTABLE;

	status = rpc_call_sync(NFS_CLIENT(inode), &msg, rpcflags);

	dprintk("NFS reply read: %d\n", status);
	return status < 0? status : res.count;
}

/*
 * Create a regular file.
 * For now, we don't implement O_EXCL.
 */
static int
nfs3_proc_create(struct inode *dir, struct qstr *name, struct iattr *sattr,
		 int flags, struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_createargs	arg = { NFS_FH(dir), name->name, name->len,
					sattr, 0, { 0, 0 } };
	struct nfs3_diropres	res = { &dir_attr, fhandle, fattr };
	int			status;

	dprintk("NFS call  create %s\n", name->name);
	arg.createmode = NFS3_CREATE_UNCHECKED;
	if (flags & O_EXCL) {
		arg.createmode  = NFS3_CREATE_EXCLUSIVE;
		arg.verifier[0] = jiffies;
		arg.verifier[1] = current->pid;
	}

again:
	dir_attr.valid = 0;
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(dir), NFS3PROC_CREATE, &arg, &res, 0);
	nfs_refresh_inode(dir, &dir_attr);

	/* If the server doesn't support the exclusive creation semantics,
	 * try again with simple 'guarded' mode. */
	if (status == NFSERR_NOTSUPP) {
		switch (arg.createmode) {
			case NFS3_CREATE_EXCLUSIVE:
				arg.createmode = NFS3_CREATE_GUARDED;
				break;

			case NFS3_CREATE_GUARDED:
				arg.createmode = NFS3_CREATE_UNCHECKED;
				break;

			case NFS3_CREATE_UNCHECKED:
				goto exit;
		}
		goto again;
	}

exit:
	dprintk("NFS reply create: %d\n", status);

	/* When we created the file with exclusive semantics, make
	 * sure we set the attributes afterwards. */
	if (status == 0 && arg.createmode == NFS3_CREATE_EXCLUSIVE) {
		struct nfs3_sattrargs	arg = { fhandle, sattr, 0, 0 };
		dprintk("NFS call  setattr (post-create)\n");

		/* Note: we could use a guarded setattr here, but I'm
		 * not sure this buys us anything (and I'd have
		 * to revamp the NFSv3 XDR code) */
		fattr->valid = 0;
		status = rpc_call(NFS_CLIENT(dir), NFS3PROC_SETATTR,
						&arg, fattr, 0);
		dprintk("NFS reply setattr (post-create): %d\n", status);
	}

	return status;
}

static int
nfs3_proc_remove(struct inode *dir, struct qstr *name)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_diropargs	arg = { NFS_FH(dir), name->name, name->len };
	struct rpc_message	msg = { NFS3PROC_REMOVE, &arg, &dir_attr, NULL };
	int			status;

	dprintk("NFS call  remove %s\n", name->name);
	dir_attr.valid = 0;
	status = rpc_call_sync(NFS_CLIENT(dir), &msg, 0);
	nfs_refresh_inode(dir, &dir_attr);
	dprintk("NFS reply remove: %d\n", status);
	return status;
}

static int
nfs3_proc_unlink_setup(struct rpc_message *msg, struct dentry *dir, struct qstr *name)
{
	struct nfs3_diropargs	*arg;
	struct nfs_fattr	*res;

	arg = (struct nfs3_diropargs *)kmalloc(sizeof(*arg)+sizeof(*res), GFP_KERNEL);
	if (!arg)
		return -ENOMEM;
	res = (struct nfs_fattr*)(arg + 1);
	arg->fh = NFS_FH(dir->d_inode);
	arg->name = name->name;
	arg->len = name->len;
	res->valid = 0;
	msg->rpc_proc = NFS3PROC_REMOVE;
	msg->rpc_argp = arg;
	msg->rpc_resp = res;
	return 0;
}

static void
nfs3_proc_unlink_done(struct dentry *dir, struct rpc_message *msg)
{
	struct nfs_fattr	*dir_attr;

	if (msg->rpc_argp) {
		dir_attr = (struct nfs_fattr*)msg->rpc_resp;
		nfs_refresh_inode(dir->d_inode, dir_attr);
		kfree(msg->rpc_argp);
	}
}

static int
nfs3_proc_rename(struct inode *old_dir, struct qstr *old_name,
		 struct inode *new_dir, struct qstr *new_name)
{
	struct nfs_fattr	old_dir_attr, new_dir_attr;
	struct nfs3_renameargs	arg = { NFS_FH(old_dir),
					old_name->name, old_name->len,
					NFS_FH(new_dir),
					new_name->name, new_name->len };
	struct nfs3_renameres	res = { &old_dir_attr, &new_dir_attr };
	int			status;

	dprintk("NFS call  rename %s -> %s\n", old_name->name, new_name->name);
	old_dir_attr.valid = 0;
	new_dir_attr.valid = 0;
	status = rpc_call(NFS_CLIENT(old_dir), NFS3PROC_RENAME, &arg, &res, 0);
	nfs_refresh_inode(old_dir, &old_dir_attr);
	nfs_refresh_inode(new_dir, &new_dir_attr);
	dprintk("NFS reply rename: %d\n", status);
	return status;
}

static int
nfs3_proc_link(struct inode *inode, struct inode *dir, struct qstr *name)
{
	struct nfs_fattr	dir_attr, fattr;
	struct nfs3_linkargs	arg = { NFS_FH(inode), NFS_FH(dir),
					name->name, name->len };
	struct nfs3_linkres	res = { &dir_attr, &fattr };
	int			status;

	dprintk("NFS call  link %s\n", name->name);
	dir_attr.valid = 0;
	fattr.valid = 0;
	status = rpc_call(NFS_CLIENT(inode), NFS3PROC_LINK, &arg, &res, 0);
	nfs_refresh_inode(dir, &dir_attr);
	nfs_refresh_inode(inode, &fattr);
	dprintk("NFS reply link: %d\n", status);
	return status;
}

static int
nfs3_proc_symlink(struct inode *dir, struct qstr *name, struct qstr *path,
		  struct iattr *sattr, struct nfs_fh *fhandle,
		  struct nfs_fattr *fattr)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_symlinkargs	arg = { NFS_FH(dir), name->name, name->len,
					path->name, path->len, sattr };
	struct nfs3_diropres	res = { &dir_attr, fhandle, fattr };
	int			status;

	dprintk("NFS call  symlink %s -> %s\n", name->name, path->name);
	dir_attr.valid = 0;
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(dir), NFS3PROC_SYMLINK, &arg, &res, 0);
	nfs_refresh_inode(dir, &dir_attr);
	dprintk("NFS reply symlink: %d\n", status);
	return status;
}

static int
nfs3_proc_mkdir(struct inode *dir, struct qstr *name, struct iattr *sattr,
		struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_mkdirargs	arg = { NFS_FH(dir), name->name, name->len,
					sattr };
	struct nfs3_diropres	res = { &dir_attr, fhandle, fattr };
	int			status;

	dprintk("NFS call  mkdir %s\n", name->name);
	dir_attr.valid = 0;
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(dir), NFS3PROC_MKDIR, &arg, &res, 0);
	nfs_refresh_inode(dir, &dir_attr);
	dprintk("NFS reply mkdir: %d\n", status);
	return status;
}

static int
nfs3_proc_rmdir(struct inode *dir, struct qstr *name)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_diropargs	arg = { NFS_FH(dir), name->name, name->len };
	int			status;

	dprintk("NFS call  rmdir %s\n", name->name);
	dir_attr.valid = 0;
	status = rpc_call(NFS_CLIENT(dir), NFS3PROC_RMDIR, &arg, &dir_attr, 0);
	nfs_refresh_inode(dir, &dir_attr);
	dprintk("NFS reply rmdir: %d\n", status);
	return status;
}

/*
 * The READDIR implementation is somewhat hackish - we pass the user buffer
 * to the encode function, which installs it in the receive iovec.
 * The decode function itself doesn't perform any decoding, it just makes
 * sure the reply is syntactically correct.
 *
 * Also note that this implementation handles both plain readdir and
 * readdirplus.
 */
static int
nfs3_proc_readdir(struct inode *dir, struct rpc_cred *cred,
		  u64 cookie, struct page *page, unsigned int count, int plus)
{
	struct nfs_fattr	dir_attr;
	u32			*verf = NFS_COOKIEVERF(dir);
	struct nfs3_readdirargs	arg = { NFS_FH(dir), cookie, {verf[0], verf[1]},
	       				plus, count, &page };
	struct nfs3_readdirres	res = { &dir_attr, verf, plus };
	struct rpc_message	msg = { NFS3PROC_READDIR, &arg, &res, cred };
	int			status;

	if (plus)
		msg.rpc_proc = NFS3PROC_READDIRPLUS;

	dprintk("NFS call  readdir%s %d\n",
			plus? "plus" : "", (unsigned int) cookie);

	dir_attr.valid = 0;
	status = rpc_call_sync(NFS_CLIENT(dir), &msg, 0);
	nfs_refresh_inode(dir, &dir_attr);
	dprintk("NFS reply readdir: %d\n", status);
	return status;
}

static int
nfs3_proc_mknod(struct inode *dir, struct qstr *name, struct iattr *sattr,
		dev_t rdev, struct nfs_fh *fh, struct nfs_fattr *fattr)
{
	struct nfs_fattr	dir_attr;
	struct nfs3_mknodargs	arg = { NFS_FH(dir), name->name, name->len, 0,
					sattr, rdev };
	struct nfs3_diropres	res = { &dir_attr, fh, fattr };
	int			status;

	switch (sattr->ia_mode & S_IFMT) {
	case S_IFBLK:	arg.type = NF3BLK;  break;
	case S_IFCHR:	arg.type = NF3CHR;  break;
	case S_IFIFO:	arg.type = NF3FIFO; break;
	case S_IFSOCK:	arg.type = NF3SOCK; break;
	default:	return -EINVAL;
	}

	dprintk("NFS call  mknod %s %x\n", name->name, rdev);
	dir_attr.valid = 0;
	fattr->valid = 0;
	status = rpc_call(NFS_CLIENT(dir), NFS3PROC_MKNOD, &arg, &res, 0);
	nfs_refresh_inode(dir, &dir_attr);
	dprintk("NFS reply mknod: %d\n", status);
	return status;
}

/*
 * This is a combo call of fsstat and fsinfo
 */
static int
nfs3_proc_statfs(struct nfs_server *server, struct nfs_fh *fhandle,
		 struct nfs_fsinfo *info)
{
	int	status;

	dprintk("NFS call  fsstat\n");
	memset((char *)info, 0, sizeof(*info));
	status = rpc_call(server->client, NFS3PROC_FSSTAT, fhandle, info, 0);
	if (status < 0)
		goto error;
	status = rpc_call(server->client, NFS3PROC_FSINFO, fhandle, info, 0);

error:
	dprintk("NFS reply statfs: %d\n", status);
	return status;
}

#ifdef CONFIG_NFS_ACL
static int
nfs3_proc_checkacls(struct inode *inode)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs_fattr fattr;
	struct nfs3_getaclargs args;
	struct nfs3_getaclres res = { &fattr };
	int status;

	if (!(server->flags & NFS_SOLARIS_GETACL) ||
	    (server->flags & NFS_MOUNT_NOACL))
		return -EOPNOTSUPP;

	args.fh = NFS_FH(inode);
	args.mask = NFS3_ACLCNT | NFS3_DFACLCNT;

	dprintk("NFS call getacl\n");
	status = rpc_call(NFS_ACL_CLIENT(inode), NFS3_ACL_PROC_GETACL,
			  &args, &res, 0);
	dprintk("NFS reply getacl: %d\n", status);

	if (status) {
		if (status == -ENOSYS) {
			dprintk("NFS_ACL GETACL RPC not supported "
				"(will not retry)\n");
			server->flags &= ~NFS_SOLARIS_GETACL;
			status = -EOPNOTSUPP;
		} else if (status == -ENOTSUPP)
			status = -EOPNOTSUPP;
		goto getout;
	}
	if ((args.mask & res.mask) != args.mask) {
		status = -EIO;
		goto getout;
	}

	status = nfs_refresh_inode(inode, &fattr);

getout:
	posix_acl_release(res.acl_access);
	posix_acl_release(res.acl_default);

	if (!status) {
		/* The (count > 4) test will exclude ACL entries from the list
		   of names even if their ACL_GROUP_ENTRY and ACL_MASK have
		   different permissions. Getacl still returns these as
		   four-entry ACLs, instead of minimal (three-entry) ACLs. */
		   
		if ((args.mask & NFS3_ACLCNT) && res.acl_access_count > 4)
			status |= ACL_TYPE_ACCESS;
		if ((args.mask & NFS3_DFACLCNT) && res.acl_default_count > 0)
			status |= ACL_TYPE_DEFAULT;
	}
	return status;
}
#endif  /* CONFIG_NFS_ACL */

#ifdef CONFIG_NFS_ACL
static struct posix_acl *
nfs3_proc_getacl(struct inode *inode, int type)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs_fattr fattr;
	struct nfs3_getaclargs args;
	struct nfs3_getaclres res = { &fattr };
	struct posix_acl *acl = NULL;
	int status;

	if (!(server->flags & NFS_SOLARIS_GETACL) ||
	    (server->flags & NFS_MOUNT_NOACL))
		return ERR_PTR(-EOPNOTSUPP);

	args.fh = NFS_FH(inode);
	switch(type) {
		case ACL_TYPE_ACCESS:
			args.mask = NFS3_ACLCNT|NFS3_ACL;
			break;

		case ACL_TYPE_DEFAULT:
			args.mask = NFS3_DFACLCNT|NFS3_DFACL;
			break;

		default:
			return ERR_PTR(-EINVAL);
	}

	dprintk("NFS call getacl\n");
	status = rpc_call(NFS_ACL_CLIENT(inode), NFS3_ACL_PROC_GETACL,
			  &args, &res, 0);
	dprintk("NFS reply getacl: %d\n", status);

	if (status) {
		if (status == -ENOSYS) {
			dprintk("NFS_ACL GETACL RPC not supported "
				"(will not retry)\n");
			server->flags &= ~NFS_SOLARIS_GETACL;
			status = -EOPNOTSUPP;
		} else if (status == -ENOTSUPP)
			status = -EOPNOTSUPP;
		goto getout;
	}
	if ((args.mask & res.mask) != args.mask) {
		status = -EIO;
		goto getout;
	}

	if (type == ACL_TYPE_ACCESS) {
		if (res.acl_access) {
			mode_t mode = inode->i_mode;
			if (!posix_acl_equiv_mode(res.acl_access, &mode) &&
			    inode->i_mode == mode) {
				posix_acl_release(res.acl_access);
				res.acl_access = NULL;
			}
		}
		acl = res.acl_access;
		res.acl_access = NULL;
	} else {
		acl = res.acl_default;
		res.acl_default = NULL;
	}

	status = nfs_refresh_inode(inode, &fattr);

getout:
	posix_acl_release(res.acl_access);
	posix_acl_release(res.acl_default);

	if (status) {
		posix_acl_release(acl);
		acl = ERR_PTR(status);
	}
	return acl;
}
#endif  /* CONFIG_NFS_ACL */

#ifdef CONFIG_NFS_ACL
static int
nfs3_proc_setacl(struct inode *inode, int type, struct posix_acl *acl)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs_fattr fattr;
	struct nfs3_setaclargs args = { };
	int status;

	if (!(server->flags & NFS_SOLARIS_SETACL) ||
	    (server->flags & NFS_MOUNT_NOACL))
		return -EOPNOTSUPP;

	/* We are doing this here, because XDR marshalling can only
	   return -ENOMEM. */
	if (acl && acl->a_count > NFS3_ACL_MAX_ENTRIES)
		return -EINVAL;
	args.inode = inode;
	args.mask = NFS3_ACL|NFS3_DFACL;
	if (S_ISDIR(inode->i_mode)) {
		switch(type) {
			case ACL_TYPE_ACCESS:
				args.acl_access = acl;
				args.acl_default = NFS_PROTO(inode)->getacl(
					inode, ACL_TYPE_DEFAULT);
				status = PTR_ERR(args.acl_default);
				if (IS_ERR(args.acl_default)) {
					args.acl_default = NULL;
					goto cleanup;
				}
				break;

			case ACL_TYPE_DEFAULT:
				args.acl_access = NFS_PROTO(inode)->getacl(
					inode, ACL_TYPE_ACCESS);
				status = PTR_ERR(args.acl_access);
				if (IS_ERR(args.acl_access)) {
					args.acl_access = NULL;
					goto cleanup;
				}
				args.acl_default = acl;
				break;

			default:
				status = -EINVAL;
				goto cleanup;
		}
	} else {
		status = -EINVAL;
		if (type != ACL_TYPE_ACCESS)
			goto cleanup;
		args.mask = NFS3_ACL;
		args.acl_access = acl;
	}
	if (args.acl_access == NULL) {
		args.acl_access = posix_acl_from_mode(inode->i_mode,
						      GFP_KERNEL);
		status = PTR_ERR(args.acl_access);
		if (IS_ERR(args.acl_access)) {
			args.acl_access = NULL;
			goto cleanup;
		}
	}
	args.mask = NFS3_ACL | (args.acl_default ? NFS3_DFACL : 0);

	dprintk("NFS call setacl\n");
	status = rpc_call(NFS_ACL_CLIENT(inode), NFS3_ACL_PROC_SETACL,
			  &args, &fattr, 0);
	dprintk("NFS reply setacl: %d\n", status);

	if (status) {
		if (status == -ENOSYS) {
			dprintk("NFS_ACL SETACL RPC not supported"
				"(will not retry)\n");
			server->flags &= ~NFS_SOLARIS_SETACL;
			status = -EOPNOTSUPP;
		} else if (status == -ENOTSUPP)
			status = -EOPNOTSUPP;
	} else {
		/* Force an attribute cache update if the file mode
		 * has changed. */
		if (inode->i_mode != fattr.mode)
			NFS_CACHEINV(inode);
		status = nfs_refresh_inode(inode, &fattr);
	}

cleanup:
	if (args.acl_access != acl)
		posix_acl_release(args.acl_access);
	if (args.acl_default != acl)
		posix_acl_release(args.acl_default);
	return status;
}
#endif  /* CONFIG_NFS_ACL */

extern u32 *nfs3_decode_dirent(u32 *, struct nfs_entry *, int);

struct nfs_rpc_ops	nfs_v3_clientops = {
	3,			/* protocol version */
	nfs3_proc_get_root,
	nfs3_proc_getattr,
	nfs3_proc_setattr,
	nfs3_proc_lookup,
	nfs3_proc_access,
	nfs3_proc_readlink,
	nfs3_proc_read,
	nfs3_proc_write,
	NULL,			/* commit */
	nfs3_proc_create,
	nfs3_proc_remove,
	nfs3_proc_unlink_setup,
	nfs3_proc_unlink_done,
	nfs3_proc_rename,
	nfs3_proc_link,
	nfs3_proc_symlink,
	nfs3_proc_mkdir,
	nfs3_proc_rmdir,
	nfs3_proc_readdir,
	nfs3_proc_mknod,
	nfs3_proc_statfs,
	nfs3_decode_dirent,
#ifdef CONFIG_NFS_ACL
	nfs3_proc_getacl,
	nfs3_proc_setacl,
	nfs3_proc_checkacls,
#endif  /* CONFIG_NFS_ACL */
};
