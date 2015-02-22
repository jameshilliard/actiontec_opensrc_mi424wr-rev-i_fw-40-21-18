/*
 * drivers/usb/host/m82xx-rh.h
 *
 * MPC82xx Host Controller Interface driver for USB.
 * (C) Copyright 2005 Compulab, Ltd
 * Mike Rapoport, mike@compulab.co.il
 *
 * Brad Parker, brad@heeltoe.com
 * (C) Copyright 2000-2004 Brad Parker <brad@heeltoe.com>
 *
 * Fixes and cleanup by:
 * George Panageas <gpana@intracom.gr>
 * Pantelis Antoniou <panto@intracom.gr>
 *
 * designed for the EmbeddedPlanet RPX lite board
 * (C) Copyright 2000 Embedded Planet
 * http://www.embeddedplanet.com
 *
 */

/*
 * m82xx HC driver Virtual Root Hub implementation
 */

/* USB HUB CONSTANTS (not OHCI-specific; see hub.h) */

/* destination of request */
#define RH_INTERFACE               0x01
#define RH_ENDPOINT                0x02
#define RH_OTHER                   0x03

#define RH_CLASS                   0x20
#define RH_VENDOR                  0x40

/* Requests: bRequest << 8 | bmRequestType */
#define RH_GET_STATUS           0x0080
#define RH_CLEAR_FEATURE        0x0100
#define RH_SET_FEATURE          0x0300
#define RH_SET_ADDRESS          0x0500
#define RH_GET_DESCRIPTOR       0x0680
#define RH_SET_DESCRIPTOR       0x0700
#define RH_GET_CONFIGURATION    0x0880
#define RH_SET_CONFIGURATION    0x0900

/* Hub port features */
#define RH_PORT_CONNECTION         0x00
#define RH_PORT_ENABLE             0x01
#define RH_PORT_SUSPEND            0x02
#define RH_PORT_OVER_CURRENT       0x03
#define RH_PORT_RESET              0x04
#define RH_PORT_POWER              0x08
#define RH_PORT_LOW_SPEED          0x09

#define RH_C_PORT_CONNECTION       0x10
#define RH_C_PORT_ENABLE           0x11
#define RH_C_PORT_SUSPEND          0x12
#define RH_C_PORT_OVER_CURRENT     0x13
#define RH_C_PORT_RESET            0x14

/* Hub features */
#define RH_C_HUB_LOCAL_POWER       0x00
#define RH_C_HUB_OVER_CURRENT      0x01

#define RH_DEVICE_REMOTE_WAKEUP    0x00
#define RH_ENDPOINT_STALL          0x01

/* Hub class-specific descriptor */

static __u8 root_hub_des[] = {
	0x09,			/*  __u8  bLength; */
	0x29,			/*  __u8  bDescriptorType; Hub-descriptor */
	0x01,			/*  __u8  bNbrPorts; */
	0x00,			/* __u16  wHubCharacteristics; */
	0x00,
	0x01,			/*  __u8  bPwrOn2pwrGood; 2ms */
	0x00,			/*  __u8  bHubContrCurrent; 0 mA */
	0x00,			/*  __u8  DeviceRemovable; *** 7 Ports max *** */
	0xff			/*  __u8  PortPwrCtrlMask; *** 7 ports max *** */
};

static void rh_port_enable(struct m8xxhci_private *hp, int val)
{
	volatile usbregs_t *usbregs = hp->usbregs;

	BUG_ON(hp->usbregs == NULL);

	switch (val) {
		case 1:
			usbregs->usb_usmod |= USMOD_EN;
			break;
	}
}

static void rh_port_reset(struct m8xxhci_private *hp, int val)
{
	volatile usbregs_t *usbregs = hp->usbregs;
	BUG_ON(hp->usbregs == NULL);
	
	DBG("%s\n", __FUNCTION__);

	reset_bus_history(hp);

	msleep(20);
	usbregs->usb_usmod &= ~USMOD_EN;

	assert_reset(hp);
	msleep(50);
	usbregs->usb_usmod |= USMOD_EN;

	hp->rh.port_status |= RH_PS_PES;
	hp->rh.port_status &= ~RH_PS_PRS;
}

