/****************************************************************************
 *
 * rg/pkg/voip/asterisk/channels/chan_jfxo.c
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
 * jfxo abstraction layer interface
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <errno.h>
#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.9 $")

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/config.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/options.h"
#include "asterisk/file.h"
#include "asterisk/causes.h"
#include "asterisk/jdsp_common.h"

#include <util/openrg_gpl.h>
#include <voip/dsp/phone.h>
#include <kos_chardev_id.h>
#include <rg_ioctl.h>

/* XXX Should vary according to DSP. Currently we configure SIP and H.323 to
 * negotiate only the codecs that are supported by the DSP, so it doesn't do
 * any harm to pretend that the DSP supports all the formats. */
/* XXX Must be configurable */
#define NATIVE_FORMATS AST_FORMAT_ULAW

/* Waits between offhook and dialing the first DTMF (in ms) */
#define JFXO_WAIT_FOR_DIALTONE 800
#define JFXO_WAIT_DIALING_TOGGLE 200
#define MINHOOK_ON_TIME 1000

static const char desc[] = "Jungo FXO Abstraction Layer";
static const char tdesc[] = "Jungo FXO Abstraction Layer Driver";
static const char type[] = "jfxo";
static const char config[] = "jfxo.conf";

static int global_native_formats = 0;

/*! \brief Protect the interface list (of jfxo_pvt's) */
AST_MUTEX_DEFINE_STATIC(iflock);

static int usecnt = 0;
AST_MUTEX_DEFINE_STATIC(usecnt_lock);

/*! \brief Protect the monitoring thread, so only one process can kill or start
 * it, and not when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(monlock);

/*! \brief This is the thread for the monitor which checks for input on the
 * channels which are not currently in use.  */
static pthread_t monitor_thread = AST_PTHREADT_NULL;

static struct sched_context *sched;

static struct ast_channel *jfxo_request(const char *type, int format, void *data, int *cause);
static int jfxo_digit_begin(struct ast_channel *ast, char digit);
static int jfxo_digit_end(struct ast_channel *ast, char digit);
static int jfxo_call(struct ast_channel *ast, char *rdest, int timeout);
static int jfxo_hangup(struct ast_channel *ast);
static int jfxo_answer(struct ast_channel *ast);
struct ast_frame *jfxo_read(struct ast_channel *ast);
static int jfxo_write(struct ast_channel *ast, struct ast_frame *frame);
struct ast_frame *jfxo_exception(struct ast_channel *ast);
static int jfxo_indicate(struct ast_channel *chan, int condition);
static int jfxo_setoption(struct ast_channel *chan, int option, void *data, int datalen);
static void jfxo_pre_bridge(struct ast_channel *chan);
static void jfxo_post_bridge(struct ast_channel *chan);

static const struct ast_channel_tech jfxo_tech = {
	.type = type,
	.description = tdesc,
	.capabilities = NATIVE_FORMATS,
	.requester = jfxo_request,
	.call = jfxo_call,
	.send_digit_begin = jfxo_digit_begin,
	.send_digit_end = jfxo_digit_end,
	.hangup = jfxo_hangup,
	.answer = jfxo_answer,
	.read = jfxo_read,
	.write = jfxo_write,
	.exception = jfxo_exception,
	.indicate = jfxo_indicate,
	.setoption = jfxo_setoption,
	.pre_bridge = jfxo_pre_bridge,
	.post_bridge = jfxo_post_bridge,
};

#define RTP_HEADER_SIZE 12

/*! Chunk size to read -- we use 20ms chunks to make things happy.  */   
#define READ_SIZE (160 + RTP_HEADER_SIZE)

struct jfxo_pvt;

typedef void (*hookstate_set_cb_t)(struct jfxo_pvt *data);

static struct jfxo_pvt {
	ast_mutex_t lock;
	struct ast_channel *owner;			/*!< Our current active owner (if applicable) */
	int jfd;
	int dfd;

	char buffer[AST_FRIENDLY_OFFSET + READ_SIZE];
	unsigned short seqno;
	unsigned long lastts;
	struct timeval txcore;
	
	/* Dialing repeater state machine */
	char extentodial[AST_MAX_EXTENSION];
	int dialindex;
	int dialtoggle;
	int dial_sched_id;

