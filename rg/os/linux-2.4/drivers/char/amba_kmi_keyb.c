/*
 *  linux/drivers/char/amba_kmi_keyb.c
 *
 *  AMBA Keyboard and Mouse Interface Driver
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  This keyboard driver drives a PS/2 keyboard and mouse connected
 *  to the KMI interfaces.  The KMI interfaces are nothing more than
 *  a uart; there is no inteligence in them to do keycode translation.
 *  We leave all that up to the keyboard itself.
 *
 *	   FIXES:
 *		 dirk.uffmann@nokia.com:  enabled PS/2 reconnection
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>	/* for in_interrupt */
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/delay.h>	/* for udelay */
#include <linux/kbd_kern.h>	/* for keyboard_tasklet */
#include <linux/kbd_ll.h>

#include <asm/io.h>
#include <asm/hardware/amba_kmi.h>
#include <asm/mach/amba_kmi.h>
#include <asm/keyboard.h>

//#define DEBUG(s) printk s
#define DEBUG(s) do { } while (0)

#define CONFIG_AMBA_PS2_RECONNECT

#define KMI_BASE	(kmi->base)

#define KMI_RESET		0x00
#define KMI_RESET_POR		0x01
#define KMI_RESET_DONE		0x02

#define KMI_NO_ACK		0xffff

#define PS2_O_RESET		0xff
#define PS2_O_RESEND		0xfe
#define PS2_O_DISABLE		0xf5
#define PS2_O_ENABLE		0xf4
#define PS2_O_ECHO		0xee

/*
 * Keyboard
 */
#define PS2_O_SET_DEFAULT	0xf6
#define PS2_O_SET_RATE_DELAY	0xf3
#define PS2_O_SET_SCANSET	0xf0
#define PS2_O_INDICATORS	0xed

/*
 * Mouse
 */
#define PS2_O_SET_SAMPLE	0xf3
#define PS2_O_SET_STREAM	0xea
#define PS2_O_SET_RES		0xe8
#define PS2_O_SET_SCALE21	0xe7
#define PS2_O_SET_SCALE11	0xe6
#define PS2_O_REQ_STATUS	0xe9

/*
 * Responses
 */
#define PS2_I_RESEND		0xfe
#define PS2_I_DIAGFAIL		0xfc
#define PS2_I_ACK		0xfa
#define PS2_I_BREAK		0xf0
#define PS2_I_ECHO		0xee
#define PS2_I_BAT_OK		0xaa

static char *kmi_type[] = { "Keyboard", "Mouse" };

static struct kmi_info *kmi_keyb;
static struct kmi_info *kmi_mouse;

static inline void __kmi_send(struct kmi_info *kmi, u_int val)
{
	u_int status;

	do {
		status = __raw_readb(KMISTAT);
	} while (!(status & KMISTAT_TXEMPTY));

	kmi->resend_count += 1;
	__raw_writeb(val, KMIDATA);
}

static void kmi_send(struct kmi_info *kmi, u_int val)
{
	kmi->last_tx = val;
	kmi->resend_count = -1;
	__kmi_send(kmi, val);
}

static u_int kmi_send_and_wait(struct kmi_info *kmi, u_int val, u_int timeo)
{
	DECLARE_WAITQUEUE(wait, current);

	if (kmi->present == 0)
		return KMI_NO_ACK;

	kmi->res = KMI_NO_ACK;
	kmi->last_tx = val;
	kmi->resend_count = -1;

	if (current->pid != 0 && !in_interrupt()) {
		add_wait_queue(&kmi->wait_q, &wait);
		set_current_state(TASK_UNINTERRUPTIBLE);
		__kmi_send(kmi, val);
		schedule_timeout(timeo);
		current->state = TASK_RUNNING;
		remove_wait_queue(&kmi->wait_q, &wait);
	} else {
		int i;

		__kmi_send(kmi, val);
		for (i = 0; i < 1000; i++) {
			if (kmi->res != KMI_NO_ACK)
				break;
			udelay(100);
		}
	}

	return kmi->res;
}

/*
 * This lot should probably be separated into a separate file...
 */
#ifdef CONFIG_KMI_MOUSE

