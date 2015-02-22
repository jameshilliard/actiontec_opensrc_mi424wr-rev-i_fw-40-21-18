/******************************************************************************
	Copyright (C) 2002 Motorola GSG-China

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*******************************************************************************/
/*****************************************************************************
 * File Name:	dbmx1fb.c
 *
 * Progammers:	Chen Ning, Zhang Juan
 *
 * Date of Creations:	10 DEC,2001
 *
 * Synopsis:
 *
 * Descirption: DB-MX1 LCD controller Linux frame buffer driver
 * 		This file is subject to the terms and conditions of the
 * 		GNU General Public License.  See the file COPYING in the main
 * 		directory of this archive for more details.
 *
 * Modification History:
 * 10 DEC, 2001, initialization version, frame work for frame buffer driver
 *
*******************************************************************************/
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/wrapper.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

#include "asm/arch/hardware.h"
#include "asm/arch/platform.h"
#include "asm/arch/memory.h"

#include "dbmx1fb.h"

#undef SUP_TTY0

#define LCD_PM
#ifdef LCD_PM
#include <linux/pm.h>
struct pm_dev *pm;
#endif

// PLAM - make sure fbmem.c also has this defined for full screen frame
// buffer support in SDRAM
#define FULL_SCREEN

#undef HARDWARE_CURSOR
// #undef HARDWARE_CURSOR
#undef DEBUG


/********************************************************************************/
#ifdef DEBUG
#  define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#define FUNC_START	DPRINTK(KERN_ERR"start\n");
#define FUNC_END	DPRINTK(KERN_ERR"end\n");
#else
#  define DPRINTK(fmt, args...)
#define FUNC_START
#define FUNC_END
#endif

#define _IO_ADDRESS(r)		((r)+0xf0000000)
unsigned int READREG(unsigned int r)
{
	volatile unsigned int * reg;
	reg = (volatile unsigned int*) _IO_ADDRESS(r);
	return *reg;
}
void WRITEREG(unsigned int r, unsigned int val)
{
	volatile unsigned int *reg;
	reg = (volatile unsigned int*) _IO_ADDRESS(r);
	*reg = val;
	return;
}

#define FONT_DATA ((unsigned char *)font->data)
struct fbcon_font_desc *font;

/* Local LCD controller parameters */
struct dbmx1fb_par{
	u_char          *screen_start_address;	/* Screen Start Address */
	u_char          *v_screen_start_address;/* Virtul Screen Start Address */
	unsigned long   screen_memory_size;	/* screen memory size */
	unsigned int    palette_size;
	unsigned int    max_xres;
	unsigned int    max_yres;
	unsigned int    xres;
	unsigned int    yres;
	unsigned int    xres_virtual;
	unsigned int    yres_virtual;
	unsigned int    max_bpp;
	unsigned int    bits_per_pixel;
	unsigned int    currcon;
	unsigned int    visual;
	unsigned int 	TFT :1;
	unsigned int    color :1 ;
	unsigned int	sharp :1 ;

        unsigned short cfb16[16];
};

#ifdef HARDWARE_CURSOR
/* hardware cursor parameters */
struct dbmx1fb_cursor{
	//	int	enable;
		int	startx;
		int	starty;
		int	blinkenable;
		int	blink_rate;
		int	width;
		int	height;
		int	color[3];
		int	state;
};

/* Frame buffer of LCD information */
struct dbmx1fb_info{
	struct display_switch dispsw;
	struct dbmx1fb_cursor cursor;
};
#endif // HARDWARE_CURSOR

static u_char*	p_framebuffer_memory_address;
static u_char*	v_framebuffer_memory_address;

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	 30000, 70000, 50, 65, 0 	/* Generic */
};

/* color map initial */
static unsigned short __attribute__((unused)) color4map[16] = {
	0x0000, 0x000f,	0x00f0,	0x0f2a,	0x0f00,	0x0f0f,	0x0f88,	0x0ccc,
	0x0888,	0x00ff,	0x00f8,	0x0f44,	0x0fa6,	0x0f22,	0x0ff0,	0x0fff
};

static unsigned short gray4map[16] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
	0x0008, 0x0009, 0x000a, 0x000b, 0x000c,	0x000d,	0x000e,	0x000f
};

static struct display global_disp;      /* Initial (default) Display Settings */
static struct fb_info fb_info;
static struct fb_var_screeninfo init_var = {};
static struct dbmx1fb_par current_par={ };

/* Frame buffer device API */
static int  dbmx1fb_get_fix(struct fb_fix_screeninfo *fix, int con,
		struct fb_info *info);
static int  dbmx1fb_get_var(struct fb_var_screeninfo *var, int con,
		struct fb_info *info);
static int  dbmx1fb_set_var(struct fb_var_screeninfo *var, int con,
		struct fb_info *info);
static int  dbmx1fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		struct fb_info *info);
static int  dbmx1fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		struct fb_info *info);

/* Interface to the low level driver  */
static int  dbmx1fb_switch(int con, struct fb_info *info);
static void dbmx1fb_blank(int blank, struct fb_info *info);
static int dbmx1fb_updatevar(int con, struct fb_info *info);

/*  Internal routines  */
static int  _reserve_fb_memory(void);
static void _install_cmap(int con, struct fb_info *info);
static void _enable_lcd_controller(void);
static void _disable_lcd_controller(void);
static int  _encode_var(struct fb_var_screeninfo *var,
			struct dbmx1fb_par *par);
static int  _decode_var(struct fb_var_screeninfo *var,
                    struct dbmx1fb_par *par);

/* initialization routines */
static void __init _init_lcd_system(void);
static int  __init _init_lcd(void);
static void __init _init_fbinfo(void);
static int  __init _reserve_fb_memory(void);

/* frame buffer ops */
static struct fb_ops dbmx1fb_ops = {
        owner:          THIS_MODULE,
        fb_get_fix:     dbmx1fb_get_fix,
        fb_get_var:     dbmx1fb_get_var,
        fb_set_var:     dbmx1fb_set_var,
        fb_get_cmap:    dbmx1fb_get_cmap,
        fb_set_cmap:    dbmx1fb_set_cmap,
};

#ifdef HARDWARE_CURSOR
/* Hardware Cursor */
static void dbmx1fb_cursor(struct display *p, int mode, int x, int y);
static int dbmx1fb_set_font(struct display *d, int width, int height);
static UINT8 cursor_color_map[] = {0xf8};
static void dbmx1fb_set_cursor_state(struct dbmx1fb_info *fb,UINT32 state);
static void dbmx1fb_set_cursor(struct dbmx1fb_info *fb);
static void dbmx1fb_set_cursor_blink(struct dbmx1fb_info *fb,int blink);

struct display_switch dbmx1fb_cfb4 = {
	    setup:              fbcon_cfb4_setup,
	    bmove:              fbcon_cfb4_bmove,
	    clear:              fbcon_cfb4_clear,
	    putc:               fbcon_cfb4_putc,
	    putcs:              fbcon_cfb4_putcs,
	    revc:               fbcon_cfb4_revc,
	    cursor:             dbmx1fb_cursor,
	    set_font:           dbmx1fb_set_font,
	    fontwidthmask:      FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(16)
};

struct display_switch dbmx1fb_cfb8 = {
	    setup:              fbcon_cfb8_setup,
	    bmove:              fbcon_cfb8_bmove,
	    clear:              fbcon_cfb8_clear,
	    putc:               fbcon_cfb8_putc,
	    putcs:              fbcon_cfb8_putcs,
	    revc:               fbcon_cfb8_revc,
	    cursor:             dbmx1fb_cursor,
	    set_font:           dbmx1fb_set_font,
	    fontwidthmask:      FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(16)
};

