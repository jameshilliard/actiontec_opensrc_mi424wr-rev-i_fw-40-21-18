/*
 *
 * Per Hallsmark, per.hallsmark@mvista.com
 *
 * Based on jmr3927/common/prom.c
 *
 * 2004 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/serial_ip3106.h>

#include <asm/bootinfo.h>
#include <uart.h>

/* #define DEBUG_CMDLINE */

extern int prom_argc;
extern char **prom_argv, **prom_envp;

typedef struct
{
    char *name;
/*    char *val; */
}t_env_var;


char * prom_getcmdline(void)
{
	return &(arcs_cmdline[0]);
}

void  prom_init_cmdline(void)
{
	char *cp;
	int actr;

	actr = 1; /* Always ignore argv[0] */

	cp = &(arcs_cmdline[0]);
	while(actr < prom_argc) {
	        strcpy(cp, prom_argv[actr]);
		cp += strlen(prom_argv[actr]);
		*cp++ = ' ';
		actr++;
	}
	if (cp != &(arcs_cmdline[0])) /* get rid of trailing space */
		--cp;
	*cp = '\0';
}

char *prom_getenv(char *envname)
{
	/*
	 * Return a pointer to the given environment variable.
	 * Environment variables are stored in the form of "memsize=64".
	 */

	t_env_var *env = (t_env_var *)prom_envp;
	int i;

	i = strlen(envname);

	while(env->name) {
		if(strncmp(envname, env->name, i) == 0) {
			return(env->name + strlen(envname) + 1);
		}
		env++;
	}
	return(NULL);
}

inline unsigned char str2hexnum(unsigned char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0; /* foo */
}

inline void str2eaddr(unsigned char *ea, unsigned char *str)
{
	int i;

	for(i = 0; i < 6; i++) {
		unsigned char num;

		if((*str == '.') || (*str == ':'))
			str++;
		num = str2hexnum(*str++) << 4;
		num |= (str2hexnum(*str++));
		ea[i] = num;
	}
}

int get_ethernet_addr(char *ethernet_addr)
{
        char *ethaddr_str;

        ethaddr_str = prom_getenv("ethaddr");
	if (!ethaddr_str) {
	        printk("ethaddr not set in boot prom\n");
		return -1;
	}
	str2eaddr(ethernet_addr, ethaddr_str);
	return 0;
}

unsigned long __init prom_free_prom_memory(void)
{
	return 0;
}

extern int pnx8550_console_port;

/* used by prom_printf */
void prom_putchar(char c)
{
	if (pnx8550_console_port != -1) {
		/* Wait until FIFO not full */
		while( ((ip3106_fifo(UART_BASE, pnx8550_console_port) & IP3106_UART_FIFO_TXFIFO) >> 16) >= 16)
			;
		/* Send one char */
		ip3106_fifo(UART_BASE, pnx8550_console_port) = c;
	}
}

EXPORT_SYMBOL(prom_getcmdline);
EXPORT_SYMBOL(get_ethernet_addr);
EXPORT_SYMBOL(str2eaddr);
