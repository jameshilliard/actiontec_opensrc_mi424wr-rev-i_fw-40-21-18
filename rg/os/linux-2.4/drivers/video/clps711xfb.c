/*
 *  linux/drivers/video/clps711xfb.c
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd.
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
 *  Framebuffer driver for the CLPS7111 and EP7212 processors.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-mfb.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include <asm/hardware/clps7111.h>
#include <asm/arch/syspld.h>

static struct clps7111fb_info {
	struct fb_info		fb;
	int			currcon;
} *cfb;

#define CMAP_MAX_SIZE	16

/* The /proc entry for the backlight. */
static struct proc_dir_entry *clps7111fb_backlight_proc_entry = NULL;

static int clps7111fb_proc_backlight_read(char *page, char **start, off_t off,
		int count, int *eof, void *data);
static int clps7111fb_proc_backlight_write(struct file *file, 
		const char *buffer, unsigned long count, void *data);

/*
 * LCD AC Prescale.  This comes from the LCD panel manufacturers specifications.
 * This determines how many clocks + 1 of CL1 before the M signal toggles.
 * The number of lines on the display must not be divisible by this number.
 */
static unsigned int lcd_ac_prescale = 13;

/*
 *    Set a single color register. Return != 0 for invalid regno.
 */
static int
clps7111fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		     u_int transp, struct fb_info *info)
{
	unsigned int level, mask, shift, pal;

	if (regno >= (1 << info->var.bits_per_pixel))
		return 1;

	/* gray = 0.30*R + 0.58*G + 0.11*B */
	level = (red * 77 + green * 151 + blue * 28) >> 20;

	/*
	 * On an LCD, a high value is dark, while a low value is light. 
	 * So we invert the level.
	 *
	 * This isn't true on all machines, so we only do it on EDB7211.
	 *  --rmk
	 */
	if (machine_is_edb7211() || machine_is_guidea07()) {
		level = 15 - level;
	}

	shift = 4 * (regno & 7);
	level <<= shift;
	mask  = 15 << shift;
	level &= mask;

	regno = regno < 8 ? PALLSW : PALMSW;

	pal = clps_readl(regno);
	pal = (pal & ~mask) | level;
	clps_writel(pal, regno);

	return 0;
}
		    
/*
 * Set the colormap
 */
static int
clps7111fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		    struct fb_info *info)
{
	struct clps7111fb_info *cfb = (struct clps7111fb_info *)info;
	struct fb_cmap *dcmap = &fb_display[con].cmap;
	int err = 0;

	/* no colormap allocated? */
	if (!dcmap->len)
		err = fb_alloc_cmap(dcmap, CMAP_MAX_SIZE, 0);

	if (!err && con == cfb->currcon) {
		err = fb_set_cmap(cmap, kspc, clps7111fb_setcolreg, &cfb->fb);
		dcmap = &cfb->fb.cmap;
	}

	if (!err)
		fb_copy_cmap(cmap, dcmap, kspc ? 0 : 1);

	return err;
}

/*
 *    Set the User Defined Part of the Display
 */
static int
clps7111fb_set_var(struct fb_var_screeninfo *var, int con,
		   struct fb_info *info)
{
	struct display *display;
	unsigned int lcdcon, syscon, pixclock;
	int chgvar = 0;

	if (var->activate & FB_ACTIVATE_TEST)
		return 0;

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW)
		return -EINVAL;

	if (cfb->fb.var.xres != var->xres)
		chgvar = 1;
	if (cfb->fb.var.yres != var->yres)
		chgvar = 1;
	if (cfb->fb.var.xres_virtual != var->xres_virtual)
		chgvar = 1;
	if (cfb->fb.var.yres_virtual != var->yres_virtual)
		chgvar = 1;
	if (cfb->fb.var.bits_per_pixel != var->bits_per_pixel)
		chgvar = 1;

	if (con < 0) {
		display = cfb->fb.disp;
		chgvar = 0;
	} else {
		display = fb_display + con;
	}

	var->transp.msb_right	= 0;
	var->transp.offset	= 0;
	var->transp.length	= 0;
	var->red.msb_right	= 0;
	var->red.offset		= 0;
	var->red.length		= var->bits_per_pixel;
	var->green		= var->red;
	var->blue		= var->red;

	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_MFB
	case 1:
		cfb->fb.fix.visual	= FB_VISUAL_MONO01;
		display->dispsw		= &fbcon_mfb;
		display->dispsw_data	= NULL;
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
		cfb->fb.fix.visual	= FB_VISUAL_PSEUDOCOLOR;
		display->dispsw		= &fbcon_cfb2;
		display->dispsw_data	= NULL;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		cfb->fb.fix.visual	= FB_VISUAL_PSEUDOCOLOR;
		display->dispsw		= &fbcon_cfb4;
		display->dispsw_data	= NULL;
		break;
