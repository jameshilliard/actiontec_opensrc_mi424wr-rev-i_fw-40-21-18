/*
 * drivers/pcmcia/sa1100_adsbitsy.c
 *
 * PCMCIA implementation routines for ADS Bitsy
 *
 * 9/18/01 Woojung
 *         Fixed wrong PCMCIA voltage setting
 *
 * 7/5/01 Woojung Huh <whuh@applieddata.net>
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/irq.h>

#include "sa1100_generic.h"
#include "sa1111_generic.h"

int adsbitsy_smc91111_present(void);

#ifndef	CONFIG_SMC91111
#define adsbitsy_smc91111_present() 0
#endif

static struct irqs {
	int irq;
	const char *str;
} irqs[] = {
	{ S0_CD_VALID,    "SA1111 PCMCIA card detect" },
	{ S0_BVD1_STSCHG, "SA1111 PCMCIA BVD1"        },
	{ S1_CD_VALID,    "SA1111 CF card detect"     },
	{ S1_BVD1_STSCHG, "SA1111 CF BVD1"            },
};

static int adsbitsy_pcmcia_init(struct pcmcia_init *init)
{
  int ret=0;
  int nirq = 0;
  int slots = 0;
  int i;

  /* Set GPIO_A<1:0> to be outputs for PCMCIA power controller: */
  PA_DDR &= ~(GPIO_GPIO0 | GPIO_GPIO1);

  /* Disable Power 3.3V/5V for PCMCIA */
  PA_DWR |= GPIO_GPIO0 | GPIO_GPIO1;

  if (!request_mem_region(_PCCR, 512, "PCMCIA"))
	  return -1;


  INTPOL1 |= SA1111_IRQMASK_HI(S0_READY_NINT) |
             SA1111_IRQMASK_HI(S0_CD_VALID)   |
             SA1111_IRQMASK_HI(S0_BVD1_STSCHG);

  nirq = 2;
  slots = 1;

  if (!adsbitsy_smc91111_present()) {
    /* If the SMC91111 is used CF cannot be used */
    /* Set GPIO_A<3:2> to be outputs for CF power controller: */
    PA_DDR &= ~(GPIO_GPIO2 | GPIO_GPIO3);

    /* Disable Power 3.3V/5V for CF */
    PA_DWR |= GPIO_GPIO2 | GPIO_GPIO3;

    INTPOL1 |= SA1111_IRQMASK_HI(S1_READY_NINT) |
               SA1111_IRQMASK_HI(S1_CD_VALID)   |
               SA1111_IRQMASK_HI(S1_BVD1_STSCHG);

    nirq = 4;
    slots = 2;
  }

  for (i = ret = 0; i < nirq; i++) {
	  ret = request_irq(irqs[i].irq, init->handler, SA_INTERRUPT,
			    irqs[i].str, NULL);
	  if (ret)
		  break;
  }

  if (i < nirq) {
	  printk(KERN_ERR "sa1111_pcmcia: unable to grab IRQ%d (%d)\n",
		 irqs[i].irq, ret);
	  while (i--)
		  free_irq(irqs[i].irq, NULL);

	  release_mem_region(_PCCR, 16);
  }

  return ret ? -1 : slots;
}

static int adsbitsy_pcmcia_shutdown(void)
{

  free_irq(S0_CD_VALID, NULL);
  free_irq(S0_BVD1_STSCHG, NULL);
  INTPOL1 &= ~(SA1111_IRQMASK_HI(S0_CD_VALID) | SA1111_IRQMASK_HI(S0_BVD1_STSCHG));

  if (!adsbitsy_smc91111_present()) {
    free_irq(S1_CD_VALID, NULL);
    free_irq(S1_BVD1_STSCHG, NULL);
    INTPOL1 &= ~(SA1111_IRQMASK_HI(S1_CD_VALID) | SA1111_IRQMASK_HI(S1_BVD1_STSCHG));
  }

  return 0;
}


