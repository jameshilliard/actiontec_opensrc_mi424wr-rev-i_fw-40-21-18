/*
 * control KLIPS debugging options
 * Copyright (C) 1996  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs <rgb@freeswan.org>
 *                                 2001  Michael Richardson <mcr@freeswan.org>
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

#include <sys/types.h>
#include <linux/types.h> /* new */
#include <string.h>
#include <errno.h>
#include <stdlib.h> /* system(), strtoul() */
#include <sys/stat.h> /* open() */
#include <fcntl.h> /* open() */

#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>



#include <unistd.h>
#include <freeswan.h>
#if 0
#include <linux/autoconf.h>	/* CONFIG_IPSEC_PFKEYv2 */
#endif
/* permanently turn it on since netlink support has been disabled */
     #include <signal.h>
     #include <pfkeyv2.h>
     #include <pfkey.h>
#include "radij.h"
#include "ipsec_encap.h"
#ifndef CONFIG_IPSEC_DEBUG
#define CONFIG_IPSEC_DEBUG
#endif /* CONFIG_IPSEC_DEBUG */
#include "ipsec_netlink.h"
#include "ipsec_tunnel.h"

#include <stdio.h>
#include <getopt.h>

__u32 bigbuf[1024];
char *program_name;
char me[] = "ipsec klipsdebug";
extern unsigned int pfkey_lib_debug; /* used by libfreeswan/pfkey_v2_build */
int pfkey_sock;
fd_set pfkey_socks;
uint32_t pfkey_seq = 0;

static void
usage(char * arg)
{
	fprintf(stdout, "usage: %s {--set|--clear} {tunnel|tunnel-xmit|netlink|xform|eroute|spi|radij|esp|ah|rcv|pfkey|ipcomp|verbose|reject|log-all}\n", arg);
	fprintf(stdout, "       %s {--all|--none}\n", arg);
	fprintf(stdout, "       %s --help\n", arg);
	fprintf(stdout, "       %s --version\n", arg);
	fprintf(stdout, "       %s\n", arg);
	fprintf(stdout, "        [ --debug ] is optional to any %s command\n", arg);
	fprintf(stdout, "        [ --label <label> ] is optional to any %s command.\n", arg);
	exit(1);
}

static struct option const longopts[] =
{
	{"set", 1, 0, 's'},
	{"clear", 1, 0, 'c'},
	{"all", 0, 0, 'a'},
	{"none", 0, 0, 'n'},
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'v'},
	{"label", 1, 0, 'l'},
	{"optionsfrom", 1, 0, '+'},
	{"debug", 0, 0, 'd'},
	{0, 0, 0, 0}
};