	/* These are meant to force delay of 1 second (MINHOOK_ON_TIME) between
	 * hook on and hook off to prevent flash detection on the other side */
	int offhook_sched_id;
	struct timeval lasthookontime;
	hookstate_set_cb_t hook_state_cb;
	
	char context[AST_MAX_CONTEXT];
	/* The phone module channel number */ 
	int channel;
	struct ast_frame f;
	int voice_on;
	int offhook;
	struct jfxo_pvt *next;
} *iflist = NULL;

static int ifcount;

static char context[AST_MAX_CONTEXT] = "default";

static int restart_monitor(void);

static inline int jfxo_get_event(int fd, phone_event_t *event)
{
	if (read(fd, event, sizeof(*event)) < 0)
		return -1;
	return 0;
}

struct ast_frame *jfxo_exception(struct ast_channel *ast)
{
	return NULL;
}

static int jfxo_setoption(struct ast_channel *chan, int option, void *data, int datalen)
{
	return 0;
}

static int jfxo_indicate(struct ast_channel *chan, int condition)
{
	return 0;
}

static void jfxo_stop_audio(struct ast_channel *ast)
{
	struct jfxo_pvt *p = ast->tech_pvt;

	if (!p->voice_on)
		return;

	if (ioctl(p->jfd, VOIP_DSP_STOP, 0) < 0)
	{
		ast_log(LOG_ERROR, "Failed to stop audio on channel %s, fd %d"
		    "\n", ast->name, p->jfd);
	}
	p->voice_on = 0;
}

static int jfxo_play_tone(struct jfxo_pvt *p, phone_tone_t tone);
	
struct ast_frame *jfxo_read(struct ast_channel *ast)
{
	struct jfxo_pvt *p = ast->tech_pvt;
	phone_event_t ev;
	int fd = ast->fds[ast->fdno];

	if (!p)
		return 0;

	ast_mutex_lock(&p->lock);
	p->f.frametype = AST_FRAME_NULL;
	p->f.datalen = 0;
	p->f.samples = 0;
	p->f.mallocd = 0;
	p->f.offset = 0;
	p->f.subclass = 0;
	p->f.delivery = ast_tv(0,0);
	p->f.src = __FUNCTION__;
	p->f.data = NULL;

	/* If we got a voice packet */
	if (fd == p->dfd)
	{
		int ast_format, res;
		unsigned char *readbuf;

		readbuf = p->buffer + AST_FRIENDLY_OFFSET;
		CHECK_BLOCKING(ast);
		res = read(fd, readbuf, READ_SIZE);
		ast_clear_flag(ast, AST_FLAG_BLOCKING);
		if (res < 0)
		{
			ast_mutex_unlock(&p->lock);
		    	return NULL;
		}

		ast_format = jdsp_codec_rtp2ast(readbuf);
		/* Unknown voice format - therefore this is not voice data */
		if (ast_format == -1)
		{
			ast_mutex_unlock(&p->lock);
			return &p->f;
		}
		p->f.datalen = res - RTP_HEADER_SIZE;
		p->f.frametype = AST_FRAME_VOICE;
		p->f.subclass = ast_format;
		p->f.offset = AST_FRIENDLY_OFFSET + RTP_HEADER_SIZE;
		p->f.data = readbuf + RTP_HEADER_SIZE;
		p->f.samples = ast_codec_get_samples(&p->f);
		ast_mutex_unlock(&p->lock);
		return &p->f;
	}

	if (jfxo_get_event(p->jfd, &ev) < 0)
	{
		ast_log(LOG_WARNING, "No event\n");
		ast_mutex_unlock(&p->lock);
		return NULL;
	}
	switch (ev.key)
	{
	case PHONE_KEY_RING_OFF:
		if (ast->_state == AST_STATE_UP)
			break;
		ast_mutex_unlock(&p->lock);
		return NULL;
	case PHONE_KEY_HOOK_ON:
		jfxo_play_tone(p, PHONE_TONE_NONE);
		ast_mutex_unlock(&p->lock);
		return NULL;
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
		if (ev.pressed)
		{
			char dtmf_key = jdsp_key2char(ev.key);

			ast_log(LOG_DEBUG, "DTMF Down '%c'\n", dtmf_key);
			p->f.frametype = AST_FRAME_DTMF;
			p->f.subclass = dtmf_key;
		}
		break;
	default:
		break;
	}