struct display_switch dbmx1fb_cfb16 = {
	    setup:              fbcon_cfb16_setup,
	    bmove:              fbcon_cfb16_bmove,
	    clear:              fbcon_cfb16_clear,
	    putc:               fbcon_cfb16_putc,
	    putcs:              fbcon_cfb16_putcs,
	    revc:               fbcon_cfb16_revc,
	    cursor:             dbmx1fb_cursor,
	    set_font:           dbmx1fb_set_font,
	    fontwidthmask:      FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(16)
};
#endif // HARDWARE_CURSOR


/*****************************************************************************
 * Function Name: dbmx1fb_getcolreg()
 *
 * Input: 	regno	: Color register ID
 *		red	: Color map red[]
 *		green	: Color map green[]
 *		blue	: Color map blue[]
 *	transparent	: Flag
 *		info	: Fb_info database
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Description: Transfer to fb_xxx_cmap handlers as parameters to
 *		control color registers
 *
 * Modification History:
 *	10 DEC,2001, Chen Ning
******************************************************************************/
#define RED	0xf00
#define GREEN	0xf0
#define BLUE	0x0f
static int dbmx1fb_getcolreg(u_int regno, u_int *red, u_int *green,
	u_int *blue, u_int *trans, struct fb_info *info)
{
	unsigned int val;

	FUNC_START;

	if(regno >= current_par.palette_size)
		return 1;

	val = READREG(DBMX1_LCD_MAPRAM+regno);

	if((current_par.bits_per_pixel == 4)&&(!current_par.color))
	{
		*red = *green = *blue = (val & BLUE) << 4;//TODO:
		*trans = 0;
	}
	else
	{
		*red = (val & RED) << 4;
		*green = (val & GREEN) << 8;
		*blue = (val & BLUE) << 12;
		*trans = 0;
	}

	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx1fb_setcolreg()
 *
 * Input: 	regno	: Color register ID
 *		red	: Color map red[]
 *		green	: Color map green[]
 *		blue	: Color map blue[]
 *	transparent	: Flag
 *		info	: Fb_info database
 *
 * Value Returned: int 	: Return status.If no error, return 0.
 *
 * Description: Transfer to fb_xxx_cmap handlers as parameters to
 *		control color registers
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
 *****************************************************************************/
static int
dbmx1fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		u_int trans, struct fb_info *info)
{
	unsigned int val=0;
	FUNC_START
	if(regno >= current_par.palette_size)
		return 1;

	if((current_par.bits_per_pixel == 4)&&(!current_par.color))
		val = (blue & 0x00f) << 12;//TODO:
	else
	{
		val = (blue >> 12 ) & BLUE;
		val |= (green >> 8) & GREEN;
		val |= (red >> 4) & RED;
	}

        if (regno < 16) {
		current_par.cfb16[regno] =
			regno | regno << 5 | regno << 10;
}

	WRITEREG(DBMX1_LCD_MAPRAM+regno, val);
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx1fb_get_cmap()
 *
 * Input: 	cmap	: Ouput data pointer
 *		kspc   	: Kernel space flag
 *		con    	: Console ID
 *		info	: Frame buffer information
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Description: Data is copied from hardware or local or system DISPAY,
 *		and copied to cmap.
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int
dbmx1fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		                 struct fb_info *info)
{
        int err = 0;

	FUNC_START;
        DPRINTK("current_par.visual=%d\n", current_par.visual);
        if (con == current_par.currcon)
		err = fb_get_cmap(cmap, kspc, dbmx1fb_getcolreg, info);
        else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
        else
		fb_copy_cmap(fb_default_cmap(current_par.palette_size),
                             cmap, kspc ? 0 : 2);
        FUNC_END;
	return err;
}

/*****************************************************************************
 * Function Name: dbmx1fb_set_cmap()
 *
 * Input: 	cmap	: Ouput data pointer
 *		kspc   	: Kernel space flag
 *		con    	: Console ID
 *		info	: Frame buffer information
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Description: Copy data from cmap and copy to DISPLAY. If DISPLAy has no cmap,
 * 		allocate memory for it. If DISPLAY is current console and visible,
 * 		then hardware color map shall be set.
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int
dbmx1fb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
        int err = 0;

	FUNC_START;
        DPRINTK("current_par.visual=%d\n", current_par.visual);
        if (!fb_display[con].cmap.len)
                err = fb_alloc_cmap(&fb_display[con].cmap,
                                    current_par.palette_size, 0);

        if (!err) {
                if (con == current_par.currcon)
                        err = fb_set_cmap(cmap, kspc, dbmx1fb_setcolreg,
                                          info);
                fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
        }
	FUNC_END;
        return err;
}
/*****************************************************************************
 * Function Name: dbmx1fb_get_var()
 *
 * Input: 	var	: Iuput data pointer
 *		con	: Console ID
 *		info	: Frame buffer information
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: 	_encode_var()
 *
 * Description: Get color map from current, or global display[console]
 * 		used by ioctl
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int
dbmx1fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
        if (con == -1) {
		_encode_var(var, &current_par);
        } else
		*var = fb_display[con].var;
        return 0;
}


/*****************************************************************************
 * Function Name: dbmx1fb_updatevar()
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Fill in display switch with LCD information,
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int dbmx1fb_updatevar(int con, struct fb_info *info)
{
        DPRINTK("entered\n");
        return 0;
}


/*****************************************************************************
 * Function Name: dbmx1fb_set_dispsw()
 *
 * Input: 	display		: Iuput data pointer
 *		dbmx1fb_info   	: Frame buffer of LCD information
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Fill in display switch with LCD information,
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void dbmx1fb_set_dispsw(struct display *disp
#ifdef HARDWARE_CURSOR
		,struct dbmx1fb_info *info
#endif
		)
{
	FUNC_START;
	switch (disp->var.bits_per_pixel) {
#ifdef HARDWARE_CURSOR
#ifdef FBCON_HAS_CFB4
		case 4:
                    fb_info.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		    info->dispsw = dbmx1fb_cfb4;
		    disp->dispsw = &info->dispsw;
		    disp->dispsw_data = NULL;
		    break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
                    fb_info.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		    info->dispsw = dbmx1fb_cfb8;
		    disp->dispsw = &info->dispsw;
		    disp->dispsw_data = NULL;
		    break;
#endif
#ifdef FBCON_HAS_CFB16
		case 12:
		case 16:
                    fb_info.fix.visual = FB_VISUAL_DIRECTCOLOR;
		    info->dispsw = dbmx1fb_cfb16;
		    disp->dispsw = &info->dispsw;
		    disp->dispsw_data = current_par.cfb16;
		    break;
#endif
#else //!HARDWARE_CURSOR
		    /* first step disable the hardware cursor */
#ifdef FBCON_HAS_CFB4
		case 4:
                    fb_info.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		    disp->dispsw = &fbcon_cfb4;
		    disp->dispsw_data = NULL;
		    break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
                    fb_info.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		    disp->dispsw = &fbcon_cfb8;
		    disp->dispsw_data = NULL;
		    break;
#endif
#ifdef FBCON_HAS_CFB16
		case 12:
		case 16:
                    fb_info.fix.visual = FB_VISUAL_DIRECTCOLOR;
		    disp->dispsw = &fbcon_cfb16;
		    disp->dispsw_data = current_par.cfb16;
		    break;