#include <linux/fs.h>		/* for struct file_ops */
#include <linux/poll.h> 	/* for poll_table */
#include <linux/miscdevice.h>	/* for struct miscdev */
#include <linux/random.h>	/* for add_mouse_randomness */
#include <linux/slab.h> 	/* for kmalloc */
#include <linux/smp_lock.h>	/* for {un,}lock_kernel */
#include <linux/spinlock.h>

#include <asm/uaccess.h>

#define BUF_SZ	2048

static spinlock_t kmi_mouse_lock;
static int kmi_mouse_count;
static struct queue {
	u_int			head;
	u_int			tail;
	struct fasync_struct	*fasync;
	unsigned char		buf[BUF_SZ];
} *queue;

#define queue_empty() (queue->head == queue->tail)

static u_char get_from_queue(void)
{
	unsigned long flags;
	u_char res;

	spin_lock_irqsave(&kmi_mouse_lock, flags);
	res = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (BUF_SZ-1);
	spin_unlock_irqrestore(&kmi_mouse_lock, flags);

	return res;
}

static ssize_t
kmi_mouse_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	ssize_t i = count;

	if (queue_empty()) {
		int ret;

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(kmi_mouse->wait_q, !queue_empty());
		if (ret)
			return ret;
	}
	while (i > 0 && !queue_empty()) {
		u_char c;
		c = get_from_queue();
		put_user(c, buf++);
		i--;
	}
	if (count - i)
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
	return count - i;
}

static ssize_t
kmi_mouse_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	ssize_t retval = 0;

	if (count > 32)
		count = 32;

	do {
		char c;
		get_user(c, buf++);
		kmi_send_and_wait(kmi_mouse, c, HZ);
		retval++;
	} while (--count);

	if (retval)
		file->f_dentry->d_inode->i_mtime = CURRENT_TIME;

	return retval;
}

static unsigned int
kmi_mouse_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &kmi_mouse->wait_q, wait);
	return (!queue_empty()) ? POLLIN | POLLRDNORM : 0;
}

static int
kmi_mouse_release(struct inode *inode, struct file *file)
{
	lock_kernel();
	fasync_helper(-1, file, 0, &queue->fasync);
	if (--kmi_mouse_count == 0)
		kmi_send_and_wait(kmi_mouse, PS2_O_DISABLE, HZ);
	unlock_kernel();
	return 0;
}

static int
kmi_mouse_open(struct inode *inode, struct file *file)
{
	if (kmi_mouse_count++)
		return 0;
	queue->head = queue->tail = 0;
	kmi_send_and_wait(kmi_mouse, PS2_O_ENABLE, HZ);
	return 0;
}

static int
kmi_mouse_fasync(int fd, struct file *filp, int on)
{
	int retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval > 0)
		retval = 0;
	return retval;
}

static struct file_operations ps_fops = {
	read:		kmi_mouse_read,
	write:		kmi_mouse_write,
	poll:		kmi_mouse_poll,
	open:		kmi_mouse_open,
	release:	kmi_mouse_release,
	fasync: 	kmi_mouse_fasync,
};

static struct miscdevice ps_mouse = {
	minor:		PSMOUSE_MINOR,
	name:		"psaux",
	fops:		&ps_fops,
};

static u_char kmi_mse_init_string[] = {
	PS2_O_DISABLE,
	PS2_O_SET_SAMPLE, 100,
	PS2_O_SET_RES, 3,
	PS2_O_SET_SCALE21
};

/*
 * The "normal" mouse scancode processing
 */
