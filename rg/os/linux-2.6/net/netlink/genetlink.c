/*
 * NETLINK      Generic Netlink Family
 *
 * 		Authors:	Jamal Hadi Salim
 * 				Thomas Graf <tgraf@suug.ch>
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/genetlink.h>

struct sock *genl_sock = NULL;

static DECLARE_MUTEX(genl_sem); /* serialization of message processing */

static void genl_lock(void)
{
	down(&genl_sem);
}

static int genl_trylock(void)
{
	return down_trylock(&genl_sem);
}

static void genl_unlock(void)
{
	up(&genl_sem);

	if (genl_sock && genl_sock->sk_receive_queue.qlen)
		genl_sock->sk_data_ready(genl_sock, 0);
}

#define GENL_FAM_TAB_SIZE	16
#define GENL_FAM_TAB_MASK	(GENL_FAM_TAB_SIZE - 1)

static struct list_head family_ht[GENL_FAM_TAB_SIZE];

static int genl_ctrl_event(int event, void *data);

static inline unsigned int genl_family_hash(unsigned int id)
{
	return id & GENL_FAM_TAB_MASK;
}

static inline struct list_head *genl_family_chain(unsigned int id)
{
	return &family_ht[genl_family_hash(id)];
}

static struct genl_family *genl_family_find_byid(unsigned int id)
{
	struct genl_family *f;

	list_for_each_entry(f, genl_family_chain(id), family_list)
		if (f->id == id)
			return f;

	return NULL;
}

static struct genl_family *genl_family_find_byname(char *name)
{
	struct genl_family *f;
	int i;

	for (i = 0; i < GENL_FAM_TAB_SIZE; i++)
		list_for_each_entry(f, genl_family_chain(i), family_list)
			if (strcmp(f->name, name) == 0)
				return f;

	return NULL;
}

static struct genl_ops *genl_get_cmd(u8 cmd, struct genl_family *family)
{
	struct genl_ops *ops;

	list_for_each_entry(ops, &family->ops_list, ops_list)
		if (ops->cmd == cmd)
			return ops;

	return NULL;
}

/* Of course we are going to have problems once we hit
 * 2^16 alive types, but that can only happen by year 2K
*/
static inline u16 genl_generate_id(void)
{
	static u16 id_gen_idx;
	int overflowed = 0;

	do {
		if (id_gen_idx == 0)
			id_gen_idx = GENL_MIN_ID;

		if (++id_gen_idx > GENL_MAX_ID) {
			if (!overflowed) {
				overflowed = 1;
				id_gen_idx = 0;
				continue;
			} else
				return 0;
		}

	} while (genl_family_find_byid(id_gen_idx));

	return id_gen_idx;
}

/**
 * genl_register_ops - register generic netlink operations
 * @family: generic netlink family
 * @ops: operations to be registered
 *
 * Registers the specified operations and assigns them to the specified
 * family. Either a doit or dumpit callback must be specified or the
 * operation will fail. Only one operation structure per command
 * identifier may be registered.
 *
 * See include/net/genetlink.h for more documenation on the operations
 * structure.
 *
 * Returns 0 on success or a negative error code.
 */
int genl_register_ops(struct genl_family *family, struct genl_ops *ops)
{
	int err = -EINVAL;

	if (ops->dumpit == NULL && ops->doit == NULL)
		goto errout;

	if (genl_get_cmd(ops->cmd, family)) {
		err = -EEXIST;
		goto errout;
	}

	genl_lock();
	list_add_tail(&ops->ops_list, &family->ops_list);
	genl_unlock();

	genl_ctrl_event(CTRL_CMD_NEWOPS, ops);
	err = 0;
errout:
	return err;
}

/**
 * genl_unregister_ops - unregister generic netlink operations
 * @family: generic netlink family
 * @ops: operations to be unregistered
 *
 * Unregisters the specified operations and unassigns them from the
 * specified family. The operation blocks until the current message
 * processing has finished and doesn't start again until the
 * unregister process has finished.
 *
 * Note: It is not necessary to unregister all operations before
 *       unregistering the family, unregistering the family will cause
 *       all assigned operations to be unregistered automatically.
 *
 * Returns 0 on success or a negative error code.
 */
int genl_unregister_ops(struct genl_family *family, struct genl_ops *ops)
{
	struct genl_ops *rc;

	genl_lock();
	list_for_each_entry(rc, &family->ops_list, ops_list) {
		if (rc == ops) {
			list_del(&ops->ops_list);
			genl_unlock();
			genl_ctrl_event(CTRL_CMD_DELOPS, ops);
			return 0;
		}
	}
	genl_unlock();

	return -ENOENT;
}

/**
 * genl_register_family - register a generic netlink family
 * @family: generic netlink family
 *
 * Registers the specified family after validating it first. Only one
 * family may be registered with the same family name or identifier.
 * The family id may equal GENL_ID_GENERATE causing an unique id to
 * be automatically generated and assigned.
 *
 * Return 0 on success or a negative error code.
 */