int
main(int argc, char **argv)
{
/*	int fd; */
	unsigned char action = 0;
	unsigned int i;
	int *bitfieldp;
	int c, previous = -1;
	
	struct encap_msghdr *em = (struct encap_msghdr *)bigbuf;
	int debug = 0;
	int error = 0;
	int argcount = argc;

	struct sadb_ext *extensions[SADB_EXT_MAX + 1];
	struct sadb_msg *pfkey_msg;
	
	bitfieldp = (int *)&em->em_db_tn;

	program_name = argv[0];

	while((c = getopt_long(argc, argv, ""/*"s:c:anhvl:+:d"*/, longopts, 0)) != EOF) {
		switch(c) {
		case 'd':
			debug = 1;
			pfkey_lib_debug = 1;
			argcount--;
			break;
		case 's':
			if(action) {
				fprintf(stderr, "%s: Only one of '--set', '--clear', '--all' or '--none' options permitted.\n",
					program_name);
				exit(1);
			}
			action = 's';
			for (i = 0; i < (sizeof(em->Eu.Dbg)/sizeof(int)); i++) {
				*(bitfieldp++) = 0;
			}
			if(strcmp(optarg, "tunnel") == 0) {
				em->em_db_tn = -1L;
			} else if(strcmp(optarg, "tunnel-xmit") == 0) {
				em->em_db_tn = DB_TN_XMIT;
			} else if(strcmp(optarg, "netlink") == 0) {
				em->em_db_nl = -1L;
			} else if(strcmp(optarg, "xform") == 0) {
				em->em_db_xf = -1L;
			} else if(strcmp(optarg, "eroute") == 0) {
				em->em_db_er = -1L;
			} else if(strcmp(optarg, "spi") == 0) {
				em->em_db_sp = -1L;
			} else if(strcmp(optarg, "radij") == 0) {
				em->em_db_rj = -1L;
			} else if(strcmp(optarg, "esp") == 0) {
				em->em_db_es = -1L;
			} else if(strcmp(optarg, "ah") == 0) {
				em->em_db_ah = -1L;
			} else if(strcmp(optarg, "rcv") == 0) {
				em->em_db_rx = -1L;
			} else if(strcmp(optarg, "pfkey") == 0) {
				em->em_db_ky = -1L;
			} else if(strcmp(optarg, "ipcomp") == 0) {
				em->em_db_gz = -1L;
			} else if(strcmp(optarg, "verbose") == 0) {
				em->em_db_vb = -1L;
			} else if(strcmp(optarg, "reject") == 0) {
				em->em_db_rt = -1L;
			} else if(strcmp(optarg, "log-all") == 0) {
				em->em_db_la = -1L;
			} else {
				usage(program_name);
			}
			em->em_db_nl |= 1 << (sizeof(em->em_db_nl) * 8 -1);
			break;
		case 'c':
			if(action) {
				fprintf(stderr, "%s: Only one of '--set', '--clear', '--all' or '--none' options permitted.\n",
					program_name);
				exit(1);
			}
			action = 'c';
			for (i = 0; i < (sizeof(em->Eu.Dbg)/sizeof(int)); i++) {
				*(bitfieldp++) = -1;
			}
			if(strcmp(optarg, "tunnel") == 0) {
				em->em_db_tn = 0;
			} else if(strcmp(optarg, "tunnel-xmit") == 0) {
				em->em_db_tn = ~DB_TN_XMIT;
			} else if(strcmp(optarg, "netlink") == 0) {
				em->em_db_nl = 0;
			} else if(strcmp(optarg, "xform") == 0) {
				em->em_db_xf = 0;
			} else if(strcmp(optarg, "eroute") == 0) {
				em->em_db_er = 0;
			} else if(strcmp(optarg, "spi") == 0) {
				em->em_db_sp = 0;
			} else if(strcmp(optarg, "radij") == 0) {
				em->em_db_rj = 0;
			} else if(strcmp(optarg, "esp") == 0) {
				em->em_db_es = 0;
			} else if(strcmp(optarg, "ah") == 0) {
				em->em_db_ah = 0;
			} else if(strcmp(optarg, "rcv") == 0) {
				em->em_db_rx = 0;
			} else if(strcmp(optarg, "pfkey") == 0) {
				em->em_db_ky = 0;
			} else if(strcmp(optarg, "ipcomp") == 0) {
				em->em_db_gz = 0;
			} else if(strcmp(optarg, "verbose") == 0) {
				em->em_db_vb = 0;
			} else if(strcmp(optarg, "reject") == 0) {
				em->em_db_rt = 0;
			} else if(strcmp(optarg, "log-all") == 0) {
				em->em_db_la = 0;
			} else {
				usage(program_name);
			}
			em->em_db_nl &= ~(1 << (sizeof(em->em_db_nl) * 8 -1));
			break;
		case 'a':
			if(action) {
				fprintf(stderr, "%s: Only one of '--set', '--clear', '--all' or '--none' options permitted.\n",
					program_name);
				exit(1);
			}
			action = 'a';
			for (i = 0; i < (sizeof(em->Eu.Dbg)/sizeof(int)); i++) {
				*(bitfieldp++) = -1;
			}
			*(bitfieldp++) = 0;
			break;
		case 'n':
			if(action) {
				fprintf(stderr, "%s: Only one of '--set', '--clear', '--all' or '--none' options permitted.\n",
					program_name);
				exit(1);
			}
			action = 'n';
			for (i = 0; i < (sizeof(em->Eu.Dbg)/sizeof(int)); i++) {
				*(bitfieldp++) = 0;
			}
			break;
		case 'h':
		case '?':
			usage(program_name);
			exit(1);
		case 'v':
			fprintf(stdout, "%s %s\n", me, ipsec_version_code());
			fprintf(stdout, "See `ipsec --copyright' for copyright information.\n");
			exit(0);
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
			break;
		}
		previous = c;
	}

	if(argcount == 1) {
		system("cat /proc/net/ipsec_klipsdebug");
		exit(0);
	}

 	em->em_magic = EM_MAGIC;
	em->em_version = 0;
	if(action) {
		em->em_type = EMT_SETDEBUG;
	} else {
#if 0
		em->em_type = EMT_GETDEBUG;
#else
		usage(program_name);
#endif
	}
	em->em_msglen = EMT_SETDEBUG_FLEN;

	if((pfkey_sock = socket(PF_KEY, SOCK_RAW, PF_KEY_V2) ) < 0) {
		fprintf(stderr, "%s: Trouble openning PF_KEY family socket with error: ",
			program_name);
		switch(errno) {
		case ENOENT:
			fprintf(stderr, "device does not exist.  See FreeS/WAN installation procedure.\n");
			break;
		case EACCES:
			fprintf(stderr, "access denied.  ");
			if(getuid() == 0) {
				fprintf(stderr, "Check permissions.  Should be 600.\n");
			} else {
				fprintf(stderr, "You must be root to open this file.\n");
			}
			break;
		case EUNATCH:
			fprintf(stderr, "Netlink not enabled OR KLIPS not loaded.\n");
			break;
		case ENODEV:
			fprintf(stderr, "KLIPS not loaded or enabled.\n");
			break;
		case EBUSY:
			fprintf(stderr, "KLIPS is busy.  Most likely a serious internal error occured in a previous command.  Please report as much detail as possible to development team.\n");
			break;
		case EINVAL:
			fprintf(stderr, "Invalid argument, KLIPS not loaded or check kernel log messages for specifics.\n");
			break;
		case ENOBUFS:
			fprintf(stderr, "No kernel memory to allocate SA.\n");
			break;
		case ESOCKTNOSUPPORT:
			fprintf(stderr, "Algorithm support not available in the kernel.  Please compile in support.\n");
			break;
		case EEXIST:
			fprintf(stderr, "SA already in use.  Delete old one first.\n");
			break;
		case ENXIO:
			fprintf(stderr, "SA does not exist.  Cannot delete.\n");
			break;
		case EAFNOSUPPORT:
			fprintf(stderr, "KLIPS not loaded or enabled.\n");
			break;
		default:
			fprintf(stderr, "Unknown file open error %d.  Please report as much detail as possible to development team.\n", errno);
		}
		exit(1);
	}

	pfkey_extensions_init(extensions);

	if((error = pfkey_msg_hdr_build(&extensions[0],
					SADB_X_DEBUG,
					0,
					0,
					++pfkey_seq,
					getpid()))) {
		fprintf(stderr, "%s: Trouble building message header, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		exit(1);
	}
	
	if((error = pfkey_x_debug_build(&extensions[SADB_X_EXT_DEBUG],
					em->em_db_tn,
					em->em_db_nl,
					em->em_db_xf,
					em->em_db_er,
					em->em_db_sp,
					em->em_db_rj,
					em->em_db_es,
					em->em_db_ah,
					em->em_db_rx,
					em->em_db_ky,
					em->em_db_gz,
					em->em_db_vb,
					em->em_db_rt,
					em->em_db_la))) {
		fprintf(stderr, "%s: Trouble building message header, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		exit(1);
	}
	
	if((error = pfkey_msg_build(&pfkey_msg, extensions, EXT_BITS_IN))) {
		fprintf(stderr, "%s: Trouble building pfkey message, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
		exit(1);
	}
	
	if((error = write(pfkey_sock,
			  pfkey_msg,
			  pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN)) !=
	   pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		fprintf(stderr, "%s: pfkey write failed, tried to write %d octets, returning %d with errno=%d.\n",
			program_name, pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN, error, errno);
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
		switch(errno) {
		case EACCES:
			fprintf(stderr, "access denied.  ");
			if(getuid() == 0) {
				fprintf(stderr, "Check permissions.  Should be 600.\n");
			} else {
				fprintf(stderr, "You must be root to open this file.\n");
			}
			break;
		case EUNATCH:
			fprintf(stderr, "Netlink not enabled OR KLIPS not loaded.\n");
			break;
		case EBUSY:
			fprintf(stderr, "KLIPS is busy.  Most likely a serious internal error occured in a previous command.  Please report as much detail as possible to development team.\n");
			break;
		case EINVAL:
			fprintf(stderr, "Invalid argument, check kernel log messages for specifics.\n");
			break;
		case ENODEV:
			fprintf(stderr, "KLIPS not loaded or enabled.\n");
			fprintf(stderr, "No device?!?\n");
			break;
		case ENOBUFS:
			fprintf(stderr, "No kernel memory to allocate SA.\n");
			break;
		case ESOCKTNOSUPPORT:
			fprintf(stderr, "Algorithm support not available in the kernel.  Please compile in support.\n");
			break;
		case EEXIST:
			fprintf(stderr, "SA already in use.  Delete old one first.\n");
			break;
		case ENOENT:
			fprintf(stderr, "device does not exist.  See FreeS/WAN installation procedure.\n");
			break;
		case ENXIO:
			fprintf(stderr, "SA does not exist.  Cannot delete.\n");
			break;
		default:
			fprintf(stderr, "Unknown socket write error %d.  Please report as much detail as possible to development team.\n", errno);
		}
		exit(1);
	}

	if(pfkey_msg) {
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
	}

	(void) close(pfkey_sock);  /* close the socket */
	exit(0);
}