static void kmi_mse_intr(struct kmi_info *kmi, u_int val, struct pt_regs *regs)
{
	u_int head;

	add_mouse_randomness(val);

#ifdef CONFIG_AMBA_PS2_RECONNECT
	/* Try to detect a hot-plug event on the PS/2 mouse port */
	switch (kmi->hotplug_state) {
	case 0:
		/* Maybe we lost contact... */
		if (val == PS2_I_BAT_OK) {
			kmi->hotplug_state++;
			DEBUG(("%s: Saw 0xAA. Going to hotplug state %d\n", kmi->name, kmi->hotplug_state));
		}
		break;

	case 1:
		/* Again, maybe (but only maybe) we lost contact... */
		if (val == 0) {
			kmi->hotplug_state++;
			kmi_send(kmi, PS2_O_REQ_STATUS);
			DEBUG(("%s: Got 0xAA 0x00. Sent Status Request\n", kmi->name));
		} else {
			kmi->hotplug_state = 0;
			DEBUG(("%s: No 0x00 followed 0xAA. No reconnect.\n", kmi->name));
		}
		break;

	case 2:
		/* Eat up acknowledge */
		if (val == PS2_I_ACK)
			kmi->hotplug_state++;
		else {
			kmi->hotplug_state = 0;
			DEBUG(("%s: didn't get ack (0x%2.2x)\n", kmi->name, val));
		}
		break;

	case 3:
		/* check if data reporting is still enabled, then no POR has happend */
		kmi->reconnect = !(val & 1<<5);
		DEBUG(("%s: Data reporting disabled?: (%d)\n", kmi->name, kmi->reconnect));
		kmi->hotplug_state++;
		DEBUG(("%s: Going to hotplug state %d\n", kmi->name, kmi->hotplug_state));
		break;

	case 4:
		/* Eat up one status byte */
		kmi->hotplug_state++;
		DEBUG(("%s: Going to hotplug state %d\n", kmi->name, kmi->hotplug_state));
		break;

	case 5:
		/* Eat up another status byte */
		if (kmi->reconnect) {
			kmi->config_num = 0;
			kmi_send(kmi, kmi_mse_init_string[kmi->config_num]);
			kmi->config_num++;
			kmi->hotplug_state++;
			DEBUG(("%s: Sending byte %d of PS/2 init string.\n", kmi->name, kmi->config_num));
		} else {
			kmi->hotplug_state = 0;
			DEBUG(("%s: False Alarm...\n", kmi->name));
		}
		break;

	case 6:
		if (val == PS2_I_ACK && kmi->config_num < sizeof(kmi_mse_init_string)) {
			kmi_send(kmi, kmi_mse_init_string[kmi->config_num]);
			kmi->config_num++;
			DEBUG(("%s: Sending byte %d of PS/2 init string.\n", kmi->name, kmi->config_num));
		} else {
			if (val == PS2_I_ACK) {
				DEBUG(("%s: Now enable the mouse again...\n", kmi->name));
				queue->head = queue->tail = 0;
				kmi_send(kmi, PS2_O_ENABLE);
				kmi->hotplug_state++;
			} else {
				kmi->hotplug_state = 0;
				DEBUG(("%s: didn't get ack (0x%2.2x)\n", kmi->name, val));
			}
		}
		break;

	case 7:
		/* Eat up last acknowledge from enable */
		if (val == PS2_I_ACK)
			printk(KERN_ERR "%s: reconnected\n", kmi->name);
		else
			DEBUG(("%s: didn't get ack (0x%2.2x)\n", kmi->name, val));

		kmi->hotplug_state = 0;
		break;

	} /* switch (kmi->hotplug_state) */

	/* while inside hotplug mechanism, don't misinterpret values */
	if (kmi->hotplug_state > 2)
		return;
#endif

	/* We are waiting for the mouse to respond to a kmi_send_and_wait() */
	if (kmi->res == KMI_NO_ACK) {
		if (val == PS2_I_RESEND) {
			if (kmi->resend_count < 5)
				__kmi_send(kmi, kmi->last_tx);
			else {
				printk(KERN_ERR "%s: too many resends\n", kmi->name);
				return;
			}
		}

		if (val == PS2_I_ACK) {
			kmi->res = val;
			wake_up(&kmi->wait_q);
		}
		return;
	}

	/* The mouse autonomously send new data, so wake up mouse_read() */
	if (queue) {
		head = queue->head;
		queue->buf[head] = val;
		head = (head + 1) & (BUF_SZ - 1);
		if (head != queue->tail) {
			queue->head = head;
			kill_fasync(&queue->fasync, SIGIO, POLL_IN);
			wake_up_interruptible(&kmi->wait_q);
		}
	}
}