	ast_mutex_unlock(&p->lock);

	return &p->f;
}

static unsigned int calc_txstamp(struct jfxo_pvt *s, struct timeval *delivery)
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

static int my_jfxo_write(struct jfxo_pvt *p, struct ast_frame *frame)
{
	int fd;
	unsigned int *rtpheader;
	unsigned long ts;

	fd = p->dfd;
	if (fd == -1)
		return 0;

	rtpheader = (unsigned int *)((char *)frame->data - RTP_HEADER_SIZE);
	rtpheader[0] = htonl((2 << 30) | (0 << 23) |\
	    (jdsp_codec_ast2rtp(frame->subclass) << 16) | (p->seqno + 1));
	ts = p->lastts + calc_txstamp(p, &frame->delivery) * 8;
	rtpheader[1] = htonl(ts);
	rtpheader[2] = 0; /* The DSP don't care about the ssrc */
	if (!write(fd, rtpheader, frame->datalen + RTP_HEADER_SIZE))
	{
		p->lastts = ts;
		p->seqno++;
	}

	return frame->datalen;
}

static void jfxo_start_audio(struct ast_channel *ast);

static int jfxo_write(struct ast_channel *ast, struct ast_frame *frame)
{
	struct jfxo_pvt *p = ast->tech_pvt;
	int res = 0;

	if (frame->frametype != AST_FRAME_VOICE)
		return 0;
    
	/* We may receive voice frames before start_audio was called. This occurs
	 * for example when some recording is played (e.g. voicemail or
	 * auto-attendant). In such cases there is no bridged channel, so the
	 * pre_bridge function (which calls start_audio) hasn't been called. */
	if (p->offhook && !p->voice_on)
	{
		ast_mutex_lock(&p->lock);
		jfxo_start_audio(ast);
		ast_mutex_unlock(&p->lock);
	}
	if (!(frame->subclass & ast->nativeformats)) {
		ast_log(LOG_WARNING, "Cannot handle frames in %d format\n",
		    frame->subclass);
		return -1;
	}
	if (!p->owner) {
		if (option_debug)
			ast_log(LOG_DEBUG, "Dropping frame since there is no "
			    "active owner on %s...\n",ast->name);
		return 0;
	}
	/* Return if it's not valid data */
	if (!frame->data || !frame->datalen)
		return 0;

	if (frame->subclass != AST_FORMAT_SLINEAR)
		res = my_jfxo_write(p, frame);
	if (res < 0) {
		ast_log(LOG_WARNING, "write failed: %s\n", strerror(errno));
		return -1;
	} 

	return 0;
}

/*
 * parse the dialstring
 */
static void parse_dialstring(char *buffer, char **interface, char **dest,
    char **param)
{
	int cp = 0;
	char *buffer_p = buffer;

	/* interface is the first part of the string */
	*interface = buffer;

	*dest = "";
	*param = "";

	while (*buffer_p) {
		if (*buffer_p == '/' || *buffer_p == '@') {
			*buffer_p = 0;
			buffer_p++;
			if (cp == 0) {
				*dest = buffer_p;
				cp++;
			} else if (cp == 1) {
				*param = buffer_p;
				cp++;
			} else {
				ast_log(LOG_WARNING, "Too many parts in "
				    "dialstring '%s'\n", buffer);
			}
			continue;
		}
		buffer_p++;
	}
	return;
}

static struct ast_channel *jfxo_new(struct jfxo_pvt *i, int state)
{
	struct ast_channel *tmp;
	int deflaw = AST_FORMAT_ULAW;

	if (i->owner) {
		ast_log(LOG_WARNING, "Channel %d already has a call\n",
		    i->channel);
		return NULL;
	}
	tmp = ast_channel_alloc(1);
	if (tmp) {
		tmp->tech = &jfxo_tech;
		tmp->type = type;
		tmp->fds[0] = i->jfd;
		tmp->fds[1] = i->dfd;
		tmp->nativeformats = global_native_formats;
		/* Start out assuming ulaw since it's smaller :) */
		tmp->rawreadformat = deflaw;
		tmp->readformat = deflaw;
		tmp->rawwriteformat = deflaw;
		tmp->writeformat = deflaw;
		if (state == AST_STATE_RINGING)
			tmp->rings = 1;
		tmp->tech_pvt = i;
		i->owner = tmp;
		ast_copy_string(tmp->context, i->context, sizeof(tmp->context));
		snprintf(tmp->name, sizeof(tmp->name), "jfxo/%d-1", i->channel);
		ast_setstate(tmp, state);
		ast_mutex_lock(&usecnt_lock);
		usecnt++;
		ast_mutex_unlock(&usecnt_lock);
		ast_update_use_count();
	} else
		ast_log(LOG_WARNING, "Unable to allocate channel structure\n");
	return tmp;
}

