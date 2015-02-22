#include <linux/module.h>
#include <usb.h>

void (*_usbled_device_add)(int busnum, int port) = 0;
void (*_usbled_device_remove)(int busnum, int port) = 0;
void (*_usbled_data_start)(int busnum, int port) = 0;

int _usbled_devices[3][4];

#define CHECK_DEV(dev) \
	(dev && dev->bus && dev->parent && dev->parent->devpath[0] == '0' \
	&& 1 <= dev->bus->busnum && dev->bus->busnum <= 3 \
	&& 0 <= dev->port && dev->port <= 3)

static void usb_led_device_set(struct usb_device *dev, int is_added)
{
	if (!CHECK_DEV(dev))
		return;

	_usbled_devices[dev->bus->busnum - 1][dev->port] = is_added;

	if (is_added && _usbled_device_add)
		_usbled_device_add(dev->bus->busnum, dev->port);
	else if (!is_added && _usbled_device_remove)
		_usbled_device_remove(dev->bus->busnum, dev->port);
}

void usbled_device_add(struct usb_device *dev)
{
	usb_led_device_set(dev, 1);
}

void usbled_device_remove(struct usb_device *dev)
{
	usb_led_device_set(dev, 0);
}


void usbled_data_start(struct usb_device *dev)
{
	if (!CHECK_DEV(dev))
		return;

	if (_usbled_data_start)
		_usbled_data_start(dev->bus->busnum, dev->port);
}

EXPORT_SYMBOL(_usbled_devices);
EXPORT_SYMBOL(_usbled_device_add);
EXPORT_SYMBOL(_usbled_device_remove);
EXPORT_SYMBOL(_usbled_data_start);

