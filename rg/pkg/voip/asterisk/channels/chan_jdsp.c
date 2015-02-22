/****************************************************************************
 *
 * rg/pkg/voip/asterisk/channels/chan_jdsp.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

/*
 *
 * JDSP abstraction layer interface
 *
 */

#include <stdio.h>
#include <string.h>
#ifdef __NetBSD__
#include <pthread.h>
#include <signal.h>
#else
#include <sys/signal.h>
#endif
#include <errno.h>
#include <stdlib.h>
#if !defined(SOLARIS) && !defined(__FreeBSD__)
#include <stdint.h>
#endif
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.129 $")

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/config.h"
#include "asterisk/logger.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/options.h"
#include "asterisk/file.h"
#include "asterisk/ulaw.h"
#include "asterisk/alaw.h"
#include "asterisk/callerid.h"
#include "asterisk/adsi.h"
#include "asterisk/cli.h"
#include "asterisk/cdr.h"
#include "asterisk/features.h"
#include "asterisk/musiconhold.h"
#include "asterisk/say.h"
#include "asterisk/tdd.h"
#include "asterisk/app.h"
#include "asterisk/astdb.h"
#include "asterisk/manager.h"
#include "asterisk/causes.h"
#include "asterisk/term.h"
#include "asterisk/utils.h"
#include "asterisk/transcap.h"
#include "asterisk/rtp.h"
#include "asterisk/jdsp_common.h"

#include <util/openrg_gpl.h>
#include <voip/dsp/phone.h>
#include <kos_chardev_id.h>
#include <rg_ioctl.h>

/* XXX Must be configurable */
#define NATIVE_FORMATS ((AST_FORMAT_MAX_AUDIO << 1) - 1)

static const char desc[] = "Jungo DSP Abstraction Layer";

static const char tdesc[] = "Jungo DSP Abstraction Layer Driver";

static const char type[] = "jdsp";
static const char config[] = "jdsp.conf";

#define SIG_FXOLS	1 /* JDSP_SIG_FXOLS */
#define SIG_FXOGS	2 /* JDSP_SIG_FXOGS */

typedef enum {
    TRANSFER_OFF = 0,
    TRANSFER_SIGNALLING = 1,
    TRANSFER_BRIDGING = 2,
} transfermode_t;

typedef enum {
    FAX_TX_NONE = 0,
    FAX_TX_T38_AUTO = 1,
    FAX_TX_PASSTHROUGH_AUTO = 2,
    FAX_TX_PASSTHROUGH_FORCE = 3,
} faxtxmethod_t;

static int global_native_formats = 0; 

static char context[AST_MAX_CONTEXT] = "default";
static char cid_num[256] = "";
static char cid_name[256] = "";

static char language[MAX_LANGUAGE] = "";
static char musicclass[MAX_MUSICCLASS] = "";
static char progzone[10]= "";

static int transfertobusy = 1;

static int use_callerid = 1;
static int cid_signalling = CID_SIG_BELL;
static int jdsptrcallerid = 0;
static int cur_signalling = -1;

static ast_group_t cur_group = 0;
static ast_group_t cur_callergroup = 0;
static ast_group_t cur_pickupgroup = 0;

static int enabled = 1;

static int immediate = 0;

static int internalmoh = 1;

static int callwaiting = 0;

static int callwaitingcallerid = 0;

static int hidecallerid = 0;

static int callreturn = 0;

static int threewaycalling = 0;

static int threewayconference = 0;

static int stutterdialtone = 0;

static int mwi = 0;

static transfermode_t transfermode = TRANSFER_OFF;

static int canpark = 0;

static float rxgain = 0.0;

static float txgain = 0.0;

static int tonezone = -1;

static int callprogress = 0;

static char accountcode[AST_MAX_ACCOUNT_CODE] = "";

static char mailbox[AST_MAX_EXTENSION];

static char cfwd_unconditional_activate_code[AST_MAX_EXTENSION];

static char cfwd_unconditional_deactivate_code[AST_MAX_EXTENSION];

static char cfwd_busy_activate_code[AST_MAX_EXTENSION];

static char cfwd_busy_deactivate_code[AST_MAX_EXTENSION];

static char cfwd_no_answer_activate_code[AST_MAX_EXTENSION];

static char cfwd_no_answer_deactivate_code[AST_MAX_EXTENSION];

static char dnd_activate_code[AST_MAX_EXTENSION];

static char dnd_deactivate_code[AST_MAX_EXTENSION];

static int adsi = 0;

static faxtxmethod_t faxtxmethod = FAX_TX_NONE;

/*! \brief Wait up to 10 seconds for first digit (FXO logic) */
/* Equivalent to the fdt (first digit timer) in RFC 2897 */
static int firstdigittimeout = 10000;

/*! \brief Wait 10 seconds after start playing reorder tone, and before playing
 * off-hook warning tone (FXO logic) */
static int offhookwarningtimeout = 10000;

/*! \brief How long to wait for following digits (FXO logic) */
/* Equivalent to the idt (inter digit timer) in RFC 2897 */
/* Similar to InterDigitTimerStd in TR-104 */
static int gendigittimeout = 8000;

/*! \brief How long to wait for an extra digit, if there is an ambiguous match */
/* Also quivalent to the idt (inter digit timer) in RFC 2897 */
/* Similar to InterDigitTimerOpen in TR-104 */
static int matchdigittimeout = 3000;

static int usecnt = 0;
AST_MUTEX_DEFINE_STATIC(usecnt_lock);

/*! \brief Protect the interface list (of jdsp_pvt's) */
AST_MUTEX_DEFINE_STATIC(iflock);

static int ifcount = 0;

/*! \brief Protect the monitoring thread, so only one process can kill or start it, and not
   when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(monlock);

/*! \brief This is the thread for the monitor which checks for input on the channels
   which are not currently in use.  */
static pthread_t monitor_thread = AST_PTHREADT_NULL;

static struct sched_context *sched;

static int restart_monitor(void);

static inline int jdsp_get_event(int fd, phone_event_t *event)
{
	if (read(fd, event, sizeof(*event)) < 0)
		return -1;
	return 0;
}

#define RTP_HEADER_SIZE 12

/*! Chunk size to read -- we use 20ms chunks to make things happy.  */   
#define READ_SIZE (160 + RTP_HEADER_SIZE)

#define MIN_MS_SINCE_FLASH			( (2000) )	/*!< 2000 ms */

struct jdsp_pvt;

#define SUB_REAL	0			/*!< Active call */
#define SUB_CALLWAIT	1			/*!< Call-Waiting call on hold */
#define SUB_THREEWAY	2			/*!< Three-way call */

static char *subnames[] = {
	"Real",
	"Callwait",
	"Threeway"
};

struct jdsp_subchannel {
	int dfd; /*!< File descriptor for direct access for DSP (not SLIC) */
	struct ast_channel *owner;
	int chan; 				/*!< Associated channel in DSP (0/1) */
	char buffer[AST_FRIENDLY_OFFSET + READ_SIZE];
	struct ast_frame f;		/*!< One frame for each channel.  How did this ever work before? */
	struct ast_rtp *rtp;

	unsigned short seqno;
	unsigned long lastts;
	struct timeval txcore;

	unsigned int needanswer:1;
	unsigned int inthreeway:1;
};

#define CONF_USER_REAL		(1 << 0)
#define CONF_USER_THIRDCALL	(1 << 1)

#define MAX_SLAVES	4

static struct jdsp_pvt {
	ast_mutex_t lock;
	struct ast_channel *owner;			/*!< Our current active owner (if applicable) */
							/*!< Up to three channels can be associated with this call */
		
	struct jdsp_subchannel sub_unused;		/*!< Just a safety precaution */
	struct jdsp_subchannel subs[3];			/*!< Sub-channels */

	int jfd;
	struct jdsp_pvt *slaves[MAX_SLAVES];		/*!< Slave to us (follows our conferencing) */
	struct jdsp_pvt *master;			/*!< Master to us (we follow their conferencing) */
	int inconference;				/*!< If our real should be in the conference */
	
	int sig;					/*!< Signalling style */
	float rxgain;
	float txgain;
	int tonezone;					/*!< tone zone for this chan, or -1 for default */
	struct jdsp_pvt *next;				/*!< Next channel in list */
	struct jdsp_pvt *prev;				/*!< Prev channel in list */

	/* flags */
	unsigned int adsi:1;
	unsigned int callreturn:1;
	unsigned int callwaiting:1;
	unsigned int callwaitingcallerid:1;
	unsigned int canpark:1;
	unsigned int confirmanswer:1;			/*!< Wait for '#' to confirm answer */
	unsigned int dialing:1;
	unsigned int hidecallerid;
	unsigned int immediate:1;			/*!< Answer before getting digits? */
	unsigned int outgoing:1;
	unsigned int permcallwaiting:1;
	unsigned int permhidecallerid:1;		/*!< Whether to hide our outgoing caller ID or not */
	unsigned int threewaycalling:1;
	unsigned int threewayconference:1;
	unsigned int stutterdialtone:1;
	unsigned int mwi:1;
	unsigned int use_callerid:1;			/*!< Whether or not to use caller id on this channel */
	unsigned int jdsptrcallerid:1;			/*!< should we use the callerid from incoming call on jdsp transfer or not */
	unsigned int transfertobusy:1;			/*!< allow flash-transfers to busy channels */
	unsigned int faxdetected:1;
	unsigned int enabled:1;
	
	char context[AST_MAX_CONTEXT];
	char defcontext[AST_MAX_CONTEXT];
	char exten[AST_MAX_EXTENSION];
	char language[MAX_LANGUAGE];
	char musicclass[MAX_MUSICCLASS];
	char cid_num[AST_MAX_EXTENSION];
	int cid_ton;					/*!< Type Of Number (TON) */
	char cid_name[AST_MAX_EXTENSION];
	char lastcid_num[AST_MAX_EXTENSION];
	char lastcid_name[AST_MAX_EXTENSION];
	char *origcid_num;				/*!< malloced original callerid */
	char *origcid_name;				/*!< malloced original callerid */
	char callwait_num[AST_MAX_EXTENSION];
	char callwait_name[AST_MAX_EXTENSION];
	char rdnis[AST_MAX_EXTENSION];
	char dnid[AST_MAX_EXTENSION];
	unsigned int group;
	ast_group_t callgroup;
	ast_group_t pickupgroup;
	int channel;					/*!< Channel Number or CRV */
	time_t guardtime;				/*!< Must wait this much time before using for new call */
	int cid_signalling;				/*!< CID signalling type bell202 or v23 */
	int callingpres;				/*!< The value of callling presentation that we're going to use when placing a PRI call */
	int callwaitingalert;				/*!< Flag, means that now played call waiting alert */
	int callwaitrings;
	int callprogress;
	struct timeval flashtime;			/*!< Last flash-hook time */
	char accountcode[AST_MAX_ACCOUNT_CODE];		/*!< Account code */
	char mailbox[AST_MAX_EXTENSION];
	char cfwd_unconditional_activate_code[AST_MAX_EXTENSION];
	char cfwd_unconditional_deactivate_code[AST_MAX_EXTENSION];
	char cfwd_busy_activate_code[AST_MAX_EXTENSION];
	char cfwd_busy_deactivate_code[AST_MAX_EXTENSION];
	char cfwd_no_answer_activate_code[AST_MAX_EXTENSION];
	char cfwd_no_answer_deactivate_code[AST_MAX_EXTENSION];
	char dnd_activate_code[AST_MAX_EXTENSION];
	char dnd_deactivate_code[AST_MAX_EXTENSION];
	char dialdest[256];
	int onhooktime;
	int distinctivering;				/*!< Which distinctivering to use */
	int cidrings;					/*!< Which ring to deliver CID on */
	int dtmfrelax;					/*!< whether to run in relaxed DTMF mode */
#if 0
	int fake_event;
#endif
	int polarityonanswerdelay;
	struct timeval polaritydelaytv;
	int sendcalleridafter;
	int polarity;
	int offhookwarningschedid;
	int play_tone;
	int matchdigittimeout;
	int faxtxmethod;
	transfermode_t transfermode;

} *iflist = NULL, *ifend = NULL;

static struct ast_channel *jdsp_request(const char *type, int format, void *data, int *cause);
static int jdsp_digit_begin(struct ast_channel *ast, char digit);
static int jdsp_digit_end(struct ast_channel *ast, char digit);
static int jdsp_call(struct ast_channel *ast, char *rdest, int timeout);
static int jdsp_hangup(struct ast_channel *ast);
static int jdsp_answer(struct ast_channel *ast);
struct ast_frame *jdsp_read(struct ast_channel *ast);
static int jdsp_write(struct ast_channel *ast, struct ast_frame *frame);
static int jdsp_indicate(struct ast_channel *chan, int condition);
static int jdsp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static void jdsp_pre_bridge(struct ast_channel *chan);
static void jdsp_post_bridge(struct ast_channel *chan);

static const struct ast_channel_tech jdsp_tech = {
	.type = type,
	.description = tdesc,
	.capabilities = NATIVE_FORMATS,
	.requester = jdsp_request,
	.send_digit_begin = jdsp_digit_begin,
	.send_digit_end = jdsp_digit_end,
	.call = jdsp_call,
	.hangup = jdsp_hangup,
	.answer = jdsp_answer,
	.read = jdsp_read,
	.write = jdsp_write,
	.indicate = jdsp_indicate,
	.fixup = jdsp_fixup,
	.pre_bridge = jdsp_pre_bridge,
	.post_bridge = jdsp_post_bridge,
};

struct jdsp_pvt *round_robin[32];

#define ISTRUNK(p) ((p->sig == SIG_FXSLS) || (p->sig == SIG_FXSGS))

#define CANBUSYDETECT(p) ISTRUNK(p)
#define CANPROGRESSDETECT(p) ISTRUNK(p)

static int jdsp_get_index(struct ast_channel *ast, struct jdsp_pvt *p, int nullok)
{
	int res;
	if (p->subs[0].owner == ast)
		res = 0;
	else if (p->subs[1].owner == ast)
		res = 1;
	else if (p->subs[2].owner == ast)
		res = 2;
	else {
		res = -1;
		if (!nullok)
			ast_log(LOG_WARNING, "Unable to get index, and nullok is not asserted\n");
	}
	return res;
}

static void wakeup_sub(struct jdsp_pvt *p, int a, void *pri)
{
	struct ast_frame null = { AST_FRAME_NULL, };
	for (;;) {
		if (p->subs[a].owner) {
			if (ast_mutex_trylock(&p->subs[a].owner->lock)) {
				ast_mutex_unlock(&p->lock);
				usleep(1);
				ast_mutex_lock(&p->lock);
			} else {
				ast_queue_frame(p->subs[a].owner, &null);
				ast_mutex_unlock(&p->subs[a].owner->lock);
				break;
			}
		} else
			break;
	}
}

static void jdsp_queue_frame(struct jdsp_pvt *p, struct ast_frame *f)
{
	/* We must unlock the PRI to avoid the possibility of a deadlock */
	for (;;) {
		if (p->owner) {
			if (ast_mutex_trylock(&p->owner->lock)) {
				ast_mutex_unlock(&p->lock);
				usleep(1);
				ast_mutex_lock(&p->lock);
			} else {
				ast_queue_frame(p->owner, f);
				ast_mutex_unlock(&p->owner->lock);
				break;
			}
		} else
			break;
	}
}

static void jdsp_queue_control(struct ast_channel *ast, int control)
{
	struct ast_frame f = { AST_FRAME_CONTROL, };
	f.subclass = control;
	return jdsp_queue_frame(ast->tech_pvt, &f);
}

static void swap_subs(struct jdsp_pvt *p, int a, int b)
{
	int tchan;
	int tinthreeway;
	struct ast_channel *towner;
	int i;
	voip_dsp_bind_arg_t bind_arg;
	struct ast_rtp *trtp;

	ast_log(LOG_DEBUG, "Swapping %d and %d\n", a, b);

	tchan = p->subs[a].chan;
	towner = p->subs[a].owner;
	tinthreeway = p->subs[a].inthreeway;
	trtp = p->subs[a].rtp;

	p->subs[a].chan = p->subs[b].chan;
	p->subs[a].owner = p->subs[b].owner;
	p->subs[a].inthreeway = p->subs[b].inthreeway;
	p->subs[a].rtp = p->subs[b].rtp;

	p->subs[b].chan = tchan;
	p->subs[b].owner = towner;
	p->subs[b].inthreeway = tinthreeway;
	p->subs[b].rtp = trtp;

	if (p->subs[a].chan != -1 && p->subs[a].dfd != -1 )
	{
		bind_arg.line = p->channel - 1;
		bind_arg.channel = p->subs[a].chan;
		ioctl(p->subs[a].dfd, VOIP_DSP_BIND, &bind_arg);
	}

	if (p->subs[b].chan != -1 && p->subs[b].dfd != -1 )
	{
		bind_arg.line = p->channel - 1;
		bind_arg.channel = p->subs[b].chan;
		ioctl(p->subs[b].dfd, VOIP_DSP_BIND, &bind_arg);
	}

	if (p->subs[a].owner) 
	{
		i = 0;
		if (a == SUB_REAL)
			p->subs[a].owner->fds[i++] = p->jfd;
		p->subs[a].owner->fds[i++] = p->subs[a].dfd;
		p->subs[a].owner->fds[i++] = -1;
	}
	if (p->subs[b].owner) 
	{
		i = 0;
		if (b == SUB_REAL)
			p->subs[b].owner->fds[i++] = p->jfd;
		p->subs[b].owner->fds[i++] = p->subs[b].dfd;
		p->subs[b].owner->fds[i++] = -1;
	}

	wakeup_sub(p, a, NULL);
	wakeup_sub(p, b, NULL);
}