#endif
	default:
		return -EINVAL;
	}

	display->next_line	= var->xres_virtual * var->bits_per_pixel / 8;

	cfb->fb.fix.line_length = display->next_line;

	display->screen_base	= cfb->fb.screen_base;
	display->line_length	= cfb->fb.fix.line_length;
	display->visual		= cfb->fb.fix.visual;
	display->type		= cfb->fb.fix.type;
	display->type_aux	= cfb->fb.fix.type_aux;
	display->ypanstep	= cfb->fb.fix.ypanstep;
	display->ywrapstep	= cfb->fb.fix.ywrapstep;
	display->can_soft_blank = 1;
	display->inverse	= 0;

	cfb->fb.var		= *var;
	cfb->fb.var.activate	&= ~FB_ACTIVATE_ALL;

	/*
	 * Update the old var.  The fbcon drivers still use this.
	 * Once they are using cfb->fb.var, this can be dropped.
	 *                                      --rmk
	 */
	display->var		= cfb->fb.var;

	/*
	 * If we are setting all the virtual consoles, also set the
	 * defaults used to create new consoles.
	 */
	if (var->activate & FB_ACTIVATE_ALL)
		cfb->fb.disp->var = cfb->fb.var;

	if (chgvar && info && cfb->fb.changevar)
		cfb->fb.changevar(con);

	lcdcon = (var->xres_virtual * var->yres_virtual * var->bits_per_pixel) / 128 - 1;
	lcdcon |= ((var->xres_virtual / 16) - 1) << 13;
	lcdcon |= lcd_ac_prescale << 25;

	/*
	 * Calculate pixel prescale value from the pixclock.  This is:
	 *  36.864MHz / pixclock_mhz - 1.
	 * However, pixclock is in picoseconds, so this ends up being:
	 *  36864000 * pixclock_ps / 10^12 - 1
	 * and this will overflow the 32-bit math.  We perform this as
	 * (9 * 4096000 == 36864000):
	 *  pixclock_ps * 9 * (4096000 / 10^12) - 1
	 */
	pixclock = 9 * info->var.pixclock / 244140 - 1;
	lcdcon |= pixclock << 19;

	if (info->var.bits_per_pixel == 4)
		lcdcon |= LCDCON_GSMD;
	if (info->var.bits_per_pixel >= 2)
		lcdcon |= LCDCON_GSEN;

	/*
	 * LCDCON must only be changed while the LCD is disabled
	 */
	syscon = clps_readl(SYSCON1);
	clps_writel(syscon & ~SYSCON1_LCDEN, SYSCON1);
	clps_writel(lcdcon, LCDCON);
	clps_writel(syscon | SYSCON1_LCDEN, SYSCON1);

	fb_set_cmap(&cfb->fb.cmap, 1, clps7111fb_setcolreg, &cfb->fb);

	return 0;
}

/*
 * Get the currently displayed virtual consoles colormap.
 */
static int
gen_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	fb_copy_cmap(&info->cmap, cmap, kspc ? 0 : 2);
	return 0;
}

/*
 * Get the currently displayed virtual consoles fixed part of the display.
 */
static int
gen_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	*fix = info->fix;
	return 0;
}

/*
 * Get the current user defined part of the display.
 */
static int
gen_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	*var = info->var;
	return 0;
}

static struct fb_ops clps7111fb_ops = {
	owner:		THIS_MODULE,
	fb_set_var:	clps7111fb_set_var,
	fb_set_cmap:	clps7111fb_set_cmap,
	fb_get_fix:	gen_get_fix,
	fb_get_var:	gen_get_var,
	fb_get_cmap:	gen_get_cmap,
};