static int kmi_init_mouse(struct kmi_info *kmi)
{
	u_int ret, i;

	if (kmi->present) {
		kmi->rx = kmi_mse_intr;

		for (i = 0; i < sizeof(kmi_mse_init_string); i++) {
			ret = kmi_send_and_wait(kmi, kmi_mse_init_string[i], HZ);
			if (ret != PS2_I_ACK)
				printk("%s: didn't get ack (0x%2.2x)\n",
					kmi->name, ret);
		}
	}

	queue = kmalloc(sizeof(*queue), GFP_KERNEL);
	if (queue) {
		memset(queue, 0, sizeof(*queue));
		misc_register(&ps_mouse);
		ret = 0;
	} else
		ret = -ENOMEM;

	return ret;
}
#endif /* CONFIG_KMI_MOUSE */

/*
 * The "program" we send to the keyboard to set it up how we want it:
 *  - default typematic delays
 *  - scancode set 1
 */
static u_char kmi_kbd_init_string[] = {
	PS2_O_DISABLE,
	PS2_O_SET_DEFAULT,
	PS2_O_SET_SCANSET, 0x01,
	PS2_O_ENABLE
};

static void kmi_kbd_intr(struct kmi_info *kmi, u_int val, struct pt_regs *regs);

static int __kmi_init_keyboard(struct kmi_info *kmi)
{
	u_int ret, i;

	if (!kmi->present)
		return 0;

	kmi->rx = kmi_kbd_intr;

	for (i = 0; i < sizeof(kmi_kbd_init_string); i++) {
		ret = kmi_send_and_wait(kmi, kmi_kbd_init_string[i], HZ);
		if (ret != PS2_I_ACK)
			printk("%s: didn't ack (0x%2.2x)\n",
				kmi->name, ret);
	}

	return 0;
}

static void kmi_kbd_init_tasklet(unsigned long k)
{
	struct kmi_info *kmi = (struct kmi_info *)k;
	__kmi_init_keyboard(kmi);
}

static DECLARE_TASKLET_DISABLED(kmikbd_init_tasklet, kmi_kbd_init_tasklet, 0);

/*
 * The "normal" keyboard scancode processing
 */
static void kmi_kbd_intr(struct kmi_info *kmi, u_int val, struct pt_regs *regs)
{
#ifdef CONFIG_AMBA_PS2_RECONNECT
	/* Try to detect a hot-plug event on the PS/2 keyboard port */
	switch (kmi->hotplug_state) {
	case 0:
		/* Maybe we lost contact... */
		if (val == PS2_I_BAT_OK) {
			kmi_send(kmi, PS2_O_SET_SCANSET);
			kmi->hotplug_state++;
			DEBUG(("%s: Saw 0xAA. Going to hotplug state %d\n", kmi->name, kmi->hotplug_state));
		}
		break;

	case 1:
		/* Eat up acknowledge */
		if (val == PS2_I_ACK) {
			/* Request scan code set: '2' if POR has happend, '1' is false alarm */
			kmi_send(kmi, 0);
			kmi->hotplug_state++;
		}
		else {
			kmi->hotplug_state = 0;
			DEBUG(("%s: didn't get ack (0x%2.2x)\n", kmi->name, val));
		}
		break;

	case 2:
		/* Eat up acknowledge */
		if (val == PS2_I_ACK)
			kmi->hotplug_state++;
		else {
			kmi->hotplug_state = 0;
			DEBUG(("%s: didn't get ack (0x%2.2x)\n", kmi->name, val));
		}
		break;

	case 3:
		kmi->hotplug_state = 0;
		if (val == 2) {
			DEBUG(("%s: POR detected. Scan code is: (%d)\n", kmi->name, val));
			kmi->present = 1;
			tasklet_schedule(&kmikbd_init_tasklet);
			printk(KERN_ERR "%s: reconnected\n", kmi->name);
			return;
		}
		else
			DEBUG(("%s: False Alarm...\n", kmi->name));
		break;

	} /* switch (kmi->hotplug_state) */
#endif

	if (val == PS2_I_DIAGFAIL) {
		printk(KERN_ERR "%s: diagnostic failed\n", kmi->name);
		return;
	}

	/* We are waiting for the keyboard to respond to a kmi_send_and_wait() */
	if (kmi->res == KMI_NO_ACK) {
		if (val == PS2_I_RESEND) {
			if (kmi->resend_count < 5)
				__kmi_send(kmi, kmi->last_tx);
			else {
				printk(KERN_ERR "%s: too many resends\n", kmi->name);
				return;
			}
		}

		if (val >= 0xee) {
			kmi->res = val;
			wake_up(&kmi->wait_q);
		}
		return;
	}

#ifdef CONFIG_VT
	kbd_pt_regs = regs;
	handle_scancode(val, !(val & 0x80));
	tasklet_schedule(&keyboard_tasklet);
#endif
}