int genl_register_family(struct genl_family *family)
{
	int err = -EINVAL;

	if (family->id && family->id < GENL_MIN_ID)
		goto errout;

	if (family->id > GENL_MAX_ID)
		goto errout;

	INIT_LIST_HEAD(&family->ops_list);

	genl_lock();

	if (genl_family_find_byname(family->name)) {
		err = -EEXIST;
		goto errout_locked;
	}

	if (genl_family_find_byid(family->id)) {
		err = -EEXIST;
		goto errout_locked;
	}

	if (family->id == GENL_ID_GENERATE) {
		u16 newid = genl_generate_id();

		if (!newid) {
			err = -ENOMEM;
			goto errout_locked;
		}

		family->id = newid;
	}

	if (family->maxattr) {
		family->attrbuf = kmalloc((family->maxattr+1) *
					sizeof(struct nlattr *), GFP_KERNEL);
		if (family->attrbuf == NULL) {
			err = -ENOMEM;
			goto errout_locked;
		}
	} else
		family->attrbuf = NULL;

	list_add_tail(&family->family_list, genl_family_chain(family->id));
	genl_unlock();

	genl_ctrl_event(CTRL_CMD_NEWFAMILY, family);

	return 0;

errout_locked:
	genl_unlock();
errout:
	return err;
}

/**
 * genl_unregister_family - unregister generic netlink family
 * @family: generic netlink family
 *
 * Unregisters the specified family.
 *
 * Returns 0 on success or a negative error code.
 */
int genl_unregister_family(struct genl_family *family)
{
	struct genl_family *rc;

	genl_lock();

	list_for_each_entry(rc, genl_family_chain(family->id), family_list) {
		if (family->id != rc->id || strcmp(rc->name, family->name))
			continue;

		list_del(&rc->family_list);
		INIT_LIST_HEAD(&family->ops_list);
		genl_unlock();

		kfree(family->attrbuf);
		genl_ctrl_event(CTRL_CMD_DELFAMILY, family);
		return 0;
	}

	genl_unlock();

	return -ENOENT;
}

static int genl_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh,
			       int *errp)
{
	struct genl_ops *ops;
	struct genl_family *family;
	struct genl_info info;
	struct genlmsghdr *hdr = nlmsg_data(nlh);
	int hdrlen, err = -EINVAL;

	if (!(nlh->nlmsg_flags & NLM_F_REQUEST))
		goto ignore;

	if (nlh->nlmsg_type < NLMSG_MIN_TYPE)
		goto ignore;

       	family = genl_family_find_byid(nlh->nlmsg_type);
	if (family == NULL) {
		err = -ENOENT;
		goto errout;
	}

	hdrlen = GENL_HDRLEN + family->hdrsize;
	if (nlh->nlmsg_len < nlmsg_msg_size(hdrlen))
		goto errout;

	ops = genl_get_cmd(hdr->cmd, family);
	if (ops == NULL) {
		err = -EOPNOTSUPP;
		goto errout;
	}

	if ((ops->flags & GENL_ADMIN_PERM) && security_netlink_recv(skb)) {
		err = -EPERM;
		goto errout;
	}

	if (nlh->nlmsg_flags & NLM_F_DUMP) {
		if (ops->dumpit == NULL) {
			err = -EOPNOTSUPP;
			goto errout;
		}

		*errp = err = netlink_dump_start(genl_sock, skb, nlh,
						 ops->dumpit, NULL);
		if (err == 0)
			skb_pull(skb, min(NLMSG_ALIGN(nlh->nlmsg_len),
					  skb->len));
		return -1;
	}

	if (ops->doit == NULL) {
		err = -EOPNOTSUPP;
		goto errout;
	}

	if (family->attrbuf) {
		err = nlmsg_parse(nlh, hdrlen, family->attrbuf, family->maxattr,
				  ops->policy);
		if (err < 0)
			goto errout;
	}

	info.snd_seq = nlh->nlmsg_seq;
	info.snd_pid = NETLINK_CB(skb).pid;
	info.nlhdr = nlh;
	info.genlhdr = nlmsg_data(nlh);
	info.userhdr = nlmsg_data(nlh) + GENL_HDRLEN;
	info.attrs = family->attrbuf;

	*errp = err = ops->doit(skb, &info);
	return err;

ignore:
	return 0;

errout:
	*errp = err;
	return -1;
}

static void genl_rcv(struct sock *sk, int len)
{
	unsigned int qlen = 0;

	do {
		if (genl_trylock())
			return;
		netlink_run_queue(sk, &qlen, genl_rcv_msg);
		genl_unlock();
	} while (qlen && genl_sock && genl_sock->sk_receive_queue.qlen);
}

/**************************************************************************
 * Controller
 **************************************************************************/

static int ctrl_fill_info(struct genl_family *family, u32 pid, u32 seq,
			  u32 flags, struct sk_buff *skb, u8 cmd)
{
	void *hdr;

	hdr = genlmsg_put(skb, pid, seq, GENL_ID_CTRL, 0, flags, cmd,
			  family->version);
	if (hdr == NULL)
		return -1;

	NLA_PUT_STRING(skb, CTRL_ATTR_FAMILY_NAME, family->name);
	NLA_PUT_U16(skb, CTRL_ATTR_FAMILY_ID, family->id);

	return genlmsg_end(skb, hdr);

nla_put_failure:
	return genlmsg_cancel(skb, hdr);
}

