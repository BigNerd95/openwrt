/*
 * arch/arm/mach-pnx8181/sctu2.c
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

#ifdef CONFIG_TICK_ONESHOT
#include "sctu.h"

/* The timer data */
static struct sctu_data sctu2 = {
	SCTU_ADD_PTRS(PNX8181_TIM2_CR)
	.scaling_factor = 250,
	.clkevt = {
		.name		= "sctu2",
		.rating		= 500,
		.features	= CLOCK_EVT_FEAT_ONESHOT,
		.shift		= 32,
		.irq		= 17,
	}
};

/* We use the default IRQ settings */
static struct irqaction sctu2_irq = {
	.flags = IRQF_IRQPOLL,
};

/* Read function */
static cycle_t sctu2_read_clk(void)
{
	return sctu_read_clk(&sctu2);
}

/* The clock source itself */
static struct clocksource sctu2_clk = {
	.name		= "sctu2",
	.rating		= 600,
	.read		= sctu2_read_clk,
	.shift		= 16,
	.mask		= CLOCKSOURCE_MASK(64),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int __init sctu2_clk_init(void)
{
	return sctu_clk_init(&sctu2, &sctu2_clk, &sctu2_irq);
}

module_init(sctu2_clk_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georg Gottleuber, Sebastian Smolorz, Murali TD. Mohan");
MODULE_DESCRIPTION("High Resolution Timer support for pnx8181 using SCTU2");
#endif

