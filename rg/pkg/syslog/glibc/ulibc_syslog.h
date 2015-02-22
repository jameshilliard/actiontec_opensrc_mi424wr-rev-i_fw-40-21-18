#ifndef _ULIBC_SYSLOG_H_
#define _ULIBC_SYSLOG_H_

#define openlog ulibc_openlog
#define closelog ulibc_closelog
#define syslog ulibc_syslog
#define vsyslog ulibc_vsyslog
#define setlogmask ulibc_setlogmask

#endif
