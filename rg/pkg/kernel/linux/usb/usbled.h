#ifndef _USBLED_H_L
#define _USBLED_H_L

void usbled_device_add(struct usb_device *dev);
void usbled_device_remove(struct usb_device *dev);
void usbled_data_start(struct usb_device *dev);

#endif