#endif

#endif // HARDWARE_CURSOR
		default:
		    disp->dispsw = &fbcon_dummy;
		    disp->dispsw_data = NULL;
	}
#ifdef HARDWARE_CURSOR
	if (&info->cursor)
	{
		info->dispsw.cursor = dbmx1fb_cursor;
		info->dispsw.set_font = dbmx1fb_set_font;
	}
#endif // HARDWARE_CURSOR
	FUNC_END;
}

/*****************************************************************************
 * Function Name: dbmx1fb_set_var()
 *
 * Input: 	var	: Iuput data pointer
 *		con	: Console ID
 *		info	: Frame buffer information
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: 	dbmx1fb_decode_var()
 * 			dbmx1fb_encode_var()
 *  			dbmx1fb_set_dispsw()
 *
 * Description: set current_par by var, also set display data, specially the console
 * 		related fileops, then enable the lcd controller, and set cmap to
 * 		hardware.
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int
dbmx1fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct display *display;
	int err, chgvar = 0;
	struct dbmx1fb_par par;

	FUNC_START;
	if (con >= 0)
		display = &fb_display[con]; /* Display settings for console */
        else
                display = &global_disp;     /* Default display settings */

        /* Decode var contents into a par structure, adjusting any */
        /* out of range values. */
        if ((err = _decode_var(var, &par))){
		DPRINTK("decode var error!");
                return err;
	}

	// Store adjusted par values into var structure
	_encode_var(var, &par);

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;

	else if (((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW) &&
			((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NXTOPEN))
		return -EINVAL;

	if (con >= 0) {
		if ((display->var.xres != var->xres) ||
			(display->var.yres != var->yres) ||
			(display->var.xres_virtual != var->xres_virtual) ||
			(display->var.yres_virtual != var->yres_virtual) ||
			(display->var.sync != var->sync)                 ||
			(display->var.bits_per_pixel != var->bits_per_pixel) ||
			(memcmp(&display->var.red, &var->red, sizeof(var->red))) ||
			(memcmp(&display->var.green, &var->green, sizeof(var->green)
				)) ||
			(memcmp(&display->var.blue, &var->blue, sizeof(var->blue))))
			chgvar = 1;
	}

	display->var 		= *var;
	display->screen_base    = par.v_screen_start_address;
	display->visual         = par.visual;
	display->type           = FB_TYPE_PACKED_PIXELS;
	display->type_aux       = 0;
	display->ypanstep       = 0;
	display->ywrapstep      = 0;
	display->line_length    =
	display->next_line      = (var->xres * 16) / 8;

	display->can_soft_blank = 1;
	display->inverse        = 0;

	dbmx1fb_set_dispsw(display
#ifdef HARDWARE_CURSOR
			, (struct dbmx1fb_info *)info
#endif // HARDWARE_CURSOR
			);

	/* If the console has changed and the console has defined */
	/* a changevar function, call that function. */
	if (chgvar && info && info->changevar)
		info->changevar(con);	// TODO:

	/* If the current console is selected and it's not truecolor,
	*  update the palette
	*/
	if ((con == current_par.currcon) &&
			(current_par.visual != FB_VISUAL_TRUECOLOR)) {
		struct fb_cmap *cmap;

		current_par = par;	// TODO ?
		if (display->cmap.len)
			cmap = &display->cmap;
		else
			cmap = fb_default_cmap(current_par.palette_size);

		fb_set_cmap(cmap, 1, dbmx1fb_setcolreg, info);
	}

	/* If the current console is selected, activate the new var. */
	if (con == current_par.currcon){
		init_var = *var;	// TODO:gcc support structure copy?
		_enable_lcd_controller();
	}

	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx1fb_get_fix()
 *
 * Input: 	fix	: Ouput data pointer
 *		con	: Console ID
 *		info	: Frame buffer information
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: VOID
 *
 * Description: get fix from display data, current_par data
 * 		used by ioctl
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int
dbmx1fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
        struct display *display;

	FUNC_START;
        memset(fix, 0, sizeof(struct fb_fix_screeninfo));
        strcpy(fix->id, DBMX1_NAME);

        if (con >= 0)
        {
                DPRINTK("Using console specific display for con=%d\n",con);
                display = &fb_display[con];  /* Display settings for console */
        }
        else
                display = &global_disp;      /* Default display settings */

        fix->smem_start  = (unsigned long)current_par.screen_start_address;
        fix->smem_len    = current_par.screen_memory_size;
//printk("dbmx1fb_get_fix, pointer fix: 0x%08x, smem_len: 0x%08x\n",fix,fix->smem_len);
        fix->type        = display->type;
        fix->type_aux    = display->type_aux;
        fix->xpanstep    = 0;
        fix->ypanstep    = display->ypanstep;
        fix->ywrapstep   = display->ywrapstep;
        fix->visual      = display->visual;
        fix->line_length = display->line_length;
        fix->accel       = FB_ACCEL_NONE;

	FUNC_END;
        return 0;
}

/*****************************************************************************
 * Function Name: dbmx1fb_inter_handler()
 *
 * Input:
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Interrupt handler
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void dbmx1fb_inter_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int intsr;
	FUNC_START;
	intsr = READREG(DBMX1_LCD_INTSR);		// read to clear status
	printk(KERN_ERR"lcd interrupt!\n");
	FUNC_END;
	// handle
}

#ifdef LCD_PM
#define PM_OPT " [pm]"
#define LCD_PMST_RESUME 0
#define LCD_PMST_SUSPEND 1
static unsigned int lcd_pm_status = LCD_PMST_RESUME;

void lcd_pm_resume(void)
{
	if(lcd_pm_status == LCD_PMST_RESUME)
		return;
	WRITEREG(0x21c21c, 0x10000);	// light on
	WRITEREG(DBMX1_LCD_REFMCR, 0xf000002);
	WRITEREG(DBMX1_LCD_PWMR, 0x00a9008a);
	lcd_pm_status = LCD_PMST_RESUME;
//	printk(KERN_ERR"lcd resumed\n");
}

void lcd_pm_suspend(void)
{
	unsigned val;
	if(lcd_pm_status == LCD_PMST_SUSPEND)
		return;
	val = READREG(0x20502c);
	val |= 0x8000;
	WRITEREG(0x20502c, val);
	//To produce enough dealy time before trun off the LCDC.
	for(val=0;val<=600000;val++);
	val = READREG(0x21c21c);
	val &= ~0x10000;
	WRITEREG(0x21c21c, val);	// light off
	WRITEREG(DBMX1_LCD_REFMCR, 0x0);
	lcd_pm_status = LCD_PMST_SUSPEND;
//	printk(KERN_ERR"lcd suspended\n");
}

int lcd_pm_handler(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch(rqst){
		case PM_RESUME:
			lcd_pm_resume();
			break;
		case PM_SUSPEND:
			lcd_pm_suspend();
			break;
		default:
			break;
		}
	return 0;
}
#endif // LCD_PM

/*****************************************************************************
 * Function Name: dbmx1fb_init()
 *
 * Input: VOID
 *
 * Value Returned: int 		: Return status.If no error, return 0.
 *
 * Functions Called: 	_init_fbinfo()
 *			disable_irq()
 *	 		enable_irq()
 *			_init_lcd()
 *			dbmx1fb_init_cursor()
 *
 * Description: initialization module, all of init routine's entry point
 * 		initialize fb_info, init_var, current_par
 * 		and setup interrupt, memory, lcd controller
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
int __init dbmx1fb_init(void)
{
	int ret;
#ifdef HARDWARE_CURSOR
	struct dbmx1fb_info *info;
#endif // HARDWARE_CURSOR

	_init_lcd_system();

	_init_fbinfo();

	if ((ret = _reserve_fb_memory()) != 0){
		printk(KERN_ERR"failed for reserved DBMX frame buffer memory\n");
		return ret;
	}

#if 0
	if (request_irq(IRQ_LCD,
				dbmx1fb_inter_handler,
				SA_INTERRUPT,
				DEV_NAME,
				NULL) != 0) {
		printk(KERN_ERR "dbmx1fb: failed in request_irq\n");
		return -EBUSY;
	}

	disable_irq(IRQ_LCD);
#endif
        if (dbmx1fb_set_var(&init_var, -1, &fb_info))
		; //current_par.allow_modeset = 0;

	_init_lcd();
	_enable_lcd_controller();

#ifdef HARDWARE_CURSOR
	info = kmalloc(sizeof(struct dbmx1fb_info), GFP_KERNEL);
	if(info == NULL){
		printk(KERN_ERR"can not kmalloc dbmx1fb_info memory\n");
		return -1;
	}

	memset(info,0,sizeof(struct dbmx1fb_info));

	info->cursor.blink_rate = DEFAULT_CURSOR_BLINK_RATE;
	info->cursor.blinkenable = 0;
	info->cursor.state = LCD_CURSOR_OFF;
	WRITEREG(DBMX1_LCD_LCXYP,0x90010001);
	WRITEREG(DBMX1_LCD_CURBLKCR,0x1F1F0000);
	WRITEREG(DBMX1_LCD_LCHCC,0x0000F800);

	DPRINTK(KERN_ERR"LCXYP = %x\n",READREG(DBMX1_LCD_LCXYP));
	DPRINTK(KERN_ERR"CURBLICR = %x\n",READREG(DBMX1_LCD_CURBLKCR));
	DPRINTK(KERN_ERR"LCHCC = %x\n",READREG(DBMX1_LCD_LCHCC));

	//dbmx1fb_set_cursor(info);
	//info->cursor = dbmx1fb_init_cursor(info);
#endif // HARDWARE_CURSOR

	register_framebuffer(&fb_info);

#ifdef LCD_PM
	pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, lcd_pm_handler);
	printk("register LCD power management successfully.\n");
#endif
#if 0
	enable_irq(IRQ_LCD);	// TODO:
#endif
	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;

	return 0;
}

/*****************************************************************************
 * Function Name: dbmx1fb_setup()
 *
 * Input: info	: VOID
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: VOID
 *
 * Description: basically, this routine used to parse command line parameters, which
 * 		is initialization parameters for lcd controller, such as freq, xres,
 * 		yres, and so on
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
int __init dbmx1fb_setup(char *options)
{
	FUNC_START;
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: _init_fbinfo()
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: while 16bpp is used to store a 12 bits pixels packet, but
 * 		it is not a really 16bpp system. maybe in-compatiable with
 * 		other system or GUI.There are some field in var which specify
 *		the red/green/blue offset in a 16bit word, just little endian is
 * 		concerned
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void __init _init_fbinfo(void)
{
// Thomas Wong add this for debugging
//	*((unsigned char *)0xF5000008) = '&';

	FUNC_START;
        strcpy(fb_info.modename, DBMX1_NAME);
        strcpy(fb_info.fontname, "Acorn8x8");

        fb_info.node            = -1;
        fb_info.flags           = FBINFO_FLAG_DEFAULT;	// Low-level driver is not a module
        fb_info.fbops           = &dbmx1fb_ops;
        fb_info.monspecs        = monspecs;
        fb_info.disp            = &global_disp;
        fb_info.changevar       = NULL;
        fb_info.switch_con      = dbmx1fb_switch;
        fb_info.updatevar       = dbmx1fb_updatevar;
        fb_info.blank           = dbmx1fb_blank;

/*
 * * setup initial parameters
 * */
        memset(&init_var, 0, sizeof(init_var));

        init_var.transp.length  = 0;
        init_var.nonstd         = 0;
        init_var.activate       = FB_ACTIVATE_NOW;
        init_var.xoffset        = 0;
        init_var.yoffset        = 0;
        init_var.height         = -1;
        init_var.width          = -1;
        init_var.vmode          = FB_VMODE_NONINTERLACED;

        if (1) {
                current_par.max_xres    = LCD_MAXX;
                current_par.max_yres    = LCD_MAXY;
                current_par.max_bpp     = LCD_MAX_BPP;		// 12
                init_var.red.length     = 5;  // 5;
                init_var.green.length   = 6;  // 6;
                init_var.blue.length    = 5;  // 5;
#ifdef __LITTLE_ENDIAN
                init_var.red.offset    = 11;
                init_var.green.offset  = 5;
                init_var.blue.offset   = 0;
#endif //__LITTLE_ENDIAN
		init_var.grayscale      = 16;	// i suppose, TODO
                init_var.sync           = 0;
                init_var.pixclock       = 171521;	// TODO
        }

        current_par.screen_start_address       = NULL;
        current_par.v_screen_start_address       = NULL;
        current_par.screen_memory_size         = MAX_PIXEL_MEM_SIZE;
// Thomas Wong add this for debugging
//printk("_init_fbinfo, pointer to current_par: 0x%08x, screen_memory_size: 0x%08x\n", &current_par,current_par.screen_memory_size);
        current_par.currcon             = -1;	// TODO

        init_var.xres                   = current_par.max_xres;
        init_var.yres                   = current_par.max_yres;
        init_var.xres_virtual           = init_var.xres;
        init_var.yres_virtual           = init_var.yres;
        init_var.bits_per_pixel         = current_par.max_bpp;

	FUNC_END;
}

