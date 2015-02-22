/*
 * All-in-one program to set Security Association parameters
 * Copyright (C) 1996  John Ioannidis.
 * Copyright (C) 1997, 1998, 1999, 2000, 2001  Richard Guy Briggs.
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

#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
/* #include <linux/netdevice.h> */
#include <net/if.h>
/* #include <linux/types.h> */ /* new */
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

/* #include <sys/socket.h> */

#include <netinet/in.h>
#include <arpa/inet.h>
/* #include <linux/ip.h> */
#include <netdb.h>

#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <freeswan.h>
#include <signal.h>
#include <sys/socket.h>
#include <pfkeyv2.h>
#include <pfkey.h>
#include "ipsec_xform.h"

/* 	
 * 	Manual conn support for ipsec_alg (modular algos).
 * 	Rather ugly to include from pluto dir but avoids
 * 	code duplication.
 */
#ifndef NO_KERNEL_ALG
#include <alg_info.h>
#include <constants.h>
struct connection;
#include <kernel_alg.h>
#endif /* NO_KERNEL_ALG */

char *program_name;
char me[] = "ipsec spi";
int debug = 0;
char *command;
extern char *optarg;
extern int optind, opterr, optopt;
char scratch[2];
char *iv = NULL, *enckey = NULL, *authkey = NULL;
size_t ivlen = 0, enckeylen = 0, authkeylen = 0;
ip_address edst, dst, src;
int address_family = 0;
unsigned char proto = 0;
int alg = 0;

#ifndef NO_KERNEL_ALG
/* 
 * 	Manual connection support for modular algos (ipsec_alg) --Juanjo.
 */
#define XF_OTHER_ALG (XF_CLR-1)	/* define magic XF_ symbol for alg_info's */
#include <assert.h>
const char *alg_string = NULL;	/* algorithm string */
struct alg_info_esp *alg_info = NULL;	/* algorithm info got from string */
struct esp_info *esp_info = NULL;	/* esp info from 1st (only) element */
const char *alg_err;		/* auxiliar for parsing errors */
int proc_read_ok = 0;		/* /proc/net/pf_key_support read ok */
#endif /* NO_KERNEL_ALG */

int replay_window = 0;
char sa[SATOT_BUF];

extern unsigned int pfkey_lib_debug; /* used by libfreeswan/pfkey_v2_build */
int pfkey_sock;
fd_set pfkey_socks;
uint32_t pfkey_seq = 0;
enum life_severity {
	life_soft = 0,
	life_hard = 1,
	life_maxsever = 2
};
enum life_type {
	life_alloc = 0,
	life_bytes = 1,
	life_addtime = 2,
	life_usetime = 3,
	life_packets = 4,
	life_maxtype = 5
};

#define streql(_a,_b) (!strcmp((_a),(_b)))

static const char *usage_string =
"Usage:\n"
"	in the following, <SA> is: --af <inet | inet6> --edst <dstaddr> --spi <spi> --proto <proto>\n"
"                               OR: --said <proto><.|:><spi>@<dstaddr>\n"
"	                  <life> is: --life <soft|hard>-<allocations|bytes|addtime|usetime|packets>=<value>[,...]\n"
"spi --clear\n"
"spi --help\n"
"spi --version\n"
"spi\n"
"spi --del <SA>\n"
"spi --ip4 <SA> --src <encap-src> --dst <encap-dst>\n"
"spi --ip6 <SA> --src <encap-src> --dst <encap-dst>\n"
"spi --ah <algo> <SA> [<life> ][ --replay_window <replay_window> ] --authkey <key>\n"
"	where <algo> is one of:	hmac-md5-96 | hmac-sha1-96\n"
"spi --esp <algo> <SA> [<life> ][ --replay_window <replay-window> ] --enckey <ekey> --authkey <akey>\n"
"	where <algo> is one of:	3des-md5-96 | 3des-sha1-96\n"
"spi --esp <algo> <SA> [<life> ][ --replay_window <replay-window> ] --enckey <ekey>\n"
"	where <algo> is:	3des\n"
"spi --comp <algo> <SA>\n"
"	where <algo> is:	deflate\n"
"[ --debug ] is optional to any spi command.\n"
"[ --label <label> ] is optional to any spi command.\n";


static void
usage(char *s, FILE *f)
{
	/* s argument is actually ignored, at present */
	fprintf(f, "%s:%s", s, usage_string);
	exit(-1);
}


#ifdef SIGS_FROM_BELOW
static void
pfkey_sig_handler(int x)
{
	int len;
	int error = 0;
#if 0
	int i;
	fd_set l_pfkey_socks;
#endif
	unsigned char buffer[PFKEYv2_MAX_MSGSIZE];
	struct sadb_ext *extensions_parse[SADB_EXT_MAX + 1];
	struct sadb_msg *pfkey_msg;
	
	/* if(signal.type == SIGIO) } */
	while(1) {
#if 0
		memcpy(&l_pfkey_socks, &pfkey_socks, sizeof(pfkey_socks));
		
		fprintf(stdout, "%s:pfkey_sig_handler: "
			"entering select for pfkey_sock=%d.\n",
			program_name,
			pfkey_sock);
		if((i = select(pfkey_sock + 1, &l_pfkey_socks, NULL, NULL, NULL)) < 0) {
			fprintf(stderr, "%s:system error:pfkey_sig_handler: "
				"select returned errno:%d.\n",
				program_name,
				errno);
			break;
		}
		if(!i) {
			fprintf(stdout, "%s:pfkey_sig_handler: "
				"select returned %d.\n",
				program_name,
				i);
			return;
		}

		ret = recvmsg(pfkey_sock,
			      /* struct msghdr * */msg,
			      /* unsigned int */flags);
		if(ret == -1) {
			fprintf(stderr, "%s: pfkey recvmsg failed.\n",
				program_name);
			switch(errno) {
			case EBADF:
			case ENOTCONN:
			case ENOTSOCK:
			case EWOULDBLOCK:
			case EINTR:
			case EFAULT:
				fprintf(stderr, "system error:%d\n",
					errno);
				exit(1);
			default:
				fprintf(stderr, "unknown error:%d\n",
					errno);
				exit(1);
			}
		}
#endif
		fprintf(stdout, "%s:pfkey_sig_handler: "
			"entering read for pfkey_sock=%d.\n",
			program_name,
			pfkey_sock);
		if((len = read(pfkey_sock, buffer, sizeof(buffer))) < 0) {
			fprintf(stderr, "%s: pfkey read failed.\n",
				program_name);
			switch(errno) {
			case EBADF:
			case ENOTCONN:
			case ENOTSOCK:
			case EWOULDBLOCK:
			case EINTR:
			case EFAULT:
				fprintf(stderr, "%s:system error:%d\n",
					program_name,
					errno);
				exit(1);
			default:
				fprintf(stderr, "%s:unknown error:%d\n",
					program_name,
					errno);
				exit(1);
			}
			break;
		}
		
		if(len < sizeof(struct sadb_msg)) {
			fprintf(stderr, "%s:system error:pfkey_sig_handler: "
				"read returned only %d octets of a minimum of %d octets for the message header.\n",
				program_name,
				len, sizeof(struct sadb_msg));
			break;
		}
		
		fprintf(stdout, "%s:pfkey_sig_handler: "
			"read %d octets from pfkey_sock=%d.\n",
			program_name,
			len, pfkey_sock);
		
		pfkey_msg = (struct sadb_msg*)buffer;

		if(pfkey_msg->sadb_msg_version != PF_KEY_V2) {
			fprintf(stderr, "system error:pfkey_sig_handler: not PF_KEY_V2 msg.\n");
			break;
		}
		
		if(len != pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
			fprintf(stderr, "system error:pfkey_sig_handler: bogus msg len of %d, not %d byte aligned.\n",
				len, IPSEC_PFKEYv2_ALIGN);
			break;
		}
		
		/* XXX when this becomes a lib, keying daemons must be able to receive errors */
		if(pfkey_msg->sadb_msg_errno) {
			fprintf(stderr, "system error:pfkey_sig_handler: errno set to %d.\n",
				pfkey_msg->sadb_msg_errno);
			break;
		}
		
		/* check PID */
		if(pfkey_msg->sadb_msg_pid != getpid()) {
			fprintf(stderr, "system error:pfkey_sig_handler: pid (%d) does not equal originating process pid (%d).\n",
				pfkey_msg->sadb_msg_pid, getpid());
			break;
		}
		
		if(pfkey_msg->sadb_msg_seq != pfkey_seq) {
			fprintf(stderr, "system error:pfkey_sig_handler: seq (%d) does not equal original message seq (%d).\n",
				pfkey_msg->sadb_msg_seq, pfkey_seq);
			break;
		}
		
		if(pfkey_msg->sadb_msg_reserved) {
			fprintf(stderr, "system error:pfkey_sig_handler: reserved field must be zero, set to %d.\n",
				pfkey_msg->sadb_msg_reserved);
			break;
		}
		
		if((pfkey_msg->sadb_msg_type > SADB_MAX) || (!pfkey_msg->sadb_msg_type)){
			fprintf(stderr, "system error:pfkey_sig_handler: msg type too large or small:%d.\n",
				pfkey_msg->sadb_msg_type);
			break;
		}
		
		if((error = pfkey_msg_parse(pfkey_msg, NULL, extensions_parse, EXT_BITS_OUT))) {
			fprintf(stderr, "system error:pfkey_sig_handler: pfkey_msg_parse returns %d.\n",
				error);
		} else {
			fprintf(stdout, "%s:pfkey_sig_handler: return (msg would normally be sent for parsing).\n",
				program_name);
		}
		break;
	}
	return;
}
#endif /* SIGS_FROM_BELOW */