static void jfxo_start_audio(struct ast_channel *ast)
{
	struct jfxo_pvt *p = ast->tech_pvt;
	voip_dsp_record_args_t args;
	voip_dsp_bind_arg_t bind_arg;

	ast_log(LOG_DEBUG, "Starting audio on channel %s\n", ast->name);

	if (p->voice_on)
		return;

	args.channel = 0;
	args.codec = jdsp_codec_ast2rtp(ast->readformat);
	args.ptime_ms = ast->ptime ? ast->ptime : 20;
	args.suppress_dtmf = 0;
	args.rtp_context = 0;

	if (p->dfd == -1)
	{
		ast_log(LOG_ERROR, "It's impossible - Starting audio on channel"
		    "%s\n", ast->name);
	}
	else
	{
		bind_arg.line = p->channel - 1;
		bind_arg.channel = args.channel;
		ioctl(p->dfd, VOIP_DSP_BIND, &bind_arg);
	}
	
	if (ioctl(p->jfd, VOIP_DSP_START, &args) < 0) {
		ast_log(LOG_ERROR, "Failed to start audio on channel %s\n",
		    ast->name);
		return;
	}
	p->voice_on = 1;
	p->seqno = 0;
}

static int do_offhook_cb(void *data)
{
	struct jfxo_pvt *p = data;

	ast_mutex_lock(&p->lock);
	if (p->offhook_sched_id == -1)
	{
		ast_mutex_unlock(&p->lock);
		return 0;
	}
		
	p->offhook = 1;
	if (ioctl(p->jfd, VOIP_FXO_SET_HOOK, VOIP_OFFHOOK) < 0)
		ast_log(LOG_ERROR, "Failed to set the hook offhook\n");

	if (p->hook_state_cb)
		p->hook_state_cb(p);
	p->offhook_sched_id = -1;
	ast_mutex_unlock(&p->lock);

	return 0;
}

static void jfxo_set_hook_state(struct jfxo_pvt *p, int state,
    hookstate_set_cb_t cb)
{
	struct timeval timenow = ast_tvnow();
	
	if (state == VOIP_ONHOOK)
	{
		if (p->offhook_sched_id > -1) 
		{
			ast_sched_del(sched, p->offhook_sched_id);
			p->offhook_sched_id = -1;
		} 
		else
		{
			p->lasthookontime = timenow;
			
			p->offhook = 0;
			if (ioctl(p->jfd, VOIP_FXO_SET_HOOK, VOIP_ONHOOK) < 0)
			{
				ast_log(LOG_ERROR, "Failed to set line on"
				    "hook\n");
				return;
			}
		}
	}
	else
	{
		if (p->offhook_sched_id > -1)
		{
			ast_log(LOG_WARNING, "Hooked off twice on fxo\n");
			return;
		}
		p->hook_state_cb = cb;
		if (ast_tvdiff_ms(timenow, p->lasthookontime) < MINHOOK_ON_TIME)
		{
			p->offhook_sched_id = ast_sched_add(sched,
			    MINHOOK_ON_TIME - ast_tvdiff_ms(timenow,
			    p->lasthookontime), do_offhook_cb, p);
		}
		else
		{
			p->offhook_sched_id = ast_sched_add(sched, 1,
			    do_offhook_cb, p);
		}
		restart_monitor();
	}
}

static int jfxo_answer(struct ast_channel *ast)
{
	struct jfxo_pvt *p = ast->tech_pvt;
	
	ast_setstate(ast, AST_STATE_UP);
	ast_mutex_lock(&p->lock);
	jfxo_set_hook_state(p, VOIP_OFFHOOK, NULL);
	ast_mutex_unlock(&p->lock);

	return 0;
}

