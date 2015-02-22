/*
 * Shared library add-on to iptables to add early socket matching support.
 *
 * Copyright (C) 2007 BalaBit IT Ltd.
 */
#define INIT_FUNC libxt_socket_init
#include "xtables.h"

static struct xtables_match socket_mt_reg = {
	.name	       = "socket",
	.version       = XTABLES_VERSION,
	.family	       = NFPROTO_IPV4,
	.size	       = XT_ALIGN(0),
	.userspacesize = XT_ALIGN(0),
};

void INIT_FUNC(void)
{
	xtables_register_match(&socket_mt_reg);
}