static int jdsp_open_slic(int line)
{
	int fd;

	line--; /* Asterisk uses one based numbering for slics */
	fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_SLIC, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		ast_log(LOG_WARNING, "Unable to open jdsp kos char device\n");
		return -1;
	}
	
	if (ioctl(fd, VOIP_SLIC_BIND, line) < 0)
	{
		ast_log(LOG_WARNING, "Invalid line number '%d'\n", line);
		close(fd);
		return -1;
	}

	return fd;
}

static int jdsp_open_dsp(void)
{
	int fd;

	fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_DSP, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		ast_log(LOG_WARNING, "Unable to open jdsp kos char device\n");
		return -1;
	}

	return fd;
}

static void jdsp_close(int fd)
{
	if(fd > 0)
		close(fd);
}

static void jdsp_stop_audio(struct ast_channel *ast, int index);

static int unalloc_sub(struct jdsp_pvt *p, int x)
{
	if (!x) {
		ast_log(LOG_WARNING, "Trying to unalloc the real channel %d?!?\n", p->channel);
		return -1;
	}
	ast_log(LOG_DEBUG, "Released sub %d of channel %d\n", x, p->channel);
	/* This is our last chance to stop audio on this subchannel, just before
	 * we zero 'owner'.
	 * For example, this is the place where audio is stopped in the scenario
	 * where we hang up a call while there is a call waiting.
	 * XXX Should we stop it in swap_subs()? */
	if (p->subs[x].owner)
	    jdsp_stop_audio(p->subs[x].owner, x);
	p->subs[x].owner = NULL;
	p->subs[x].inthreeway = 0;
	return 0;
}

static int jdsp_digit_begin(struct ast_channel *ast, char digit)
{
	struct jdsp_pvt *p;
	int res = 0;
	int index;
	int tone;

	switch (digit)
	{
	case '#':
		tone = PHONE_TONE_DTMF_POUND;
		break;

	case '*':
		tone = PHONE_TONE_DTMF_ASTERISK;
		break;

	default:
		tone = PHONE_TONE_DTMF0 + digit - '0';
		break;
	}

	p = ast->tech_pvt;
	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(ast, p, 0);
	if ((index == SUB_REAL) && p->owner) {
		if ((res = ioctl(p->jfd, VOIP_SLIC_TONE, tone)))
			ast_log(LOG_WARNING, "Couldn't dial digit %c\n", digit);
	}
	ast_mutex_unlock(&p->lock);
	restart_monitor();
	return res;
}

static int jdsp_digit_end(struct ast_channel *ast, char digit)
{
	struct jdsp_pvt *p = ast->tech_pvt;

	if (!p)
	    return 0;

	ast_mutex_lock(&p->lock);
	ioctl(p->jfd, VOIP_SLIC_TONE, PHONE_TONE_NONE);
	p->dialing = 0;
	ast_mutex_unlock(&p->lock);
	return 0;
}

static char *jdsp_sig2str(int sig)
{
	static char buf[256];
	switch(sig) {
	case SIG_FXOLS:
		return "FXO Loopstart";
	case SIG_FXOGS:
		return "FXO Groundstart";
	default:
		snprintf(buf, sizeof(buf), "Unknown signalling %d", sig);
		return buf;
	}
	return NULL;
}

#define sig2str jdsp_sig2str

static int need_mwi_tone(struct jdsp_pvt *p)
{
	return p->mwi && (ast_app_has_voicemail(p->mailbox, "REMOTE") ||
	    ast_app_has_voicemail(p->mailbox, NULL));
}

static void prepare_cid(struct jdsp_pvt *pvt, phone_caller_id_t *cid,
    char *name, char *num)
{
	time_t tt = time(NULL);
	struct tm *tm = gmtime(&tt);

	memset(cid, 0, sizeof(phone_caller_id_t));

	if (!pvt->use_callerid)
	    return;

	snprintf(cid->time, TIME_STRING_LEN, "%.2d%.2d%.2d%.2d", tm->tm_mon + 1,
	    tm->tm_mday, tm->tm_hour, tm->tm_min);

	if (name && *name)
	    strncpy(cid->name, name, sizeof(cid->name));
	else
	    cid->name_status = PHONE_CALLER_NAME_ABSENT;

	if (num && *num)
	    strncpy(cid->number, num, sizeof(cid->number));
	else
	    cid->number_status = PHONE_CALLER_NUMBER_ABSENT;
}

static int get_distinctive_ring(struct ast_channel *ast)
{
    	char *alert_info, *c;
	int ret = 0;

	alert_info = pbx_builtin_getvar_helper(ast, "ALERTINFO");
	if (alert_info && (c = strcasestr(alert_info, "Bellcore-dr")) && *c)
	{
	    c += strlen("Bellcore-dr");
	    ret = *c - '1';
	}
	
	ast_log(LOG_DEBUG, "jdsp_callwait: distinctive_ring=%d\n", ret);
	return ret;
}

static int jdsp_callwait(struct jdsp_pvt *pvt, int start)
{
	call_params_t params;

	if (!start)
	{
		if (pvt->callwaitingalert)
		{
			pvt->callwaitingalert = 0;
			return ioctl(pvt->jfd, VOIP_SLIC_CALL_WAITING_ALERT, NULL);
		}
		return 0;
	}

	pvt->callwaitingalert = 1;

	prepare_cid(pvt, &params.cid, pvt->callwait_name, pvt->callwait_num);

	params.distinctive_ring = get_distinctive_ring(pvt->subs[SUB_CALLWAIT].owner);
	return ioctl(pvt->jfd, VOIP_SLIC_CALL_WAITING_ALERT, &params);
}

static void jdsp_ring(struct jdsp_pvt *pvt, int start)
{
    	call_params_t params;

	if (!start)
	{
		ioctl(pvt->jfd, VOIP_SLIC_RING, NULL);
		return;
	}

	if (pvt->owner)
	{
		prepare_cid(pvt, &params.cid, pvt->owner->cid.cid_name,
			pvt->owner->cid.cid_num);
	}
	else
		prepare_cid(pvt, &params.cid, NULL, NULL);
	
	params.distinctive_ring = get_distinctive_ring(pvt->owner);
	ioctl(pvt->jfd, VOIP_SLIC_RING, &params);
}

static int jdsp_off_hook_warning(void *data);

static int jdsp_play_tone(struct jdsp_pvt *p, phone_tone_t tone)
{
	if (tone == PHONE_TONE_REORDER && p->offhookwarningschedid == -1)
	{
		p->offhookwarningschedid = ast_sched_add(sched, offhookwarningtimeout,
			jdsp_off_hook_warning, p);
	}
	else if (p->offhookwarningschedid > -1 && tone != PHONE_TONE_REORDER)
	{
		ast_sched_del(sched, p->offhookwarningschedid);
		p->offhookwarningschedid = -1;
	}
	if (tone && p->play_tone)
	    ioctl(p->jfd, VOIP_SLIC_TONE, PHONE_TONE_NONE);
	p->play_tone = tone;

	return ioctl(p->jfd, VOIP_SLIC_TONE, tone);
}

static int jdsp_call(struct ast_channel *ast, char *rdest, int timeout)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	char *c, *n, *l;
	char dest[256]; /* must be same length as p->dialdest */
	ast_mutex_lock(&p->lock);
	ast_copy_string(dest, rdest, sizeof(dest));
	ast_copy_string(p->dialdest, rdest, sizeof(p->dialdest));
	if ((ast->_state == AST_STATE_BUSY)) {
		ast_mutex_unlock(&p->lock);
		return 0;
	}
	if ((ast->_state != AST_STATE_DOWN) && (ast->_state != AST_STATE_RESERVED)) {
		ast_log(LOG_WARNING, "jdsp_call called on %s, neither down nor reserved\n", ast->name);
		ast_mutex_unlock(&p->lock);
		return -1;
	}
	p->outgoing = 1;

	if (p->owner == ast) {
		/* Normal ring, on hook */
		
		/* Don't send audio while on hook, until the call is answered */
		p->dialing = 1;

		/* nick@dccinc.com 4/3/03 mods to allow for deferred dialing */
		c = strchr(dest, '/');
		if (c)
			c++;

		jdsp_ring(p, 1);
		
		p->dialing = 1;
	} else {
		/* Call waiting call */
		p->callwaitrings = 0;
		if (ast->cid.cid_num)
			ast_copy_string(p->callwait_num, ast->cid.cid_num, sizeof(p->callwait_num));
		else
			p->callwait_num[0] = '\0';
		if (ast->cid.cid_name)
			ast_copy_string(p->callwait_name, ast->cid.cid_name, sizeof(p->callwait_name));
		else
			p->callwait_name[0] = '\0';

		/* Call waiting tone instead */
		if (jdsp_callwait(p, 1)) {
			ast_mutex_unlock(&p->lock);
			return -1;
		}
	}
	n = ast->cid.cid_name;
	l = ast->cid.cid_num;
	if (l)
		ast_copy_string(p->lastcid_num, l, sizeof(p->lastcid_num));
	else
		p->lastcid_num[0] = '\0';
	if (n)
		ast_copy_string(p->lastcid_name, n, sizeof(p->lastcid_name));
	else
		p->lastcid_name[0] = '\0';
	ast_setstate(ast, AST_STATE_RINGING);
	ast_queue_control(ast, AST_CONTROL_RINGING);	
	
	ast_mutex_unlock(&p->lock);
	return 0;
}

static void destroy_jdsp_pvt(struct jdsp_pvt **pvt)
{
	struct jdsp_pvt *p = *pvt;
	/* Remove channel from the list */
	if(p->prev)
		p->prev->next = p->next;
	if(p->next)
		p->next->prev = p->prev;
	ast_mutex_destroy(&p->lock);
	free(p);
	*pvt = NULL;
}

static int destroy_channel(struct jdsp_pvt *prev, struct jdsp_pvt *cur, int now)
{
	int owned = 0;
	int i = 0;

	if (!now) {
		if (cur->owner) {
			owned = 1;
		}

		for (i = 0; i < 3; i++) {
			if (cur->subs[i].owner) {
				owned = 1;
			}
		}
		if (!owned) {
			if (prev) {
				prev->next = cur->next;
				if (prev->next)
					prev->next->prev = prev;
				else
					ifend = prev;
			} else {
				iflist = cur->next;
				if (iflist)
					iflist->prev = NULL;
				else
					ifend = NULL;
			}
			if (cur->jfd > -1) {
				jdsp_close(cur->jfd);
			}
			destroy_jdsp_pvt(&cur);
		}
	} else {
		if (prev) {
			prev->next = cur->next;
			if (prev->next)
				prev->next->prev = prev;
			else
				ifend = prev;
		} else {
			iflist = cur->next;
			if (iflist)
				iflist->prev = NULL;
			else
				ifend = NULL;
		}
		if (cur->jfd > -1) {
			jdsp_close(cur->jfd);
		}
		destroy_jdsp_pvt(&cur);
	}
	return 0;
}

static void jdsp_stop_audio(struct ast_channel *ast, int index);

static void jdsp_start_audio(struct ast_channel *ast, int index)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int otherindex;
	voip_dsp_record_args_t args;
	voip_dsp_bind_arg_t bind_arg;

	ast_log(LOG_DEBUG, "Starting audio on channel %s\n", p->owner->name);
	
	if (p->subs[index].chan != -1)
		jdsp_stop_audio(ast, index);

	if (ast_bridged_channel(ast) && ast_bridged_channel(ast)->tech->get_rtp)
	    p->subs[index].rtp = ast_bridged_channel(ast)->tech->get_rtp(ast_bridged_channel(ast));

	/* We assume that voice may be started only for SUB_REAL and SUB_THREEWAY */
	otherindex = index == SUB_REAL ? SUB_THREEWAY : SUB_REAL;
	args.channel = p->subs[otherindex].chan == 0 ? 1 : 0;
	switch (p->faxtxmethod) {
	case FAX_TX_NONE:
	    args.data_mode = VOIP_DATA_MODE_VOICE;
	    break;
	case FAX_TX_T38_AUTO:
	    args.data_mode = p->faxdetected ? VOIP_DATA_MODE_T38 :
		VOIP_DATA_MODE_VOICE;
	    break;
	case FAX_TX_PASSTHROUGH_AUTO:
	    args.data_mode = p->faxdetected ? VOIP_DATA_MODE_FAX :
		VOIP_DATA_MODE_VOICE;
	    break;
	case FAX_TX_PASSTHROUGH_FORCE:
	    args.data_mode = VOIP_DATA_MODE_FAX;
	    break;
	}

	/* For T.38 packets we don't use the jdsp->jrtp fastpath because jrtp
	 * is not designed to handle these kinds of packets. So don't bind the
	 * jdsp to a jrtp session in this case. */
	if (p->subs[index].rtp && args.data_mode != VOIP_DATA_MODE_T38)
	{
		ast_rtp_set_formats(p->subs[index].rtp, ast->readformat, ast->writeformat);
		args.rtp_context = ast_rtp_get_context(p->subs[index].rtp);
		args.codec = ast_rtp_lookup_code(p->subs[index].rtp, 1, ast->readformat);
		args.suppress_dtmf = args.data_mode != VOIP_DATA_MODE_FAX &&
		    !ast_rtp_get_inband_dtmf(p->subs[index].rtp);
	}
	else
	{
		args.rtp_context = 0;
		args.codec = jdsp_codec_ast2rtp(ast->readformat);
		args.suppress_dtmf = 1;
	}
	args.ptime_ms = ast->ptime ? ast->ptime : 20;

	if (p->subs[index].dfd == -1)
	{
		ast_log(LOG_ERROR, "It's impossible - Starting audio on channel %s, index %d\n",
			ast->name, index);
	}
	else
	{
		bind_arg.line = p->channel - 1;
		bind_arg.channel = args.channel;
		ioctl(p->subs[index].dfd, VOIP_DSP_BIND, &bind_arg);
	}

	if (ioctl(p->jfd, VOIP_DSP_START, &args) < 0) {
		ast_log(LOG_ERROR, "Failed to start audio on channel %s\n", ast->name);
		return;
	}

	p->subs[index].chan = args.channel;
	p->subs[index].seqno = 0;
}

static void jdsp_stop_audio(struct ast_channel *ast, int index)
{
	struct jdsp_pvt *p = ast->tech_pvt;

	ast_log(LOG_DEBUG, "Stopping audio on channel %s\n", ast->name);
	
	if (p->subs[index].rtp)
	{
	    ast_rtp_set_formats(p->subs[index].rtp, 0, 0);
	    p->subs[index].rtp = NULL;
	}
	
	/* This is not error case. jdsp_stop_audio may be called when voice is not
	 * started */
	if (p->subs[index].chan == -1)
	{
	    ast_log(LOG_DEBUG, "Can't stop audio on channel %s, "
			"no dsp channel\n", ast->name);
		return;
	}

	if (ioctl(p->jfd, VOIP_DSP_STOP, p->subs[index].chan) < 0)
	{
		ast_log(LOG_ERROR, "Failed to stop audio on channel %s, fd %d\n",
			ast->name, p->jfd);
	}
	p->subs[index].chan = -1;
}

static void jdsp_moh_stop(struct jdsp_pvt *p, int index)
{
    if (internalmoh)
		ast_moh_stop(ast_bridged_channel(p->subs[index].owner));
    else
    {
		ast_indicate(ast_bridged_channel(p->subs[index].owner),
			AST_CONTROL_UNHOLD);
    }
}

static void jdsp_moh_start(struct jdsp_pvt *p, int index)
{
    if (internalmoh)
		ast_moh_start(ast_bridged_channel(p->subs[index].owner), NULL);
    else
	{
		ast_indicate(ast_bridged_channel(p->subs[index].owner),
			AST_CONTROL_HOLD);
	}
}

static int jdsp_off_hook_warning(void *data)
{
	struct jdsp_pvt *p = data;

	ast_mutex_lock(&p->lock);
	jdsp_play_tone(p, PHONE_TONE_HOOK_OFF);
	ast_mutex_unlock(&p->lock);
	
	return 0;
}