int
parse_life_options(uint32_t life[life_maxsever][life_maxtype],
		   char *life_opt[life_maxsever][life_maxtype],
		   char *optarg)
{
	char *optargp = optarg;
	char *endptr;
	
	do {
		int life_severity, life_type;
		char *optargt = optargp;
		
		if(strncmp(optargp, "soft", sizeof("soft")-1) == 0) {
			life_severity = life_soft;
			optargp += sizeof("soft")-1;
		} else if(strncmp(optargp, "hard", sizeof("hard")-1) == 0) {
			life_severity = life_hard;
			optargp += sizeof("hard")-1;
		} else {
			fprintf(stderr, "%s: missing lifetime severity in %s, optargt=%p, optargp=%p, sizeof(\"soft\")=%d\n",
				program_name, optargt, optargt, optargp, sizeof("soft"));
			usage(program_name, stderr);
			return(1);
		}
		if(debug) {
			fprintf(stdout, "%s: debug: life_severity=%d, optargt=%p=\"%s\", optargp=%p=\"%s\", sizeof(\"soft\")=%d\n",
				program_name, life_severity, optargt, optargt, optargp, optargp, sizeof("soft"));
		}
		if(*(optargp++) != '-') {
			fprintf(stderr, "%s: expected '-' after severity of lifetime parameter to --life option.\n",
				program_name);
			usage(program_name, stderr);
			return(1);
		}
		if(debug) {
			fprintf(stdout, "%s: debug: optargt=%p=\"%s\", optargp=%p=\"%s\", strlen(optargt)=%d, strlen(optargp)=%d, strncmp(optargp, \"addtime\", sizeof(\"addtime\")-1)=%d\n",
				program_name, optargt, optargt, optargp, optargp, strlen(optargt), strlen(optargp), strncmp(optargp, "addtime", sizeof("addtime")-1));
		}
		if(strncmp(optargp, "allocations", sizeof("allocations")-1) == 0) {
			life_type = life_alloc;
			optargp += sizeof("allocations")-1;
		} else if(strncmp(optargp, "bytes", sizeof("bytes")-1) == 0) {
			life_type = life_bytes;
			optargp += sizeof("bytes")-1;
		} else if(strncmp(optargp, "addtime", sizeof("addtime")-1) == 0) {
			life_type = life_addtime;
			optargp += sizeof("addtime")-1;
		} else if(strncmp(optargp, "usetime", sizeof("usetime")-1) == 0) {
			life_type = life_usetime;
			optargp += sizeof("usetime")-1;
		} else if(strncmp(optargp, "packets", sizeof("packets")-1) == 0) {
			life_type = life_packets;
			optargp += sizeof("packets")-1;
		} else {
			fprintf(stderr, "%s: missing lifetime type after '-' in %s\n",
				program_name, optargt);
			usage(program_name, stderr);
			return(1);
		}
		if(debug) {
			fprintf(stdout, "%s: debug: life_type=%d\n",
				program_name, life_type);
		}
		if(life_opt[life_severity][life_type] != NULL) {
			fprintf(stderr, "%s: Error, lifetime parameter redefined:%s, already defined as:%p\n",
				program_name, optargt, life_opt[life_severity][life_type]);
			return(1);
		}
		if(*(optargp++) != '=') {
			fprintf(stderr, "%s: expected '=' after type of lifetime parameter to --life option.\n",
				program_name);
			usage(program_name, stderr);
			return(1);
		}
		if(debug) {
			fprintf(stdout, "%s: debug: optargt=%p, optargt+strlen(optargt)=%p, optargp=%p, strlen(optargp)=%d\n",
				program_name, optargt, optargt+strlen(optargt), optargp, strlen(optargp));
		}
		if(strlen(optargp) == 0) {
			fprintf(stderr, "%s: expected value after '=' in --life option. optargt=%p, optargt+strlen(optargt)=%p, optargp=%p\n",
				program_name, optargt, optargt+strlen(optargt), optargp);
			usage(program_name, stderr);
			return(1);
		}
		life[life_severity][life_type] = strtoul(optargp, &endptr, 0);

		if(!((endptr == optargp + strlen(optargp)) || (endptr == optargp + strcspn(optargp, ", ")))) {
			fprintf(stderr, "%s: Invalid character='%c' at offset %d in lifetime option parameter: '%s', parameter string is %d characters long, %d valid value characters found.\n",
				program_name,
				*endptr, endptr - optarg, optarg, strlen(optarg), strcspn(optargp, ", ") - 1);
			return(1);
		}
		life_opt[life_severity][life_type] = optargt;
		if(debug) {
			fprintf(stdout, "%s lifetime %s set to %d.\n",
				program_name, optargt, life[life_severity][life_type]);
		}
		optargp=endptr+1;
	} while(*endptr==',' || isspace(*endptr));
	
	return(0);
}

