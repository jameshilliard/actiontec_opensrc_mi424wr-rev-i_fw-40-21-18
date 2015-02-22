#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <errno.h>
#include <limits.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif

#include <netinet/in.h>
#ifdef __KAME__
#include <net/if_dl.h>
#include <netinet6/in6_var.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <ifaddrs.h>

#include <dhcp6.h>
#include <config.h>
#include <common.h>
#include <timer.h>
#include <dhcp6c.h>
#include <control.h>
#include <dhcp6_ctl.h>
#include <dhcp6c_ia.h>
#include <prefixconf.h>
#include <auth.h>

#include <util/mgt_client.h>

#define DUID_FILE SYSCONFDIR "/dhcp6_duid"

int main(int argc,char **argv)
{
	struct duid dhcpv6_duid;
    time_t time_val;
    if (argc >= 2 ) {
        time_val = atol(argv[1]);
    }
    else
    {
        time_val = time(NULL);
    }

    printf("get duid to %s using time value %ld\n",DUID_FILE,(u_int64_t)time_val);
	/* get our DUID */
	if (get_duid_with_time_value(DUID_FILE, &dhcpv6_duid,time_val)) {
		dprintf(LOG_ERR, FNAME, "failed to get a DUID");
		return(-1);
	}
	return 0;

}


	