static int clps7111fb_switch(int con, struct fb_info *info)
{
	struct clps7111fb_info *cfb = (struct clps7111fb_info *)info;
	struct display *disp;
	struct fb_cmap *cmap;

	if (cfb->currcon >= 0) {
		disp = fb_display + cfb->currcon;

		/*
		 * Save the old colormap and video mode.
		 */
		disp->var = cfb->fb.var;
		if (disp->cmap.len)
			fb_copy_cmap(&cfb->fb.cmap, &disp->cmap, 0);
	}

	cfb->currcon = con;
	disp = fb_display + con;

	/*
	 * Install the new colormap and change the video mode.  By default,
	 * fbcon sets all the colormaps and video modes to the default
	 * values at bootup.
	 */
	if (disp->cmap.len)
		cmap = &disp->cmap;
	else
		cmap = fb_default_cmap(CMAP_MAX_SIZE);

	fb_copy_cmap(cmap, &cfb->fb.cmap, 0);

	cfb->fb.var = disp->var;
	cfb->fb.var.activate = FB_ACTIVATE_NOW;

	clps7111fb_set_var(&cfb->fb.var, con, &cfb->fb);

	return 0;
}

static int clps7111fb_updatevar(int con, struct fb_info *info)
{
	return -EINVAL;
}

static void clps7111fb_blank(int blank, struct fb_info *info)
{
    	if (blank) {
		if (machine_is_edb7211()) {
			/* Turn off the LCD backlight. */
			clps_writeb(clps_readb(PDDR) & ~EDB_PD3_LCDBL, PDDR);

			/* Power off the LCD DC-DC converter. */
			clps_writeb(clps_readb(PDDR) & ~EDB_PD1_LCD_DC_DC_EN, PDDR);

			/* Delay for a little while (half a second). */
			udelay(100);

			/* Power off the LCD panel. */
			clps_writeb(clps_readb(PDDR) & ~EDB_PD2_LCDEN, PDDR);

			/* Power off the LCD controller. */
			clps_writel(clps_readl(SYSCON1) & ~SYSCON1_LCDEN, 
					SYSCON1);
		}
	} else {
		if (machine_is_edb7211()) {
			/* Power up the LCD controller. */
			clps_writel(clps_readl(SYSCON1) | SYSCON1_LCDEN,
					SYSCON1);

			/* Power up the LCD panel. */
			clps_writeb(clps_readb(PDDR) | EDB_PD2_LCDEN, PDDR);

			/* Delay for a little while. */
			udelay(100);

			/* Power up the LCD DC-DC converter. */
			clps_writeb(clps_readb(PDDR) | EDB_PD1_LCD_DC_DC_EN,
						PDDR);

			/* Turn on the LCD backlight. */
			clps_writeb(clps_readb(PDDR) | EDB_PD3_LCDBL, PDDR);
		}
	}
}

static int 
clps7111fb_proc_backlight_read(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	/* We need at least two characters, one for the digit, and one for
	 * the terminating NULL. */
	if (count < 2) 
		return -EINVAL;

	if (machine_is_edb7211()) {
		return sprintf(page, "%d\n", 
				(clps_readb(PDDR) & EDB_PD3_LCDBL) ? 1 : 0);
	}

	return 0;
}

static int 
clps7111fb_proc_backlight_write(struct file *file, const char *buffer, 
		unsigned long count, void *data)
{
	unsigned char char_value;
	int value;

	if (count < 1) {
		return -EINVAL;
	}

	if (copy_from_user(&char_value, buffer, 1)) 
		return -EFAULT;

	value = char_value - '0';

	if (machine_is_edb7211()) {
		unsigned char port_d;

		port_d = clps_readb(PDDR);

		if (value) {
			port_d |= EDB_PD3_LCDBL;
		} else {
			port_d &= ~EDB_PD3_LCDBL;
		}

		clps_writeb(port_d, PDDR);
	}

	return count;
}