int
pfkey_register(uint8_t satype) {
	/* for registering SA types that can be negotiated */
	int error = 0;
	struct sadb_ext *extensions[SADB_EXT_MAX + 1];
	struct sadb_msg *pfkey_msg;

	pfkey_extensions_init(extensions);
	if((error = pfkey_msg_hdr_build(&extensions[0],
					SADB_REGISTER,
					satype,
					0,
					++pfkey_seq,
					getpid()))) {
		fprintf(stderr, "%s: Trouble building message header, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		return(1);
	}
	if((error = pfkey_msg_build(&pfkey_msg, extensions, EXT_BITS_IN))) {
		fprintf(stderr, "%s: Trouble building pfkey message, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
		return(1);
	}
	if(write(pfkey_sock, pfkey_msg,
		 pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) !=
	   pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		/* cleanup code here */
		fprintf(stderr, "%s: Trouble writing to channel PF_KEY.\n", program_name);
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
		return(1);
	}
	pfkey_extensions_free(extensions);
	pfkey_msg_free(&pfkey_msg);
	
	return(0);
}

static struct option const longopts[] =
{
	{"ah", 1, 0, 'H'},
	{"esp", 1, 0, 'P'},
	{"comp", 1, 0, 'Z'},
	{"ip4", 0, 0, '4'},
	{"ip6", 0, 0, '6'},
	{"del", 0, 0, 'd'},

	{"authkey", 1, 0, 'A'},
	{"enckey", 1, 0, 'E'},
	{"edst", 1, 0, 'e'},
	{"spi", 1, 0, 's'},
	{"proto", 1, 0, 'p'},
	{"af", 1, 0, 'a'},
	{"replay_window", 1, 0, 'w'},
	{"iv", 1, 0, 'i'},
	{"dst", 1, 0, 'D'},
	{"src", 1, 0, 'S'},
	{"said", 1, 0, 'I'},

	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'v'},
	{"clear", 0, 0, 'c'},
	{"label", 1, 0, 'l'},
	{"debug", 0, 0, 'g'},
	{"optionsfrom", 1, 0, '+'},
	{"life", 1, 0, 'f'},
	{0, 0, 0, 0}
};

int
main(int argc, char *argv[])
{
	char *endptr;
	__u32 spi = 0;
	int c, previous = -1;
/*	int ret; */
	ip_said said;
	size_t sa_len;
	const char* error_s;
	char ipaddr_txt[ADDRTOT_BUF];
	char ipsaid_txt[SATOT_BUF];

	int error = 0;
	int argcount = argc;

	unsigned char authalg, encryptalg;
	struct sadb_ext *extensions[SADB_EXT_MAX + 1];
	struct sadb_msg *pfkey_msg;
	char *iv_opt, *akey_opt, *ekey_opt, *alg_opt, *edst_opt, *spi_opt, *proto_opt, *af_opt, *said_opt, *dst_opt, *src_opt;
#if 0
	ip_address pfkey_address_p_ska;
	ip_address pfkey_ident_s_ska;
	ip_address pfkey_ident_d_ska;
#endif
	uint32_t life[life_maxsever][life_maxtype];
	char *life_opt[life_maxsever][life_maxtype];
	
	program_name = argv[0];
	memset(&said, 0, sizeof(said));
	iv_opt = akey_opt = ekey_opt = alg_opt = edst_opt = spi_opt = proto_opt = af_opt = said_opt = dst_opt = src_opt = NULL;
	{
		int i,j;
		for(i = 0; i < life_maxsever; i++) {
			for(j = 0; j < life_maxtype; j++) {
				life_opt[i][j] = NULL;
				life[i][j] = 0;
			}
		}
	}

	while((c = getopt_long(argc, argv, ""/*"H:P:Z:46dcA:E:e:s:a:w:i:D:S:hvgl:+:f:"*/, longopts, 0)) != EOF) {
		switch(c) {
		case 'g':
			debug = 1;
			pfkey_lib_debug = 1;
			argcount--;
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
		case 'H':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear'  options permitted.\n",
					program_name);
				exit(1);
			}
			if       (!strcmp(optarg, "hmac-md5-96")) {
				alg = XF_AHHMACMD5;
			} else if(!strcmp(optarg, "hmac-sha1-96")) {
				alg = XF_AHHMACSHA1;
			} else {
				fprintf(stderr, "%s: Unknown authentication algorithm '%s' follows '--ah' option.\n",
					program_name, optarg);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case 'P':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear'  options permitted.\n",
					program_name);
				exit(1);
			}
			if (!strcmp(optarg, "des-md5-96")) {
				alg = XF_ESPDESMD596;
			} else if(!strcmp(optarg, "3des-md5-96")) {
				alg = XF_ESP3DESMD596;
			} else if(!strcmp(optarg, "null-md5-96")) {
				alg = XF_ESPNULLMD596;
			} else if(!strcmp(optarg, "null-sha1-96")) {
				alg = XF_ESPNULLSHA196;
			} else if(!strcmp(optarg, "des-sha1-96")) {
				alg = XF_ESPDESSHA196;
			} else if(!strcmp(optarg, "3des-sha1-96")) {
				alg = XF_ESP3DESSHA196;
			} else if(!strcmp(optarg, "des")) {
				alg = XF_ESPDES;
			} else if(!strcmp(optarg, "3des")) {
				alg = XF_ESP3DES;
#ifdef CONFIG_IPSEC_ENC_AES
			} else if(!strcmp(optarg, "aes128-md5-96")) {
			        alg = XF_ESPAES128MD596;
			} else if(!strcmp(optarg, "aes128-sha1-96")) {
			        alg = XF_ESPAES128SHA196;
			} else if(!strcmp(optarg, "aes192-md5-96")) {
			        alg = XF_ESPAES192MD596;
			} else if(!strcmp(optarg, "aes192-sha1-96")) {
			        alg = XF_ESPAES192SHA196;
			} else if(!strcmp(optarg, "aes256-md5-96")) {
			        alg = XF_ESPAES256MD596;
			} else if(!strcmp(optarg, "aes256-sha1-96")) {
			        alg = XF_ESPAES256SHA196;
#endif
#ifndef NO_KERNEL_ALG
			} else if((alg_info=alg_info_esp_create_from_str(optarg, &alg_err))) {
				int esp_ealg_id, esp_aalg_id;
				alg = XF_OTHER_ALG;
				if (alg_info->alg_info_cnt>1) {
					fprintf(stderr, "%s: Invalid encryption algorithm '%s' "
						"follows '--esp' option: lead too many(%d) "
						"transforms\n",
						program_name, optarg, alg_info->alg_info_cnt);
					exit(1);
				}
				alg_string=optarg;
				esp_info=&alg_info->esp[0];
				if (debug) {
					fprintf(stdout, "%s: alg_info: cnt=%d ealg[0]=%d aalg[0]=%d\n",
						program_name, 
						alg_info->alg_info_cnt,
						esp_info->encryptalg,
						esp_info->authalg);
				}
				esp_ealg_id=esp_info->esp_ealg_id;
				esp_aalg_id=esp_info->esp_aalg_id;
				if (kernel_alg_proc_read()==0) {
					proc_read_ok++;
					if (!kernel_alg_esp_enc_ok(esp_ealg_id, 0, 0))
					{
						fprintf(stderr, "%s: ESP encryptalg=%d (\"%s\") "
								"not present\n",
							program_name,
							esp_ealg_id,
							enum_name(&esp_transformid_names, esp_ealg_id));
						exit(1);
					}
					if (!kernel_alg_esp_auth_ok(esp_aalg_id, 0))
					{
						fprintf(stderr, "%s: ESP authalg=%d (\"%s\")"
								"not present\n",
							program_name,
							esp_aalg_id,
							enum_name(&auth_alg_names, esp_aalg_id));
						exit(1);
					}
				}
#endif /* NO_KERNEL_ALG */
			} else {
				fprintf(stderr, "%s: Invalid encryption algorithm '%s' follows '--esp' option.\n",
					program_name, optarg);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case 'Z':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear'  options permitted.\n",
					program_name);
				exit(1);
			}
			if       (!strcmp(optarg, "deflate")) {
				alg = XF_COMPDEFLATE;
			} else {
				fprintf(stderr, "%s: Unknown compression algorithm '%s' follows '--comp' option.\n",
					program_name, optarg);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case '4':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear' options permitted.\n",
					program_name);
				exit(1);
			}
		       	alg = XF_IP4;
			address_family = AF_INET;
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case '6':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear' options permitted.\n",
					program_name);
				exit(1);
			}
		       	alg = XF_IP6;
			address_family = AF_INET6;
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case 'd':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear'  options permitted.\n",
					program_name);
				exit(1);
			}
			alg = XF_DEL;
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case 'c':
			if(alg) {
				fprintf(stderr, "%s: Only one of '--ah', '--esp', '--comp', '--ip4', '--ip6', '--del' or '--clear'  options permitted.\n",
					program_name);
				exit(1);
			}
			alg = XF_CLR;
			if(debug) {
				fprintf(stdout, "Algorithm %d selected.\n", alg);
			}
			alg_opt = optarg;
			break;
		case 'e':
			if(said_opt) {
				fprintf(stderr, "%s: Error, EDST parameter redefined:%s, already defined in SA:%s\n",
					program_name, optarg, said_opt);
				exit (1);
			}				
			if(edst_opt) {
				fprintf(stderr, "%s: Error, EDST parameter redefined:%s, already defined as:%s\n",
					program_name, optarg, edst_opt);
				exit (1);
			}
			error_s = ttoaddr(optarg, 0, address_family, &edst);
			if(error_s != NULL) {
				if(error_s) {
					fprintf(stderr, "%s: Error, %s converting --edst argument:%s\n",
						program_name, error_s, optarg);
					exit (1);
				}
			}
			edst_opt = optarg;
			if(debug) {
				addrtot(&edst, 0, ipaddr_txt, sizeof(ipaddr_txt));
				fprintf(stdout, "edst=%s.\n", ipaddr_txt);
			}
			break;
		case 's':
			if(said_opt) {
				fprintf(stderr, "%s: Error, SPI parameter redefined:%s, already defined in SA:%s\n",
					program_name, optarg, said_opt);
				exit (1);
			}				
			if(spi_opt) {
				fprintf(stderr, "%s: Error, SPI parameter redefined:%s, already defined as:%s\n",
					program_name, optarg, spi_opt);
				exit (1);
			}				
			spi = strtoul(optarg, &endptr, 0);
			if(!(endptr == optarg + strlen(optarg))) {
				fprintf(stderr, "%s: Invalid character in SPI parameter: %s\n",
					program_name, optarg);
				exit (1);
			}
			if(spi < 0x100) {
				fprintf(stderr, "%s: Illegal reserved spi: %s => 0x%x Must be larger than 0x100.\n",
					program_name, optarg, spi);
				exit(1);
			}
			spi_opt = optarg;
			break;
		case 'p':
			if(said_opt) {
				fprintf(stderr, "%s: Error, PROTO parameter redefined:%s, already defined in SA:%s\n",
					program_name, optarg, said_opt);
				exit (1);
			}				
			if(proto_opt) {
				fprintf(stderr, "%s: Error, PROTO parameter redefined:%s, already defined as:%s\n",
					program_name, optarg, proto_opt);
				exit (1);
			}
			if(!strcmp(optarg, "ah"))
				proto = SA_AH;
			if(!strcmp(optarg, "esp"))
				proto = SA_ESP;
			if(!strcmp(optarg, "tun"))
				proto = SA_IPIP;
			if(!strcmp(optarg, "comp"))
				proto = SA_COMP;
			if(proto == 0) {
				fprintf(stderr, "%s: Invalid PROTO parameter: %s\n",
					program_name, optarg);
				exit (1);
			}
			proto_opt = optarg;
			break;
		case 'a':
			if(said_opt) {
				fprintf(stderr, "%s: Error, ADDRESS FAMILY parameter redefined:%s, already defined in SA:%s\n",
					program_name, optarg, said_opt);
				exit (1);
			}				
			if(af_opt) {
				fprintf(stderr, "%s: Error, ADDRESS FAMILY parameter redefined:%s, already defined as:%s\n",
					program_name, optarg, af_opt);
				exit (1);
			}
			if(strcmp(optarg, "inet") == 0) {
				address_family = AF_INET;
				/* currently we ensure that all addresses belong to the same address family */
				anyaddr(address_family, &dst);
				anyaddr(address_family, &edst);
				anyaddr(address_family, &src);
			}
			if(strcmp(optarg, "inet6") == 0) {
				address_family = AF_INET6;
				/* currently we ensure that all addresses belong to the same address family */
				anyaddr(address_family, &dst);
				anyaddr(address_family, &edst);
				anyaddr(address_family, &src);
			}
			if((strcmp(optarg, "inet") != 0) && (strcmp(optarg, "inet6") != 0)) {
				fprintf(stderr, "%s: Invalid ADDRESS FAMILY parameter: %s.\n",
					program_name, optarg);
				exit (1);
			}
			af_opt = optarg;
			break;
		case 'I':
			if(said_opt) {
				fprintf(stderr, "%s: Error, SAID parameter redefined:%s, already defined in SA:%s\n",
					program_name, optarg, said_opt);
				exit (1);
			}				
			if(proto_opt) {
				fprintf(stderr, "%s: Error, PROTO parameter redefined in SA:%s, already defined as:%s\n",
					program_name, optarg, proto_opt);
				exit (1);
			}
			if(edst_opt) {
				fprintf(stderr, "%s: Error, EDST parameter redefined in SA:%s, already defined as:%s\n",
					program_name, optarg, edst_opt);
				exit (1);
			}
			if(spi_opt) {
				fprintf(stderr, "%s: Error, SPI parameter redefined in SA:%s, already defined as:%s\n",
					program_name, optarg, spi_opt);
				exit (1);
			}
			error_s = ttosa(optarg, 0, &said);
			if(error_s != NULL) {
				fprintf(stderr, "%s: Error, %s converting --sa argument:%s\n",
					program_name, error_s, optarg);
				exit (1);
			}
			if(debug) {
				satot(&said, 0, ipsaid_txt, sizeof(ipsaid_txt));
				fprintf(stdout, "said=%s.\n", ipsaid_txt);
			}
			/* init the src and dst with the same address family */
			if(address_family == 0) {
				address_family = addrtypeof(&said.dst);
			} else if(address_family != addrtypeof(&said.dst)) {
				fprintf(stderr, "%s: Error, specified address family (%d) is different that of SAID: %s\n",
					program_name, address_family, optarg);
				exit (1);
			}
			anyaddr(address_family, &dst);
			anyaddr(address_family, &edst);
			anyaddr(address_family, &src);
			said_opt = optarg;
			break;
		case 'A':
			if(optarg[0] == '0') {
				switch(optarg[1]) {
				case 't':
				case 'x':
				case 's':
					break;
				default:
					fprintf(stderr, "%s: Authentication key must have a '0x', '0t' or '0s' prefix to select the format: %s\n",
						program_name, optarg);
					exit(1);
				}
			}
			authkeylen = atodata(optarg, 0, NULL, 0);
			if(!authkeylen) {
				fprintf(stderr, "%s: unknown format or syntax error in authentication key: %s\n",
					program_name, optarg);
				exit (1);
			}
			authkey = malloc(authkeylen);
			if(authkey == NULL) {
				fprintf(stderr, "%s: Memory allocation error.\n", program_name);
				exit(1);
			}
			memset(authkey, 0, authkeylen);
			authkeylen = atodata(optarg, 0, authkey, authkeylen);
			akey_opt = optarg;
			break;
		case 'E':
			if(optarg[0] == '0') {
				switch(optarg[1]) {
				case 't':
				case 'x':
				case 's':
					break;
				default:
					fprintf(stderr, "%s: Encryption key must have a '0x', '0t' or '0s' prefix to select the format: %s\n",
						program_name, optarg);
					exit(1);
				}
			}
			enckeylen = atodata(optarg, 0, NULL, 0);
			if(!enckeylen) {
				fprintf(stderr, "%s: unknown format or syntax error in encryption key: %s\n",
					program_name, optarg);
				exit (1);
			}
			enckey = malloc(enckeylen);
			if(enckey == NULL) {
				fprintf(stderr, "%s: Memory allocation error.\n", program_name);
				exit(1);
			}
			memset(enckey, 0, enckeylen);
			enckeylen = atodata(optarg, 0, enckey, enckeylen);
			ekey_opt = optarg;
			break;
		case 'w':
			replay_window = strtoul(optarg, &endptr, 0);
			if(!(endptr == optarg + strlen(optarg))) {
				fprintf(stderr, "%s: Invalid character in replay_window parameter: %s\n",
					program_name, optarg);
				exit (1);
			}
			if((replay_window < 0x1) || (replay_window > 64)) {
				fprintf(stderr, "%s: Failed -- Illegal window size: arg=%s, replay_window=%d, must be 1 <= size <= 64.\n",
					program_name, optarg, replay_window);
				exit(1);
			}
			break;
		case 'i':
			if(optarg[0] == '0') {
				switch(optarg[1]) {
				case 't':
				case 'x':
				case 's':
					break;
				default:
					fprintf(stderr, "%s: IV must have a '0x', '0t' or '0s' prefix to select the format, found '%c'.\n",
						program_name, optarg[1]);
					exit(1);
				}
			}
			ivlen = atodata(optarg, 0, NULL, 0);
			if(!ivlen) {
				fprintf(stderr, "%s: unknown format or syntax error in IV: %s\n",
					program_name, optarg);
				exit (1);
			}
			iv = malloc(ivlen);
			if(iv == NULL) {
				fprintf(stderr, "%s: Memory allocation error.\n", program_name);
				exit(1);
			}
			memset(iv, 0, ivlen);
			ivlen = atodata(optarg, 0, iv, ivlen);
			iv_opt = optarg;
			break;
		case 'D':
			if(dst_opt) {
				fprintf(stderr, "%s: Error, DST parameter redefined:%s, already defined as:%s\n",
					program_name, optarg, dst_opt);
				exit (1);
			}				
			error_s = ttoaddr(optarg, 0, address_family, &dst);
			if(error_s != NULL) {
				fprintf(stderr, "%s: Error, %s converting --dst argument:%s\n",
					program_name, error_s, optarg);
				exit (1);
			}
			dst_opt = optarg;
			if(debug) {
				addrtot(&dst, 0, ipaddr_txt, sizeof(ipaddr_txt));
				fprintf(stdout, "dst=%s.\n", ipaddr_txt);
			}
			break;
		case 'S':
			if(src_opt) {
				fprintf(stderr, "%s: Error, SRC parameter redefined:%s, already defined as:%s\n",
					program_name, optarg, src_opt);
				exit (1);
			}				
			error_s = ttoaddr(optarg, 0, address_family, &src);
			if(error_s != NULL) {
				fprintf(stderr, "%s: Error, %s converting --src argument:%s\n",
					program_name, error_s, optarg);
				exit (1);
			}
			src_opt = optarg;
			if(debug) {
				addrtot(&src, 0, ipaddr_txt, sizeof(ipaddr_txt));
				fprintf(stdout, "src=%s.\n", ipaddr_txt);
			}
			break;
		case 'h':
			usage(program_name, stdout);
			exit(0);
		case '?':
			usage(program_name, stderr);
			exit(1);
		case 'v':
			fprintf(stdout, "%s %s\n", me, ipsec_version_code());
			fprintf(stdout, "See `ipsec --copyright' for copyright information.\n");
			exit(1);
		case '+': /* optionsfrom */
			optionsfrom(optarg, &argc, &argv, optind, stderr);
			/* no return on error */
			break;
		case 'f':
			if(parse_life_options(life,
					   life_opt,
					   optarg) != 0) {
				exit(1);
			};
			break;
		default:
			fprintf(stderr, "%s: unrecognized option '%c', update option processing.\n",
				program_name, c);
			exit(1);
		}
		previous = c;
	}
	if(debug) {
		fprintf(stdout, "All options processed.\n");
	}

	if(argcount == 1) {
		system("cat /proc/net/ipsec_spi");
		exit(0);
	}

	switch(alg) {
#ifndef NO_KERNEL_ALG
	case XF_OTHER_ALG: 
		/* validate keysizes */
		if (proc_read_ok) {
		       const struct sadb_alg *alg_p;
		       int keylen, minbits, maxbits;
		       alg_p=kernel_alg_sadb_alg_get(SADB_SATYPE_ESP,SADB_EXT_SUPPORTED_ENCRYPT, 
				       esp_info->encryptalg);
		       assert(alg_p);
		       keylen=enckeylen * 8;

		       if (alg_p->sadb_alg_id==ESP_3DES || alg_p->sadb_alg_id==ESP_DES) {
			       maxbits=minbits=alg_p->sadb_alg_minbits * 8 /7;
		       } else {
			       minbits=alg_p->sadb_alg_minbits;
			       maxbits=alg_p->sadb_alg_maxbits;
		       }
		       /* 
			* if explicit keylen told in encrypt algo, eg "aes128"
			* check actual keylen "equality"
			*/
		       if (esp_info->esp_ealg_keylen &&
			       esp_info->esp_ealg_keylen!=keylen) {
			       fprintf(stderr, "%s: invalid encryption keylen=%d, "
					       "required %d by encrypt algo string=\"%s\"\n",
				       program_name, 
				       keylen,
				       esp_info->esp_ealg_keylen,
				       alg_string);
			       exit(1);

		       }
		       /* thanks DES for this sh*t */

		       if (minbits > keylen || maxbits < keylen) {
			       fprintf(stderr, "%s: invalid encryption keylen=%d, "
					       "must be between %d and %d bits\n",
					       program_name, 
					       keylen, minbits, maxbits);
			       exit(1);
		       }
		       alg_p=kernel_alg_sadb_alg_get(SADB_SATYPE_ESP,SADB_EXT_SUPPORTED_AUTH, 
				       esp_info->authalg);
		       assert(alg_p);
		       keylen=authkeylen * 8;
		       minbits=alg_p->sadb_alg_minbits;
		       maxbits=alg_p->sadb_alg_maxbits;
		       if (minbits > keylen || maxbits < keylen) {
			       fprintf(stderr, "%s: invalid auth keylen=%d, "
					       "must be between %d and %d bits\n",
					       program_name, 
					       keylen, minbits, maxbits);
			       exit(1);
		       }

		}
#endif /* NO_KERNEL_ALG */
	case XF_IP4:
	case XF_IP6:
	case XF_DEL:
	case XF_AHHMACMD5:
	case XF_AHHMACSHA1:
#ifdef USE_SINGLE_DES
	case XF_ESPDESMD596:
	case XF_ESPDESSHA196:
	case XF_ESPDES:
#endif
	case XF_ESP3DESMD596:
	case XF_ESP3DESSHA196:
	case XF_ESPNULLMD596:
	case XF_ESPNULLSHA196:
	case XF_ESP3DES:
	case XF_COMPDEFLATE:
#ifdef CONFIG_IPSEC_ENC_AES
	case XF_ESPAES128MD596:
	case XF_ESPAES128SHA196:
	case XF_ESPAES192MD596:
	case XF_ESPAES192SHA196:
	case XF_ESPAES256MD596:
	case XF_ESPAES256SHA196:
#endif
		if(!said_opt) {
			if(isanyaddr(&edst)) {
				fprintf(stderr, "%s: SA destination not specified.\n",
					program_name);
				exit(1);
			}
			if(!spi) {
				fprintf(stderr, "%s: SA SPI not specified.\n",
					program_name);
				exit(1);
			}
			if(!proto) {
				fprintf(stderr, "%s: SA PROTO not specified.\n",
					program_name);
				exit(1);
			}
			initsaid(&edst, htonl(spi), proto, &said);
		} else {
			proto = said.proto;
			spi = ntohl(said.spi);
			edst = said.dst;
		}
		if((address_family != 0) && (address_family != addrtypeof(&said.dst))) {
			fprintf(stderr, "%s: Defined address family and address family of SA missmatch.\n",
				program_name);
			exit(1);
		}
		sa_len = satot(&said, 0, sa, sizeof(sa));

		if(debug) {
			fprintf(stdout, "SA valid.\n");
		}
		break;
	case XF_CLR:
		break;
	default:
		fprintf(stderr, "%s: No action chosen.  See '%s --help' for usage.\n",
			program_name, program_name);
		exit(1);
	}

	switch(alg) {
	case XF_CLR:
	case XF_DEL:
	case XF_IP4:
	case XF_IP6:
	case XF_AHHMACMD5:
	case XF_AHHMACSHA1:
#ifdef USE_SINGLE_DES
	case XF_ESPDESMD596:
	case XF_ESPDESSHA196:
	case XF_ESPDES:
#endif
	case XF_ESP3DESMD596:
	case XF_ESP3DESSHA196:
	case XF_ESPNULLMD596:
	case XF_ESPNULLSHA196:
	case XF_ESP3DES:
	case XF_COMPDEFLATE:
#ifdef CONFIG_IPSEC_ENC_AES
	case XF_ESPAES128MD596:
	case XF_ESPAES128SHA196:
	case XF_ESPAES192MD596:
	case XF_ESPAES192SHA196:
	case XF_ESPAES256MD596:
	case XF_ESPAES256SHA196:
#endif
#ifndef NO_KERNEL_ALG
	case XF_OTHER_ALG:
#endif /* NO_KERNEL_ALG */
		break;
	default:
		fprintf(stderr, "%s: No action chosen.  See '%s --help' for usage.\n",
			program_name, program_name);
		exit(1);
	}
	if(debug) {
		fprintf(stdout, "Algorithm ok.\n");
	}

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

