/* Shared library add-on to iptables to add TRACE target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#define INIT_FUNC libxt_TRACE_init
#include "xtables.h"
#include "x_tables.h"

static struct xtables_target trace_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "TRACE",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(0),
	.userspacesize	= XT_ALIGN(0),
};

void INIT_FUNC(void)
{
	xtables_register_target(&trace_target);
}
