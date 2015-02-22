/*
 *
 * Copyright (C) 2004  MontaVista Software Inc.
 * Author: Manish Lachwani, mlachwani@mvista.com or manish@koffee-break.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _MIPS_KDEBUG_H
#define _MIPS_KDEBUG_H

#include <linux/notifier.h>

struct pt_regs;

struct die_args {
	struct pt_regs *regs;
	const char *str;
	long err;
};

int register_die_notifier(struct notifier_block *nb);
extern struct notifier_block *mips_die_chain;

enum die_val {
	DIE_OOPS = 1,
	DIE_PANIC,
	DIE_DIE,
	DIE_KERNELDEBUG,
	DIE_TRAP,
	DIE_PAGE_FAULT,
};

/*
 * trap number can be computed from regs and signr can be computed using
 * compute_signal()
 */
static inline int notify_die(enum die_val val,char *str,struct pt_regs *regs,long err)
{
	struct die_args args = { .regs=regs, .str=str, .err=err };
	return notifier_call_chain(&mips_die_chain, val, &args);
}

#endif /* _MIPS_KDEBUG_H */