/*****************************************************************************
 * Function Name: dbmx1fb_blank()
 *
 * Input: 	blank	: Blank flag
 *		info	: Frame buffer database
 *
 * Value Returned: VOID
 *
 * Functions Called: 	_enable_lcd_controller()
 * 			_disable_lcd_controller()
 *
 * Description: blank the screen, if blank, disable lcd controller, while if no blank
 * 		set cmap and enable lcd controller
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void
dbmx1fb_blank(int blank, struct fb_info *info)
{
#ifdef SUP_TTY0
        int i;

	FUNC_START;
        DPRINTK("blank=%d info->modename=%s\n", blank, info->modename);
        if (blank) {
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
  	              for (i = 0; i < current_par.palette_size; i++)
			      ; // TODO
//printk("Disable LCD\n");
					 _disable_lcd_controller();
        }
        else {
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
                	dbmx1fb_set_cmap(&fb_display[current_par.currcon].cmap,
					1,
					current_par.currcon, info);
//printk("Enable LCD\n");
                _enable_lcd_controller();
        }
	FUNC_END;
#endif //SUP_TTY0
}

/*****************************************************************************
 * Function Name: dbmx1fb_switch()
 *
 * Input:  	info	: Frame buffer database
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: Switch to another console
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int dbmx1fb_switch(int con, struct fb_info *info)
{
	FUNC_START;
	if (current_par.visual != FB_VISUAL_TRUECOLOR) {
		struct fb_cmap *cmap;
	        if (current_par.currcon >= 0) {
	        	// Get the colormap for the selected console
			cmap = &fb_display[current_par.currcon].cmap;

		if (cmap->len)
			fb_get_cmap(cmap, 1, dbmx1fb_getcolreg, info);
		}
	}

	current_par.currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	dbmx1fb_set_var(&fb_display[con].var, con, info);
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: _encode_par()
 *
 * Input:  	var	: Input var data
 *		par	: LCD controller parameters
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: use current_par to set a var structure
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int _encode_var(struct fb_var_screeninfo *var,
		struct dbmx1fb_par *par)
{
        // Don't know if really want to zero var on entry.
	// Look at set_var to see.  If so, may need to add extra params to par

	//      memset(var, 0, sizeof(struct fb_var_screeninfo));

	var->xres = par->xres;
	var->yres = par->yres;
	var->xres_virtual = par->xres_virtual;
	var->yres_virtual = par->yres_virtual;

	var->bits_per_pixel = par->bits_per_pixel;

	DPRINTK("var->bits_per_pixel=%d\n", var->bits_per_pixel);
	switch(var->bits_per_pixel) {
	case 2:
	case 4:
	case 8:
		var->red.length    = 4;
		var->green         = var->red;
		var->blue          = var->red;
		var->transp.length = 0;
		break;
	case 12:          // This case should differ for Active/Passive mode
	case 16:
		if (1) {
			var->red.length    = 4;
			var->blue.length   = 4;
			var->green.length  = 4;
			var->transp.length = 0;
#ifdef __LITTLE_ENDIAN
			var->red.offset    = 8;
			var->green.offset  = 4;
			var->blue.offset   = 0;
			var->transp.offset = 0;
#endif // __LITTLE_ENDIAN
		}
		else
		{
			var->red.length    = 5;
			var->blue.length   = 5;
			var->green.length  = 6;
		    	var->transp.length = 0;
		    	var->red.offset    = 11;
	        	var->green.offset  = 5;
		    	var->blue.offset   = 0;
	       		var->transp.offset = 0;
	    	}
	        break;
	  }

        return 0;
}

/*****************************************************************************
 * Function Name: _decode_var
 *
 * Input:  	var	: Input var data
 *		par	: LCD controller parameters
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Get the video params out of 'var'. If a value doesn't fit,
 * 		round it up,if it's too big, return -EINVAL.
 *
 * Cautions: Round up in the following order: bits_per_pixel, xres,
 * 	yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 * 	bitfields, horizontal timing, vertical timing.
 *
 * Modification History:
 *	10 DEC,2001, Chen Ning
******************************************************************************/
static int _decode_var(struct fb_var_screeninfo *var,
                    struct dbmx1fb_par *par)
{
	*par = current_par;