static int jdsp_hangup(struct ast_channel *ast)
{
	int index, x;
	struct jdsp_pvt *p = ast->tech_pvt;
	struct jdsp_pvt *tmp = NULL;
	struct jdsp_pvt *prev = NULL;

	if (option_debug)
		ast_log(LOG_DEBUG, "jdsp_hangup(%s)\n", ast->name);
	if (!ast->tech_pvt) {
		ast_log(LOG_WARNING, "Asked to hangup channel not connected\n");
		return 0;
	}
	
	ast_mutex_lock(&p->lock);
	
	index = jdsp_get_index(ast, p, 1);

	jdsp_stop_audio(ast, index);
	x = 0;

	if (p->origcid_num) {
		ast_copy_string(p->cid_num, p->origcid_num, sizeof(p->cid_num));
		free(p->origcid_num);
		p->origcid_num = NULL;
	}	
	if (p->origcid_name) {
		ast_copy_string(p->cid_name, p->origcid_name, sizeof(p->cid_name));
		free(p->origcid_name);
		p->origcid_name = NULL;
	}	
	if (p->exten)
		p->exten[0] = '\0';

	ast_log(LOG_DEBUG, "Hangup: channel: %d, index = %d, normal = %p, callwait = %p, thirdcall = %pd\n",
		p->channel, index, p->subs[SUB_REAL].owner, p->subs[SUB_CALLWAIT].owner, p->subs[SUB_THREEWAY].owner);
	
	if (index > -1) {
		/* Real channel, do some fixup */
		p->subs[index].owner = NULL;
		p->subs[index].needanswer = 0;

		if (index == SUB_REAL) {
			if ((p->subs[SUB_CALLWAIT].owner) && (p->subs[SUB_THREEWAY].owner)) {
				ast_log(LOG_DEBUG, "Normal call hung up with both three way call and a call waiting call in place?\n");
				if (p->subs[SUB_CALLWAIT].inthreeway) {
					/* We had flipped over to answer a callwait and now it's gone */
					ast_log(LOG_DEBUG, "We were flipped over to the callwait, moving back and unowning.\n");
					/* Move to the call-wait, but un-own us until they flip back. */
					swap_subs(p, SUB_CALLWAIT, SUB_REAL);
					unalloc_sub(p, SUB_CALLWAIT);
					p->owner = NULL;
				} else {
					/* The three way hung up, but we still have a call wait */
					ast_log(LOG_DEBUG, "We were in the threeway and have a callwait still.  Ditching the threeway.\n");
					swap_subs(p, SUB_THREEWAY, SUB_REAL);
					unalloc_sub(p, SUB_THREEWAY);
					if (p->subs[SUB_REAL].inthreeway) {
						/* This was part of a three way call.  Immediately make way for
						   another call */
						ast_log(LOG_DEBUG, "Call was complete, setting owner to former third call\n");
						p->owner = p->subs[SUB_REAL].owner;
					} else {
						/* This call hasn't been completed yet...  Set owner to NULL */
						ast_log(LOG_DEBUG, "Call was incomplete, setting owner to NULL\n");
						p->owner = NULL;
					}
					p->subs[SUB_REAL].inthreeway = 0;
				}
			} else if (p->subs[SUB_CALLWAIT].owner) {
				/* Move to the call-wait and switch back to them. */
				swap_subs(p, SUB_CALLWAIT, SUB_REAL);
				unalloc_sub(p, SUB_CALLWAIT);
				p->owner = p->subs[SUB_REAL].owner;
				if (p->owner->_state != AST_STATE_UP)
				{
				        ast_setstate(p->owner, AST_STATE_UP);
					p->subs[SUB_REAL].needanswer = 1;
				}

				if (ast_bridged_channel(p->subs[SUB_REAL].owner))
				{
					jdsp_moh_stop(p, SUB_REAL);
					jdsp_start_audio(p->subs[SUB_REAL].owner, SUB_REAL); /* XXX Need to check whether this line is needed */
				}

				jdsp_callwait(p, 0);
			} else if (p->subs[SUB_THREEWAY].owner) {
				swap_subs(p, SUB_THREEWAY, SUB_REAL);
				unalloc_sub(p, SUB_THREEWAY);
				if (p->subs[SUB_REAL].inthreeway) {
					/* This was part of a three way call.  Immediately make way for
					   another call */
					ast_log(LOG_DEBUG, "Call was complete, setting owner to former third call\n");
					p->owner = p->subs[SUB_REAL].owner;
				} else {
					/* This call hasn't been completed yet...  Set owner to NULL */
					ast_log(LOG_DEBUG, "Call was incomplete, setting owner to NULL\n");
					p->owner = NULL;
					jdsp_play_tone(p, PHONE_TONE_REORDER);
				}
				p->subs[SUB_REAL].inthreeway = 0;
			}
		} else if (index == SUB_CALLWAIT) {
			/* Ditch the holding callwait call, and immediately make it availabe */
			if (p->subs[SUB_CALLWAIT].inthreeway) {
				/* This is actually part of a three way, placed on hold.  Place the third part
				   on music on hold now */

				if (p->subs[SUB_THREEWAY].owner && ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
				{
					jdsp_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY); /* XXX Need to check whether this line is needed */
					jdsp_moh_start(p, SUB_THREEWAY);
				}

				p->subs[SUB_THREEWAY].inthreeway = 0;
				/* Make it the call wait now */
				swap_subs(p, SUB_CALLWAIT, SUB_THREEWAY);
				unalloc_sub(p, SUB_THREEWAY);
			} else
				unalloc_sub(p, SUB_CALLWAIT);
		} else if (index == SUB_THREEWAY) {
			if (p->subs[SUB_CALLWAIT].inthreeway) {
				/* The other party of the three way call is currently in a call-wait state.
				   Start music on hold for them, and take the main guy out of the third call */

				if (p->subs[SUB_CALLWAIT].owner && ast_bridged_channel(p->subs[SUB_CALLWAIT].owner))
				{
					jdsp_stop_audio(p->subs[SUB_CALLWAIT].owner, SUB_CALLWAIT); /* XXX Need to check whether this line is needed */
					jdsp_moh_start(p, SUB_CALLWAIT);
				}

				p->subs[SUB_CALLWAIT].inthreeway = 0;
			}
			p->subs[SUB_REAL].inthreeway = 0;
			/* If this was part of a three way call index, let us make
			   another three way call */
			unalloc_sub(p, SUB_THREEWAY);
		} else {
			/* This wasn't any sort of call, but how are we an index? */
			ast_log(LOG_WARNING, "Index found but not any type of call?\n");
		}
	}


	if (!p->subs[SUB_REAL].owner && !p->subs[SUB_CALLWAIT].owner && !p->subs[SUB_THREEWAY].owner) {
	    	int res, rxisoffhook;

		p->owner = NULL;
		p->distinctivering = 0;
		p->confirmanswer = 0;
		p->outgoing = 0;
		p->onhooktime = time(NULL);
		p->faxdetected = 0;

		res = ioctl(p->jfd, VOIP_SLIC_GET_HOOK, &rxisoffhook);
		if (!res) {
			/* If they're off hook, try playing congestion */
			if (rxisoffhook)
				jdsp_play_tone(p, PHONE_TONE_REORDER);
			else
				jdsp_ring(p, 0);
		}
	}


	jdsp_callwait(p, 0);
	ast->tech_pvt = NULL;
	ast_mutex_unlock(&p->lock);
	ast_mutex_lock(&usecnt_lock);
	usecnt--;
	if (usecnt < 0) 
		ast_log(LOG_WARNING, "Usecnt < 0???\n");
	ast_mutex_unlock(&usecnt_lock);
	ast_update_use_count();
	if (option_verbose > 2) 
		ast_verbose( VERBOSE_PREFIX_3 "Hungup '%s'\n", ast->name);

	ast_mutex_lock(&iflock);
	tmp = iflist;
	prev = NULL;
	ast_mutex_unlock(&iflock);
	return 0;
}

static int jdsp_answer(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int res = 0;
	int index;
	int oldstate = ast->_state;
	ast_setstate(ast, AST_STATE_UP);
	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(ast, p, 0);
	if (index < 0)
		index = SUB_REAL;

	/* Pick up the line */
	ast_log(LOG_DEBUG, "Took %s off hook\n", ast->name);
	jdsp_play_tone(p, PHONE_TONE_NONE);
	p->dialing = 0;
	if ((index == SUB_REAL) && p->subs[SUB_THREEWAY].inthreeway) {
		if (oldstate == AST_STATE_RINGING) {
			ast_log(LOG_DEBUG, "Finally swapping real and threeway\n");
			swap_subs(p, SUB_THREEWAY, SUB_REAL);
			p->owner = p->subs[SUB_REAL].owner;
			index = SUB_THREEWAY;
		}
	}
	jdsp_start_audio(p->subs[index].owner, index);

	ast_mutex_unlock(&p->lock);
	return res;
}

static void jdsp_unlink(struct jdsp_pvt *slave, struct jdsp_pvt *master, int needlock)
{
	/* Unlink a specific slave or all slaves/masters from a given master */
	int x;
	int hasslaves;
	if (!master)
		return;
	if (needlock) {
		ast_mutex_lock(&master->lock);
		if (slave) {
			while(ast_mutex_trylock(&slave->lock)) {
				ast_mutex_unlock(&master->lock);
				usleep(1);
				ast_mutex_lock(&master->lock);
			}
		}
	}
	hasslaves = 0;
	for (x = 0; x < MAX_SLAVES; x++) {
		if (master->slaves[x]) {
			if (!slave || (master->slaves[x] == slave)) {
				/* Take slave out of the conference */
				ast_log(LOG_DEBUG, "Unlinking slave %d from %d\n", master->slaves[x]->channel, master->channel);
				master->slaves[x]->master = NULL;
				master->slaves[x] = NULL;
			} else
				hasslaves = 1;
		}
		if (!hasslaves)
			master->inconference = 0;
	}
	if (!slave) {
		if (master->master) {
			/* Take master out of the conference */
			hasslaves = 0;
			for (x = 0; x < MAX_SLAVES; x++) {
				if (master->master->slaves[x] == master)
					master->master->slaves[x] = NULL;
				else if (master->master->slaves[x])
					hasslaves = 1;
			}
			if (!hasslaves)
				master->master->inconference = 0;
		}
		master->master = NULL;
	}
	if (needlock) {
		if (slave)
			ast_mutex_unlock(&slave->lock);
		ast_mutex_unlock(&master->lock);
	}
}

static int jdsp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct jdsp_pvt *p = newchan->tech_pvt;
	int x;
	ast_mutex_lock(&p->lock);
	ast_log(LOG_DEBUG, "New owner for channel %d is %s\n", p->channel, newchan->name);
	if (p->owner == oldchan) {
		p->owner = newchan;
	}
	for (x = 0; x < 3; x++)
		if (p->subs[x].owner == oldchan) {
			if (!x)
				jdsp_unlink(NULL, p, 0);
			p->subs[x].owner = newchan;
		}
	if (newchan->_state == AST_STATE_RINGING) 
		jdsp_indicate(newchan, AST_CONTROL_RINGING);
	ast_mutex_unlock(&p->lock);
	return 0;
}

static void *ss_thread(void *data);

static struct ast_channel *jdsp_new(struct jdsp_pvt *, int, int, int, int);

static int attempt_transfer(struct jdsp_pvt *p)
{
	/* In order to transfer, we need at least one of the channels to
	   actually be in a call bridge.  We can't conference two applications
	   together (but then, why would we want to?) */
	if (ast_bridged_channel(p->subs[SUB_REAL].owner)) {
		/* The three-way person we're about to transfer to could still be in MOH, so
		   stop if now if appropriate */
		if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
			jdsp_moh_stop(p, SUB_THREEWAY);
		if (p->subs[SUB_THREEWAY].owner->_state == AST_STATE_RINGING) {
			ast_indicate(ast_bridged_channel(p->subs[SUB_REAL].owner), AST_CONTROL_RINGING);
		}
		if (p->subs[SUB_REAL].owner->cdr) {
			/* Move CDR from second channel to current one */
			p->subs[SUB_THREEWAY].owner->cdr =
				ast_cdr_append(p->subs[SUB_THREEWAY].owner->cdr, p->subs[SUB_REAL].owner->cdr);
			p->subs[SUB_REAL].owner->cdr = NULL;
		}
		if (ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr) {
			/* Move CDR from second channel's bridge to current one */
			p->subs[SUB_THREEWAY].owner->cdr =
				ast_cdr_append(p->subs[SUB_THREEWAY].owner->cdr, ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr);
			ast_bridged_channel(p->subs[SUB_REAL].owner)->cdr = NULL;
		}
		/* Last chance to stop audio on the channel before it becomes a
		 * zombie */
		jdsp_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY);
		 if (ast_channel_masquerade(p->subs[SUB_THREEWAY].owner, ast_bridged_channel(p->subs[SUB_REAL].owner))) {
			ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
					ast_bridged_channel(p->subs[SUB_REAL].owner)->name, p->subs[SUB_THREEWAY].owner->name);
			return -1;
		}
		/* Orphan the channel after releasing the lock */
		ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
		unalloc_sub(p, SUB_THREEWAY);
	} else if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner)) {
		if (p->subs[SUB_REAL].owner->_state == AST_STATE_RINGING) {
			ast_indicate(ast_bridged_channel(p->subs[SUB_THREEWAY].owner), AST_CONTROL_RINGING);
		}
		jdsp_moh_stop(p, SUB_THREEWAY);
		if (p->subs[SUB_THREEWAY].owner->cdr) {
			/* Move CDR from second channel to current one */
			p->subs[SUB_REAL].owner->cdr = 
				ast_cdr_append(p->subs[SUB_REAL].owner->cdr, p->subs[SUB_THREEWAY].owner->cdr);
			p->subs[SUB_THREEWAY].owner->cdr = NULL;
		}
		if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->cdr) {
			/* Move CDR from second channel's bridge to current one */
			p->subs[SUB_REAL].owner->cdr = 
				ast_cdr_append(p->subs[SUB_REAL].owner->cdr, ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->cdr);
			ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->cdr = NULL;
		}
		/* Last chance to stop audio on the channel before it becomes a
		 * zombie */
		jdsp_stop_audio(p->subs[SUB_REAL].owner, SUB_REAL);
		if (ast_channel_masquerade(p->subs[SUB_REAL].owner, ast_bridged_channel(p->subs[SUB_THREEWAY].owner))) {
			ast_log(LOG_WARNING, "Unable to masquerade %s as %s\n",
					ast_bridged_channel(p->subs[SUB_THREEWAY].owner)->name, p->subs[SUB_REAL].owner->name);
			return -1;
		}
		/* Three-way is now the REAL */
		swap_subs(p, SUB_THREEWAY, SUB_REAL);
		ast_mutex_unlock(&p->subs[SUB_REAL].owner->lock);
		unalloc_sub(p, SUB_THREEWAY);
		/* Tell the caller not to hangup */
		return 1;
	} else {
		ast_log(LOG_DEBUG, "Neither %s nor %s are in a bridge, nothing to transfer\n",
					p->subs[SUB_REAL].owner->name, p->subs[SUB_THREEWAY].owner->name);
		p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
		return -1;
	}
	return 0;
}

static void start_conference(struct ast_channel *ast, struct jdsp_pvt *p)
{
	int otherindex = SUB_THREEWAY;

	if (option_verbose > 2)
	{
		ast_verbose(VERBOSE_PREFIX_3 "Building conference on call on %s and %s"
			    		    "\n", p->subs[SUB_THREEWAY].owner->name, 
	    	p->subs[SUB_REAL].owner->name);
    	}
    	/* Put them in the threeway, and flip */
    	p->subs[SUB_THREEWAY].inthreeway = 1;
    	p->subs[SUB_REAL].inthreeway = 1;
    	if (ast->_state == AST_STATE_UP) {
		swap_subs(p, SUB_THREEWAY, SUB_REAL);
		otherindex = SUB_REAL;
    	}
    	else
		jdsp_play_tone(p, PHONE_TONE_NONE);

    	if (p->subs[otherindex].owner &&
	    	ast_bridged_channel(p->subs[otherindex].owner))
    	{
		jdsp_moh_stop(p, otherindex);
		jdsp_start_audio(p->subs[otherindex].owner, otherindex); /* XXX Need to check whether this line is needed */
    	}

    	p->owner = p->subs[SUB_REAL].owner;
    	if (ast->_state == AST_STATE_RINGING) {
		ast_log(LOG_DEBUG, "Enabling ringtone on real and threeway\n");
			jdsp_play_tone(p, PHONE_TONE_RING);
	}
}

