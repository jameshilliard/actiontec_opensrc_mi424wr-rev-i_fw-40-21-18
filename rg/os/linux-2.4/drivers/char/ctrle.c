/****************************************************************************/

/*
 *	ctrle.c -- Test/debug driver for the ST ADSL/CTRLE interface
 *
 */

/****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/****************************************************************************/

#define	CTRLE_NAME	"adsl/ctrle"
#define	CTRLE_MAJOR	120

/****************************************************************************/

/*
 *	Offcourse different boards would have to use different CS lines
 *	to control the CTRLE interface...
 */
#if 1
#define	IXP425_EXP_CS			IXP425_EXP_CS4
#define	IXP425_EXP_BUS_CS_BASE_PHYS	IXP425_EXP_BUS_CS4_BASE_PHYS
#elif defined(CONFIG_ARCH_IXDP425) || defined(CONFIG_ARCH_ADI_COYOTE)
#define	IXP425_EXP_CS			IXP425_EXP_CS1
#define	IXP425_EXP_BUS_CS_BASE_PHYS	IXP425_EXP_BUS_CS1_BASE_PHYS
#else
#error "ERROR: don't know which CS line to use for CTRLE interface?"
#endif

/*
 *	I think only the first 0x200 addresses in the CTRLE region are
 *	actually usefyll. So limit the region to 0x200 bytes in size.
 */
#define	CTRLE_SIZE	0x200

void *ctrle_map;
unsigned long ctrle_io;

/****************************************************************************/

static ssize_t ctrle_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	int i, end;

#if 0
	printk("ctrle_read(filp=%x,buf=%x,count=%d,ppos=%x)\n",
		(int)filp, (int)buf, count, (int)ppos);
#endif

	if ((i = *ppos) >= CTRLE_SIZE)
		return 0;
	if ((i + count) > CTRLE_SIZE)
		count = CTRLE_SIZE - i;
	for (end = i + count; (i < end); i++)
		put_user(readb(ctrle_map+i), buf++);

	*ppos = i;
	return count;
}

/****************************************************************************/

static ssize_t ctrle_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	unsigned char val;
	int i, end;

#if 0
	printk("ctrle_write(filp=%x,buf=%x,count=%d,ppos=%x)\n",
		(int)filp, (int)buf, count, (int)ppos);
#endif

	if ((i = *ppos) >= CTRLE_SIZE)
		return 0;
	if ((i + count) > CTRLE_SIZE)
		count = CTRLE_SIZE - i;
	for (end = i + count; (i < end); i++) {
		get_user(val, buf++);
#if 0
	printk("%s(%d): REG[%x]=%x\n", __FILE__, __LINE__, i, val);
#endif
	writeb(val, ctrle_map+i);
	}

	*ppos = i;
	return count;
}

/****************************************************************************/

static loff_t ctrle_lseek(struct file *file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:
		file->f_pos = offset;
		break;
	case 1:
		file->f_pos += offset;
		break;
	default:
		return -EINVAL;
	}
	return file->f_pos;
}

/****************************************************************************/

static struct file_operations ctrle_fops = {
	.read = ctrle_read,
	.write = ctrle_write,
	.llseek = ctrle_lseek,
};

/****************************************************************************/

static int ctrle_init(void)
{
	printk("CTRLE: test/debug interface loaded\n");

	/* Set timing for CS line. Port is Intel style, 8bit. */
	*IXP425_EXP_CS = 0x81c03c03;

	if (register_chrdev(CTRLE_MAJOR, CTRLE_NAME, &ctrle_fops) < 0) {
		printk("CTRLE: failed to register chr device?\n");
		return -EBUSY;
	}

	ctrle_map = ioremap(IXP425_EXP_BUS_CS_BASE_PHYS, CTRLE_SIZE);
	ctrle_io = (unsigned long) ctrle_map;
	if (ctrle_map == NULL) {
		printk("CTRLE: failed to ioremap ctrle memory region?\n");
		return -ENOMEM;
	}

	return 0;
}

/****************************************************************************/

static void ctrle_exit(void)
{
	printk("CTRLE: test/debug interface unloaded\n");
	unregister_chrdev(CTRLE_MAJOR, CTRLE_NAME);
	if (ctrle_map)
		iounmap(ctrle_map);
}

/****************************************************************************/

module_init(ctrle_init);
module_exit(ctrle_exit);

MODULE_DESCRIPTION("IXP425 ADSL/CTRLE test/debug interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.org>");

/****************************************************************************/