	if ((par->xres = var->xres) < MIN_XRES)
		par->xres = MIN_XRES;
	if ((par->yres = var->yres) < MIN_YRES)
		par->yres = MIN_YRES;
	if (par->xres > current_par.max_xres)
		par->xres = current_par.max_xres;
	if (par->yres > current_par.max_yres)
		par->yres = current_par.max_yres;
	par->xres_virtual =
		var->xres_virtual < par->xres ? par->xres : var->xres_virtual;
        par->yres_virtual =
		var->yres_virtual < par->yres ? par->yres : var->yres_virtual;
        par->bits_per_pixel = var->bits_per_pixel;

	switch (par->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
        case 4:
		par->visual = FB_VISUAL_PSEUDOCOLOR;
		par->palette_size = 16;
                break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		par->visual = FB_VISUAL_PSEUDOCOLOR;
		par->palette_size = 256;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 12: 	// RGB 444
	case 16:  	/* RGB 565 */
		par->visual = FB_VISUAL_TRUECOLOR;
		par->palette_size = 0;
		break;
#endif
	default:
		return -EINVAL;
	}

	par->screen_start_address  =(u_char*)(
			(u_long)p_framebuffer_memory_address+PAGE_SIZE);
	par->v_screen_start_address =(u_char*)(
			(u_long)v_framebuffer_memory_address+PAGE_SIZE);

// Thomas Wong - try to change start address here (map to SRAM, instead of SDRAM)
#ifndef FULL_SCREEN
	par->screen_start_address  =(u_char*)(0x00300000);
	par->v_screen_start_address =(u_char*)(0xF0300000);
#endif

//	par->screen_start_address  =(u_char*)(0x0BE00000);
//	par->v_screen_start_address =(u_char*)(0xFBE00000);

//	par->screen_start_address  =(u_char*)(0x12000000);
//	par->v_screen_start_address =(u_char*)(0xF2000000);

	return 0;
}


/*****************************************************************************
 * Function Name: _reserve_fb_memory()
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: get data out of var structure and set related LCD controller registers
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int __init _reserve_fb_memory(void)
{
        u_int  required_pages;
        u_int  extra_pages;
        u_int  order;
        struct page *page;
        char   *allocated_region;

	DPRINTK("frame buffer memory size = %x\n", (unsigned int)ALLOCATED_FB_MEM_SIZE);
        if (v_framebuffer_memory_address != NULL)
                return -EINVAL;

        /* Find order required to allocate enough memory for framebuffer */
        required_pages = ALLOCATED_FB_MEM_SIZE >> PAGE_SHIFT;
        for (order = 0 ; required_pages >> order ; order++) {;}
        extra_pages = (1 << order) - required_pages;

        if ((allocated_region =
             	(char *)__get_free_pages(GFP_KERNEL | GFP_DMA, order)) == NULL){

		DPRINTK("can not allocated memory\n");
           	return -ENOMEM;
	}


        v_framebuffer_memory_address = (u_char *)allocated_region +
		(extra_pages << PAGE_SHIFT);
        p_framebuffer_memory_address = (u_char *)__virt_to_phys(
			(u_long)v_framebuffer_memory_address);
#if 0
	printk(KERN_ERR"Frame buffer __get_free_pages vd:= %x, pd= %x",
			(unsigned int)v_framebuffer_memory_address,
			(unsigned int)p_framebuffer_memory_address);
#endif
        /* Free all pages that we don't need but were given to us because */
        /* __get_free_pages() works on powers of 2. */
        for (;extra_pages;extra_pages--)
          free_page((u_int)allocated_region + ((extra_pages-1) << PAGE_SHIFT));

	/* Set reserved flag for fb memory to allow it to be remapped into */
        /* user space by the common fbmem driver using remap_page_range(). */
        for(page = virt_to_page(v_framebuffer_memory_address);
            page < virt_to_page(v_framebuffer_memory_address
		    + ALLOCATED_FB_MEM_SIZE);
	    page++)
		mem_map_reserve(page);
#if 0
        /* Remap the fb memory to a non-buffered, non-cached region */
        v_framebuffer_memory_address = (u_char *)__ioremap(
			(u_long)p_framebuffer_memory_address,
			ALLOCATED_FB_MEM_SIZE,
			L_PTE_PRESENT 	|
			L_PTE_YOUNG 	|
			L_PTE_DIRTY 	|
			L_PTE_WRITE);
#endif
	current_par.screen_start_address  =(u_char*)(
			(u_long)p_framebuffer_memory_address+PAGE_SIZE);
	current_par.v_screen_start_address =(u_char*)(
			(u_long)v_framebuffer_memory_address+PAGE_SIZE);

	DPRINTK("physical screen start addres: %x\n",
			(u_long)p_framebuffer_memory_address+PAGE_SIZE);