static void drop_last_party_from_conference(struct jdsp_pvt *p)
{
	/* Call is already up, drop the last person */
	if (option_debug)
		ast_log(LOG_DEBUG, "Got flash with three way call up, dropping last call on %d\n", p->channel);
	/* If the primary call isn't answered yet, use it */
	if ((p->subs[SUB_REAL].owner->_state != AST_STATE_UP) && (p->subs[SUB_THREEWAY].owner->_state == AST_STATE_UP)) {
		/* Swap back -- we're dropping the real 3-way that isn't finished yet*/
		swap_subs(p, SUB_THREEWAY, SUB_REAL);
		p->owner = p->subs[SUB_REAL].owner;
	}
	/* Drop the last call and stop the conference */
	if (option_verbose > 2)
		ast_verbose(VERBOSE_PREFIX_3 "Dropping three-way call on %s\n", p->subs[SUB_THREEWAY].owner->name);
	jdsp_play_tone(p, PHONE_TONE_NONE);
	p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
	p->subs[SUB_REAL].inthreeway = 0;
	p->subs[SUB_THREEWAY].inthreeway = 0;
}

static void switch_calls(struct jdsp_pvt *p, int is_callwaiting)
{
    	int other_sub = is_callwaiting ? SUB_CALLWAIT : SUB_THREEWAY;

	swap_subs(p, SUB_REAL, other_sub);
	if (is_callwaiting)
		jdsp_callwait(p, 0);
	p->owner = p->subs[SUB_REAL].owner;
	ast_log(LOG_DEBUG, "Making %s the new owner\n", p->owner->name);
	if (is_callwaiting && p->owner->_state == AST_STATE_RINGING) {
		ast_setstate(p->owner, AST_STATE_UP);
		jdsp_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_ANSWER);
	}

    	/* Start music on hold if appropriate */
	if (!p->subs[other_sub].inthreeway && ast_bridged_channel(p->subs[other_sub].owner))
	{
		jdsp_stop_audio(p->subs[other_sub].owner, other_sub);
		jdsp_moh_start(p, other_sub);
	}
    	if (ast_bridged_channel(p->subs[SUB_REAL].owner))
    	{
		jdsp_moh_stop(p, SUB_REAL);
		jdsp_start_audio(p->subs[SUB_REAL].owner, SUB_REAL);
    	}
}

static void start_threeway_call(struct jdsp_pvt *p)
{
	struct ast_channel *chan;
	int res;
	pthread_t threadid;
	pthread_attr_t attr;
	char cid_num[256];
	char cid_name[256];

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (p->jdsptrcallerid && p->owner) {
		if (p->owner->cid.cid_num)
			ast_copy_string(cid_num, p->owner->cid.cid_num, sizeof(cid_num));
		if (p->owner->cid.cid_name)
			ast_copy_string(cid_name, p->owner->cid.cid_name, sizeof(cid_name));
	}

	/* Make new channel */
	chan = jdsp_new(p, AST_STATE_RESERVED, 0, SUB_THREEWAY, 0);
	if (p->jdsptrcallerid) {
		if (!p->origcid_num)
			p->origcid_num = strdup(p->cid_num);
		if (!p->origcid_name)
			p->origcid_name = strdup(p->cid_name);
		ast_copy_string(p->cid_num, cid_num, sizeof(p->cid_num));
		ast_copy_string(p->cid_name, cid_name, sizeof(p->cid_name));
	}
	/* Swap things around between the three-way and real call */
	swap_subs(p, SUB_THREEWAY, SUB_REAL);
	res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
	if (res)
		ast_log(LOG_WARNING, "Unable to start dial recall tone on channel %d\n", p->channel);
	p->owner = chan;
	if (!chan) {
		ast_log(LOG_WARNING, "Cannot allocate new structure on channel %d\n", p->channel);
	} else if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) {
		ast_log(LOG_WARNING, "Unable to start simple switch on channel %d\n", p->channel);
		res = jdsp_play_tone(p, PHONE_TONE_REORDER);
		ast_hangup(chan);
	} else {
		if (option_verbose > 2)	
			ast_verbose(VERBOSE_PREFIX_3 "Started three way call on channel %d\n", p->channel);

		/* Start music on hold if appropriate */
		if (ast_bridged_channel(p->subs[SUB_THREEWAY].owner))
		{
			jdsp_stop_audio(p->subs[SUB_THREEWAY].owner, SUB_THREEWAY); /* XXX Need to check whether this line is needed */
			jdsp_moh_start(p, SUB_THREEWAY);
		}
		else
			ast_queue_hangup(p->subs[SUB_THREEWAY].owner);
	}		
}

static struct ast_frame *jdsp_handle_event(struct ast_channel *ast)
{
	int index;
	struct jdsp_pvt *p = ast->tech_pvt;
	phone_event_t ev;
	char dtmf_key;
	int res;

	index = jdsp_get_index(ast, p, 0);
	p->subs[index].f.frametype = AST_FRAME_NULL;
	p->subs[index].f.datalen = 0;
	p->subs[index].f.samples = 0;
	p->subs[index].f.mallocd = 0;
	p->subs[index].f.offset = 0;
	p->subs[index].f.src = __FUNCTION__;
	p->subs[index].f.data = NULL;
	if (index < 0)
		return &p->subs[index].f;
	if (jdsp_get_event(p->jfd, &ev) < 0)
	{
	    ast_log(LOG_WARNING, "No event\n");
	    return NULL;
	}

	ast_log(LOG_DEBUG, "Got event \"%s\" on channel %d (index %d)\n", jdsp_event2str(&ev), p->channel, index);
	
	switch(ev.key) {
	case PHONE_KEY_0:
	case PHONE_KEY_1:
	case PHONE_KEY_2:
	case PHONE_KEY_3:
	case PHONE_KEY_4:
	case PHONE_KEY_5:
	case PHONE_KEY_6:
	case PHONE_KEY_7:
	case PHONE_KEY_8:
	case PHONE_KEY_9:
	case PHONE_KEY_ASTERISK:
	case PHONE_KEY_POUND:
		dtmf_key = jdsp_key2char(ev.key);
		if (ev.pressed)
		{
			ast_log(LOG_DEBUG, "DTMF Down '%c'\n", dtmf_key);
			p->subs[index].f.frametype = AST_FRAME_DTMF_BEGIN;
			p->subs[index].f.subclass = dtmf_key;
			return &p->subs[index].f;
		}
		else
		{
			ast_log(LOG_DEBUG, "DTMF Up '%c'\n", dtmf_key);
			p->subs[index].f.frametype = AST_FRAME_DTMF_END;
			p->subs[index].f.subclass = dtmf_key;
			return &p->subs[index].f;
		}
	case PHONE_KEY_FAX_CNG:
		if (p->faxdetected || p->faxtxmethod == FAX_TX_NONE ||
		    p->faxtxmethod == FAX_TX_PASSTHROUGH_FORCE) {
		    break;
		}
		
		p->faxdetected = 1;
		jdsp_stop_audio(ast, SUB_REAL);
		jdsp_start_audio(ast, SUB_REAL);
		jdsp_queue_control(p->subs[SUB_REAL].owner, p->faxtxmethod ==
		    FAX_TX_PASSTHROUGH_AUTO ? AST_CONTROL_FAX :
		    AST_CONTROL_T38);
		break;
	case PHONE_KEY_HOOK_ON:
		p->onhooktime = time(NULL);
		/* Check for some special conditions regarding call waiting */
		if (index == SUB_REAL) {
			jdsp_play_tone(p, PHONE_TONE_NONE);
			/* The normal line was hung up */
			if (p->subs[SUB_CALLWAIT].owner) {
				/* There's a call waiting call, so ring the phone, but make it unowned in the mean time */
				swap_subs(p, SUB_CALLWAIT, SUB_REAL);
				if (option_verbose > 2) 
					ast_verbose(VERBOSE_PREFIX_3 "Channel %d still has (callwait) call, ringing phone\n", p->channel);
				unalloc_sub(p, SUB_CALLWAIT);	
				jdsp_callwait(p, 0);
				p->owner = NULL;
				/* Don't start streaming audio yet if the incoming call isn't up yet */
				if (p->subs[SUB_REAL].owner->_state != AST_STATE_UP)
					p->dialing = 1;
				jdsp_ring(p, 1);
			} else if (p->subs[SUB_THREEWAY].owner) {
				unsigned int mssinceflash;
				/* Here we have to retain the lock on both the main channel, the 3-way channel, and
				   the private structure -- not especially easy or clean */
				while(p->subs[SUB_THREEWAY].owner && ast_mutex_trylock(&p->subs[SUB_THREEWAY].owner->lock)) {
					/* Yuck, didn't get the lock on the 3-way, gotta release everything and re-grab! */
					ast_mutex_unlock(&p->lock);
					ast_mutex_unlock(&ast->lock);
					usleep(1);
					/* We can grab ast and p in that order, without worry.  We should make sure
					   nothing seriously bad has happened though like some sort of bizarre double
					   masquerade! */
					ast_mutex_lock(&ast->lock);
					ast_mutex_lock(&p->lock);
					if (p->owner != ast) {
						ast_log(LOG_WARNING, "This isn't good...\n");
						return NULL;
					}
				}
				if (!p->subs[SUB_THREEWAY].owner) {
					ast_log(LOG_NOTICE, "Whoa, threeway disappeared kinda randomly.\n");
					return NULL;
				}
				mssinceflash = ast_tvdiff_ms(ast_tvnow(), p->flashtime);
				ast_log(LOG_DEBUG, "Last flash was %d ms ago\n", mssinceflash);
				if (mssinceflash < MIN_MS_SINCE_FLASH) {
					/* It hasn't been long enough since the last flashook.  This is probably a bounce on 
					   hanging up.  Hangup both channels now */
					if (p->subs[SUB_THREEWAY].owner)
						ast_queue_hangup(p->subs[SUB_THREEWAY].owner);
					p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
					ast_log(LOG_DEBUG, "Looks like a bounced flash, hanging up both calls on %d\n", p->channel);
					ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
				} else if ((ast->pbx) || (ast->_state == AST_STATE_UP)) {
					if (p->transfermode != TRANSFER_OFF) {
					    	int inthreeway = p->subs[SUB_REAL].inthreeway && p->subs[SUB_THREEWAY].inthreeway;

						/* In any case this isn't a threeway call anymore */
						p->subs[SUB_REAL].inthreeway = 0;
						p->subs[SUB_THREEWAY].inthreeway = 0;

						/* Only attempt transfer if the phone is ringing; why transfer to busy tone eh? */
						if (!p->transfertobusy && ast->_state == AST_STATE_BUSY) {
							ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
							/* Swap subs and dis-own channel */
							swap_subs(p, SUB_THREEWAY, SUB_REAL);
							p->owner = NULL;
							/* Ring the phone */
							jdsp_ring(p, 1);
						} else if (p->transfermode == TRANSFER_BRIDGING) {
							if ((res = attempt_transfer(p)) < 0) {
								p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
								if (p->subs[SUB_THREEWAY].owner)
									ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
							} else if (res) {
								/* Don't actually hang up at this point */
								if (p->subs[SUB_THREEWAY].owner)
									ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
								break;
							}
						} else if (p->transfermode == TRANSFER_SIGNALLING) {
							struct ast_frame f;
							/* XXX If transfer-target hasn't answered yet, 'target_chan' will be
							 * NULL and transfer will not be performed. We should add code that
							 * knows how to obtain 'target_chan' in this scenario, and perform
							 * blind transfer. */
							struct ast_channel *target_chan;
							int unlock_sub_index = SUB_THREEWAY;

							/* If we are in a confernce call, undo the swap performed by
							 * start_conference. This is done in order to prevent the problem
							 * described in B39277. */
							if (inthreeway) {
								swap_subs(p, SUB_THREEWAY, SUB_REAL);
								unlock_sub_index = SUB_REAL;
							}
							
							target_chan = ast_bridged_channel(p->subs[SUB_REAL].owner);
							if (target_chan) {
								/* Tell bridged channel to do
								 * attended transfer */
								memset(&f, 0 , sizeof(f));
								f.frametype = AST_FRAME_ATTENDEDTRANSFER;
								f.data = &target_chan;
								f.datalen = sizeof(struct ast_channel *);
								ast_queue_frame(p->subs[SUB_THREEWAY].owner, &f);
								if (p->subs[unlock_sub_index].owner)
								    	ast_mutex_unlock(&p->subs[unlock_sub_index].owner->lock);

								p->subs[index].f.frametype = AST_FRAME_NULL;
								p->subs[index].f.subclass = 0;

								return &p->subs[index].f;
							} else {
								p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
								if (p->subs[unlock_sub_index].owner)
									ast_mutex_unlock(&p->subs[unlock_sub_index].owner->lock);
							}
						}
					} else {
						p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
						if (p->subs[SUB_THREEWAY].owner)
							ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
					}
				} else {
					ast_mutex_unlock(&p->subs[SUB_THREEWAY].owner->lock);
					/* Swap subs and dis-own channel */
					swap_subs(p, SUB_THREEWAY, SUB_REAL);
					p->owner = NULL;
					/* Ring the phone */
					jdsp_ring(p, 1);
				}
			}
		} else {
			ast_log(LOG_WARNING, "Got a hangup and my index is %d?\n", index);
		}
		
		return NULL;
		break;

	case PHONE_KEY_HOOK_OFF:
		switch(ast->_state) {
		case AST_STATE_RINGING:
			p->subs[index].f.frametype = AST_FRAME_CONTROL;
			p->subs[index].f.subclass = AST_CONTROL_ANSWER;
			/* Make sure it stops ringing */
			jdsp_ring(p, 0);
			ast_log(LOG_DEBUG, "channel %d answered\n", p->channel);
			p->dialing = 0;
			if (p->confirmanswer) {
				/* Ignore answer if "confirm answer" is enabled */
				p->subs[index].f.frametype = AST_FRAME_NULL;
				p->subs[index].f.subclass = 0;
			} else {
				ast_setstate(ast, AST_STATE_UP);
			}
			return &p->subs[index].f;
		case AST_STATE_DOWN:
			ast_setstate(ast, AST_STATE_RING);
			ast->rings = 1;
			p->subs[index].f.frametype = AST_FRAME_CONTROL;
			p->subs[index].f.subclass = AST_CONTROL_OFFHOOK;
			ast_log(LOG_DEBUG, "channel %d picked up\n", p->channel);
			return &p->subs[index].f;
		case AST_STATE_UP:
			/* Make sure it stops ringing */
			jdsp_ring(p, 0);
			/* Okay -- probably call waiting*/

			if (ast_bridged_channel(p->owner))
			{
				jdsp_moh_stop(p, index);
				jdsp_start_audio(p->subs[index].owner, index); /* XXX Need to check whether this line is needed */
			}

			break;
		case AST_STATE_RESERVED:
			/* Start up dialtone */
			if (need_mwi_tone(p))
				res = jdsp_play_tone(p, PHONE_TONE_MWI);
			else if (p->stutterdialtone)
				res = jdsp_play_tone(p, PHONE_TONE_STUTTER_DIAL);
			else
				res = jdsp_play_tone(p, PHONE_TONE_DIAL);
			break;
		default:
			ast_log(LOG_WARNING, "FXO phone off hook in weird state %d??\n", ast->_state);
		}
		break;
	case PHONE_KEY_FLASH:
		/* Remember last time we got a flash-hook */
		gettimeofday(&p->flashtime, NULL);
		ast_log(LOG_DEBUG, "Winkflash, index: %d, normal: %p, callwait: %p, thirdcall: %p\n",
			index, p->subs[SUB_REAL].owner, p->subs[SUB_CALLWAIT].owner, p->subs[SUB_THREEWAY].owner);

		if (index != SUB_REAL) {
			ast_log(LOG_WARNING, "Got flash hook with index %d on channel %d?!?\n", index, p->channel);
			break;
		}
		
		if (p->subs[SUB_CALLWAIT].owner) {
		    	switch_calls(p, 1);
		} else if (!p->subs[SUB_THREEWAY].owner) {
			if (!p->threewaycalling) {
				/* Just send a flash if no 3-way calling */
			    	jdsp_queue_control(p->subs[SUB_REAL].owner, AST_CONTROL_FLASH);
				break;
			} else {
				/* XXX This section needs much more error checking!!! XXX */
				/* Start a 3-way call if feasible */
				if (!((ast->pbx) ||
					(ast->_state == AST_STATE_UP) ||
					(ast->_state == AST_STATE_RING))) {
					ast_log(LOG_DEBUG, "Flash when call not up or ringing\n");
					break;
				}
				start_threeway_call(p);
			}
		} else {
			/* Already have a 3 way call */
			if (p->subs[SUB_THREEWAY].inthreeway) {
				drop_last_party_from_conference(p);
			} else {
				if (p->threewayconference && ((ast->pbx) || (ast->_state == AST_STATE_UP)) && 
					(p->transfertobusy || (ast->_state != AST_STATE_BUSY))) {
				    	/* If conference call is enabled,
					 * pressing flash while in a threeway
					 * call results in starting a conference
					 * call */
					start_conference(ast, p);
				} else if (!p->threewayconference && ast->_state == AST_STATE_UP) {
				    	/* If conference call is disabled,
					 * pressing flash while in a threeway
					 * call results in switching between
					 * calls */
				    	switch_calls(p, 0);
				} else {
					if (option_verbose > 2)
						ast_verbose(VERBOSE_PREFIX_3 "Dumping incomplete call on on %s\n", p->subs[SUB_THREEWAY].owner->name);
					swap_subs(p, SUB_THREEWAY, SUB_REAL);
					p->subs[SUB_THREEWAY].owner->_softhangup |= AST_SOFTHANGUP_DEV;
					p->owner = p->subs[SUB_REAL].owner;

					if (p->subs[SUB_REAL].owner && ast_bridged_channel(p->subs[SUB_REAL].owner))
					{
						jdsp_moh_stop(p, SUB_REAL);
						jdsp_start_audio(p->subs[SUB_REAL].owner, SUB_REAL); /* XXX Need to check whether this line is needed */
					}
				}

			}
		}
		break;
	default:
		ast_log(LOG_DEBUG, "Dunno what to do with event \"%s\" on channel %d\n", jdsp_event2str(&ev), p->channel);
	}
	return &p->subs[index].f;
}