static void kmi_intr(int nr, void *devid, struct pt_regs *regs)
{
	struct kmi_info *kmi = devid;
	u_int status = __raw_readb(KMIIR);

	if (status & KMIIR_RXINTR) {
		u_int val = __raw_readb(KMIDATA);

		if (kmi->rx)
			kmi->rx(kmi, val, regs);
	}
}

static int kmi_init_keyboard(struct kmi_info *kmi)
{
	kmikbd_init_tasklet.data = (unsigned long)kmi;
	tasklet_enable(&kmikbd_init_tasklet);

	return __kmi_init_keyboard(kmi);
}

/*
 * Reset interrupt handler
 */
static void __init
kmi_reset_intr(struct kmi_info *kmi, u_int val, struct pt_regs *regs)
{
	if (kmi->state == KMI_RESET) {
		if (val == PS2_I_ACK)
			kmi->state = KMI_RESET_POR;
		else {
			val = KMI_NO_ACK;
			goto finished;
		}
	} else if (kmi->state == KMI_RESET_POR) {
finished:
		kmi->res = val;
		kmi->state = KMI_RESET_DONE;
		kmi->rx = NULL;
		wake_up(&kmi->wait_q);
	}
}

/*
 * Reset the device plugged into this interface
 */
static int __init kmi_reset(struct kmi_info *kmi)
{
	u_int res;
	int ret = 0;

	kmi->state = KMI_RESET;
	kmi->rx = kmi_reset_intr;
	res = kmi_send_and_wait(kmi, PS2_O_RESET, HZ);
	kmi->rx = NULL;

	if (res != PS2_I_BAT_OK) {
		printk(KERN_ERR "%s: reset failed; ", kmi->name);
		if (kmi->res != KMI_NO_ACK)
			printk("code 0x%2.2x\n", kmi->res);
		else
			printk("no ack\n");
		ret = -EINVAL;
	}
	return ret;
}

static int __init kmi_init_one_interface(struct kmi_info *kmi)
{
	u_int stat;
	int ret = -ENODEV;

	init_waitqueue_head(&kmi->wait_q);

	printk(KERN_INFO "%s at 0x%8.8x on irq %d (%s)\n", kmi->name,
		kmi->base, kmi->irq, kmi_type[kmi->type]);

	/*
	 * Initialise the KMI interface
	 */
	__raw_writeb(kmi->divisor, KMICLKDIV);
	__raw_writeb(KMICR_EN, KMICR);

	/*
	 * Check that the data and clock lines are OK.
	 */
	stat = __raw_readb(KMISTAT);
	if ((stat & (KMISTAT_IC|KMISTAT_ID)) != (KMISTAT_IC|KMISTAT_ID)) {
		printk(KERN_ERR "%s: %s%s%sline%s stuck low\n", kmi->name,
			(stat & KMISTAT_IC) ? "" : "clock ",
			(stat & (KMISTAT_IC | KMISTAT_ID)) ? "" : "and ",
			(stat & KMISTAT_ID) ? "" : "data ",
			(stat & (KMISTAT_IC | KMISTAT_ID)) ? "" : "s");
		goto bad;
	}

	/*
	 * Claim the appropriate interrupts
	 */
	ret = request_irq(kmi->irq, kmi_intr, 0, kmi->name, kmi);
	if (ret)
		goto bad;

	/*
	 * Enable the receive interrupt, and reset the device.
	 */
	__raw_writeb(KMICR_EN | KMICR_RXINTREN, KMICR);
	kmi->present = 1;
	kmi->present = kmi_reset(kmi) == 0;

	switch (kmi->type) {
	case KMI_KEYBOARD:
		ret = kmi_init_keyboard(kmi);
		break;

#ifdef CONFIG_KMI_MOUSE
	case KMI_MOUSE:
		ret = kmi_init_mouse(kmi);
		break;
#endif
	}

	return ret;

bad:
	/*
	 * Oh dear, the interface was bad, disable it.
	 */
	__raw_writeb(0, KMICR);
	return ret;
}