#ifdef SIGS_FROM_BELOW
	{
		struct sigaction sig_act;
		int sig_act_err;
		memset(&sig_act, 0, sizeof(sig_act));

		sig_act.sa_handler = pfkey_sig_handler;
		sigemptyset(&sig_act.sa_mask);
		sig_act.sa_flags = SA_RESTART;

		sig_act_err = sigaction(SIGIO, &sig_act, NULL);
		if(sig_act_err < 0) {
			fprintf(stderr, "Signal handler registration error.\n", sig_act_err);
			exit(1);
		}
	}
#endif /* SIGS_FROM_BELOW */
	
#ifdef MANUAL_IS_NOT_ABLE_TO_NEGOTIATE
	/* for registering SA types that can be negotiated */
	if(pfkey_register(SADB_SATYPE_AH) != 0) {
		exit(1);
	}
	if(pfkey_register(SADB_SATYPE_ESP)) != 0) {
		exit(1);
	}
	if(pfkey_register(SADB_X_SATYPE_IPIP)) != 0) {
		exit(1);
	}
	if(pfkey_register(SADB_X_SATYPE_COMP)) != 0) {
		exit(1);
	}
#endif /* MANUAL_IS_NOT_ABLE_TO_NEGOTIATE */

	/* Build an SADB_ADD message to send down. */
	/* It needs <base, SA, address(SD), key(AE)> minimum. */
	/*   Lifetime(HS) could be added before addresses. */
	pfkey_extensions_init(extensions);
	if(debug) {
		fprintf(stdout, "%s: extensions=%p &extensions=%p extensions[0]=%p &extensions[0]=%p cleared.\n",
			program_name,
			extensions,
			&extensions,
			extensions[0],
			&extensions[0]);
	}
	if((error = pfkey_msg_hdr_build(&extensions[0],
					(alg == XF_DEL ? SADB_DELETE : alg == XF_CLR ? SADB_FLUSH : SADB_ADD),
					proto2satype(proto),
					0,
			    ++pfkey_seq,
			    getpid()))) {
		fprintf(stderr, "%s: Trouble building message header, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		exit(1);
	}
	if(debug) {
		fprintf(stdout, "%s: extensions=%p &extensions=%p extensions[0]=%p &extensions[0]=%p set w/msghdr.\n",
			program_name,
			extensions,
			&extensions,
			extensions[0],
			&extensions[0]);
	}
	if(debug) {
		fprintf(stdout, "%s: base message assembled.\n", program_name);
	}
	
	switch(alg) {
	case XF_AHHMACMD5:
#ifdef USE_SINGLE_DES
	case XF_ESPDESMD596:
#endif
	case XF_ESP3DESMD596:
	case XF_ESPNULLMD596:
#ifdef CONFIG_IPSEC_ENC_AES
	case XF_ESPAES128MD596:
	case XF_ESPAES192MD596:
	case XF_ESPAES256MD596:
#endif
		authalg = SADB_AALG_MD5HMAC;
		break;
	case XF_AHHMACSHA1:
#ifdef USE_SINGLE_DES
	case XF_ESPDESSHA196:
#endif
	case XF_ESP3DESSHA196:
	case XF_ESPNULLSHA196:
#ifdef CONFIG_IPSEC_ENC_AES
	case XF_ESPAES128SHA196:
	case XF_ESPAES192SHA196:
	case XF_ESPAES256SHA196:
#endif
		authalg = SADB_AALG_SHA1HMAC;
		break;
#ifndef NO_KERNEL_ALG
	case XF_OTHER_ALG:
		authalg= esp_info->authalg;
		if(debug) {
		    fprintf(stdout, "%s: debug: authalg=%d\n",
			program_name, authalg);
		}
		break;
#endif /* NO_KERNEL_ALG */
#ifdef USE_SINGLE_DES
	case XF_ESPDESMD5:
#endif
	case XF_ESP3DESMD5:
	default:
		authalg = SADB_AALG_NONE;
	}
	switch(alg) {
#ifdef USE_SINGLE_DES
	case XF_ESPDES:
	case XF_ESPDESMD596:
	case XF_ESPDESSHA196:
		encryptalg = SADB_EALG_DESCBC;
		break;
#endif
	case XF_ESP3DES:
	case XF_ESP3DESMD596:
	case XF_ESP3DESSHA196:
		encryptalg = SADB_EALG_3DESCBC;
		break;
#ifdef CONFIG_IPSEC_ENC_AES
	case XF_ESPAES128MD596:
	case XF_ESPAES192MD596:
	case XF_ESPAES256MD596:
	case XF_ESPAES128SHA196:
	case XF_ESPAES192SHA196:
	case XF_ESPAES256SHA196:
		encryptalg = SADB_EALG_AESCBC;
		break;
#endif
	case XF_ESPNULLMD596:
	case XF_ESPNULLSHA196:
		encryptalg = SADB_EALG_NULL;
		break;
	case XF_COMPDEFLATE:
		encryptalg = SADB_X_CALG_DEFLATE;
		break;
#ifndef NO_KERNEL_ALG
	case XF_OTHER_ALG:
		encryptalg= esp_info->encryptalg;
		if(debug) {
			fprintf(stdout, "%s: debug: encryptalg=%d\n",
				program_name, encryptalg);
		}
		break;
#endif /* NO_KERNEL_ALG */
	default:
		encryptalg = SADB_EALG_NONE;
	}
	if(!(alg == XF_CLR /* IE: pfkey_msg->sadb_msg_type == SADB_FLUSH */)) {
		if((error = pfkey_sa_build(&extensions[SADB_EXT_SA],
					   SADB_EXT_SA,
					   htonl(spi), /* in network order */
					   replay_window,
					   SADB_SASTATE_MATURE,
					   authalg,
					   encryptalg,
					   0))) {
			fprintf(stderr, "%s: Trouble building sa extension, error=%d.\n",
				program_name, error);
			pfkey_extensions_free(extensions);
			exit(1);
		}
		if(debug) {
			fprintf(stdout, "%s: extensions[0]=%p previously set with msg_hdr.\n",
				program_name,
				extensions[0]);
		}
		if(debug) {
			fprintf(stdout, "%s: assembled SA extension, pfkey msg authalg=%d encalg=%d.\n",
				program_name,
				authalg,
				encryptalg);
		}
		
#if 1 /* def PFKEY_LIFETIME */
		if(debug) {
			int i,j;
			for(i = 0; i < life_maxsever; i++) {
				for(j = 0; j < life_maxtype; j++) {
					fprintf(stdout, "i=%d, j=%d, life_opt[%d][%d]=%p, life[%d][%d]=%d\n",
						i, j, i, j, life_opt[i][j], i, j, life[i][j]);
				}
			}
		}
		if(life_opt[life_soft][life_alloc] != NULL ||
		   life_opt[life_soft][life_bytes] != NULL ||
		   life_opt[life_soft][life_addtime] != NULL ||
		   life_opt[life_soft][life_usetime] != NULL ||
		   life_opt[life_soft][life_packets] != NULL) {
			if((error = pfkey_lifetime_build(&extensions[SADB_EXT_LIFETIME_SOFT],
							 SADB_EXT_LIFETIME_SOFT,
							 life[life_soft][life_alloc],/*-1,*/		/*allocations*/
							 life[life_soft][life_bytes],/*-1,*/		/*bytes*/
							 life[life_soft][life_addtime],/*-1,*/		/*addtime*/
							 life[life_soft][life_usetime],/*-1,*/		/*usetime*/
							 life[life_soft][life_packets]/*-1*/))) {	/*packets*/
				fprintf(stderr, "%s: Trouble building lifetime_s extension, error=%d.\n",
					program_name, error);
				pfkey_extensions_free(extensions);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "%s: lifetime_s extension assembled.\n",
					program_name);
			}
		}

		if(life_opt[life_hard][life_alloc] != NULL ||
		   life_opt[life_hard][life_bytes] != NULL ||
		   life_opt[life_hard][life_addtime] != NULL ||
		   life_opt[life_hard][life_usetime] != NULL ||
		   life_opt[life_hard][life_packets] != NULL) {
			if((error = pfkey_lifetime_build(&extensions[SADB_EXT_LIFETIME_HARD],
							 SADB_EXT_LIFETIME_HARD,
							 life[life_hard][life_alloc],/*-1,*/		/*allocations*/
							 life[life_hard][life_bytes],/*-1,*/		/*bytes*/
							 life[life_hard][life_addtime],/*-1,*/		/*addtime*/
							 life[life_hard][life_usetime],/*-1,*/		/*usetime*/
							 life[life_hard][life_packets]/*-1*/))) {	/*packets*/
				fprintf(stderr, "%s: Trouble building lifetime_h extension, error=%d.\n",
					program_name, error);
				pfkey_extensions_free(extensions);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "%s: lifetime_h extension assembled.\n",
					program_name);
			}
		}
