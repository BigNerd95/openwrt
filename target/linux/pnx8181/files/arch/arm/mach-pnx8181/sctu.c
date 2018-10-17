/*
 * arch/arm/mach-pnx8181/sctu.c
 *
 * Copyright (C) 2010 Murali TD. Mohan, DSPG
 * Copyright (C) 2008 Georg Gottleuber <ggottle@emlix.com>, emlix GmbH
 * Copyright (c) 2008 Sebastian Hess <sh@emlix.com>, emlix GmbH
 * Copyright (C) 2007 Sebastian Smolorz <ssm@emlix.com>, emlix GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/io.h>

#include <asm/arch/hardware.h>
#include <asm/arch/platform.h>
#include <asm/arch/pnx8181-regs.h>
#include <asm/arch/irqs.h>

#ifdef  CONFIG_GENERIC_TIME 
#include "sctu.h"

/*
 * Helper function to determine clk13mm, the SCTU clock.
 * The clock for the SCTU unit is the same for SCTU1 and SCTU2
 */
static uint32_t sctu_get_clock(void)
{
	uint32_t cgufixcon, fixdiv;

	cgufixcon = __raw_readl(PNX8181_CGU_FIXCON);
	fixdiv = (cgufixcon & PNX8181_CGU_FIXCON_FIXDIV_MASK)
			>> PNX8181_CGU_FIXCON_FIXDIV_SHIFT;
	cgufixcon &= (PNX8181_CGU_FIXCON_FIXDIV_MASK ||
				PNX8181_CGU_FIXCON_PCLK_MASK);
	cgufixcon >>= PNX8181_CGU_FIXCON_FIXDIV_SHIFT;

	switch (fixdiv) {
	case 0:
		return 11520000;	/* 11.520 MHz */
	case 2:
		return 13000000;	/* 13.000 MHz */
	default:
		break;
	}

	switch (cgufixcon) {
	case 1:
	case 5:
		return 13824000;	/* 13.824 MHz */
	case 9:
	case 13:
		return 11520000;	/* 11.520 MHz */
	case 3:
	case 7:
		return 15600000;	/* 15.600 MHz */
	case 11:
	case 15:
		return 13000000;	/* 13.000 MHz */
	default:
		break;
	}

	/* never reached */
	return 0;
}

#define PNX8181_TIMER_DEF \
    (PNX8181_TIM_CR_RUN | PNX8181_TIM_CR_CM0 | PNX8181_TIM_CR_EI0)

static void sctu_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	struct sctu_data *sctu = container_of(evt, struct sctu_data,
						clkevt);

	/* We are doing nothing on RESUME */
	if (mode == CLOCK_EVT_MODE_RESUME)
		return;

	/* Ensure that the timer is not running. */
	__raw_writew(0, sctu->cr);

	if (mode == CLOCK_EVT_MODE_UNUSED || mode == CLOCK_EVT_MODE_SHUTDOWN)
		return;

	/* Clear timer status */
	__raw_writew(0, sctu->sr);

	/*
	 * Set prescaler reload value to a factor and the timer
	 * We want to have an increasing, free running counter from 0 to 0xFFFF
	 */
	__raw_writew(0x100 - sctu->scaling_factor, sctu->pr);
	__raw_writew(0x0, sctu->rr);

	if (mode == CLOCK_EVT_MODE_ONESHOT)
		sctu->cnt = 0;

	if (mode == CLOCK_EVT_MODE_PERIODIC)
		__raw_writel(sctu->ticks_per_jiffy, sctu->c0);

	/* Enable the channel 0 and its interrupt and let the timer start */
	if (mode == CLOCK_EVT_MODE_ONESHOT)
		__raw_writew(PNX8181_TIMER_DEF | PNX8181_TIM_CR_ETOV, sctu->cr);
	else
		__raw_writew(PNX8181_TIMER_DEF, sctu->cr);
}

static int sctu_next_event(unsigned long delta, struct clock_event_device *evt)
{
	unsigned long flags;
	cycle_t pcntr;
	cycle_t  temp;

	struct sctu_data *sctu = container_of(evt, struct sctu_data,
						clkevt);

	local_irq_save(flags);

	/* Decide between ONE shot and periodic mode */
	if (evt->mode == CLOCK_EVT_MODE_PERIODIC) {
		delta += (unsigned long) __raw_readw(sctu->wr);
		__raw_writew(delta, sctu->c0);
	} else if (evt->mode == CLOCK_EVT_MODE_ONESHOT) {
		/* Updated the count value  */
		pcntr = __raw_readw(sctu->wr);
		if ((sctu->cnt & 0xFFFF) > pcntr)
			temp = 0x10000 - (sctu->cnt & 0xFFFF) + pcntr;
		else
			temp = pcntr - (sctu->cnt & 0xFFFF);

		sctu->cnt += temp;
		sctu->last = sctu->cnt;
		__raw_writew(delta + sctu->cnt, sctu->c0);
	}
	local_irq_restore(flags);

	return 0;
}