#ifndef FULL_SCREEN
// Thomas Wong - we'll try to change the screen start address here
//	printk("\n\rMap LCD screen to SDRAM.\n\r");

	printk("\n\rMap LCD screen to embedded SRAM.\n\r");
	current_par.screen_start_address  =(u_char*)(0x00300000);
	current_par.v_screen_start_address =(u_char*)(0xF0300000);
#endif

//	printk("\n\rMap LCD screen to SDRAM 0xFBE00000.\n\r");
//	current_par.screen_start_address  =(u_char*)(0x0BE00000);
//	current_par.v_screen_start_address =(u_char*)(0xFBE00000);

//	printk("\n\rMap LCD screen to SRAM 0x12000000.\n\r");
//	current_par.screen_start_address  =(u_char*)(0x12000000);
//	current_par.v_screen_start_address =(u_char*)(0xF2000000);

        return (v_framebuffer_memory_address == NULL ? -EINVAL : 0);
}

/*****************************************************************************
 * Function Name: _enable_lcd_controller()
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: enable Lcd controller, setup registers,
 *		base on current_par value
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void _enable_lcd_controller(void)
{
	unsigned int val;
#if 0
	int i;
	val =0;

	FUNC_START;
	_decode_var(&init_var, &current_par);

	val = current_par.xres/16;
	val <<= 20;
	val += current_par.yres;
	WRITEREG(DBMX1_LCD_XYMAX, val);

	val=0;
	val=current_par.xres_virtual /2;
	WRITEREG(DBMX1_LCD_VPW, val);

	switch(current_par.bits_per_pixel ){
	case 4:	// for gray only
		for(i=0; i<16; i++){
			WRITEREG(DBMX1_LCD_MAPRAM+i, gray4map[i]);
		}
		WRITEREG(DBMX1_LCD_PANELCFG, PANELCFG_VAL_4);
		WRITEREG(DBMX1_LCD_VCFG, VCFG_VAL_4);
		WRITEREG(DBMX1_LCD_HCFG, HCFG_VAL_4);
		WRITEREG(DBMX1_LCD_INTCR, INTCR_VAL_4);	// no interrupt
		WRITEREG(DBMX1_LCD_REFMCR, REFMCR_VAL_4);
		WRITEREG(DBMX1_LCD_DMACR, DMACR_VAL_4);
		WRITEREG(DBMX1_LCD_PWMR, PWMR_VAL);
		break;
	case 12:
	case 16:
		WRITEREG(DBMX1_LCD_PANELCFG, PANELCFG_VAL_12);
		WRITEREG(DBMX1_LCD_HCFG, HCFG_VAL_12);
		WRITEREG(DBMX1_LCD_VCFG, VCFG_VAL_12);
		WRITEREG(DBMX1_LCD_REFMCR, 0x0);
		WRITEREG(DBMX1_LCD_DMACR, DMACR_VAL_12);
		WRITEREG(DBMX1_LCD_PWMR, 0x00008200);
		WRITEREG(DBMX1_LCD_REFMCR, 0x0f000002);
		WRITEREG(DBMX1_LCD_PWMR, 0x0000008a);

		break;
	}
#else
	val = READREG(0x21c21c);
	val |= 0x0010000;
	WRITEREG(0x21c21c, val);

	WRITEREG(DBMX1_LCD_PWMR, 0x8200);
	WRITEREG(DBMX1_LCD_REFMCR, 0xf000002);

	// Thomas Wong - we want 0x8A not 0x200
//	WRITEREG(DBMX1_LCD_PWMR, 0x200);
// PLAM -- for rev2 (endian bit)
//	WRITEREG(DBMX1_LCD_PWMR, 0x0000008A);
	WRITEREG(DBMX1_LCD_PWMR, 0x00A9008A);
// end PLAM
#endif

	FUNC_END;
}

/*****************************************************************************
 * Function Name: _disable_lcd_controller()
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: just disable the LCD controller
 * 		disable lcd interrupt. others, i have no ideas
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void _disable_lcd_controller(void)
{
	unsigned int val;
	val = READREG(0x21c21c);
	val &= ~0x0010000;
	WRITEREG(0x21c21c, val);

	WRITEREG(DBMX1_LCD_PWMR, 0x8200);
//	WRITEREG(DBMX1_LCD_REFMCR, DISABLELCD_VAL);
	WRITEREG(0x205034, 0x0);
}

/*****************************************************************************
 * Function Name: _install_cmap
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: set color map to hardware
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void _install_cmap(int con, struct fb_info *info)
{
        if (con != current_par.currcon)
                return ;
        if (fb_display[con].cmap.len)
                fb_set_cmap(&fb_display[con].cmap, 1, dbmx1fb_setcolreg, info);
        else
                fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
				1,
				dbmx1fb_setcolreg,
				info);
	return ;
}

/*****************************************************************************
 * Function Name: _init_lcd_system
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: initialize the gpio port C and port D with DMA enable
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static void __init _init_lcd_system(void)
{
unsigned int val;

/* Thomas Wong - we don't need DMA here
	// dma reset & enable
	val = READREG(0x209000);
	val |= 0x02;
	WRITEREG(0x209000, val);
	val = READREG(0x209000);
	val |= 0x01;
	WRITEREG(0x209000, val);
*/
	// gpio
	//clear port D for LCD signal
	WRITEREG(0x21c320, 0x0);
	WRITEREG(0x21c338, 0x0);

	val = READREG(0x21c220);
	val |= 0x10000;
	WRITEREG(0x21c220, val);
	val = READREG(0x21c208);
	val |= 0x03;
	WRITEREG(0x21c208, val);
	val = READREG(0x21c200);
	val |= 0x10000;
	WRITEREG(0x21c200, val);
	val = READREG(0x21c238);
	val |= 0x10000;
	WRITEREG(0x21c238, val);
	val = READREG(0x21c21c);
	val |= 0x10000;
	WRITEREG(0x21c21c, val);

}

static void set_pclk(unsigned int fmhz)
{
	unsigned int div= 96/fmhz;
	unsigned int reg;
	reg = READREG(0x21b020);
	reg &= ~0xf0;
	WRITEREG(0x21b020, reg);
	reg |= ((div-1)<<4) &0xf0;
	WRITEREG(0x21b020, reg);
}