#endif /* PFKEY_LIFETIME */
		
		if(debug) {
                	addrtot(&src, 0, ipaddr_txt, sizeof(ipaddr_txt));
			fprintf(stdout, "%s: assembling address_s extension (%s).\n",
				program_name, ipaddr_txt);
		}
	
		if((error = pfkey_address_build(&extensions[SADB_EXT_ADDRESS_SRC],
						SADB_EXT_ADDRESS_SRC,
						0,
						0,
						sockaddrof(&src)))) {
                	addrtot(&src, 0, ipaddr_txt, sizeof(ipaddr_txt));
			fprintf(stderr, "%s: Trouble building address_s extension (%s), error=%d.\n",
				program_name, ipaddr_txt, error);
			pfkey_extensions_free(extensions);
			exit(1);
		}
		if(debug) {
			ip_address temp_addr;
			
			switch(address_family) {
				case AF_INET:
					initaddr((const unsigned char *)&(((struct sockaddr_in*)( ((struct sadb_address*)(extensions[SADB_EXT_ADDRESS_SRC])) + 1))->sin_addr),
						sockaddrlenof(&src), address_family, &temp_addr);
					break;
				case AF_INET6:
					initaddr((const unsigned char *)&(((struct sockaddr_in6*)( ((struct sadb_address*)(extensions[SADB_EXT_ADDRESS_SRC])) + 1))->sin6_addr),
						sockaddrlenof(&src), address_family, &temp_addr);
					break;
				default:
					fprintf(stdout, "%s: unknown address family (%d).\n",
						program_name, address_family);
					exit(1);
			}
                	addrtot(&temp_addr, 0, ipaddr_txt, sizeof(ipaddr_txt));
			fprintf(stdout, "%s: address_s extension assembled (%s).\n",
				program_name, ipaddr_txt);
		}
	
		if(debug) {
                	addrtot(&edst, 0, ipaddr_txt, sizeof(ipaddr_txt));
			fprintf(stdout, "%s: assembling address_d extension (%s).\n",
				program_name, ipaddr_txt);
		}
	
		if((error = pfkey_address_build(&extensions[SADB_EXT_ADDRESS_DST],
						SADB_EXT_ADDRESS_DST,
						0,
						0,
						sockaddrof(&edst)))) {
                	addrtot(&edst, 0, ipaddr_txt, sizeof(ipaddr_txt));
			fprintf(stderr, "%s: Trouble building address_d extension (%s), error=%d.\n",
				program_name, ipaddr_txt, error);
			pfkey_extensions_free(extensions);
			exit(1);
		}
		if(debug) {
			ip_address temp_addr;
			switch(address_family) {
				case AF_INET:
					initaddr((const unsigned char *)&(((struct sockaddr_in*)( ((struct sadb_address*)(extensions[SADB_EXT_ADDRESS_DST])) + 1))->sin_addr),
						4, address_family, &temp_addr);
					break;
				case AF_INET6:
					initaddr((const unsigned char *)&(((struct sockaddr_in6*)( ((struct sadb_address*)(extensions[SADB_EXT_ADDRESS_DST])) + 1))->sin6_addr),
						16, address_family, &temp_addr);
					break;
				default:
					fprintf(stdout, "%s: unknown address family (%d).\n",
						program_name, address_family);
					exit(1);
			}
                	addrtot(&temp_addr, 0, ipaddr_txt, sizeof(ipaddr_txt));
			fprintf(stdout, "%s: address_d extension assembled (%s).\n",
				program_name, ipaddr_txt);
		}