#ifdef CONFIG_VT
/*
 * The fragment between #ifdef above and #endif * CONFIG_VT *
 * is from the pc_keyb.c driver.  It is not copyrighted under the
 * above notice.  This code is by various authors; please see
 * drivers/char/pc_keyb.c for further information.
 */

/*
 * Translation of escaped scancodes to keycodes.
 * This is now user-settable.
 * The keycodes 1-88,96-111,119 are fairly standard, and
 * should probably not be changed - changing might confuse X.
 * X also interprets scancode 0x5d (KEY_Begin).
 *
 * For 1-88 keycode equals scancode.
 */

#define E0_KPENTER 96
#define E0_RCTRL   97
#define E0_KPSLASH 98
#define E0_PRSCR   99
#define E0_RALT    100
#define E0_BREAK   101	/* (control-pause) */
#define E0_HOME    102
#define E0_UP	   103
#define E0_PGUP    104
#define E0_LEFT    105
#define E0_RIGHT   106
#define E0_END	   107
#define E0_DOWN    108
#define E0_PGDN    109
#define E0_INS	   110
#define E0_DEL	   111

#define E1_PAUSE   119

/* BTC */
#define E0_MACRO   112
/* LK450 */
#define E0_F13	   113
#define E0_F14	   114
#define E0_HELP    115
#define E0_DO	   116
#define E0_F17	   117
#define E0_KPMINPLUS 118
/*
 * My OmniKey generates e0 4c for  the "OMNI" key and the
 * right alt key does nada. [kkoller@nyx10.cs.du.edu]
 */
#define E0_OK	124
/*
 * New microsoft keyboard is rumoured to have
 * e0 5b (left window button), e0 5c (right window button),
 * e0 5d (menu button). [or: LBANNER, RBANNER, RMENU]
 * [or: Windows_L, Windows_R, TaskMan]
 */
#define E0_MSLW 125
#define E0_MSRW 126
#define E0_MSTM 127

static u_char e0_keys[128] = {
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	E0_KPENTER,	E0_RCTRL,	0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		E0_KPSLASH,	0,		E0_PRSCR,
	E0_RALT,	0,		0,		0,
	0,		E0_F13, 	E0_F14, 	E0_HELP,
	E0_DO,		E0_F17, 	0,		0,
	0,		0,		E0_BREAK,	E0_HOME,
	E0_UP,		E0_PGUP,	0,		E0_LEFT,
	E0_OK,		E0_RIGHT,	E0_KPMINPLUS,	E0_END,
	E0_DOWN,	E0_PGDN,	E0_INS, 	E0_DEL,
	0,		0,		0,		0,
	0,		0,		0,		E0_MSLW,
	E0_MSRW,	E0_MSTM,	0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		E0_MACRO,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0,
	0,		0,		0,		0
};

#ifdef CONFIG_MAGIC_SYSRQ
u_char kmi_kbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1" 	/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

int kmi_kbd_setkeycode(u_int scancode, u_int keycode)
{
	if (scancode < 128 || scancode > 255 || keycode > 127)
		return -EINVAL;
	e0_keys[scancode - 128] = keycode;
	return 0;
}

int kmi_kbd_getkeycode(u_int scancode)
{
	if (scancode < 128 || scancode > 255)
		return -EINVAL;
	return e0_keys[scancode - 128];
}
	        