static int ctrl_dumpfamily(struct sk_buff *skb, struct netlink_callback *cb)
{

	int i, n = 0;
	struct genl_family *rt;
	int chains_to_skip = cb->args[0];
	int fams_to_skip = cb->args[1];

	for (i = 0; i < GENL_FAM_TAB_SIZE; i++) {
		if (i < chains_to_skip)
			continue;
		n = 0;
		list_for_each_entry(rt, genl_family_chain(i), family_list) {
			if (++n < fams_to_skip)
				continue;
			if (ctrl_fill_info(rt, NETLINK_CB(cb->skb).pid,
					   cb->nlh->nlmsg_seq, NLM_F_MULTI,
					   skb, CTRL_CMD_NEWFAMILY) < 0)
				goto errout;
		}

		fams_to_skip = 0;
	}

errout:
	cb->args[0] = i;
	cb->args[1] = n;

	return skb->len;
}

static struct sk_buff *ctrl_build_msg(struct genl_family *family, u32 pid,
				      int seq, u8 cmd)
{
	struct sk_buff *skb;
	int err;

	skb = nlmsg_new(NLMSG_GOODSIZE);
	if (skb == NULL)
		return ERR_PTR(-ENOBUFS);

	err = ctrl_fill_info(family, pid, seq, 0, skb, cmd);
	if (err < 0) {
		nlmsg_free(skb);
		return ERR_PTR(err);
	}

	return skb;
}

static struct nla_policy ctrl_policy[CTRL_ATTR_MAX+1] __read_mostly = {
	[CTRL_ATTR_FAMILY_ID]	= { .type = NLA_U16 },
	[CTRL_ATTR_FAMILY_NAME]	= { .type = NLA_STRING },
};

static int ctrl_getfamily(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	struct genl_family *res = NULL;
	int err = -EINVAL;

	if (info->attrs[CTRL_ATTR_FAMILY_ID]) {
		u16 id = nla_get_u16(info->attrs[CTRL_ATTR_FAMILY_ID]);
		res = genl_family_find_byid(id);
	}

	if (info->attrs[CTRL_ATTR_FAMILY_NAME]) {
		char name[GENL_NAMSIZ];

		if (nla_strlcpy(name, info->attrs[CTRL_ATTR_FAMILY_NAME],
				GENL_NAMSIZ) >= GENL_NAMSIZ)
			goto errout;

		res = genl_family_find_byname(name);
	}

	if (res == NULL) {
		err = -ENOENT;
		goto errout;
	}

	msg = ctrl_build_msg(res, info->snd_pid, info->snd_seq,
			     CTRL_CMD_NEWFAMILY);
	if (IS_ERR(msg)) {
		err = PTR_ERR(msg);
		goto errout;
	}

	err = genlmsg_unicast(msg, info->snd_pid);
errout:
	return err;
}

static int genl_ctrl_event(int event, void *data)
{
	struct sk_buff *msg;

	if (genl_sock == NULL)
		return 0;

	switch (event) {
	case CTRL_CMD_NEWFAMILY:
	case CTRL_CMD_DELFAMILY:
		msg = ctrl_build_msg(data, 0, 0, event);
		if (IS_ERR(msg))
			return PTR_ERR(msg);

		genlmsg_multicast(msg, 0, GENL_ID_CTRL);
		break;
	}

	return 0;
}

static struct genl_ops genl_ctrl_ops = {
	.cmd		= CTRL_CMD_GETFAMILY,
	.doit		= ctrl_getfamily,
	.dumpit		= ctrl_dumpfamily,
	.policy		= ctrl_policy,
};

static struct genl_family genl_ctrl = {
	.id = GENL_ID_CTRL,
	.name = "nlctrl",
	.version = 0x1,
	.maxattr = CTRL_ATTR_MAX,
};

static int __init genl_init(void)
{
	int i, err;

	for (i = 0; i < GENL_FAM_TAB_SIZE; i++)
		INIT_LIST_HEAD(&family_ht[i]);

	err = genl_register_family(&genl_ctrl);
	if (err < 0)
		goto errout;

	err = genl_register_ops(&genl_ctrl, &genl_ctrl_ops);
	if (err < 0)
		goto errout_register;

	netlink_set_nonroot(NETLINK_GENERIC, NL_NONROOT_RECV);
	genl_sock = netlink_kernel_create(NETLINK_GENERIC, GENL_MAX_ID,
					  genl_rcv, THIS_MODULE);
	if (genl_sock == NULL)
		panic("GENL: Cannot initialize generic netlink\n");

	return 0;

errout_register:
	genl_unregister_family(&genl_ctrl);
errout:
	panic("GENL: Cannot register controller: %d\n", err);
}

subsys_initcall(genl_init);

EXPORT_SYMBOL(genl_sock);
EXPORT_SYMBOL(genl_register_ops);
EXPORT_SYMBOL(genl_unregister_ops);
EXPORT_SYMBOL(genl_register_family);
EXPORT_SYMBOL(genl_unregister_family);