#if PFKEY_PROXY
		anyaddr(address_family, &pfkey_address_p_ska);
		if((error = pfkey_address_build(&extensions[SADB_EXT_ADDRESS_PROXY],
						SADB_EXT_ADDRESS_PROXY,
						0,
						0,
						sockaddrof(&pfkey_address_p_ska)))) {
			fprintf(stderr, "%s: Trouble building address_p extension, error=%d.\n",
				program_name, error);
			pfkey_extensions_free(extensions);
			exit(1);
		}
		if(debug) {
			fprintf(stdout, "%s: address_p extension assembled.\n", program_name);
		}
#endif /* PFKEY_PROXY */
		
		switch(alg) {
#ifndef NO_KERNEL_ALG
		/*	Allow no auth ... after all is local root decision 8)  */
		case XF_OTHER_ALG:
			if (!authalg)
				break;
#endif /* NO_KERNEL_ALG */
		case XF_AHHMACMD5:
#ifdef USE_SINGLE_DES
		case XF_ESPDESMD596:
		case XF_ESPDESSHA196:
#endif
		case XF_ESP3DESMD596:
		case XF_ESPNULLMD596:
		case XF_AHHMACSHA1:
		case XF_ESP3DESSHA196:
		case XF_ESPNULLSHA196:
#ifdef CONFIG_IPSEC_ENC_AES
		case XF_ESPAES128MD596:
		case XF_ESPAES192MD596:
		case XF_ESPAES256MD596:
		case XF_ESPAES128SHA196:
		case XF_ESPAES192SHA196:
		case XF_ESPAES256SHA196:
#endif
			if((error = pfkey_key_build(&extensions[SADB_EXT_KEY_AUTH],
						    SADB_EXT_KEY_AUTH,
						    authkeylen * 8,
						    authkey))) {
				fprintf(stderr, "%s: Trouble building key_a extension, error=%d.\n",
					program_name, error);
				pfkey_extensions_free(extensions);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "%s: key_a extension assembled.\n",
					program_name);
			}
			break;
		default:
			;
		}
		
		switch(alg) {
#ifdef USE_SINGLE_DES
		case XF_ESPDES:
		case XF_ESPDESMD596:
		case XF_ESPDESSHA196:
#endif
		case XF_ESP3DES:
		case XF_ESP3DESMD596:
		case XF_ESP3DESSHA196:
#ifdef CONFIG_IPSEC_ENC_AES
		case XF_ESPAES128MD596:
		case XF_ESPAES128SHA196:
		case XF_ESPAES192MD596:
		case XF_ESPAES192SHA196:
		case XF_ESPAES256MD596:
		case XF_ESPAES256SHA196:
#endif
#ifndef NO_KERNEL_ALG
		case XF_OTHER_ALG:
#endif /* NO_KERNEL_ALG */
			if((error = pfkey_key_build(&extensions[SADB_EXT_KEY_ENCRYPT],
						    SADB_EXT_KEY_ENCRYPT,
						    enckeylen * 8,
						    enckey))) {
				fprintf(stderr, "%s: Trouble building key_e extension, error=%d.\n",
					program_name, error);
				pfkey_extensions_free(extensions);
				exit(1);
			}
			if(debug) {
				fprintf(stdout, "%s: key_e extension assembled.\n",
					program_name);
			}
			break;
		default:
			;
		}
		