static struct ast_frame *__jdsp_read(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int res;
	int usedindex = -1;
	int index;
	unsigned char *readbuf;
	struct ast_frame *f;
	phone_event_t ev;
	int fd = ast->fds[ast->fdno];

	index = jdsp_get_index(ast, p, 1);

	/* Hang up if we don't really exist */
	if (index < 0)
	{
		ast_log(LOG_WARNING, "We dont exist?\n");
		return NULL;
	}
	
	p->subs[index].f.frametype = AST_FRAME_NULL;
	p->subs[index].f.datalen = 0;
	p->subs[index].f.samples = 0;
	p->subs[index].f.mallocd = 0;
	p->subs[index].f.offset = 0;
	p->subs[index].f.subclass = 0;
	p->subs[index].f.delivery = ast_tv(0,0);
	p->subs[index].f.src = __FUNCTION__;
	p->subs[index].f.data = NULL;

	if (p->subs[index].needanswer) {
		/* Send answer frame if requested */
		p->subs[index].needanswer = 0;
		p->subs[index].f.frametype = AST_FRAME_CONTROL;
		p->subs[index].f.subclass = AST_CONTROL_ANSWER;
		return &p->subs[index].f;
	}

	/* If we got a voice packet */
	if (fd == p->subs[index].dfd)
	{
		int ast_format;

		readbuf = p->subs[index].buffer + AST_FRIENDLY_OFFSET;
		CHECK_BLOCKING(ast);
		res = read(fd, readbuf, READ_SIZE);
		ast_clear_flag(ast, AST_FLAG_BLOCKING);
		if (res < 0)
			return NULL;

		p->subs[index].f.mallocd = 0;
		if (p->faxtxmethod == FAX_TX_T38_AUTO && p->faxdetected)
		{
		    p->subs[index].f.datalen = res;
		    p->subs[index].f.frametype = AST_FRAME_MODEM;
		    p->subs[index].f.subclass = AST_MODEM_T38;
		    p->subs[index].f.offset = AST_FRIENDLY_OFFSET;
		    p->subs[index].f.data = readbuf;
		}
		else
		{
		    ast_format = jdsp_codec_rtp2ast(readbuf);
		    /* Unknown voice format - therefore this is not voice data */
		    if (ast_format == -1)
			return &p->subs[index].f;

		    p->subs[index].f.datalen = res - RTP_HEADER_SIZE;
		    p->subs[index].f.frametype = AST_FRAME_VOICE;
		    p->subs[index].f.subclass = ast_format;
		    p->subs[index].f.offset = AST_FRIENDLY_OFFSET + RTP_HEADER_SIZE;
		    p->subs[index].f.data = readbuf + RTP_HEADER_SIZE;
		}
		p->subs[index].f.samples = ast_codec_get_samples(&p->subs[index].f);
		return &p->subs[index].f;
	}

	if (!p->owner)
	{
		/* If nobody owns us, absorb the event appropriately, otherwise
		   we loop indefinitely.  This occurs when, during call waiting, the
		   other end hangs up our channel so that it no longer exists, but we
		   have neither FLASH'd nor ONHOOK'd to signify our desire to
		   change to the other channel. */
		if (jdsp_get_event(p->jfd, &ev) < 0)
		{
			ast_log(LOG_WARNING, "No event\n");
			return NULL;
		}
		
		/* Switch to real if it is a PHONE_KEY_HOOK_OFF, a PHONE_KEY_HOOK_ON or a PHONE_KEY_FLASH event */
		if (ev.key == PHONE_KEY_HOOK_OFF || ev.key == PHONE_KEY_FLASH ||
		    ev.key == PHONE_KEY_HOOK_ON)
		{
			ast_log(LOG_DEBUG, "Restoring owner of channel %d on event \"%s\"\n", p->channel, jdsp_event2str(&ev));
			p->owner = p->subs[SUB_REAL].owner;

			if (ev.key != PHONE_KEY_HOOK_ON && p->owner && 
			    ast_bridged_channel(p->owner))
			{
				jdsp_moh_stop(p, index);
				jdsp_start_audio(p->subs[index].owner, index); /* XXX Need to check whether this line is needed */
			}

		}
		switch(ev.key) {
		case PHONE_KEY_HOOK_ON:
			jdsp_play_tone(p, PHONE_TONE_NONE);
			if (p->owner) {
				if (option_verbose > 2) 
					ast_verbose(VERBOSE_PREFIX_3 "Channel %s still has call, ringing phone\n", p->owner->name);
				jdsp_callwait(p, 0);
				jdsp_ring(p, 1);
			} else
				ast_log(LOG_WARNING, "Absorbed on hook, but nobody is left!?!?\n");
			break;
		case PHONE_KEY_HOOK_OFF:
			if (p->owner && (p->owner->_state == AST_STATE_RINGING)) {
				jdsp_ring(p, 0);
				p->dialing = 0;
				ast_setstate(p->owner, AST_STATE_UP);
				p->subs[index].f.frametype = AST_FRAME_CONTROL;
				p->subs[index].f.subclass = AST_CONTROL_ANSWER;
			}
			break;
		case PHONE_KEY_FLASH:
			gettimeofday(&p->flashtime, NULL);
			if (p->owner) {
				if (option_verbose > 2) 
					ast_verbose(VERBOSE_PREFIX_3 "Channel %d flashed to other channel %s\n", p->channel, p->owner->name);
				if (p->owner->_state != AST_STATE_UP) {
					/* Answer if necessary */
					usedindex = jdsp_get_index(p->owner, p, 0);
					if (usedindex > -1) {
						p->subs[usedindex].needanswer = 1;
					}
					ast_setstate(p->owner, AST_STATE_UP);
				}
				jdsp_callwait(p, 0);
				jdsp_play_tone(p, PHONE_TONE_NONE);

			} else
				ast_log(LOG_WARNING, "Absorbed on hook, but nobody is left!?!?\n");
			break;
		default:
			ast_log(LOG_WARNING, "Don't know how to absorb event %s\n", jdsp_event2str(&ev));
		}
		f = &p->subs[index].f;
		return f;
	}

	ast_log(LOG_DEBUG, "Read on %d, channel %d\n", fd, p->channel);
	/* If it's not us, return NULL immediately */
	if (ast != p->owner) {
		ast_log(LOG_WARNING, "We're %s, not %s\n", ast->name, p->owner->name);
		f = &p->subs[index].f;
		return f;
	}
	f = jdsp_handle_event(ast);
	return f;
}

struct ast_frame *jdsp_read(struct ast_channel *ast)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	struct ast_frame *f;
	ast_mutex_lock(&p->lock);
	f = __jdsp_read(ast);
	ast_mutex_unlock(&p->lock);
	return f;
}

static unsigned int calc_txstamp(struct jdsp_subchannel *s, struct timeval *delivery)
{
	struct timeval t;
	long ms;
	if (ast_tvzero(s->txcore)) {
		s->txcore = ast_tvnow();
		/* Round to 20ms for nice, pretty timestamps */
		s->txcore.tv_usec -= s->txcore.tv_usec % 20000;
	}
	/* Use previous txcore if available */
	t = (delivery && !ast_tvzero(*delivery)) ? *delivery : ast_tvnow();
	ms = ast_tvdiff_ms(t, s->txcore);
	if (ms < 0)
		ms = 0;
	/* Use what we just got for next time */
	s->txcore = t;
	return (unsigned int) ms;
}

static int jdsp_write_voice_frame(struct jdsp_pvt *p, int index, struct ast_frame *frame)
{
	int fd;
	unsigned int *rtpheader;
	unsigned long ts;

	fd = p->subs[index].dfd;
	if (fd == -1)
		return 0;

	rtpheader = (unsigned int *)((char *)frame->data - RTP_HEADER_SIZE);
	rtpheader[0] = htonl((2 << 30) | (0 << 23) | (jdsp_codec_ast2rtp(frame->subclass) << 16) |\
		(p->subs[index].seqno + 1));
	ts = p->subs[index].lastts + calc_txstamp(&p->subs[index], &frame->delivery) * 8;
	rtpheader[1] = htonl(ts);
	rtpheader[2] = 0; /* The DSP don't care about the ssrc */
	if (!write(fd, rtpheader, frame->datalen + RTP_HEADER_SIZE))
	{
		p->subs[index].lastts = ts;
		p->subs[index].seqno++;
	}

	return frame->datalen;
}

static int jdsp_write_modem_frame(struct jdsp_pvt *p, int index, struct ast_frame *frame)
{
	int fd;
	unsigned int *udptlheader;

	fd = p->subs[index].dfd;
	if (fd == -1)
		return 0;

	udptlheader = (unsigned int *)((char *)frame->data);
	write(fd, udptlheader, frame->datalen);
	
	return frame->datalen;
}

static int jdsp_write(struct ast_channel *ast, struct ast_frame *frame)
{
	struct jdsp_pvt *p = ast->tech_pvt;
	int res = 0;
	int index;

	if (frame->frametype != AST_FRAME_CALLWAITING && frame->frametype != AST_FRAME_VOICE && frame->frametype != AST_FRAME_MODEM)
		return 0;

	if (frame->frametype == AST_FRAME_CALLWAITING)
	{
		ast_mutex_lock(&p->lock);
		if (frame->subclass == AST_CALLWAITING_STOP)
			jdsp_callwait(p, 0);
		else
		{
			char *cid = frame->data;
			char *name = NULL, *num = NULL;

			if (cid)
				ast_callerid_parse(cid, &name, &num);
			if (num)
				ast_copy_string(p->callwait_num, num, sizeof(p->callwait_num));
			if (name)
				ast_copy_string(p->callwait_name, name, sizeof(p->callwait_name));
			jdsp_callwait(p, 1);
		}
		ast_mutex_unlock(&p->lock);
		return 0;
	}
	
	index = jdsp_get_index(ast, p, 0);
	if (index < 0) {
		ast_log(LOG_WARNING, "%s doesn't really exist?\n", ast->name);
		return -1;
	}

	/* Write a frame of (presumably voice) data */
	if (frame->frametype != AST_FRAME_VOICE && frame->frametype != AST_FRAME_MODEM) {
		if (frame->frametype != AST_FRAME_IMAGE)
			ast_log(LOG_WARNING, "Don't know what to do with frame type '%d'\n", frame->frametype);
		return 0;
	}
	/* Workaround for FAX/T.38, which happens to correspond to voice format G.723. */
	if ((frame->frametype == AST_FRAME_VOICE && !(frame->subclass & ast->nativeformats)) ||
	    (frame->frametype == AST_FRAME_MODEM && frame->subclass == AST_MODEM_T38 &&
  	             p->faxtxmethod != FAX_TX_T38_AUTO)) {
		ast_log(LOG_WARNING, "Cannot handle frames in %d format\n", frame->subclass);
		return -1;
	}
	if (p->dialing) {
		if (option_debug)
			ast_log(LOG_DEBUG, "Dropping frame since I'm still dialing on %s...\n",ast->name);
		return 0;
	}
	if (!p->owner) {
		if (option_debug)
			ast_log(LOG_DEBUG, "Dropping frame since there is no active owner on %s...\n",ast->name);
		return 0;
	}
	/* Return if it's not valid data */
	if (!frame->data || !frame->datalen)
		return 0;

	if (frame->subclass != AST_FORMAT_SLINEAR) {
		res = frame->frametype == AST_FRAME_VOICE ?
			jdsp_write_voice_frame(p, index, frame) :
			jdsp_write_modem_frame(p, index, frame);
	}
	if (res < 0) {
		ast_log(LOG_WARNING, "write failed: %s\n", strerror(errno));
		return -1;
	} 
	return 0;
}

static int jdsp_indicate(struct ast_channel *chan, int condition)
{
	struct jdsp_pvt *p = chan->tech_pvt;
	int res = -1;
	int index;

	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(chan, p, 0);
	ast_log(LOG_DEBUG, "Requested indication %d on channel %s\n", condition, chan->name);
	if (index == SUB_REAL) {
		switch(condition) {
		case AST_CONTROL_BUSY:
				res = jdsp_play_tone(p, PHONE_TONE_BUSY);
			break;
		case AST_CONTROL_RINGING:
			res = jdsp_play_tone(p, PHONE_TONE_RING);
			if (chan->_state != AST_STATE_UP) {
			    	ast_setstate(chan, AST_STATE_RINGING);
			}
            		break;
		case AST_CONTROL_PROCEEDING:
			ast_log(LOG_DEBUG,"Received AST_CONTROL_PROCEEDING on %s\n",chan->name);
			/* don't continue in ast_indicate */
			res = 0;
			break;
		case AST_CONTROL_PROGRESS:
			ast_log(LOG_DEBUG,"Received AST_CONTROL_PROGRESS on %s\n",chan->name);
			/* don't continue in ast_indicate */
			res = 0;
			break;
		case AST_CONTROL_CONGESTION:
			chan->hangupcause = AST_CAUSE_CONGESTION;
				res = jdsp_play_tone(p, PHONE_TONE_REORDER);
			break;
		case -1:
			res = jdsp_play_tone(p, PHONE_TONE_NONE);
			break;
		}
	} else
		res = 0;
	ast_mutex_unlock(&p->lock);
	return res;
}

static struct ast_channel *jdsp_new(struct jdsp_pvt *i, int state, int startpbx, int index, int transfercapability)
{
	struct ast_channel *tmp;
	int deflaw = AST_FORMAT_ULAW;
	int x,y;
	if (i->subs[index].owner) {
		ast_log(LOG_WARNING, "Channel %d already has a %s call\n", i->channel,subnames[index]);
		return NULL;
	}
	tmp = ast_channel_alloc(1);
	if (tmp) {
		tmp->tech = &jdsp_tech;
		y = 1;
		do {
			snprintf(tmp->name, sizeof(tmp->name), "jdsp/%d-%d", i->channel, y);
			for (x = 0; x < 3; x++) {
				if ((index != x) && i->subs[x].owner && !strcasecmp(tmp->name, i->subs[x].owner->name))
					break;
			}
			y++;
		} while (x < 3);
		tmp->type = type;
		if (index == SUB_REAL)
		{
			tmp->fds[0] = i->jfd;
			tmp->fds[1] = i->subs[SUB_REAL].dfd;
		}
		if (index == SUB_THREEWAY)
		{
			tmp->fds[0] = i->subs[SUB_THREEWAY].dfd;
		    /* XXX Temporary solution for the problematic scenario where
		     * we're in a call with party A using codec X, put them on
		     * hold, and call party B which chooses codec Y, and then
		     * try to transfer A to B, the transfer will fail since we
		     * will try to be the bridge but we can't do codec
		     * conversion. Before making the 2nd call, we pretend to
		     * support only the codec that is already used in the 1st
		     * call, and set the '_FORCECODEC' variable to indicate the
		     * trunk channel to include only this codec in the SDP.
		     * This fix will be no longer needed when either transfer is
		     * be implemented using REFER, or when a new negotiation
		     * algorithm will be used that will allow us to force a
		     * codec in more graceful way. */
		    tmp->nativeformats = i->subs[SUB_REAL].owner->readformat;
		    pbx_builtin_setvar_helper(tmp, "_FORCECODEC", "");
		}
		else
		    tmp->nativeformats = global_native_formats;

		/* Start out assuming ulaw since it's smaller :) */
		tmp->rawreadformat = deflaw;
		tmp->readformat = deflaw;
		tmp->rawwriteformat = deflaw;
		tmp->writeformat = deflaw;
		
		if (state == AST_STATE_RING)
			tmp->rings = 1;
		tmp->tech_pvt = i;
		tmp->callgroup = i->callgroup;
		tmp->pickupgroup = i->pickupgroup;
		if (!ast_strlen_zero(i->language))
			ast_copy_string(tmp->language, i->language, sizeof(tmp->language));
		if (!ast_strlen_zero(i->musicclass))
			ast_copy_string(tmp->musicclass, i->musicclass, sizeof(tmp->musicclass));
		if (!i->owner)
			i->owner = tmp;
		if (!ast_strlen_zero(i->accountcode))
			ast_copy_string(tmp->accountcode, i->accountcode, sizeof(tmp->accountcode));
		i->subs[index].owner = tmp;
		ast_copy_string(tmp->context, i->context, sizeof(tmp->context));
		/* If we've been told "no ADSI" then enforce it */
		if (!i->adsi)
			tmp->adsicpe = AST_ADSI_UNAVAILABLE;
		if (!ast_strlen_zero(i->exten))
			ast_copy_string(tmp->exten, i->exten, sizeof(tmp->exten));
		if (!ast_strlen_zero(i->rdnis))
			tmp->cid.cid_rdnis = strdup(i->rdnis);
		if (!ast_strlen_zero(i->dnid))
			tmp->cid.cid_dnid = strdup(i->dnid);

		ast_set_callerid(tmp, i->cid_num, i->cid_name, i->cid_num);
		tmp->cid.cid_pres = i->callingpres;
		tmp->cid.cid_ton = i->cid_ton;
		ast_setstate(tmp, state);
		ast_mutex_lock(&usecnt_lock);
		usecnt++;
		ast_mutex_unlock(&usecnt_lock);
		ast_update_use_count();
		if (startpbx) {
			if (ast_pbx_start(tmp)) {
				ast_log(LOG_WARNING, "Unable to start PBX on %s\n", tmp->name);
				ast_hangup(tmp);
				tmp = NULL;
			}
		}
	} else
		ast_log(LOG_WARNING, "Unable to allocate channel structure\n");
	return tmp;
}

