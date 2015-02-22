/*
 * q_wrr.c		WRR.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Martin Devera  <devik@cdi.cz>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... wrr\n");
}

static void explain2(void)
{
	fprintf(stderr, "Usage: ... wrr quantum q\n");
}

#define usage() return(-1)

static int wrr_parse_opt(struct qdisc_util *qu, int argc, char **argv,
    struct nlmsghdr *n)
{
	struct tc_sfq_qopt opt;

	memset(&opt, 0, sizeof(opt));

	while (argc > 0) {
		/*
		if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_size(&opt.quantum, *argv)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "perturb") == 0) {
			NEXT_ARG();
			if (get_integer(&opt.perturb_period, *argv, 0)) {
				fprintf(stderr, "Illegal \"perturb\"\n");
				return -1;
			}
			ok++;
		} else*/ if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt));
	return 0;
}

static int wrr_parse_class_opt(struct qdisc_util *qu, int argc, char **argv,
    struct nlmsghdr *n)
{
	int ok=0; long qm=1500;

	while (argc > 0) {
		if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_size((unsigned *)&qm, *argv)) {
				fprintf(stderr, "Illegal \"quantum\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "help") == 0) {
			explain2();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain2();
			return -1;
		}
		argc--; argv++;
	}

	if (ok)
		addattr_l(n, 1024, TCA_OPTIONS, &qm, sizeof(qm));
	return 0;
}

static int wrr_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	long *qm;
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	if (RTA_PAYLOAD(opt)  < sizeof(qm))
		return -1;
	qm = RTA_DATA(opt);
	fprintf(f, "quantum %s ", sprint_size(*qm, b1));
/*	if (show_details) {
		fprintf(f, "limit %up flows %u/%u ",
			qopt->limit, qopt->flows, qopt->divisor);
	}
	if (qopt->perturb_period)
		fprintf(f, "perturb %dsec ", qopt->perturb_period);
		*/
	return 0;
}

static int wrr_print_xstats(struct qdisc_util *qu, FILE *f,
    struct rtattr *xstats)
{
	return 0;
}


struct qdisc_util wrr_qdisc_util = {
	NULL,
	"wrr",
	wrr_parse_opt,
	wrr_print_opt,
	wrr_print_xstats,
	wrr_parse_class_opt,
	wrr_print_opt,
};