static int jfxo_play_tone(struct jfxo_pvt *p, phone_tone_t tone)
{
	return ioctl(p->jfd, VOIP_SLIC_TONE, tone);
}

static void jfxo_play_digit(struct jfxo_pvt *p, char digit)
{
	phone_tone_t tone;

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

	jfxo_play_tone(p, tone);
}

static int jfxo_digit_end(struct ast_channel *ast, char digit)
{
	struct jfxo_pvt *p;

	p = ast->tech_pvt;
	ast_mutex_lock(&p->lock);
	ioctl(p->jfd, VOIP_SLIC_TONE, PHONE_TONE_NONE);
	ast_mutex_unlock(&p->lock);

	return 0;
}

static int jfxo_digit_begin(struct ast_channel *ast, char digit)
{
	struct jfxo_pvt *p;
	
	p = ast->tech_pvt;
	ast_mutex_lock(&p->lock);
	jfxo_play_digit(p, digit);
	ast_mutex_unlock(&p->lock);
	restart_monitor();
	
	return 0;
}

static void jfxo_queue_frame(struct jfxo_pvt *p, struct ast_frame *f)
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

static void jfxo_queue_control(struct ast_channel *ast, int control)
{
	struct ast_frame f = { AST_FRAME_CONTROL, };
	f.subclass = control;
	return jfxo_queue_frame(ast->tech_pvt, &f);
}

/* XXX Should be made a thread */
static int jfxo_dial(void *data)
{
	struct jfxo_pvt *p = data;
	int ret = 1;

	ast_mutex_lock(&p->lock);
	if (!p->owner)
		ast_log(LOG_WARNING, "Oops the channel is gone\n");

	/* dialtoggle == 2 after the waiting for dialtone (before the first 
	 * digit) */
	if (p->dialtoggle == 2)
	{
		p->dialtoggle = 0;
		ast_log(LOG_DEBUG, "Waited for dial tone\n");
		p->dial_sched_id = ast_sched_add(sched,
		    JFXO_WAIT_DIALING_TOGGLE, jfxo_dial, p);
		ret = 0;
	}

	if (p->dialindex == strlen(p->extentodial))
	{
	    	jfxo_queue_control(p->owner, AST_CONTROL_ANSWER);
	   	ret = 0;
	}
	else if (p->dialtoggle)
	{
		ast_log(LOG_DEBUG, "Stop playing digit\n");
		jfxo_play_tone(p, PHONE_TONE_NONE);
		p->dialindex++;
	}
	else
	{
		ast_log(LOG_DEBUG, "Play digit %c\n",
		    p->extentodial[p->dialindex]);
		jfxo_play_digit(p, p->extentodial[p->dialindex]);
	}

	p->dialtoggle = !p->dialtoggle;
	ast_mutex_unlock(&p->lock);

	return ret;
}

static void jfxo_schedule_dialing(struct jfxo_pvt *p)
{
	p->dialtoggle = 2;
	p->dial_sched_id = ast_sched_add(sched, JFXO_WAIT_FOR_DIALTONE,
	    jfxo_dial, p);
}

static int jfxo_call(struct ast_channel *ast, char *rdest, int timeout)
{
	struct jfxo_pvt *p = ast->tech_pvt;

	if (!p)
	{
		ast_log(LOG_WARNING, "Asked to call channel not connected\n");
		return 0;
	}
	
	ast_mutex_lock(&p->lock);

	if (ast->_state == AST_STATE_BUSY)
	{
		ast_queue_control(ast, AST_CONTROL_BUSY);	
		ast_mutex_unlock(&p->lock);
		return 0;
	}

	if (ast_strlen_zero(p->extentodial))
	{
	    	/* XXX Why do we need this? */
		jfxo_set_hook_state(p, VOIP_OFFHOOK, NULL);
		ast_setstate(ast, AST_STATE_UP);
		ast_queue_control(ast, AST_CONTROL_ANSWER);	
	}
	else
		jfxo_set_hook_state(p, VOIP_OFFHOOK, jfxo_schedule_dialing);

	ast_mutex_unlock(&p->lock);
	return 0;
}