static void openrg_cmd(char *cmd)
{
    manager_event(EVENT_FLAG_SYSTEM, "openrg_cmd", "%s\n", cmd);
}

#define CFWD_UNCONDITIONAL_ENTRY "call_forwarding_unconditional"
#define CFWD_BUSY_ENTRY "call_forwarding_on_busy"
#define CFWD_NO_ANSWER_ENTRY "call_forwarding_on_no_answer"

static void configure_cfwd(int channel, char *entry, char *dest)
{
    char cmd[256];

    snprintf(cmd, sizeof(cmd), "conf set voip/line/%d/%s/enabled "
	"%d\n", channel - 1, entry, dest ? 1 : 0); 
    openrg_cmd(cmd);
    
    if (dest)
    {
	snprintf(cmd, sizeof(cmd), "conf set voip/line/%d/%s/"
	    "destination %s\n", channel - 1, entry, dest);
	openrg_cmd(cmd);
    }

    openrg_cmd("conf reconf 1");
}

static char *get_cfwd_activate_type(struct jdsp_pvt *p, char *exten)
{
    if (!strcmp(exten, p->cfwd_unconditional_activate_code))
	return CFWD_UNCONDITIONAL_ENTRY;
    else if (!strcmp(exten, p->cfwd_busy_activate_code)) 
	return CFWD_BUSY_ENTRY;
    else if (!strcmp(exten, p->cfwd_no_answer_activate_code))
	return CFWD_NO_ANSWER_ENTRY;
    else
	return NULL;
}

static char *get_cfwd_deactivate_type(struct jdsp_pvt *p, char *exten)
{
    if (!strcmp(exten, p->cfwd_unconditional_deactivate_code))
	return CFWD_UNCONDITIONAL_ENTRY;
    else if (!strcmp(exten, p->cfwd_busy_deactivate_code)) 
	return CFWD_BUSY_ENTRY;
    else if (!strcmp(exten, p->cfwd_no_answer_deactivate_code))
	return CFWD_NO_ANSWER_ENTRY;
    else
	return NULL;
}

static void *ss_thread(void *data)
{
	struct ast_channel *chan = data;
	struct jdsp_pvt *p = chan->tech_pvt;
	char exten[AST_MAX_EXTENSION] = "";
	int end_dialing;
	int timeout;
	char *getforward = NULL;
	int len = 0;
	int res;
	int index;
	int blindtransfer = 0;
	if (option_verbose > 2) 
		ast_verbose( VERBOSE_PREFIX_3 "Starting simple switch on '%s'\n", chan->name);
	index = jdsp_get_index(chan, p, 1);
	if (index < 0) {
		ast_log(LOG_WARNING, "Huh?\n");
		ast_hangup(chan);
		return NULL;
	}
	/* Read the first digit */
	timeout = firstdigittimeout;
	/* If starting a threeway call, never timeout on the first digit so someone
	   can use flash-hook as a "hold" feature */
	if (p->subs[SUB_THREEWAY].owner) 
		timeout = 999999;
	while(len < AST_MAX_EXTENSION-1) {
	    	char *cfwd_type = NULL;

		/* Read digit unless it's supposed to be immediate, in which case the
		   only answer is 's' */
		if (p->immediate) 
			res = 's';
		else
			res = ast_waitfordigit(chan, timeout);
		end_dialing = 0;
		timeout = 0;
		if (res < 0) {
			ast_log(LOG_DEBUG, "waitfordigit returned < 0...\n");
			res = jdsp_play_tone(p, PHONE_TONE_NONE);
			ast_hangup(chan);
			return NULL;
		} else if (res)  {
		    /* Don't treat # as dial terminator if it's the first dialed digit
			 * since some feature access codes use it */
			if (res == '#' && len)
				end_dialing = 1;
			else {
			    	exten[len++] = res;
			    	exten[len] = '\0';
			}
		}
		if (!ast_ignore_pattern(chan->context, exten))
			jdsp_play_tone(p, PHONE_TONE_NONE);
		else
			jdsp_play_tone(p, PHONE_TONE_DIAL);
		if (!strcmp(exten, "*98") && p->subs[SUB_THREEWAY].owner) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Initiating blind transfer on %s\n", chan->name);

			/* Treat the next dialed number as a
			 * destination for blind transfer */
			blindtransfer = 1;

			res = jdsp_play_tone(p, PHONE_TONE_DIAL); /* XXX need secondary dial tone */
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			*exten = 0;
			timeout = firstdigittimeout;
		} else if ((cfwd_type = get_cfwd_activate_type(p, exten))) {
			jdsp_play_tone(p, PHONE_TONE_STUTTER_DIAL);
			getforward = cfwd_type;
			memset(exten, 0, sizeof(exten));
			len = 0;
		} else if ((cfwd_type = get_cfwd_deactivate_type(p, exten))) {
			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Cancelling call forwarding unconditional on channel %d\n", p->channel);
			jdsp_play_tone(p, PHONE_TONE_CONFIRM);
			configure_cfwd(p->channel, cfwd_type, NULL); 
			getforward = NULL;
			memset(exten, 0, sizeof(exten));
			len = 0;
		} else if (!strcmp(exten, p->dnd_activate_code) || !strcmp(exten, p->dnd_deactivate_code)) {
		    	char cmd[256];

			jdsp_play_tone(p, PHONE_TONE_CONFIRM);
			snprintf(cmd, sizeof(cmd), "conf set "
			    "voip/line/%d/do_not_disturb_enabled %d\n",
			    p->channel - 1,
			    !strcmp(exten, p->dnd_activate_code) ? 1 : 0); 
			openrg_cmd(cmd);
			openrg_cmd("conf reconf 1");

			memset(exten, 0, sizeof(exten));
			len = 0;
		} else if (end_dialing || ast_exists_extension(chan, chan->context, exten, 1, p->cid_num) || strcmp(exten, ast_parking_ext())) {
			if (end_dialing || !res || !ast_matchmore_extension(chan, chan->context, exten, 1, p->cid_num)) {
				if (getforward) {
					/* Record this as the forwarding extension */
					configure_cfwd(p->channel, getforward, exten); 
					if (option_verbose > 2)
						ast_verbose(VERBOSE_PREFIX_3 "Setting call forward to '%s' on channel %d\n", exten, p->channel);
					jdsp_play_tone(p, PHONE_TONE_CONFIRM);
					memset(exten, 0, sizeof(exten));
					len = 0;
					getforward = NULL;
				} else if (blindtransfer) {
					struct ast_frame f;

					/* Tell bridged channel to do
					 * blind transfer */
					memset(&f, 0 , sizeof(f));
					f.frametype = AST_FRAME_BLINDTRANSFER;
					f.data = exten;
					f.datalen = strlen(exten) + 1;
					ast_queue_frame(p->subs[SUB_THREEWAY].owner, &f);
				
					/* Provide a new dial tone and
					 * allow making another call */
					blindtransfer = 0;
					len = 0;
					*exten = 0;
					timeout = firstdigittimeout;
					jdsp_play_tone(p, PHONE_TONE_CONFIRM);
				} else {
					res = jdsp_play_tone(p, PHONE_TONE_NONE);
					ast_copy_string(chan->exten, exten, sizeof(chan->exten));
					if (!ast_strlen_zero(p->cid_num)) {
						if (!p->hidecallerid)
							ast_set_callerid(chan, p->cid_num, NULL, p->cid_num); 
						else
							ast_set_callerid(chan, NULL, NULL, p->cid_num); 
					}
					if (!ast_strlen_zero(p->cid_name)) {
						if (!p->hidecallerid)
							ast_set_callerid(chan, NULL, p->cid_name, NULL);
					}
					ast_setstate(chan, AST_STATE_RING);
					res = ast_pbx_run(chan);
					if (res) {
						ast_log(LOG_WARNING, "PBX exited non-zero\n");
						res = jdsp_play_tone(p, PHONE_TONE_REORDER);
					}
					return NULL;
				}
			} else {
				/* It's a match, but they just typed a digit, and there is an ambiguous match,
				   so just set the timeout to matchdigittimeout and wait some more */
				timeout = p->matchdigittimeout;
			}
		} else if (res == 0) {
			ast_log(LOG_DEBUG, "not enough digits (and no ambiguous match)...\n");
			ast_hangup(chan);
			return NULL;
		} else if (p->callwaiting && !strcmp(exten, "*70")) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Disabling call waiting on %s\n", chan->name);
			/* Disable call waiting if enabled */
			p->callwaiting = 0;
			res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			memset(exten, 0, sizeof(exten));
			timeout = firstdigittimeout;
				
		} else if (!strcmp(exten,ast_pickup_ext())) {
			/* Scan all channels and see if any there
			 * ringing channqels with that have call groups
			 * that equal this channels pickup group  
			 */
		  	if (index == SUB_REAL) {
				/* Switch us from Third call to Call Wait */
			  	if (p->subs[SUB_THREEWAY].owner) {
					/* If you make a threeway call and the *8# a call, it should actually 
					   look like a callwait */
				  	swap_subs(p, SUB_CALLWAIT, SUB_THREEWAY);
					unalloc_sub(p, SUB_THREEWAY);
				}
				if (ast_pickup_call(chan)) {
					ast_log(LOG_DEBUG, "No call pickup possible...\n");
					res = jdsp_play_tone(p, PHONE_TONE_REORDER);
				}
				ast_hangup(chan);
				return NULL;
			} else {
				ast_log(LOG_WARNING, "Huh?  Got *8# on call not on real\n");
				ast_hangup(chan);
				return NULL;
			}
			
		} else if (!p->hidecallerid && !strcmp(exten, "*67")) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Disabling Caller*ID on %s\n", chan->name);
			/* Disable Caller*ID if enabled */
			p->hidecallerid = 1;
			if (chan->cid.cid_num)
				free(chan->cid.cid_num);
			chan->cid.cid_num = NULL;
			if (chan->cid.cid_name)
				free(chan->cid.cid_name);
			chan->cid.cid_name = NULL;
			res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			memset(exten, 0, sizeof(exten));
			timeout = firstdigittimeout;
		} else if (p->callreturn && !strcmp(exten, "*69")) {
			res = 0;
			if (!ast_strlen_zero(p->lastcid_num)) {
				res = ast_say_digit_str(chan, p->lastcid_num, "", chan->language);
			}
			if (!res)
				res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			goto Exit;
		} else if ((p->transfermode == TRANSFER_BRIDGING || p->canpark) && !strcmp(exten, ast_parking_ext()) && 
					p->subs[SUB_THREEWAY].owner &&
					ast_bridged_channel(p->subs[SUB_THREEWAY].owner)) {
			/* This is a three way call, the main call being a real channel, 
				and we're parking the first call. */
			ast_masq_park_call(ast_bridged_channel(p->subs[SUB_THREEWAY].owner), chan, 0, NULL);
			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Parking call to '%s'\n", chan->name);
			goto Exit;
		} else if (!ast_strlen_zero(p->lastcid_num) && !strcmp(exten, "*60")) {
			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Blacklisting number %s\n", p->lastcid_num);
			res = ast_db_put("blacklist", p->lastcid_num, "1");
			if (!res) {
				res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
				memset(exten, 0, sizeof(exten));
				len = 0;
			}
		} else if (p->hidecallerid && !strcmp(exten, "*82")) {
			if (option_verbose > 2) 
				ast_verbose(VERBOSE_PREFIX_3 "Enabling Caller*ID on %s\n", chan->name);
			/* Enable Caller*ID if enabled */
			p->hidecallerid = 0;
			if (chan->cid.cid_num)
				free(chan->cid.cid_num);
			chan->cid.cid_num = NULL;
			if (chan->cid.cid_name)
				free(chan->cid.cid_name);
			chan->cid.cid_name = NULL;
			ast_set_callerid(chan, p->cid_num, p->cid_name, NULL);
			res = jdsp_play_tone(p, PHONE_TONE_DIAL /*JDSP_TONE_DIALRECALL*/);
			if (res) {
				ast_log(LOG_WARNING, "Unable to do dial recall on channel %s: %s\n", 
					chan->name, strerror(errno));
			}
			len = 0;
			memset(exten, 0, sizeof(exten));
			timeout = firstdigittimeout;
		} else if (!ast_canmatch_extension(chan, chan->context, exten, 1, chan->cid.cid_num) &&
						((exten[0] != '*') || (strlen(exten) > 2))) {
			if (option_debug)
				ast_log(LOG_DEBUG, "Can't match %s from '%s' in context %s\n", exten, chan->cid.cid_num ? chan->cid.cid_num : "<Unknown Caller>", chan->context);
			goto Exit;
		}
		if (!timeout)
			timeout = gendigittimeout;
		if (len && !ast_ignore_pattern(chan->context, exten))
			jdsp_play_tone(p, PHONE_TONE_NONE);
	}
	
Exit:
	res = jdsp_play_tone(p, PHONE_TONE_REORDER);
	if (res < 0)
			ast_log(LOG_WARNING, "Unable to play congestion tone on channel %d\n", p->channel);
	ast_hangup(chan);
	return NULL;
}

static int handle_init_event(struct jdsp_pvt *i, phone_event_t *event)
{
	int res;
	pthread_t threadid;
	pthread_attr_t attr;
	struct ast_channel *chan;

	if (!i->enabled)
		return 0;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ast_log(LOG_DEBUG, "Got event \"%s\" on channel %d\n", jdsp_event2str(event), i->channel);
	/* Handle an event on a given channel for the monitor thread. */
	switch(event->key) {
	case PHONE_KEY_HOOK_OFF:
	case PHONE_KEY_FLASH:
		/* Got a ring/answer */
		if (i->immediate) {
			/* The channel is immediately up.  Start right away */
			chan = jdsp_new(i, AST_STATE_RING, 1, SUB_REAL, 0);
			if (!chan) {
				ast_log(LOG_WARNING, "Unable to start PBX on channel %d\n", i->channel);
				res = jdsp_play_tone(i, PHONE_TONE_REORDER);
				if (res < 0)
					ast_log(LOG_WARNING, "Unable to play congestion tone on channel %d\n", i->channel);
			}
		} else {
			/* Check for callerid, digits, etc */
			chan = jdsp_new(i, AST_STATE_RESERVED, 0, SUB_REAL, 0);
			if (chan) {
				if (need_mwi_tone(i))
					res = jdsp_play_tone(i, PHONE_TONE_MWI);
				else if (i->stutterdialtone)
					res = jdsp_play_tone(i, PHONE_TONE_STUTTER_DIAL);
				else
					res = jdsp_play_tone(i, PHONE_TONE_DIAL);
				if (res < 0) 
					ast_log(LOG_WARNING, "Unable to play dialtone on channel %d\n", i->channel);
				if (ast_pthread_create(&threadid, &attr, ss_thread, chan)) {
					ast_log(LOG_WARNING, "Unable to start simple switch thread on channel %d\n", i->channel);
					res = jdsp_play_tone(i, PHONE_TONE_REORDER);
					if (res < 0)
						ast_log(LOG_WARNING, "Unable to play congestion tone on channel %d\n", i->channel);
					ast_hangup(chan);
				}
			} else
				ast_log(LOG_WARNING, "Unable to create channel\n");
		}
		break;
	case PHONE_KEY_HOOK_ON:
		res = jdsp_play_tone(i, PHONE_TONE_NONE);
		break;
	default:
		ast_verbose("Don't know how to handle \"%s\" event on channel %d\n",
			jdsp_event2str(event), i->channel);
	}
	return 0;
}