int kmi_kbd_translate(u_char scancode, u_char *keycode, char raw_mode)
{
	static int prev_scancode = 0;

	/* special prefix scancodes.. */
	if (scancode == 0xe0 || scancode == 0xe1) {
		prev_scancode = scancode;
		return 0;
	}

	/* 0xff is sent by a few keyboards, ignore it.	0x00 is error */
	if (scancode == 0x00 || scancode == 0xff) {
		prev_scancode = 0;
		return 0;
	}

	scancode &= 0x7f;

	if (prev_scancode) {
		int old_scancode = prev_scancode;

		prev_scancode = 0;
		switch (old_scancode) {
		case 0xe0:
			/*
			 * The keyboard maintains its own internal caps lock
			 * and num lock status.  In caps lock mode, E0 AA
			 * precedes make code and E0 2A follows break code.
			 * In numlock mode, E0 2A precedes make code, and
			 * E0 AA follows break code.  We do our own book-
			 * keeping, so we will just ignore these.
			 *
			 * For my keyboard there is no caps lock mode, but
			 * there are both Shift-L and Shift-R modes. The
			 * former mode generates E0 2A / E0 AA pairs, the
			 * latter E0 B6 / E0 36 pairs.	So, we should also
			 * ignore the latter. - aeb@cwi.nl
			 */
			if  (scancode == 0x2a || scancode == 0x36)
				return 0;
			if (e0_keys[scancode])
				*keycode = e0_keys[scancode];
			else {
				if (!raw_mode)
					printk(KERN_INFO "kbd: unknown "
						"scancode e0 %02x\n",
						scancode);
				return 0;
			}
			break;

		case 0xe1:
			if (scancode == 0x1d)
				prev_scancode = 0x100;
			else {
				if (!raw_mode)
					printk(KERN_INFO "kbd: unknown "
						"scancode e1 %02x\n",
						scancode);
				return 0;
			}
			break;

		case 0x100:
			if (scancode == 0x45)
				*keycode = E1_PAUSE;
			else {
				if (!raw_mode)
					printk(KERN_INFO "kbd: unknown "
						"scan code e1 1d %02x\n",
						scancode);
				return 0;
			}
			break;
		}
	} else
		*keycode = scancode;
	return 1;
}

char kmi_kbd_unexpected_up(u_char keycode)
{
	return 0x80;
}

void kmi_kbd_leds(u_char leds)
{
	struct kmi_info *kmi = kmi_keyb;
	u_int ret;

	if (kmi) {
		ret = kmi_send_and_wait(kmi, PS2_O_INDICATORS, HZ);
		if (ret != KMI_NO_ACK)
			ret = kmi_send_and_wait(kmi, leds, HZ);
		if (ret == KMI_NO_ACK)
			kmi->present = 0;
	}
}

int __init kmi_kbd_init(void)
{
	int  ret = -ENODEV;

	if (kmi_keyb) {
		strcpy(kmi_keyb->name, "kmikbd");
		ret = kmi_init_one_interface(kmi_keyb);
	}

	if (ret == 0) {
		k_setkeycode	= kmi_kbd_setkeycode;
		k_getkeycode	= kmi_kbd_getkeycode;
		k_translate	= kmi_kbd_translate;
		k_unexpected_up = kmi_kbd_unexpected_up;
		k_leds		= kmi_kbd_leds;
#ifdef CONFIG_MAGIC_SYSRQ
		k_sysrq_xlate	= kmi_kbd_sysrq_xlate;
		k_sysrq_key	= 0x54;
#endif
	}

	return ret;
}

#endif /* CONFIG_VT */

int register_kmi(struct kmi_info *kmi)
{
	struct kmi_info **kmip = NULL;
	int ret;

	if (kmi->type == KMI_KEYBOARD)
		kmip = &kmi_keyb;
	else if (kmi->type == KMI_MOUSE)
		kmip = &kmi_mouse;

	ret = -EINVAL;
	if (kmip) {
		ret = -EBUSY;
		if (!*kmip) {
			*kmip = kmi;
			ret = 0;
		}
	}

	return ret;
}

#ifdef CONFIG_KMI_MOUSE
static int __init kmi_init(void)
{
	int  ret = -ENODEV;

	if (kmi_mouse) {
		strcpy(kmi_mouse->name, "kmimouse");
		ret = kmi_init_one_interface(kmi_mouse);
	}

	return ret;
}

__initcall(kmi_init);
#endif
