/*
 * IPSEC interface configuration
 * Copyright (C) 1996  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* system(), strtoul() */
#include <unistd.h> /* getuid() */
#include <linux/types.h>
#include <sys/ioctl.h> /* ioctl() */

#include <freeswan.h>
#ifdef NET_21 /* from freeswan.h */
#include <linux/sockios.h>
#include <sys/socket.h>
#endif /* NET_21 */ /* from freeswan.h */

#if 0
#include <linux/if.h>
#else
#include <net/if.h>
#endif
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include "ipsec_tunnel.h"

static void
usage(char *name)
{	
	fprintf(stdout,"%s --attach --virtual <virtual-device> --physical <physical-device>\n",
		name);
	fprintf(stdout,"%s --detach --virtual <virtual-device>\n",
		name);
	fprintf(stdout,"%s --clear\n",
		name);
	fprintf(stdout,"%s --help\n",
		name);
	fprintf(stdout,"%s --version\n",
		name);
	fprintf(stdout,"%s\n",
		name);
	fprintf(stdout, "        [ --debug ] is optional to any %s command.\n", name);
	fprintf(stdout, "        [ --label <label> ] is optional to any %s command.\n", name);
	exit(1);
}

static struct option const longopts[] =
{
	{"virtual", 1, 0, 'V'},
	{"physical", 1, 0, 'P'},
	{"attach", 0, 0, 'a'},
	{"detach", 0, 0, 'd'},
	{"clear", 0, 0, 'c'},
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'v'},
	{"label", 1, 0, 'l'},
	{"optionsfrom", 1, 0, '+'},
	{"debug", 0, 0, 'g'},
	{0, 0, 0, 0}
};

