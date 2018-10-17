/*
 * arch/arm/mach-pnx8181/sctu1.c
 *
 * Copyright (C) 2010 Murali TD. Mohan, DSPG
 * Copyright (C) 2008 Georg Gottleuber <ggottle@emlix.com>, emlix GmbH
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/io.h>

#include <asm/mach/time.h>
#include <asm/arch/hardware.h>
#include <asm/arch/platform.h>
#include <asm/arch/pnx8181-regs.h>
#include <asm/arch/irqs.h>

#ifdef CONFIG_GENERIC_TIME
/* Common header */
#include "sctu.h"

/* The timer data */
static struct sctu_data sctu1 = {
	SCTU_ADD_PTRS(PNX8181_TIM1_CR)
	.scaling_factor = 255,
	.clkevt = {
		.name		= "sctu1",
		.rating		= 400,
		.features	= (CLOCK_EVT_FEAT_PERIODIC),
		.shift		= 32,
		.irq		= 18,
	}
};

/* Read function */
static cycle_t sctu1_read_clk(void)
{
	return sctu_read_clk(&sctu1);
}

/* We use the default IRQ settings */
static struct irqaction sctu1_irq;

/* The clock source itself */
static struct clocksource sctu1_clk = {
	.name		= "sctu1",
	.rating		= 400,
	.read		= sctu1_read_clk,
	.shift		= 16,
	.mask		= CLOCKSOURCE_MASK(16),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init sctu1_clk_init(void)
{
	sctu_clk_init(&sctu1, &sctu1_clk, &sctu1_irq);
}

struct sys_timer pnx8181_timer = {
	.init	= sctu1_clk_init,
};
#endif
