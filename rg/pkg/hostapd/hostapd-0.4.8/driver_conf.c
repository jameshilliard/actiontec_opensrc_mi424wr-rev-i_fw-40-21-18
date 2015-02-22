/* LICENSE JGPL */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "hostapd.h"
#include "driver.h"

/* ifdef CONFIG_HOSTAPD_DRIVER_MADWIFI */
void madwifi_driver_register(void);

void register_drivers(void)
{
/* ifdef CONFIG_HOSTAPD_DRIVER_MADWIFI */
    madwifi_driver_register();
}