static int jfxo_hangup(struct ast_channel *ast)
{
	struct jfxo_pvt *p = ast->tech_pvt;

	if (!p)
	{
		ast_log(LOG_WARNING, "Asked to hangup channel not connected\n");
		return 0;
	}

	ast_mutex_lock(&p->lock);

	if (p->owner)
		p->owner = NULL;
	
	if (p->voice_on)
		jfxo_stop_audio(ast);
	p->dialindex = 0;
	p->extentodial[0] = '\0';
	p->dialtoggle = 0;
	if (p->dial_sched_id > -1)
		ast_sched_del(sched, p->dial_sched_id);
	p->dial_sched_id = -1;
	jfxo_play_tone(p, PHONE_TONE_NONE);
	jfxo_set_hook_state(p, VOIP_ONHOOK, NULL);
	ast_mutex_unlock(&p->lock);
	ast_mutex_lock(&usecnt_lock);
	usecnt--;
	if (usecnt < 0) 
		ast_log(LOG_WARNING, "Usecnt < 0???\n");
	ast_mutex_unlock(&usecnt_lock);
	ast_update_use_count();
	ast->tech_pvt = NULL;

	return 0;
}

static struct ast_channel *jfxo_request(const char *type, int format,
    void *data, int *cause)
{
	struct ast_channel *tmp = NULL;
	char *buffer;
	char *channel;
	char *dest;
	char *params;
	struct jfxo_pvt *p;
	int chan;

	if (!(format & global_native_formats))
	{
		ast_log(LOG_NOTICE, "Asked to get a channel of unsupported "
		    "format '%d'\n", format);
		return NULL;
	}
	buffer = ast_strdupa((char *)data);
	parse_dialstring(buffer, &channel, &dest, &params);
	chan = atoi(channel);
	ast_mutex_lock(&iflock);
	for (p = iflist; p; p = p->next)
	{
		if (chan == p->channel)
			break;
	}
	if (p)
	{
		ast_mutex_lock(&p->lock);
		if (p->owner)
			*cause = AST_CAUSE_BUSY;
		else
		{
			tmp = jfxo_new(p, AST_STATE_RESERVED);
			ast_copy_string(p->extentodial, dest,
			    sizeof(p->extentodial));
		}
		ast_mutex_unlock(&p->lock);
	}

	ast_mutex_unlock(&iflock);

	return tmp;
}

static void destroy_jfxo_pvt(struct jfxo_pvt **pvt)
{
	struct jfxo_pvt *p = *pvt;

	/* Remove channel from the list */
	*pvt = p->next;
	ifcount--;
	ast_mutex_destroy(&p->lock);
	free(p);
}