int
main(int argc, char *argv[])
{
	struct ifreq ifr;
	struct ipsectunnelconf *shc=(struct ipsectunnelconf *)&ifr.ifr_data;
	int s;
	int c, previous = -1;
	char *program_name;
	int debug = 0;
	int argcount = argc;
	char me[] = "ipsec tncfg";
     
	memset(&ifr, 0, sizeof(ifr));
	program_name = argv[0];

	while((c = getopt_long_only(argc, argv, ""/*"adchvV:P:l:+:"*/, longopts, 0)) != EOF) {
		switch(c) {
		case 'g':
			debug = 1;
			argcount--;
			break;
		case 'a':
			if(shc->cf_cmd) {
				fprintf(stderr, "%s: exactly one of '--attach', '--detach' or '--clear' options must be specified.\n",	program_name);
				exit(1);
			}
			shc->cf_cmd = IPSEC_SET_DEV;
			break;
		case 'd':
			if(shc->cf_cmd) {
				fprintf(stderr, "%s: exactly one of '--attach', '--detach' or '--clear' options must be specified.\n",	program_name);
				exit(1);
			}
			shc->cf_cmd = IPSEC_DEL_DEV;
			break;
		case 'c':
			if(shc->cf_cmd) {
				fprintf(stderr, "%s: exactly one of '--attach', '--detach' or '--clear' options must be specified.\n",	program_name);
				exit(1);
			}
			shc->cf_cmd = IPSEC_CLR_DEV;
			break;
		case 'h':
			usage(program_name);
			break;
		case 'v':
			if(optarg) {
				fprintf(stderr, "%s: warning; '-v' and '--version' options don't expect arguments, arg '%s' found, perhaps unintended.\n",
					program_name, optarg);
			}
			fprintf(stdout, "%s %s\n", me, ipsec_version_code());
			fprintf(stdout, "See `ipsec --copyright' for copyright information.\n");
			exit(1);
			break;
		case 'V':
			strcpy(ifr.ifr_name, optarg);
			break;
		case 'P':
			strcpy(shc->cf_name, optarg);
			break;
		case 'l':
			program_name = malloc(strlen(argv[0])
					      + 10 /* update this when changing the sprintf() */
					      + strlen(optarg));
			sprintf(program_name, "%s --label %s",
				argv[0],
				optarg);
			argcount -= 2;
			break;
		case '+': /* optionsfrom */
			optionsfrom(optarg, &argc, &argv, optind, stderr);
			/* no return on error */
			break;
		default:
			usage(program_name);
			break;
		}
		previous = c;
	}

	if(argcount == 1) {
		system("cat /proc/net/ipsec_tncfg");
		exit(0);
	}

	switch(shc->cf_cmd) {
	case IPSEC_SET_DEV:
		if(!shc->cf_name) {
			fprintf(stderr, "%s: physical I/F parameter missing.\n",
				program_name);
			exit(1);
		}
	case IPSEC_DEL_DEV:
		if(!ifr.ifr_name) {
			fprintf(stderr, "%s: virtual I/F parameter missing.\n",
				program_name);
			exit(1);
		}
		break;
	case IPSEC_CLR_DEV:
		strcpy(ifr.ifr_name, "ipsec0");
		break;
	default:
		fprintf(stderr, "%s: exactly one of '--attach', '--detach' or '--clear' options must be specified.\n"
			"Try %s --help' for usage information.\n",
			program_name, program_name);
		exit(1);
	}

	s=socket(AF_INET, SOCK_DGRAM,0);
	if(s==-1)
	{
		fprintf(stderr, "%s: Socket creation failed -- ", program_name);
		switch(errno)
		{
		case EACCES:
			if(getuid()==0)
				fprintf(stderr, "Root denied permission!?!\n");
			else
				fprintf(stderr, "Run as root user.\n");
			break;
		case EPROTONOSUPPORT:
			fprintf(stderr, "Internet Protocol not enabled");
			break;
		case EMFILE:
		case ENFILE:
		case ENOBUFS:
			fprintf(stderr, "Insufficient system resources.\n");
			break;
		case ENODEV:
			fprintf(stderr, "No such device.  Is the virtual device valid?  Is the ipsec module linked into the kernel or loaded as a module?\n");
			break;
		default:
			fprintf(stderr, "Unknown socket error %d.\n", errno);
		}
		exit(1);
	}
	if(ioctl(s, shc->cf_cmd, &ifr)==-1)
	{
		if(shc->cf_cmd == IPSEC_SET_DEV) {
			fprintf(stderr, "%s: Socket ioctl failed on attach -- ", program_name);
			switch(errno)
			{
			case EINVAL:
				fprintf(stderr, "Invalid argument, check kernel log messages for specifics.\n");
				break;
			case ENODEV:
				fprintf(stderr, "No such device.  Is the virtual device valid?  Is the ipsec module linked into the kernel or loaded as a module?\n");
				break;
			case ENXIO:
				fprintf(stderr, "No such device.  Is the physical device valid?\n");
				break;
			case EBUSY:
				fprintf(stderr, "Device busy.  Virtual device %s is already attached to a physical device -- Use detach first.\n",
				       ifr.ifr_name);
				break;
			default:
				fprintf(stderr, "Unknown socket error %d.\n", errno);
			}
			exit(1);
		}
		if(shc->cf_cmd == IPSEC_DEL_DEV) {
			fprintf(stderr, "%s: Socket ioctl failed on detach -- ", program_name);
			switch(errno)
			{
			case EINVAL:
				fprintf(stderr, "Invalid argument, check kernel log messages for specifics.\n");
				break;
			case ENODEV:
				fprintf(stderr, "No such device.  Is the virtual device valid?  The ipsec module may not be linked into the kernel or loaded as a module.\n");
				break;
			case ENXIO:
				fprintf(stderr, "Device requested is not linked to any physical device.\n");
				break;
			default:
				fprintf(stderr, "Unknown socket error %d.\n", errno);
			}
			exit(1);
		}
		if(shc->cf_cmd == IPSEC_CLR_DEV) {
			fprintf(stderr, "%s: Socket ioctl failed on clear -- ", program_name);
			switch(errno)
			{
			case EINVAL:
				fprintf(stderr, "Invalid argument, check kernel log messages for specifics.\n");
				break;
			case ENODEV:
				fprintf(stderr, "Failed.  Is the ipsec module linked into the kernel or loaded as a module?.\n");
				break;
			default:
				fprintf(stderr, "Unknown socket error %d.\n", errno);
			}
			exit(1);
		}
	}
	exit(0);
}