static void __init clps711x_guess_lcd_params(struct fb_info *info)
{
	unsigned int lcdcon, syscon, size;
	unsigned long phys_base = PHYS_OFFSET;
	void *virt_base = (void *)PAGE_OFFSET;

	info->var.xres_virtual	 = 640;
	info->var.yres_virtual	 = 240;
	info->var.bits_per_pixel = 4;
	info->var.activate	 = FB_ACTIVATE_NOW;
	info->var.height	 = -1;
	info->var.width		 = -1;
	info->var.pixclock	 = 93006; /* 10.752MHz pixel clock */

	/*
	 * If the LCD controller is already running, decode the values
	 * in LCDCON to xres/yres/bpp/pixclock/acprescale
	 */
	syscon = clps_readl(SYSCON1);
	if (syscon & SYSCON1_LCDEN) {
		lcdcon = clps_readl(LCDCON);

		/*
		 * Decode GSMD and GSEN bits to bits per pixel
		 * (in cs89712 docs, GSEN is GSMD1, GSMD is GSMD2)
		 */
		switch (lcdcon & (LCDCON_GSMD | LCDCON_GSEN)) {
		case LCDCON_GSMD | LCDCON_GSEN:
			info->var.bits_per_pixel = 4;
			break;

		case LCDCON_GSEN:
			info->var.bits_per_pixel = 2;
			break;

		default:
			info->var.bits_per_pixel = 1;
			break;
		}

		/*
		 * Decode xres/yres
		 */
		info->var.xres_virtual = (((lcdcon >> 13) & 0x3f) + 1) * 16;
		info->var.yres_virtual = (((lcdcon & 0x1fff) + 1) * 128) /
					  (info->var.xres_virtual *
					   info->var.bits_per_pixel);

		/*
		 * Calculate pixclock
		 */
		info->var.pixclock = (((lcdcon >> 19) & 0x3f) + 1) * 244140 / 9;

		/*
		 * Grab AC prescale
		 */
		lcd_ac_prescale = (lcdcon >> 25) & 0x1f;
	}

	info->var.xres = info->var.xres_virtual;
	info->var.yres = info->var.yres_virtual;
	info->var.grayscale = info->var.bits_per_pixel > 1;

	size = info->var.xres * info->var.yres * info->var.bits_per_pixel / 8;

	/*
	 * Might be worth checking to see if we can use the on-board
	 * RAM if size here...
	 * CLPS7110 - no on-board SRAM
	 * EP7212   - 38400 bytes
	 */
	if (size <= 38400) {
		printk(KERN_INFO "CLPS711xFB: could use on-board SRAM?\n");
	}

	if ((syscon & SYSCON1_LCDEN) == 0) {
		/*
		 * The display isn't running.  Ensure that
		 * the display memory is empty.
		 */
		memset(virt_base, 0, size);
	}

	info->screen_base    = virt_base;
	info->fix.smem_start = phys_base;
	info->fix.smem_len   = PAGE_ALIGN(size);
	info->fix.type       = FB_TYPE_PACKED_PIXELS;
}

/* this function initializes the LCD registers as required by the Guide A07
*/
int __init clps711xfb_guidea07_init(void)
{
	unsigned long videomem, videosize;
	unsigned int lcdcon, syscon;

	//LCDCON must only be changed while the LCD is disabled
	syscon = clps_readl(SYSCON1);
	clps_writel(syscon & ~SYSCON1_LCDEN, SYSCON1);

	printk(KERN_INFO "CLPS711xFB: setting up cs89712 SRAM as QVGA video framebuffer\n");
	clps_writel(0x6, FBADDR); //internal SRAM location
	videomem = (unsigned long)ioremap(SRAM_START, SRAM_SIZE);
	if (!videomem) {
		printk("ioremap failed!\n");
		kfree(cfb);
		return -EIO;
	}

	videosize = (320*240 * 4 / 8);  //ie. 38400 or 0x9600
	memset((void *)videomem, 0, videosize); //clear the vid memory

	cfb->fb.screen_base     = (void *)videomem;
	cfb->fb.fix.smem_start  = SRAM_START; //must be physical address
	cfb->fb.fix.smem_len    = videosize;

	cfb->fb.var.grayscale   = 1;
        cfb->fb.var.bits_per_pixel = 4;
	cfb->fb.var.xres_virtual = 320;
	cfb->fb.var.activate	 = FB_ACTIVATE_NOW;

	// change to 4bpp mode
	lcdcon = clps_readl(LCDCON);
	lcdcon |= LCDCON_GSMD | LCDCON_GSEN; //4bpp
        //lcdcon |= 5 << 19;	//pixel prescale value (pixclock)
	//lcdcon |= 13 << 25;	//AC prescale value
	clps_writel(lcdcon, LCDCON);

	//reenable LCD
	clps_writel(syscon | SYSCON1_LCDEN, SYSCON1);

	return 0;
}

