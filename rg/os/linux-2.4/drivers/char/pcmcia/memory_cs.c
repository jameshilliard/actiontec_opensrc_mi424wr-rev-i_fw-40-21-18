#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mtd.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/ds.h>
#include <pcmcia/cisreg.h>

#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>

static dev_info_t dev_info = "memory_cs";
static dev_link_t *dev_list;

struct memcs_dev {
	dev_link_t	link;
	struct map_info	map;
};

static __u8 mem_cs_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 mem_cs_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 mem_cs_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void mem_cs_copy_from(struct map_info *map, void *to, unsigned long ofs, ssize_t size)
{
	memcpy_fromio(to, map->map_priv_1 + ofs, size);
}

static void mem_cs_write8(struct map_info *map, __u8 val, unsigned long ofs)
{
	writeb(val, map->map_priv_1 + ofs);
}

static void mem_cs_write16(struct map_info *map, __u16 val, unsigned long ofs)
{
	writew(val, map->map_priv_1 + ofs);
}

static void mem_cs_write32(struct map_info *map, __u32 val, unsigned long ofs)
{
	writel(val, map->map_priv_1 + ofs);
}

static void mem_cs_copy_to(struct map_info *map, unsigned long ofs, const void *to, ssize_t size)
{
	memcpy_toio(map->map_priv_1 + ofs, from, size);
}

static void mem_cs_release(u_long arg);

static void mem_cs_detach(dev_link_t *link)
{
	del_timer(&link->release);
	if (link->state & DEV_CONFIG) {
		mem_cs_release((u_long)link);
		if (link->state & DEV_STALE_CONFIG) {
			link->state |= DEV_STALE_LINK;
			return;
		}
	}

	if (link->handle)
		CardServices(DeregisterClient, link->handle);

	kfree(link);
}

static void mem_cs_release(u_long arg)
{
	dev_link_t *link = (dev_link_t *)arg;

	link->dev = NULL;
	if (link->win) {
		CardServices(ReleaseWindow, link->win);
	}
	link->state &= ~DEV_CONFIG;

	if (link->state & DEV_STALE_LINK)
		mem_cs_detach(link);
}

static void mem_cs_config(dev_link_t *link)
{
	struct memcs_dev *dev = link->priv;
	cs_status_t status;
	win_req_t req;

	link->state |= DEV_CONFIG;

	req.Attributes = word_width ? WIN_DATA_WIDTH_16 : WIN_DATA_WIDTH_8;
	req.Base = 0;
	req.Size = 0;
	req.AccessSpeed = mem_speed;

	link->win = (window_handle_t)link->handle;

	CS_CHECK(RequestWindow, &link->win, &req);

	CS_CHECK(GetStatus, link->handle, &status);

	dev->map.buswidth = word_width ? 2 : 1;
	dev->map.size = req.Size;
	dev->map.map_priv_1 = ioremap(req.Base, req.Size);
}

static int
mem_cs_event(event_t event, int priority, event_callback_args_t *args)
{
	dev_link_t *link = args->client_data;

	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		link->state &= ~DEV_PRESENT;
		if (link->state & DEV_CONFIG)
			mod_timer(&link->release, jiffies + HZ/20);
		break;

	case CS_EVENT_CARD_INSERTION:
		link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
		mem_cs_config(link);
		break;
	}
	return 0;
}

static dev_link_t *mem_cs_attach(void)
{
	struct memcs_dev *dev;
	client_reg_t clnt;

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (dev) {
		memset(dev, 0, sizeof(*dev));

		dev->map.read8     = mem_cs_read8;
		dev->map.read16    = mem_cs_read16;
		dev->map.read32    = mem_cs_read32;
		dev->map.copy_from = mem_cs_copy_from;
		dev->map.write8    = mem_cs_write8;
		dev->map.write16   = mem_cs_write16;
		dev->map.write32   = mem_cs_write32;
		dev->map.copy_to   = mem_cs_copy_to;

		dev->link.release.function = &mem_cs_release;
		dev->link.release.data = (u_long)link;
		dev->link.priv = dev;

		dev->link.next = dev_list;
		dev_list = &dev->link;

		clnt.dev_info = &dev_info;
		clnt.Attributes = INOF_IO_CLIENT | INFO_CARD_SHARE;
		clnt.EventMask =
			CS_EVENT_WRITE_PROTECT  | CS_EVENT_CARD_INSERTION |
			CS_EVENT_CARD_REMOVAL   | CS_EVENT_BATTERY_DEAD   |
			CS_EVENT_BATTERY_LOW;

		clnt.event_handler = &mem_cs_event;
		clnt.Version = 0x0210;
		clnt.event_callback_args.client_data = &dev->link;

		ret = CardServices(RegisterClient, &dev->link.handle, &clnt);
		if (ret != CS_SUCCESS) {
			error_info_t err = { RegisterClient, ret };
			CardServices(ReportError, dev->link.handle, &err);
			mem_cs_detach(&dev->link);
			dev = NULL;
		}
	}

	return &dev->link;
}

static int __init mem_cs_init(void)
{
	servinfo_t serv;

	CardServices(GetCardServicesInfo, &serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "memory_cs: Card services release "
			"does not match\n");
		return -ENXIO;
	}
	register_pccard_driver(&dev_info, mem_cs_attach, mem_cs_detach);
	return 0;
}

static void __exit mem_cs_exit(void)
{
	unregister_pccard_driver(&dev_info);
	while (dev_list != NULL)
		memory_detach(dev_list);
}

module_init(mem_cs_init);
module_exit(mem_cs_exit);

MODULE_LICENSE("GPL");