static void *do_monitor(void *data)
{
	int count, res, spoint, pollres = 0;
	struct jdsp_pvt *i;
	time_t thispass = 0, lastpass = 0;
	int found;
	struct pollfd *pfds = NULL;
	int lastalloc = -1;
	/* This thread monitors all the frame relay interfaces which are not yet in use
	   (and thus do not have a separate thread) indefinitely */
	/* From here on out, we die whenever asked */
	for(;;) {
		/* Lock the interface list */
		if (ast_mutex_lock(&iflock)) {
			ast_log(LOG_ERROR, "Unable to grab interface lock\n");
			return NULL;
		}
		if (!pfds || (lastalloc != ifcount)) {
			if (pfds)
				free(pfds);
			if (ifcount) {
				pfds = malloc(ifcount * sizeof(struct pollfd));
				if (!pfds) {
					ast_log(LOG_WARNING, "Critical memory error.  Jdsp dies.\n");
					ast_mutex_unlock(&iflock);
					return NULL;
				}
			}
			lastalloc = ifcount;
		}
		/* Build the stuff we're going to poll on, that is the socket of every
		   jdsp_pvt that does not have an associated owner channel */
		count = 0;
		i = iflist;
		while(i) {
			if ((i->jfd > -1) && i->sig) {
				if (!i->owner && !i->subs[SUB_REAL].owner) {
					/* This needs to be watched, as it lacks an owner */
					pfds[count].fd = i->jfd;
					pfds[count].events = POLLIN;
 					pfds[count].revents = 0;
					count++;
				}
			}
			i = i->next;
		}
		/* Okay, now that we know what to do, release the interface lock */
		ast_mutex_unlock(&iflock);
		
		pthread_testcancel();
		res = ast_sched_wait(sched);
		if ((res < 0) || (res > 1000))
			res = 1000;
		/* Wait at most a second for something to happen */
		res = poll(pfds, count, res);
		pthread_testcancel();
		/* Okay, poll has finished.  Let's see what happened.  */
		if (res < 0) {
			if ((errno != EAGAIN) && (errno != EINTR))
				ast_log(LOG_WARNING, "poll return %d: %s\n", res, strerror(errno));
			continue;
		}
		ast_mutex_lock(&monlock);
		ast_sched_runq(sched);
		ast_mutex_unlock(&monlock);
		/* Alright, lock the interface list again, and let's look and see what has
		   happened */
		if (ast_mutex_lock(&iflock)) {
			ast_log(LOG_WARNING, "Unable to lock the interface list\n");
			continue;
		}
		found = 0;
		spoint = 0;
		lastpass = thispass;
		thispass = time(NULL);
		i = iflist;
		while(i) {
			if ((i->jfd > -1) && i->sig) {
				pollres = ast_fdisset(pfds, i->jfd, count, &spoint);
				if (pollres & POLLIN) {
					phone_event_t ev;
					if (i->owner || i->subs[SUB_REAL].owner) {
						ast_log(LOG_WARNING, "Whoa....  I'm owned but found (%d) in read...\n", i->jfd);
						i = i->next;
						continue;
					}

					/* Error may arise if the driver has detected a double key release. Ignore
					 * it and continue */
					if (jdsp_get_event(i->jfd, &ev) < 0)
						continue;

					if (option_debug)
						ast_log(LOG_DEBUG, "Monitor got event %s on channel %d\n", jdsp_event2str(&ev), i->channel);
					/* Don't hold iflock while handling init events -- race with chlock */
					ast_mutex_unlock(&iflock);
					handle_init_event(i, &ev);
					ast_mutex_lock(&iflock);	
				}
			}
			i = i->next;
		}
		ast_mutex_unlock(&iflock);
	}
	/* Never reached */
	return NULL;
	
}