/*****************************************************************************
 * Function Name: _init_lcd
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called: _decode_var()
 *
 * Description: initialize the LCD controller, use current_par for 12bpp
 *
 * Modification History:
 *		10 DEC,2001, Chen Ning
******************************************************************************/
static int __init _init_lcd()
{

	unsigned int val, rate;
	int i;
	unsigned char * pscr;
	//unsigned long pclk,temp,PCLKDIV2;
	//unsigned long MFI,MFN,PD,MFD,tempReg,MCUPLLCLK;


	_decode_var(&init_var, &current_par);

	// gpio	begin
	val = READREG(0x21c21c);
	val &= ~0x00010000;
	WRITEREG(0x21c21c, val);
	DPRINTK(KERN_ERR"DR=%x\n", READREG(0x21c21c));

	// LCD regs
	DPRINTK(KERN_ERR"write SSA by %x\n",
			(unsigned int)current_par.screen_start_address);
	WRITEREG(DBMX1_LCD_SSA, (unsigned int)current_par.screen_start_address);
	DPRINTK(KERN_ERR"SSA=%x\n", READREG(DBMX1_LCD_SSA));

	val =0;
	val = current_par.xres/16;
	val = val<<20;
	val += current_par.yres;
	DPRINTK(KERN_ERR"par.x=%x, y=%x\n",
			current_par.xres,
			current_par.yres);
	WRITEREG(DBMX1_LCD_XYMAX, val);
	DPRINTK(KERN_ERR"XYMAX=%x\n", READREG(DBMX1_LCD_XYMAX));

	val=0;
	val=current_par.xres_virtual/2;
	WRITEREG(DBMX1_LCD_VPW, val);
	DPRINTK(KERN_ERR"VPW=%x\n", READREG(DBMX1_LCD_VPW));

	set_pclk(4);

	DPRINTK(KERN_ERR"DBMX1_LCD_PANELCFG=%x\n",
			READREG(DBMX1_LCD_PANELCFG));

	WRITEREG(DBMX1_LCD_HCFG, 0x04000f06);
	DPRINTK(KERN_ERR"DBMX1_LCD_HCFG=%x\n",
			READREG(DBMX1_LCD_HCFG));

	WRITEREG(DBMX1_LCD_VCFG, 0x04000907);
	DPRINTK(KERN_ERR"DBMX1_LCD_VCFG=%x\n",
			READREG(DBMX1_LCD_VCFG));

	WRITEREG(DBMX1_LCD_REFMCR, 0x0);
	DPRINTK(KERN_ERR"DBMX1_LCD_REFMCR=%x\n",
			READREG(DBMX1_LCD_REFMCR));

	WRITEREG(DBMX1_LCD_DMACR, DMACR_VAL_12);
	DPRINTK(KERN_ERR"DBMX1_LCD_DMACR=%x\n",
			READREG(DBMX1_LCD_DMACR));

	WRITEREG(DBMX1_LCD_PWMR, 0x00008200);
	DPRINTK(KERN_ERR"DBMX1_LCD_PWMR=%x\n",
			READREG(DBMX1_LCD_PWMR));

	// Thomas Wong - we don't want to turn it on here
	/*
	WRITEREG(DBMX1_LCD_REFMCR, 0x0f000002)
	DPRINTK(KERN_ERR"DBMX1_LCD_REFMCR=%x\n",
			READREG(DBMX1_LCD_REFMCR));
	*/

	WRITEREG(DBMX1_LCD_PWMR, 0x0000008a);
	DPRINTK(KERN_ERR"DBMX1_LCD_PWMR=%x\n",
			READREG(DBMX1_LCD_PWMR));

	// Thomas Wong
	WRITEREG(0x21B020, 0x000B005B);
// PLAM -- for rev2 (new register and endian bits)
	WRITEREG(DBMX1_LCD_PANELCFG, 0xF8008B42);	// little endian
//	WRITEREG(DBMX1_LCD_PANELCFG, 0xF8048B42);	// big endian
	WRITEREG(DBMX1_LCD_LGPMR, 0x00090300);
//	WRITEREG(DBMX1_LCD_PWMR, 0x0000008A);
	WRITEREG (DBMX1_LCD_PWMR, 0x009A008A);
// end PLAM

// PLAM -- for rev2 (new DMACR setting)
//	WRITEREG(DBMX1_LCD_DMACR, 0x800C0002);
	WRITEREG(DBMX1_LCD_DMACR, 0x00040008);
//end PLAM


#define DMACR_VAL_12	0x800C0002


//	WRITEREG(DBMX1_LCD_INTCR, INTCR_VAL_12);
//	DPRINTK(KERN_ERR"DBMX1_LCD_INTCR=%x\n",
//			READREG(DBMX1_LCD_INTCR));

	// warm up LCD
	for(i=0;i<10000000;i++) ;

	// gpio end
	val = READREG(0x21c21c);
	val |= 0x00010000;
	WRITEREG(0x21c21c, val);
	DPRINTK(KERN_ERR"DR=%x\n",
			READREG(0x21c21c));
#if 0
	// clear screen
	pscr = current_par.v_screen_start_address;
	for(i=0; i< (current_par.screen_memory_size - 2*PAGE_SIZE); i++){
		*pscr++ = 0xff;
	}
#endif
	DPRINTK(KERN_ERR"_init_lcd end \n");

	return 0;
}

#ifdef HARDWARE_CURSOR