#ifdef PFKEY_IDENT /* GG: looks wired, not touched */
		if((pfkey_ident_build(&extensions[SADB_EXT_IDENTITY_SRC],
				      SADB_EXT_IDENTITY_SRC,
				      SADB_IDENTTYPE_PREFIX,
				      0,
				      strlen(pfkey_ident_s_ska),
				      pfkey_ident_s_ska))) {
			fprintf(stderr, "%s: Trouble building ident_s extension, error=%d.\n",
				program_name, error);
			pfkey_extensions_free(extensions);
			exit(1);
		}
		if(subnettoa(addr, mask, format, pfkey_ident_s_ska,
			     sizeof(pfkey_ident_s_ska) ) !=
		   sizeof(pfkey_ident_s_ska) ) {
			exit (1);
		}
		
		if((error = pfkey_ident_build(&extensions[SADB_EXT_IDENTITY_DST],
					      SADB_EXT_IDENTITY_DST,
					      SADB_IDENTTYPE_PREFIX,
					      0,
					      strlen(pfkey_ident_d_ska),
					      pfkey_ident_d_ska))) {
			fprintf(stderr, "%s: Trouble building ident_d extension, error=%d.\n",
				program_name, error);
			pfkey_extensions_free(extensions);
			exit(1);
		}
		if(subnettoa(addr, mask, format, pfkey_ident_d_ska,
			     sizeof(pfkey_ident_d_ska) ) !=
		   sizeof(pfkey_ident_d_ska) ) {
			exit (1);
		}

		if(debug) {
			fprintf(stdout, "%s: ident extensions assembled.\n",
				program_name);
		}