int __init clps711xfb_init(void)
{
	int err = -ENOMEM;

	cfb = kmalloc(sizeof(*cfb) + sizeof(struct display), GFP_KERNEL);
	if (!cfb)
		goto out;

	memset(cfb, 0, sizeof(*cfb) + sizeof(struct display));
	strcpy(cfb->fb.fix.id, "clps7111");

	cfb->currcon		= -1;
	cfb->fb.screen_base	= (void *)PAGE_OFFSET;
	cfb->fb.fix.smem_start	= PAGE_OFFSET;
	cfb->fb.fix.smem_len	= 0x14000;
	cfb->fb.fix.type	= FB_TYPE_PACKED_PIXELS;

	cfb->fb.fbops		= &clps7111fb_ops;
	cfb->fb.changevar	= NULL;
	cfb->fb.switch_con	= clps7111fb_switch;
	cfb->fb.updatevar	= clps7111fb_updatevar;
	cfb->fb.blank		= clps7111fb_blank;
	cfb->fb.flags		= FBINFO_FLAG_DEFAULT;
	cfb->fb.disp		= (struct display *)(cfb + 1);

	clps711x_guess_lcd_params(&cfb->fb);

	if (machine_is_guidea07()) {
		clps711xfb_guidea07_init();
	}

	if (machine_is_fortunet()) {
		cfb->fb.fix.smem_len = 0x20000;
	}

	fb_alloc_cmap(&cfb->fb.cmap, CMAP_MAX_SIZE, 0);

	/* Register the /proc entries. */
	clps7111fb_backlight_proc_entry = create_proc_entry("backlight", 0444,
		&proc_root);
	if (clps7111fb_backlight_proc_entry == NULL) {
		printk("Couldn't create the /proc entry for the backlight.\n");
		return -EINVAL;
	}

	clps7111fb_backlight_proc_entry->read_proc = 
		&clps7111fb_proc_backlight_read;
	clps7111fb_backlight_proc_entry->write_proc = 
		&clps7111fb_proc_backlight_write;

	/*
	 * Power up the LCD
	 */
	if (machine_is_p720t()) {
		PLD_LCDEN = PLD_LCDEN_EN;
		PLD_PWR |= (PLD_S4_ON|PLD_S3_ON|PLD_S2_ON|PLD_S1_ON);
	}

	if (machine_is_edb7211()) {
		/* Power up the LCD panel. */
		clps_writeb(clps_readb(PDDR) | EDB_PD2_LCDEN, PDDR);

		/* Delay for a little while. */
		udelay(100);

		/* Power up the LCD DC-DC converter. */
		clps_writeb(clps_readb(PDDR) | EDB_PD1_LCD_DC_DC_EN, PDDR);

		/* Turn on the LCD backlight. */
		clps_writeb(clps_readb(PDDR) | EDB_PD3_LCDBL, PDDR);
	}

	clps7111fb_set_var(&cfb->fb.var, -1, &cfb->fb);
	err = register_framebuffer(&cfb->fb);

out:	return err;
}

static void __exit clps711xfb_exit(void)
{
	unregister_framebuffer(&cfb->fb);
	kfree(cfb);

	/*
	 * Power down the LCD
	 */
	if (machine_is_p720t()) {
		PLD_LCDEN = 0;
		PLD_PWR &= ~(PLD_S4_ON|PLD_S3_ON|PLD_S2_ON|PLD_S1_ON);
	}
}

#ifdef MODULE
module_init(clps711xfb_init);
#endif
module_exit(clps711xfb_exit);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("CLPS711x framebuffer driver");
MODULE_LICENSE("GPL");