/*
 * Hardware cursor support
 */
 /*****************************************************************************
 * Function Name: dbmx1fb_set_cursor_color()
 *
 * Input:   fb	: frame buffer database
 *	    red	: red component level in the cursor
 *	  green	: green component level in the cursor
 *	   blue	: blue component level in the cursor
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Set color of hardware cursor
 *
 * Modification History:
 *		10 DEC,2001, Zhang Juan
******************************************************************************/
static void dbmx1fb_set_cursor_color(struct dbmx1fb_info *fb, UINT8 *red, UINT8 *green, UINT8 *blue)
{
	struct dbmx1fb_cursor *c = &fb->cursor;
	UINT32 color;

	FUNC_START;
	c->color[0] = *red;
	c->color[1] = *green;
	c->color[2] = *blue;
	color = (UINT32)*red;
	color |= (UINT32)(*green>>5);
	color |= (UINT32)(*blue>>11);

	WRITEREG(DBMX1_LCD_LCHCC, color);
	FUNC_END;
}

 /*****************************************************************************
 * Function Name: dbmx1fb_set_cursor()
 *
 * Input:   fb	: frame buffer database
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Load information of hardware cursor
 *
 * Modification History:
 *		10 DEC,2001, Zhang Juan
******************************************************************************/
static void dbmx1fb_set_cursor(struct dbmx1fb_info *fb)
{
	struct dbmx1fb_cursor *c = &fb->cursor;
	UINT32 temp,tempReg,x,y;

	FUNC_START;
	//DPRINTK(KERN_ERR"BLINK_RATE=%x\n",c->blink_rate);
//	DPRINTK(KERN_ERR"width=%x\n",c->width);
//	DPRINTK(KERN_ERR"height=%x\n",c->height);

	x = c->startx << 16;
	if (c->state == LCD_CURSOR_ON)
	x |= CURSOR_ON_MASK;

#ifdef FBCON_HAS_CFB4
    else if(c->state == LCD_CURSOR_REVERSED)
		x |= CURSOR_REVERSED_MASK;
    else if(c->state == LCD_CURSOR_ON_WHITE)
		x |= CURSOR_WHITE_MASK;
#elif defined(FBCON_HAS_CFB8)||defined(FBCON_HAS_CFB16)
    else if(c->state == LCD_CURSOR_INVERT_BGD)
		x |= CURSOR_INVERT_MASK;
    else if(c->state == LCD_CURSOR_AND_BGD)
		x |= CURSOR_AND_BGD_MASK;
    else if(c->state == LCD_CURSOR_OR_BGD)
		x |= CURSOR_OR_BGD_MASK;
    else if(c->state == LCD_CURSOR_XOR_BGD)
		x |= CURSOR_XOR_BGD_MASK;
#endif
    else
	x = c->startx;

	y = c->starty;

	temp = (UINT32)x | (UINT32)y;
	WRITEREG(DBMX1_LCD_LCXYP, temp);
	//DPRINTK(KERN_ERR"lcxyp=%x\n",READREG(DBMX1_LCD_LCXYP));

	temp = (UINT32)((c->width<<8) | (c->height));
	tempReg = (UINT32)((temp<<16) | c->blink_rate);

	WRITEREG(DBMX1_LCD_CURBLKCR, tempReg);

	//c->blink_rate = 10;
	if (c->blinkenable)
		dbmx1fb_set_cursor_blink(fb,c->blink_rate);
	DPRINTK(KERN_ERR"CURBLKCR=%x\n",READREG(DBMX1_LCD_CURBLKCR));
	FUNC_END;
}

 /*****************************************************************************
 * Function Name: dbmx1fb_set_cursor_blink()
 *
 * Input:   fb  : frame buffer database
 *	 blink	: input blink frequency of cursor
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Set blink frequency of hardware cursor
 *
 * Modification History:
 *		10 DEC,2001, Zhang Juan
******************************************************************************/
static void dbmx1fb_set_cursor_blink(struct dbmx1fb_info *fb,int blink)
{
	struct dbmx1fb_cursor *c = &fb->cursor;

	unsigned long   temp,tempReg;
	unsigned long   PCD, XMAX, YMAX, PCLKDIV2;
	unsigned long   tempMicroPeriod;

	FUNC_START;
	DPRINTK(KERN_ERR"dbmx1fb_set_cursor_blink\n");

	if(!c){
		DPRINTK(KERN_ERR"dangerouts, for c == null\n");
	}
	c->blink_rate = blink;

	tempReg = READREG(DBMX1_LCD_XYMAX);
	XMAX = (tempReg & XMAX_MASK) >> 20;
	YMAX = tempReg & YMAX_MASK;
	//XMAX = 240;
	//YMAX = 320;
	tempReg = READREG(PCDR);
	PCLKDIV2 = (tempReg & PCLKDIV2_MASK) >> 4;
	tempReg = READREG(DBMX1_LCD_PANELCFG);
	PCD = tempReg & PCD_MASK;

	temp = (PCLKDIV2 + 1);

	if (!blink)
	{
		/* disable the blinking cursor function when frequency is 0 */
		tempReg = READREG(DBMX1_LCD_CURBLKCR);
		tempReg &= CURSORBLINK_DIS_MASK;
		WRITEREG(DBMX1_LCD_CURBLKCR,tempReg);
	}
	else
	{

		tempMicroPeriod = temp * XMAX * YMAX * (PCD + 1);
		temp = 96*10000000/(blink * tempMicroPeriod);
		tempReg = READREG(DBMX1_LCD_CURBLKCR);
		tempReg |= CURSORBLINK_EN_MASK;
		tempReg |= temp;
		WRITEREG(DBMX1_LCD_CURBLKCR,tempReg);
		DPRINTK(KERN_ERR"Inter_CURBLKCR=%x\n",READREG(DBMX1_LCD_CURBLKCR));
	}

	FUNC_END;
}

 /*****************************************************************************
 * Function Name: dbmx1fb_set_cursor_state()
 *
 * Input:   fb  : frame buffer database
 *	 state	: The status of the cursor to be set. e.g.
 *            		LCD_CURSOR_OFF
 *                      LCD_CURSOR_ON
 *                      LCD_CURSOR_REVERSED
 *                      LCD_CURSOR_ON_WHITE
 *                      LCD_CURSOR_OR_BGD
 *                      LCD_CURSOR_XOR_BGD
 *                      LCD_CURSOR_AND_BGD
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Set state of cursor
 *
 * Modification History:
 *		10 DEC,2001, Zhang Juan
******************************************************************************/
static void dbmx1fb_set_cursor_state(struct dbmx1fb_info *fb,UINT32 state)
{
	struct dbmx1fb_cursor *c = &fb->cursor;
	UINT32 temp;

	FUNC_START;
	c->state = state;
	temp = READREG(DBMX1_LCD_LCXYP);
    	temp &= CURSOR_OFF_MASK;

    	if (state == LCD_CURSOR_OFF)
        	temp = temp;
    	else if (state == LCD_CURSOR_ON)
        	temp |= CURSOR_ON_MASK;
#ifdef FBCON_HAS_CFB4
    	else if (state == LCD_CURSOR_REVERSED)
        	temp |= CURSOR_REVERSED_MASK;
    	else if (state == LCD_CURSOR_ON_WHITE)
		temp |= CURSOR_WHITE_MASK;
#elif defined(FBCON_HAS_CFB8)||defined(FBCON_HAS_CFB16)
	else if(state == LCD_CURSOR_INVERT_BGD)
		temp |= CURSOR_INVERT_MASK;
    	else if (state == LCD_CURSOR_OR_BGD)
        	temp |= CURSOR_OR_BGD_MASK;
    	else if (state == LCD_CURSOR_XOR_BGD)
        	temp |= CURSOR_XOR_BGD_MASK;
	else if (state == LCD_CURSOR_AND_BGD)
        	temp |= CURSOR_AND_BGD_MASK;
#endif
	WRITEREG(DBMX1_LCD_LCXYP,temp);
	DPRINTK(KERN_ERR"LCDXYP=%x\n",READREG(DBMX1_LCD_LCXYP));
	FUNC_END;
}


/*****************************************************************************
 * Function Name: dbmx1fb_cursor()
 *
 * Input:   fb     		: frame buffer database
 *
 * Value Returned: 	cursor : The structure of hardware cursor
 *
 * Functions Called: 	dbmx1fb_set_cursor()
 *			dbmx1fb_set_cursor_state()
 *
 * Description: The entry for display switch to operate hardware cursor
 *
 * Modification History:
 *		10 DEC,2001, Zhang Juan
******************************************************************************/
static void dbmx1fb_cursor(struct display *p, int mode, int x, int y)
{
	struct dbmx1fb_info *fb = (struct dbmx1fb_info *)p->fb_info;
	struct dbmx1fb_cursor *c = &fb->cursor;

	FUNC_START;
	if (c==0) return;


	x *= fontwidth(p);
	y *= fontheight(p);

	c->startx = x;
	c->starty = y;

	switch (mode) {
	case CM_ERASE:
		dbmx1fb_set_cursor_state(fb,LCD_CURSOR_OFF);
		break;

	case CM_DRAW:
	case CM_MOVE:
		c->state = LCD_CURSOR_ON;
		dbmx1fb_set_cursor(fb);
		dbmx1fb_set_cursor_state(fb, c->state);
		break;
	}
	FUNC_END;
}

/*****************************************************************************
 * Function Name: dbmx1fb_set_font()
 *
 * Input:   display	: console datebase
 * 	    width	: The new width of cursor to be set.
 * 	    height	: The new height of cursor position to be set
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: dbmx1fb_set_cursor()
 *		     dbmx1fb_set_cursor_color()
 *
 * Description: Set  font for cursor
 *
 * Modification History:
 *		10 DEC,2001, Zhang Juan
******************************************************************************/
static int dbmx1fb_set_font(struct display *d, int width, int height)
{
	struct dbmx1fb_info *fb=(struct dbmx1fb_info *)d->fb_info;
	struct dbmx1fb_cursor *c = &fb->cursor;

	FUNC_START;
	if(!d){
		printk(KERN_ERR"dbmx1fb_set_font d=null\n");
		return -1;
	}

	if (c) {
		if (!width || !height) {
			width = 16;
			height = 16;
		}

		c->width = width;
		c->height = height;

		DPRINTK(KERN_ERR"set cursor\n");
		dbmx1fb_set_cursor(fb);
		DPRINTK(KERN_ERR"set color cursor\n");
		dbmx1fb_set_cursor_color(fb, cursor_color_map,
				cursor_color_map, cursor_color_map);
	}else{
		DPRINTK(KERN_ERR"set cursor failed, cursor == null\n");
	}

	FUNC_END;
	return 1;
}
#endif // HARDWARE_CURSOR
/* end of file */