#endif /* PFKEY_IDENT */
	}
	
	if(debug) {
		fprintf(stdout, "%s: assembling pfkey msg....\n",
			program_name);
	}
	if((error = pfkey_msg_build(&pfkey_msg, extensions, EXT_BITS_IN))) {
		fprintf(stderr, "%s: Trouble building pfkey message, error=%d.\n",
			program_name, error);
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
		exit(1);
	}
	if(debug) {
		fprintf(stdout, "%s: assembled.\n",
			program_name);
	}
	if(debug) {
		fprintf(stdout, "%s: writing pfkey msg.\n",
			program_name);
	}
	if((error = write(pfkey_sock,
			  pfkey_msg,
			  pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN)) !=
	   pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		fprintf(stderr, "%s: pfkey write failed, returning %d with errno=%d.\n",
			program_name, error, errno);
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
		case ESRCH:
			fprintf(stderr, "SA does not exist.  Cannot delete.\n");
			break;
		default:
			fprintf(stderr, "Unknown socket write error %d.  Please report as much detail as possible to development team.\n", errno);
		}
/*		fprintf(stderr, "%s: socket write returned errno %d\n",
		program_name, errno);*/
		exit(1);
	}
	if(debug) {
		fprintf(stdout, "%s: pfkey command written to socket.\n",
			program_name);
	}
	
#if 0 /* use write() rather than sendmsg() */
	error = sendmsg(/* int */pfkey_sock,
		/* const void* */msg,
		/* int */len,
		/* unsigned int flags MSG_OOB|MSG_DONTROUTE */ 0);
	if( error == -1 ) {
		fprintf(stderr, "%s: pfkey sendmsg failed.\n",
			program_name);
		switch(errno) {
			case EINVAL:
				fprintf(stderr, "bad data error, since this should not happen, advise the maintainer.\n");
				exit(1);
			case EBADF:
			case ENOTSOCK:
			case EFAULT:
			case EMSGSIZE:
			case EWOULDBLOCK:
			case ENOBUFS:
				fprintf(stderr, "system error:%d\n", error);
				exit(1);
			default:
				fprintf(stderr, "unknown error:%d\n", error);
				exit(1);
		}
	}
#endif

#if 0
	sleep (1); /* wait for errors to come back through signal handling */
	read(pfkey_sock, &pfkey_buf, sizeof(pfkey_msg) );
	fprintf(stdout, "%s: pfkey_buf read.\n", program_name);
	/* fprintf(stdout, "%s: press a key to close pfkey socket.\n", program_name); */
	/* getchar(); */ /* RGB wait for keystroke to exit (debug) */
	fprintf(stdout, "%s: sleeping 2 seconds to allow return messages.\n", program_name);
	sleep(1); /* wait for errors to come back through signal handling */
	fprintf(stdout, "%s: pfkey_sig_handler called.\n", program_name);
	pfkey_sig_handler(0); /* solicit upmsg */
#endif
	if(pfkey_msg) {
		pfkey_extensions_free(extensions);
		pfkey_msg_free(&pfkey_msg);
	}
	if(debug) {
		fprintf(stdout, "%s: pfkey message buffer freed.\n",
			program_name);
	}
	(void) close(pfkey_sock);  /* close the socket */
	if(authkey) {
		memset((caddr_t)authkey, 0, authkeylen);
		free(authkey);
	}
	if(enckey) {
		memset((caddr_t)enckey, 0, enckeylen);
		free(enckey);
	}
	if(iv) {
		memset((caddr_t)iv, 0, ivlen);
		free(iv);
	}
	exit(0);
}