static int restart_monitor(void)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	/* If we're supposed to be stopped -- stay stopped */
	if (monitor_thread == AST_PTHREADT_STOP)
		return 0;
	if (ast_mutex_lock(&monlock)) {
		ast_log(LOG_WARNING, "Unable to lock monitor\n");
		return -1;
	}
	if (monitor_thread == pthread_self()) {
		ast_mutex_unlock(&monlock);
		ast_log(LOG_WARNING, "Cannot kill myself\n");
		return -1;
	}
	if (monitor_thread != AST_PTHREADT_NULL) {
		/* Just signal it to be sure it wakes up */
		pthread_kill(monitor_thread, SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create(&monitor_thread, &attr, do_monitor, NULL) < 0) {
			ast_mutex_unlock(&monlock);
			ast_log(LOG_ERROR, "Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&monlock);
	return 0;
}

static struct jdsp_pvt *mkintf(int channel, int signalling, int reloading)
{
	/* Make a jdsp_pvt structure for this interface (or CRV if "pri" is specified) */
	struct jdsp_pvt *tmp = NULL, *tmp2,  *prev = NULL;
	int here = 0;
	int x;
	struct jdsp_pvt **wlist;
	struct jdsp_pvt **wend;

	wlist = &iflist;
	wend = &ifend;

	tmp2 = *wlist;
	prev = NULL;

	while (tmp2) {
		if (tmp2->channel == channel) {
			tmp = tmp2;
			here = 1;
			break;
		}
		if (tmp2->channel > channel) {
			break;
		}
		prev = tmp2;
		tmp2 = tmp2->next;
	}

	if (!here && !reloading) {
		tmp = (struct jdsp_pvt*)malloc(sizeof(struct jdsp_pvt));
		if (!tmp) {
			ast_log(LOG_ERROR, "MALLOC FAILED\n");
			destroy_jdsp_pvt(&tmp);
			return NULL;
		}
		memset(tmp, 0, sizeof(struct jdsp_pvt));
		ast_mutex_init(&tmp->lock);
		ifcount++;
		tmp->jfd = -1;
		tmp->offhookwarningschedid = -1;
		for (x = 0; x < 3; x++)
		{
			tmp->subs[x].chan = -1;
			tmp->subs[x].dfd = -1;
		}
		tmp->channel = channel;
	}

	if (tmp) {
		if (!here) {
			/* Open non-blocking */
			tmp->jfd = jdsp_open_slic(channel);
			tmp->subs[SUB_REAL].dfd = jdsp_open_dsp();
			tmp->subs[SUB_THREEWAY].dfd = jdsp_open_dsp();
			/* Allocate a jdsp structure */
			if (tmp->jfd < 0) {
				ast_log(LOG_ERROR, "Unable to open channel %d: %s\nhere = %d, tmp->channel = %d, channel = %d\n", channel, strerror(errno), here, tmp->channel, channel);
				destroy_jdsp_pvt(&tmp);
				return NULL;
			}
		} else {
			signalling = tmp->sig;
		}
		tmp->enabled = enabled;
		tmp->immediate = immediate;
 		tmp->transfertobusy = transfertobusy;
		tmp->sig = signalling;
		tmp->permcallwaiting = callwaiting;
		tmp->callwaitingcallerid = callwaitingcallerid;
		tmp->threewaycalling = threewaycalling;
		tmp->threewayconference = threewayconference;
		tmp->stutterdialtone = stutterdialtone;
		tmp->mwi = mwi;
		tmp->adsi = adsi;
		tmp->permhidecallerid = hidecallerid;
		tmp->callreturn = callreturn;
		tmp->callprogress = callprogress;
		ast_copy_string(tmp->cfwd_unconditional_activate_code, cfwd_unconditional_activate_code, sizeof(tmp->cfwd_unconditional_activate_code));
		ast_copy_string(tmp->cfwd_unconditional_deactivate_code, cfwd_unconditional_deactivate_code, sizeof(tmp->cfwd_unconditional_deactivate_code));
		ast_copy_string(tmp->cfwd_busy_activate_code, cfwd_busy_activate_code, sizeof(tmp->cfwd_busy_activate_code));
		ast_copy_string(tmp->cfwd_busy_deactivate_code, cfwd_busy_deactivate_code, sizeof(tmp->cfwd_busy_deactivate_code));
		ast_copy_string(tmp->cfwd_no_answer_activate_code, cfwd_no_answer_activate_code, sizeof(tmp->cfwd_no_answer_activate_code));
		ast_copy_string(tmp->cfwd_no_answer_deactivate_code, cfwd_no_answer_deactivate_code, sizeof(tmp->cfwd_no_answer_deactivate_code));
		ast_copy_string(tmp->dnd_activate_code, dnd_activate_code, sizeof(tmp->dnd_activate_code));
		ast_copy_string(tmp->dnd_deactivate_code, dnd_deactivate_code, sizeof(tmp->dnd_deactivate_code));
		tmp->callwaiting = tmp->permcallwaiting;
		tmp->hidecallerid = tmp->permhidecallerid;
		tmp->channel = channel;
		tmp->use_callerid = use_callerid;
		tmp->cid_signalling = cid_signalling;
		tmp->jdsptrcallerid = jdsptrcallerid;
		tmp->matchdigittimeout = matchdigittimeout;

		ast_copy_string(tmp->accountcode, accountcode, sizeof(tmp->accountcode));
		tmp->canpark = canpark;
		tmp->transfermode = transfermode;
		ast_copy_string(tmp->defcontext,context,sizeof(tmp->defcontext));
		ast_copy_string(tmp->language, language, sizeof(tmp->language));
		ast_copy_string(tmp->musicclass, musicclass, sizeof(tmp->musicclass));
		ast_copy_string(tmp->context, context, sizeof(tmp->context));
		ast_copy_string(tmp->cid_num, cid_num, sizeof(tmp->cid_num));
		tmp->cid_ton = 0;
		ast_copy_string(tmp->cid_name, cid_name, sizeof(tmp->cid_name));
		ast_copy_string(tmp->mailbox, mailbox, sizeof(tmp->mailbox));
		tmp->group = cur_group;
		tmp->callgroup = cur_callergroup;
		tmp->pickupgroup = cur_pickupgroup;
		tmp->rxgain = rxgain;
		tmp->txgain = txgain;
		tmp->tonezone = tonezone;
		tmp->onhooktime = time(NULL);
		tmp->faxtxmethod = faxtxmethod;
		tmp->faxdetected = 0;
	}
	if (tmp && !here) {
		/* nothing on the iflist */
		if (!*wlist) {
			*wlist = tmp;
			tmp->prev = NULL;
			tmp->next = NULL;
			*wend = tmp;
		} else {
			/* at least one member on the iflist */
			struct jdsp_pvt *working = *wlist;

			/* check if we maybe have to put it on the begining */
			if (working->channel > tmp->channel) {
				tmp->next = *wlist;
				tmp->prev = NULL;
				(*wlist)->prev = tmp;
				*wlist = tmp;
			} else {
			/* go through all the members and put the member in the right place */
				while (working) {
					/* in the middle */
					if (working->next) {
						if (working->channel < tmp->channel && working->next->channel > tmp->channel) {
							tmp->next = working->next;
							tmp->prev = working;
							working->next->prev = tmp;
							working->next = tmp;
							break;
						}
					} else {
					/* the last */
						if (working->channel < tmp->channel) {
							working->next = tmp;
							tmp->next = NULL;
							tmp->prev = working;
							*wend = tmp;
							break;
						}
					}
					working = working->next;
				}
			}
		}
	}
	return tmp;
}

static inline int available(struct jdsp_pvt *p, int channelmatch, int groupmatch, int *busy, int *channelmatched, int *groupmatched)
{
	/* First, check group matching */
	if (groupmatch) {
	    if ((p->group & groupmatch) != groupmatch)
			return 0;
		*groupmatched = 1;
	}
	/* Check to see if we have a channel match */
	if (channelmatch != -1) {
	    if (p->channel != channelmatch)
			return 0;
		*channelmatched = 1;
	}
	/* We're at least busy at this point */
	if (busy) {
		*busy = 1;
	}
	/* If guard time, definitely not */
	if (p->guardtime && (time(NULL) < p->guardtime)) 
		return 0;
		
	/* If no owner definitely available */
	if (!p->owner) {
	    	int res;
	    	int rxisoffhook;

	    	if (!p->sig)
			return 1;
	    	/* Check hook state */
	    	if (p->jfd > -1)
			res = ioctl(p->jfd, VOIP_SLIC_GET_HOOK, &rxisoffhook);
	    	else {
			/* Assume not off hook on CVRS */
			res = 0;
			rxisoffhook = 0;
	    	}
	    	if (res) {
			ast_log(LOG_WARNING, "Unable to check hook state on channel %d\n", p->channel);
	    	} else if (rxisoffhook) {
			ast_log(LOG_DEBUG, "Channel %d off hook, can't use\n", p->channel);
			/* Not available when the other end is off hook */
			return 0;
	    	}
		return 1;
	}

	if (!p->callwaiting) {
		/* If they don't have call waiting enabled, then for sure they're unavailable at this point */
		return 0;
	}

	if (p->subs[SUB_CALLWAIT].owner) {
		/* If there is already a call waiting call, then we can't take a second one */
		return 0;
	}

	if (p->owner->_state != AST_STATE_UP) {
		/* If the current call is not up, then don't allow the call */
		return 0;
	}
	if ((p->subs[SUB_THREEWAY].owner)) {
		/* Can't take a call wait when we are in a three way call. */
		return 0;
	}
	/* We're cool */
	return 1;
}

static struct ast_channel *jdsp_request(const char *type, int format, void *data, int *cause)
{
	int oldformat;
	int groupmatch = 0;
	int channelmatch = -1;
	int roundrobin = 0;
	int callwait = 0;
	int busy = 0;
	struct jdsp_pvt *p;
	struct ast_channel *tmp = NULL;
	char *dest = NULL;
	int x;
	char *s;
	char opt = 0;
	int res = 0, y = 0;
	int backwards = 0;
	struct jdsp_pvt *exit, *start, *end;
	ast_mutex_t *lock;
	int channelmatched = 0;
	int groupmatched = 0;
	
	/* Assume we're locking the iflock */
	lock = &iflock;
	start = iflist;
	end = ifend;
	/* We do signed linear */
	oldformat = format;
	format &= global_native_formats;
	if (!format) {
		ast_log(LOG_NOTICE, "Asked to get a channel of unsupported format '%d'\n", oldformat);
		return NULL;
	}
	if (data) {
		dest = ast_strdupa((char *)data);
	} else {
		ast_log(LOG_WARNING, "Channel requested with no data\n");
		return NULL;
	}
	if (toupper(dest[0]) == 'G' || toupper(dest[0]) == 'R') {
		/* Retrieve the group number */
		char *stringp = NULL;
		stringp = dest + 1;
		s = strsep(&stringp, "/");
		if ((res = sscanf(s, "%d%c%d", &x, &opt, &y)) < 1) {
			ast_log(LOG_WARNING, "Unable to determine group for data %s\n", (char *)data);
			return NULL;
		}
		groupmatch = 1 << x;
		if (toupper(dest[0]) == 'G') {
			if (dest[0] == 'G') {
				backwards = 1;
				p = ifend;
			} else
				p = iflist;
		} else {
			if (dest[0] == 'R') {
				backwards = 1;
				p = round_robin[x]?round_robin[x]->prev:ifend;
				if (!p)
					p = ifend;
			} else {
				p = round_robin[x]?round_robin[x]->next:iflist;
				if (!p)
					p = iflist;
			}
			roundrobin = 1;
		}
	} else {
		char *stringp = NULL;
		stringp = dest;
		s = strsep(&stringp, "/");
		p = iflist;
		if ((res = sscanf(s, "%d%c%d", &x, &opt, &y)) < 1) {
			ast_log(LOG_WARNING, "Unable to determine channel for data %s\n", (char *)data);
			return NULL;
		} else {
			channelmatch = x;
		}
	}
	/* Search for an unowned channel */
	if (ast_mutex_lock(lock)) {
		ast_log(LOG_ERROR, "Unable to lock interface list???\n");
		return NULL;
	}
	exit = p;
	while(p && !tmp) {
		if (roundrobin)
			round_robin[x] = p;

		if (p && available(p, channelmatch, groupmatch, &busy, &channelmatched, &groupmatched)) {
			if (option_debug)
				ast_log(LOG_DEBUG, "Using channel %d\n", p->channel);

			callwait = (p->owner != NULL);
			p->outgoing = 1;
			tmp = jdsp_new(p, AST_STATE_RESERVED, 0, p->owner ? SUB_CALLWAIT : SUB_REAL, 0);
			/* Make special notes */
			if (res > 1) {
				if (opt == 'c') {
					/* Confirm answer */
					p->confirmanswer = 1;
				} else if (opt == 'r') {
					/* Distinctive ring */
					if (res < 3)
						ast_log(LOG_WARNING, "Distinctive ring missing identifier in '%s'\n", (char *)data);
					else
						p->distinctivering = y;
				} else {
					ast_log(LOG_WARNING, "Unknown option '%c' in '%s'\n", opt, (char *)data);
				}
			}
			/* Note if the call is a call waiting call */
			if (tmp && callwait)
				tmp->cdrflags |= AST_CDR_CALLWAIT;
			break;
		}
		if (backwards) {
			p = p->prev;
			if (!p)
				p = end;
		} else {
			p = p->next;
			if (!p)
				p = start;
		}
		/* stop when you roll to the one that we started from */
		if (p == exit)
			break;
	}
	ast_mutex_unlock(lock);
	restart_monitor();
	if (callwait)
		*cause = AST_CAUSE_BUSY;
 	else if (!tmp) {
 		if (channelmatched) {
 			if (busy)
 				*cause = AST_CAUSE_BUSY;
 		} else if (groupmatched) {
 			*cause = AST_CAUSE_CONGESTION;
 		}
 	}

	return tmp;
}

static int jdsp_destroy_channel(int fd, int argc, char **argv)
{
	int channel = 0;
	struct jdsp_pvt *tmp = NULL;
	struct jdsp_pvt *prev = NULL;
	
	if (argc != 4) {
		return RESULT_SHOWUSAGE;
	}
	channel = atoi(argv[3]);

	tmp = iflist;
	while (tmp) {
		if (tmp->channel == channel) {
			destroy_channel(prev, tmp, 1);
			return RESULT_SUCCESS;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	return RESULT_FAILURE;
}

static int jdsp_show_channels(int fd, int argc, char **argv)
{
#define FORMAT "%7s %-10.10s %-15.15s %-10.10s %-20.20s\n"
#define FORMAT2 "%7s %-10.10s %-15.15s %-10.10s %-20.20s\n"
	struct jdsp_pvt *tmp = NULL;
	char tmps[20] = "";
	ast_mutex_t *lock;
	struct jdsp_pvt *start;

	lock = &iflock;
	start = iflist;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	ast_mutex_lock(lock);
	
	tmp = start;
	while (tmp) {
		snprintf(tmps, sizeof(tmps), "%d", tmp->channel);
		ast_cli(fd, FORMAT, tmps, tmp->exten, tmp->context, tmp->language, tmp->musicclass);
		tmp = tmp->next;
	}
	ast_mutex_unlock(lock);
	return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char show_channels_usage[] =
	"Usage: jdsp show channels\n"
	"	Shows a list of available channels\n";

static char destroy_channel_usage[] =
	"Usage: jdsp destroy channel <chan num>\n"
	"	DON'T USE THIS UNLESS YOU KNOW WHAT YOU ARE DOING.  Immediately removes a given channel, whether it is in use or not\n";

static struct ast_cli_entry jdsp_cli[] = {
    	{ {"jdsp", "show", "channels", NULL}, jdsp_show_channels,
	  "Show active jdsp channels", show_channels_usage },
	{ {"jdsp", "destroy", "channel", NULL}, jdsp_destroy_channel,
	  "Destroy a channel", destroy_channel_usage },
};

#define TRANSFER	0
#define HANGUP		1

static int __unload_module(void)
{
	int x = 0;
	struct jdsp_pvt *p, *pl;
	ast_cli_unregister_multiple(jdsp_cli, sizeof(jdsp_cli) / sizeof(jdsp_cli[0]));
	ast_manager_unregister( "JDSPDialOffhook" );
	ast_manager_unregister( "JDSPHangup" );
	ast_manager_unregister( "JDSPTransfer" );
	ast_manager_unregister( "JDSPDNDoff" );
	ast_manager_unregister( "JDSPDNDon" );
	ast_manager_unregister("JDSPShowChannels");
	ast_channel_unregister(&jdsp_tech);
	if (!ast_mutex_lock(&iflock)) {
		/* Hangup all interfaces if they have an owner */
		p = iflist;
		while(p) {
			if (p->owner)
				ast_softhangup(p->owner, AST_SOFTHANGUP_APPUNLOAD);
			p = p->next;
		}
		ast_mutex_unlock(&iflock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}
	if (!ast_mutex_lock(&monlock)) {
		if (monitor_thread && (monitor_thread != AST_PTHREADT_STOP) && (monitor_thread != AST_PTHREADT_NULL)) {
			pthread_cancel(monitor_thread);
			pthread_kill(monitor_thread, SIGURG);
			pthread_join(monitor_thread, NULL);
		}
		monitor_thread = AST_PTHREADT_STOP;
		ast_mutex_unlock(&monlock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}

	if (!ast_mutex_lock(&iflock)) {
		/* Destroy all the interfaces and free their memory */
		p = iflist;
		while(p) {
			/* Close the jdsp thingy */
			if (p->jfd > -1)
				jdsp_close(p->jfd);

			if (p->offhookwarningschedid > -1)
				ast_sched_del(sched, p->offhookwarningschedid);

			pl = p;
			p = p->next;
			x++;
			/* Free associated memory */
			if(pl)
				destroy_jdsp_pvt(&pl);
			ast_verbose(VERBOSE_PREFIX_3 "Unregistered channel %d\n", x);
		}
		iflist = NULL;
		ifcount = 0;
		ast_mutex_unlock(&iflock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}
	sched_context_destroy(sched);
	sched = NULL;
	return 0;
}

int unload_module()
{
	return __unload_module();
}
		
static int setup_jdsp(int reload)
{
	struct ast_config *cfg;
	struct ast_variable *v;
	struct jdsp_pvt *tmp;
	char *chan;
	char *c;
	int start, finish,x;

	cfg = ast_config_load(config);

	/* We *must* have a config file otherwise stop immediately */
	if (!cfg) {
		ast_log(LOG_ERROR, "Unable to load config %s\n", config);
		return -1;
	}
	

	if (ast_mutex_lock(&iflock)) {
		/* It's a little silly to lock it, but we mind as well just to be sure */
		ast_log(LOG_ERROR, "Unable to lock interface list???\n");
		return -1;
	}
	v = ast_variable_browse(cfg, "channels");
	while(v) {
		/* Create the interface list */
		if (!strcasecmp(v->name, "channel")
					) {
			if (reload == 0) {
				if (cur_signalling < 0) {
					ast_log(LOG_ERROR, "Signalling must be specified before any channels are.\n");
					ast_config_destroy(cfg);
					ast_mutex_unlock(&iflock);
					return -1;
				}
			}
			c = v->value;

			chan = strsep(&c, ",");
			while(chan) {
				if (sscanf(chan, "%d-%d", &start, &finish) == 2) {
					/* Range */
				} else if (sscanf(chan, "%d", &start)) {
					/* Just one */
					finish = start;
				} else {
					ast_log(LOG_ERROR, "Syntax error parsing '%s' at '%s'\n", v->value, chan);
					ast_config_destroy(cfg);
					ast_mutex_unlock(&iflock);
					return -1;
				}
				if (finish < start) {
					ast_log(LOG_WARNING, "Sillyness: %d < %d\n", start, finish);
					x = finish;
					finish = start;
					start = x;
				}
				for (x = start; x <= finish; x++) {
					tmp = mkintf(x, cur_signalling, reload);

					if (tmp) {
						if (option_verbose > 2) {
								ast_verbose(VERBOSE_PREFIX_3 "%s channel %d, %s signalling\n", reload ? "Reconfigured" : "Registered", x, sig2str(tmp->sig));
						}
					} else {
						if (reload == 1)
							ast_log(LOG_ERROR, "Unable to reconfigure channel '%s'\n", v->value);
						else
							ast_log(LOG_ERROR, "Unable to register channel '%s'\n", v->value);
						ast_config_destroy(cfg);
						ast_mutex_unlock(&iflock);
						return -1;
					}
				}
				chan = strsep(&c, ",");
			}
		} else if (!strcasecmp(v->name, "enabled")) {
			enabled = ast_true(v->value);
		} else if (!strcasecmp(v->name, "usecallerid")) {
			use_callerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "cidsignalling")) {
			if (!strcasecmp(v->value, "bell"))
				cid_signalling = CID_SIG_BELL;
			else if (!strcasecmp(v->value, "v23"))
				cid_signalling = CID_SIG_V23;
			else if (!strcasecmp(v->value, "dtmf"))
				cid_signalling = CID_SIG_DTMF;
			else if (ast_true(v->value))
				cid_signalling = CID_SIG_BELL;
		} else if (!strcasecmp(v->name, "threewaycalling")) {
			threewaycalling = ast_true(v->value);
		} else if (!strcasecmp(v->name, "threewayconference")) {
			threewayconference = ast_true(v->value);
		} else if (!strcasecmp(v->name, "stutterdialtone")) {
			stutterdialtone = ast_true(v->value);
		} else if (!strcasecmp(v->name, "mwi")) {
			mwi = ast_true(v->value);
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(NULL, &global_native_formats, v->value, 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(NULL, &global_native_formats, v->value, 0);
		} else if (!strcasecmp(v->name, "cfwd_unconditional_activate_code")) {
			ast_copy_string(cfwd_unconditional_activate_code, v->value, sizeof(cfwd_unconditional_activate_code));
		} else if (!strcasecmp(v->name, "cfwd_unconditional_deactivate_code")) {
			ast_copy_string(cfwd_unconditional_deactivate_code, v->value, sizeof(cfwd_unconditional_deactivate_code));
		} else if (!strcasecmp(v->name, "cfwd_busy_activate_code")) {
			ast_copy_string(cfwd_busy_activate_code, v->value, sizeof(cfwd_busy_activate_code));
		} else if (!strcasecmp(v->name, "cfwd_busy_deactivate_code")) {
			ast_copy_string(cfwd_busy_deactivate_code, v->value, sizeof(cfwd_busy_deactivate_code));
		} else if (!strcasecmp(v->name, "cfwd_no_answer_activate_code")) {
			ast_copy_string(cfwd_no_answer_activate_code, v->value, sizeof(cfwd_no_answer_activate_code));
		} else if (!strcasecmp(v->name, "cfwd_no_answer_deactivate_code")) {
			ast_copy_string(cfwd_no_answer_deactivate_code, v->value, sizeof(cfwd_no_answer_deactivate_code));
		} else if (!strcasecmp(v->name, "dnd_activate_code")) {
			ast_copy_string(dnd_activate_code, v->value, sizeof(dnd_activate_code));
		} else if (!strcasecmp(v->name, "dnd_deactivate_code")) {
			ast_copy_string(dnd_deactivate_code, v->value, sizeof(dnd_deactivate_code));
		} else if (!strcasecmp(v->name, "mailbox")) {
			ast_copy_string(mailbox, v->value, sizeof(mailbox));
		} else if (!strcasecmp(v->name, "faxtxmethod")) {
		    if (!strcasecmp(v->value, "none"))
			faxtxmethod = FAX_TX_NONE;
		    else if (!strcasecmp(v->value, "t38_auto"))
			faxtxmethod = FAX_TX_T38_AUTO;
		    else if (!strcasecmp(v->value, "passthrough_auto"))
			faxtxmethod = FAX_TX_PASSTHROUGH_AUTO;
		    else if (!strcasecmp(v->value, "passthrough_force"))
			faxtxmethod = FAX_TX_PASSTHROUGH_FORCE;
		} else if (!strcasecmp(v->name, "adsi")) {
			adsi = ast_true(v->value);
		} else if (!strcasecmp(v->name, "transfermode")) {
			if (!strcasecmp(v->value, "off"))
			    	transfermode = TRANSFER_OFF;
			else if (!strcasecmp(v->value, "signalling"))
			    	transfermode = TRANSFER_SIGNALLING;
			else if (!strcasecmp(v->value, "bridging"))
			    	transfermode = TRANSFER_BRIDGING;
			else
			    	ast_log(LOG_WARNING, "Unknown transfer mode '%s'\n", v->value);
		} else if (!strcasecmp(v->name, "canpark")) {
			canpark = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callprogress")) {
			if (ast_true(v->value))
				callprogress |= 1;
			else
				callprogress &= ~1;
		} else if (!strcasecmp(v->name, "faxdetect")) {
			if (!strcasecmp(v->value, "incoming")) {
				callprogress |= 4;
				callprogress &= ~2;
			} else if (!strcasecmp(v->value, "outgoing")) {
				callprogress &= ~4;
				callprogress |= 2;
			} else if (!strcasecmp(v->value, "both") || ast_true(v->value))
				callprogress |= 6;
			else
				callprogress &= ~6;
		} else if (!strcasecmp(v->name, "hidecallerid")) {
			hidecallerid = ast_true(v->value);
	    } else if (!strcasecmp(v->name, "callreturn")) {
			callreturn = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callwaiting")) {
			callwaiting = ast_true(v->value);
		} else if (!strcasecmp(v->name, "callwaitingcallerid")) {
			callwaitingcallerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "context")) {
			ast_copy_string(context, v->value, sizeof(context));
		} else if (!strcasecmp(v->name, "language")) {
			ast_copy_string(language, v->value, sizeof(language));
		} else if (!strcasecmp(v->name, "progzone")) {
			ast_copy_string(progzone, v->value, sizeof(progzone));
		} else if (!strcasecmp(v->name, "musiconhold")) {
			ast_copy_string(musicclass, v->value, sizeof(musicclass));
		} else if (!strcasecmp(v->name, "internalmoh")) {
			internalmoh = ast_true(v->value);
		} else if (!strcasecmp(v->name, "group")) {
			cur_group = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "callgroup")) {
			cur_callergroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "pickupgroup")) {
			cur_pickupgroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "immediate")) {
			immediate = ast_true(v->value);
 		} else if (!strcasecmp(v->name, "transfertobusy")) {
 			transfertobusy = ast_true(v->value);
		} else if (!strcasecmp(v->name, "rxgain")) {
			if (sscanf(v->value, "%f", &rxgain) != 1) {
				ast_log(LOG_WARNING, "Invalid rxgain: %s\n", v->value);
			}
		} else if (!strcasecmp(v->name, "txgain")) {
			if (sscanf(v->value, "%f", &txgain) != 1) {
				ast_log(LOG_WARNING, "Invalid txgain: %s\n", v->value);
			}
		} else if (!strcasecmp(v->name, "tonezone")) {
			if (sscanf(v->value, "%d", &tonezone) != 1) {
				ast_log(LOG_WARNING, "Invalid tonezone: %s\n", v->value);
			}
		} else if (!strcasecmp(v->name, "callerid")) {
			if (!strcasecmp(v->value, "asreceived")) {
				cid_num[0] = '\0';
				cid_name[0] = '\0';
			} else {
				ast_callerid_split(v->value, cid_name, sizeof(cid_name), cid_num, sizeof(cid_num));
			}
		} else if (!strcasecmp(v->name, "useincomingcalleridonjdsptransfer")) {
			jdsptrcallerid = ast_true(v->value);
		} else if (!strcasecmp(v->name, "accountcode")) {
			ast_copy_string(accountcode, v->value, sizeof(accountcode));
		} else if (!strcasecmp(v->name, "dialingtimeout")) {
			matchdigittimeout = atoi(v->value);
		} else if(!reload){ 
			 if (!strcasecmp(v->name, "signalling")) {
				if (!strcasecmp(v->value, "fxo_ls")) {
					cur_signalling = SIG_FXOLS;
				} else if (!strcasecmp(v->value, "fxo_gs")) {
					cur_signalling = SIG_FXOGS;
				} else {
					ast_log(LOG_ERROR, "Unknown signalling method '%s'\n", v->value);
				}
			} 
		} else 
			ast_log(LOG_WARNING, "Ignoring %s\n", v->name);
		v = v->next;
	}
	ast_mutex_unlock(&iflock);
	ast_config_destroy(cfg);
	/* And start the monitor for the first time */
	restart_monitor();
	return 0;
}

int load_module(void)
{
	int res;

	sched = sched_context_create();
	if (!sched) {
		ast_log(LOG_ERROR, "Unable to create schedule context\n");
		return -1;
	}

	res = setup_jdsp(0);
	/* Make sure we can register our JDSP channel type */
	if(res) {
	  return -1;
	}
	if (ast_channel_register(&jdsp_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel class %s\n", type);
		__unload_module();
		return -1;
	}
	ast_cli_register_multiple(jdsp_cli, sizeof(jdsp_cli) / sizeof(jdsp_cli[0]));
	
	memset(round_robin, 0, sizeof(round_robin));
	return res;
}

int reload(void)
{
	int res = 0;

	res = setup_jdsp(1);
	if (res) {
		ast_log(LOG_WARNING, "Reload of chan_jdsp.so is unsuccessful!\n");
		return -1;
	}
	return 0;
}

int usecount()
{
	return usecnt;
}

char *description()
{
	return (char *) desc;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}

static void jdsp_pre_bridge(struct ast_channel *chan)
{
	struct jdsp_pvt *p = chan->tech_pvt;
	int index;

	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(chan, p, 0);
	if (index == SUB_REAL || p->subs[index].inthreeway)
		jdsp_start_audio(chan, index);
	ast_mutex_unlock(&p->lock);
}

static void jdsp_post_bridge(struct ast_channel *chan)
{
	struct jdsp_pvt *p = chan->tech_pvt;
	int index;

	ast_mutex_lock(&p->lock);
	index = jdsp_get_index(chan, p, 0);
	if (index == SUB_REAL || p->subs[index].inthreeway)
		jdsp_stop_audio(chan, index);
	ast_mutex_unlock(&p->lock);
}