int unload_module(void)
{
	int x = 0;
	struct jfxo_pvt *p, *pl;
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
		if (monitor_thread && (monitor_thread != AST_PTHREADT_STOP)
		    && (monitor_thread != AST_PTHREADT_NULL)) {
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
			/* Close the jfxo thingy */
			if (p->jfd > -1)
				close(p->jfd);
			if (p->dfd > -1)
				close(p->dfd);

			pl = p;
			p = p->next;
			x++;
			/* Free associated memory */
			if(pl)
				destroy_jfxo_pvt(&pl);
			ast_verbose(VERBOSE_PREFIX_3 "Unregistered channel "
			    "%d\n", x);
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

static int handle_init_event(struct jfxo_pvt *i, phone_event_t *event)
{
	int res = 0;
	struct ast_channel *chan;

	ast_log(LOG_DEBUG, "Got event \"%s\" on channel %d\n",
	    jdsp_event2str(event), i->channel);
	/* Handle an event on a given channel for the monitor thread. */
	switch(event->key) {
	case PHONE_KEY_RING_ON:
	    /* Handle incoming call */
		if (!i->owner)
		{
			i->owner = jfxo_new(i, AST_STATE_RINGING);
			if (ast_exists_extension(i->owner, i->context, "s", 1, NULL))
			{
				if (ast_pbx_start(i->owner)) {
					ast_log(LOG_WARNING, "Unable to start PBX on %s\n", i->owner->name);
					ast_hangup(i->owner);
					i->owner = NULL;
				}
			}
		}
		break;
	case PHONE_KEY_HOOK_OFF:
	case PHONE_KEY_FLASH:
		/* Check for callerid, digits, etc */
		chan = jfxo_new(i, AST_STATE_RESERVED);
		if (chan) {
			res = jfxo_play_tone(i, PHONE_TONE_DIAL);
			if (res < 0) 
				ast_log(LOG_WARNING, "Unable to play dialtone on channel %d\n", i->channel);
		} else
			ast_log(LOG_WARNING, "Unable to create channel\n");
		break;
	case PHONE_KEY_HOOK_ON:
		res = jfxo_play_tone(i, PHONE_TONE_NONE);
		break;
	default:
		ast_verbose("Don't know how to handle \"%s\" event on channel "
		    "%d\n", jdsp_event2str(event), i->channel);
	}
	return res;
}

static void *do_monitor(void *data)
{
	int count, res, spoint, pollres=0;
	struct jfxo_pvt *i;
	int found;
	struct pollfd *pfds=NULL;
	int lastalloc = -1;
	/* This thread monitors all the frame relay interfaces which are not yet
	 * in use (and thus do not have a separate thread) indefinitely. From 
	 * here on out, we die whenever asked */
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
					ast_log(LOG_WARNING, "Critical memory "
					    "error.  jfxo dies.\n");
					ast_mutex_unlock(&iflock);
					return NULL;
				}
			}
			lastalloc = ifcount;
		}
		/* Build the stuff we're going to poll on, that is the socket of
		 * every jfxo_pvt that does not have an associated owner 
		 * channel */
		count = 0;
		i = iflist;
		while(i) {
			if (i->jfd > -1 && !i->owner) {
				/* This needs to be watched, as it lacks an 
				 * owner */
				pfds[count].fd = i->jfd;
				pfds[count].events = POLLIN;
				pfds[count].revents = 0;
				count++;
			}
			i = i->next;
		}
		/* Okay, now that we know what to do, release the interface
		 * lock */
		ast_mutex_unlock(&iflock);

		pthread_testcancel();
		res = ast_sched_wait(sched);
		if ((res < 0) || (res > 1000))
			res = 1000;
		/* Wait at most a second for something to happen */
		res = poll(pfds, count, res);
		pthread_testcancel();
		/* Okay, poll has finished.  Let's see what happened */
		if (res < 0) {
			if ((errno != EAGAIN) && (errno != EINTR))
				ast_log(LOG_WARNING, "poll return %d: %s\n",
				    res, strerror(errno));
			continue;
		}
		ast_mutex_lock(&monlock);
		ast_sched_runq(sched);
		ast_mutex_unlock(&monlock);
		/* Alright, lock the interface list again, and let's look and 
		 * see what has happened */
		if (ast_mutex_lock(&iflock)) {
			ast_log(LOG_WARNING, "Unable to lock the interface list"
			    "\n");
			continue;
		}
		found = 0;
		spoint = 0;
		i = iflist;
		while(i) {
			if (i->jfd > -1) {
				pollres = ast_fdisset(pfds, i->jfd, count,
				    &spoint);
				if (pollres & POLLIN) {
					phone_event_t ev;
					if (i->owner) {
						ast_log(LOG_WARNING, "Whoa.... "
						    "I'm owned but found (%d) "
						    "in read...\n", i->jfd);
						i = i->next;
						continue;
					}

					/* Error may arise if the driver has
					 * detected a double key release.
					 * Ignore it and continue */
					if (jfxo_get_event(i->jfd, &ev) < 0)
						continue;

					if (option_debug)
						ast_log(LOG_DEBUG, "Monitor got"
						    " event %s on channel %d\n",
						    jdsp_event2str(&ev),
						    i->channel);
					/* Don't hold iflock while handling 
					 * init events -- race with chlock */
					ast_mutex_unlock(&iflock);
					ast_mutex_lock(&i->lock);
					handle_init_event(i, &ev);
					ast_mutex_unlock(&i->lock);
					ast_mutex_lock(&iflock);	
				}
			}
			i=i->next;
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
		pthread_kill(monitor_thread, SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create(&monitor_thread, &attr, do_monitor, NULL)
		    < 0) {
			ast_mutex_unlock(&monlock);
			ast_log(LOG_ERROR, "Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&monlock);

	return 0;
}

static int jfxo_open_dsp(int channel)
{
    	int fd;

    	fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_DSP, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		ast_log(LOG_WARNING, "Unable to open jfxo kos char device\n");
		return -1;
	}
	
	return fd;
}

static int jfxo_open_slic(int channel)
{
	int fd;

	fd = gpl_sys_rg_chrdev_open(KOS_CDT_VOIP_SLIC, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		ast_log(LOG_WARNING, "Unable to open jfxo kos char device\n");
		return -1;
	}

	if (ioctl(fd, VOIP_SLIC_BIND, channel - 1) < 0)
	{
		ast_log(LOG_WARNING, "Invalid channel number '%d'\n", channel);
		close(fd);
		return -1;
	}

	return fd;
}

static struct jfxo_pvt *mkintf(int channel)
{
	/* Make a jfxo_pvt structure for this interface (or CRV if "pri" is 
	 * specified) */
	struct jfxo_pvt *tmp = NULL;

	tmp = (struct jfxo_pvt*)malloc(sizeof(struct jfxo_pvt));
	if (!tmp) {
		ast_log(LOG_ERROR, "MALLOC FAILED\n");
		destroy_jfxo_pvt(&tmp);
		return NULL;
	}
	memset(tmp, 0, sizeof(struct jfxo_pvt));
	ast_mutex_init(&tmp->lock);
	ifcount++;
	tmp->channel = channel;

	/* Open non-blocking */
	tmp->jfd = jfxo_open_slic(channel);
	tmp->dfd = jfxo_open_dsp(channel);
	/* Allocate a jfxo structure */
	if (tmp->jfd < 0) {
		ast_log(LOG_ERROR, "Unable to open channel %d:\n", channel);
		destroy_jfxo_pvt(&tmp);
		return NULL;
	}
	ast_copy_string(tmp->context, context, sizeof(tmp->context));
	tmp->dial_sched_id = -1;
	tmp->offhook_sched_id = -1;
	tmp->lasthookontime = ast_tvnow();

	return tmp;
}

static int setup_jfxo(void)
{
	struct ast_config *cfg;
	struct ast_variable *v;
	struct jfxo_pvt *tmp;
	char *chan;
	int channel;

	cfg = ast_config_load(config);

	/* We *must* have a config file otherwise stop immediately */
	if (!cfg) {
		ast_log(LOG_ERROR, "Unable to load config %s\n", config);
		return -1;
	}


	if (ast_mutex_lock(&iflock)) {
		/* It's a little silly to lock it, but we mind as well just to
		 * be sure */
		ast_log(LOG_ERROR, "Unable to lock interface list???\n");
		return -1;
	}
	v = ast_variable_browse(cfg, "channels");
	while(v) {
		/* Create the interface list */
		if (!strcasecmp(v->name, "channel")) { 
			chan = v->value;
			if (!sscanf(chan, "%d", &channel)) {
				ast_log(LOG_ERROR, "Syntax error parsing '%s' "
				    "at '%s'\n", v->value, chan);
				ast_config_destroy(cfg);
				ast_mutex_unlock(&iflock);
				return -1;
			}
			tmp = mkintf(channel);

			if (tmp) {
				tmp->next = iflist;
				iflist = tmp;
			} else {
				ast_config_destroy(cfg);
				ast_mutex_unlock(&iflock);
				return -1;
			}
		} else if (!strcasecmp(v->name, "context")) {
			ast_copy_string(context, v->value, sizeof(context));
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(NULL, &global_native_formats, v->value, 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(NULL, &global_native_formats, v->value, 0);
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

	res = setup_jfxo();
	/* Make sure we can register our jfxo channel type */
	if (res)
		return -1;

	if (ast_channel_register(&jfxo_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel class %s\n",
		    type);
		unload_module();
		return -1;
	}
	return res;
}

int reload(void)
{
	ast_log(LOG_WARNING, "chan_jfxo.so doesn't support reload!\n");

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

static void jfxo_pre_bridge(struct ast_channel *chan)
{
	struct jfxo_pvt *p = chan->tech_pvt;

	ast_mutex_lock(&p->lock);
	jfxo_start_audio(chan);
	ast_mutex_unlock(&p->lock);
}

static void jfxo_post_bridge(struct ast_channel *chan)
{
	struct jfxo_pvt *p = chan->tech_pvt;

	ast_mutex_lock(&p->lock);
	jfxo_stop_audio(chan);
	ast_mutex_unlock(&p->lock);
}