static int adsbitsy_pcmcia_socket_state(struct pcmcia_state_array *state)
{
	unsigned long status;

	if (adsbitsy_smc91111_present()) {
		if(state->size<1) return -1;
	}
	else
		if(state->size<2) return -1;

	memset(state->state, 0,
	       (state->size)*sizeof(struct pcmcia_state));

	status = PCSR;

	state->state[0].detect = status & PCSR_S0_DETECT ? 0 : 1;
	state->state[0].ready  = status & PCSR_S0_READY  ? 1 : 0;
	state->state[0].bvd1   = status & PCSR_S0_BVD1   ? 1 : 0;
	state->state[0].bvd2   = status & PCSR_S0_BVD2   ? 1 : 0;
	state->state[0].wrprot = status & PCSR_S0_WP     ? 1 : 0;
	state->state[0].vs_3v  = status & PCSR_S0_VS1    ? 0 : 1;
	state->state[0].vs_Xv  = status & PCSR_S0_VS2    ? 0 : 1;

	if (state->size > 1) {
		if (adsbitsy_smc91111_present()) {
			// If there is SMC91111 on ADS Bitsy connector board
			// it returns not detect/ready/...
			state->state[1].detect = 0;
			state->state[1].ready = 0;
			state->state[1].bvd1 = 0;
			state->state[1].bvd2 = 0;
			state->state[1].wrprot = 0;
			state->state[1].vs_3v = 0;
			state->state[1].vs_Xv = 0;
		}
		else {
			state->state[1].detect = status & PCSR_S1_DETECT ? 0 : 1;
			state->state[1].ready  = status & PCSR_S1_READY  ? 1 : 0;
			state->state[1].bvd1   = status & PCSR_S1_BVD1   ? 1 : 0;
			state->state[1].bvd2   = status & PCSR_S1_BVD2   ? 1 : 0;
			state->state[1].wrprot = status & PCSR_S1_WP     ? 1 : 0;
			state->state[1].vs_3v  = status & PCSR_S1_VS1    ? 0 : 1;
			state->state[1].vs_Xv  = status & PCSR_S1_VS2    ? 0 : 1;
		}
	}
	return 1;
}

static int adsbitsy_pcmcia_configure_socket(const struct pcmcia_configure *conf)
{
  unsigned int pa_dwr_mask, pa_dwr_set;
  int ret;

  switch (conf->sock) {
  case 0:
    pa_dwr_mask = GPIO_GPIO0 | GPIO_GPIO1;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = GPIO_GPIO0 | GPIO_GPIO1;	break;
    case 33:	pa_dwr_set = GPIO_GPIO1;		break;
    case 50:	pa_dwr_set = GPIO_GPIO0;		break;
    }
    break;

  case 1:
    pa_dwr_mask = GPIO_GPIO2 | GPIO_GPIO3;

    switch (conf->vcc) {
    default:
    case 0:	pa_dwr_set = GPIO_GPIO2 | GPIO_GPIO3;   break;
    case 33:	pa_dwr_set = GPIO_GPIO2;		break;
    case 50:	pa_dwr_set = GPIO_GPIO3;		break;
    }
    break;

  default:
    return -1;
  }

  if (conf->vpp != conf->vcc && conf->vpp != 0) {
    printk(KERN_ERR "%s(): CF slot cannot support VPP %u\n",
		__FUNCTION__, conf->vpp);
    return -1;
  }

  ret = sa1111_pcmcia_configure_socket(conf);
  if (ret == 0) {
    unsigned long flags;

    local_irq_save(flags);
    PA_DWR = (PA_DWR & ~pa_dwr_mask) | pa_dwr_set;
    local_irq_restore(flags);
  }

  return ret;
}

struct pcmcia_low_level adsbitsy_pcmcia_ops = {
  init:			adsbitsy_pcmcia_init,
  shutdown:		adsbitsy_pcmcia_shutdown,
  socket_state:		adsbitsy_pcmcia_socket_state,
  get_irq_info:		sa1111_pcmcia_get_irq_info,
  configure_socket:	adsbitsy_pcmcia_configure_socket,

  socket_init:		sa1111_pcmcia_socket_init,
  socket_suspend:	sa1111_pcmcia_socket_suspend,
};