/* IRQ handler */
static irqreturn_t sctu_interrupt(int irq, void *dev_id)
{
	struct sctu_data *sctu = dev_id;
	volatile unsigned short regval;
	cycle_t pcntr;
	cycle_t temp;
	unsigned long flags;

	local_irq_save(flags);

	regval = __raw_readw(sctu->sr);
	__raw_writew(0, sctu->sr);

	/* Handle the condition even if overflow happens within
	 * next call to sctu_next_event, then sctu->cnt has to be updated */

	write_seqlock(&xtime_lock);

	/* In PERIODIC mode we increment here */
	if (sctu->clkevt.mode == CLOCK_EVT_MODE_PERIODIC) {
		sctu->cnt += sctu->ticks_per_jiffy;
		sctu->last = sctu->cnt;
		__raw_writew(sctu->cnt + sctu->ticks_per_jiffy, sctu->c0);
	} else {
		pcntr = __raw_readw(sctu->wr);
		if ((sctu->cnt & 0xFFFF) > pcntr)
			temp = 0x10000 - (sctu->cnt & 0xFFFF) + pcntr;
		else
			temp = pcntr - (sctu->cnt & 0xFFFF);
		sctu->cnt += temp;
		/* Update the last count value so that sctu_read_clk is proper */
		sctu->last = sctu->cnt;
	}

	write_sequnlock(&xtime_lock);

	/* Call the event handler only if it is timer
	 * comparator match interrupt as we also get overflow interrupts */
	if (regval & 0x2)
		sctu->clkevt.event_handler(&sctu->clkevt);

	local_irq_restore(flags);

	return IRQ_HANDLED;
}

cycle_t sctu_read_clk(struct sctu_data *sctu)
{
	unsigned long flags;
	cycle_t pcntr;
	cycle_t temp;

	local_irq_save(flags);

	pcntr = __raw_readw(sctu->wr);
	if (pcntr >= (sctu->last & 0xFFFF)) {
		/* usual case: add current timer state, substract before added
		 * tick */
		temp = sctu->last + pcntr - (sctu->last & 0xFFFF);
	} else {
		/* overflow case: add the time last tick to 0xFFFF and add
		 * current timer state */
		temp = sctu->last + (0x10000 - (sctu->last & 0xFFFF)) + pcntr;
	}

	local_irq_restore(flags);

	return temp;
}

int sctu_clk_init(struct sctu_data *sctu,
				struct clocksource *sctu_clk,
				struct irqaction *sctu_irq)
{
	struct clock_event_device *clkevt = &sctu->clkevt;
	int ret = 0;

	sctu->freq = sctu_get_clock() / sctu->scaling_factor;
	sctu->cnt = sctu->ticks_per_jiffy = sctu->freq / HZ;

	clkevt->cpumask = cpumask_of_cpu(smp_processor_id());
	clkevt->mult = div_sc(sctu->freq, NSEC_PER_SEC, clkevt->shift);

	/* max and min time of a timer event; 0xFFFF to stop at one tick before
	 * (overflow) the current compare value; 0xF0 measured by reading the
	 * time at an interrupt, it should be assured to be after the interrupt
	 * handler */
	clkevt->max_delta_ns = clockevent_delta2ns(0xFFFF, clkevt);
	clkevt->min_delta_ns = 30000; /* 30 µs */

	/* Register our clock events */
	clkevt->set_next_event = sctu_next_event;
	clkevt->set_mode = sctu_set_mode;

	/* Setup the IRQ */
	sctu_irq->handler = sctu_interrupt;
	sctu_irq->dev_id = sctu;
	sctu_irq->name = sctu->clkevt.name;
	sctu_irq->flags |= IRQF_DISABLED | IRQF_TIMER;
	setup_irq(sctu->clkevt.irq, sctu_irq);

	/* Finish the clock source */
	sctu_clk->mult = clocksource_hz2mult(sctu->freq, sctu_clk->shift);
	ret = clocksource_register(sctu_clk);

	clockevents_register_device(clkevt);

	return ret;
}
#endif
