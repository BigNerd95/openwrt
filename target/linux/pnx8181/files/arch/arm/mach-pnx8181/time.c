/*
 * linux/arch/arm/mach-pnx8181/time.c
 *
 * Copyright (C) 2010 Murali TD. Mohan, DSPG
 * Copyright (C) 2007 Sebastian Smolorz <ssm@emlix.com>, emlix GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef CONFIG_GENERIC_TIME
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/io.h>

#include <asm/mach/time.h>

#include <asm/arch/hardware.h>
#include <asm/arch/platform.h>
#include <asm/arch/pnx8181-regs.h>
#include <asm/arch/irqs.h>


static uint32_t ticks_per_jiffy;
static uint16_t timer_startval;
static uint16_t next_cmp;

static unsigned long timer_usec_ticks;

#define TIMER_USEC_SHIFT 16

#define PNX_TIMER_IRQ 17

/* taken and slightly modified from arch/arm/plat-s3c24xx/time.c */
static inline unsigned long timer_mask_usec_ticks(unsigned long pclk)
{
	unsigned long den = pclk / 1000;

	return ((1000 << TIMER_USEC_SHIFT) + (den >> 1)) / den;
}

/* timer_ticks_to_usec
 *
 * convert timer ticks to usec.
 *
 * taken from arch/arm/plat-s3c24xx/time.c
 */

static inline unsigned long timer_ticks_to_usec(unsigned long ticks)
{
	unsigned long res;

	res = ticks * timer_usec_ticks;
	res += 1 << (TIMER_USEC_SHIFT - 4);	/* round up slightly */

	return res >> TIMER_USEC_SHIFT;
}


/* Helper function to determine clk13mm, the SCTU clock. */
static uint32_t clk_get_rate_of_timer(void)
{
	uint32_t cgufixcon, fixdiv;

	cgufixcon = __raw_readl(PNX8181_CGU_FIXCON);
	fixdiv = (cgufixcon & PNX8181_CGU_FIXCON_FIXDIV_MASK) >>
					PNX8181_CGU_FIXCON_FIXDIV_SHIFT;
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


static unsigned long pnx8181_gettimeoffset(void)
{
	uint16_t tval;
	uint32_t tdone;

	tval = __raw_readl(PNX8181_TIM2_WR) & PNX8181_TIM_WR_MASK;
	tdone = (uint16_t)(tval - timer_startval);

	/* We have to check if there is a risen timer interrupt i.e. a compare
	   event has occured. */
	if (__raw_readl(PNX8181_TIM2_SR) & PNX8181_TIM_SR_C0) {
		/* Re-read the timer. If the difference to the last start
		   value is smaller than the ticks per jiffy we assume
		   that we have to fix the value due to _one_ overflow of
		   the counter. */

		tval = __raw_readl(PNX8181_TIM2_WR) & PNX8181_TIM_WR_MASK;
		tdone = (uint16_t)(tval - timer_startval);

		if (tdone < ticks_per_jiffy)
			tdone += ticks_per_jiffy;
	}

	return timer_ticks_to_usec(tdone);
}

#ifdef CONFIG_ARCH_PNX8181_KERNEL_WATCHDOG
extern int pnx8181_wdt_driver_active = 0;
#endif

/* IRQ handler for the timer */
static irqreturn_t pnx8181_timer_interrupt(int irq, void *dev_id)
{
	timer_tick();

#ifdef CONFIG_ARCH_PNX8181_KERNEL_WATCHDOG
	if (! pnx8181_wdt_driver_active)
		__raw_writew(10000, PNX8181_WDTIM); /* 10 seconds */
#endif

	timer_startval = next_cmp;
	next_cmp += ticks_per_jiffy;
	__raw_writel(0, PNX8181_TIM2_SR);
	__raw_writel(next_cmp, PNX8181_TIM2_C0);

	return IRQ_HANDLED;
}

static struct irqaction pnx8181_timer_irq = {
	.name		= "PNX8181 Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQ_TYPE_LEVEL_HIGH,
	.handler	= pnx8181_timer_interrupt,
};


static void __init pnx8181_timer_init(void)
{
	uint32_t clk_freq;

	clk_freq = clk_get_rate_of_timer();

	timer_usec_ticks = timer_mask_usec_ticks(clk_freq);

	ticks_per_jiffy = clk_freq / HZ;
	if (ticks_per_jiffy > 0xFFFF) {
		panic("pnx8181_timer_init: HZ is too small, "
			"cannot configure timer! Try HZ >= 250.\n");
		return;
	}

	timer_startval = 0;
	next_cmp = ticks_per_jiffy;

	/* Ensure that the timer is not running. */
	__raw_writel(0, PNX8181_TIM2_CR);

	/* Clear timer status. */
	__raw_writel(0, PNX8181_TIM2_SR);

	/* Set prescale reload value to 0xFF (no prescaler) and the timer
	   reload value to 0. We want to have an increasing, free running
	   counter from 0 to 0xFFFF. */
	__raw_writel(0xFF, PNX8181_TIM2_PR);
	__raw_writel(0x0, PNX8181_TIM2_RR);

	/* Set initial compare value, use channel 0. */
	__raw_writel(ticks_per_jiffy, PNX8181_TIM2_C0);

	/* Enable channel 0 and its interrupt and let the timer start. */
	__raw_writel(PNX8181_TIM_CR_RUN | PNX8181_TIM_CR_CM0 |
				PNX8181_TIM_CR_EI0, PNX8181_TIM2_CR);

	setup_irq(PNX_TIMER_IRQ, &pnx8181_timer_irq);
}

struct sys_timer pnx8181_timer = {
	.init	= pnx8181_timer_init,
	.offset	= pnx8181_gettimeoffset,
};
#endif