static void rh_port_power(struct m8xxhci_private *hp, int val)
{
	board_rh_power(hp, val);
}

static void rh_port_suspend(struct m8xxhci_private *hp, int val)
{
}


static int m82xx_hub_suspend(struct usb_hcd *hcd)
{
	return 0;
}

static int m82xx_hub_resume(struct usb_hcd *hcd)
{
	return 0;
}

static int m82xx_hub_status_data (struct usb_hcd *hcd, char *buf)
{
	int retval;
	struct m8xxhci_private *m82xx = hcd_to_m82xx(hcd);

	idle_bus(m82xx);

	if ( !(m82xx->rh.port_status & RH_PS_CHNG) ) {
		retval = 0;
	}
	else {
		*buf = (1 << 1);
		retval = 1;
	}

	return retval;
}

#undef min
#define min(a, b)       ((a) < (b) ? (a) : (b))
static int m82xx_hub_control(struct usb_hcd *hcd,
			     u16 typeReq, u16 wValue, u16 wIndex,
			     char *buf, u16 wLength)
{
	struct m8xxhci_private *m82xx = hcd_to_m82xx(hcd);
	int		retval = 0;
	int len;

	switch (typeReq) {
		case ClearHubFeature:
			break;
		case SetHubFeature:
			switch (wValue) {
				case C_HUB_OVER_CURRENT:
				case C_HUB_LOCAL_POWER:
					break;
				default:
					goto error;
			}
			break;
		case ClearPortFeature:
			if (wIndex != 1 || wLength != 0)
				goto error;

			switch (wValue) {
				case USB_PORT_FEAT_ENABLE:
					m82xx->rh.port_status &= ~RH_PS_PES;
					rh_port_enable(m82xx, 0);
					break;
				case USB_PORT_FEAT_SUSPEND:
					m82xx->rh.port_status &= ~RH_PS_PSS;
					rh_port_suspend(m82xx, 0);
					break;
				case USB_PORT_FEAT_POWER:
					m82xx->rh.port_status &= ~RH_PS_PRS;
					rh_port_power(m82xx, 0);
					break;
				case USB_PORT_FEAT_C_ENABLE:
				case USB_PORT_FEAT_C_SUSPEND:
				case USB_PORT_FEAT_C_CONNECTION:
				case USB_PORT_FEAT_C_OVER_CURRENT:
				case USB_PORT_FEAT_C_RESET:
					break;
				default:
					goto error;
			}
			m82xx->rh.port_status &= ~(1 << wValue);
		
			break;
		case GetHubDescriptor:
			len = min(9,wLength);
			memcpy(buf, root_hub_des, len);
			break;
		case GetHubStatus:
			*(__le32 *) buf = cpu_to_le32(0);
			break;
		case GetPortStatus:
			if (wIndex != 1)
				goto error;
			*(__le32 *) buf = cpu_to_le32(m82xx->rh.port_status);
			break;
		case SetPortFeature:
			if (wIndex != 1 || wLength != 0)
				goto error;
			switch (wValue) {
				case USB_PORT_FEAT_SUSPEND:
					m82xx->rh.port_status |= RH_PS_PSS;
					rh_port_suspend(m82xx, 1);
					break;
				case USB_PORT_FEAT_POWER:
					m82xx->rh.port_status |= RH_PS_PPS;
					rh_port_power(m82xx, 1);
					break;
				case USB_PORT_FEAT_RESET:
					m82xx->rh.port_status |= RH_PS_PRS;
					/* reset port change */
					m82xx->rh.port_status &= 0x0000ffff;
					rh_port_reset(m82xx, 1);
					break;
				case USB_PORT_FEAT_ENABLE:
					m82xx->rh.port_status |= RH_PS_PES;
					rh_port_enable(m82xx, 1);
				default:
					goto error;
			}
			break;

		default:
	  error:
			/* "protocol stall" on error */
			retval = -EPIPE;
	}

	return retval;
}


