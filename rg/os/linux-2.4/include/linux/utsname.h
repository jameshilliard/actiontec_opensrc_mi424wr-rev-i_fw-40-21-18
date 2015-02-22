#ifndef _LINUX_UTSNAME_H
#define _LINUX_UTSNAME_H

#define __OLD_UTS_LEN 8

struct oldold_utsname {
	char sysname[9];
	char nodename[9];
	char release[9];
	char version[9];
	char machine[9];
};

#define __NEW_UTS_LEN 64

struct old_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
};

struct new_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

extern struct new_utsname system_utsname;

extern struct rw_semaphore uts_sem;

#ifdef CONFIG_IPV6_NODEINFO
extern void (*icmpv6_sethostname_hook)(struct new_utsname *);
extern struct rw_semaphore icmpv6_sethostname_hook_sem;
#endif

#endif
